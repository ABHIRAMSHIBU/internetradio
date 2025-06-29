#ifndef I2SDETECTOR_H
#define I2SDETECTOR_H

#include <Arduino.h>
#include "driver/i2s.h"

class I2SDetector {
public:
    struct I2SDevice {
        String name;
        String type;
        bool detected;
        String status;
    };

    // I2S detection and diagnostics
    static bool begin();
    static bool testI2SBus();
    static std::vector<I2SDevice> detectDevices();
    static void printDetectionResults();
    static int getDeviceCount();
    static bool isI2SReady();
    
    // I2S configuration info
    static String getI2SConfig();
    static String getPinConfiguration();

private:
    static bool initialized;
    static std::vector<I2SDevice> lastDetectionResults;
    
    // I2S test parameters
    static const i2s_port_t I2S_PORT = I2S_NUM_0;
    static const int I2S_BCLK_PIN = 25;
    static const int I2S_LRC_PIN = 26;
    static const int I2S_DIN_PIN = 27;
    static const int I2S_SAMPLE_RATE = 44100;
    static const int I2S_BITS_PER_SAMPLE = 16;
    
    // Test methods
    static bool testI2SOutput();
    static bool testI2SInput();
    static bool detectDAC();
    static bool detectMicrophone();
};

#endif // I2SDETECTOR_H 