# SD Card Logging Implementation

## Overview

This document describes the SD Card logging functionality added to the ESP32 BMS monitor system. The implementation provides persistent local storage of BMS telemetry data in CSV format with advanced features like file rotation, buffering, and error handling.

## Features

### Core Functionality
- **SPI Interface**: Uses 1-bit SPI interface for SD card communication
- **CSV Format**: Stores data in CSV format compatible with existing serializers
- **Buffered Writes**: Configurable buffer size (default 10KB) with timed flush intervals
- **File Rotation**: Daily rotation with line-count fallback (default 10,000 lines)
- **Timestamped Files**: Automatic filename generation with date stamps
- **CSV Headers**: Automatic CSV header writing for each new file

### Advanced Features
- **Free Space Monitoring**: Configurable minimum free space checking
- **Error Handling**: Graceful handling of SD card removal/insertion
- **Thread Safety**: Mutex-protected buffer operations
- **Configuration**: Full JSON configuration support
- **Statistics**: File and write statistics tracking

## Configuration

### JSON Configuration Format

```json
{
  "type": "sdcard",
  "config": {
    "mount_point": "/sdcard",
    "file_prefix": "bms_data",
    "file_extension": ".csv",
    "buffer_size": 8192,
    "flush_interval_ms": 30000,
    "max_lines_per_file": 10000,
    "enable_free_space_check": true,
    "min_free_space_mb": 10,
    "spi": {
      "mosi_pin": 23,
      "miso_pin": 19,
      "clk_pin": 18,
      "cs_pin": 5,
      "freq_khz": 20000
    }
  }
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `mount_point` | string | "/sdcard" | SD card mount point |
| `file_prefix` | string | "bms" | Prefix for log files |
| `file_extension` | string | ".csv" | File extension |
| `buffer_size` | number | 10240 | Write buffer size in bytes |
| `flush_interval_ms` | number | 30000 | Buffer flush interval in milliseconds |
| `max_lines_per_file` | number | 10000 | Maximum lines per file before rotation |
| `enable_free_space_check` | boolean | true | Enable free space monitoring |
| `min_free_space_mb` | number | 10 | Minimum free space in MB |

### SPI Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `mosi_pin` | number | 23 | SPI MOSI pin |
| `miso_pin` | number | 19 | SPI MISO pin |
| `clk_pin` | number | 18 | SPI clock pin |
| `cs_pin` | number | 5 | SPI chip select pin |
| `freq_khz` | number | 20000 | SPI frequency in kHz |

## File Naming Convention

Files are automatically named using the following pattern:
- `{prefix}_{YYYYMMDD}.csv` - Daily files
- `{prefix}_{YYYYMMDD}_001.csv` - Sequential files for same day
- `{prefix}_uptime_{seconds}.csv` - Fallback when no time sync

Examples:
- `bms_data_20240315.csv`
- `bms_data_20240315_001.csv`
- `bms_data_uptime_3600.csv`

## File Rotation

### Daily Rotation
- Files rotate automatically at midnight
- New file created with updated date stamp
- Previous file is closed and flushed

### Line Count Rotation
- Fallback rotation when file exceeds `max_lines_per_file`
- Sequential numbering for same-day files
- Prevents excessively large files

## Error Handling

### SD Card States
- `UNINITIALIZED` - Initial state
- `INITIALIZING` - During initialization
- `READY` - Normal operation
- `ERROR_NO_CARD` - SD card not detected
- `ERROR_MOUNT_FAILED` - Filesystem mount failed
- `ERROR_DISK_FULL` - Insufficient free space
- `ERROR_IO_FAILURE` - I/O operation failed

### Error Recovery
- Automatic retry on transient errors
- Graceful degradation when SD card unavailable
- Detailed error logging for troubleshooting

## Integration

### Build System
The SD card sink is automatically included when `INCLUDE_SDCARD_SINK=1` is defined in the build system.

### Dependencies
- `fatfs` - FAT filesystem support
- `sdmmc` - SD/MMC card driver
- `spi_flash` - SPI flash operations

### Usage Example

```cpp
// In main.cpp
std::string logging_config = R"({"sinks":[
    {"type":"serial","config":{"format":"csv","print_header":true}},
    {"type":"sdcard","config":{"file_prefix":"bms_data","buffer_size":8192}}
]})";

if (!LOG_INIT(logging_config)) {
    ESP_LOGE(TAG, "Failed to initialize logging system");
}
```

## Performance Considerations

### Buffering
- Default 10KB buffer reduces SD card write frequency
- Configurable flush intervals balance data safety and performance
- Automatic flush on buffer full or time interval

### SPI Configuration
- Default 20MHz SPI frequency for good performance
- 1-bit SPI mode for simple wiring
- Configurable pins for flexible hardware design

### Memory Usage
- Buffer size configurable based on available RAM
- Minimal heap allocation during operation
- Thread-safe operations with mutex protection

## Troubleshooting

### Common Issues

1. **SD Card Not Detected**
   - Check SPI wiring connections
   - Verify SD card is properly formatted (FAT32)
   - Check power supply to SD card

2. **Mount Failed**
   - Ensure SD card is FAT32 formatted
   - Check for corrupted filesystem
   - Verify SPI pin configuration

3. **Write Errors**
   - Check available free space
   - Verify SD card is not write-protected
   - Monitor for SD card removal during operation

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
