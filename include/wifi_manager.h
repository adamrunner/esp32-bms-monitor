#pragma once

#include <WiFi.h>

namespace wifi_manager {

struct WiFiConfig {
    String ssid;
    String password;
    unsigned int timeout_ms = 10000;
    unsigned int retry_count = 3;
};

enum class WiFiStatus {
    NOT_INITIALIZED,
    CONNECTING,
    CONNECTED,
    FAILED,
    DISCONNECTED
};

bool initialize();
bool connect();
bool isConnected();
WiFiStatus getStatus();
String getStatusString();
String getLocalIP();
void disconnect();

} // namespace wifi_manager