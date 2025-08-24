# Local OTA Setup with Nginx on NAS

This guide shows you how to set up OTA updates using a local nginx server on your NAS (without HTTPS).

## Quick Setup

### 1. Configure for Local Server

```bash
# Run the local server setup script
./tools/setup_local_ota_server.sh

# Edit the generated config with your NAS IP
nano ota_deploy_config.json
```

Update your NAS IP address in the configuration:

```json
{
  "server": {
    "base_url": "http://192.168.1.100:80",
    "verify_ssl": false
  }
}
```

### 2. Set Up Nginx on Your NAS

Copy the nginx configuration to your NAS:

```bash
# On your NAS, create the OTA directory
sudo mkdir -p /var/www/ota/firmware
sudo chown -R www-data:www-data /var/www/ota

# Copy and enable the nginx configuration
sudo cp nginx_ota_server.conf /etc/nginx/sites-available/ota-server
sudo ln -s /etc/nginx/sites-available/ota-server /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

### 3. Deploy Firmware

```bash
# Build firmware
idf.py build

# Deploy to NAS (manual method)
./deploy_to_nas.sh

# Or use integrated command (requires SCP configuration)
idf.py ota_flash
```

## Detailed Configuration

### Nginx Server Configuration

The provided `nginx_ota_server.conf` creates:

- **Firmware endpoint**: `http://your-nas/firmware/firmware.bin`
- **Version endpoint**: `http://your-nas/firmware/version.json`
- **Status endpoint**: `http://your-nas/status`

### Directory Structure on NAS

```
/var/www/ota/
└── firmware/
    ├── firmware.bin      # ESP32 firmware file
    └── version.json      # Version information
```

### ESP32 Configuration

Update `/spiffs/ota_config.txt` on your ESP32:

```json
{
  "server_url": "http://192.168.1.100/firmware/firmware.bin",
  "skip_cert_verification": true,
  "current_version": "1.0.0"
}
```

## Usage Examples

### Deploy New Firmware

```bash
# Method 1: Direct deployment script
./deploy_to_nas.sh

# Method 2: Using idf.py (requires SSH setup)
idf.py build ota_flash

# Method 3: Manual SCP
scp build/esp32-bms-monitor.bin user@nas:/var/www/ota/firmware/firmware.bin
```

### Test Server Accessibility

```bash
# Test from development machine
curl http://your-nas-ip/firmware/version.json
curl -I http://your-nas-ip/firmware/firmware.bin

# Test from ESP32 network
ping your-nas-ip
telnet your-nas-ip 80
```

## Advantages of Local HTTP Server

✅ **No SSL certificate management**  
✅ **Faster transfers on local network**  
✅ **Simple nginx configuration**  
✅ **No external dependencies**  
✅ **Full control over update server**  

## Security Considerations

⚠️ **For internal networks only** - HTTP is not encrypted  
⚠️ **Use firewall rules** to restrict access to OTA server  
⚠️ **Consider VPN** for remote access to OTA updates  

## Troubleshooting

### ESP32 Cannot Connect to Server

1. **Check IP address**: Ensure ESP32 can ping the NAS
2. **Verify nginx**: Check `sudo systemctl status nginx`
3. **Check firewall**: Ensure port 80 is open
4. **Test manually**: `curl http://nas-ip/firmware/version.json`

### Upload Issues

1. **SSH key setup**: Configure passwordless SSH to NAS
2. **Permissions**: Ensure www-data owns `/var/www/ota`
3. **Disk space**: Check available space on NAS

### Version Detection Issues

1. **Check version.json format**: Must be valid JSON
2. **CORS headers**: Ensure nginx serves proper headers
3. **Cache issues**: Add no-cache headers to version endpoint

## Example Version File

```json
{
  "version": "1.2.0",
  "build_date": "2024-01-15T10:30:00Z",
  "description": "ESP32 BMS Monitor firmware",
  "min_version": "1.0.0",
  "size": 1380448
}
```

## Complete Example Workflow

```bash
# 1. Initial setup (one time)
./tools/setup_local_ota_server.sh
# Configure nginx on NAS using provided config

# 2. Build and deploy
idf.py build
./deploy_to_nas.sh

# 3. Trigger OTA update
mosquitto_pub -h your-nas -t "bms/ota/command" -m '{"action": "update"}'

# 4. Monitor progress
mosquitto_sub -h your-nas -t "bms/ota/status"
```