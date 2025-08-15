# Modular Logging System

This component provides a flexible, modular logging system that replaces the basic output functionality with support for multiple destinations.

## Overview

The logging system uses a `LogSink` interface pattern with the following components:

- **LogSink**: Base interface for log destinations
- **LogManager**: Singleton that coordinates multiple sinks
- **BMSSerializer**: Interface for different serialization formats
- **Various adapters**: For serial, UDP, TCP, MQTT, HTTP

## Configuration Examples

### Basic Serial Logging (CSV Format)
```json
{
  "sinks": [
    {
      "type": "serial",
      "config": {
        "format": "csv",
        "print_header": true,
        "max_cells": 16,
        "max_temps": 8
      }
    }
  ]
}
```

### Multiple Sinks (Serial + UDP Broadcast)
```json
{
  "sinks": [
    {
      "type": "serial",
      "config": {
        "format": "human"
      }
    },
    {
      "type": "udp",
      "config": {
        "ip": "192.168.1.255",
        "port": 3330,
        "format": "json"
      }
    }
  ]
}
```

### TCP Client to Remote Server
```json
{
  "sinks": [
    {
      "type": "tcp",
      "config": {
        "mode": "client",
        "host": "192.168.1.100",
        "port": 3331,
        "format": "json",
        "auto_reconnect": true
      }
    }
  ]
}
```

### MQTT Publish
```json
{
  "sinks": [
    {
      "type": "mqtt",
      "config": {
        "broker_host": "192.168.1.50",
        "broker_port": 1883,
        "topic": "bms/data",
        "format": "json",
        "qos": 1,
        "username": "user",
        "password": "pass"
      }
    }
  ]
}
```

### HTTP POST with Authentication
```json
{
  "sinks": [
    {
      "type": "http",
      "config": {
        "url": "https://api.example.com/bms/data",
        "method": "POST",
        "format": "json",
        "auth_token": "Bearer xxxxxx",
        "timeout_ms": 5000
      }
    }
  ]
}
```

## Serial Sink Options

The serial sink supports three formats:

1. **human**: Human-readable multi-line format (default)
2. **csv**: CSV format with headers
3. **json**: JSON format

Configuration:
- `format`: One of "human", "csv", "json"
- `print_header`: true/false (CSV format)
- `max_cells`: Maximum cells to include
- `max_temps`: Maximum temperature sensors

Example:
```
format=csv,print_header=true,max_cells=16,max_temps=8
```

## UDP Sink Options

Configuration:
- `ip`: Destination IP (default: 255.255.255.255 for broadcast)
- `port`: Destination port (default: 3330)
- `broadcast`: true/false for broadcast mode
- `format`: format type
- `max_packet_size`: Maximum UDP packet size

## TCP Sink Options

Supports client and server modes:
- `mode`: "client" or "server"
- `host`: Server host (client mode)
- `port`: Port number
- `auto_reconnect`: true/false for client mode
- `max_connections`: For server mode

## Programmatic Usage

```cpp
#include "log_manager.h"

// Initialize logging system
std::string config = R"({"sinks":[{"type":"serial","config":{"format":"csv"}}]})";
LOG_INIT(config);

// Send data
output::BMSSnapshot data = /* ... create snapshot ... */;
LOG_SEND(data);

// Add/remove sinks dynamically
LogManager::getInstance().addSink("udp", "ip=192.168.1.255,port=3330,format=json");
LogManager::getInstance().removeSink("tcp");

// Shutdown
LOG_SHUTDOWN();
```

## Build Integration

Add to your project CMakeLists.txt:

```cmake
# Add logging subdirectory
add_subdirectory(logging)

# Link to your main target
target_link_libraries(your_app logging)