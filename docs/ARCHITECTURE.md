# Radio Benziger ESP32 Architecture

## System Overview

Radio Benziger is designed as a modular ESP32-based internet radio with robust WiFi persistence, web-based configuration, and I2S audio output. The architecture prioritizes reliability, maintainability, and efficient resource usage.

## Core Architecture Principles

### 1. **Modular Design**
- **Separation of Concerns**: Each module handles a specific responsibility
- **Clean Interfaces**: Well-defined APIs between components
- **Independent Testing**: Modules can be tested and developed separately
- **Maintainability**: Easy to modify or extend individual components

### 2. **Robust Persistence**
- **Dual Storage**: NVS (primary) + EEPROM (backup) for maximum reliability
- **Automatic Fallback**: Seamless switching between storage methods
- **Data Validation**: Checksum verification for stored configuration
- **Power-Safe**: Configuration survives unexpected power loss

### 3. **Resource Efficiency**
- **Memory Management**: Conservative heap usage (~100KB)
- **Flash Optimization**: 74% program usage, 26% available for expansion
- **CPU Utilization**: Efficient task scheduling with FreeRTOS
- **Network Efficiency**: Minimal bandwidth usage for configuration

## Component Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Radio Benziger ESP32                     │
├─────────────────────────────────────────────────────────────┤
│                  Main Application Loop                      │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │   Web Server    │ │  Status Monitor │ │  LED Controller ││
│  │   (Port 80)     │ │  (Periodic)     │ │  (Visual Feedback)│
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
├─────────────────────────────────────────────────────────────┤
│                    Core Modules Layer                       │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │  WiFiManager    │ │     Config      │ │   I2CScanner    ││
│  │                 │ │                 │ │                 ││
│  │ • Auto-Connect  │ │ • NVS Storage   │ │ • Device Detect ││
│  │ • Config Mode   │ │ • EEPROM Backup │ │ • Status Report ││
│  │ • Reconnection  │ │ • Validation    │ │ • Known Devices ││
│  │ • Status Events │ │ • Persistence   │ │ • Scan Results  ││
│  │ • Status Events │ │ • Persistence   │ │ • Scan Results  ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
├─────────────────────────────────────────────────────────────┤
│                   Hardware Abstraction                      │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │   WiFi Stack    │ │  NVS/EEPROM     │ │   I2C/GPIO      ││
│  │                 │ │                 │ │                 ││
│  │ • ESP32 WiFi    │ │ • Preferences   │ │ • Wire Library  ││
│  │ • Network Events│ │ • EEPROM Lib    │ │ • Digital I/O   ││
│  │ • AP/STA Modes  │ │ • Flash Storage │ │ • Hardware Pins ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
├─────────────────────────────────────────────────────────────┤
│                     ESP32 Hardware                          │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │   Wireless      │ │     Memory      │ │      I/O        ││
│  │                 │ │                 │ │                 ││
│  │ • 2.4GHz WiFi   │ │ • 4MB Flash     │ │ • GPIO Pins     ││
│  │ • Bluetooth     │ │ • 520KB SRAM    │ │ • I2S Interface ││
│  │ • Antenna       │ │ • NVS Partition │ │ • I2C Bus       ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

## Enhanced WiFi Management System

### WiFiManager Class Architecture

```cpp
class WiFiManager {
public:
    enum Status {
        DISCONNECTED,    // Not connected to any network
        CONNECTING,      // Attempting connection
        CONNECTED,       // Successfully connected
        HOTSPOT_MODE,    // Running as access point
        FAILED          // Connection attempt failed
    };

    // Core connection methods
    static bool begin();                           // Initialize WiFi system
    static bool connectToSaved();                  // Connect using stored credentials
    static bool connectToWiFi(ssid, password);    // Connect to new network
    static void startConfigMode();                 // Start AP for configuration
    static void update();                          // Handle reconnection logic

    // Status and monitoring
    static bool isConnected();                     // Check connection status
    static String getIP();                         // Get current IP address
    static Status getStatus();                     // Get detailed status
    
private:
    static const unsigned long CONNECT_TIMEOUT_MS = 15000;  // Connection timeout
    static const unsigned long RECONNECT_INTERVAL = 30000; // Auto-reconnect interval
    static void onWiFiEvent(WiFiEvent_t event);           // Handle WiFi events
};
```

### Connection State Machine

```
┌─────────────┐    begin()     ┌─────────────┐
│   INITIAL   │──────────────→│ INITIALIZED │
└─────────────┘                └─────────────┘
                                       │
                               connectToSaved()
                                       ↓
┌─────────────┐               ┌─────────────┐
│   FAILED    │←─── timeout ──│ CONNECTING  │
└─────────────┘               └─────────────┘
       │                             │
       │ startConfigMode()           │ success
       ↓                             ↓
┌─────────────┐               ┌─────────────┐
│ HOTSPOT_MODE│               │  CONNECTED  │
└─────────────┘               └─────────────┘
       ↑                             │
       │ connection lost             │
       └─────────────────────────────┘
```

## Enhanced Configuration System

### Dual Storage Architecture

```cpp
class Config {
    struct Settings {
        char wifiSSID[64];        // WiFi network name
        char wifiPassword[64];    // WiFi password
        char streamURL[256];      // Radio stream URL
        char deviceName[32];      // Device identifier
        bool autoStart;           // Auto-start streaming
        uint32_t checksum;        // Data integrity validation
    };

    // Storage methods
    static bool saveToNVS();      // Primary storage (Preferences)
    static bool loadFromNVS();    // Load from NVS
    static bool saveToEEPROM();   // Backup storage (EEPROM)
    static bool loadFromEEPROM(); // Load from EEPROM
    static bool validateEEPROM(); // Verify checksum
};
```

### Storage Fallback Logic

```
┌─────────────┐    begin()     ┌─────────────┐
│   CONFIG    │──────────────→│  LOAD NVS   │
│   INIT      │                └─────────────┘
└─────────────┘                       │
                                      │ success
                                      ↓
                               ┌─────────────┐
                               │   READY     │
                               └─────────────┘
                                      ↑
                                      │ fallback
                               ┌─────────────┐
                               │ LOAD EEPROM │←── NVS failed
                               └─────────────┘
                                      │
                                      │ failed
                                      ↓
                               ┌─────────────┐
                               │  DEFAULTS   │
                               └─────────────┘
```

## Web Interface Architecture

### HTTP Server Structure

```
┌─────────────────────────────────────────────────────────────┐
│                     WebServer (Port 80)                     │
├─────────────────────────────────────────────────────────────┤
│                      Route Handlers                         │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────┐│
│  │     /       │ │   /config   │ │   /stream   │ │  /info  ││
│  │             │ │             │ │             │ │         ││
│  │ • Status    │ │ • WiFi Scan │ │ • Stream    │ │ • System││
│  │ • Dashboard │ │ • Network   │ │ • URL Config│ │ • Debug ││
│  │ • Navigation│ │ • Credentials│ │ • Save      │ │ • Stats ││
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────┘│
├─────────────────────────────────────────────────────────────┤
│                    Response Generation                       │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────┐│
│  │    HTML     │ │    JSON     │ │ Form Handle │ │ Redirect││
│  │             │ │             │ │             │ │         ││
│  │ • Templates │ │ • API Data  │ │ • POST Data │ │ • Status││
│  │ • Styling   │ │ • Status    │ │ • Validation│ │ • Config││
│  │ • JavaScript│ │ • Networks  │ │ • Storage   │ │ • Success││
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────┘│
└─────────────────────────────────────────────────────────────┘
```

### Configuration Flow

```
User Browser ←→ ESP32 Web Server ←→ Config System ←→ Storage (NVS/EEPROM)
     │                 │                │                    │
     │ GET /config     │                │                    │
     │────────────────→│                │                    │
     │                 │ loadSettings() │                    │
     │                 │───────────────→│                    │
     │                 │                │ readFromStorage()  │
     │                 │                │───────────────────→│
     │                 │                │←───────────────────│
     │                 │←───────────────│                    │
     │ HTML Form       │                │                    │
     │←────────────────│                │                    │
     │                 │                │                    │
     │ POST /config    │                │                    │
     │────────────────→│                │                    │
     │                 │ saveSettings() │                    │
     │                 │───────────────→│                    │
     │                 │                │ writeToStorage()   │
     │                 │                │───────────────────→│
     │                 │                │←───────────────────│
     │                 │←───────────────│                    │
     │ Success Page    │                │                    │
     │←────────────────│                │                    │
```

## Status Monitoring System

### Periodic Reporting Architecture

```cpp
// Main loop monitoring intervals
const unsigned long STATUS_INTERVAL = 30000;     // System status every 30s
const unsigned long IP_PRINT_INTERVAL = 5000;    // IP address every 5s

void loop() {
    server.handleClient();        // Process web requests
    WiFiManager::update();        // Handle reconnection logic
    
    // LED status indication
    updateLEDStatus();
    
    // Periodic IP address printing
    if (WiFiManager::isConnected() && 
        millis() - lastIPPrint > IP_PRINT_INTERVAL) {
        printIPStatus();
        lastIPPrint = millis();
    }
    
    // Comprehensive status reporting
    if (millis() - lastStatusPrint > STATUS_INTERVAL) {
        printSystemStatus();
        Config::printStatus();
        lastStatusPrint = millis();
    }
    
    // Connection state management
    handleConnectionStateChanges();
}
```

### Status Information Hierarchy

```
System Status Report
├── Uptime & Performance
│   ├── Runtime (seconds/minutes)
│   ├── Free heap memory
│   ├── Largest free block
│   └── CPU frequency
├── WiFi Status
│   ├── Connection state
│   ├── SSID and IP address
│   ├── Signal strength (RSSI)
│   ├── MAC address
│   ├── Gateway and DNS
│   └── Connection events
├── Configuration Status
│   ├── Storage method (NVS/EEPROM)
│   ├── Saved credentials status
│   ├── Stream URL configuration
│   └── Device settings
└── Hardware Status
    ├── I2C devices detected
    ├── LED status indication
    └── GPIO pin states
```

## I2C Device Management

### I2CScanner Architecture

```cpp
class I2CScanner {
    struct Device {
        uint8_t address;      // I2C address (7-bit)
        String name;          // Device name/description
        bool responding;      // Current response status
    };

    // Known device database
    static const KnownDevice knownDevices[];
    
    // Scanning and detection
    static std::vector<Device> scanBus();          // Scan for devices
    static bool isDevicePresent(uint8_t address); // Check specific device
    static String getDeviceName(uint8_t address); // Get device name
    static int getDeviceCount();                   // Count found devices
};
```

### Device Detection Flow

```
I2C Bus Scan
├── Initialize I2C (GPIO 21/22)
├── Scan addresses 0x01 to 0x7F
│   ├── Send start condition
│   ├── Send device address
│   ├── Check for ACK response
│   └── Record responding devices
├── Match against known device database
│   ├── OLED displays (0x3C, 0x3D)
│   ├── ADC modules (0x48-0x4B)
│   ├── RTC/IMU devices (0x68)
│   └── Environmental sensors (0x76, 0x77)
└── Generate status report
    ├── Device count
    ├── Device list with names
    └── I2C bus health status
```

## Memory Management Strategy

### Flash Memory Layout (4MB Total)

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Flash Memory (4MB)                 │
├─────────────────────────────────────────────────────────────┤
│ Bootloader (64KB)     │ 0x1000 - 0x10000                   │
├─────────────────────────────────────────────────────────────┤
│ Partition Table (4KB) │ 0x8000 - 0x9000                    │
├─────────────────────────────────────────────────────────────┤
│ NVS Storage (20KB)    │ 0x9000 - 0xE000                    │
├─────────────────────────────────────────────────────────────┤
│ Application (1.25MB)  │ 0x10000 - 0x140000                 │
│ • Program Code: 978KB │ • Currently 74% utilized           │
│ • Available: 332KB    │ • Space for audio libraries        │
├─────────────────────────────────────────────────────────────┤
│ OTA Update (1.25MB)   │ 0x140000 - 0x270000                │
│ • Future OTA Support  │ • Mirror of application partition   │
├─────────────────────────────────────────────────────────────┤
│ SPIFFS/LittleFS       │ 0x270000 - 0x400000                │
│ • File system space   │ • 1.5MB available                  │
│ • Audio buffers       │ • Stream caching                   │
│ • Web assets          │ • Configuration backup             │
└─────────────────────────────────────────────────────────────┘
```

### RAM Memory Layout (520KB Total)

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 SRAM (520KB)                      │
├─────────────────────────────────────────────────────────────┤
│ System Reserved       │ ~40KB  │ FreeRTOS, WiFi stack      │
├─────────────────────────────────────────────────────────────┤
│ Application Heap      │ ~48KB  │ Current program usage     │
│ • Global variables    │        │ • Config data             │
│ • Dynamic allocation  │        │ • Web server buffers     │
│ • WiFi buffers        │        │ • String operations       │
├─────────────────────────────────────────────────────────────┤
│ Available for Audio   │ ~279KB │ Future audio implementation│
│ • Stream buffers      │        │ • 32KB recommended        │
│ • Decode buffers      │        │ • MP3 decoder workspace   │
│ • I2S DMA buffers     │        │ • Metadata storage        │
├─────────────────────────────────────────────────────────────┤
│ Stack Space           │ ~153KB │ Task stacks               │
│ • Main task stack     │        │ • Web server tasks        │
│ • WiFi task stacks    │        │ • Audio processing tasks  │
│ • FreeRTOS overhead   │        │ • Interrupt handlers      │
└─────────────────────────────────────────────────────────────┘
```

## Build System Architecture

### Compilation Pipeline

```
Source Files → Arduino CLI → ESP32 Toolchain → Firmware Binary
     │              │              │               │
     │              │              │               └─→ radiobenziger_firmware.bin
     │              │              └─→ Linking & Optimization
     │              └─→ Library Resolution & Compilation
     └─→ Preprocessing & Dependency Analysis

Build Scripts:
├── build.sh          # Compile only
├── flash.sh          # Upload to device  
└── build_flash.sh    # Combined build & flash
```

### Library Dependencies

```
Project Dependencies
├── Core ESP32 Libraries
│   ├── WiFi (3.2.0)          # WiFi connectivity
│   ├── WebServer (3.2.0)     # HTTP server
│   ├── Preferences (3.2.0)   # NVS storage
│   ├── EEPROM (3.2.0)        # EEPROM backup
│   └── Wire (3.2.0)          # I2C communication
├── Network Libraries
│   └── Networking (3.2.0)    # Network abstraction
└── Future Audio Libraries
    ├── ESP32-audioI2S         # Audio streaming (planned)
    ├── ESP32_MP3_Decoder      # MP3 decoding (planned)
    └── I2S Audio Driver       # Hardware interface (planned)
```

## Error Handling & Recovery

### Fault Tolerance Strategy

```
Error Scenarios & Recovery
├── WiFi Connection Failures
│   ├── Auto-retry every 30 seconds
│   ├── Fallback to configuration mode
│   ├── Visual indication via LED
│   └── Web interface status updates
├── Configuration Storage Failures
│   ├── NVS failure → EEPROM fallback
│   ├── EEPROM corruption → defaults
│   ├── Checksum validation
│   └── Automatic repair attempts
├── Web Server Errors
│   ├── Graceful error pages
│   ├── Request timeout handling
│   ├── Memory cleanup
│   └── Service restart capability
└── Hardware Communication Failures
    ├── I2C bus recovery
    ├── GPIO state validation
    ├── Peripheral re-initialization
    └── Diagnostic reporting
```

## Future Audio Architecture (Phase 2)

### Planned Audio Pipeline

```
Internet Stream → HTTP Client → MP3 Decoder → I2S DAC → Audio Output
      │              │            │            │           │
      │              │            │            │           └─→ MAX98357A
      │              │            │            └─→ ESP32 I2S peripheral
      │              │            └─→ ESP32-audioI2S library
      │              └─→ WiFiClient with stream buffering
      └─→ https://icecast.octosignals.com/benziger
```

### Audio System Components

```cpp
// Future audio classes (Phase 2)
class AudioStreamer {
    static bool begin();                    // Initialize audio system
    static bool connectToStream(url);       // Connect to radio stream
    static void setVolume(level);          // Control audio volume
    static bool isPlaying();               // Check playback status
    static String getCurrentTrack();       // Get metadata
};

class I2SOutput {
    static bool initializeDAC();          // Setup MAX98357A
    static void writeAudioData(buffer);   // Send audio to DAC
    static void setGain(level);           // Hardware volume control
};
```

This architecture provides a solid foundation for the current WiFi and configuration functionality while being designed to easily accommodate the future audio streaming components. The modular design ensures that each component can be developed, tested, and maintained independently while working together as a cohesive system.
