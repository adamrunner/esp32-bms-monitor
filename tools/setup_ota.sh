#!/bin/bash
# Setup script for ESP32 BMS Monitor OTA deployment

set -e

echo "ESP32 BMS Monitor - OTA Setup"
echo "=============================="

# Check if we're in the right directory
if [ ! -f "idf_ext.py" ] || [ ! -f "tools/ota_deploy.py" ]; then
    echo "Error: Run this script from the project root directory"
    exit 1
fi

# Install Python dependencies
echo "Installing Python dependencies..."
if command -v pip3 &> /dev/null; then
    pip3 install -r requirements.txt
elif command -v pip &> /dev/null; then
    pip install -r requirements.txt
else
    echo "Warning: pip not found. Please install Python dependencies manually:"
    echo "  pip install requests paho-mqtt"
fi

# Create OTA configuration if it doesn't exist
if [ ! -f "ota_deploy_config.json" ]; then
    echo "Creating default OTA configuration..."
    python3 tools/ota_deploy.py --config ota_deploy_config.json --check-only 2>/dev/null || true
fi

# Make scripts executable
chmod +x tools/ota_deploy.py
chmod +x tools/setup_ota.sh

echo ""
echo "✓ OTA deployment setup complete!"
echo ""
echo "Next steps:"
echo "1. Edit 'ota_deploy_config.json' with your server and MQTT settings"
echo "2. Build your project: idf.py build"
echo "3. Deploy via OTA: idf.py ota_flash"
echo ""
echo "Available OTA commands:"
echo "  idf.py ota_config  - Configure OTA settings"
echo "  idf.py ota_check   - Check for updates"
echo "  idf.py ota_flash   - Deploy firmware via OTA"
echo "  idf.py ota_flash --force  - Force deployment"
echo ""
echo "For more information, see: OTA_DEPLOYMENT_GUIDE.md"