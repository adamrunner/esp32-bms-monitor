#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "mqtt_sink.h"

namespace {
WiFiClient g_wifi_client;
PubSubClient g_mqtt(g_wifi_client);
}

namespace logging {

mqtt_sink::mqtt_sink(const char* host, uint16_t port, const char* topic, bool enabled, const char* username, const char* password)
: port_(port), enabled_(enabled)
{
    if (host) host_s_ = host;
    if (topic) topic_s_ = topic;
    if (username) username_s_ = username;
    if (password) password_s_ = password;
}

void mqtt_sink::begin()
{
    if (!enabled_) return;
    g_mqtt.setServer(host_s_.c_str(), port_);
    g_mqtt.setSocketTimeout(2); // seconds, keep operations short
    Serial.printf("[MQTT] config host=%s port=%u topic=%s enabled=%d user=%s\n",
                  host_s_.c_str(), (unsigned)port_, topic_s_.c_str(), enabled_ ? 1 : 0,
                  (username_s_.length() ? "set" : "none"));
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

    // WiFi and DNS info
    wl_status_t wst = WiFi.status();
    int32_t rssi = WiFi.RSSI();
    IPAddress resolved;
    bool dns_ok = WiFi.hostByName(host_s_.c_str(), resolved);
    if (!dns_ok) {
        Serial.printf("[MQTT] DNS failed for host=%s (WiFi=%d RSSI=%d)\n", host_s_.c_str(), (int)wst, (int)rssi);
        last_state_ = -2; // network fail
        return false;
    }

    Serial.printf("[MQTT] connecting to %s (%s):%u user=%s WiFi=%d RSSI=%d t=%lu\n",
                  host_s_.c_str(), resolved.toString().c_str(), (unsigned)port_,
                  (username_s_.length() ? "set" : "none"), (int)wst, (int)rssi, now);

    String client_id = String("esp32-bms-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    unsigned long t0 = millis();
    bool ok;
    if (username_s_.length()) {
        ok = g_mqtt.connect(client_id.c_str(), username_s_.c_str(), password_s_.c_str());
    } else {
        ok = g_mqtt.connect(client_id.c_str());
    }
    unsigned long dt = millis() - t0;
    last_state_ = g_mqtt.state();
    Serial.printf("[MQTT] connect result ok=%d state=%ld elapsed=%lums\n", ok ? 1 : 0, last_state_, dt);
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
    if (g_mqtt.publish(topic_s_.c_str(), line.c_str(), false)) {
        publish_ok_++;
    } else {
        publish_fail_++;
        last_state_ = g_mqtt.state();
    }
}

} // namespace logging
