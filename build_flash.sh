#!/bin/bash

# Build and Flash Script for Radio Benziger ESP32 Project
# Usage: ./build_flash.sh <port>
# Example: ./build_flash.sh /dev/ttyUSB0

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="$PWD/build"
FIRMWARE_NAME="radiobenziger_firmware.bin"

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

# Check if port is provided
if [ $# -eq 0 ]; then
    print_error "No port specified!"
    echo "Usage: $0 <port>"
    echo "Example: $0 /dev/ttyUSB0"
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
FIRMWARE_PATH="$BUILD_DIR/$FIRMWARE_NAME"

print_status "Starting combined build and flash process..."
print_status "Target port: $PORT"

# Step 1: Build the firmware
print_status "=== STEP 1: BUILDING FIRMWARE ==="
if ! ./build.sh; then
    print_error "Build failed! Cannot proceed with flashing."
    exit 1
fi

# Verify firmware was built
if [ ! -f "$FIRMWARE_PATH" ]; then
    print_error "Firmware file not found after build: $FIRMWARE_PATH"
    exit 1
fi

print_success "Build completed successfully!"

# Step 2: Flash the firmware
print_status "=== STEP 2: FLASHING FIRMWARE ==="
if ! ./flash.sh "$PORT" "$FIRMWARE_PATH"; then
    print_error "Flash failed!"
    exit 1
fi

print_success "Build and flash completed successfully!"
print_status "Your ESP32 should now be running the Radio Benziger project"
print_status "You can monitor serial output with:"
echo "arduino-cli monitor -p $PORT -c baudrate=115200" 