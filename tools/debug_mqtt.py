#!/usr/bin/env python3
"""
Debug MQTT messages for OTA functionality
"""

import paho.mqtt.client as mqtt
import json
import time
import sys

# Configuration (should match your ota_deploy_config.json)
MQTT_BROKER = "anton.local"
MQTT_PORT = 1883
MQTT_USERNAME = "admin"
MQTT_PASSWORD = "password1234"
COMMAND_TOPIC = "bms/ota/command"
STATUS_TOPIC = "bms/ota/status"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✓ Connected to MQTT broker: {MQTT_BROKER}")
        client.subscribe(STATUS_TOPIC)
        print(f"Subscribed to status topic: {STATUS_TOPIC}")
    else:
        print(f"MQTT connection failed with code {rc}")

def on_message(client, userdata, msg):
    print(f"Received message on topic {msg.topic}:")
    try:
        payload = json.loads(msg.payload.decode())
        print(f"  {json.dumps(payload, indent=2)}")
    except json.JSONDecodeError:
        print(f"  {msg.payload.decode()}")

def main():
    print("Debugging MQTT messages...")

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    if MQTT_USERNAME and MQTT_PASSWORD:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()

        # Wait a bit to see any initial messages
        print("Listening for messages for 5 seconds...")
        time.sleep(5)

        # Send a check command
        print(f"Sending check command to {COMMAND_TOPIC}...")
        check_command = {"action": "check"}
        result = client.publish(COMMAND_TOPIC, json.dumps(check_command), qos=1)
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print("✓ Check command sent")
        else:
            print(f"Failed to send command: {result.rc}")

        # Wait longer to see responses
        print("Waiting 10 seconds for responses...")
        time.sleep(10)

        client.loop_stop()
        client.disconnect()

    except Exception as e:
        print(f"Error: {e}")
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
