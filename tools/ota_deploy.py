#!/usr/bin/env python3
"""
OTA Deployment Tool for ESP32 BMS Monitor

This script handles uploading firmware to the OTA server via SCP and triggering
remote updates via MQTT commands.
"""

import argparse
import json
import os
import sys
import time
import subprocess
import tempfile
from pathlib import Path
from typing import Optional, Dict, Any
from urllib.parse import urlparse

try:
    import paho.mqtt.client as mqtt
    MQTT_AVAILABLE = True
except ImportError:
    MQTT_AVAILABLE = False
    print("Warning: paho-mqtt not installed. Install with: pip install paho-mqtt")

class OTADeployer:
    def __init__(self, config_file: str = "ota_deploy_config.json", cli_version: str = None):
        self.config = self.load_config(config_file)
        self.mqtt_client = None
        self.mqtt_connected = False
        self.cli_version = cli_version

    def load_config(self, config_file: str) -> Dict[str, Any]:
        """Load deployment configuration"""
        config_path = Path(config_file)

        # Default configuration
        default_config = {
            "server": {
                "base_url": "http://your-nas-ip",
                "firmware_path": "/var/www/firmware/firmware.bin",
                "version_path": "/var/www/firmware/version.json",
                "username": "",
                "password": "",
                "verify_ssl": False
            },
            "mqtt": {
                "broker": "localhost",
                "port": 1883,
                "username": "",
                "password": "",
                "command_topic": "bms/ota/command",
                "status_topic": "bms/ota/status",
                "timeout": 30
            },
            "build": {
                "firmware_path": "build/esp32-bms-monitor.bin"
            }
        }

        if not config_path.exists():
            print(f"Creating default config file: {config_file}")
            with open(config_file, 'w') as f:
                json.dump(default_config, f, indent=2)
            print(f"Please edit {config_file} with your server and MQTT details")
            return default_config

        with open(config_file, 'r') as f:
            config = json.load(f)

        # Merge with defaults to ensure all keys exist
        def merge_configs(default, user):
            result = default.copy()
            for key, value in user.items():
                if key in result and isinstance(result[key], dict) and isinstance(value, dict):
                    result[key] = merge_configs(result[key], value)
                else:
                    result[key] = value
            return result

        return merge_configs(default_config, config)

    def get_version(self) -> str:
        """Get version from CLI args or generate from git"""
        # 1. Use CLI version if provided
        if self.cli_version:
            return self.cli_version

        # 2. Try to get version from git
        try:
            # Get branch name
            branch_result = subprocess.run(
                ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
                capture_output=True,
                text=True,
                check=True
            )
            branch = branch_result.stdout.strip()

            # Get short commit hash
            commit_result = subprocess.run(
                ['git', 'rev-parse', '--short', 'HEAD'],
                capture_output=True,
                text=True,
                check=True
            )
            short_commit = commit_result.stdout.strip()

            version = f"{branch}-{short_commit}"

            # Check for uncommitted changes
            status_result = subprocess.run(
                ['git', 'status', '--porcelain', '--untracked-files=no'],
                capture_output=True,
                text=True,
                check=True
            )
            if status_result.stdout.strip():
                version += "-dirty"

            return version
        except subprocess.CalledProcessError:
            # 3. Fallback version
            return "1.0.0-dev"

    def upload_firmware(self, firmware_path: str, version: str) -> bool:
        """Upload firmware to OTA server via SCP"""
        server_config = self.config["server"]

        if not os.path.exists(firmware_path):
            print(f"Error: Firmware file not found: {firmware_path}")
            return False

        file_size = os.path.getsize(firmware_path)
        print(f"Uploading firmware: {firmware_path} ({file_size} bytes)")

        # Upload via SCP
        return self.upload_via_scp(firmware_path, version)

    def upload_via_scp(self, firmware_path: str, version: str) -> bool:
        """Upload firmware and version file via SCP"""
        server_config = self.config["server"]

        # Extract host from URL
        parsed = urlparse(server_config["base_url"])
        host = parsed.hostname

        if not host:
            print("Error: Could not extract hostname from server URL")
            return False

        username = server_config["username"]
        if not username:
            print("Error: Username not configured in server settings")
            return False

        # Extract remote paths from config
        firmware_remote_path = server_config["firmware_path"]
        version_remote_path = server_config["version_path"]

        # Get directory path for remote creation
        remote_dir = os.path.dirname(firmware_remote_path)

        # Create remote directories using SSH
        print(f"Creating remote directories on {host}...")
        ssh_cmd = [
            'ssh',
            f"{username}@{host}",
            f"mkdir -p {remote_dir}"
        ]

        try:
            subprocess.run(ssh_cmd, check=True, capture_output=True, text=True)
            print("✓ Remote directories created successfully")
        except subprocess.CalledProcessError as e:
            print(f"Failed to create remote directories: {e}")
            return False

        # Upload firmware via SCP
        print("Uploading firmware...")
        scp_firmware_cmd = [
            'scp',
            firmware_path,
            f"{username}@{host}:{firmware_remote_path}"
        ]

        try:
            subprocess.run(scp_firmware_cmd, check=True, capture_output=True, text=True)
            print("✓ Firmware uploaded successfully")
        except subprocess.CalledProcessError as e:
            print(f"Firmware upload failed: {e}")
            return False

        # Create and upload version file
        print("Creating and uploading version file...")
        return self.create_and_upload_version_file(version, host, username, version_remote_path)

    def create_and_upload_version_file(self, version: str, host: str, username: str, version_remote_path: str) -> bool:
        """Create version.json file locally and upload via SCP"""
        # Create version info
        try:
            # Get firmware file size
            firmware_path = self.config["build"]["firmware_path"]
            file_size = os.path.getsize(firmware_path) if os.path.exists(firmware_path) else 0

            version_info = {
                "version": version,
                "build_date": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "description": f"ESP32 BMS Monitor firmware version {version}",
                "min_version": "1.0.0",
                "size": file_size
            }

            # Create temporary version file
            with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
                json.dump(version_info, f, indent=2)
                temp_version_file = f.name

            # Upload version file via SCP
            scp_version_cmd = [
                'scp',
                temp_version_file,
                f"{username}@{host}:{version_remote_path}"
            ]

            result = subprocess.run(scp_version_cmd, check=True, capture_output=True, text=True)
            print("✓ Version file uploaded successfully")

            # Clean up temporary file
            os.unlink(temp_version_file)

            return True

        except subprocess.CalledProcessError as e:
            print(f"Version file upload failed: {e}")
            return False
        except Exception as e:
            print(f"Error creating or uploading version file: {e}")
            return False

    def setup_mqtt(self) -> bool:
        """Set up MQTT connection"""
        if not MQTT_AVAILABLE:
            print("Error: paho-mqtt not available. Cannot trigger OTA via MQTT.")
            return False

        mqtt_config = self.config["mqtt"]

        def on_connect(client, userdata, flags, rc):
            if rc == 0:
                self.mqtt_connected = True
                print(f"✓ Connected to MQTT broker: {mqtt_config['broker']}")
                client.subscribe(mqtt_config["status_topic"])
            else:
                print(f"MQTT connection failed with code {rc}")

        def on_message(client, userdata, msg):
            try:
                status = json.loads(msg.payload.decode())
                print(f"OTA Status: {status.get('message', 'Unknown')} ({status.get('progress_pct', 0)}%)")

                # Check for completion during update process
                if status.get('status') == 4:  # SUCCESS (OTA_STATUS_SUCCESS = 4)
                    print("✓ OTA update completed successfully!")
                elif status.get('status') == 5:  # FAILED (OTA_STATUS_FAILED = 5)
                    print("✗ OTA update failed!")

            except json.JSONDecodeError:
                print(f"Received non-JSON message: {msg.payload}")

        self.mqtt_client = mqtt.Client()
        self.mqtt_client.on_connect = on_connect
        self.mqtt_client.on_message = on_message

        if mqtt_config["username"] and mqtt_config["password"]:
            self.mqtt_client.username_pw_set(
                mqtt_config["username"],
                mqtt_config["password"]
            )

        try:
            self.mqtt_client.connect(mqtt_config["broker"], mqtt_config["port"], 60)
            self.mqtt_client.loop_start()

            # Wait for connection with improved timeout handling
            start_time = time.time()
            timeout = max(10, mqtt_config.get("timeout", 30) // 2)  # Wait up to half of configured timeout
            while not self.mqtt_connected and (time.time() - start_time) < timeout:
                time.sleep(0.1)

            if not self.mqtt_connected:
                print(f"MQTT connection timeout after {timeout} seconds")
                self.mqtt_client.loop_stop()
                self.mqtt_client.disconnect()
                return False

            return True

        except Exception as e:
            print(f"MQTT connection failed: {e}")
            if self.mqtt_client:
                self.mqtt_client.loop_stop()
                self.mqtt_client.disconnect()
            return False

    def trigger_ota_update(self, force: bool = False) -> bool:
        """Trigger OTA update via MQTT"""
        if not self.mqtt_client or not self.mqtt_connected:
            print("Error: MQTT not connected")
            return False

        mqtt_config = self.config["mqtt"]
        command = {
            "action": "update",
            "force": force
        }

        print(f"Sending OTA update command (force: {force})...")

        try:
            result = self.mqtt_client.publish(
                mqtt_config["command_topic"],
                json.dumps(command),
                qos=1
            )

            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print("✓ OTA update command sent")

                # Wait for completion or timeout
                start_time = time.time()
                while self.mqtt_connected and (time.time() - start_time) < mqtt_config["timeout"]:
                    time.sleep(1)

                return True
            else:
                print(f"Failed to send MQTT command: {result.rc}")

        except Exception as e:
            print(f"MQTT publish failed: {e}")

        return False

    def check_for_updates(self) -> bool:
        """Check for available updates"""
        if not self.mqtt_client or not self.mqtt_connected:
            print("Error: MQTT not connected")
            return False

        mqtt_config = self.config["mqtt"]
        command = {"action": "check"}

        print("Checking for available updates...")

        # Set up a flag to track if we received a relevant status update
        update_check_complete = False
        update_available = False
        status_message = None

        def on_check_message(client, userdata, msg):
            nonlocal update_check_complete, update_available, status_message
            try:
                status = json.loads(msg.payload.decode())
                status_message = f"OTA Status: {status.get('message', 'Unknown')} ({status.get('progress_pct', 0)}%)"

                # Check if this status message contains update information
                available_version = status.get('available_version', '')
                message = status.get('message', '').lower()

                if available_version and available_version.strip():
                    # Update is available
                    print(f"✓ Update available: {available_version}")
                    print(status_message)
                    update_available = True
                    update_check_complete = True
                elif 'no update' in message or 'up to date' in message:
                    # No update available
                    print("No update available")
                    print(status_message)
                    update_check_complete = True
                elif status.get('status') == 0 and 'idle' in message:
                    # Device is idle after check
                    print("Update check completed - no update available")
                    print(status_message)
                    update_check_complete = True

            except json.JSONDecodeError:
                print(f"Received non-JSON message: {msg.payload}")

        # Temporarily replace the message handler to capture check responses
        original_on_message = self.mqtt_client.on_message
        self.mqtt_client.on_message = on_check_message

        try:
            result = self.mqtt_client.publish(
                mqtt_config["command_topic"],
                json.dumps(command),
                qos=1
            )

            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print("✓ Update check command sent")

                # Wait for response with timeout
                start_time = time.time()
                while not update_check_complete and (time.time() - start_time) < 10:
                    time.sleep(0.1)

                # Restore original message handler
                self.mqtt_client.on_message = original_on_message

                if update_check_complete:
                    return True
                else:
                    print("No update status received from device within timeout period")
                    # Restore original message handler before returning
                    self.mqtt_client.on_message = original_on_message
                    return False
            else:
                print(f"Failed to send MQTT command: {result.rc}")
                self.mqtt_client.on_message = original_on_message

        except Exception as e:
            print(f"MQTT publish failed: {e}")
            self.mqtt_client.on_message = original_on_message

        return False

    def deploy(self, force_update: bool = False, check_only: bool = False) -> bool:
        """Complete deployment process"""
        # Get version
        version = self.get_version()
        print(f"Deploying version: {version}")

        # Check firmware exists
        firmware_path = self.config["build"]["firmware_path"]
        if not os.path.exists(firmware_path):
            print(f"Error: Firmware not found at {firmware_path}")
            print("Run 'idf.py build' first")
            return False

        # Upload firmware
        print("\n1. Uploading firmware...")
        if not self.upload_firmware(firmware_path, version):
            print("✗ Firmware upload failed")
            return False

        # Set up MQTT
        print("\n2. Connecting to MQTT...")
        if not self.setup_mqtt():
            print("✗ MQTT setup failed")
            return False

        # Check or trigger update
        result = False
        if check_only:
            print("\n3. Checking for updates...")
            result = self.check_for_updates()
        else:
            print("\n3. Triggering OTA update...")
            result = self.trigger_ota_update(force_update)

        return result

def main():
    parser = argparse.ArgumentParser(description="ESP32 BMS Monitor OTA Deployment Tool")
    parser.add_argument("--config", default="ota_deploy_config.json",
                       help="Configuration file path")
    parser.add_argument("--force", action="store_true",
                       help="Force update (skip version check)")
    parser.add_argument("--check-only", action="store_true",
                       help="Only check for updates, don't trigger")
    parser.add_argument("--version", help="Override version string")

    args = parser.parse_args()

    # Pass version override to constructor
    deployer = OTADeployer(args.config, args.version)

    success = deployer.deploy(args.force, args.check_only)

    if success:
        print("\n✓ Deployment completed successfully!")
        return 0
    else:
        print("\n✗ Deployment failed!")
        return 1

if __name__ == "__main__":
    sys.exit(main())
