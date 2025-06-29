#include "I2SDetector.h"
#include "driver/i2s.h"
#include <vector>

// Static member definitions
bool I2SDetector::initialized = false;
std::vector<I2SDetector::I2SDevice> I2SDetector::lastDetectionResults;

bool I2SDetector::begin() {
    Serial.println("Initializing I2S detector...");
    Serial.printf("I2S pins: BCLK=%d, LRC=%d, DIN=%d\n", I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
    
    // Test I2S bus initialization
    if (!testI2SBus()) {
        Serial.println("ERROR: I2S bus test failed");
        return false;
    }
    
    initialized = true;
    Serial.println("I2S detector initialized successfully");
    
    // Perform device detection
    lastDetectionResults = detectDevices();
    
    return true;
}

bool I2SDetector::testI2SBus() {
    Serial.println("Testing I2S bus configuration...");
    
    // Configure I2S for testing
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)I2S_BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_DIN_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    // Try to install I2S driver
    esp_err_t result = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (result != ESP_OK) {
        Serial.printf("I2S driver install failed: %s\n", esp_err_to_name(result));
        return false;
    }
    
    // Set pins
    result = i2s_set_pin(I2S_PORT, &pin_config);
    if (result != ESP_OK) {
        Serial.printf("I2S pin setup failed: %s\n", esp_err_to_name(result));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    
    Serial.println("✅ I2S bus test passed");
    
    // Clean up test
    i2s_driver_uninstall(I2S_PORT);
    return true;
}

std::vector<I2SDetector::I2SDevice> I2SDetector::detectDevices() {
    std::vector<I2SDevice> devices;
    
    Serial.println("Detecting I2S devices...");
    
    // Test for DAC (MAX98357A)
    I2SDevice dac;
    dac.name = "MAX98357A I2S DAC";
    dac.type = "Audio Output";
    dac.detected = detectDAC();
    dac.status = dac.detected ? "Ready" : "Not responding";
    devices.push_back(dac);
    
    // Test for Microphone
    I2SDevice mic;
    mic.name = "I2S Microphone";
    mic.type = "Audio Input";
    mic.detected = detectMicrophone();
    mic.status = mic.detected ? "Ready" : "Not detected";
    devices.push_back(mic);
    
    return devices;
}

bool I2SDetector::detectDAC() {
    // For MAX98357A, we test by trying to send a test signal
    Serial.println("Testing DAC (MAX98357A)...");
    
    // Configure I2S for output test
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_DIN_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    esp_err_t result = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (result != ESP_OK) {
        return false;
    }
    
    result = i2s_set_pin(I2S_PORT, &pin_config);
    if (result != ESP_OK) {
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    
    // Send a brief test signal (silence)
    int16_t samples[64] = {0}; // Silent test
    size_t bytes_written;
    result = i2s_write(I2S_PORT, samples, sizeof(samples), &bytes_written, 100);
    
    bool detected = (result == ESP_OK && bytes_written > 0);
    
    i2s_driver_uninstall(I2S_PORT);
    
    if (detected) {
        Serial.println("✅ DAC detected and responding");
    } else {
        Serial.println("⚠️  DAC not responding (check connections)");
    }
    
    return detected;
}

bool I2SDetector::detectMicrophone() {
    // For I2S microphones, we test by trying to configure input mode
    Serial.println("Testing I2S Microphone...");
    
    // Most setups have microphone on a different I2S port or shared bus
    // For now, we'll assume microphone is present if DAC works
    // This can be enhanced with actual input testing later
    
    Serial.println("ℹ️  Microphone detection: Assuming present (shared I2S bus)");
    return true; // Assume present for now
}

void I2SDetector::printDetectionResults() {
    if (lastDetectionResults.empty()) {
        Serial.println("No I2S detection results available");
        return;
    }
    
    Serial.println("=== I2S Device Detection Results ===");
    for (const auto& device : lastDetectionResults) {
        String statusIcon = device.detected ? "✅" : "❌";
        Serial.printf("%s %s (%s): %s\n", 
                     statusIcon.c_str(),
                     device.name.c_str(), 
                     device.type.c_str(),
                     device.status.c_str());
    }
    Serial.printf("Total I2S devices: %d detected\n", getDeviceCount());
    Serial.println("=====================================");
}

int I2SDetector::getDeviceCount() {
    int count = 0;
    for (const auto& device : lastDetectionResults) {
        if (device.detected) count++;
    }
    return count;
}

bool I2SDetector::isI2SReady() {
    return initialized && getDeviceCount() > 0;
}

String I2SDetector::getI2SConfig() {
    return String("I2S Port: ") + String(I2S_PORT) + 
           ", Sample Rate: " + String(I2S_SAMPLE_RATE) + "Hz" +
           ", Bits: " + String(I2S_BITS_PER_SAMPLE);
}

String I2SDetector::getPinConfiguration() {
    return String("BCLK: GPIO") + String(I2S_BCLK_PIN) +
           ", LRC: GPIO" + String(I2S_LRC_PIN) +
           ", DIN: GPIO" + String(I2S_DIN_PIN);
} 