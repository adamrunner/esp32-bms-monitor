#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "mqtt_sink.h"

namespace {
WiFiClient g_wifi_client;
PubSubClient g_mqtt(g_wifi_client);
}

namespace logging {

mqtt_sink::mqtt_sink(const char* host, uint16_t port, const char* topic, bool enabled)
: host_(host), port_(port), topic_(topic), enabled_(enabled) {}

void mqtt_sink::begin()
{
    if (!enabled_) return;
    g_mqtt.setServer(host_, port_);
    g_mqtt.setSocketTimeout(2); // seconds, keep operations short
}

bool mqtt_sink::ensure_connected()
{
    if (!enabled_) return false;
    if (g_mqtt.connected()) return true;

    const unsigned long now = millis();
    // Only attempt every 5 seconds to avoid blocking loop
    if (now - last_connect_ms_ < 5000) {
        return false;
    }
    last_connect_ms_ = now;
    reconnect_attempts_++;

    String client_id = String("esp32-bms-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    bool ok = g_mqtt.connect(client_id.c_str());
    last_state_ = g_mqtt.state();
    return ok;
}

void mqtt_sink::tick()
{
    if (!enabled_) return;
    // Try a quick connect if backoff allows, then process network
    (void)ensure_connected();
    g_mqtt.loop();
}

void mqtt_sink::write(const String& line)
{
    if (!enabled_) return;
    if (!g_mqtt.connected()) {
        dropped_++;
        return; // never block waiting to connect
    }
    // Publish and record result; do not retry synchronously
    if (g_mqtt.publish(topic_, line.c_str(), false)) {
        publish_ok_++;
    } else {
        publish_fail_++;
        last_state_ = g_mqtt.state();
    }
}

} // namespace logging
