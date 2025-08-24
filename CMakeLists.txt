cmake_minimum_required(VERSION 3.16.0)

# Set the C++ standard (if needed)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(LOG_FORMAT_CSV)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-bms-monitor)

# Enable WiFi and networking components
set(COMPONENTS main wifi_manager)

# Set custom partition table
set(PARTITION_TABLE_CUSTOM_FILENAME "partitions.csv")
set(CONFIG_PARTITION_TABLE_CUSTOM ON)

# SPIFFS image can be uploaded manually with:
# python $IDF_PATH/components/spiffs/spiffsgen.py 0x100000 spiffs_image storage.bin
# esptool.py --port /dev/ttyUSB0 write_flash 0x110000 storage.bin

# OTA Deployment Commands (provided by idf_ext.py):
# idf.py ota_config  - Configure OTA deployment settings
# idf.py ota_check   - Check for available OTA updates  
# idf.py ota_flash   - Deploy firmware via OTA
# idf.py ota_flash --force  - Force deployment (skip version check)
# See OTA_DEPLOYMENT_GUIDE.md for full documentation
