#include "Config.h"

// Static member definitions
Config::Settings Config::settings;
Preferences Config::prefs;
bool Config::initialized = false;

const char* Config::DEFAULT_STREAM_URL = "https://icecast.octosignals.com/benziger";  // Changed to HTTP (or use your proxy URL)
const char* Config::DEFAULT_DEVICE_NAME = "Radio Benziger";
const int Config::EEPROM_SIZE = 512;
const int Config::EEPROM_CONFIG_ADDR = 0;

bool Config::begin() {
    if (initialized) return true;
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Initialize Preferences (NVS)
    if (!prefs.begin("radiobenziger", false)) {
        Serial.println("Failed to initialize NVS, trying EEPROM backup...");
        
        // Try to load from EEPROM as backup
        if (loadFromEEPROM()) {
            Serial.println("Configuration loaded from EEPROM backup");
            initialized = true;
            return true;
        } else {
            Serial.println("No valid configuration found, using defaults");
            setDefaults();
            initialized = true;
            return true;
        }
    }
    
    initialized = true;
    
    // Try to load from NVS first
    if (load()) {
        // Also save to EEPROM as backup
        saveToEEPROM();
        return true;
    }
    
    // If NVS fails, try EEPROM
    if (loadFromEEPROM()) {
        Serial.println("Configuration loaded from EEPROM backup");
        save(); // Save back to NVS
        return true;
    }
    
    // If both fail, use defaults
    setDefaults();
    save();
    saveToEEPROM();
    return true;
}

bool Config::load() {
    if (!initialized) return false;
    
    // Load settings from NVS
    prefs.getString("wifiSSID", settings.wifiSSID, sizeof(settings.wifiSSID));
    prefs.getString("wifiPassword", settings.wifiPassword, sizeof(settings.wifiPassword));
    prefs.getString("streamURL", settings.streamURL, sizeof(settings.streamURL));
    prefs.getString("deviceName", settings.deviceName, sizeof(settings.deviceName));
    settings.autoStart = prefs.getBool("autoStart", true);
    
    // Set defaults if values are empty
    if (strlen(settings.streamURL) == 0) {
        strcpy(settings.streamURL, DEFAULT_STREAM_URL);
    }
    if (strlen(settings.deviceName) == 0) {
        strcpy(settings.deviceName, DEFAULT_DEVICE_NAME);
    }
    
    Serial.println("Configuration loaded from NVS:");
    printStatus();
    
    return true;
}

bool Config::save() {
    if (!initialized) return false;
    
    // Save to NVS
    prefs.putString("wifiSSID", settings.wifiSSID);
    prefs.putString("wifiPassword", settings.wifiPassword);
    prefs.putString("streamURL", settings.streamURL);
    prefs.putString("deviceName", settings.deviceName);
    prefs.putBool("autoStart", settings.autoStart);
    
    // Also save to EEPROM as backup
    saveToEEPROM();
    
    Serial.println("Configuration saved to NVS and EEPROM backup");
    return true;
}

bool Config::saveToEEPROM() {
    // Calculate checksum for validation
    settings.checksum = calculateChecksum();
    
    // Write settings to EEPROM
    EEPROM.put(EEPROM_CONFIG_ADDR, settings);
    EEPROM.commit();
    
    Serial.println("Configuration saved to EEPROM backup");
    return true;
}

bool Config::loadFromEEPROM() {
    Settings tempSettings;
    
    // Read from EEPROM
    EEPROM.get(EEPROM_CONFIG_ADDR, tempSettings);
    
    // Validate checksum
    uint32_t savedChecksum = tempSettings.checksum;
    tempSettings.checksum = 0; // Clear for calculation
    uint32_t calculatedChecksum = 0;
    
    // Calculate checksum
    uint8_t* data = (uint8_t*)&tempSettings;
    for (size_t i = 0; i < sizeof(Settings) - sizeof(uint32_t); i++) {
        calculatedChecksum += data[i];
    }
    
    if (savedChecksum != calculatedChecksum) {
        Serial.println("EEPROM checksum mismatch, data may be corrupted");
        return false;
    }
    
    // Restore checksum and copy settings
    tempSettings.checksum = savedChecksum;
    memcpy(&settings, &tempSettings, sizeof(Settings));
    
    Serial.println("Configuration loaded from EEPROM:");
    printStatus();
    
    return true;
}

bool Config::validateEEPROM() {
    Settings tempSettings;
    EEPROM.get(EEPROM_CONFIG_ADDR, tempSettings);
    
    uint32_t savedChecksum = tempSettings.checksum;
    tempSettings.checksum = 0;
    
    return savedChecksum == calculateChecksum();
}

uint32_t Config::calculateChecksum() {
    uint32_t checksum = 0;
    uint8_t* data = (uint8_t*)&settings;
    
    // Calculate checksum excluding the checksum field itself
    for (size_t i = 0; i < sizeof(Settings) - sizeof(uint32_t); i++) {
        checksum += data[i];
    }
    
    return checksum;
}

void Config::printStatus() {
    Serial.println("=== Configuration Status ===");
    Serial.printf("  WiFi SSID: %s\n", strlen(settings.wifiSSID) > 0 ? settings.wifiSSID : "(not set)");
    Serial.printf("  WiFi Password: %s\n", strlen(settings.wifiPassword) > 0 ? "***set***" : "(not set)");
    Serial.printf("  Stream URL: %s\n", settings.streamURL);
    Serial.printf("  Device Name: %s\n", settings.deviceName);
    Serial.printf("  Auto Start: %s\n", settings.autoStart ? "true" : "false");
    Serial.printf("  Has WiFi Credentials: %s\n", hasWiFiCredentials() ? "yes" : "no");
    Serial.printf("  Configuration Valid: %s\n", isValid() ? "yes" : "no");
    Serial.println("============================");
}

bool Config::isValid() {
    return (strlen(settings.deviceName) > 0 && 
            strlen(settings.streamURL) > 0);
}

bool Config::hasWiFiCredentials() {
    return (strlen(settings.wifiSSID) > 0);
}

void Config::reset() {
    if (!initialized) return;
    
    // Clear NVS
    prefs.clear();
    
    // Clear EEPROM
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    
    setDefaults();
    save();
    Serial.println("Configuration reset to defaults (NVS and EEPROM cleared)");
}

void Config::setDefaults() {
    memset(&settings, 0, sizeof(settings));
    strcpy(settings.streamURL, DEFAULT_STREAM_URL);
    strcpy(settings.deviceName, DEFAULT_DEVICE_NAME);
    settings.autoStart = true;
    settings.checksum = 0;
} 