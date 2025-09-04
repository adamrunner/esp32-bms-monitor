#!/bin/bash
# Setup script for local nginx OTA server on NAS/local network

set -e

echo "ESP32 BMS Monitor - Local OTA Server Setup"
echo "=========================================="

# Configuration variables (edit these for your setup)
NAS_IP="${NAS_IP:-192.168.1.100}"
NAS_USER="${NAS_USER:-admin}"
OTA_DIR="${OTA_DIR:-/var/www/ota}"
NGINX_CONF_DIR="${NGINX_CONF_DIR:-/etc/nginx/sites-available}"

echo "Configuration:"
echo "  NAS IP: $NAS_IP"
echo "  NAS User: $NAS_USER"
echo "  OTA Directory: $OTA_DIR"
echo ""

# Check if we're in the right directory
if [ ! -f "nginx_ota_server.conf" ] || [ ! -f "ota_deploy_config.json" ]; then
    echo "Error: Run this script from the project root directory"
    exit 1
fi

# Update local OTA configuration
echo "Updating OTA configuration for local server..."
cat > ota_deploy_config.json << EOF
{
  "server": {
    "base_url": "http://$NAS_IP:80",
    "upload_endpoint": "/firmware/upload",
    "firmware_path": "/firmware/firmware.bin",
    "version_path": "/firmware/version.json",
    "username": "$NAS_USER",
    "password": "",
    "verify_ssl": false
  },
  "mqtt": {
    "broker": "$NAS_IP",
    "port": 1883,
    "username": "",
    "password": "",
    "command_topic": "bms/ota/command",
    "status_topic": "bms/ota/status",
    "timeout": 60
  },
  "build": {
    "firmware_path": "build/esp32-bms-monitor.bin",
    "version_file": "version.txt"
  }
}
EOF

echo "✓ OTA configuration updated"

# Create deployment script
echo "Creating deployment script..."
cat > deploy_to_nas.sh << 'EOF'
#!/bin/bash
# Deploy firmware to local NAS server

# Load configuration
NAS_IP=$(grep -o '"base_url": "http://[^:]*' ota_deploy_config.json | cut -d'/' -f3)
NAS_USER=$(jq -r '.server.username' ota_deploy_config.json 2>/dev/null || grep -A 10 '"server"' ota_deploy_config.json | grep -o '"username": "[^"]*' | head -n 1 | cut -d'"' -f4)

if [ -z "$NAS_IP" ] || [ "$NAS_IP" = "your-nas-ip" ]; then
    echo "Error: Please configure NAS IP in ota_deploy_config.json"
    exit 1
fi

# Check if firmware exists
if [ ! -f "build/esp32-bms-monitor.bin" ]; then
    echo "Error: Firmware not found. Run 'idf.py build' first"
    exit 1
fi

# Get version
VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "1.0.0-dev")
echo "Deploying version: $VERSION"

# Create remote directories
ssh $NAS_USER@$NAS_IP "mkdir -p /var/www/ota/firmware"

# Upload firmware
echo "Uploading firmware..."
scp build/esp32-bms-monitor.bin $NAS_USER@$NAS_IP:/var/www/ota/firmware/firmware.bin

# Create and upload version file
echo "Creating version file..."
cat > /tmp/version.json << EOV
{
  "version": "$VERSION",
  "build_date": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "description": "ESP32 BMS Monitor firmware version $VERSION",
  "min_version": "1.0.0",
  "size": $(stat -f%z build/esp32-bms-monitor.bin 2>/dev/null || stat -c%s build/esp32-bms-monitor.bin)
}
EOV

scp /tmp/version.json $NAS_USER@$NAS_IP:/var/www/ota/firmware/version.json
rm /tmp/version.json

echo "✓ Firmware deployed successfully!"
echo "  Firmware URL: http://$NAS_IP/firmware/firmware.bin"
echo "  Version URL: http://$NAS_IP/firmware/version.json"

# Test accessibility
echo "Testing server accessibility..."
if curl -s "http://$NAS_IP/firmware/version.json" > /dev/null; then
    echo "✓ Server is accessible"
else
    echo "⚠ Warning: Server may not be accessible. Check nginx configuration."
fi
EOF

chmod +x deploy_to_nas.sh
echo "✓ Deployment script created: deploy_to_nas.sh"

echo ""
echo "Next steps:"
echo "1. Set up nginx on your NAS using nginx_ota_server.conf"
echo "2. Create the directory: $OTA_DIR/firmware on your NAS"
echo "3. Set up SSH key authentication to your NAS"
echo "4. Test deployment: ./deploy_to_nas.sh"
echo "5. Use OTA commands: idf.py ota_flash"
echo ""
echo "Manual nginx setup commands for your NAS:"
echo "  sudo mkdir -p $OTA_DIR/firmware"
echo "  sudo chown -R www-data:www-data $OTA_DIR"
echo "  sudo cp nginx_ota_server.conf $NGINX_CONF_DIR/ota-server"
echo "  sudo ln -s $NGINX_CONF_DIR/ota-server /etc/nginx/sites-enabled/"
echo "  sudo nginx -t && sudo systemctl reload nginx"