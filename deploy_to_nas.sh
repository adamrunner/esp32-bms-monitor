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
BRANCH=$(git rev-parse --abbrev-ref HEAD)
SHORT_COMMIT=$(git rev-parse --short HEAD)
VERSION="${BRANCH}-${SHORT_COMMIT}"

if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    VERSION="${VERSION}-dirty"
fi
echo "Deploying version: $VERSION"
echo "$NAS_USER@$NAS_IP mkdir -p /var/www/firmware"
# Create remote directories
ssh $NAS_USER@$NAS_IP "mkdir -p /var/www/firmware"

# Upload firmware
echo "Uploading firmware..."
scp build/esp32-bms-monitor.bin $NAS_USER@$NAS_IP:/var/www/firmware/firmware.bin

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

scp /tmp/version.json $NAS_USER@$NAS_IP:/var/www/firmware/version.json
rm /tmp/version.json

echo "✓ Firmware deployed successfully!"
echo "  Firmware URL: http://$NAS_IP/firmware/firmware.bin"
echo "  Version URL: http://$NAS_IP/firmware/version"

# Test accessibility
echo "Testing server accessibility..."
if curl -s "http://$NAS_IP/firmware/version" > /dev/null; then
    echo "✓ Server is accessible"
else
    echo "⚠ Warning: Server may not be accessible. Check nginx configuration."
fi
