#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>

namespace mqtt_manager {
struct MqttConfig {
    String host;
    uint16_t port = 1883;
    String topic;
    String username;
    String password;
    bool enabled = true;
};

bool load_config(MqttConfig& out);

} // namespace mqtt_manager

#endif
