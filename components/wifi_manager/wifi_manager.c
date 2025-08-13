#include "wifi_manager.h"
#include <string.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <esp_spiffs.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MAX_RETRY_DELAY_MS 30000
#define MIN_RETRY_DELAY_MS 1000

static const char *TAG = "wifi_manager";
static EventGroupHandle_t s_wifi_event_group;
static wifi_manager_config_t s_wifi_config = {0};
static wifi_status_t s_wifi_status = {0};
static esp_netif_t *s_sta_netif = NULL;
static bool s_initialized = false;
static uint32_t s_retry_delay_ms = MIN_RETRY_DELAY_MS;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started");
        esp_wifi_connect();
        s_wifi_status.state = WIFI_STATE_CONNECTING;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi disconnected (reason: %d)", disconnected->reason);

        s_wifi_status.state = WIFI_STATE_DISCONNECTED;
        s_wifi_status.ip_address = 0;
        s_wifi_status.disconnect_count++;

        if (s_wifi_status.retry_attempts < s_wifi_config.retry_count) {
            ESP_LOGI(TAG, "Retry connecting to WiFi... (attempt %d/%d)",
                     s_wifi_status.retry_attempts + 1, s_wifi_config.retry_count);

            vTaskDelay(pdMS_TO_TICKS(s_retry_delay_ms));
            esp_wifi_connect();
            s_wifi_status.retry_attempts++;
            s_wifi_status.state = WIFI_STATE_CONNECTING;

            // Exponential backoff with cap
            s_retry_delay_ms = (s_retry_delay_ms * 2 > MAX_RETRY_DELAY_MS) ?
                               MAX_RETRY_DELAY_MS : s_retry_delay_ms * 2;
        } else {
            ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", s_wifi_config.retry_count);
            s_wifi_status.state = WIFI_STATE_FAILED;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));

        s_wifi_status.state = WIFI_STATE_CONNECTED;
        s_wifi_status.ip_address = event->ip_info.ip.addr;
        s_wifi_status.retry_attempts = 0;
        s_wifi_status.connected_time_us = esp_timer_get_time();
        s_retry_delay_ms = MIN_RETRY_DELAY_MS; // Reset backoff delay

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Initialize SPIFFS for config files
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize SPIFFS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS initialized successfully");
    }

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    return ESP_OK;
}

esp_err_t wifi_manager_config_from_file(const char* config_file_path)
{
    if (!config_file_path) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE* file = fopen(config_file_path, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open WiFi config file: %s", config_file_path);
        return ESP_ERR_NOT_FOUND;
    }

    char line[128];
    memset(&s_wifi_config, 0, sizeof(s_wifi_config));

    // Set defaults
    s_wifi_config.timeout_ms = 10000;
    s_wifi_config.retry_count = 3;

    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "ssid=", 5) == 0) {
            strncpy(s_wifi_config.ssid, line + 5, sizeof(s_wifi_config.ssid) - 1);
        } else if (strncmp(line, "password=", 9) == 0) {
            strncpy(s_wifi_config.password, line + 9, sizeof(s_wifi_config.password) - 1);
        } else if (strncmp(line, "timeout_ms=", 11) == 0) {
            s_wifi_config.timeout_ms = strtoul(line + 11, NULL, 10);
        } else if (strncmp(line, "retry_count=", 12) == 0) {
            s_wifi_config.retry_count = strtoul(line + 12, NULL, 10);
        }
    }

    fclose(file);

    if (strlen(s_wifi_config.ssid) == 0) {
        ESP_LOGE(TAG, "SSID not found in config file");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "WiFi config loaded: SSID=%s, timeout=%lu ms, retry_count=%d",
             s_wifi_config.ssid, s_wifi_config.timeout_ms, s_wifi_config.retry_count);

    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (strlen(s_wifi_config.ssid) == 0) {
        ESP_LOGE(TAG, "WiFi configuration not loaded");
        return ESP_ERR_INVALID_STATE;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strncpy((char*)wifi_config.sta.ssid, s_wifi_config.ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, s_wifi_config.password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi connecting to SSID: %s", s_wifi_config.ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(s_wifi_config.timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID: %s", s_wifi_config.ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi SSID: %s", s_wifi_config.ssid);
        return ESP_ERR_WIFI_CONN;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_manager_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_ERROR_CHECK(esp_wifi_stop());
    s_wifi_status.state = WIFI_STATE_DISCONNECTED;
    s_wifi_status.ip_address = 0;

    ESP_LOGI(TAG, "WiFi stopped");
    return ESP_OK;
}

esp_err_t wifi_manager_get_status(wifi_status_t* status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(status, &s_wifi_status, sizeof(wifi_status_t));

    // Update RSSI if connected
    if (s_wifi_status.state == WIFI_STATE_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            status->rssi = ap_info.rssi;
        }
    }

    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_wifi_status.state == WIFI_STATE_CONNECTED;
}

const char* wifi_manager_get_state_string(wifi_state_t state)
{
    switch (state) {
        case WIFI_STATE_DISCONNECTED: return "DISCONNECTED";
        case WIFI_STATE_CONNECTING: return "CONNECTING";
        case WIFI_STATE_CONNECTED: return "CONNECTED";
        case WIFI_STATE_FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}