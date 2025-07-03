#ifndef PCMSTREAMER_H
#define PCMSTREAMER_H

#include <Arduino.h>
#include <vector>
#include "driver/i2s.h"

/**
 * PCMStreamer - Configurable PCM data streaming to I2S DAC
 * 
 * This class provides a flexible interface for streaming PCM audio data
 * to an I2S DAC (MAX98357A). It supports configurable sample rates,
 * bit depths, and channel configurations.
 */
class PCMStreamer {
public:
    /**
     * Audio configuration structure
     */
    struct AudioConfig {
        uint32_t sampleRate;           // Sample rate in Hz (e.g., 44100, 48000, 32000)
        uint8_t bitsPerSample;         // Bits per sample (8, 16, 24, 32)
        uint8_t channels;              // Number of channels (1=mono, 2=stereo)
        uint32_t bufferSize;           // DMA buffer size in bytes
        uint8_t bufferCount;           // Number of DMA buffers
        bool useAPLL;                  // Use APLL for better clock precision
        
        // Default constructor with sensible defaults for MAX98357A
        AudioConfig() : 
            sampleRate(44100),
            bitsPerSample(16),
            channels(2),
            bufferSize(1024),
            bufferCount(8),
            useAPLL(false) {}
    };
    
    /**
     * I2S pin configuration structure
     */
    struct PinConfig {
        int bclkPin;                   // Bit clock pin
        int lrckPin;                   // Left/Right clock pin (Word Select)
        int dataPin;                   // Data output pin
        int enablePin;                 // Optional enable/shutdown pin (-1 if not used)
        
        // Default constructor with MAX98357A pin configuration
        PinConfig() : 
            bclkPin(25),               // GPIO 25 -> BCLK
            lrckPin(26),               // GPIO 26 -> LRCK/WS  
            dataPin(27),               // GPIO 27 -> DIN
            enablePin(-1) {}           // No enable pin by default
    };
    
    /**
     * Streaming status enumeration
     */
    enum class StreamStatus {
        STOPPED,
        INITIALIZING,
        READY,
        STREAMING,
        ERROR_INIT_FAILED,
        ERROR_PIN_CONFIG_FAILED,
        ERROR_BUFFER_OVERFLOW,
        ERROR_UNDERRUN
    };

private:
    // Configuration
    AudioConfig audioConfig;
    PinConfig pinConfig;
    i2s_port_t i2sPort;
    
    // State management
    StreamStatus currentStatus;
    bool initialized;
    bool streaming;
    
    // Buffer management
    std::vector<uint8_t> internalBuffer;
    size_t bufferWritePos;
    size_t bufferReadPos;
    size_t maxBufferSize;
    
    // Statistics
    uint32_t totalBytesWritten;
    uint32_t totalPacketsProcessed;
    uint32_t bufferOverflows;
    uint32_t bufferUnderruns;
    uint32_t lastWriteTime;
    
    // Internal methods
    bool configureI2S();
    bool validateConfig();
    size_t getAvailableBufferSpace() const;
    size_t getBufferedDataSize() const;
    void updateStatistics();
    
public:
    /**
     * Constructor with full configuration
     * 
     * @param config Audio configuration (sample rate, bits, channels, etc.)
     * @param pins I2S pin configuration
     * @param port I2S port to use (default: I2S_NUM_0)
     */
    PCMStreamer(const AudioConfig& config, const PinConfig& pins, i2s_port_t port = I2S_NUM_0);
    
    /**
     * Constructor with default MAX98357A configuration
     * Only sample rate and basic parameters need to be specified
     * 
     * @param sampleRate Sample rate in Hz
     * @param bitsPerSample Bits per sample (8, 16, 24, 32)
     * @param channels Number of channels (1 or 2)
     */
    PCMStreamer(uint32_t sampleRate = 44100, uint8_t bitsPerSample = 16, uint8_t channels = 2);
    
    /**
     * Destructor - ensures proper cleanup
     */
    ~PCMStreamer();
    
    // Core functionality
    /**
     * Initialize the I2S interface and prepare for streaming
     * 
     * @return true if initialization successful, false otherwise
     */
    bool begin();
    
    /**
     * Stop streaming and deinitialize I2S
     */
    void end();
    
    /**
     * Write PCM data from a vector buffer
     * 
     * @param data Vector containing PCM data
     * @param timeoutMs Timeout in milliseconds (0 = no timeout)
     * @return Number of bytes actually written
     */
    size_t write(const std::vector<uint8_t>& data, uint32_t timeoutMs = 1000);
    
    /**
     * Write PCM data from a raw buffer
     * 
     * @param data Pointer to PCM data
     * @param size Size of data in bytes
     * @param timeoutMs Timeout in milliseconds (0 = no timeout)
     * @return Number of bytes actually written
     */
    size_t write(const uint8_t* data, size_t size, uint32_t timeoutMs = 1000);
    
    /**
     * Write PCM data from a vector of 16-bit samples
     * 
     * @param samples Vector of 16-bit PCM samples
     * @param timeoutMs Timeout in milliseconds
     * @return Number of bytes actually written
     */
    size_t writeSamples(const std::vector<int16_t>& samples, uint32_t timeoutMs = 1000);
    
    /**
     * Flush any buffered data to the I2S interface
     * 
     * @return true if flush successful
     */
    bool flush();
    
    /**
     * Clear internal buffers
     */
    void clearBuffers();
    
    // Status and configuration
    /**
     * Get current streaming status
     */
    StreamStatus getStatus() const { return currentStatus; }
    
    /**
     * Get status as human-readable string
     */
    String getStatusString() const;
    
    /**
     * Check if streamer is ready for data
     */
    bool isReady() const { return currentStatus == StreamStatus::READY || currentStatus == StreamStatus::STREAMING; }
    
    /**
     * Check if currently streaming
     */
    bool isStreaming() const { return currentStatus == StreamStatus::STREAMING; }
    
    /**
     * Get current audio configuration
     */
    const AudioConfig& getAudioConfig() const { return audioConfig; }
    
    /**
     * Get current pin configuration
     */
    const PinConfig& getPinConfig() const { return pinConfig; }
    
    /**
     * Get I2S port being used
     */
    i2s_port_t getI2SPort() const { return i2sPort; }
    
    // Buffer management
    /**
     * Get available buffer space in bytes
     */
    size_t getAvailableSpace() const { return getAvailableBufferSpace(); }
    
    /**
     * Get amount of data currently buffered
     */
    size_t getBufferedBytes() const { return getBufferedDataSize(); }
    
    /**
     * Get buffer utilization as percentage (0-100)
     */
    float getBufferUtilization() const;
    
    /**
     * Check if buffer is nearly full (>80%)
     */
    bool isBufferNearlyFull() const { return getBufferUtilization() > 80.0f; }
    
    /**
     * Check if buffer is nearly empty (<20%)
     */
    bool isBufferNearlyEmpty() const { return getBufferUtilization() < 20.0f; }
    
    // Statistics and diagnostics
    /**
     * Get total bytes written since initialization
     */
    uint32_t getTotalBytesWritten() const { return totalBytesWritten; }
    
    /**
     * Get total packets processed
     */
    uint32_t getTotalPacketsProcessed() const { return totalPacketsProcessed; }
    
    /**
     * Get number of buffer overflows
     */
    uint32_t getBufferOverflows() const { return bufferOverflows; }
    
    /**
     * Get number of buffer underruns
     */
    uint32_t getBufferUnderruns() const { return bufferUnderruns; }
    
    /**
     * Get time since last write operation (milliseconds)
     */
    uint32_t getTimeSinceLastWrite() const { return millis() - lastWriteTime; }
    
    /**
     * Print detailed diagnostics to Serial
     */
    void printDiagnostics() const;
    
    /**
     * Reset statistics counters
     */
    void resetStatistics();
    
    // Utility methods
    /**
     * Calculate bytes per second based on current configuration
     */
    uint32_t getBytesPerSecond() const;
    
    /**
     * Calculate buffer duration in milliseconds
     */
    uint32_t getBufferDurationMs() const;
    
    /**
     * Convert sample count to byte count based on configuration
     */
    size_t samplesToBytes(size_t samples) const;
    
    /**
     * Convert byte count to sample count based on configuration
     */
    size_t bytesToSamples(size_t bytes) const;
    
    /**
     * Validate if data size is properly aligned for current configuration
     */
    bool isDataAligned(size_t bytes) const;
};

#endif // PCMSTREAMER_H 