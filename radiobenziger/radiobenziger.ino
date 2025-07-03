#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <EEPROM.h>
#include <vector>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "Config.h"
#include "WiFiManager.h"
#include "PCMStreamer.h"

// Global objects
Config config;
WiFiManager wifiManager;
WebServer server(80);
PCMStreamer* audioStreamer = nullptr;

// Connection state
bool isConnected = false;
bool audioInitialized = false;

// Audio streaming parameters - Optimized for WiFi resilience
const size_t AUDIO_BUFFER_QUEUE_SIZE = 20;     // Large buffer for WiFi interruptions (1 second total)
const size_t AUDIO_CHUNK_SAMPLES = 1600;       // Larger chunks (50ms at 32kHz)
const size_t AUDIO_CHUNK_DURATION_MS = 50;     // 50ms per chunk
const size_t HTTP_BUFFER_SIZE = 6400;          // Larger HTTP buffer (4x chunk size)

// PCM streaming configuration
const char* PCM_SERVER_HOST = "192.168.1.189"; // Update with your server IP (find with: ip addr show)
const int PCM_SERVER_PORT = 8080;
const char* PCM_STREAM_PATH = "/stream";

// Audio buffer structure with fixed-size array (safer for FreeRTOS queues)
struct AudioBuffer {
    int16_t samples[AUDIO_CHUNK_SAMPLES]; // Fixed size: mono samples from server
    size_t sampleCount;
    bool isValid;
    bool isSilence;
    
    AudioBuffer() : sampleCount(0), isValid(false), isSilence(true) {
        memset(samples, 0, sizeof(samples));
    }
    
    AudioBuffer(const int16_t* data, size_t count) : 
        sampleCount(count), isValid(true), isSilence(false) {
        memset(samples, 0, sizeof(samples));
        if (count <= sizeof(samples)/sizeof(samples[0])) {
            memcpy(samples, data, count * sizeof(int16_t));
        }
    }
    
    // Create silence buffer
    static AudioBuffer createSilence() {
        AudioBuffer buffer;
        buffer.sampleCount = AUDIO_CHUNK_SAMPLES;
        buffer.isValid = true;
        buffer.isSilence = true;
        memset(buffer.samples, 0, sizeof(buffer.samples));
        return buffer;
    }
};

// Streaming control variables
bool streamingActive = false;
bool streamingRequested = false;
SemaphoreHandle_t streamingMutex = nullptr;

// Task handles
TaskHandle_t audioTaskHandle = nullptr;
TaskHandle_t streamingTaskHandle = nullptr;
QueueHandle_t audioBufferQueue = nullptr;

// Audio playback task (runs on Core 1) - consumes from buffer queue
void audioTask(void* parameter) {
    Serial.println("Audio playback task started on Core 1");
    
    // Static vector for reuse (prevents stack overflow)
    static std::vector<int16_t> sampleVector;
    sampleVector.reserve(AUDIO_CHUNK_SAMPLES);
    
    // Silence detection
    uint32_t silenceCount = 0;
    const uint32_t MAX_SILENCE_BEFORE_MUTE = 4; // 4 buffers (200ms) of silence before muting
    
    while (true) {
        bool audioWritten = false;
        
        if (audioInitialized && audioStreamer && audioStreamer->isReady()) {
            // Try to get audio buffer from queue
            AudioBuffer buffer;
            if (audioBufferQueue != nullptr && xQueueReceive(audioBufferQueue, &buffer, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (buffer.isValid && buffer.sampleCount > 0) {
                    // Check if this is silence or valid audio
                    if (buffer.isSilence || !streamingActive) {
                        silenceCount++;
                        // Only output silence if we haven't been silent too long
                        if (silenceCount <= MAX_SILENCE_BEFORE_MUTE) {
                            sampleVector.clear();
                            sampleVector.assign(buffer.samples, buffer.samples + buffer.sampleCount);
                            audioStreamer->writeSamples(sampleVector);
                            audioWritten = true;
                        }
                    } else {
                        // Valid audio data
                        silenceCount = 0;
                        sampleVector.clear();
                        sampleVector.assign(buffer.samples, buffer.samples + buffer.sampleCount);
                        
                        size_t bytesWritten = audioStreamer->writeSamples(sampleVector);
                        if (bytesWritten > 0) {
                            audioWritten = true;
                            // Successfully played buffer
                            static uint32_t bufferCount = 0;
                            bufferCount++;
                            if (bufferCount % 20 == 0) { // Print every second (20 * 50ms)
                                UBaseType_t queueCount = uxQueueMessagesWaiting(audioBufferQueue);
                                Serial.printf("Played %u buffers, queue: %u/%u (streaming: %s)\n", 
                                            bufferCount, queueCount, AUDIO_BUFFER_QUEUE_SIZE, streamingActive ? "yes" : "no");
                            }
                        } else {
                            Serial.println("Warning: Audio write failed");
                        }
                    }
                }
            } else {
                // No buffer available - only send silence if we're actively streaming and queue is empty
                if (streamingActive && streamingRequested) {
                    UBaseType_t queueCount = uxQueueMessagesWaiting(audioBufferQueue);
                    if (queueCount == 0) {
                        // Only send silence if queue is truly empty
                        AudioBuffer silenceBuffer = AudioBuffer::createSilence();
                        sampleVector.clear();
                        sampleVector.assign(silenceBuffer.samples, silenceBuffer.samples + silenceBuffer.sampleCount);
                        audioStreamer->writeSamples(sampleVector);
                        audioWritten = true;
                        silenceCount++;
                        
                        static uint32_t underrunCount = 0;
                        underrunCount++;
                        if (underrunCount % 10 == 0) {
                            Serial.printf("Buffer underrun: %u occurrences\n", underrunCount);
                        }
                    }
                }
            }
        }
        
        // If no audio was written and streaming is active, ensure we maintain timing
        if (!audioWritten && streamingActive) {
            vTaskDelay(pdMS_TO_TICKS(AUDIO_CHUNK_DURATION_MS));
        } else if (!streamingActive) {
            // When not streaming, wait longer and ensure audio is stopped
            if (audioStreamer && audioStreamer->isReady()) {
                audioStreamer->clearBuffers();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            // Small delay for timing
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

// PCM streaming task (runs on Core 0) - fetches data from server
void streamingTask(void* parameter) {
    Serial.println("PCM streaming task started on Core 0");
    
    HTTPClient http;
    WiFiClient client;
    
    while (true) {
        if (streamingRequested && WiFi.status() == WL_CONNECTED) {
            Serial.println("Starting PCM stream connection...");
            
            // Configure HTTP client with better settings for streaming
            String url = String("http://") + PCM_SERVER_HOST + ":" + PCM_SERVER_PORT + PCM_STREAM_PATH;
            http.begin(client, url);
            http.setTimeout(30000); // 30 second timeout for streaming
            http.addHeader("User-Agent", "ESP32-RadioBenziger/1.0");
            http.addHeader("Connection", "keep-alive");
            http.addHeader("Cache-Control", "no-cache");
            
            // Start HTTP request
            int httpResponseCode = http.GET();
            
            if (httpResponseCode == 200) {
                Serial.println("✅ Connected to PCM stream");
                
                // Get stream
                WiFiClient* stream = http.getStreamPtr();
                if (stream) {
                    Serial.println("Pre-buffering audio data...");
                    
                    // Pre-buffer some data before starting audio playback
                    int preBufferCount = 0;
                    static uint8_t preBuffer[HTTP_BUFFER_SIZE];  // Static to save stack space
                    int16_t* preBufferPCM = (int16_t*)preBuffer;
                    
                    while (preBufferCount < 8 && streamingRequested && stream->connected()) {
                        int bytesRead = stream->readBytes(preBuffer, HTTP_BUFFER_SIZE);
                        if (bytesRead > 0) {
                            size_t sampleCount = bytesRead / sizeof(int16_t);
                            size_t offset = 0;
                            
                            while (offset < sampleCount && preBufferCount < 8) {
                                size_t chunkSize = (sampleCount - offset) > AUDIO_CHUNK_SAMPLES ? 
                                                  AUDIO_CHUNK_SAMPLES : (sampleCount - offset);
                                
                                AudioBuffer buffer;
                                buffer.sampleCount = chunkSize;
                                buffer.isValid = true;
                                buffer.isSilence = false;
                                memcpy(buffer.samples, &preBufferPCM[offset], chunkSize * sizeof(int16_t));
                                
                                if (audioBufferQueue != nullptr) {
                                    xQueueSend(audioBufferQueue, &buffer, pdMS_TO_TICKS(100));
                                    preBufferCount++;
                                }
                                
                                offset += chunkSize;
                            }
                        } else {
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }
                    }
                    
                    Serial.printf("Pre-buffered %d audio buffers\n", preBufferCount);
                    
                    // Wait for queue to fill up before starting audio playback
                    Serial.println("Waiting for queue to fill before starting audio...");
                    int waitCycles = 0;
                    while (waitCycles < 50 && streamingRequested) { // Max 5 seconds wait
                        UBaseType_t queueCount = uxQueueMessagesWaiting(audioBufferQueue);
                        Serial.printf("Queue fill status: %u/20 buffers\n", queueCount);
                        
                        if (queueCount >= 15) {
                            Serial.printf("✅ Queue sufficiently filled (%u/20), starting audio playback\n", queueCount);
                            break;
                        }
                        
                        vTaskDelay(pdMS_TO_TICKS(100)); // Wait 100ms between checks
                        waitCycles++;
                    }
                    
                    streamingActive = true;
                    Serial.println("🎵 Audio playback started!");
                    
                    // Buffer for reading HTTP data (static to save stack space)
                    static uint8_t httpBuffer[HTTP_BUFFER_SIZE];
                    int16_t* pcmBuffer = (int16_t*)httpBuffer;
                    
                    while (streamingRequested && stream->connected()) {
                        // Read PCM data from HTTP stream - read multiple chunks to fill queue
                        while (streamingRequested && stream->connected()) {
                            int bytesRead = stream->readBytes(httpBuffer, HTTP_BUFFER_SIZE);
                            
                            if (bytesRead > 0) {
                                // Convert bytes to sample count (16-bit samples)
                                size_t sampleCount = bytesRead / sizeof(int16_t);
                                
                                // Process data in chunks that match our buffer size
                                size_t offset = 0;
                                while (offset < sampleCount && streamingRequested) {
                                    size_t chunkSize = (sampleCount - offset) > AUDIO_CHUNK_SAMPLES ? 
                                                      AUDIO_CHUNK_SAMPLES : (sampleCount - offset);
                                    
                                    // Create buffer for this chunk
                                    AudioBuffer buffer;
                                    buffer.sampleCount = chunkSize;
                                    buffer.isValid = true;
                                    buffer.isSilence = false;
                                    
                                    // Copy chunk data
                                    memcpy(buffer.samples, &pcmBuffer[offset], chunkSize * sizeof(int16_t));
                                    
                                    // Send to audio playback queue
                                    if (audioBufferQueue != nullptr) {
                                        // Check queue space
                                        UBaseType_t queueSpace = uxQueueSpacesAvailable(audioBufferQueue);
                                        
                                        if (queueSpace == 0) {
                                            // Queue full, remove oldest buffer and add new one
                                            AudioBuffer oldBuffer;
                                            xQueueReceive(audioBufferQueue, &oldBuffer, 0);
                                            static uint32_t dropCount = 0;
                                            dropCount++;
                                            if (dropCount % 20 == 0) {
                                                Serial.printf("Buffer overflow: dropped %u buffers\n", dropCount);
                                            }
                                        }
                                        
                                        // Add new buffer
                                        if (xQueueSend(audioBufferQueue, &buffer, pdMS_TO_TICKS(10)) != pdTRUE) {
                                            Serial.println("Failed to queue audio buffer");
                                        }
                                    }
                                    
                                    offset += chunkSize;
                                }
                                
                                // Check if we should fill the queue more aggressively
                                UBaseType_t queueCount = uxQueueMessagesWaiting(audioBufferQueue);
                                if (queueCount < 2) {
                                    // Queue is low, continue reading without delay
                                    continue;
                                } else if (queueCount >= 6) {
                                    // Queue is getting full, slow down
                                    vTaskDelay(pdMS_TO_TICKS(20));
                                    break;
                                } else {
                                    // Normal level, small delay
                                    vTaskDelay(pdMS_TO_TICKS(5));
                                    break;
                                }
                            } else {
                                // No data available, wait a bit
                                vTaskDelay(pdMS_TO_TICKS(10));
                                break;
                            }
                        }
                    }
                    
                    streamingActive = false;
                    Serial.println("PCM stream disconnected");
                }
            } else {
                Serial.printf("❌ HTTP request failed: %d\n", httpResponseCode);
                
                // Send silence buffers when connection fails to prevent noise
                for (int i = 0; i < 5; i++) {
                    if (!streamingRequested) break;
                    
                    AudioBuffer silenceBuffer = AudioBuffer::createSilence();
                    if (audioBufferQueue != nullptr) {
                        xQueueSend(audioBufferQueue, &silenceBuffer, 0);
                    }
                    vTaskDelay(pdMS_TO_TICKS(AUDIO_CHUNK_DURATION_MS));
                }
                
                vTaskDelay(pdMS_TO_TICKS(3000)); // Wait 3 seconds before retry
            }
            
            http.end();
        } else {
            // Not streaming, wait longer
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        // Small delay between connection attempts
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Start PCM streaming
void startPCMStreaming() {
    if (xSemaphoreTake(streamingMutex, portMAX_DELAY) == pdTRUE) {
        if (!streamingRequested) {
            streamingRequested = true;
            Serial.println("PCM streaming requested");
            
            // Clear audio buffer queue
            if (audioBufferQueue != nullptr) {
                AudioBuffer buffer;
                while (xQueueReceive(audioBufferQueue, &buffer, 0) == pdTRUE) {
                    // Clear all queued buffers
                }
            }
        }
        xSemaphoreGive(streamingMutex);
    }
}

// Stop PCM streaming
void stopPCMStreaming() {
    if (xSemaphoreTake(streamingMutex, portMAX_DELAY) == pdTRUE) {
        if (streamingRequested) {
            streamingRequested = false;
            streamingActive = false;
            Serial.println("PCM streaming stopped");
            
            // Clear audio buffer queue
            if (audioBufferQueue != nullptr) {
                AudioBuffer buffer;
                while (xQueueReceive(audioBufferQueue, &buffer, 0) == pdTRUE) {
                    // Clear all queued buffers
                }
            }
            
            if (audioStreamer) {
                audioStreamer->clearBuffers();
            }
        }
        xSemaphoreGive(streamingMutex);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Radio Benziger PCM Streaming System");
    
    // Initialize configuration
    config.begin();
    
    // Initialize WiFi
    wifiManager.begin();
    
    if (config.hasWiFiCredentials()) {
        Serial.println("Connecting to WiFi...");
        WiFiManager::connectToWiFi(config.settings.wifiSSID, config.settings.wifiPassword);
        
        // Wait for connection
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            Serial.println();
            Serial.println("WiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println();
            Serial.println("WiFi connection failed, starting AP mode");
            wifiManager.startConfigMode();
        }
    } else {
        Serial.println("No WiFi credentials found, starting AP mode");
        wifiManager.startConfigMode();
    }
    
    // Initialize audio system
    Serial.println("Initializing audio system...");
    
    // Create mutex for streaming control
    streamingMutex = xSemaphoreCreateMutex();
    if (streamingMutex == nullptr) {
        Serial.println("❌ Failed to create streaming mutex");
        return;
    }
    
    // Create audio buffer queue
    audioBufferQueue = xQueueCreate(AUDIO_BUFFER_QUEUE_SIZE, sizeof(AudioBuffer));
    if (audioBufferQueue == nullptr) {
        Serial.println("❌ Failed to create audio buffer queue");
        return;
    }
    
    // Create audio config with safe buffer settings
    PCMStreamer::AudioConfig audioConfig;
    audioConfig.sampleRate = 32000;
    audioConfig.bitsPerSample = 16;
    audioConfig.channels = 1;        // Mono audio for single speaker
    audioConfig.bufferSize = 1024;   // Larger buffer for stability
    audioConfig.bufferCount = 8;     // More buffers for smoother playback
    audioConfig.useAPLL = false;
    
    PCMStreamer::PinConfig pinConfig; // Uses default pins (BCLK=25, LRCK=26, DIN=27)
    
    audioStreamer = new PCMStreamer(audioConfig, pinConfig, I2S_NUM_0);
    
    if (audioStreamer->begin()) {
        audioInitialized = true;
        Serial.println("✅ Audio system initialized successfully");
        audioStreamer->printDiagnostics();
        
        // Create audio playback task (Core 0) - can handle more congestion
        BaseType_t result = xTaskCreatePinnedToCore(
            audioTask,          // Task function
            "AudioPlayback",    // Task name
            12288,             // Increased stack size for Core 0 (was 8192)
            nullptr,           // Parameters
            2,                 // Priority (higher than default)
            &audioTaskHandle,  // Task handle
            0                  // Core 0 (more congested, but audio is more resilient)
        );
        
        if (result == pdPASS) {
            Serial.println("✅ Audio playback task created successfully on Core 0 (12KB stack)");
        } else {
            Serial.println("❌ Failed to create audio playback task");
        }
        
        // Create PCM streaming task (Core 1) - prioritize HTTP streaming
        BaseType_t streamingResult = xTaskCreatePinnedToCore(
            streamingTask,         // Task function
            "PCMStreaming",        // Task name
            16384,                // Increased stack size for HTTP + pre-buffering
            nullptr,              // Parameters
            3,                    // Higher priority than audio for HTTP streaming
            &streamingTaskHandle, // Task handle
            1                     // Core 1 (less congested, prioritize HTTP)
        );
        
        if (streamingResult == pdPASS) {
            Serial.println("✅ PCM streaming task created successfully on Core 1 (prioritized)");
        } else {
            Serial.println("❌ Failed to create PCM streaming task");
        }
    } else {
        Serial.println("❌ Failed to initialize audio system");
    }
    
    // Setup web server routes
    server.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<title>Radio Benziger PCM Streaming</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>";
        html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
        html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
        html += "h1 { color: #333; text-align: center; }";
        html += ".status { padding: 10px; margin: 10px 0; border-radius: 5px; }";
        html += ".connected { background: #d4edda; color: #155724; }";
        html += ".disconnected { background: #f8d7da; color: #721c24; }";
        html += ".controls { text-align: center; margin: 20px 0; }";
        html += "button { padding: 10px 20px; margin: 5px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; }";
        html += ".start { background: #28a745; color: white; }";
        html += ".stop { background: #dc3545; color: white; }";
        html += ".info { background: #17a2b8; color: white; }";
        html += ".settings { margin: 20px 0; padding: 15px; background: #f8f9fa; border-radius: 5px; }";
        html += "</style></head><body>";
        html += "<div class='container'>";
        html += "<h1>Radio Benziger PCM Streaming</h1>";
        
        html += "<div class='status " + String(isConnected ? "connected" : "disconnected") + "'>";
        html += "WiFi: " + String(isConnected ? "Connected" : "Disconnected");
        html += "</div>";
        
        html += "<div class='status " + String(streamingActive ? "connected" : "disconnected") + "'>";
        html += "PCM Stream: " + String(streamingActive ? "Active" : "Inactive");
        html += "</div>";
        
        html += "<div class='controls'>";
        html += "<button class='start' onclick='startStream()'>Start Stream</button>";
        html += "<button class='stop' onclick='stopStream()'>Stop Stream</button>";
        html += "<button class='info' onclick='getStatus()'>Status</button>";
        html += "</div>";
        
        html += "<div class='settings'>";
        html += "<h3>Stream Settings</h3>";
        html += "<p><strong>Server:</strong> " + String(PCM_SERVER_HOST) + ":" + String(PCM_SERVER_PORT) + "</p>";
        html += "<p><strong>Format:</strong> 32kHz, 16-bit, Mono to Stereo</p>";
        html += "<p><strong>Buffer:</strong> " + String(AUDIO_CHUNK_SAMPLES) + " samples (25ms)</p>";
        html += "<p><strong>Queue:</strong> " + String(AUDIO_BUFFER_QUEUE_SIZE) + " buffers</p>";
        html += "</div>";
        
        html += "<div id='status-info'></div>";
        html += "</div>";
        
        html += "<script>";
        html += "function startStream() {";
        html += "  fetch('/start-stream', {method: 'POST'})";
        html += "    .then(response => response.text())";
        html += "    .then(data => { alert(data); setTimeout(() => location.reload(), 1000); });";
        html += "}";
        html += "function stopStream() {";
        html += "  fetch('/stop-stream', {method: 'POST'})";
        html += "    .then(response => response.text())";
        html += "    .then(data => { alert(data); setTimeout(() => location.reload(), 1000); });";
        html += "}";
        html += "function getStatus() {";
        html += "  fetch('/status')";
        html += "    .then(response => response.json())";
        html += "    .then(data => {";
        html += "      document.getElementById('status-info').innerHTML = ";
        html += "        '<div class=\"settings\"><h3>System Status</h3><pre>' + ";
        html += "        JSON.stringify(data, null, 2) + '</pre></div>';";
        html += "    });";
        html += "}";
        html += "</script></body></html>";
        
        server.send(200, "text/html", html);
    });
    
    server.on("/start-stream", HTTP_POST, []() {
        startPCMStreaming();
        server.send(200, "text/plain", "PCM streaming started");
    });
    
    server.on("/stop-stream", HTTP_POST, []() {
        stopPCMStreaming();
        server.send(200, "text/plain", "PCM streaming stopped");
    });
    
    server.on("/status", HTTP_GET, []() {
        String json = "{";
        json += "\"wifi_connected\":" + String(isConnected ? "true" : "false") + ",";
        json += "\"audio_initialized\":" + String(audioInitialized ? "true" : "false") + ",";
        json += "\"streaming_requested\":" + String(streamingRequested ? "true" : "false") + ",";
        json += "\"streaming_active\":" + String(streamingActive ? "true" : "false") + ",";
        json += "\"server_host\":\"" + String(PCM_SERVER_HOST) + "\",";
        json += "\"server_port\":" + String(PCM_SERVER_PORT) + ",";
        json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"queue_count\":" + String(audioBufferQueue ? uxQueueMessagesWaiting(audioBufferQueue) : 0);
        json += "}";
        server.send(200, "application/json", json);
    });
    
    server.begin();
    Serial.println("✅ Web server started on port 80");
    Serial.println("Ready for PCM streaming!");
}

void loop() {
    server.handleClient();
    delay(10);
}