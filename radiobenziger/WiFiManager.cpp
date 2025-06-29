#include "WiFiManager.h"
#include <WiFi.h>

// Static member definitions
WiFiManager::Status WiFiManager::currentStatus = DISCONNECTED;
unsigned long WiFiManager::lastConnectionAttempt = 0;
unsigned long WiFiManager::connectionTimeout = 15000; // Increased timeout
bool WiFiManager::configModeActive = false;
const unsigned long WiFiManager::CONNECT_TIMEOUT_MS = 15000;
const char* WiFiManager::CONFIG_AP_SSID = "RadioBenziger-Config";

bool WiFiManager::begin() {
    Serial.println("WiFiManager: Initializing...");
    
    // Set WiFi mode to station
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true); // Enable WiFi credential persistence
    
    // Set event handler
    WiFi.onEvent(onWiFiEvent);
    
    currentStatus = DISCONNECTED;
    configModeActive = false;
    
    Serial.println("WiFiManager: Initialized successfully");
    return true;
}

bool WiFiManager::connectToSaved() {
    // Ensure config is loaded
    if (!Config::hasWiFiCredentials()) {
        Serial.println("WiFiManager: No saved WiFi credentials found");
        currentStatus = FAILED;
        return false;
    }
    
    Serial.printf("WiFiManager: Connecting to saved WiFi: %s\n", Config::settings.wifiSSID);
    
    // Stop any existing AP mode
    if (configModeActive) {
        stopConfigMode();
        delay(1000); // Allow time for mode switch
    }
    
    // Ensure we're in STA mode
    WiFi.mode(WIFI_STA);
    delay(100);
    
    currentStatus = CONNECTING;
    lastConnectionAttempt = millis();
    
    // Start connection
    WiFi.begin(Config::settings.wifiSSID, Config::settings.wifiPassword);
    
    // Wait for connection with timeout
    Serial.print("WiFiManager: Connecting");
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < CONNECT_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
        
        // Check every 2 seconds
        if ((millis() - startTime) % 2000 == 0) {
            Serial.printf(" [%d] ", WiFi.status());
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.printf("WiFiManager: Connected successfully! IP: %s\n", WiFi.localIP().toString().c_str());
        currentStatus = CONNECTED;
        return true;
    } else {
        Serial.println();
        Serial.printf("WiFiManager: Connection failed. Status: %d\n", WiFi.status());
        currentStatus = FAILED;
        return false;
    }
}

bool WiFiManager::connectToWiFi(const char* ssid, const char* password) {
    Serial.printf("WiFiManager: Connecting to new WiFi: %s\n", ssid);
    
    // Stop config mode if active
    if (configModeActive) {
        stopConfigMode();
        delay(1000);
    }
    
    // Ensure we're in STA mode
    WiFi.mode(WIFI_STA);
    delay(100);
    
    currentStatus = CONNECTING;
    lastConnectionAttempt = millis();
    
    // Start connection
    WiFi.begin(ssid, password);
    
    // Wait for connection with timeout
    Serial.print("WiFiManager: Connecting");
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < CONNECT_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.printf("WiFiManager: Connected successfully! IP: %s\n", WiFi.localIP().toString().c_str());
        currentStatus = CONNECTED;
        
        // Save credentials if connection successful
        strncpy(Config::settings.wifiSSID, ssid, sizeof(Config::settings.wifiSSID) - 1);
        strncpy(Config::settings.wifiPassword, password, sizeof(Config::settings.wifiPassword) - 1);
        Config::settings.wifiSSID[sizeof(Config::settings.wifiSSID) - 1] = '\0';
        Config::settings.wifiPassword[sizeof(Config::settings.wifiPassword) - 1] = '\0';
        
        if (Config::save()) {
            Serial.println("WiFiManager: Credentials saved successfully");
        } else {
            Serial.println("WiFiManager: WARNING - Failed to save credentials");
        }
        
        return true;
    } else {
        Serial.println();
        Serial.printf("WiFiManager: Connection failed. Status: %d\n", WiFi.status());
        currentStatus = FAILED;
        return false;
    }
}

void WiFiManager::startConfigMode() {
    Serial.println("WiFiManager: Starting configuration mode...");
    
    // Disconnect from any existing WiFi
    WiFi.disconnect(true);
    delay(1000);
    
    setupConfigAP();
    configModeActive = true;
    currentStatus = HOTSPOT_MODE;
    
    Serial.printf("WiFiManager: Access Point started: %s\n", CONFIG_AP_SSID);
    Serial.println("WiFiManager: Connect to configure at http://192.168.4.1");
}

void WiFiManager::stopConfigMode() {
    if (configModeActive) {
        Serial.println("WiFiManager: Stopping configuration mode...");
        WiFi.softAPdisconnect(true);
        delay(500);
        WiFi.mode(WIFI_STA);
        delay(500);
        configModeActive = false;
        currentStatus = DISCONNECTED;
        Serial.println("WiFiManager: Configuration mode stopped");
    }
}

void WiFiManager::setupConfigAP() {
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Start AP with no password for easy access
    bool result = WiFi.softAP(CONFIG_AP_SSID, "", 1, 0, 4);
    
    if (result) {
        Serial.printf("WiFiManager: AP started successfully. IP: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("WiFiManager: ERROR - Failed to start AP");
    }
}

void WiFiManager::update() {
    static unsigned long lastReconnectAttempt = 0;
    const unsigned long RECONNECT_INTERVAL = 30000; // Try reconnect every 30 seconds
    
    // Handle connection timeout
    if (currentStatus == CONNECTING && (millis() - lastConnectionAttempt) > connectionTimeout) {
        currentStatus = FAILED;
        Serial.println("WiFiManager: Connection timeout");
    }
    
    // Auto-reconnect logic if we have credentials and we're not in config mode
    if (!configModeActive && currentStatus != CONNECTED && currentStatus != CONNECTING) {
        if (Config::hasWiFiCredentials() && (millis() - lastReconnectAttempt) > RECONNECT_INTERVAL) {
            Serial.println("WiFiManager: Attempting auto-reconnect...");
            lastReconnectAttempt = millis();
            connectToSaved();
        }
    }
    
    // Update status based on actual WiFi state
    if (!configModeActive) {
        if (WiFi.status() == WL_CONNECTED && currentStatus != CONNECTED) {
            currentStatus = CONNECTED;
            Serial.println("WiFiManager: Connection established");
        } else if (WiFi.status() != WL_CONNECTED && currentStatus == CONNECTED) {
            currentStatus = DISCONNECTED;
            Serial.println("WiFiManager: Connection lost");
        }
    }
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED && !configModeActive;
}

bool WiFiManager::isInConfigMode() {
    return configModeActive;
}

String WiFiManager::getIP() {
    if (configModeActive) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

String WiFiManager::getSSID() {
    if (configModeActive) {
        return String(CONFIG_AP_SSID);
    } else {
        return WiFi.SSID();
    }
}

WiFiManager::Status WiFiManager::getStatus() {
    return currentStatus;
}

int WiFiManager::scanNetworks() {
    return WiFi.scanNetworks();
}

String WiFiManager::getScannedSSID(int index) {
    return WiFi.SSID(index);
}

int WiFiManager::getScannedRSSI(int index) {
    return WiFi.RSSI(index);
}

bool WiFiManager::getScannedEncryption(int index) {
    return WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
}

void WiFiManager::onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("WiFiManager: Event - WiFi connected");
            if (!configModeActive) {
                currentStatus = CONNECTED;
            }
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("WiFiManager: Event - WiFi disconnected");
            if (currentStatus == CONNECTED && !configModeActive) {
                currentStatus = DISCONNECTED;
            }
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("WiFiManager: Event - Got IP: %s\n", WiFi.localIP().toString().c_str());
            if (!configModeActive) {
                currentStatus = CONNECTED;
            }
            break;
        default:
            break;
    }
} 