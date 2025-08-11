#include "wifi_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>

namespace wifi_manager {

static WiFiConfig config;
static WiFiStatus current_status = WiFiStatus::NOT_INITIALIZED;

static bool loadConfig() {
    if (!SPIFFS.begin(true)) {
        Serial.println("WiFi Manager: Failed to mount SPIFFS");
        return false;
    }

    File file = SPIFFS.open("/wifi_config.txt", "r");
    if (!file) {
        Serial.println("WiFi Manager: Config file not found");
        return false;
    }

    String line;
    while (file.available()) {
        line = file.readStringUntil('\n');
        line.trim();
        
        if (line.startsWith("ssid=")) {
            config.ssid = line.substring(5);
        } else if (line.startsWith("password=")) {
            config.password = line.substring(9);
        } else if (line.startsWith("timeout_ms=")) {
            config.timeout_ms = line.substring(11).toInt();
        } else if (line.startsWith("retry_count=")) {
            config.retry_count = line.substring(12).toInt();
        }
    }
    file.close();

    if (config.ssid.length() == 0) {
        Serial.println("WiFi Manager: No SSID found in config");
        return false;
    }

    Serial.printf("WiFi Manager: Config loaded - SSID: %s\n", config.ssid.c_str());
    return true;
}

bool initialize() {
    if (!loadConfig()) {
        current_status = WiFiStatus::FAILED;
        return false;
    }

    WiFi.mode(WIFI_STA);
    current_status = WiFiStatus::NOT_INITIALIZED;
    return true;
}

bool connect() {
    if (current_status == WiFiStatus::CONNECTED) {
        return true;
    }

    if (config.ssid.length() == 0) {
        Serial.println("WiFi Manager: Not initialized");
        current_status = WiFiStatus::FAILED;
        return false;
    }

    for (unsigned int attempt = 0; attempt < config.retry_count; attempt++) {
        Serial.printf("WiFi Manager: Connecting to %s (attempt %d/%d)...\n", 
                      config.ssid.c_str(), attempt + 1, config.retry_count);
        
        current_status = WiFiStatus::CONNECTING;
        WiFi.begin(config.ssid.c_str(), config.password.c_str());

        unsigned long start_time = millis();
        while (WiFi.status() != WL_CONNECTED && 
               (millis() - start_time) < config.timeout_ms) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            current_status = WiFiStatus::CONNECTED;
            Serial.printf("WiFi Manager: Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }

        Serial.printf("WiFi Manager: Connection attempt %d failed\n", attempt + 1);
        delay(1000);
    }

    current_status = WiFiStatus::FAILED;
    Serial.println("WiFi Manager: All connection attempts failed");
    return false;
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED && current_status == WiFiStatus::CONNECTED;
}

WiFiStatus getStatus() {
    if (current_status == WiFiStatus::CONNECTED && WiFi.status() != WL_CONNECTED) {
        current_status = WiFiStatus::DISCONNECTED;
    }
    return current_status;
}

String getStatusString() {
    switch (getStatus()) {
        case WiFiStatus::NOT_INITIALIZED: return "Not Initialized";
        case WiFiStatus::CONNECTING: return "Connecting";
        case WiFiStatus::CONNECTED: return "Connected";
        case WiFiStatus::FAILED: return "Failed";
        case WiFiStatus::DISCONNECTED: return "Disconnected";
        default: return "Unknown";
    }
}

String getLocalIP() {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

void disconnect() {
    WiFi.disconnect();
    current_status = WiFiStatus::DISCONNECTED;
    Serial.println("WiFi Manager: Disconnected");
}

} // namespace wifi_manager