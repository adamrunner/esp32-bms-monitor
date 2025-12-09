#!/bin/bash

# Check if port parameter is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <port> [baud_rate]"
    echo "Example: $0 /dev/ttyUSB0"
    echo "Example: $0 /dev/ttyUSB0 921600"
    exit 1
fi

PORT=$1
BAUD=${2:-460800}

# Source ESP-IDF environment
source /Users/adamrunner/esp/v5.5/esp-idf/export.sh

# Check if ESP-IDF is properly sourced
if [ -z "$IDF_PATH" ]; then
    echo "Error: ESP-IDF environment not found. Please check your ESP-IDF installation."
    exit 1
fi

# Check if spiffsgen.py exists
if [ ! -f "$IDF_PATH/components/spiffs/spiffsgen.py" ]; then
    echo "Error: spiffsgen.py not found at $IDF_PATH/components/spiffs/spiffsgen.py"
    exit 1
fi

# Check if data directory exists
if [ ! -d "data" ]; then
    echo "Error: 'data' directory not found. Please create a 'data' directory with your SPIFFS content."
    exit 1
fi

# Check if build directory exists, create if not
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir -p build
fi

# Build the SPIFFS image
echo "Building SPIFFS image..."
if ! python "$IDF_PATH/components/spiffs/spiffsgen.py" 0x80000 data build/storage.bin; then
    echo "Error: Failed to build SPIFFS image. Check your data directory and permissions."
    exit 1
fi

# Check if SPIFFS image was created successfully
if [ ! -f "build/storage.bin" ]; then
    echo "Error: SPIFFS image not created. Check the build process for errors."
    exit 1
fi

# Verify the SPIFFS image
if [ ! -s "build/storage.bin" ]; then
    echo "Error: SPIFFS image is empty. Check your data directory contents."
    exit 1
fi

echo "SPIFFS image built successfully: build/storage.bin ($(stat -f%z build/storage.bin 2>/dev/null || stat -c%s build/storage.bin) bytes)"

# Flash the SPIFFS image
echo "Flashing SPIFFS image to port $PORT at baud rate $BAUD..."
if ! esptool.py --chip esp32c6 --port "$PORT" --baud "$BAUD" write_flash 0x420000 build/storage.bin; then
    echo "Error: Failed to flash SPIFFS image. Check your connection and port settings."
    echo "Common issues:"
    echo "  - Device not connected or wrong port"
    echo "  - Device not in download mode"
    echo "  - Permission issues (try sudo or add user to dialout group)"
    exit 1
fi

echo "SPIFFS image flashed successfully!"
echo "Port: $PORT"
echo "Baud rate: $BAUD"
echo "Image size: $(stat -f%z build/storage.bin 2>/dev/null || stat -c%s build/storage.bin) bytes"
