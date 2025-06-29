#ifndef AUDIOSTREAMER_H
#define AUDIOSTREAMER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "driver/i2s.h"

class AudioStreamer {
public:
    enum State {
        STOPPED,
        CONNECTING,
        BUFFERING,
        PLAYING,
        PAUSED,
        ERROR
    };

    // Core audio functions
    static bool begin();
    static bool connectToStream(const char* url);
    static void stop();
    static void pause();
    static void resume();
    static void setVolume(uint8_t level);  // 0-100
    static void update();  // Call in main loop
    
    // Status functions
    static State getState();
    static uint8_t getVolume();
    static String getStreamURL();
    static String getStatusString();
    static bool isPlaying();
    static size_t getBufferLevel();
    static uint32_t getBitrate();
    static uint32_t getSampleRate();
    
    // Stream information
    static String getCurrentTitle();
    static String getCurrentArtist();
    static bool hasMetadata();

private:
    // I2S configuration
    static const i2s_port_t I2S_PORT = I2S_NUM_0;
    static const int I2S_BCLK_PIN = 25;     // Updated to match hardware
    static const int I2S_LRC_PIN = 26;      // Updated to match hardware  
    static const int I2S_DATA_PIN = 27;     // Updated to match hardware
    static const int I2S_SAMPLE_RATE = 32000;  // Match stream rate
    static const int I2S_BITS_PER_SAMPLE = 16;
    
    // Buffer configuration
    static const size_t AUDIO_BUFFER_SIZE = 8192;   // 8KB audio buffer
    static const size_t HTTP_BUFFER_SIZE = 4096;    // 4KB HTTP buffer
    static const size_t DMA_BUFFER_COUNT = 8;
    static const size_t DMA_BUFFER_SIZE = 1024;
    
    // State variables
    static State currentState;
    static uint8_t currentVolume;
    static String streamURL;
    static String currentTitle;
    static String currentArtist;
    static bool metadataAvailable;
    
    // HTTP streaming
    static HTTPClient httpClient;
    static WiFiClient wifiClient;
    static uint8_t httpBuffer[HTTP_BUFFER_SIZE];
    static size_t httpBufferPos;
    static size_t httpBufferLen;
    static uint32_t streamBitrate;
    static uint32_t streamSampleRate;
    
    // Audio processing
    static uint8_t audioBuffer[AUDIO_BUFFER_SIZE];
    static size_t audioBufferPos;
    static size_t audioBufferLen;
    static unsigned long lastBufferFill;
    static unsigned long lastMetadataCheck;
    
    // Private methods
    static bool initializeI2S();
    static bool startHTTPStream();
    static void stopHTTPStream();
    static size_t readHTTPData();
    static bool processAudioData();
    static void writeToI2S(const uint8_t* data, size_t len);
    static void applyVolumeControl(int16_t* samples, size_t count);
    static bool parseICYMetadata(const String& metadata);
    static void handleStreamError();
    static void resetBuffers();
};

#endif // AUDIOSTREAMER_H 