# ESP32 BMS Monitor - OTA Quick Start

This guide shows you how to use the integrated OTA (Over-The-Air) update commands in ESP-IDF.

## Prerequisites

1. ESP-IDF environment set up
2. Project built successfully
3. OTA server configured (see full deployment guide)

## Setup (One-time)

```bash
# Set up ESP-IDF environment
. ~/esp/v5.5/esp-idf/export.sh  # Adjust path as needed

# Install Python dependencies and create config
./tools/setup_ota.sh

# Edit OTA configuration
idf.py ota_config
# Then edit ota_deploy_config.json with your server/MQTT settings
```

## Usage

### Deploy New Firmware

```bash
# Build and deploy in one command
idf.py build ota_flash

# Or deploy already-built firmware
idf.py ota_flash

# Force deployment (skip version checking)
idf.py ota_flash --force
```

### Check for Updates

```bash
# Check if updates are available without deploying
idf.py ota_check
```

### View Configuration

```bash
# Show current OTA configuration
idf.py ota_config
```

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
- **Build settings**: Firmware path, version file

## Troubleshooting

1. **Command not found**: Ensure you're in the project root directory
2. **Python errors**: Run `pip install -r requirements.txt`
3. **Upload fails**: Check server credentials in config file
4. **MQTT errors**: Verify broker settings and connectivity

## Full Documentation

For complete setup instructions, server configuration, and troubleshooting, see:
- [OTA_DEPLOYMENT_GUIDE.md](OTA_DEPLOYMENT_GUIDE.md)

## Example Workflow

```bash
# 1. Build firmware
idf.py build

# 2. Deploy to development devices
idf.py ota_flash --force

# 3. Deploy to production devices (with version check)
idf.py ota_flash

# 4. Monitor deployment status via MQTT topic: bms/ota/status
```