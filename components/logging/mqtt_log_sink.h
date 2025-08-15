#ifndef MQTT_LOG_SINK_H
#define MQTT_LOG_SINK_H

#include "log_sink.h"
#include "log_serializers.h"

// ESP-IDF includes
#include <mqtt_client.h>

namespace logging {

/**
 * MQTT log sink for publishing BMS data to MQTT brokers
 * Supports different QoS levels, authentication, and wildcard topics
 */
class MQTTLogSink : public LogSink {
public:
    MQTTLogSink();
    ~MQTTLogSink() override;

    bool init(const std::string& config) override;
    bool send(const output::BMSSnapshot& data) override;
    void shutdown() override;
    const char* getName() const override;
    bool isReady() const override;

private:
    BMSSerializer* serializer_;
    esp_mqtt_client_handle_t mqtt_client_;
    bool initialized_;
    bool connected_;

    // Configuration
    struct Config {
        std::string broker_host = "localhost";
        int broker_port = 1883;
        std::string topic = "bms/telemetry";
        std::string format = "csv";
        int qos = 0;
        bool retain = false;
        std::string username = "";
        std::string password = "";
        std::string client_id = "bms_mqtt_client";
        int keep_alive = 60;
        bool clean_session = true;
        int connect_timeout_ms = 5000;
    } config_;

    bool parseConfig(const std::string& config_str);
    bool loadSpiffsConfig();
    bool connectMQTT();
    void disconnectMQTT();
    void mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);

    // Stats
    size_t messages_published_;
    size_t bytes_published_;
    size_t connection_failures_;
};

} // namespace logging
#endif // MQTT_LOG_SINK_H