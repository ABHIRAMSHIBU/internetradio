#!/bin/bash

# Build Script for Radio Benziger ESP32 Project
# Usage: ./build.sh
# This script only builds the firmware, use flash.sh to upload or build_flash.sh for both

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_DIR="$PWD/radiobenziger"
SKETCH_NAME="radiobenziger.ino"
BOARD_FQBN="esp32:esp32:esp32"
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

# Check if Arduino CLI is installed
if ! command -v arduino-cli &> /dev/null; then
    print_error "Arduino CLI is not installed!"
    echo "Please install Arduino CLI first:"
    echo "curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    exit 1
fi

# Check if project directory exists
if [ ! -d "$PROJECT_DIR" ]; then
    print_error "Project directory not found: $PROJECT_DIR"
    exit 1
fi

# Check if sketch file exists
if [ ! -f "$PROJECT_DIR/$SKETCH_NAME" ]; then
    print_error "Sketch file not found: $PROJECT_DIR/$SKETCH_NAME"
    exit 1
fi

print_status "Starting build process for Radio Benziger ESP32 project..."

# Create build directory
mkdir -p "$BUILD_DIR"

# Update Arduino CLI index
print_status "Updating Arduino CLI core index..."
arduino-cli core update-index

# Install ESP32 core if not already installed
print_status "Checking ESP32 core installation..."
if ! arduino-cli core list | grep -q "esp32:esp32"; then
    print_status "Installing ESP32 core..."
    arduino-cli core install esp32:esp32
else
    print_status "ESP32 core already installed"
fi

# Install required libraries (add more as needed)
print_status "Checking required libraries..."
# Uncomment and modify as needed when you add libraries
# arduino-cli lib install "WiFi"
# arduino-cli lib install "HTTPClient"
# arduino-cli lib install "ArduinoJson"

# Navigate to project directory
cd "$PROJECT_DIR"

# Compile the sketch
print_status "Compiling sketch..."
arduino-cli compile --fqbn "$BOARD_FQBN" "$SKETCH_NAME" --build-path "$BUILD_DIR" --verbose

if [ $? -eq 0 ]; then
    print_success "Compilation successful!"
else
    print_error "Compilation failed!"
    exit 1
fi

# Copy the firmware to a standardized name
if [ -f "$BUILD_DIR/$SKETCH_NAME.bin" ]; then
    cp "$BUILD_DIR/$SKETCH_NAME.bin" "$BUILD_DIR/$FIRMWARE_NAME"
    print_success "Firmware generated: $BUILD_DIR/$FIRMWARE_NAME"
elif [ -f "$BUILD_DIR/radiobenziger.ino.bin" ]; then
    cp "$BUILD_DIR/radiobenziger.ino.bin" "$BUILD_DIR/$FIRMWARE_NAME"
    print_success "Firmware generated: $BUILD_DIR/$FIRMWARE_NAME"
else
    print_error "Could not find generated firmware binary!"
    print_status "Looking for binary files in build directory:"
    ls -la "$BUILD_DIR"/*.bin 2>/dev/null || echo "No .bin files found"
    exit 1
fi

print_success "Build completed successfully!"
print_status "To flash the firmware, use:"
echo "./flash.sh /dev/ttyUSB0 $BUILD_DIR/$FIRMWARE_NAME"
print_status "Or use the combined script:"
echo "./build_flash.sh /dev/ttyUSB0"
print_status "See docs/BUILD.md for more information" 