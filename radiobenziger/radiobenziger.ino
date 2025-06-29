#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include "MP3DecoderHelix.h"
#include "Config.h"
#include "WiFiManager.h"
#include "I2SDetector.h"

using namespace libhelix;

// I2S pins for MAX98357A DAC
#define I2S_DOUT      27  // DIN pin
#define I2S_BCLK      25  // BCLK pin  
#define I2S_LRC       26  // LRC pin

// Global objects
Config config;
WiFiManager wifiManager;
WebServer server(80);
I2SDetector i2sDetector;
HTTPClient httpClient;
WiFiClient wifiClient;

// Audio objects
MP3DecoderHelix mp3Decoder;
i2s_config_t i2sConfig;

// Audio state
bool isPlaying = false;
bool isConnected = false;
String currentStreamTitle = "";
int currentVolume = 75;
uint8_t httpBuffer[4096];
size_t httpBufferLen = 0;

// MP3 callback function - called when PCM data is decoded
void mp3DataCallback(MP3FrameInfo &info, int16_t *pcm_buffer, size_t len, void* userData) {
    if (isPlaying && len > 0) {
        // Write PCM data directly to I2S
        size_t bytes_written = 0;
        i2s_write(I2S_NUM_0, pcm_buffer, len * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Radio Benziger with Helix MP3 Decoder");
    
    // Initialize configuration
    config.begin();
    
    // Initialize I2S for audio output
    setupI2S();
    
    // Initialize MP3 decoder with callback
    mp3Decoder.setDataCallback(mp3DataCallback);
    mp3Decoder.begin();
    
    // Initialize I2S detector
    i2sDetector.begin();
    
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
            Serial.println("\nWiFi connected!");
            Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
            
            // Start web server
            setupWebServer();
            server.begin();
            Serial.println("Web server started");
            
            // Auto-start stream if configured
            if (config.settings.autoStart) {
                Serial.println("Auto-starting stream...");
                startStream();
            }
        } else {
            Serial.println("\nWiFi connection failed!");
        }
    } else {
        Serial.println("No WiFi credentials configured");
    }
    
    printStatus();
}

void setupI2S() {
    // Configure I2S
    i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 32000,  // Will be updated based on stream
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    // Install and start I2S driver
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pin_config));
    ESP_ERROR_CHECK(i2s_start(I2S_NUM_0));
    
    Serial.println("I2S initialized");
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/play", handlePlay);
    server.on("/stop", handleStop);
    server.on("/status", handleStatus);
    server.on("/volume", HTTP_POST, handleVolume);
    server.onNotFound(handleNotFound);
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Radio Benziger</title>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#333;text-align:center;margin-bottom:30px;}";
    html += ".button{background:#007bff;color:white;padding:12px 24px;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin:5px;}";
    html += ".button:hover{background:#0056b3;}";
    html += ".stop{background:#dc3545;}.stop:hover{background:#c82333;}";
    html += ".status{background:#f8f9fa;padding:15px;border-radius:5px;margin:20px 0;}";
    html += "input[type=range]{width:100%;margin:10px 0;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>🎵 Radio Benziger</h1>";
    html += "<div class='status' id='status'>Loading...</div>";
    html += "<button class='button' onclick='play()'>▶️ Play</button>";
    html += "<button class='button stop' onclick='stop()'>⏹️ Stop</button>";
    html += "<br><br>Volume: <input type='range' min='0' max='100' value='75' id='volume' onchange='setVolume(this.value)'>";
    html += "<script>";
    html += "function play(){fetch('/play').then(updateStatus);}";
    html += "function stop(){fetch('/stop').then(updateStatus);}";
    html += "function setVolume(v){fetch('/volume',{method:'POST',body:'volume='+v,headers:{'Content-Type':'application/x-www-form-urlencoded'}}).then(updateStatus);}";
    html += "function updateStatus(){fetch('/status').then(r=>r.json()).then(d=>{document.getElementById('status').innerHTML='Playing: '+d.playing+'<br>Volume: '+d.volume+'%<br>Stream: '+d.url;});}";
    html += "updateStatus();setInterval(updateStatus,2000);";
    html += "</script></div></body></html>";
    
    server.send(200, "text/html", html);
}

void handlePlay() {
    if (startStream()) {
        server.send(200, "text/plain", "Stream started");
    } else {
        server.send(500, "text/plain", "Failed to start stream");
    }
}

void handleStop() {
    stopStream();
    server.send(200, "text/plain", "Stream stopped");
}

void handleStatus() {
    String json = "{";
    json += "\"playing\":" + String(isPlaying ? "true" : "false") + ",";
    json += "\"volume\":" + String(currentVolume) + ",";
    json += "\"url\":\"" + String(config.settings.streamURL) + "\",";
    json += "\"connected\":" + String(isConnected ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

void handleVolume() {
    if (server.hasArg("volume")) {
        currentVolume = server.arg("volume").toInt();
        currentVolume = constrain(currentVolume, 0, 100);
        // Note: Volume control would need to be implemented in PCM processing
        Serial.printf("Volume set to: %d%%\n", currentVolume);
    }
    server.send(200, "text/plain", "OK");
}

void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

bool startStream() {
    if (isPlaying) {
        stopStream();
    }
    
    Serial.println("Starting stream...");
    String url = String(config.settings.streamURL);
    Serial.println("Connecting to: " + url);
    
    // Configure HTTP client
    httpClient.begin(wifiClient, url);
    httpClient.addHeader("User-Agent", "RadioBenziger/1.0");
    httpClient.addHeader("Icy-MetaData", "1");
    httpClient.setTimeout(10000);
    
    // Start connection
    int httpCode = httpClient.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        Serial.println("HTTP connection successful");
        isConnected = true;
        isPlaying = true;
        
        // Reset MP3 decoder
        mp3Decoder.begin();
        
        return true;
    } else {
        Serial.printf("HTTP connection failed: %d\n", httpCode);
        httpClient.end();
        return false;
    }
}

void stopStream() {
    Serial.println("Stopping stream...");
    isPlaying = false;
    isConnected = false;
    httpClient.end();
    
    // Clear I2S buffer
    i2s_zero_dma_buffer(I2S_NUM_0);
    
    Serial.println("Stream stopped");
}

void loop() {
    server.handleClient();
    
    if (isPlaying && isConnected) {
        // Read data from HTTP stream
        WiFiClient* stream = httpClient.getStreamPtr();
        if (stream && stream->available()) {
            size_t available = stream->available();
            if (available > 0) {
                size_t toRead = min(available, sizeof(httpBuffer) - httpBufferLen);
                size_t bytesRead = stream->readBytes(httpBuffer + httpBufferLen, toRead);
                httpBufferLen += bytesRead;
                
                // Feed data to MP3 decoder
                if (httpBufferLen > 0) {
                    size_t processed = mp3Decoder.write(httpBuffer, httpBufferLen);
                    
                    // Shift remaining data
                    if (processed > 0 && processed < httpBufferLen) {
                        memmove(httpBuffer, httpBuffer + processed, httpBufferLen - processed);
                        httpBufferLen -= processed;
                    } else if (processed >= httpBufferLen) {
                        httpBufferLen = 0;
                    }
                }
            }
        } else if (!stream || !stream->connected()) {
            Serial.println("Stream disconnected");
            stopStream();
        }
    }
    
    // Handle WiFi reconnection
    if (WiFi.status() != WL_CONNECTED && config.hasWiFiCredentials()) {
        Serial.println("WiFi disconnected, attempting reconnection...");
        WiFiManager::connectToWiFi(config.settings.wifiSSID, config.settings.wifiPassword);
    }
    
    delay(10); // Small delay to prevent watchdog issues
}

void printStatus() {
    Serial.println("===================================");
    Serial.println("=== Radio Benziger Status ===");
    Serial.printf("  Audio State: %s\n", isPlaying ? "Playing" : "Stopped");
    Serial.printf("  Connected: %s\n", isConnected ? "Yes" : "No");
    Serial.printf("  Volume: %d%%\n", currentVolume);
    Serial.printf("  Stream URL: %s\n", config.settings.streamURL);
    Serial.printf("  I2S Devices: %d\n", i2sDetector.getDeviceCount());
    Serial.printf("  Free Heap: %d bytes\n", ESP.getFreeHeap());
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("  WiFi: %s (%s)\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
        Serial.println("  WiFi: Disconnected");
    }
    Serial.println("===================================");
}