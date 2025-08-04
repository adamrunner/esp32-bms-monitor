# ESP32 BMS Monitor

ESP32-C6 based Battery Management System monitor for Daly and JBD BMS units. Provides a common abstraction (bms_interface) with per-vendor drivers, periodic polling, and serial output of pack, cell, temperature, MOSFET, and peak power/current data.

## Features
- Unified C interface for BMS data access (include/bms_interface.h)
- Daly BMS driver (lib/daly_bms) and JBD BMS driver (lib/jbd_bms)
- Periodic polling loop with ESP-IDF/FreeRTOS (src/main.cpp)
- Peak current/power tracking, min/max cell voltage, temperature ranges
- Example protocol references and tooling under examples/

## Hardware
- Board: ESP32-C6-DevKitC-1
- UART1 to BMS: RX=GPIO16, TX=GPIO17 (default), 9600 baud
- Power the ESP32 appropriately, connect BMS UART GND/RX/TX

## Build and Run (PlatformIO)
- Build: platformio run -e esp32-c6-devkitc-1
- Upload: platformio run -e esp32-c6-devkitc-1 -t upload
- Monitor: platformio device monitor (configure speed in platformio.ini)
- Clean: platformio run -e esp32-c6-devkitc-1 -t clean
- Tests (all): platformio test -e esp32-c6-devkitc-1
- Tests (single): platformio test -e esp32-c6-devkitc-1 -f <test_name>

## Project Layout
- src/main.cpp: app_main initializes and polls autodetected BMS, prints readings
- include/bms_interface.h: C API for measurements and status
- lib/daly_bms/*: Daly protocol, data structures, helpers
- lib/jbd_bms/*: JBD packet protocol, parsing, protection flags
- examples/: protocol docs, sketches, captures, PC tools
- platformio.ini: env esp32-c6-devkitc-1, framework espidf

## Usage
Flash, open serial monitor, and observe periodic dumps of voltages, currents, SOC, temperatures, MOSFET states, peak values, and per-cell/per-sensor details. Edit UART pins/baud in the respective headers if your wiring differs.

## Roadmap
See PLAN.md for detailed architecture and future work: auto-detect BMS type, SD logging, display integration, robustness (retries/timeouts/CRC).

## License
Project code MIT-style; see library and example subfolders for their respective licenses.
