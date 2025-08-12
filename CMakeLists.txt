cmake_minimum_required(VERSION 3.16.0)

# Set the C++ standard (if needed)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile definitions from platformio.ini
add_compile_definitions(LOG_FORMAT_CSV)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-bms-monitor)
