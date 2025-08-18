# ESP32 BMS Monitor

ESP32-C6 based Battery Management System monitor for Daly and JBD BMS units. Provides a common abstraction (`bms_interface`) with per-vendor drivers, periodic polling, and serial output of pack, cell, temperature, MOSFET, and peak power/current data.

## Features
- Unified C interface for BMS data access (`include/bms_interface.h`)
- Daly BMS driver (`components/daly_bms`) and JBD BMS driver (`components/jbd_bms`)
- Periodic polling loop with ESP-IDF/FreeRTOS (`main/main.cpp`)
- Peak current/power tracking, min/max cell voltage, temperature ranges
- Modular logging system with multiple output formats and sinks (`components/logging`)
- WiFi connectivity with credential management (`components/wifi_manager`)
- SNTP time synchronization support
- Example protocol references and tooling under examples/

## Hardware
- Board: ESP32-C6-DevKitC-1
- UART1 to BMS: RX=GPIO4, TX=GPIO5, 9600 baud
- Power the ESP32 appropriately, connect BMS UART GND/RX/TX
```
                 RS485-TTL
┌──────────┐                ┌─────────┐
│          │<----- RX ----->│         │
│ JBD-BMS  │<----- TX ----->│ ESP32   │
│          │<----- GND ---->│         │
│          │                │         │<-- USB to Computer
└──────────┘                └─────────┘



│                JBD-BMS                   │
│                                          │
│                          UART   Balancer │
└─────────────────────────[oooo]──[ooooo]──┘
                            │││
                            │││      (ESP32)
                            │││
                            ││└─ GND (GND)
                            │└── RXD (GPIO4)
                            └─── TXD (GPIO5)
```


## Build and Run (ESP-IDF)

Before building, ensure ESP-IDF is set up correctly:
```bash
# Set up ESP-IDF environment (fish shell)
. /Users/adamrunner/esp/v5.5/esp-idf/export.fish

# Set up ESP-IDF environment (bash/zsh)
. /Users/adamrunner/esp/v5.5/esp-idf/export.sh
```

Build commands:
- Build: `idf.py build`
- Upload: `idf.py flash`
- Monitor: `idf.py monitor`
- Clean: `idf.py clean`
- Combined: `idf.py build flash monitor`

## Configuration

The project includes several configuration files in the `data/` directory:
- `wifi_config.txt`: WiFi credentials and settings
- `mqtt_config.txt`: MQTT broker configuration
- `timezone.txt`: POSIX TZ string used to set local timezone for file rotation (optional; defaults to Pacific with DST)

These files are flashed to SPIFFS using:
```bash
./build_spiffs.sh
./flash_spiffs.sh
```

### Timezone Configuration

Daily SD card file rotation uses the device's local timezone (TZ) when computing the date for filenames and rotation boundaries. Configure the timezone by placing a POSIX TZ string in `data/timezone.txt` (flashed to `/spiffs/timezone.txt`).

- Default (if file missing/empty): `PST8PDT,M3.2.0/2,M11.1.0/2` (Pacific with DST)
- Arizona (no DST): `MST7`
- New York: `EST5EDT,M3.2.0/2,M11.1.0/2`
- UTC: `UTC`

Example:
```
PST8PDT,M3.2.0/2,M11.1.0/2
```

Notes:
- Rotation occurs at local midnight based on TZ; per-line CSV timestamps remain Unix epoch seconds.
- After editing `data/timezone.txt`, re-run `./build_spiffs.sh && ./flash_spiffs.sh` to update the device.

## Project Layout
- `main/main.cpp`: app_main initializes and polls autodetected BMS, manages logging
- `include/bms_interface.h`: C API for measurements and status
- `include/bms_snapshot.h`: Data structures for BMS snapshots and output configuration
- `include/sntp_manager.h`: SNTP time synchronization manager
- `components/daly_bms/`: Daly protocol, data structures, helpers
- `components/jbd_bms/`: JBD packet protocol, parsing, protection flags
- `components/logging/`: Modular logging system with multiple sinks and serializers
- `components/wifi_manager/`: WiFi connection management with credential storage
- `data/`: Configuration files for WiFi and MQTT (flashed to SPIFFS)
- `CMakeLists.txt`: ESP-IDF project configuration

## Usage

1. **Configure WiFi and MQTT** (optional):
   - Edit `data/wifi_config.txt` with your WiFi credentials
   - Edit `data/mqtt_config.txt` with your MQTT broker settings
   - Flash configuration to SPIFFS: `./build_spiffs.sh && ./flash_spiffs.sh`

2. **Build and flash**:
   ```bash
   idf.py build flash monitor
   ```

3. **Monitor output**:
   The system outputs BMS data in configurable formats:
   - Serial output (CSV or human-readable)
   - MQTT publishing (if configured)
   - Multiple logging sinks available

The system will display voltages, currents, SOC, temperatures, MOSFET states, peak values, and per-cell/per-sensor details.

**Note**: GPIO pins are configured as RX=GPIO4, TX=GPIO5. Edit `main/main.cpp` if your wiring differs.

## Roadmap
See [PLAN.md](PLAN.md) for detailed architecture and future work: auto-detect BMS type, SD logging, display integration, robustness (retries/timeouts/CRC).

## References / Inspiration
Inspired by some of the existing Daly and JBD BMS projects, including these used as references:

- [sshoecraft/jbdtool](https://github.com/sshoecraft/jbdtool)
- [maland16/daly-bms-uart](https://github.com/maland16/daly-bms-uart)


## License
MIT-style
