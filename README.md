# ESP32 BMS Monitor

ESP32 based Battery Management System monitor for Daly and JBD BMS units. Provides a common abstraction (`bms_interface`) with per-vendor drivers, periodic polling, and serial output of pack, cell, temperature, MOSFET, and peak power/current data.

## Features
- Unified C interface for BMS data access (`include/bms_interface.h`)
- Daly BMS driver (`lib/daly_bms`) and JBD BMS driver (`lib/jbd_bms`)
- Periodic polling loop with Arduino framework (`src/main.cpp`)
- Peak current/power tracking, min/max cell voltage, temperature ranges
- Example protocol references and tooling under examples/

## Hardware
- Board: ESP32 Dev Module
- UART1 to BMS: RX=GPIO27, TX=GPIO14, 9600 baud
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
                            │└── RXD (GPIO27)
                            └─── TXD (GPIO14)
```


## Build and Run (PlatformIO)
- Build: `pio run`
- Upload: `pio run -t upload`
- Monitor: `pio run -t monitor` (configure speed in `platformio.ini`)
- Clean: `pio run -t clean`
- Tests (all): `pio test`
- Tests (single): `pio test -f <test_name>`

## Project Layout
- `src/main.cpp`: Arduino setup()/loop() initializes and polls autodetected BMS, prints readings
- `include/bms_interface.h`: C API for measurements and status
- `lib/daly_bms/*`: Daly protocol, data structures, helpers
- `lib/jbd_bms/*`: JBD packet protocol, parsing, protection flags
- `platformio.ini`: env esp32dev, framework arduino

## Usage
Flash, open serial monitor, and you'll see scroll back of voltages, currents, SOC, temperatures, MOSFET states, peak values, and per-cell/per-sensor details. 

Make sure to edit UART pins/baud in the respective files if your wiring differs.

## Roadmap
See [PLAN.md](PLAN.md) for detailed architecture and future work: auto-detect BMS type, SD logging, display integration, robustness (retries/timeouts/CRC).

## References / Inspiration
Inspired by some of the existing Daly and JBD BMS projects, including these used as references:

- [sshoecraft/jbdtool](https://github.com/sshoecraft/jbdtool)
- [maland16/daly-bms-uart](https://github.com/maland16/daly-bms-uart)


## License
MIT-style
