#include "AudioStreamer.h"
#include "Config.h"

// Static member definitions
AudioStreamer::State AudioStreamer::currentState = STOPPED;
uint8_t AudioStreamer::currentVolume = 75;
String AudioStreamer::streamURL = "";
String AudioStreamer::currentTitle = "";
String AudioStreamer::currentArtist = "";
bool AudioStreamer::metadataAvailable = false;

HTTPClient AudioStreamer::httpClient;
WiFiClient AudioStreamer::wifiClient;
uint8_t AudioStreamer::httpBuffer[HTTP_BUFFER_SIZE];
size_t AudioStreamer::httpBufferPos = 0;
size_t AudioStreamer::httpBufferLen = 0;
uint32_t AudioStreamer::streamBitrate = 0;
uint32_t AudioStreamer::streamSampleRate = 0;

uint8_t AudioStreamer::audioBuffer[AUDIO_BUFFER_SIZE];
size_t AudioStreamer::audioBufferPos = 0;
size_t AudioStreamer::audioBufferLen = 0;
unsigned long AudioStreamer::lastBufferFill = 0;
unsigned long AudioStreamer::lastMetadataCheck = 0;

bool AudioStreamer::begin() {
    Serial.println("Initializing AudioStreamer...");
    
    // Initialize I2S
    if (!initializeI2S()) {
        Serial.println("ERROR: Failed to initialize I2S");
        currentState = ERROR;
        return false;
    }
    
    // Reset buffers
    resetBuffers();
    
    // Set default stream URL from config
    streamURL = String(Config::settings.streamURL);
    
    currentState = STOPPED;
    Serial.println("AudioStreamer initialized successfully");
    return true;
}

bool AudioStreamer::initializeI2S() {
    // I2S configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_COUNT,
        .dma_buf_len = DMA_BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    // Pin configuration
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_DATA_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    // Install and start I2S driver
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("ERROR: I2S driver install failed: %s\n", esp_err_to_name(err));
        return false;
    }
    
    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("ERROR: I2S pin config failed: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    
    // Start I2S
    err = i2s_start(I2S_PORT);
    if (err != ESP_OK) {
        Serial.printf("ERROR: I2S start failed: %s\n", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    
    Serial.printf("I2S initialized: %d Hz, %d bits, pins BCLK=%d, LRC=%d, DATA=%d\n",
                  I2S_SAMPLE_RATE, I2S_BITS_PER_SAMPLE, I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DATA_PIN);
    return true;
}

bool AudioStreamer::connectToStream(const char* url) {
    if (currentState == PLAYING || currentState == CONNECTING) {
        stop();
        delay(100);
    }
    
    streamURL = String(url);
    Serial.printf("Connecting to stream: %s\n", url);
    
    currentState = CONNECTING;
    
    if (!startHTTPStream()) {
        Serial.println("ERROR: Failed to start HTTP stream");
        currentState = ERROR;
        return false;
    }
    
    currentState = BUFFERING;
    Serial.println("Stream connected, buffering...");
    return true;
}

bool AudioStreamer::startHTTPStream() {
    // Configure HTTP client with better settings
    httpClient.begin(wifiClient, streamURL);
    httpClient.addHeader("User-Agent", "RadioBenziger ESP32/1.0");
    httpClient.addHeader("Icy-MetaData", "1");  // Request metadata
    httpClient.addHeader("Accept", "*/*");
    httpClient.setTimeout(15000);  // Increased timeout
    httpClient.setReuse(false);    // Don't reuse connections
    httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    Serial.printf("Starting HTTP connection to: %s\n", streamURL.c_str());
    
    // Start connection with retry logic
    int httpCode = -1;
    int retryCount = 0;
    const int maxRetries = 3;
    
    while (retryCount < maxRetries && httpCode != HTTP_CODE_OK) {
        if (retryCount > 0) {
            Serial.printf("Retry attempt %d/%d\n", retryCount + 1, maxRetries);
            delay(1000);
        }
        
        httpCode = httpClient.GET();
        Serial.printf("HTTP response code: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK) {
            break;
        } else if (httpCode > 0) {
            // Got a response but not 200 OK
            String payload = httpClient.getString();
            Serial.printf("HTTP error response: %s\n", payload.c_str());
        } else {
            // Connection error
            Serial.printf("HTTP connection error: %s\n", httpClient.errorToString(httpCode).c_str());
        }
        
        retryCount++;
        if (retryCount < maxRetries) {
            httpClient.end();
            delay(500);
            httpClient.begin(wifiClient, streamURL);
            httpClient.addHeader("User-Agent", "RadioBenziger ESP32/1.0");
            httpClient.addHeader("Icy-MetaData", "1");
            httpClient.addHeader("Accept", "*/*");
            httpClient.setTimeout(15000);
            httpClient.setReuse(false);
            httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        }
    }
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("ERROR: HTTP GET failed after %d retries, final code: %d\n", maxRetries, httpCode);
        httpClient.end();
        return false;
    }
    
    // Check if we have a stream
    int contentLength = httpClient.getSize();
    if (contentLength > 0) {
        Serial.printf("Stream has fixed content length: %d bytes\n", contentLength);
    } else {
        Serial.println("Stream has chunked/unknown length (normal for live streams)");
    }
    
    // Parse headers for stream info
    String contentType = httpClient.header("Content-Type");
    String icyBr = httpClient.header("icy-br");
    String icySr = httpClient.header("icy-sr");
    String icyName = httpClient.header("icy-name");
    
    Serial.printf("Stream connected successfully!\n");
    Serial.printf("Content-Type: %s\n", contentType.c_str());
    if (icyBr.length() > 0) {
        streamBitrate = icyBr.toInt();
        Serial.printf("Bitrate: %d kbps\n", streamBitrate);
    }
    if (icySr.length() > 0) {
        streamSampleRate = icySr.toInt();
        Serial.printf("Sample Rate: %d Hz\n", streamSampleRate);
    }
    if (icyName.length() > 0) {
        Serial.printf("Station: %s\n", icyName.c_str());
    }
    
    resetBuffers();
    return true;
}

void AudioStreamer::stop() {
    if (currentState == STOPPED) return;
    
    Serial.println("Stopping audio stream");
    
    stopHTTPStream();
    resetBuffers();
    
    // Clear I2S buffer
    i2s_zero_dma_buffer(I2S_PORT);
    
    currentState = STOPPED;
    currentTitle = "";
    currentArtist = "";
    metadataAvailable = false;
}

void AudioStreamer::stopHTTPStream() {
    httpClient.end();
}

void AudioStreamer::pause() {
    if (currentState == PLAYING) {
        currentState = PAUSED;
        Serial.println("Audio paused");
    }
}

void AudioStreamer::resume() {
    if (currentState == PAUSED) {
        currentState = PLAYING;
        Serial.println("Audio resumed");
    }
}

void AudioStreamer::setVolume(uint8_t level) {
    if (level > 100) level = 100;
    currentVolume = level;
    Serial.printf("Volume set to: %d%%\n", level);
}

void AudioStreamer::update() {
    if (currentState == STOPPED || currentState == ERROR) {
        return;
    }
    
    // Handle HTTP stream reading
    if (currentState == CONNECTING || currentState == BUFFERING || currentState == PLAYING) {
        size_t bytesRead = readHTTPData();
        
        if (bytesRead == 0 && httpClient.connected()) {
            // No data available, but still connected
            return;
        } else if (bytesRead == 0) {
            // Connection lost
            Serial.println("Stream connection lost");
            handleStreamError();
            return;
        }
        
        // Process audio data
        if (httpBufferLen > 0) {
            processAudioData();
        }
        
        // Check if we have enough buffer to start playing
        if (currentState == BUFFERING && audioBufferLen > AUDIO_BUFFER_SIZE / 4) {
            currentState = PLAYING;
            Serial.println("Buffering complete, starting playback");
        }
    }
    
    // Output audio data if playing
    if (currentState == PLAYING && audioBufferLen > 0) {
        // Calculate how much to write (ensure even number for 16-bit samples)
        size_t bytesToWrite = min(audioBufferLen - audioBufferPos, (size_t)512);
        bytesToWrite = (bytesToWrite / 4) * 4; // Align to 32-bit (2 channels * 16-bit)
        
        if (bytesToWrite > 0) {
            writeToI2S(&audioBuffer[audioBufferPos], bytesToWrite);
            audioBufferPos += bytesToWrite;
            
            // Reset buffer if we've consumed it all
            if (audioBufferPos >= audioBufferLen) {
                audioBufferPos = 0;
                audioBufferLen = 0;
            }
        }
    }
}

size_t AudioStreamer::readHTTPData() {
    if (!httpClient.connected()) {
        return 0;
    }
    
    WiFiClient* stream = httpClient.getStreamPtr();
    if (!stream || !stream->available()) {
        return 0;
    }
    
    // Read available data into HTTP buffer
    size_t available = stream->available();
    size_t canRead = min(available, HTTP_BUFFER_SIZE - httpBufferLen);
    
    if (canRead > 0) {
        size_t bytesRead = stream->readBytes(&httpBuffer[httpBufferLen], canRead);
        httpBufferLen += bytesRead;
        return bytesRead;
    }
    
    return 0;
}

bool AudioStreamer::processAudioData() {
    if (httpBufferLen == 0) return false;
    
    // For now, we'll do a simple pass-through of the MP3 data
    // In a full implementation, you'd decode MP3 to PCM here
    // For this demo, we'll assume the stream might have some PCM data
    
    // Calculate how much we can copy to audio buffer
    size_t canCopy = min(httpBufferLen, AUDIO_BUFFER_SIZE - audioBufferLen);
    
    if (canCopy > 0) {
        // Copy data to audio buffer
        memcpy(&audioBuffer[audioBufferLen], httpBuffer, canCopy);
        audioBufferLen += canCopy;
        
        // Shift remaining HTTP buffer data
        if (canCopy < httpBufferLen) {
            memmove(httpBuffer, &httpBuffer[canCopy], httpBufferLen - canCopy);
        }
        httpBufferLen -= canCopy;
        
        return true;
    }
    
    return false;
}

void AudioStreamer::writeToI2S(const uint8_t* data, size_t len) {
    if (currentState != PLAYING) return;
    
    // Apply volume control to 16-bit samples
    int16_t* samples = (int16_t*)data;
    size_t sampleCount = len / 2;
    applyVolumeControl(samples, sampleCount);
    
    // Write to I2S
    size_t bytesWritten = 0;
    esp_err_t err = i2s_write(I2S_PORT, data, len, &bytesWritten, portMAX_DELAY);
    
    if (err != ESP_OK) {
        Serial.printf("ERROR: I2S write failed: %s\n", esp_err_to_name(err));
    }
}

void AudioStreamer::applyVolumeControl(int16_t* samples, size_t count) {
    if (currentVolume >= 100) return; // No attenuation needed
    
    float volumeMultiplier = currentVolume / 100.0f;
    
    for (size_t i = 0; i < count; i++) {
        samples[i] = (int16_t)(samples[i] * volumeMultiplier);
    }
}

void AudioStreamer::handleStreamError() {
    Serial.println("Handling stream error");
    currentState = ERROR;
    stopHTTPStream();
    
    // Try to reconnect after a delay
    delay(1000);
    if (streamURL.length() > 0) {
        Serial.println("Attempting to reconnect...");
        connectToStream(streamURL.c_str());
    }
}

void AudioStreamer::resetBuffers() {
    httpBufferPos = 0;
    httpBufferLen = 0;
    audioBufferPos = 0;
    audioBufferLen = 0;
    memset(httpBuffer, 0, HTTP_BUFFER_SIZE);
    memset(audioBuffer, 0, AUDIO_BUFFER_SIZE);
}

// Status functions
AudioStreamer::State AudioStreamer::getState() {
    return currentState;
}

uint8_t AudioStreamer::getVolume() {
    return currentVolume;
}

String AudioStreamer::getStreamURL() {
    return streamURL;
}

String AudioStreamer::getStatusString() {
    switch (currentState) {
        case STOPPED: return "Stopped";
        case CONNECTING: return "Connecting";
        case BUFFERING: return "Buffering";
        case PLAYING: return "Playing";
        case PAUSED: return "Paused";
        case ERROR: return "Error";
        default: return "Unknown";
    }
}

bool AudioStreamer::isPlaying() {
    return currentState == PLAYING;
}

size_t AudioStreamer::getBufferLevel() {
    return (audioBufferLen * 100) / AUDIO_BUFFER_SIZE;
}

uint32_t AudioStreamer::getBitrate() {
    return streamBitrate;
}

uint32_t AudioStreamer::getSampleRate() {
    return streamSampleRate > 0 ? streamSampleRate : I2S_SAMPLE_RATE;
}

String AudioStreamer::getCurrentTitle() {
    return currentTitle;
}

String AudioStreamer::getCurrentArtist() {
    return currentArtist;
}

bool AudioStreamer::hasMetadata() {
    return metadataAvailable;
}

bool AudioStreamer::parseICYMetadata(const String& metadata) {
    // Simple ICY metadata parsing
    int titleStart = metadata.indexOf("StreamTitle='");
    if (titleStart >= 0) {
        titleStart += 13; // Length of "StreamTitle='"
        int titleEnd = metadata.indexOf("';", titleStart);
        if (titleEnd > titleStart) {
            String fullTitle = metadata.substring(titleStart, titleEnd);
            
            // Try to split artist and title
            int dashPos = fullTitle.indexOf(" - ");
            if (dashPos > 0) {
                currentArtist = fullTitle.substring(0, dashPos);
                currentTitle = fullTitle.substring(dashPos + 3);
            } else {
                currentTitle = fullTitle;
                currentArtist = "";
            }
            
            metadataAvailable = true;
            Serial.printf("Now playing: %s - %s\n", currentArtist.c_str(), currentTitle.c_str());
            return true;
        }
    }
    return false;
} 