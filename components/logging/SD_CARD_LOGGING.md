# SD Card Logging Implementation

## Overview

This document describes the SD Card logging functionality added to the ESP32 BMS monitor system. The implementation provides persistent local storage of BMS telemetry data in CSV format with advanced features like file rotation, buffering, and error handling.

## Features

### Core Functionality
- SPI Interface: Uses 1-bit SPI interface for SD card communication
- CSV Format: Stores data in CSV format compatible with existing serializers
- Buffered Writes: Configurable buffer size with timed flush intervals
- File Rotation: Daily rotation with line-count fallback (default 10,000 lines)
- Timestamped Files: Automatic filename generation with date stamps
- CSV Headers: Automatic CSV header writing for each new file

### Advanced Features
- Free Space Monitoring: Configurable minimum free space checking
- Error Handling: Graceful handling of SD card removal/insertion
- Thread Safety: Mutex-protected buffer operations
- Configuration: Full JSON configuration support
- Statistics: File and write statistics tracking
- Fsync Throttling: Configurable fsync interval to avoid frequent long blocking syncs

## Configuration

### JSON Configuration Format (example)

```json
{
  "type": "sdcard",
  "config": {
    "mount_point": "/sdcard",
    "file_prefix": "bms_data",
    "file_extension": ".csv",
    "buffer_size": 32768,
    "flush_interval_ms": 120000,
    "fsync_interval_ms": 60000,
    "max_lines_per_file": 10000,
    "enable_free_space_check": true,
    "min_free_space_mb": 10,
    "spi": {
      "mosi_pin": 23,
      "miso_pin": 19,
      "clk_pin": 18,
      "cs_pin": 22,
      "freq_khz": 10000
    }
  }
}
```

### Configuration Parameters

- mount_point (string, default: "/sdcard"): SD card mount point
- file_prefix (string, default: "bms"): Prefix for log files
- file_extension (string, default: ".csv"): File extension
- buffer_size (number, default: 10240): Write buffer size in bytes
- flush_interval_ms (number, default: 30000): Buffer flush interval in milliseconds
- fsync_interval_ms (number, default: 60000): Minimum interval between fsync calls in milliseconds (0 disables periodic fsync)
- max_lines_per_file (number, default: 10000): Maximum lines per file before rotation
- enable_free_space_check (boolean, default: true): Enable free space monitoring
- min_free_space_mb (number, default: 10): Minimum free space in MB
- spi (object): SPI configuration block (see below)

### SPI Configuration

Defaults (match code defaults in SDCardConfig):
- mosi_pin (number, default: 23): SPI MOSI pin
- miso_pin (number, default: 19): SPI MISO pin
- clk_pin (number, default: 18): SPI clock pin
- cs_pin (number, default: 22): SPI chip select pin
- freq_khz (number, default: 10000): SPI frequency in kHz

Note: The implementation also configures stronger drive strength on MOSI/CLK/CS and an internal pull-up on MISO to improve signal integrity.

## File Naming Convention

Files are automatically named using the following pattern:
- {YYYYMMDD}.csv — Daily files (numbers-only date)
- {YYYYMMDD}NNN.csv — Sequential files for same day if a file exists (NNN = 001..999)
- uptime_{seconds}.csv — Fallback when no time sync available

Examples:
- 20250315.csv
- 20250315001.csv
- uptime_3600.csv

## File Rotation

### Daily Rotation
- Files rotate automatically when the date string changes
- New file created with updated date stamp
- Previous file is flushed and closed

### Line Count Rotation
- Fallback rotation when file exceeds max_lines_per_file
- Sequential numbering for same-day files
- Prevents excessively large files

## Error Handling

### SD Card States
- UNINITIALIZED — Initial state
- INITIALIZING — During initialization
- READY — Normal operation
- ERROR_NO_CARD — SD card not detected or mount point not accessible
- ERROR_MOUNT_FAILED — Filesystem mount failed
- ERROR_DISK_FULL — Insufficient free space
- ERROR_IO_FAILURE — I/O operation failed

### Error Recovery
- Non-fatal fsync failures are logged as warnings
- On write/flush errors the sink transitions to an error state and stops writing (future work: add retries/re-init)

## Integration

### Build System
The SD card sink is automatically included when INCLUDE_SDCARD_SINK=1 is defined in the build system.

### Dependencies
- fatfs — FAT filesystem support
- sdmmc — SD/MMC card driver
- esp_driver_spi — SPI master driver

### Usage Example (from main.cpp)

```cpp
std::string logging_config = R"({"sinks":[
  {"type":"serial","config":{"format":"csv","print_header":true,"max_cells":4,"max_temps":3}},
  {"type":"mqtt","config":{"format":"csv","topic":"bms/telemetry","qos":1}},
  {"type":"sdcard","config":{
    "file_prefix":"bms_data",
    "buffer_size":32768,
    "flush_interval_ms":120000,
    "fsync_interval_ms":60000,
    "max_lines_per_file":10000,
    "enable_free_space_check":true,
    "min_free_space_mb":10,
    "spi":{"mosi_pin":23,"miso_pin":19,"clk_pin":18,"cs_pin":22,"freq_khz":10000}
  }}
]})";
```

## Performance Considerations

### Buffering and Flush/Fsync Cadence
- A larger buffer_size reduces how often SD writes occur; 32 KB is a good starting point if RAM allows
- flush_interval_ms controls how frequently buffered data is written out
- fsync_interval_ms throttles fsync calls, which are the most expensive operation; syncing every 60 seconds balances durability and latency
- The sink no longer flushes per-N-lines; flushing is time/size based to minimize blocking

### SPI Configuration and Signal Integrity
- Default SD SPI frequency is 10 MHz for improved stability
- The driver config sets stronger drive strength on MOSI/CLK/CS and an internal pull-up on MISO
- Keep wiring as short as practical; ensure solid 3.3 V supply and decoupling near the socket

### Memory Usage
- Buffer size is configurable based on available RAM
- Minimal heap allocation during normal operation
- Thread-safe operations via mutex protection

## Troubleshooting

### Common Issues

1. SD Card Not Detected
   - Check SPI wiring connections
   - Verify SD card is properly formatted (FAT32)
   - Check power supply to SD card

2. Mount Failed
   - Ensure SD card is FAT32 formatted
   - Check for corrupted filesystem
   - Verify SPI pin configuration matches hardware

3. Write Errors or Timeouts
   - Lower SPI frequency (e.g., 8–10 MHz)
   - Increase buffer_size and flush_interval_ms; ensure fsync_interval_ms is not too aggressive
   - Inspect wiring and power integrity; try an industrial-grade SD card

### Debug Logging
Enable debug logging for detailed troubleshooting:
```
esp_log_level_set("SDCardLogSink", ESP_LOG_DEBUG);
```

## File Format

The SD card sink uses the existing CSV serializer, producing files with the following structure:

```csv
timestamp,elapsed_sec,pack_voltage_v,pack_current_a,soc_pct,power_w,...
1710518400,0,12.6,-2.5,85.2,-31.5,...
1710518401,1,12.6,-2.4,85.2,-30.2,...
```

Headers are automatically written to each new file for easy data analysis.
