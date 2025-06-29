#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"

class WiFiManager {
public:
    enum Status {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        HOTSPOT_MODE,
        FAILED
    };

    static bool begin();
    static bool connectToSaved();
    static bool connectToWiFi(const char* ssid, const char* password);
    static void startConfigMode();
    static void stopConfigMode();
    static bool isConnected();
    static bool isInConfigMode();
    static String getIP();
    static String getSSID();
    static Status getStatus();
    static void update();
    
    // Network scanning
    static int scanNetworks();
    static String getScannedSSID(int index);
    static int getScannedRSSI(int index);
    static bool getScannedEncryption(int index);

private:
    static Status currentStatus;
    static unsigned long lastConnectionAttempt;
    static unsigned long connectionTimeout;
    static bool configModeActive;
    static const unsigned long CONNECT_TIMEOUT_MS;
    static const char* CONFIG_AP_SSID;
    
    static void onWiFiEvent(WiFiEvent_t event);
    static void setupConfigAP();
};

#endif // WIFIMANAGER_H 