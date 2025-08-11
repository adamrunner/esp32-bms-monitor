#ifndef MQTT_SINK_H
#define MQTT_SINK_H

#include <Arduino.h>
#include "log_sink.h"

namespace logging
{
class mqtt_sink : public log_sink
{
public:
    mqtt_sink(const char* host, uint16_t port, const char* topic, bool enabled = true);
    void begin() override;
    void tick() override;
    void write(const String& line) override;
private:
    bool ensure_connected();
    const char* host_;
    uint16_t port_;
    const char* topic_;
    bool enabled_;
};
}

#endif
