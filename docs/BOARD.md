# ESP32-WROOM-32 Board Configuration

This document provides detailed information about the hardware and software configuration for the Radio Benziger ESP32 streaming project.

## 🔧 Hardware Specifications

### **ESP32-WROOM-32 Module**
- **Chip**: ESP32-D0WD-V3 (revision v3.1)
- **Architecture**: Dual-core Xtensa LX6 32-bit
- **CPU Frequency**: 240MHz
- **Features**: WiFi, Bluetooth, Dual Core
- **Crystal**: 40MHz
- **Voltage**: 3.3V
- **VRef Calibration**: In eFuse
- **Coding Scheme**: None

### **Memory Configuration**
- **Flash Memory**: 4MB (32Mb) - Manufacturer ID: 5e, Device ID: 4016
- **SRAM**: 520KB total
  - 320KB DRAM (Data RAM)
  - 200KB IRAM (Instruction RAM)
- **RTC Memory**: 16KB (8KB slow, 8KB fast)
- **eFuse**: 1024 bits for configuration and security

## 💾 Flash Memory Layout (4MB)

### **Partition Scheme: Default 4MB**
```
Address Range    | Size    | Purpose
-----------------|---------|----------------------------------
0x1000-0x8000   | ~24KB   | Bootloader
0x8000-0xE000   | ~3KB    | Partition Table
0x9000-0xD000   | ~16KB   | NVS (Non-Volatile Storage)
0xD000-0xF000   | ~8KB    | OTA Data (Over-The-Air updates)
0x10000-0x140000| ~1.2MB  | App Partition (your sketch)
0x140000-0x400000| ~1.5MB | SPIFFS/LittleFS (file system)
```

### **Flash Configuration**
- **Flash Mode**: QIO (Quad I/O) - Maximum performance
- **Flash Frequency**: 80MHz - High speed
- **Flash Size**: 4MB (matches physical chip)
- **Flash Voltage**: 3.3V (configured by strapping pin)

### **Current Usage**
- **Program Storage**: 282,630 bytes (21% of 1.31MB available)
- **Dynamic Memory**: 20,484 bytes (6% of 327KB available)
- **Remaining Flash**: ~3.7MB available for data, web files, audio buffers

## 🖥️ Operating System: FreeRTOS

### **FreeRTOS Configuration**
- **Version**: ESP-IDF v5.4 integrated FreeRTOS
- **Scheduler**: Preemptive multitasking
- **Tick Rate**: 1000 Hz (1ms tick period)
- **Core Configuration**: Dual-core (not unicore)
- **SMP Support**: Disabled (Asymmetric Multiprocessing)

### **Task Configuration**
```
Parameter                    | Value
----------------------------|--------
Max Task Name Length        | 16 characters
Idle Task Stack Size         | 1024 bytes
Thread Local Storage Pointers| 1
Stack Overflow Protection    | Canary-based
Backward Compatibility       | Enabled
Timers Support              | Enabled
```

### **Arduino Framework Tasks**
```
Task Name              | Core | Priority | Stack Size | Purpose
-----------------------|------|----------|------------|------------------
Arduino Loop Task      | 1    | 1        | 8192 bytes | Your loop() function
Arduino Setup Task     | 1    | 1        | 8192 bytes | Your setup() function
Serial Event Task      | Any  | 24       | 2048 bytes | Serial event handling
WiFi Task             | 0    | 23       | 4096 bytes | WiFi management
Bluetooth Task        | 0    | 23       | 8192 bytes | BT stack (if used)
```

### **Core Assignment**
- **Core 0 (PRO_CPU)**: WiFi, Bluetooth, system tasks
- **Core 1 (APP_CPU)**: Arduino sketch, user tasks (default)

## 🔌 GPIO Configuration

### **Built-in Connections**
```
GPIO | Function        | Note
-----|-----------------|---------------------------
0    | Boot Button     | Pull to GND for flash mode
2    | Built-in LED    | Blue LED (used in blink sketch)
1    | TX (UART0)      | Serial output
3    | RX (UART0)      | Serial input
```

### **Available GPIOs for Radio Project**
```
GPIO | Type      | Recommended Use
-----|-----------|--------------------------------
4-5  | Digital   | I2S audio output
12-15| Digital   | Control buttons, status LEDs
16-17| Digital   | I2C (SDA/SCL) for displays
18-19| Digital   | SPI (CLK/MISO) for SD card
21-22| Digital   | I2C (SDA/SCL) alternative
23   | Digital   | SPI (MOSI) for SD card
25-26| DAC       | Analog audio output
32-39| ADC       | Volume control, sensors
```

## 🌐 Connectivity Features

### **WiFi Specifications**
- **Standard**: 802.11 b/g/n (2.4 GHz)
- **Modes**: Station, Access Point, Station+AP
- **Security**: WPA/WPA2/WPA3, WEP, Open
- **Range**: ~100m (open space)
- **Power**: Adjustable (0-20 dBm)

### **Bluetooth Specifications**
- **Classic Bluetooth**: v4.2 BR/EDR
- **Bluetooth LE**: v4.2 BLE
- **Profiles**: A2DP, AVRCP, HFP, SPP, GATT

## ⚡ Power Management

### **Power Modes**
```
Mode            | Current Draw | Wake Sources
----------------|--------------|------------------
Active          | ~160-240mA   | Always on
Modem Sleep     | ~20-68mA     | Timer, GPIO
Light Sleep     | ~0.8-1.1mA   | Timer, GPIO, UART
Deep Sleep      | ~10-150µA    | Timer, RTC GPIO, Touch
Hibernation     | ~2.5µA       | Reset only
```

### **Voltage Requirements**
- **Operating Voltage**: 3.0V - 3.6V
- **Recommended**: 3.3V
- **USB Power**: 5V (regulated to 3.3V on dev board)

## 🎵 Audio Capabilities for Radio Streaming

### **Digital Audio Interfaces**
- **I2S**: High-quality digital audio output
- **DAC**: 2x 8-bit DACs (GPIO 25, 26) for analog output
- **ADC**: Multiple channels for audio input/control

### **Audio Processing**
- **Sample Rates**: Up to 48kHz
- **Bit Depth**: 8, 16, 24, 32-bit
- **Formats**: PCM, MP3 (with decoder library)
- **Buffers**: Configurable in PSRAM or internal RAM

### **Recommended Audio Setup for Radio Benziger**
```
Component       | GPIO | Purpose
----------------|------|------------------
I2S BCLK       | 26   | Bit clock
I2S LRCK       | 25   | Left/Right clock
I2S DOUT       | 22   | Data output
Volume Control  | 34   | ADC for potentiometer
Mute Button    | 4    | Digital input
Status LED     | 2    | Built-in LED
```

## 🔧 Development Environment

### **Board Configuration in Arduino IDE**
```
Board: ESP32 Dev Module
Upload Speed: 921600
CPU Frequency: 240MHz (WiFi/BT)
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB (32Mb)
Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
Core Debug Level: None
PSRAM: Disabled
```

### **Compiler Flags**
```
-DF_CPU=240000000L
-DARDUINO_ESP32_DEV
-DARDUINO_ARCH_ESP32
-DESP32=ESP32
-DCORE_DEBUG_LEVEL=0
-DARDUINO_RUNNING_CORE=1
-DARDUINO_EVENT_RUNNING_CORE=1
```

## 📊 Performance Characteristics

### **Memory Performance**
- **Flash Read Speed**: ~40MB/s (QIO mode)
- **SRAM Access**: Single cycle (240MHz)
- **Cache**: 32KB instruction, 32KB data

### **Processing Power**
- **MIPS**: ~600 MIPS (dual core combined)
- **Floating Point**: Hardware accelerated
- **Crypto**: Hardware AES, SHA, RSA acceleration

### **Network Performance**
- **WiFi Throughput**: ~20-30 Mbps (real-world)
- **TCP Connections**: Multiple simultaneous
- **HTTP Streaming**: Excellent for audio streams

## 🎯 Radio Benziger Project Suitability

### **Advantages for Streaming**
✅ **Dual-core processing** - Separate cores for networking and audio  
✅ **4MB flash** - Ample space for web interface and audio buffers  
✅ **FreeRTOS** - Real-time task scheduling for smooth audio  
✅ **Hardware audio** - I2S and DAC for high-quality output  
✅ **WiFi performance** - Sufficient bandwidth for radio streaming  
✅ **Low latency** - Hardware acceleration and efficient OS  

### **Memory Allocation for Radio Project**
```
Purpose                | Allocation | Justification
-----------------------|------------|------------------
HTTP Client/WiFi       | ~100KB     | Connection management
Audio Buffer           | ~64KB      | 2-4 seconds @ 16kHz
MP3 Decoder           | ~50KB      | Decoder state/tables
Web Interface         | ~200KB     | HTML/CSS/JS files
Configuration         | ~16KB      | WiFi credentials, settings
Firmware              | ~300KB     | Current + radio features
Free Space            | ~3.3MB     | Future features, OTA updates
```

## 🔗 External Resources

- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [FreeRTOS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)
- [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32)
- [ESP32 Audio Development Framework](https://github.com/espressif/esp-adf) 