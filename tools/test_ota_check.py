#!/usr/bin/env python3
"""
Test script for OTA check functionality
"""

import json
import time
import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from ota_deploy import OTADeployer

def test_ota_check():
    """Test OTA check functionality"""
    print("Testing OTA check functionality...")

    # Create deployer instance
    deployer = OTADeployer()

    # Set up MQTT connection
    print("\n1. Connecting to MQTT...")
    if not deployer.setup_mqtt():
        print("✗ MQTT setup failed")
        return False

    print("✓ MQTT connected successfully")

    # Test check for updates
    print("\n2. Checking for updates...")
    result = deployer.check_for_updates()

    if result:
        print("✓ OTA check completed successfully!")
        return True
    else:
        print("✗ OTA check failed!")
        return False

if __name__ == "__main__":
    success = test_ota_check()
    sys.exit(0 if success else 1)
