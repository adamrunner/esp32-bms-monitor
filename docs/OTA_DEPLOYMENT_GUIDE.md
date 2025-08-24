# ESP32 BMS Monitor - OTA Deployment Guide

This guide provides comprehensive instructions for setting up and using the Over-The-Air (OTA) update functionality in the ESP32 BMS Monitor project.

## Overview

The OTA system enables remote firmware updates through HTTPS downloads, triggered via MQTT commands. It features:

- **Secure HTTPS Downloads**: SSL/TLS encrypted firmware transfers
- **Version Management**: Semantic versioning with anti-rollback protection
- **MQTT Integration**: Command and status reporting via separate topics
- **Rollback Safety**: Automatic rollback on failed updates
- **Progress Monitoring**: Real-time update status via logging system

## Architecture

### Components

1. **OTA Manager** (`components/ota_manager/`)
   - Core OTA functionality using ESP-IDF's `esp_https_ota`
   - Version checking and rollback protection
   - Configuration management via SPIFFS

2. **MQTT Publisher** (`ota_mqtt_publisher.c`)
   - Publishes OTA status to `bms/ota/status` topic
   - JSON formatted status messages

3. **MQTT Command Handler** (`ota_mqtt_commands.c`)
   - Listens for commands on `bms/ota/command` topic
   - Supports update, check, rollback, and status commands

4. **Status Logger** (`ota_status_logger.cpp`)
   - Integrates OTA events with existing logging system
   - Provides progress callbacks and status tracking

### Partition Layout

```
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
otadata,  data, ota,     0x10000, 0x2000,
ota_0,    app,  ota_0,   0x20000, 1536K,
ota_1,    app,  ota_1,   0x1A0000, 1536K,
storage,  data, spiffs,  0x320000, 768K,
```

## Configuration

### 1. OTA Configuration File

Create `/spiffs/ota_config.txt` with the following JSON structure:

```json
{
  "server_url": "https://your-ota-server.com/firmware.bin",
  "cert_pem": "",
  "skip_cert_verification": true,
  "timeout_ms": 60000,
  "current_version": "1.0.0",
  "auto_rollback_enabled": true
}
```

**Configuration Parameters:**

- `server_url`: HTTPS URL for firmware downloads
- `cert_pem`: Server certificate (empty for certificate bundle validation)
- `skip_cert_verification`: Set to `true` for development, `false` for production
- `timeout_ms`: Download timeout in milliseconds
- `current_version`: Current firmware version string
- `auto_rollback_enabled`: Enable automatic rollback on failure

### 2. MQTT Configuration

The OTA system uses the existing MQTT configuration from `/spiffs/mqtt_config.txt`:

```json
{
  "broker_host": "your-mqtt-broker.com",
  "broker_port": 1883,
  "username": "username",
  "password": "password",
  "qos": 1
}
```

## Server Setup

### Option 1: Simple HTTP Server

For development and testing:

```bash
# Create a simple Python HTTP server
cd /path/to/firmware/files
python3 -m http.server 8000 --bind 0.0.0.0

# Serve HTTPS (requires SSL certificates)
python3 -m http.server 8443 --bind 0.0.0.0 --cgi
```

### Option 2: Production HTTPS Server

#### Nginx Configuration

```nginx
server {
    listen 443 ssl;
    server_name your-ota-server.com;
    
    ssl_certificate /path/to/certificate.pem;
    ssl_certificate_key /path/to/private.key;
    
    # Firmware directory
    location /firmware {
        alias /var/www/firmware;
        autoindex on;
        
        # Security headers
        add_header Content-Security-Policy "default-src 'none'";
        add_header X-Content-Type-Options nosniff;
        
        # Large file support for firmware
        client_max_body_size 10M;
    }
    
    # Version endpoint
    location /firmware/version {
        alias /var/www/firmware/version.json;
        add_header Content-Type application/json;
    }
}
```

#### Apache Configuration

```apache
<VirtualHost *:443>
    ServerName your-ota-server.com
    DocumentRoot /var/www/firmware
    
    SSLEngine on
    SSLCertificateFile /path/to/certificate.pem
    SSLCertificateKeyFile /path/to/private.key
    
    # Large file support
    LimitRequestBody 10485760
    
    <Directory "/var/www/firmware">
        Options Indexes
        AllowOverride None
        Require all granted
    </Directory>
</VirtualHost>
```

### Version Management

Create `/var/www/firmware/version.json`:

```json
{
  "version": "1.1.0",
  "build_date": "2024-01-15T10:30:00Z",
  "description": "Bug fixes and performance improvements",
  "min_version": "1.0.0",
  "force_update": false
}
```

## MQTT Commands

### Command Topic: `bms/ota/command`

#### Check for Updates

```json
{
  "action": "check"
}
```

#### Start Update

```json
{
  "action": "update",
  "force": false
}
```

#### Force Update (skip version check)

```json
{
  "action": "update",
  "force": true
}
```

#### Rollback to Previous Version

```json
{
  "action": "rollback"
}
```

#### Get Current Status

```json
{
  "action": "status"
}
```

### Status Topic: `bms/ota/status`

Status messages are published as JSON:

```json
{
  "timestamp_us": 1234567890123456,
  "uptime_sec": 3600,
  "status": 2,
  "progress_pct": 45,
  "message": "Downloading firmware update",
  "current_version": "1.0.0",
  "available_version": "1.1.0",
  "rollback_pending": false,
  "free_heap": 95632
}
```

**Status Codes:**
- `0`: Idle
- `1`: Checking for updates
- `2`: Downloading
- `3`: Installing
- `4`: Success
- `5`: Failed
- `6`: Rollback

## Command Options

### `idf.py ota_flash`
Deploy firmware via OTA update

**Options:**
- `--force` - Force update (skip version check)
- `--config FILE` - Use specific configuration file
- `--version VER` - Override version string

### `idf.py ota_check`
Check for available OTA updates

**Options:**
- `--config FILE` - Use specific configuration file

### `idf.py ota_config`
Configure OTA deployment settings

**Options:**
- `--config FILE` - Specify configuration file path

## Configuration File

Edit `ota_deploy_config.json` to configure:

- **Server settings**: URL, credentials, SSL options
- **MQTT settings**: Broker, credentials, topics

## Deployment Process (Manual)

### 1. Build Firmware

```bash
# Set up ESP-IDF environment
. /Users/adamrunner/esp/v5.5/esp-idf/export.sh

# Build the project
idf.py build

# The firmware binary is located at:
# build/esp32-bms-monitor.bin
```

### 2. Upload to Server

```bash
# Copy firmware to web server
scp build/esp32-bms-monitor.bin user@your-server:/var/www/firmware/firmware.bin

# Update version information
ssh user@your-server "echo '{\"version\": \"1.1.0\"}' > /var/www/firmware/version.json"
```

### 3. Trigger Update

Using MQTT client (e.g., `mosquitto_pub`):

```bash
# Check for updates
mosquitto_pub -h your-mqtt-broker.com -t "bms/ota/command" -m '{"action": "check"}'

# Start update
mosquitto_pub -h your-mqtt-broker.com -t "bms/ota/command" -m '{"action": "update"}'

# Monitor status
mosquitto_sub -h your-mqtt-broker.com -t "bms/ota/status"
```

## Troubleshooting

### Common Issues

1. **SSL Certificate Errors**
   - Set `skip_cert_verification: true` for testing
   - Use proper certificates for production
   - Check certificate bundle includes your CA

2. **Download Timeouts**
   - Increase `timeout_ms` in configuration
   - Check network connectivity
   - Verify server response time

3. **Insufficient Memory**
   - Monitor free heap during updates
   - Close unnecessary connections
   - Optimize buffer sizes

4. **Partition Size Issues**
   - Ensure firmware fits in 1.5MB partition
   - Optimize build size with compiler flags
   - Remove unused components

### Debug Commands

```bash
# Monitor serial output during update
idf.py monitor

# Check partition information
idf.py partition-table

# Verify firmware size
ls -la build/esp32-bms-monitor.bin
```

### MQTT Debug

```bash
# Subscribe to all BMS topics
mosquitto_sub -h your-broker -t "bms/+/+"

# Monitor OTA status continuously
mosquitto_sub -h your-broker -t "bms/ota/status" -v
```

## Security Considerations

### Production Deployment

1. **Use HTTPS with Valid Certificates**
   - Obtain certificates from trusted CA
   - Implement certificate pinning
   - Enable certificate validation

2. **Secure MQTT**
   - Use TLS/SSL encryption
   - Implement authentication
   - Restrict topic access

3. **Firmware Signing**
   - Sign firmware images
   - Verify signatures before installation
   - Use secure boot if required

4. **Access Control**
   - Restrict OTA server access
   - Implement authentication
   - Log all update attempts

### Network Security

1. **Firewall Rules**
   - Limit HTTPS access to necessary IPs
   - Block unnecessary protocols
   - Monitor connection attempts

2. **VPN Access**
   - Use VPN for administrative access
   - Separate OTA network from production
   - Implement network segmentation

## Monitoring and Logging

### Status Monitoring

The OTA system integrates with the existing logging infrastructure:

- **Serial Output**: Debug information via ESP_LOG
- **MQTT Status**: Real-time updates on `bms/ota/status`
- **Local Storage**: Log files (if implemented)

### Metrics to Monitor

- Update success/failure rates
- Download speeds and timeouts
- Memory usage during updates
- Rollback frequency
- Version distribution

## Automation Scripts

### Continuous Deployment

```bash
#!/bin/bash
# deploy-ota.sh

VERSION=$1
if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>"
    exit 1
fi

# Build firmware
idf.py build

# Upload to server
scp build/esp32-bms-monitor.bin user@server:/var/www/firmware/firmware.bin

# Update version
ssh user@server "echo '{\"version\": \"$VERSION\"}' > /var/www/firmware/version.json"

# Trigger updates
mosquitto_pub -h mqtt-broker -t "bms/ota/command" -m "{\"action\": \"check\"}"

echo "Deployment complete for version $VERSION"
```

### Health Check

```bash
#!/bin/bash
# check-ota-health.sh

# Check server availability
curl -s https://your-server/firmware/version || echo "Server unavailable"

# Check MQTT connectivity
timeout 5 mosquitto_sub -h mqtt-broker -t "bms/ota/status" -C 1 || echo "MQTT unavailable"
```

## Conclusion

The OTA system provides robust, secure firmware update capabilities for the ESP32 BMS Monitor. Follow this guide for proper deployment and maintenance of OTA infrastructure.

For additional support or questions, refer to the ESP-IDF OTA documentation or project issues.
