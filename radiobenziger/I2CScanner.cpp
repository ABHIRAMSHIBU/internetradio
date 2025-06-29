#include "I2CScanner.h"
#include <Wire.h>
#include <vector>

// Static member definitions
bool I2CScanner::initialized = false;
int I2CScanner::sdaPin = 21;
int I2CScanner::sclPin = 22;
std::vector<I2CScanner::Device> I2CScanner::lastScanResults;

// Known I2C device database
const I2CScanner::KnownDevice I2CScanner::knownDevices[] = {
    {0x3C, "OLED Display (SSD1306)"},
    {0x3D, "OLED Display (SSD1306)"},
    {0x48, "ADS1115 ADC"},
    {0x49, "ADS1115 ADC"},
    {0x4A, "ADS1115 ADC"},
    {0x4B, "ADS1115 ADC"},
    {0x68, "DS1307 RTC / MPU6050"},
    {0x76, "BMP280/BME280 Sensor"},
    {0x77, "BMP280/BME280 Sensor"}
};

const size_t I2CScanner::knownDevicesCount = sizeof(knownDevices) / sizeof(knownDevices[0]);

bool I2CScanner::begin(int sda, int scl) {
    sdaPin = sda;
    sclPin = scl;
    
    Wire.begin(sdaPin, sclPin);
    initialized = true;
    
    Serial.printf("I2C Scanner initialized (SDA: %d, SCL: %d)\n", sdaPin, sclPin);
    
    // Perform initial scan
    lastScanResults = scanBus();
    
    return true;
}

std::vector<I2CScanner::Device> I2CScanner::scanBus() {
    std::vector<Device> devices;
    
    if (!initialized) {
        Serial.println("I2C Scanner not initialized");
        return devices;
    }
    
    Serial.println("Scanning I2C bus...");
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        Device device;
        device.address = address;
        device.responding = (error == 0);
        device.name = getDeviceName(address);
        
        if (device.responding) {
            devices.push_back(device);
            Serial.printf("Found device at 0x%02X: %s\n", address, device.name.c_str());
        }
    }
    
    if (devices.empty()) {
        Serial.println("No I2C devices found");
    } else {
        Serial.printf("Found %d I2C device(s)\n", devices.size());
    }
    
    // Update last scan results
    lastScanResults = devices;
    
    return devices;
}

bool I2CScanner::isDevicePresent(uint8_t address) {
    if (!initialized) return false;
    
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    return (error == 0);
}

String I2CScanner::getDeviceName(uint8_t address) {
    // Check known devices first
    for (size_t i = 0; i < knownDevicesCount; i++) {
        if (knownDevices[i].address == address) {
            return String(knownDevices[i].name);
        }
    }
    
    // Return generic name if unknown
    return "Unknown Device";
}

void I2CScanner::printScanResults() {
    if (lastScanResults.empty()) {
        Serial.println("No I2C devices found in last scan");
        return;
    }
    
    Serial.println("=== I2C Scan Results ===");
    for (const auto& device : lastScanResults) {
        Serial.printf("0x%02X: %s\n", device.address, device.name.c_str());
    }
    Serial.printf("Total devices: %d\n", lastScanResults.size());
    Serial.println("========================");
}

int I2CScanner::getDeviceCount() {
    return lastScanResults.size();
} 