#include "mqtt_manager.h"
#include <SPIFFS.h>

namespace mqtt_manager {

bool load_config(MqttConfig& out)
{
    if (!SPIFFS.begin(true)) {
        Serial.println("MQTT Manager: Failed to mount SPIFFS");
        return false;
    }
    File f = SPIFFS.open("/mqtt_config.txt", "r");
    if (!f) {
        Serial.println("MQTT Manager: /mqtt_config.txt not found; using defaults");
        // Defaults
        out.host = "192.168.1.218";
        out.port = 1883;
        out.topic = "bms/telemetry";
        out.username = "";
        out.password = "";
        out.enabled = true;
        return true;
    }
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line[0] == '#') continue;
        int eq = line.indexOf('=');
        if (eq <= 0) continue;
        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        key.trim(); val.trim();
        if (key == "host") out.host = val;
        else if (key == "port") out.port = (uint16_t) val.toInt();
        else if (key == "topic") out.topic = val;
        else if (key == "username") out.username = val;
        else if (key == "password") out.password = val;
        else if (key == "enabled") out.enabled = (val == "1" || val == "true" || val == "on");
    }
    f.close();
    if (out.host.length() == 0) out.host = "192.168.1.218";
    if (out.topic.length() == 0) out.topic = "bms/telemetry";
    return true;
}

} // namespace mqtt_manager
