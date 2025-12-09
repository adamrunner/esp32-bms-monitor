#!/bin/bash

# Script to build and flash SPIFFS image for ESP32 BMS Monitor

# Source ESP-IDF environment
source /Users/adamrunner/esp/v5.5/esp-idf/export.sh

# Define variables
PROJECT_DIR="/Users/adamrunner/Code/esp32-bms-monitor"
BUILD_DIR="$PROJECT_DIR/build"
SPIFFS_SIZE="0x80000"  # 512KB as defined in partitions.csv
SPIFFS_ADDRESS="0x420000"  # Address of storage partition as defined in partitions.csv
DATA_DIR="$PROJECT_DIR/data"

echo "ESP32 BMS Monitor SPIFFS Image Builder and Flasher"
echo "=================================================="
echo "Project directory: $PROJECT_DIR"
echo "Data directory: $DATA_DIR"
echo "SPIFFS size: $SPIFFS_SIZE"
echo "SPIFFS address: $SPIFFS_ADDRESS"
echo

# Check if data directory exists
if [ ! -d "$DATA_DIR" ]; then
    echo "Error: Data directory $DATA_DIR does not exist!"
    exit 1
fi

# Create the data directory if it doesn't exist
mkdir -p "$DATA_DIR"

# Show contents of data directory
echo "Contents of data directory:"
ls -la "$DATA_DIR"
echo

# Build SPIFFS image using spiffsgen.py
echo "Building SPIFFS image..."
python $IDF_PATH/components/spiffs/spiffsgen.py $SPIFFS_SIZE "$DATA_DIR" "$BUILD_DIR/storage.bin"

if [ $? -ne 0 ]; then
    echo "Error: Failed to build SPIFFS image!"
    exit 1
fi

echo "SPIFFS image built successfully: $BUILD_DIR/storage.bin"
echo

# Check if storage.bin was created
if [ ! -f "$BUILD_DIR/storage.bin" ]; then
    echo "Error: SPIFFS image file $BUILD_DIR/storage.bin was not created!"
    exit 1
fi

# Show size of the created image
echo "SPIFFS image size:"
ls -lh "$BUILD_DIR/storage.bin"
echo

# Flash SPIFFS image
echo "Flashing SPIFFS image to ESP32..."
echo "Make sure your ESP32 is connected and in programming mode!"
echo

# Try to flash with common serial ports
FLASHED=false

for PORT in "/dev/ttyUSB0" "/dev/ttyUSB1" "/dev/ttyACM0" "/dev/tty.usbserial-*" "/dev/tty.usbmodem1101"; do
    if [ -e "$PORT" ] || [[ "$PORT" == *"*" ]]; then
        echo "Trying to flash using port: $PORT"
        esptool.py --chip esp32c6 --port "$PORT" --baud 460800 write_flash $SPIFFS_ADDRESS "$BUILD_DIR/storage.bin"

        if [ $? -eq 0 ]; then
            echo "SPIFFS image flashed successfully to $PORT!"
            FLASHED=true
            break
        else
            echo "Failed to flash to $PORT, trying next port..."
        fi
    fi
done

if [ "$FLASHED" = false ]; then
    echo "Could not flash to any serial port. Please specify a port manually:"
    echo "esptool.py --chip esp32c6 --port /dev/YOUR_PORT --baud 460800 write_flash $SPIFFS_ADDRESS $BUILD_DIR/storage.bin"
    exit 1
fi

echo
echo "SPIFFS image has been successfully built and flashed!"
