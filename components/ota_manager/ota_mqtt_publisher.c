#include "ota_mqtt_publisher.h"
#include "ota_mqtt_config.h"
#include "ota_status.h"
#include <esp_log.h>
#include <mqtt_client.h>
#include <cJSON.h>
#include <string.h>
#include <esp_timer.h>

static const char *TAG = "ota_mqtt_publisher";

// Global MQTT client for OTA status
static esp_mqtt_client_handle_t g_ota_mqtt_client = NULL;
static bool g_ota_mqtt_connected = false;
static bool g_ota_mqtt_initialized = false;
static char g_ota_topic[OTA_MQTT_TOPIC_MAX_LEN] = OTA_MQTT_DEFAULT_STATUS_TOPIC;

static ota_mqtt_config_t g_mqtt_config = {
    .broker_host = OTA_MQTT_DEFAULT_HOST,
    .broker_port = OTA_MQTT_DEFAULT_PORT,
    .username = "",
    .password = "",
    .client_id = OTA_MQTT_STATUS_CLIENT_ID,
    .qos = OTA_MQTT_DEFAULT_QOS,
    .retain = OTA_MQTT_DEFAULT_RETAIN
};

// Forward declarations
static void ota_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static bool load_mqtt_config_from_spiffs(void);
static char* ota_status_to_json(const ota_status_snapshot_t* status);

esp_err_t ota_mqtt_publisher_init(const char* topic)
{
    if (g_ota_mqtt_initialized) {
        ESP_LOGW(TAG, "OTA MQTT publisher already initialized");
        return ESP_OK;
    }

    if (topic && strlen(topic) > 0) {
        strncpy(g_ota_topic, topic, sizeof(g_ota_topic) - 1);
        g_ota_topic[sizeof(g_ota_topic) - 1] = '\0';
    }

    // Load MQTT configuration from existing MQTT config file
    if (!load_mqtt_config_from_spiffs()) {
        ESP_LOGW(TAG, "Failed to load MQTT config, using defaults");
    }

    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {0};
    mqtt_cfg.broker.address.uri = NULL;
    mqtt_cfg.broker.address.hostname = g_mqtt_config.broker_host;
    mqtt_cfg.broker.address.port = g_mqtt_config.broker_port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mqtt_cfg.credentials.client_id = g_mqtt_config.client_id;

    if (strlen(g_mqtt_config.username) > 0) {
        mqtt_cfg.credentials.username = g_mqtt_config.username;
        mqtt_cfg.credentials.authentication.password = g_mqtt_config.password;
    }

    mqtt_cfg.session.keepalive = OTA_MQTT_KEEPALIVE_SEC;
    mqtt_cfg.session.disable_clean_session = OTA_MQTT_DISABLE_CLEAN_SESSION;
    mqtt_cfg.network.timeout_ms = OTA_MQTT_TIMEOUT_MS;
    mqtt_cfg.network.refresh_connection_after_ms = OTA_MQTT_REFRESH_CONNECTION_MS;

    // Create MQTT client
    g_ota_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!g_ota_mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    // Register event handler
    esp_err_t ret = esp_mqtt_client_register_event(g_ota_mqtt_client, ESP_EVENT_ANY_ID, ota_mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(g_ota_mqtt_client);
        g_ota_mqtt_client = NULL;
        return ret;
    }

    // Start MQTT client
    ret = esp_mqtt_client_start(g_ota_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(g_ota_mqtt_client);
        g_ota_mqtt_client = NULL;
        return ret;
    }

    g_ota_mqtt_initialized = true;
    ESP_LOGI(TAG, "OTA MQTT publisher initialized successfully");
    ESP_LOGI(TAG, "Publishing OTA status to topic: %s", g_ota_topic);

    return ESP_OK;
}

esp_err_t ota_mqtt_publisher_send_status(const ota_status_snapshot_t* status)
{
    if (!g_ota_mqtt_initialized || !g_ota_mqtt_client || !status) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!g_ota_mqtt_connected) {
        ESP_LOGD(TAG, "MQTT not connected, skipping OTA status publish");
        return ESP_ERR_INVALID_STATE;
    }

    // Convert status to JSON
    char* json_payload = ota_status_to_json(status);
    if (!json_payload) {
        ESP_LOGE(TAG, "Failed to serialize OTA status to JSON");
        return ESP_ERR_NO_MEM;
    }

    // Publish to MQTT
    int msg_id = esp_mqtt_client_publish(g_ota_mqtt_client, g_ota_topic, json_payload, 0, g_mqtt_config.qos, g_mqtt_config.retain);

    // Clean up JSON payload
    free(json_payload);

    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish OTA status to MQTT");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "OTA status published to MQTT (msg_id: %d)", msg_id);
    return ESP_OK;
}

void ota_mqtt_publisher_shutdown(void)
{
    if (g_ota_mqtt_client) {
        esp_mqtt_client_stop(g_ota_mqtt_client);
        esp_mqtt_client_destroy(g_ota_mqtt_client);
        g_ota_mqtt_client = NULL;
    }

    g_ota_mqtt_connected = false;
    g_ota_mqtt_initialized = false;
    ESP_LOGI(TAG, "OTA MQTT publisher shutdown");
}

bool ota_mqtt_publisher_is_connected(void)
{
    return g_ota_mqtt_connected;
}

static void ota_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "OTA MQTT connected");
        g_ota_mqtt_connected = true;
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "OTA MQTT disconnected");
        g_ota_mqtt_connected = false;
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "OTA status published, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "OTA MQTT error occurred");
        g_ota_mqtt_connected = false;
        break;

    default:
        ESP_LOGD(TAG, "OTA MQTT event: %d", event_id);
        break;
    }
}

static bool load_mqtt_config_from_spiffs(void)
{
    FILE *file = fopen("/spiffs/mqtt_config.txt", "r");
    if (!file) {
        ESP_LOGD(TAG, "MQTT config file not found, using defaults");
        return false;
    }

    char line[256];
    bool config_loaded = false;

    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        line[strcspn(line, "\r")] = 0;

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

            // Parse configuration values
            if (strcmp(key, "host") == 0) {
                strncpy(g_mqtt_config.broker_host, value, sizeof(g_mqtt_config.broker_host) - 1);
                g_mqtt_config.broker_host[sizeof(g_mqtt_config.broker_host) - 1] = '\0';
                config_loaded = true;
            }
            else if (strcmp(key, "port") == 0) {
                int port = atoi(value);
                if (port > 0 && port <= 65535) {
                    g_mqtt_config.broker_port = port;
                }
            }
            else if (strcmp(key, "username") == 0) {
                strncpy(g_mqtt_config.username, value, sizeof(g_mqtt_config.username) - 1);
                g_mqtt_config.username[sizeof(g_mqtt_config.username) - 1] = '\0';
            }
            else if (strcmp(key, "password") == 0) {
                strncpy(g_mqtt_config.password, value, sizeof(g_mqtt_config.password) - 1);
                g_mqtt_config.password[sizeof(g_mqtt_config.password) - 1] = '\0';
            }
            else if (strcmp(key, "qos") == 0) {
                int qos = atoi(value);
                if (qos >= 0 && qos <= 2) {
                    g_mqtt_config.qos = qos;
                }
            }
        }
    }

    fclose(file);

    if (config_loaded) {
        ESP_LOGI(TAG, "MQTT configuration loaded for OTA status publisher: %s:%d",
                 g_mqtt_config.broker_host, g_mqtt_config.broker_port);
    } else {
        ESP_LOGW(TAG, "No valid MQTT configuration found, using defaults");
    }

    return config_loaded;
}

static char* ota_status_to_json(const ota_status_snapshot_t* status)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return NULL;
    }

    cJSON_AddNumberToObject(json, "timestamp_us", status->timestamp_us);
    cJSON_AddNumberToObject(json, "uptime_sec", status->uptime_sec);
    cJSON_AddNumberToObject(json, "status", status->status);
    cJSON_AddNumberToObject(json, "progress_pct", status->progress_pct);
    cJSON_AddStringToObject(json, "message", status->message);
    cJSON_AddStringToObject(json, "current_version", status->current_version);
    cJSON_AddStringToObject(json, "available_version", status->available_version);
    cJSON_AddBoolToObject(json, "rollback_pending", status->rollback_pending);
    cJSON_AddNumberToObject(json, "free_heap", status->free_heap);

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    return json_string;
}
