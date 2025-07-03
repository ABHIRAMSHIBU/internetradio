#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "Config.h"
#include "WiFiManager.h"
#include "PCMStreamer.h"

// Global objects
Config config;
WiFiManager wifiManager;
WebServer server(80);
HTTPClient httpClient;
WiFiClient wifiClient;

// Audio objects
PCMStreamer* audioStreamer = nullptr;
bool audioInitialized = false;
bool audioPlaying = false;

// Continuous audio control
TaskHandle_t audioTaskHandle = nullptr;
float currentToneFrequency = 0.0f;
bool continuousAudio = false;
SemaphoreHandle_t audioMutex = nullptr;

// Audio generation parameters
const size_t AUDIO_BUFFER_QUEUE_SIZE = 4;      // Reduced queue size to save memory
const size_t AUDIO_CHUNK_SAMPLES = 800;        // Smaller chunks (0.025 seconds at 32kHz)
const size_t AUDIO_CHUNK_DURATION_MS = 25;     // 25ms per chunk

// Audio buffer structure with fixed-size array (safer for FreeRTOS queues)
struct AudioBuffer {
    int16_t samples[AUDIO_CHUNK_SAMPLES * 2]; // Fixed size: stereo samples
    size_t sampleCount;
    float frequency;
    bool isValid;
    
    AudioBuffer() : sampleCount(0), frequency(0.0f), isValid(false) {
        memset(samples, 0, sizeof(samples));
    }
    
    AudioBuffer(const std::vector<int16_t>& data, float freq) : 
        sampleCount(data.size()), frequency(freq), isValid(true) {
        memset(samples, 0, sizeof(samples));
        if (data.size() <= sizeof(samples)/sizeof(samples[0])) {
            memcpy(samples, data.data(), data.size() * sizeof(int16_t));
        }
    }
};

// Asynchronous audio generation
TaskHandle_t audioGeneratorTaskHandle = nullptr;
QueueHandle_t audioBufferQueue = nullptr;
SemaphoreHandle_t audioGeneratorMutex = nullptr;

// Audio generator state
volatile bool audioGeneratorRunning = false;
volatile float targetFrequency = 0.0f;
volatile bool frequencyChanged = false;

// Connection state
bool isConnected = false;

// Audio test data with continuity
std::vector<int16_t> generateTestTone(float frequency, float duration, uint32_t sampleRate, uint8_t channels) {
    // Static variables to maintain state between calls
    static float lastFrequency = 0.0f;
    static double phaseAccumulator = 0.0;
    static uint32_t lastSampleRate = 0;
    
    // Reset phase if frequency or sample rate changed
    if (frequency != lastFrequency || sampleRate != lastSampleRate) {
        phaseAccumulator = 0.0;
        lastFrequency = frequency;
        lastSampleRate = sampleRate;
        Serial.printf("Phase reset for frequency: %.1f Hz\n", frequency);
    }
    
    size_t sampleCount = (size_t)(duration * sampleRate);
    std::vector<int16_t> samples;
    samples.reserve(sampleCount * channels);
    
    // Calculate phase increment per sample
    double phaseIncrement = (2.0 * M_PI * frequency) / sampleRate;
    
    for (size_t i = 0; i < sampleCount; i++) {
        float amplitude = 0.2f; // 20% volume to avoid clipping
        int16_t sample = (int16_t)(amplitude * 32767.0f * sin(phaseAccumulator));
        
        // Add sample for each channel
        for (uint8_t ch = 0; ch < channels; ch++) {
            samples.push_back(sample);
        }
        
        // Advance phase accumulator
        phaseAccumulator += phaseIncrement;
        
        // Keep phase accumulator in reasonable range to prevent precision loss
        if (phaseAccumulator >= 2.0 * M_PI) {
            phaseAccumulator -= 2.0 * M_PI;
        }
    }
    
    return samples;
}

// Generate white noise samples
std::vector<int16_t> generateWhiteNoise(float duration, uint32_t sampleRate, uint8_t channels) {
    size_t sampleCount = (size_t)(duration * sampleRate);
    std::vector<int16_t> samples;
    samples.reserve(sampleCount * channels);
    
    for (size_t i = 0; i < sampleCount; i++) {
        int16_t sample = (int16_t)(0.1f * 32767.0f * ((float)random(-1000, 1000) / 1000.0f));
        
        // Add sample for each channel
        for (uint8_t ch = 0; ch < channels; ch++) {
            samples.push_back(sample);
        }
    }
    
    return samples;
}

// Reset tone generator phase (call when changing frequency or stopping/starting)
void resetToneGenerator() {
    // This will force generateTestTone to reset its phase on next call
    generateTestTone(0.0f, 0.0f, 0, 0);
}

// Asynchronous audio generator task (runs on Core 0)
void audioGeneratorTask(void* parameter) {
    Serial.println("Audio generator task started on Core 0");
    
    while (true) {
        if (audioGeneratorRunning) {
            std::vector<int16_t> samples;
            float chunkDuration = (float)AUDIO_CHUNK_SAMPLES / 32000.0f; // Duration in seconds
            
            if (targetFrequency > 0.0f) {
                // Generate tone
                if (frequencyChanged) {
                    Serial.printf("Frequency changed to %.1f Hz\n", targetFrequency);
                    resetToneGenerator();
                    frequencyChanged = false;
                }
                samples = generateTestTone(targetFrequency, chunkDuration, 32000, 2);
            } else if (targetFrequency < 0.0f) {
                // Generate white noise
                samples = generateWhiteNoise(chunkDuration, 32000, 2);
            } else {
                // targetFrequency == 0, no audio generation
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            // Create audio buffer
            AudioBuffer buffer(samples, targetFrequency);
            
            // Try to send to queue (non-blocking)
            if (audioBufferQueue != nullptr) {
                if (xQueueSend(audioBufferQueue, &buffer, 0) != pdTRUE) {
                    // Queue full, skip this buffer (this prevents blocking)
                    Serial.println("Audio buffer queue full, skipping buffer");
                }
            }
        } else {
            // No audio generation needed, wait longer
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Adaptive delay based on queue status
        UBaseType_t queueCount = uxQueueMessagesWaiting(audioBufferQueue);
        if (queueCount >= AUDIO_BUFFER_QUEUE_SIZE - 1) {
            // Queue almost full, wait longer
            vTaskDelay(pdMS_TO_TICKS(AUDIO_CHUNK_DURATION_MS));
        } else {
            // Queue has space, generate at normal rate
            vTaskDelay(pdMS_TO_TICKS(AUDIO_CHUNK_DURATION_MS / 2));
        }
    }
}

// Audio playback task (runs on Core 1) - consumes from buffer queue
void audioTask(void* parameter) {
    Serial.println("Audio playback task started on Core 1");
    
    // Static buffer to avoid stack allocation
    static std::vector<int16_t> sampleVector;
    sampleVector.reserve(AUDIO_CHUNK_SAMPLES * 2); // Reserve space once
    
    while (true) {
        if (continuousAudio && audioInitialized && audioStreamer && audioStreamer->isReady()) {
            // Try to get audio buffer from queue
            AudioBuffer buffer;
            if (audioBufferQueue != nullptr && xQueueReceive(audioBufferQueue, &buffer, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (buffer.isValid && buffer.sampleCount > 0) {
                    // Reuse static vector to avoid stack allocation
                    sampleVector.clear();
                    sampleVector.assign(buffer.samples, buffer.samples + buffer.sampleCount);
                    
                    // Write buffered audio to streamer
                    size_t bytesWritten = audioStreamer->writeSamples(sampleVector);
                    if (bytesWritten == 0) {
                        Serial.println("Warning: Audio write failed");
                    } else {
                        // Successfully played buffer
                        static uint32_t bufferCount = 0;
                        bufferCount++;
                        if (bufferCount % 20 == 0) { // Print every second (20 * 50ms)
                            Serial.printf("Played %u audio buffers (%.1f Hz)\n", bufferCount, buffer.frequency);
                        }
                    }
                }
            } else {
                // No buffer available - this is normal when no audio is playing
                // The generator task handles all audio generation including white noise
            }
        } else {
            // Audio not active, wait longer
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Small delay for audio timing
        vTaskDelay(pdMS_TO_TICKS(25)); // 25ms delay for smooth playback
    }
}

// Start continuous tone with async generation
void startContinuousTone(float frequency) {
    // Update target frequency for async generator
    if (targetFrequency != frequency) {
        targetFrequency = frequency;
        frequencyChanged = true;
    }
    
    // Start async audio generation
    audioGeneratorRunning = true;
    
    // Start audio playback
    if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE) {
        currentToneFrequency = frequency;
        continuousAudio = true;
        audioPlaying = true;
        Serial.printf("Started continuous tone: %.1f Hz (async mode)\n", frequency);
        xSemaphoreGive(audioMutex);
    }
}

// Stop continuous audio and async generation
void stopContinuousAudio() {
    // Stop async audio generation
    audioGeneratorRunning = false;
    targetFrequency = 0.0f;
    
    // Clear audio buffer queue
    if (audioBufferQueue != nullptr) {
        AudioBuffer buffer;
        while (xQueueReceive(audioBufferQueue, &buffer, 0) == pdTRUE) {
            // Clear all queued buffers
        }
    }
    
    // Stop audio playback
    if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE) {
        continuousAudio = false;
        audioPlaying = false;
        currentToneFrequency = 0.0f;
        resetToneGenerator(); // Reset phase when stopping
        if (audioStreamer) {
            audioStreamer->clearBuffers();
        }
        Serial.println("Stopped continuous audio (async mode)");
        xSemaphoreGive(audioMutex);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Radio Benziger Audio Test System");
    
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
    
    // Create mutex for audio control
    audioMutex = xSemaphoreCreateMutex();
    if (audioMutex == nullptr) {
        Serial.println("❌ Failed to create audio mutex");
        return;
    }
    
    // Create audio buffer queue
    audioBufferQueue = xQueueCreate(AUDIO_BUFFER_QUEUE_SIZE, sizeof(AudioBuffer));
    if (audioBufferQueue == nullptr) {
        Serial.println("❌ Failed to create audio buffer queue");
        return;
    }
    
    // Create mutex for audio generator
    audioGeneratorMutex = xSemaphoreCreateMutex();
    if (audioGeneratorMutex == nullptr) {
        Serial.println("❌ Failed to create audio generator mutex");
        return;
    }
    
    // Create audio config with safer buffer settings
    PCMStreamer::AudioConfig audioConfig;
    audioConfig.sampleRate = 32000;
    audioConfig.bitsPerSample = 16;
    audioConfig.channels = 2;
    audioConfig.bufferSize = 512;    // Smaller buffer to avoid memory issues
    audioConfig.bufferCount = 4;     // Fewer buffers
    audioConfig.useAPLL = false;
    
    PCMStreamer::PinConfig pinConfig; // Uses default pins we already updated
    
    audioStreamer = new PCMStreamer(audioConfig, pinConfig, I2S_NUM_0);
    
    if (audioStreamer->begin()) {
        audioInitialized = true;
        Serial.println("✅ Audio system initialized successfully");
        audioStreamer->printDiagnostics();
        
        // Create audio playback task (Core 1)
        BaseType_t result = xTaskCreatePinnedToCore(
            audioTask,          // Task function
            "AudioPlayback",    // Task name
            8192,              // Increased stack size for vector operations
            nullptr,           // Parameters
            2,                 // Priority (higher than default)
            &audioTaskHandle,  // Task handle
            1                  // Core 1 (opposite of main task)
        );
        
        if (result == pdPASS) {
            Serial.println("✅ Audio playback task created successfully");
        } else {
            Serial.println("❌ Failed to create audio playback task");
        }
        
        // Create async audio generator task (Core 0)
        BaseType_t generatorResult = xTaskCreatePinnedToCore(
            audioGeneratorTask,         // Task function
            "AudioGenerator",           // Task name
            6144,                      // Reduced stack size
            nullptr,                   // Parameters
            1,                         // Lower priority than playback
            &audioGeneratorTaskHandle, // Task handle
            0                          // Core 0 (same as main task)
        );
        
        if (generatorResult == pdPASS) {
            Serial.println("✅ Audio generator task created successfully");
        } else {
            Serial.println("❌ Failed to create audio generator task");
        }
    } else {
        Serial.println("❌ Failed to initialize audio system");
        delete audioStreamer;
        audioStreamer = nullptr;
    }
    
    // Setup web server routes
    setupWebServer();
    
    // Start web server
    server.begin();
    Serial.println("Web server started");
    
    // Print final status
    Serial.println("=== System Status ===");
    Serial.printf("WiFi: %s\n", isConnected ? "Connected" : "AP Mode");
    Serial.printf("Audio: %s\n", audioInitialized ? "Ready" : "Failed");
    if (isConnected) {
        Serial.printf("Web Interface: http://%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("Web Interface: http://192.168.4.1");
    }
    Serial.println("=====================");
}

void loop() {
    // Handle web server
    server.handleClient();
    
    // Print IP address periodically when connected
    static unsigned long lastIPPrint = 0;
    if (isConnected && millis() - lastIPPrint > 5000) {
        Serial.printf("IP: %s | Audio: %s\n", 
                     WiFi.localIP().toString().c_str(),
                     audioInitialized ? (audioPlaying ? "Playing" : "Ready") : "Failed");
        lastIPPrint = millis();
    }
    
    // Print system status periodically
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 30000) {
        printSystemStatus();
        lastStatusPrint = millis();
    }
    
    // Check WiFi connection
    if (isConnected && WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, attempting reconnection...");
        isConnected = false;
        WiFiManager::connectToWiFi(config.settings.wifiSSID, config.settings.wifiPassword);
    }
    
    delay(100);
}

void setupWebServer() {
    // Home page with audio controls
    server.on("/", HTTP_GET, []() {
        String html = "<!DOCTYPE html><html><head><title>Radio Benziger Audio Test</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>body{font-family:Arial;margin:40px;} .btn{padding:10px 20px;margin:5px;border:none;border-radius:5px;cursor:pointer;} .btn-primary{background:#007bff;color:white;} .btn-success{background:#28a745;color:white;} .btn-danger{background:#dc3545;color:white;} .status{padding:10px;margin:10px 0;border-radius:5px;} .status-ok{background:#d4edda;border:1px solid #c3e6cb;} .status-error{background:#f8d7da;border:1px solid #f5c6cb;}</style>";
        html += "</head><body>";
        html += "<h1>🎵 Radio Benziger Audio Test</h1>";
        
        // System status
        html += "<div class='status " + String(audioInitialized ? "status-ok" : "status-error") + "'>";
        html += "<strong>Audio System:</strong> " + String(audioInitialized ? "✅ Ready" : "❌ Failed") + "<br>";
        html += "<strong>WiFi:</strong> " + String(isConnected ? "✅ Connected" : "📶 AP Mode") + "<br>";
        if (isConnected) {
            html += "<strong>IP:</strong> " + WiFi.localIP().toString() + "<br>";
        }
        html += "<strong>Free Memory:</strong> " + String(ESP.getFreeHeap()) + " bytes";
        html += "</div>";
        
        // Audio controls
        if (audioInitialized) {
            html += "<h2>Audio Test Controls</h2>";
            html += "<button class='btn btn-primary' onclick='playTone(440)'>Play A4 (440Hz)</button>";
            html += "<button class='btn btn-primary' onclick='playTone(880)'>Play A5 (880Hz)</button>";
            html += "<button class='btn btn-primary' onclick='playTone(1320)'>Play E6 (1320Hz)</button>";
            html += "<button class='btn btn-success' onclick='playWhiteNoise()'>White Noise</button>";
            html += "<button class='btn btn-danger' onclick='stopAudio()'>Stop Audio</button>";
            html += "<br><br>";
            html += "<button class='btn btn-primary' onclick='audioStatus()'>Audio Status</button>";
            html += "<button class='btn btn-primary' onclick='audioDiagnostics()'>Diagnostics</button>";
        }
        
        // Navigation
        html += "<h2>Configuration</h2>";
        html += "<a href='/config' class='btn btn-primary'>WiFi Config</a>";
        html += "<a href='/info' class='btn btn-primary'>System Info</a>";
        
        // JavaScript for audio controls
        html += "<script>";
        html += "function playTone(freq) { fetch('/audio/tone?freq=' + freq); }";
        html += "function playWhiteNoise() { fetch('/audio/noise'); }";
        html += "function stopAudio() { fetch('/audio/stop'); }";
        html += "function audioStatus() { fetch('/audio/status').then(r=>r.text()).then(t=>alert(t)); }";
        html += "function audioDiagnostics() { window.open('/audio/diagnostics'); }";
        html += "</script>";
        
        html += "</body></html>";
        server.send(200, "text/html", html);
    });
    
    // Audio control endpoints
    server.on("/audio/tone", HTTP_GET, handlePlayTone);
    server.on("/audio/noise", HTTP_GET, handlePlayNoise);
    server.on("/audio/stop", HTTP_GET, handleStopAudio);
    server.on("/audio/status", HTTP_GET, handleAudioStatus);
    server.on("/audio/diagnostics", HTTP_GET, handleAudioDiagnostics);
    
    // Configuration pages (keep existing ones)
    server.on("/config", HTTP_GET, handleConfigPage);
    server.on("/config", HTTP_POST, handleConfigSave);
    server.on("/info", HTTP_GET, handleInfoPage);
    server.on("/reset", HTTP_GET, handleReset);
}

void handlePlayTone() {
    if (!audioInitialized) {
        server.send(500, "text/plain", "Audio system not initialized");
        return;
    }
    
    float frequency = 440.0f; // Default A4
    if (server.hasArg("freq")) {
        frequency = server.arg("freq").toFloat();
    }
    
    Serial.printf("Starting continuous tone: %.1f Hz\n", frequency);
    
    // Start continuous tone playback
    startContinuousTone(frequency);
    
    server.send(200, "text/plain", "Playing continuous tone: " + String(frequency) + " Hz");
}

void handlePlayNoise() {
    if (!audioInitialized) {
        server.send(500, "text/plain", "Audio system not initialized");
        return;
    }
    
    Serial.println("Starting continuous white noise");
    
    // Stop any current tone and start noise (using a special frequency value)
    startContinuousTone(-1.0f); // -1 indicates white noise mode
    
    server.send(200, "text/plain", "Playing continuous white noise");
}

void handleStopAudio() {
    if (!audioInitialized) {
        server.send(500, "text/plain", "Audio system not initialized");
        return;
    }
    
    Serial.println("Stopping continuous audio");
    stopContinuousAudio();
    
    server.send(200, "text/plain", "Audio stopped");
}

void handleAudioStatus() {
    if (!audioInitialized) {
        server.send(500, "text/plain", "Audio system not initialized");
        return;
    }
    
    String status = "Audio Status:\\n";
    status += "Status: " + audioStreamer->getStatusString() + "\\n";
    status += "Playing: " + String(audioPlaying ? "Yes" : "No") + "\\n";
    status += "Buffer Utilization: " + String(audioStreamer->getBufferUtilization()) + "%\\n";
    status += "Total Bytes Written: " + String(audioStreamer->getTotalBytesWritten()) + "\\n";
    status += "Buffer Overflows: " + String(audioStreamer->getBufferOverflows()) + "\\n";
    status += "Bytes Per Second: " + String(audioStreamer->getBytesPerSecond()) + "\\n";
    
    server.send(200, "text/plain", status);
}

void handleAudioDiagnostics() {
    if (!audioInitialized) {
        server.send(500, "text/plain", "Audio system not initialized");
        return;
    }
    
    String html = "<!DOCTYPE html><html><head><title>Audio Diagnostics</title>";
    html += "<style>body{font-family:monospace;margin:20px;} pre{background:#f5f5f5;padding:10px;border-radius:5px;}</style>";
    html += "</head><body><h1>Audio System Diagnostics</h1><pre>";
    
    // Capture diagnostics output
    html += "Status: " + audioStreamer->getStatusString() + "\\n";
    html += "Sample Rate: " + String(audioStreamer->getAudioConfig().sampleRate) + " Hz\\n";
    html += "Bits Per Sample: " + String(audioStreamer->getAudioConfig().bitsPerSample) + "\\n";
    html += "Channels: " + String(audioStreamer->getAudioConfig().channels) + "\\n";
    html += "Buffer Size: " + String(audioStreamer->getAudioConfig().bufferSize) + " bytes\\n";
    html += "Buffer Count: " + String(audioStreamer->getAudioConfig().bufferCount) + "\\n";
    html += "\\n";
    html += "Pin Configuration:\\n";
    html += "BCLK Pin: " + String(audioStreamer->getPinConfig().bclkPin) + "\\n";
    html += "LRCK Pin: " + String(audioStreamer->getPinConfig().lrckPin) + "\\n";
    html += "Data Pin: " + String(audioStreamer->getPinConfig().dataPin) + "\\n";
    html += "\\n";
    html += "Statistics:\\n";
    html += "Total Bytes Written: " + String(audioStreamer->getTotalBytesWritten()) + "\\n";
    html += "Total Packets: " + String(audioStreamer->getTotalPacketsProcessed()) + "\\n";
    html += "Buffer Overflows: " + String(audioStreamer->getBufferOverflows()) + "\\n";
    html += "Buffer Utilization: " + String(audioStreamer->getBufferUtilization()) + "%\\n";
    html += "Bytes Per Second: " + String(audioStreamer->getBytesPerSecond()) + "\\n";
    
    html += "</pre></body></html>";
    
    server.send(200, "text/html", html);
}

// Keep existing web handlers
void handleConfigPage() {
    // ... existing implementation from backup
    String html = "<!DOCTYPE html><html><head><title>WiFi Configuration</title></head><body>";
    html += "<h1>WiFi Configuration</h1>";
    html += "<form method='POST'>";
    html += "SSID: <input type='text' name='ssid' value='" + String(config.settings.wifiSSID) + "'><br><br>";
    html += "Password: <input type='password' name='password' value='" + String(config.settings.wifiPassword) + "'><br><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form>";
    html += "<br><a href='/'>Back to Home</a>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleConfigSave() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
        strncpy(config.settings.wifiSSID, server.arg("ssid").c_str(), sizeof(config.settings.wifiSSID) - 1);
        strncpy(config.settings.wifiPassword, server.arg("password").c_str(), sizeof(config.settings.wifiPassword) - 1);
        config.save();
        server.send(200, "text/html", "<html><body><h1>Settings Saved!</h1><a href='/'>Back to Home</a></body></html>");
    } else {
        server.send(400, "text/html", "<html><body><h1>Error: Missing parameters</h1><a href='/config'>Back</a></body></html>");
    }
}

void handleInfoPage() {
    String html = "<!DOCTYPE html><html><head><title>System Information</title></head><body>";
    html += "<h1>System Information</h1>";
    html += "<p><strong>Chip Model:</strong> " + String(ESP.getChipModel()) + "</p>";
    html += "<p><strong>Free Heap:</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";
    html += "<p><strong>Flash Size:</strong> " + String(ESP.getFlashChipSize()) + " bytes</p>";
    html += "<p><strong>WiFi Status:</strong> " + String(WiFi.status()) + "</p>";
    if (audioInitialized) {
        html += "<p><strong>Audio Status:</strong> " + audioStreamer->getStatusString() + "</p>";
    }
    html += "<br><a href='/'>Back to Home</a>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleReset() {
    server.send(200, "text/html", "<html><body><h1>Resetting...</h1></body></html>");
    delay(1000);
    ESP.restart();
}

void printSystemStatus() {
    Serial.println("=== System Status Report ===");
    Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("WiFi: %s\n", isConnected ? "Connected" : "Disconnected");
    if (isConnected) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
    }
    Serial.printf("Audio System: %s\n", audioInitialized ? "Ready" : "Failed");
    if (audioInitialized) {
        Serial.printf("Audio Status: %s\n", audioStreamer->getStatusString().c_str());
        Serial.printf("Audio Playing: %s\n", audioPlaying ? "Yes" : "No");
    }
    Serial.println("============================");
}