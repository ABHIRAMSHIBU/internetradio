#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>

// Include our custom modules
#include "Config.h"
#include "WiFiManager.h"
#include "I2CScanner.h"

// Pin definitions
#define LED_BUILTIN 2
#define I2S_BCLK 26
#define I2S_LRC 25
#define I2S_DIN 22
#define I2S_SD 21   // Optional shutdown pin for MAX98357A

// Global objects
WebServer server(80);

// Status variables
bool systemReady = false;
unsigned long lastStatusPrint = 0;
unsigned long lastIPPrint = 0;
const unsigned long STATUS_INTERVAL = 30000; // Print status every 30 seconds
const unsigned long IP_PRINT_INTERVAL = 5000; // Print IP every 5 seconds when connected

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("    Radio Benziger ESP32 Starting");
    Serial.println("========================================");
    
    // Initialize built-in LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    // Initialize configuration first (this loads saved settings)
    Serial.println("Initializing configuration...");
    if (!Config::begin()) {
        Serial.println("ERROR: Failed to initialize configuration!");
        return;
    }
    
    // Print current configuration status
    Config::printStatus();
    
    // Initialize I2C scanner
    Serial.println("Initializing I2C scanner...");
    if (I2CScanner::begin(21, 22)) {
        Serial.println("I2C scanner initialized successfully");
        I2CScanner::printScanResults();
    } else {
        Serial.println("WARNING: I2C scanner initialization failed");
    }
    
    // Initialize WiFi
    Serial.println("Initializing WiFi...");
    if (!WiFiManager::begin()) {
        Serial.println("ERROR: Failed to initialize WiFi manager!");
        return;
    }
    
    // Try to connect to saved WiFi if we have credentials
    if (Config::hasWiFiCredentials()) {
        Serial.println("Found saved WiFi credentials, attempting connection...");
        if (WiFiManager::connectToSaved()) {
            Serial.println("Connected to WiFi successfully!");
            Serial.printf("IP Address: %s\n", WiFiManager::getIP().c_str());
            Serial.printf("SSID: %s\n", WiFiManager::getSSID().c_str());
            
            // Start web server
            setupWebServer();
            server.begin();
            Serial.println("Web server started");
            
            digitalWrite(LED_BUILTIN, HIGH); // LED on = connected
            systemReady = true;
        } else {
            Serial.println("Failed to connect to saved WiFi");
            Serial.println("Starting configuration mode...");
            WiFiManager::startConfigMode();
            
            // Start web server for configuration
            setupWebServer();
            server.begin();
            Serial.println("Configuration web server started");
            Serial.printf("Connect to WiFi: %s\n", "RadioBenziger-Config");
            Serial.println("Open browser to: http://192.168.4.1");
            
            // Blink LED to indicate config mode
            systemReady = false;
        }
    } else {
        Serial.println("No saved WiFi credentials found");
        Serial.println("Starting configuration mode...");
        WiFiManager::startConfigMode();
        
        // Start web server for configuration
        setupWebServer();
        server.begin();
        Serial.println("Configuration web server started");
        Serial.printf("Connect to WiFi: %s\n", "RadioBenziger-Config");
        Serial.println("Open browser to: http://192.168.4.1");
        
        // Blink LED to indicate config mode
        systemReady = false;
    }
    
    Serial.println("Setup complete!");
    Serial.println("========================================");
}

void loop() {
    // Handle web server requests
    server.handleClient();
    
    // Update WiFi manager
    WiFiManager::update();
    
    // Handle LED status indication
    if (systemReady) {
        // Solid LED when connected and ready
        digitalWrite(LED_BUILTIN, HIGH);
    } else {
        // Blink LED in config mode
        static unsigned long lastBlink = 0;
        static bool ledState = false;
        if (millis() - lastBlink > 500) {
            ledState = !ledState;
            digitalWrite(LED_BUILTIN, ledState);
            lastBlink = millis();
        }
    }
    
    // Print IP address periodically when connected
    if (WiFiManager::isConnected() && millis() - lastIPPrint > IP_PRINT_INTERVAL) {
        Serial.printf("[%lu] IP Address: %s | SSID: %s | Signal: %d dBm\n", 
                     millis() / 1000, 
                     WiFiManager::getIP().c_str(), 
                     WiFiManager::getSSID().c_str(),
                     WiFi.RSSI());
        lastIPPrint = millis();
    }
    
    // Print status periodically
    if (millis() - lastStatusPrint > STATUS_INTERVAL) {
        printStatus();
        Config::printStatus(); // Also print configuration status
        lastStatusPrint = millis();
    }
    
    // Check if we got connected during config mode
    if (!systemReady && WiFiManager::isConnected()) {
        Serial.println("WiFi connected during config mode!");
        Serial.printf("IP Address: %s\n", WiFiManager::getIP().c_str());
        systemReady = true;
        digitalWrite(LED_BUILTIN, HIGH);
    }
    
    // Check if we lost connection
    if (systemReady && !WiFiManager::isConnected()) {
        Serial.println("WiFi connection lost! Attempting to reconnect...");
        systemReady = false;
        // Try to reconnect
        if (Config::hasWiFiCredentials()) {
            WiFiManager::connectToSaved();
        }
    }
    
    delay(10); // Small delay for stability
}

void setupWebServer() {
    // Root page - status
    server.on("/", HTTP_GET, handleRoot);
    
    // Configuration page
    server.on("/config", HTTP_GET, handleConfig);
    server.on("/config", HTTP_POST, handleConfigSave);
    
    // WiFi scan
    server.on("/scan", HTTP_GET, handleScan);
    
    // Stream configuration
    server.on("/stream", HTTP_GET, handleStream);
    server.on("/stream", HTTP_POST, handleStreamSave);
    
    // System information
    server.on("/info", HTTP_GET, handleInfo);
    
    // Reset configuration
    server.on("/reset", HTTP_POST, handleReset);
    
    // 404 handler
    server.onNotFound(handleNotFound);
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Radio Benziger</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
    html += "h1{color:#333;text-align:center}";
    html += ".status{padding:10px;margin:10px 0;border-radius:5px}";
    html += ".connected{background:#d4edda;color:#155724;border:1px solid #c3e6cb}";
    html += ".disconnected{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}";
    html += ".btn{display:inline-block;padding:10px 20px;margin:5px;text-decoration:none;border-radius:5px;border:none;cursor:pointer}";
    html += ".btn-primary{background:#007bff;color:white}";
    html += ".btn-secondary{background:#6c757d;color:white}";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>🎵 Radio Benziger</h1>";
    
    // WiFi Status
    if (WiFiManager::isConnected()) {
        html += "<div class='status connected'>";
        html += "<strong>WiFi Connected</strong><br>";
        html += "SSID: " + WiFiManager::getSSID() + "<br>";
        html += "IP: " + WiFiManager::getIP();
        html += "</div>";
    } else {
        html += "<div class='status disconnected'>";
        html += "<strong>WiFi Disconnected</strong><br>";
        html += "Please configure WiFi connection";
        html += "</div>";
    }
    
    // Stream Status
    html += "<div class='status'>";
    html += "<strong>Stream URL:</strong><br>";
    html += Config::settings.streamURL;
    html += "</div>";
    
    // Navigation buttons
    html += "<div style='text-align:center;margin-top:20px'>";
    html += "<a href='/config' class='btn btn-primary'>WiFi Config</a>";
    html += "<a href='/stream' class='btn btn-primary'>Stream Config</a>";
    html += "<a href='/info' class='btn btn-secondary'>System Info</a>";
    html += "</div>";
    
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

void handleConfig() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>WiFi Configuration</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px}";
    html += "input[type=text],input[type=password],select{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:5px}";
    html += ".btn{padding:10px 20px;margin:5px;border:none;border-radius:5px;cursor:pointer}";
    html += ".btn-primary{background:#007bff;color:white}";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>WiFi Configuration</h1>";
    
    html += "<form method='post'>";
    html += "<label>WiFi Network:</label><br>";
    html += "<select name='ssid' id='ssid'>";
    html += "<option value=''>Select network...</option>";
    
    // Add scanned networks
    int networkCount = WiFi.scanNetworks();
    for (int i = 0; i < networkCount; i++) {
        html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dBm)</option>";
    }
    
    html += "</select><br><br>";
    html += "<label>Password:</label><br>";
    html += "<input type='password' name='password' placeholder='WiFi Password'><br><br>";
    html += "<input type='submit' value='Connect' class='btn btn-primary'>";
    html += "</form>";
    
    html += "<br><a href='/'>← Back to Home</a>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

void handleConfigSave() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    if (ssid.length() > 0) {
        // Save to configuration
        ssid.toCharArray(Config::settings.wifiSSID, sizeof(Config::settings.wifiSSID));
        password.toCharArray(Config::settings.wifiPassword, sizeof(Config::settings.wifiPassword));
        Config::save();
        
        // Try to connect
        Serial.printf("Attempting to connect to: %s\n", ssid.c_str());
        
        String html = "<!DOCTYPE html><html><head>";
        html += "<title>Connecting...</title>";
        html += "<meta http-equiv='refresh' content='10;url=/'>";
        html += "</head><body>";
        html += "<h1>Connecting to WiFi...</h1>";
        html += "<p>Please wait while connecting to " + ssid + "</p>";
        html += "<p>This page will automatically redirect in 10 seconds.</p>";
        html += "</body></html>";
        
        server.send(200, "text/html", html);
        
        // Attempt connection in background
        WiFiManager::connectToWiFi(ssid.c_str(), password.c_str());
    } else {
        server.send(400, "text/html", "<h1>Error: No SSID provided</h1>");
    }
}

void handleScan() {
    String json = "{\"networks\":[";
    int networkCount = WiFi.scanNetworks();
    
    for (int i = 0; i < networkCount; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":" + String(WiFi.encryptionType(i));
        json += "}";
    }
    
    json += "]}";
    server.send(200, "application/json", json);
}

void handleStream() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Stream Configuration</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px}";
    html += "input[type=text]{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:5px}";
    html += ".btn{padding:10px 20px;margin:5px;border:none;border-radius:5px;cursor:pointer}";
    html += ".btn-primary{background:#007bff;color:white}";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>Stream Configuration</h1>";
    
    html += "<form method='post'>";
    html += "<label>Stream URL:</label><br>";
    html += "<input type='text' name='streamURL' value='" + String(Config::settings.streamURL) + "' placeholder='https://...'><br><br>";
    html += "<input type='submit' value='Save' class='btn btn-primary'>";
    html += "</form>";
    
    html += "<br><p><strong>Current:</strong> " + String(Config::settings.streamURL) + "</p>";
    html += "<br><a href='/'>← Back to Home</a>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

void handleStreamSave() {
    String streamURL = server.arg("streamURL");
    
    if (streamURL.length() > 0) {
        streamURL.toCharArray(Config::settings.streamURL, sizeof(Config::settings.streamURL));
        Config::save();
        
        server.send(200, "text/html", 
            "<h1>Stream URL Updated</h1>"
            "<p>New URL: " + streamURL + "</p>"
            "<a href='/'>← Back to Home</a>");
    } else {
        server.send(400, "text/html", "<h1>Error: No URL provided</h1>");
    }
}

void handleInfo() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>System Information</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px}";
    html += ".info{background:#f8f9fa;padding:10px;margin:10px 0;border-radius:5px;font-family:monospace}";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>System Information</h1>";
    
    html += "<div class='info'>";
    html += "<strong>Device:</strong> " + String(Config::settings.deviceName) + "<br>";
    html += "<strong>Chip Model:</strong> " + String(ESP.getChipModel()) + "<br>";
    html += "<strong>Chip Revision:</strong> " + String(ESP.getChipRevision()) + "<br>";
    html += "<strong>CPU Frequency:</strong> " + String(ESP.getCpuFreqMHz()) + " MHz<br>";
    html += "<strong>Flash Size:</strong> " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB<br>";
    html += "<strong>Free Heap:</strong> " + String(ESP.getFreeHeap()) + " bytes<br>";
    html += "<strong>Uptime:</strong> " + String(millis() / 1000) + " seconds<br>";
    html += "</div>";
    
    html += "<br><a href='/'>← Back to Home</a>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

void handleReset() {
    Config::reset();
    server.send(200, "text/html", 
        "<h1>Configuration Reset</h1>"
        "<p>All settings have been reset to defaults.</p>"
        "<p>Device will restart...</p>");
    
    delay(2000);
    ESP.restart();
}

void handleNotFound() {
    server.send(404, "text/html", 
        "<h1>404 - Page Not Found</h1>"
        "<a href='/'>← Back to Home</a>");
}

void printStatus() {
    Serial.println("========== SYSTEM STATUS ==========");
    Serial.printf("Uptime: %lu seconds (%lu minutes)\n", millis() / 1000, millis() / 60000);
    Serial.printf("Free Heap: %d bytes (%.1f KB)\n", ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
    Serial.printf("Largest Free Block: %d bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
    
    // WiFi Status
    Serial.printf("WiFi Status: %s\n", WiFiManager::isConnected() ? "Connected" : "Disconnected");
    if (WiFiManager::isConnected()) {
        Serial.printf("  SSID: %s\n", WiFiManager::getSSID().c_str());
        Serial.printf("  IP Address: %s\n", WiFiManager::getIP().c_str());
        Serial.printf("  Signal Strength: %d dBm\n", WiFi.RSSI());
        Serial.printf("  MAC Address: %s\n", WiFi.macAddress().c_str());
        Serial.printf("  Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("  DNS: %s\n", WiFi.dnsIP().toString().c_str());
    }
    
    // System Status
    Serial.printf("System Ready: %s\n", systemReady ? "Yes" : "No");
    Serial.printf("Web Server: %s\n", "Running");
    
    // I2C Status
    Serial.printf("I2C Devices Found: %d\n", I2CScanner::getDeviceCount());
    
    Serial.println("===================================");
}
