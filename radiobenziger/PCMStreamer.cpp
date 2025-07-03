#include "PCMStreamer.h"
#include <esp_log.h>

static const char* TAG = "PCMStreamer";

// Constructor with full configuration
PCMStreamer::PCMStreamer(const AudioConfig& config, const PinConfig& pins, i2s_port_t port) 
    : audioConfig(config), pinConfig(pins), i2sPort(port) {
    
    currentStatus = StreamStatus::STOPPED;
    initialized = false;
    streaming = false;
    
    bufferWritePos = 0;
    bufferReadPos = 0;
    maxBufferSize = audioConfig.bufferSize * audioConfig.bufferCount * 4; // 4x for safety
    
    // Initialize statistics
    totalBytesWritten = 0;
    totalPacketsProcessed = 0;
    bufferOverflows = 0;
    bufferUnderruns = 0;
    lastWriteTime = 0;
    
    // Pre-allocate internal buffer
    internalBuffer.reserve(maxBufferSize);
    
    ESP_LOGI(TAG, "PCMStreamer created: %dHz, %d-bit, %d-channel", 
             audioConfig.sampleRate, audioConfig.bitsPerSample, audioConfig.channels);
}

// Constructor with default MAX98357A configuration
PCMStreamer::PCMStreamer(uint32_t sampleRate, uint8_t bitsPerSample, uint8_t channels) 
    : PCMStreamer(AudioConfig(), PinConfig(), I2S_NUM_0) {
    
    // Override with specified parameters
    audioConfig.sampleRate = sampleRate;
    audioConfig.bitsPerSample = bitsPerSample;
    audioConfig.channels = channels;
    
    ESP_LOGI(TAG, "PCMStreamer created with defaults: %dHz, %d-bit, %d-channel", 
             sampleRate, bitsPerSample, channels);
}

// Destructor
PCMStreamer::~PCMStreamer() {
    end();
    ESP_LOGI(TAG, "PCMStreamer destroyed");
}

// Initialize the I2S interface
bool PCMStreamer::begin() {
    ESP_LOGI(TAG, "Initializing PCMStreamer...");
    
    if (initialized) {
        ESP_LOGW(TAG, "PCMStreamer already initialized");
        return true;
    }
    
    currentStatus = StreamStatus::INITIALIZING;
    
    // Validate configuration
    if (!validateConfig()) {
        ESP_LOGE(TAG, "Invalid configuration");
        currentStatus = StreamStatus::ERROR_INIT_FAILED;
        return false;
    }
    
    // Configure I2S
    if (!configureI2S()) {
        ESP_LOGE(TAG, "I2S configuration failed");
        currentStatus = StreamStatus::ERROR_INIT_FAILED;
        return false;
    }
    
    // Clear buffers
    clearBuffers();
    
    // Reset statistics
    resetStatistics();
    
    initialized = true;
    currentStatus = StreamStatus::READY;
    
    ESP_LOGI(TAG, "PCMStreamer initialized successfully");
    ESP_LOGI(TAG, "I2S Config: %dHz, %d-bit, %d-channel", 
             audioConfig.sampleRate, audioConfig.bitsPerSample, audioConfig.channels);
    ESP_LOGI(TAG, "Pin Config: BCLK=%d, LRCK=%d, DATA=%d", 
             pinConfig.bclkPin, pinConfig.lrckPin, pinConfig.dataPin);
    
    return true;
}

// Stop streaming and deinitialize I2S
void PCMStreamer::end() {
    if (!initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping PCMStreamer...");
    
    streaming = false;
    
    // Flush any remaining data
    flush();
    
    // Uninstall I2S driver
    esp_err_t result = i2s_driver_uninstall(i2sPort);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to uninstall I2S driver: %s", esp_err_to_name(result));
    }
    
    // Clear buffers
    clearBuffers();
    
    initialized = false;
    currentStatus = StreamStatus::STOPPED;
    
    ESP_LOGI(TAG, "PCMStreamer stopped");
}

// Write PCM data from a vector buffer
size_t PCMStreamer::write(const std::vector<uint8_t>& data, uint32_t timeoutMs) {
    if (data.empty()) {
        return 0;
    }
    
    return write(data.data(), data.size(), timeoutMs);
}

// Write PCM data from a raw buffer
size_t PCMStreamer::write(const uint8_t* data, size_t size, uint32_t timeoutMs) {
    if (!isReady() || !data || size == 0) {
        return 0;
    }
    
    // Check data alignment
    if (!isDataAligned(size)) {
        ESP_LOGW(TAG, "Data not properly aligned for %d-bit %d-channel audio", 
                 audioConfig.bitsPerSample, audioConfig.channels);
    }
    
    // Update status to streaming
    if (currentStatus == StreamStatus::READY) {
        currentStatus = StreamStatus::STREAMING;
        streaming = true;
    }
    
    size_t bytesWritten = 0;
    esp_err_t result = i2s_write(i2sPort, data, size, &bytesWritten, 
                                 timeoutMs == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs));
    
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(result));
        if (result == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "I2S write timeout after %dms", timeoutMs);
        }
        return 0;
    }
    
    // Update statistics
    totalBytesWritten += bytesWritten;
    totalPacketsProcessed++;
    lastWriteTime = millis();
    
    // Check for buffer issues
    if (bytesWritten < size) {
        bufferOverflows++;
        ESP_LOGW(TAG, "Buffer overflow: wrote %d/%d bytes", bytesWritten, size);
    }
    
    return bytesWritten;
}

// Write PCM data from a vector of 16-bit samples
size_t PCMStreamer::writeSamples(const std::vector<int16_t>& samples, uint32_t timeoutMs) {
    if (samples.empty()) {
        return 0;
    }
    
    // Convert samples to bytes
    const uint8_t* data = reinterpret_cast<const uint8_t*>(samples.data());
    size_t size = samples.size() * sizeof(int16_t);
    
    return write(data, size, timeoutMs);
}

// Flush any buffered data to the I2S interface
bool PCMStreamer::flush() {
    if (!initialized) {
        return false;
    }
    
    // I2S driver handles internal buffering, so we just need to wait
    // for any pending writes to complete
    esp_err_t result = i2s_zero_dma_buffer(i2sPort);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to flush I2S buffer: %s", esp_err_to_name(result));
        return false;
    }
    
    return true;
}

// Clear internal buffers
void PCMStreamer::clearBuffers() {
    internalBuffer.clear();
    bufferWritePos = 0;
    bufferReadPos = 0;
    
    if (initialized) {
        i2s_zero_dma_buffer(i2sPort);
    }
}

// Get status as human-readable string
String PCMStreamer::getStatusString() const {
    switch (currentStatus) {
        case StreamStatus::STOPPED: return "Stopped";
        case StreamStatus::INITIALIZING: return "Initializing";
        case StreamStatus::READY: return "Ready";
        case StreamStatus::STREAMING: return "Streaming";
        case StreamStatus::ERROR_INIT_FAILED: return "Error: Initialization Failed";
        case StreamStatus::ERROR_PIN_CONFIG_FAILED: return "Error: Pin Configuration Failed";
        case StreamStatus::ERROR_BUFFER_OVERFLOW: return "Error: Buffer Overflow";
        case StreamStatus::ERROR_UNDERRUN: return "Error: Buffer Underrun";
        default: return "Unknown";
    }
}

// Get buffer utilization as percentage
float PCMStreamer::getBufferUtilization() const {
    if (maxBufferSize == 0) return 0.0f;
    
    size_t bufferedBytes = getBufferedDataSize();
    return (float)bufferedBytes / (float)maxBufferSize * 100.0f;
}

// Print detailed diagnostics
void PCMStreamer::printDiagnostics() const {
    ESP_LOGI(TAG, "=== PCMStreamer Diagnostics ===");
    ESP_LOGI(TAG, "Status: %s", getStatusString().c_str());
    ESP_LOGI(TAG, "Initialized: %s", initialized ? "Yes" : "No");
    ESP_LOGI(TAG, "Streaming: %s", streaming ? "Yes" : "No");
    ESP_LOGI(TAG, "I2S Port: %d", i2sPort);
    
    ESP_LOGI(TAG, "Audio Config:");
    ESP_LOGI(TAG, "  Sample Rate: %d Hz", audioConfig.sampleRate);
    ESP_LOGI(TAG, "  Bits/Sample: %d", audioConfig.bitsPerSample);
    ESP_LOGI(TAG, "  Channels: %d", audioConfig.channels);
    ESP_LOGI(TAG, "  Buffer Size: %d bytes", audioConfig.bufferSize);
    ESP_LOGI(TAG, "  Buffer Count: %d", audioConfig.bufferCount);
    ESP_LOGI(TAG, "  Use APLL: %s", audioConfig.useAPLL ? "Yes" : "No");
    
    ESP_LOGI(TAG, "Pin Config:");
    ESP_LOGI(TAG, "  BCLK Pin: %d", pinConfig.bclkPin);
    ESP_LOGI(TAG, "  LRCK Pin: %d", pinConfig.lrckPin);
    ESP_LOGI(TAG, "  Data Pin: %d", pinConfig.dataPin);
    ESP_LOGI(TAG, "  Enable Pin: %d", pinConfig.enablePin);
    
    ESP_LOGI(TAG, "Buffer Status:");
    ESP_LOGI(TAG, "  Max Buffer Size: %d bytes", maxBufferSize);
    ESP_LOGI(TAG, "  Buffered Data: %d bytes", getBufferedDataSize());
    ESP_LOGI(TAG, "  Available Space: %d bytes", getAvailableBufferSpace());
    ESP_LOGI(TAG, "  Utilization: %.1f%%", getBufferUtilization());
    
    ESP_LOGI(TAG, "Statistics:");
    ESP_LOGI(TAG, "  Total Bytes Written: %d", totalBytesWritten);
    ESP_LOGI(TAG, "  Total Packets: %d", totalPacketsProcessed);
    ESP_LOGI(TAG, "  Buffer Overflows: %d", bufferOverflows);
    ESP_LOGI(TAG, "  Buffer Underruns: %d", bufferUnderruns);
    ESP_LOGI(TAG, "  Time Since Last Write: %dms", getTimeSinceLastWrite());
    ESP_LOGI(TAG, "  Bytes Per Second: %d", getBytesPerSecond());
    ESP_LOGI(TAG, "  Buffer Duration: %dms", getBufferDurationMs());
    
    ESP_LOGI(TAG, "===============================");
}

// Reset statistics counters
void PCMStreamer::resetStatistics() {
    totalBytesWritten = 0;
    totalPacketsProcessed = 0;
    bufferOverflows = 0;
    bufferUnderruns = 0;
    lastWriteTime = millis();
}

// Calculate bytes per second based on current configuration
uint32_t PCMStreamer::getBytesPerSecond() const {
    return audioConfig.sampleRate * audioConfig.channels * (audioConfig.bitsPerSample / 8);
}

// Calculate buffer duration in milliseconds
uint32_t PCMStreamer::getBufferDurationMs() const {
    uint32_t bytesPerSecond = getBytesPerSecond();
    if (bytesPerSecond == 0) return 0;
    
    return (maxBufferSize * 1000) / bytesPerSecond;
}

// Convert sample count to byte count
size_t PCMStreamer::samplesToBytes(size_t samples) const {
    return samples * audioConfig.channels * (audioConfig.bitsPerSample / 8);
}

// Convert byte count to sample count
size_t PCMStreamer::bytesToSamples(size_t bytes) const {
    size_t bytesPerSample = audioConfig.channels * (audioConfig.bitsPerSample / 8);
    return bytesPerSample > 0 ? bytes / bytesPerSample : 0;
}

// Validate if data size is properly aligned
bool PCMStreamer::isDataAligned(size_t bytes) const {
    size_t bytesPerSample = audioConfig.channels * (audioConfig.bitsPerSample / 8);
    return bytesPerSample > 0 && (bytes % bytesPerSample) == 0;
}

// Private methods implementation

// Configure I2S
bool PCMStreamer::configureI2S() {
    ESP_LOGI(TAG, "Configuring I2S...");
    
    // Convert bits per sample to ESP32 enum
    i2s_bits_per_sample_t i2sBits;
    switch (audioConfig.bitsPerSample) {
        case 8:  i2sBits = I2S_BITS_PER_SAMPLE_8BIT; break;
        case 16: i2sBits = I2S_BITS_PER_SAMPLE_16BIT; break;
        case 24: i2sBits = I2S_BITS_PER_SAMPLE_24BIT; break;
        case 32: i2sBits = I2S_BITS_PER_SAMPLE_32BIT; break;
        default:
            ESP_LOGE(TAG, "Unsupported bits per sample: %d", audioConfig.bitsPerSample);
            return false;
    }
    
    // Configure I2S
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = audioConfig.sampleRate,
        .bits_per_sample = i2sBits,
        .channel_format = audioConfig.channels == 1 ? I2S_CHANNEL_FMT_ONLY_LEFT : I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = audioConfig.bufferCount,
        .dma_buf_len = audioConfig.bufferSize,
        .use_apll = audioConfig.useAPLL,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    // Install I2S driver
    esp_err_t result = i2s_driver_install(i2sPort, &i2sConfig, 0, NULL);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S driver: %s", esp_err_to_name(result));
        return false;
    }
    
    // Configure pins
    i2s_pin_config_t pinConfigI2S = {
        .bck_io_num = pinConfig.bclkPin,
        .ws_io_num = pinConfig.lrckPin,
        .data_out_num = pinConfig.dataPin,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    result = i2s_set_pin(i2sPort, &pinConfigI2S);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S pins: %s", esp_err_to_name(result));
        i2s_driver_uninstall(i2sPort);
        return false;
    }
    
    // Configure enable pin if specified
    if (pinConfig.enablePin >= 0) {
        pinMode(pinConfig.enablePin, OUTPUT);
        digitalWrite(pinConfig.enablePin, HIGH); // Enable the DAC
        ESP_LOGI(TAG, "Enabled DAC via pin %d", pinConfig.enablePin);
    }
    
    ESP_LOGI(TAG, "I2S configured successfully");
    return true;
}

// Validate configuration
bool PCMStreamer::validateConfig() {
    // Check sample rate
    if (audioConfig.sampleRate < 8000 || audioConfig.sampleRate > 192000) {
        ESP_LOGE(TAG, "Invalid sample rate: %d (must be 8000-192000)", audioConfig.sampleRate);
        return false;
    }
    
    // Check bits per sample
    if (audioConfig.bitsPerSample != 8 && audioConfig.bitsPerSample != 16 && 
        audioConfig.bitsPerSample != 24 && audioConfig.bitsPerSample != 32) {
        ESP_LOGE(TAG, "Invalid bits per sample: %d (must be 8, 16, 24, or 32)", audioConfig.bitsPerSample);
        return false;
    }
    
    // Check channels
    if (audioConfig.channels < 1 || audioConfig.channels > 2) {
        ESP_LOGE(TAG, "Invalid channel count: %d (must be 1 or 2)", audioConfig.channels);
        return false;
    }
    
    // Check buffer parameters
    if (audioConfig.bufferSize < 64 || audioConfig.bufferSize > 4096) {
        ESP_LOGE(TAG, "Invalid buffer size: %d (must be 64-4096)", audioConfig.bufferSize);
        return false;
    }
    
    if (audioConfig.bufferCount < 2 || audioConfig.bufferCount > 32) {
        ESP_LOGE(TAG, "Invalid buffer count: %d (must be 2-32)", audioConfig.bufferCount);
        return false;
    }
    
    // Check pins
    if (pinConfig.bclkPin < 0 || pinConfig.lrckPin < 0 || pinConfig.dataPin < 0) {
        ESP_LOGE(TAG, "Invalid pin configuration");
        return false;
    }
    
    ESP_LOGI(TAG, "Configuration validated successfully");
    return true;
}

// Get available buffer space
size_t PCMStreamer::getAvailableBufferSpace() const {
    size_t bufferedSize = internalBuffer.size();
    return bufferedSize < maxBufferSize ? maxBufferSize - bufferedSize : 0;
}

// Get amount of data currently buffered
size_t PCMStreamer::getBufferedDataSize() const {
    return internalBuffer.size();
}

// Update statistics (called internally)
void PCMStreamer::updateStatistics() {
    // This method can be called periodically to update internal statistics
    // For now, statistics are updated in real-time during write operations
} 