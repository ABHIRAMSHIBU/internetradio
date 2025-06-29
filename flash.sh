#!/bin/bash

# Flash Script for Radio Benziger ESP32 Project
# Usage: ./flash.sh <port> <firmware_file>
# Example: ./flash.sh /dev/ttyUSB0 build/radiobenziger_firmware.bin

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BOARD_FQBN="esp32:esp32:esp32"
UPLOAD_SPEED="921600"
BUILD_DIR="$PWD/build"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if correct number of arguments provided
if [ $# -lt 1 ]; then
    print_error "Insufficient arguments!"
    echo "Usage: $0 <port> [firmware_file]"
    echo "Example: $0 /dev/ttyUSB0"
    echo "Example: $0 /dev/ttyUSB0 build/radiobenziger_firmware.bin"
    echo ""
    echo "Available ports:"
    if command -v arduino-cli &> /dev/null; then
        arduino-cli board list
    else
        echo "Arduino CLI not found. Please install it first."
    fi
    exit 1
fi

PORT=$1

# Determine firmware file
if [ $# -eq 2 ]; then
    FIRMWARE_FILE=$2
else
    # Default firmware file
    FIRMWARE_FILE="$BUILD_DIR/radiobenziger_firmware.bin"
fi

# Check if Arduino CLI is installed
if ! command -v arduino-cli &> /dev/null; then
    print_error "Arduino CLI is not installed!"
    echo "Please install Arduino CLI first:"
    echo "curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    exit 1
fi

# Check if firmware file exists
if [ ! -f "$FIRMWARE_FILE" ]; then
    print_error "Firmware file not found: $FIRMWARE_FILE"
    echo ""
    print_status "Available firmware files:"
    find . -name "*.bin" -type f 2>/dev/null | head -10 || echo "No .bin files found"
    echo ""
    print_status "Did you run ./build.sh first?"
    exit 1
fi

# Check if port exists
if [ ! -e "$PORT" ]; then
    print_warning "Port $PORT not found. Available ports:"
    arduino-cli board list
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

print_status "Starting flash process for Radio Benziger ESP32 project..."
print_status "Port: $PORT"
print_status "Firmware: $FIRMWARE_FILE"
print_status "Upload Speed: $UPLOAD_SPEED"

# Get firmware size
FIRMWARE_SIZE=$(stat -c%s "$FIRMWARE_FILE" 2>/dev/null || stat -f%z "$FIRMWARE_FILE" 2>/dev/null || echo "unknown")
print_status "Firmware size: $FIRMWARE_SIZE bytes"

# Check if we need to find esptool path
ESPTOOL_PATH=""
if command -v esptool &> /dev/null; then
    ESPTOOL_PATH="esptool"
elif [ -f ~/.arduino15/packages/esp32/tools/esptool_py/*/esptool ]; then
    ESPTOOL_PATH=$(find ~/.arduino15/packages/esp32/tools/esptool_py/*/esptool | head -1)
else
    print_error "esptool not found!"
    print_status "Please ensure Arduino CLI and ESP32 core are properly installed"
    exit 1
fi

print_status "Using esptool: $ESPTOOL_PATH"

# Flash the firmware
print_status "Flashing firmware to ESP32..."
print_status "If upload fails, try holding the BOOT button on your ESP32"

# Use esptool to flash the firmware
$ESPTOOL_PATH --chip esp32 --port "$PORT" --baud $UPLOAD_SPEED \
    --before default_reset --after hard_reset write_flash \
    -z --flash_mode keep --flash_freq keep --flash_size keep \
    0x10000 "$FIRMWARE_FILE"

if [ $? -eq 0 ]; then
    print_success "Flash successful!"
    print_status "Your ESP32 should now be running the Radio Benziger project"
    print_status "You can monitor serial output with:"
    echo "arduino-cli monitor -p $PORT -c baudrate=115200"
else
    print_error "Flash failed!"
    print_warning "Troubleshooting tips:"
    echo "1. Hold the BOOT button while flashing"
    echo "2. Check if the port is correct: arduino-cli board list"
    echo "3. Make sure no other program is using the serial port"
    echo "4. Try a different USB cable or port"
    echo "5. Verify the firmware file is not corrupted"
    exit 1
fi

print_success "Flash completed successfully!" 