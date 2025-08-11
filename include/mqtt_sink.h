#ifndef MQTT_SINK_H
#define MQTT_SINK_H

#include <Arduino.h>
#include "log_sink.h"

namespace logging
{
class mqtt_sink : public log_sink
{
public:
    mqtt_sink(const char* host, uint16_t port, const char* topic, bool enabled = true, const char* username = nullptr, const char* password = nullptr);
    void begin() override;
    void tick() override;
    void write(const String& line) override;

    // Diagnostics
    unsigned long reconnect_attempts() const { return reconnect_attempts_; }
    long last_state() const { return last_state_; }
    unsigned long publish_ok() const { return publish_ok_; }
    unsigned long publish_fail() const { return publish_fail_; }
    unsigned long dropped() const { return dropped_; }

private:
    bool ensure_connected();
    const char* host_;
    uint16_t port_;
    const char* topic_;
    bool enabled_;

    // Counters and timing
    unsigned long reconnect_attempts_ {0};
    unsigned long last_connect_ms_ {0};
    long last_state_ {0};
    unsigned long publish_ok_ {0};
    unsigned long publish_fail_ {0};
    unsigned long dropped_ {0};
    const char* username_ {nullptr};
    const char* password_ {nullptr};
};
}

#endif
