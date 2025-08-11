#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "mqtt_sink.h"

namespace {
WiFiClient g_wifi_client;
PubSubClient g_mqtt(g_wifi_client);
unsigned long last_reconnect_attempt = 0;
}

namespace logging {

mqtt_sink::mqtt_sink(const char* host, uint16_t port, const char* topic, bool enabled)
: host_(host), port_(port), topic_(topic), enabled_(enabled) {}

void mqtt_sink::begin()
{
    if (!enabled_) return;
    g_mqtt.setServer(host_, port_);
}

bool mqtt_sink::ensure_connected()
{
    if (!enabled_) return false;
    if (g_mqtt.connected()) return true;

    unsigned long now = millis();
    if (now - last_reconnect_attempt < 2000) {
        return false;
    }
    last_reconnect_attempt = now;

    String client_id = String("esp32-bms-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (g_mqtt.connect(client_id.c_str())) {
        return true;
    }
    return false;
}

void mqtt_sink::tick()
{
    if (!enabled_) return;
    ensure_connected();
    g_mqtt.loop();
}

void mqtt_sink::write(const String& line)
{
    if (!enabled_) return;
    if (!ensure_connected()) return;
    g_mqtt.publish(topic_, line.c_str(), false);
}

} // namespace logging
