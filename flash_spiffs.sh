#!/bin/bash

# Source ESP-IDF environment
source /Users/adamrunner/esp/v5.5/esp-idf/export.sh

# Build the SPIFFS image
echo "Building SPIFFS image..."
python $IDF_PATH/components/spiffs/spiffsgen.py 0x100000 data build/storage.bin

# Flash the SPIFFS image
echo "Flashing SPIFFS image..."
esptool.py --chip esp32c6 --port /dev/ttyUSB0 --baud 460800 write_flash 0x210000 build/storage.bin

echo "SPIFFS image flashed successfully!"