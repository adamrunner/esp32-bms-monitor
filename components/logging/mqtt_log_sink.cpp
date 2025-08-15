#include "mqtt_log_sink.h"
#include <sstream>

// ESP-IDF includes
#include <esp_log.h>
#include <mqtt_client.h>
#include <cJSON.h>
#include <esp_spiffs.h>
#include <esp_system.h>

using namespace logging;

static const char* TAG = "MQTT_LOG_SINK";

MQTTLogSink::MQTTLogSink() :
    serializer_(nullptr),
    mqtt_client_(nullptr),
    initialized_(false),
    connected_(false),
    messages_published_(0),
    bytes_published_(0),
    connection_failures_(0)
{
    setLastError("");
}

MQTTLogSink::~MQTTLogSink() {
    shutdown();
}

bool MQTTLogSink::init(const std::string& config) {
    // First load SPIFFS configuration if available
    loadSpiffsConfig();

    // Parse configuration (can override SPIFFS settings)
    if (!parseConfig(config)) {
        setLastError("Failed to parse configuration");
        return false;
    }

    // Create appropriate serializer
    serializer_ = BMSSerializer::createSerializer(config_.format);

    if (!serializer_) {
        setLastError("Failed to create serializer for format: " + config_.format);
        return false;
    }

    // Initialize MQTT client
    if (!connectMQTT()) {
        setLastError("Failed to connect to MQTT broker");
        return false;
    }

    initialized_ = true;
    return true;
}

bool MQTTLogSink::send(const output::BMSSnapshot& data) {
    if (!initialized_ || !isReady()) {
        setLastError("MQTT sink not ready");
        return false;
    }

    std::string serialized;
    if (!serializer_->serialize(data, serialized)) {
        setLastError("Failed to serialize data");
        return false;
    }

    // Publish message
    int msg_id = esp_mqtt_client_publish(mqtt_client_,
                                       config_.topic.c_str(),
                                       serialized.c_str(),
                                       serialized.length(),
                                       config_.qos,
                                       config_.retain);

    if (msg_id == -1) {
        setLastError("Failed to publish MQTT message");
        return false;
    }

    messages_published_++;
    bytes_published_ += serialized.length();

    ESP_LOGD(TAG, "Published MQTT message (ID: %d, %zu bytes) to topic: %s",
             msg_id, serialized.length(), config_.topic.c_str());

    return true;
}

void MQTTLogSink::shutdown() {
    if (mqtt_client_) {
        disconnectMQTT();
    }

    serializer_.reset();
    initialized_ = false;
    connected_ = false;
}

const char* MQTTLogSink::getName() const {
    return "mqtt";
}

bool MQTTLogSink::isReady() const {
    return initialized_ && connected_;
}

bool MQTTLogSink::parseConfig(const std::string& config_str) {
    cJSON *json = cJSON_Parse(config_str.c_str());
    if (json) {
        // JSON format parsing
        cJSON *broker_host = cJSON_GetObjectItemCaseSensitive(json, "broker_host");
        if (cJSON_IsString(broker_host)) {
            config_.broker_host = std::string(broker_host->valuestring);
        }

        cJSON *broker_port = cJSON_GetObjectItemCaseSensitive(json, "broker_port");
        if (cJSON_IsNumber(broker_port)) {
            config_.broker_port = broker_port->valueint;
            // Validate port range
            if (config_.broker_port < 1 || config_.broker_port > 65535) {
                setLastError("Invalid broker port: must be between 1-65535");
                cJSON_Delete(json);
                return false;
            }
        }

        cJSON *topic = cJSON_GetObjectItemCaseSensitive(json, "topic");
        if (cJSON_IsString(topic)) {
            config_.topic = std::string(topic->valuestring);
        }

        cJSON *format = cJSON_GetObjectItemCaseSensitive(json, "format");
        if (cJSON_IsString(format)) {
            config_.format = std::string(format->valuestring);
        }

        cJSON *qos = cJSON_GetObjectItemCaseSensitive(json, "qos");
        if (cJSON_IsNumber(qos)) {
            config_.qos = qos->valueint;
            // Validate QoS range
            if (config_.qos < 0 || config_.qos > 2) {
                setLastError("Invalid QoS value: must be between 0-2");
                cJSON_Delete(json);
                return false;
            }
        }

        cJSON *retain = cJSON_GetObjectItemCaseSensitive(json, "retain");
        if (cJSON_IsBool(retain)) {
            config_.retain = cJSON_IsTrue(retain);
        }

        cJSON *username = cJSON_GetObjectItemCaseSensitive(json, "username");
        if (cJSON_IsString(username)) {
            config_.username = std::string(username->valuestring);
        }

        cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
        if (cJSON_IsString(password)) {
            config_.password = std::string(password->valuestring);
        }

        cJSON *client_id = cJSON_GetObjectItemCaseSensitive(json, "client_id");
        if (cJSON_IsString(client_id)) {
            config_.client_id = std::string(client_id->valuestring);
        }

        cJSON *keep_alive = cJSON_GetObjectItemCaseSensitive(json, "keep_alive");
        if (cJSON_IsNumber(keep_alive)) {
            config_.keep_alive = keep_alive->valueint;
        }

        cJSON *clean_session = cJSON_GetObjectItemCaseSensitive(json, "clean_session");
        if (cJSON_IsBool(clean_session)) {
            config_.clean_session = cJSON_IsTrue(clean_session);
        }

        cJSON *connect_timeout = cJSON_GetObjectItemCaseSensitive(json, "connect_timeout_ms");
        if (cJSON_IsNumber(connect_timeout)) {
            config_.connect_timeout_ms = connect_timeout->valueint;
        }

        cJSON_Delete(json);
        return true;
    } else {
        // Fallback to key=value format
        std::string config = config_str;
        config += ",";

        size_t start = 0;
        size_t pos = config.find('=');

        while (pos != std::string::npos) {
            size_t next_comma = config.find(',', pos);
            size_t prev_comma = config.rfind(',', pos-1);

            std::string key = config.substr(prev_comma+1, pos-prev_comma-1);
            std::string value = config.substr(pos+1, next_comma-pos-1);

            // Trim whitespace
            auto first_non_space = key.find_first_not_of(" \t\r\n");
            auto last_non_space = key.find_last_not_of(" \t\r\n");
            if (first_non_space != std::string::npos) {
                key = key.substr(first_non_space, last_non_space - first_non_space + 1);
            }

            first_non_space = value.find_first_not_of(" \t\r\n");
            last_non_space = value.find_last_not_of(" \t\r\n");
            if (first_non_space != std::string::npos) {
                value = value.substr(first_non_space, last_non_space - first_non_space + 1);
            }

            // Strip quotes if present
            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length()-2);
            }

            if (key == "broker_host") config_.broker_host = value;
            else if (key == "broker_port") {
                config_.broker_port = atoi(value.c_str());
                // Validate port range
                if (config_.broker_port < 1 || config_.broker_port > 65535) {
                    setLastError("Invalid broker port: must be between 1-65535");
                    return false;
                }
            }
            else if (key == "topic") config_.topic = value;
            else if (key == "format") config_.format = value;
            else if (key == "qos") {
                config_.qos = atoi(value.c_str());
                // Validate QoS range
                if (config_.qos < 0 || config_.qos > 2) {
                    setLastError("Invalid QoS value: must be between 0-2");
                    return false;
                }
            }
            else if (key == "retain") config_.retain = (value == "true");
            else if (key == "username") config_.username = value;
            else if (key == "password") config_.password = value;
            else if (key == "client_id") config_.client_id = value;
            else if (key == "keep_alive") config_.keep_alive = atoi(value.c_str());
            else if (key == "clean_session") config_.clean_session = (value == "true");
            else if (key == "connect_timeout_ms") config_.connect_timeout_ms = atoi(value.c_str());

            start = next_comma + 1;
            pos = config.find('=', start);
            if (next_comma+1 >= config.length()) break;
        }

        return true;
    }
}

bool MQTTLogSink::loadSpiffsConfig() {
    // Mount SPIFFS if not already mounted
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS already mounted or mount failed (0x%x)", ret);
    }

    FILE* config_file = fopen("/spiffs/mqtt_config.txt", "r");
    if (!config_file) {
        ESP_LOGW(TAG, "MQTT config file not found in SPIFFS");
        return false;
    }

    char line[256];
    while (fgets(line, sizeof(line), config_file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        char* eq_pos = strchr(line, '=');
        if (eq_pos) {
            *eq_pos = '\0'; // Split string
            char* key = line;
            char* value = eq_pos + 1;

            // Trim whitespace
            while (*key && (*key == ' ' || *key == '\t')) key++;
            char* key_end = key + strlen(key) - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t')) key_end--;
            *(key_end + 1) = '\0';

            while (*value && (*value == ' ' || *value == '\t')) value++;
            char* value_end = value + strlen(value) - 1;
            while (value_end > value && (*value_end == ' ' || *value_end == '\t')) value_end--;
            *(value_end + 1) = '\0';

            if (strcmp(key, "host") == 0) config_.broker_host = value;
            else if (strcmp(key, "port") == 0) {
                config_.broker_port = atoi(value);
                // Validate port range
                if (config_.broker_port < 1 || config_.broker_port > 65535) {
                    ESP_LOGE(TAG, "Invalid broker port in SPIFFS config: must be between 1-65535");
                    fclose(config_file);
                    return false;
                }
            }
            else if (strcmp(key, "topic") == 0) config_.topic = value;
            else if (strcmp(key, "username") == 0) config_.username = value;
            else if (strcmp(key, "password") == 0) config_.password = value;
            else if (strcmp(key, "qos") == 0) {
                config_.qos = atoi(value);
                // Validate QoS range
                if (config_.qos < 0 || config_.qos > 2) {
                    ESP_LOGE(TAG, "Invalid QoS value in SPIFFS config: must be between 0-2");
                    fclose(config_file);
                    return false;
                }
            }
            else if (strcmp(key, "enabled") == 0) {
                // Note: This doesn't affect initialization, just for info
            }
        }
    }

    fclose(config_file);
    ESP_LOGI(TAG, "Loaded MQTT config from SPIFFS: %s:%d",
             config_.broker_host.c_str(), config_.broker_port);
    return true;
}

bool MQTTLogSink::connectMQTT() {
    esp_mqtt_client_config_t mqtt_config = {};
    mqtt_config.broker.address.hostname = config_.broker_host.c_str();
    mqtt_config.broker.address.port = config_.broker_port;
    mqtt_config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;

    if (!config_.username.empty()) {
        mqtt_config.credentials.username = config_.username.c_str();
    }
    if (!config_.password.empty()) {
        mqtt_config.credentials.authentication.password = config_.password.c_str();
    }
    if (!config_.client_id.empty()) {
        mqtt_config.credentials.client_id = config_.client_id.c_str();
    }

    mqtt_config.session.keepalive = config_.keep_alive;
    mqtt_config.session.disable_clean_session = !config_.clean_session;
    mqtt_config.network.timeout_ms = config_.connect_timeout_ms;

    mqtt_client_ = esp_mqtt_client_init(&mqtt_config);
    if (!mqtt_client_) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return false;
    }

    // Register event handler
    esp_mqtt_client_register_event(mqtt_client_,
                                  MQTT_EVENT_ANY,
                                  [](void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
                                      MQTTLogSink* sink = static_cast<MQTTLogSink*>(handler_args);
                                      sink->mqttEventHandler(handler_args, base, event_id, event_data);
                                  },
                                  this);

    esp_err_t ret = esp_mqtt_client_start(mqtt_client_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: 0x%x", ret);
        return false;
    }

    // Wait for connection (with timeout)
    int timeout_ms = config_.connect_timeout_ms;
    int wait_time = 0;
    while (!connected_ && wait_time < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_time += 100;
    }

    if (!connected_) {
        connection_failures_++;
        ESP_LOGE(TAG, "MQTT connection timeout");
        return false;
    }

    ESP_LOGI(TAG, "Connected to MQTT broker: %s:%d",
             config_.broker_host.c_str(), config_.broker_port);
    return true;
}

void MQTTLogSink::disconnectMQTT() {
    if (mqtt_client_) {
        esp_mqtt_client_stop(mqtt_client_);
        esp_mqtt_client_destroy(mqtt_client_);
        mqtt_client_ = nullptr;
    }
    connected_ = false;
}

void MQTTLogSink::mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            connected_ = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            connected_ = false;
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error occurred");
            connected_ = false;
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT message published (msg_id=%d)", event->msg_id);
            break;

        default:
            break;
    }
}
