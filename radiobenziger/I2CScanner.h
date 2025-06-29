#ifndef I2CSCANNER_H
#define I2CSCANNER_H

#include <Arduino.h>
#include <Wire.h>

class I2CScanner {
public:
    struct Device {
        uint8_t address;
        String name;
        bool responding;
    };

    static bool begin(int sda = 21, int scl = 22);
    static std::vector<Device> scanBus();
    static bool isDevicePresent(uint8_t address);
    static String getDeviceName(uint8_t address);
    static void printScanResults();
    static int getDeviceCount();

private:
    static bool initialized;
    static int sdaPin;
    static int sclPin;
    static std::vector<Device> lastScanResults;
    
    // Known I2C device database
    struct KnownDevice {
        uint8_t address;
        const char* name;
    };
    
    static const KnownDevice knownDevices[];
    static const size_t knownDevicesCount;
};

#endif // I2CSCANNER_H 