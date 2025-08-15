#include "wifi_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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
#include <freertos/semphr.h>
#include <nvs_flash.h>
#include <nvs.h>

// NVS keys for secure credential storage
#define NVS_NAMESPACE "wifi_creds"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"
#define NVS_KEY_TIMEOUT "timeout"
#define NVS_KEY_RETRY "retry"
#define NVS_KEY_PMF_REQUIRED "pmf_req"

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
static SemaphoreHandle_t s_wifi_mutex = NULL;
static esp_event_handler_instance_t s_wifi_event_instance;
static esp_event_handler_instance_t s_ip_event_instance;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started");
        esp_wifi_connect();
        if (xSemaphoreTake(s_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_wifi_status.state = WIFI_STATE_CONNECTING;
            xSemaphoreGive(s_wifi_mutex);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi disconnected (reason: %d)", disconnected->reason);

        if (xSemaphoreTake(s_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_wifi_status.state = WIFI_STATE_DISCONNECTED;
            s_wifi_status.ip_address = 0;
            s_wifi_status.disconnect_count++;

            if (s_wifi_status.retry_attempts < s_wifi_config.retry_count) {
            ESP_LOGI(TAG, "Retry connecting to WiFi... (attempt %d/%d)",
                     s_wifi_status.retry_attempts + 1, s_wifi_config.retry_count);

            s_wifi_status.retry_attempts++;
            s_wifi_status.state = WIFI_STATE_CONNECTING;
            
            // Exponential backoff with cap
            s_retry_delay_ms = (s_retry_delay_ms * 2 > MAX_RETRY_DELAY_MS) ?
                               MAX_RETRY_DELAY_MS : s_retry_delay_ms * 2;
                               
            xSemaphoreGive(s_wifi_mutex);
            vTaskDelay(pdMS_TO_TICKS(s_retry_delay_ms));
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", s_wifi_config.retry_count);
            s_wifi_status.state = WIFI_STATE_FAILED;
            xSemaphoreGive(s_wifi_mutex);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));

        if (xSemaphoreTake(s_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_wifi_status.state = WIFI_STATE_CONNECTED;
            s_wifi_status.ip_address = event->ip_info.ip.addr;
            s_wifi_status.retry_attempts = 0;
            s_wifi_status.connected_time_us = esp_timer_get_time();
            s_retry_delay_ms = MIN_RETRY_DELAY_MS; // Reset backoff delay
            xSemaphoreGive(s_wifi_mutex);
        }

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

    s_wifi_mutex = xSemaphoreCreateMutex();
    if (!s_wifi_mutex) {
        ESP_LOGE(TAG, "Failed to create WiFi mutex");
        vEventGroupDelete(s_wifi_event_group);
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &s_wifi_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &s_ip_event_instance));

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

    char line[WIFI_CONFIG_LINE_MAX_LEN];
    memset(&s_wifi_config, 0, sizeof(s_wifi_config));

    // Set defaults
    s_wifi_config.timeout_ms = WIFI_DEFAULT_TIMEOUT_MS;
    s_wifi_config.retry_count = WIFI_DEFAULT_RETRY_COUNT;
    s_wifi_config.pmf_required = false;  // Default to optional PMF for compatibility

    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "ssid=", 5) == 0) {
            size_t ssid_len = strlen(line + 5);
            if (ssid_len < WIFI_SSID_MAX_LEN) {
                strcpy(s_wifi_config.ssid, line + 5);
            } else {
                ESP_LOGW(TAG, "SSID too long, truncating");
                strncpy(s_wifi_config.ssid, line + 5, WIFI_SSID_MAX_LEN - 1);
                s_wifi_config.ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
            }
        } else if (strncmp(line, "password=", 9) == 0) {
            size_t password_len = strlen(line + 9);
            if (password_len < WIFI_PASSWORD_MAX_LEN) {
                strcpy(s_wifi_config.password, line + 9);
            } else {
                ESP_LOGW(TAG, "Password too long, truncating");
                strncpy(s_wifi_config.password, line + 9, WIFI_PASSWORD_MAX_LEN - 1);
                s_wifi_config.password[WIFI_PASSWORD_MAX_LEN - 1] = '\0';
            }
        } else if (strncmp(line, "timeout_ms=", 11) == 0) {
            uint32_t timeout = strtoul(line + 11, NULL, 10);
            if (timeout >= WIFI_MIN_TIMEOUT_MS && timeout <= WIFI_MAX_TIMEOUT_MS) {
                s_wifi_config.timeout_ms = timeout;
            } else {
                ESP_LOGW(TAG, "Invalid timeout %lu ms, using default %d ms", timeout, WIFI_DEFAULT_TIMEOUT_MS);
                s_wifi_config.timeout_ms = WIFI_DEFAULT_TIMEOUT_MS;
            }
        } else if (strncmp(line, "retry_count=", 12) == 0) {
            uint8_t retry = strtoul(line + 12, NULL, 10);
            if (retry >= WIFI_MIN_RETRY_COUNT && retry <= WIFI_MAX_RETRY_COUNT) {
                s_wifi_config.retry_count = retry;
            } else {
                ESP_LOGW(TAG, "Invalid retry count %d, using default %d", retry, WIFI_DEFAULT_RETRY_COUNT);
                s_wifi_config.retry_count = WIFI_DEFAULT_RETRY_COUNT;
            }
        } else if (strncmp(line, "pmf_required=", 13) == 0) {
            s_wifi_config.pmf_required = (strncmp(line + 13, "true", 4) == 0) || (strncmp(line + 13, "1", 1) == 0);
        }
    }

    fclose(file);

    if (strlen(s_wifi_config.ssid) == 0) {
        ESP_LOGE(TAG, "SSID not found in config file");
        fclose(file);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "WiFi config loaded: SSID=%s, timeout=%lu ms, retry_count=%d, PMF=%s",
             s_wifi_config.ssid, s_wifi_config.timeout_ms, s_wifi_config.retry_count,
             s_wifi_config.pmf_required ? "required" : "optional");

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
                .required = s_wifi_config.pmf_required  // Configurable PMF requirement
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

    // Take mutex to ensure thread safety
    if (xSemaphoreTake(s_wifi_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_ERROR_CHECK(esp_wifi_stop());
        s_wifi_status.state = WIFI_STATE_DISCONNECTED;
        s_wifi_status.ip_address = 0;
        
        // Clear sensitive data from memory
        memset(s_wifi_config.password, 0, sizeof(s_wifi_config.password));
        
        xSemaphoreGive(s_wifi_mutex);
    } else {
        ESP_LOGW(TAG, "Could not acquire mutex for WiFi stop");
    }

    ESP_LOGI(TAG, "WiFi stopped");
    return ESP_OK;
}

esp_err_t wifi_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Stop WiFi first
    wifi_manager_stop();

    // Unregister event handlers
    if (s_wifi_event_instance) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_instance);
        s_wifi_event_instance = NULL;
    }
    if (s_ip_event_instance) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_event_instance);
        s_ip_event_instance = NULL;
    }

    // Clean up resources
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    if (s_wifi_mutex) {
        vSemaphoreDelete(s_wifi_mutex);
        s_wifi_mutex = NULL;
    }

    // Unmount SPIFFS
    esp_vfs_spiffs_unregister("storage");

    // Clear all sensitive data
    memset(&s_wifi_config, 0, sizeof(s_wifi_config));
    memset(&s_wifi_status, 0, sizeof(s_wifi_status));
    
    s_initialized = false;
    ESP_LOGI(TAG, "WiFi manager deinitialized");
    return ESP_OK;
}

esp_err_t wifi_manager_get_status(wifi_status_t* status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(status, &s_wifi_status, sizeof(wifi_status_t));
        
        // Update RSSI if connected
        if (s_wifi_status.state == WIFI_STATE_CONNECTED) {
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                status->rssi = ap_info.rssi;
            }
        }
        
        xSemaphoreGive(s_wifi_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

bool wifi_manager_is_connected(void)
{
    bool connected = false;
    if (xSemaphoreTake(s_wifi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        connected = (s_wifi_status.state == WIFI_STATE_CONNECTED);
        xSemaphoreGive(s_wifi_mutex);
    }
    return connected;
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

esp_err_t wifi_manager_store_credentials(const char* ssid, const char* password, 
                                       uint32_t timeout_ms, uint8_t retry_count, bool pmf_required)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(ssid) >= WIFI_SSID_MAX_LEN || strlen(password) >= WIFI_PASSWORD_MAX_LEN) {
        ESP_LOGE(TAG, "SSID or password too long");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (timeout_ms < WIFI_MIN_TIMEOUT_MS || timeout_ms > WIFI_MAX_TIMEOUT_MS ||
        retry_count < WIFI_MIN_RETRY_COUNT || retry_count > WIFI_MAX_RETRY_COUNT) {
        ESP_LOGE(TAG, "Invalid timeout or retry count parameters");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    // Store credentials in encrypted NVS
    ret = nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid);
    if (ret == ESP_OK) {
        ret = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, password);
    }
    if (ret == ESP_OK) {
        ret = nvs_set_u32(nvs_handle, NVS_KEY_TIMEOUT, timeout_ms);
    }
    if (ret == ESP_OK) {
        ret = nvs_set_u8(nvs_handle, NVS_KEY_RETRY, retry_count);
    }
    if (ret == ESP_OK) {
        ret = nvs_set_u8(nvs_handle, NVS_KEY_PMF_REQUIRED, pmf_required ? 1 : 0);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store credentials: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi credentials stored securely");
    return ESP_OK;
}

esp_err_t wifi_manager_load_credentials(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No stored credentials found: %s", esp_err_to_name(ret));
        return ret;
    }

    // Clear existing config
    memset(&s_wifi_config, 0, sizeof(s_wifi_config));
    
    // Set defaults first
    s_wifi_config.timeout_ms = WIFI_DEFAULT_TIMEOUT_MS;
    s_wifi_config.retry_count = WIFI_DEFAULT_RETRY_COUNT;
    s_wifi_config.pmf_required = false;  // Default to optional PMF

    // Load SSID
    size_t ssid_len = WIFI_SSID_MAX_LEN;
    ret = nvs_get_str(nvs_handle, NVS_KEY_SSID, s_wifi_config.ssid, &ssid_len);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to load SSID: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Load password
    if (ret == ESP_OK) {
        size_t password_len = WIFI_PASSWORD_MAX_LEN;
        ret = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, s_wifi_config.password, &password_len);
        if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to load password: %s", esp_err_to_name(ret));
            // Clear password from memory on error
            memset(s_wifi_config.password, 0, sizeof(s_wifi_config.password));
            nvs_close(nvs_handle);
            return ret;
        }
    }

    // Load timeout (optional)
    uint32_t timeout;
    if (nvs_get_u32(nvs_handle, NVS_KEY_TIMEOUT, &timeout) == ESP_OK) {
        if (timeout >= WIFI_MIN_TIMEOUT_MS && timeout <= WIFI_MAX_TIMEOUT_MS) {
            s_wifi_config.timeout_ms = timeout;
        }
    }

    // Load retry count (optional)
    uint8_t retry;
    if (nvs_get_u8(nvs_handle, NVS_KEY_RETRY, &retry) == ESP_OK) {
        if (retry >= WIFI_MIN_RETRY_COUNT && retry <= WIFI_MAX_RETRY_COUNT) {
            s_wifi_config.retry_count = retry;
        }
    }

    // Load PMF requirement (optional)
    uint8_t pmf_val;
    if (nvs_get_u8(nvs_handle, NVS_KEY_PMF_REQUIRED, &pmf_val) == ESP_OK) {
        s_wifi_config.pmf_required = (pmf_val != 0);
    }

    nvs_close(nvs_handle);

    if (ret == ESP_OK && strlen(s_wifi_config.ssid) > 0) {
        ESP_LOGI(TAG, "WiFi credentials loaded: SSID=%s, timeout=%lu ms, retry_count=%d, PMF=%s",
                 s_wifi_config.ssid, s_wifi_config.timeout_ms, s_wifi_config.retry_count,
                 s_wifi_config.pmf_required ? "required" : "optional");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "No valid credentials found in NVS");
        return ESP_ERR_NOT_FOUND;
    }
}