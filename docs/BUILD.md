# Build Instructions for Radio Benziger ESP32 Project

## Prerequisites

### Hardware Requirements
- ESP32-WROOM-32 development board
- USB cable for programming and power
- Computer with USB port

### Software Requirements
- Arduino CLI installed on your system
- ESP32 board package for Arduino
- Required libraries (will be installed automatically by build script)

## Installation

### 1. Install Arduino CLI
```bash
# On Linux/macOS
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Add to PATH (add this to your ~/.bashrc or ~/.zshrc)
export PATH=$PATH:$HOME/bin
```

### 2. Install ESP32 Board Package
```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

## Building and Uploading

The build system provides three different scripts for different workflows:

### Option 1: Build and Flash in One Command
```bash
./build_flash.sh /dev/ttyUSB0
```

### Option 2: Separate Build and Flash
```bash
# First build the firmware
./build.sh

# Then flash it to the device
./flash.sh /dev/ttyUSB0 build/radiobenziger_firmware.bin
```

### Option 3: Flash Pre-built Firmware
```bash
# If you already have a firmware file
./flash.sh /dev/ttyUSB0 path/to/firmware.bin
```

Replace `/dev/ttyUSB0` with your actual ESP32 device port:
- Linux: Usually `/dev/ttyUSB0`, `/dev/ttyUSB1`, or `/dev/ttyACM0`
- macOS: Usually `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- Windows: Usually `COM3`, `COM4`, etc.

### Manual Build Steps
If you prefer to build manually:

```bash
# Navigate to project directory
cd radiobenziger

# Compile the sketch
arduino-cli compile --fqbn esp32:esp32:esp32 radiobenziger.ino --build-path ../build

# Upload to ESP32 (replace PORT with your device port)
arduino-cli upload -p PORT --fqbn esp32:esp32:esp32 radiobenziger.ino
```

## Finding Your ESP32 Port

### Linux
```bash
# List connected devices
ls /dev/ttyUSB* /dev/ttyACM*

# Or use dmesg to see recently connected devices
dmesg | grep tty
```

### macOS
```bash
ls /dev/cu.*
```

### Windows
Check Device Manager under "Ports (COM & LPT)"

## Troubleshooting

### Common Issues

1. **Permission denied on Linux**
   ```bash
   sudo usermod -a -G dialout $USER
   # Log out and log back in
   ```

2. **ESP32 not detected**
   - Check USB cable (must support data, not just power)
   - Try different USB port
   - Press and hold BOOT button while connecting

3. **Upload failed**
   - Hold BOOT button during upload
   - Check if another program is using the serial port
   - Try lower baud rate in build script

4. **Compilation errors**
   - Ensure all required libraries are installed
   - Check Arduino CLI and ESP32 core versions

## Board Configuration

The project is configured for ESP32-WROOM-32 with these settings:
- Board: ESP32 Dev Module
- Upload Speed: 921600
- CPU Frequency: 240MHz
- Flash Frequency: 80MHz
- Flash Mode: DIO
- Flash Size: 4MB
- Partition Scheme: Default 4MB with spiffs

## Build Scripts

### build.sh
Compiles the Arduino sketch and generates firmware binary in the `build/` directory.
- Input: None (uses `radiobenziger/radiobenziger.ino`)
- Output: `build/radiobenziger_firmware.bin`

### flash.sh
Flashes firmware to ESP32 using esptool.
- Usage: `./flash.sh <port> [firmware_file]`
- If no firmware file specified, uses `build/radiobenziger_firmware.bin`

### build_flash.sh
Combines build and flash operations in one command.
- Usage: `./build_flash.sh <port>`
- Automatically builds then flashes the firmware

## Project Structure

```
radiobenziger/
├── radiobenziger/
│   └── radiobenziger.ino         # Main Arduino sketch
├── build/                        # Build output directory
│   └── radiobenziger_firmware.bin # Generated firmware
├── build.sh                      # Build-only script
├── flash.sh                      # Flash-only script
├── build_flash.sh                # Combined build and flash script
├── BUILD.md                      # This file
└── README.md                    # Project overview
``` 