#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// MQTT connection configuration constants
#define OTA_MQTT_KEEPALIVE_SEC              60
#define OTA_MQTT_TIMEOUT_MS                 5000
#define OTA_MQTT_REFRESH_CONNECTION_MS      1800000  // 30 minutes
#define OTA_MQTT_DISABLE_CLEAN_SESSION      false

// Default MQTT configuration
#define OTA_MQTT_DEFAULT_HOST               "localhost"
#define OTA_MQTT_DEFAULT_PORT               1883
#define OTA_MQTT_DEFAULT_QOS                1
#define OTA_MQTT_DEFAULT_RETAIN             true

// Topic configuration
#define OTA_MQTT_DEFAULT_STATUS_TOPIC       "bms/ota/status"
#define OTA_MQTT_DEFAULT_COMMAND_TOPIC      "bms/ota/command"

// Client ID configuration
#define OTA_MQTT_STATUS_CLIENT_ID           "bms_ota_status"
#define OTA_MQTT_COMMANDS_CLIENT_ID         "bms_ota_commands"

// Buffer sizes
#define OTA_MQTT_HOST_MAX_LEN               128
#define OTA_MQTT_USERNAME_MAX_LEN           64
#define OTA_MQTT_PASSWORD_MAX_LEN           64
#define OTA_MQTT_CLIENT_ID_MAX_LEN          64
#define OTA_MQTT_TOPIC_MAX_LEN              128

// MQTT configuration structure
typedef struct {
    char broker_host[OTA_MQTT_HOST_MAX_LEN];
    int broker_port;
    char username[OTA_MQTT_USERNAME_MAX_LEN];
    char password[OTA_MQTT_PASSWORD_MAX_LEN];
    char client_id[OTA_MQTT_CLIENT_ID_MAX_LEN];
    int qos;
    bool retain;
} ota_mqtt_config_t;

#ifdef __cplusplus
}
#endif