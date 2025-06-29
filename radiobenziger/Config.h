#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <EEPROM.h>

class Config {
public:
    struct Settings {
        char wifiSSID[64];
        char wifiPassword[64];
        char streamURL[256];
        char deviceName[32];
        bool autoStart;
        uint32_t checksum;  // For EEPROM validation
    };
    
    static Settings settings;
    
    // Core functions
    static bool begin();
    static bool load();
    static bool save();
    static void reset();
    static void setDefaults();
    
    // Enhanced persistence functions
    static bool saveToEEPROM();
    static bool loadFromEEPROM();
    static bool validateEEPROM();
    static uint32_t calculateChecksum();
    static void printStatus();
    
    // Configuration validation
    static bool isValid();
    static bool hasWiFiCredentials();
    
private:
    static Preferences prefs;
    static bool initialized;
    static const char* DEFAULT_STREAM_URL;
    static const char* DEFAULT_DEVICE_NAME;
    static const int EEPROM_SIZE;
    static const int EEPROM_CONFIG_ADDR;
};

#endif 