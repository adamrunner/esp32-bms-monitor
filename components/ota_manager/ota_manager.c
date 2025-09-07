#include "ota_manager.h"
#include <string.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <esp_http_client.h>
#include <esp_app_format.h>
#include <esp_image_format.h>
#include <esp_spiffs.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char *TAG = "ota_manager";

#define OTA_TASK_STACK_SIZE 8192
#define OTA_TASK_PRIORITY 5
#define VERSION_CHECK_TIMEOUT_MS 10000
#define OTA_DOWNLOAD_TIMEOUT_MS 60000

// Global state
static ota_config_t g_ota_config = {0};
static ota_progress_callback_t g_progress_callback = NULL;
static ota_status_t g_ota_status = OTA_STATUS_IDLE;
static SemaphoreHandle_t g_ota_mutex = NULL;
static TaskHandle_t g_ota_task_handle = NULL;
static bool g_initialized = false;

// Forward declarations
static void ota_task(void *pvParameter);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static void set_status(ota_status_t status, int progress, const char* message);

esp_err_t ota_manager_init(const ota_config_t* config, ota_progress_callback_t callback)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "OTA manager already initialized");
        return ESP_OK;
    }

    if (!config) {
        ESP_LOGE(TAG, "Config cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Copy configuration
    memcpy(&g_ota_config, config, sizeof(ota_config_t));
    g_progress_callback = callback;

    // Create mutex for thread safety
    g_ota_mutex = xSemaphoreCreateMutex();
    if (!g_ota_mutex) {
        ESP_LOGE(TAG, "Failed to create OTA mutex");
        return ESP_ERR_NO_MEM;
    }

    g_initialized = true;
    g_ota_status = OTA_STATUS_IDLE;

    ESP_LOGI(TAG, "OTA manager initialized successfully");
    ESP_LOGI(TAG, "Current version: %s", g_ota_config.current_version);
    ESP_LOGI(TAG, "Update server: %s", g_ota_config.server_url);

    return ESP_OK;
}

esp_err_t ota_manager_load_config(const char* config_path, ota_config_t* config)
{
    if (!config_path || !config) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(config_path, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open OTA config file: %s", config_path);
        return ESP_ERR_NOT_FOUND;
    }

    // Read file content
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_string = malloc(file_size + 1);
    if (!json_string) {
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    size_t read_size = fread(json_string, 1, file_size, file);
    json_string[read_size] = '\0';
    fclose(file);

    // Parse JSON
    cJSON *json = cJSON_Parse(json_string);
    free(json_string);

    if (!json) {
        ESP_LOGE(TAG, "Failed to parse OTA config JSON");
        return ESP_ERR_INVALID_ARG;
    }

    // Extract configuration values
    cJSON *server_url = cJSON_GetObjectItem(json, "server_url");
    cJSON *cert_pem = cJSON_GetObjectItem(json, "cert_pem");
    cJSON *skip_cert = cJSON_GetObjectItem(json, "skip_cert_verification");
    cJSON *timeout = cJSON_GetObjectItem(json, "timeout_ms");
    cJSON *version = cJSON_GetObjectItem(json, "current_version");
    cJSON *auto_rollback = cJSON_GetObjectItem(json, "auto_rollback_enabled");

    // Set default values
    memset(config, 0, sizeof(ota_config_t));
    config->timeout_ms = 30000;
    config->auto_rollback_enabled = true;
    strcpy(config->current_version, "1.0.0");

    if (cJSON_IsString(server_url)) {
        strncpy(config->server_url, server_url->valuestring, sizeof(config->server_url) - 1);
    }

    if (cJSON_IsString(cert_pem)) {
        strncpy(config->cert_pem, cert_pem->valuestring, sizeof(config->cert_pem) - 1);
    }

    if (cJSON_IsBool(skip_cert)) {
        config->skip_cert_verification = cJSON_IsTrue(skip_cert);
    }

    if (cJSON_IsNumber(timeout)) {
        config->timeout_ms = timeout->valueint;
    }

    if (cJSON_IsString(version)) {
        strncpy(config->current_version, version->valuestring, sizeof(config->current_version) - 1);
    }

    if (cJSON_IsBool(auto_rollback)) {
        config->auto_rollback_enabled = cJSON_IsTrue(auto_rollback);
    }

    cJSON_Delete(json);
    ESP_LOGI(TAG, "OTA configuration loaded from %s", config_path);
    return ESP_OK;
}

esp_err_t ota_manager_check_update(char* available_version, size_t version_len)
{
    if (!g_initialized || !available_version) {
        return ESP_ERR_INVALID_STATE;
    }

    // Construct version check URL by replacing firmware.bin with version in the server URL
    char version_url[512];
    strncpy(version_url, g_ota_config.server_url, sizeof(version_url) - 1);
    version_url[sizeof(version_url) - 1] = '\0';

    // Replace firmware.bin with version
    char* firmware_pos = strstr(version_url, "firmware.bin");
    if (firmware_pos) {
        // Calculate lengths
        size_t prefix_len = firmware_pos - version_url;
        size_t suffix_len = strlen(firmware_pos + strlen("firmware.bin"));
        const char* replacement = "version";
        size_t replacement_len = strlen(replacement);

        // Ensure we have enough space
        if (prefix_len + replacement_len + suffix_len < sizeof(version_url)) {
            // Move the suffix to make room for replacement
            memmove(firmware_pos + replacement_len, firmware_pos + strlen("firmware.bin"), suffix_len + 1);
            // Copy the replacement string
            memcpy(firmware_pos, replacement, replacement_len);
        }
    } else {
        // Fallback: try to construct from base URL
        // Remove any trailing slash and append /version
        size_t len = strlen(version_url);
        if (len > 0 && version_url[len - 1] == '/') {
            version_url[len - 1] = '\0';
            len--;
        }
        if (len + strlen("/version") < sizeof(version_url)) {
            strcat(version_url, "/version");
        }
    }

    ESP_LOGI(TAG, "Checking for updates at: %s", version_url);

    esp_http_client_config_t config = {
        .url = version_url,
        .timeout_ms = VERSION_CHECK_TIMEOUT_MS,
        .skip_cert_common_name_check = g_ota_config.skip_cert_verification,
    };

    if (strlen(g_ota_config.cert_pem) > 0) {
        config.cert_pem = g_ota_config.cert_pem;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);

        ESP_LOGI(TAG, "Version check HTTP status: %d, content length: %d", status_code, content_length);

        if (status_code == 200 && content_length > 0) {
            char* response_buffer = malloc(content_length + 1);
            if (response_buffer) {
                int data_read = esp_http_client_read_response(client, response_buffer, content_length);
                if (data_read > 0) {
                    response_buffer[data_read] = '\0';
                    ESP_LOGI(TAG, "Version check response: %s", response_buffer);

                    // Parse version from response
                    cJSON *json = cJSON_Parse(response_buffer);
                    if (json) {
                        cJSON *version = cJSON_GetObjectItem(json, "version");
                        if (cJSON_IsString(version)) {
                            strncpy(available_version, version->valuestring, version_len - 1);
                            available_version[version_len - 1] = '\0';

                            // Compare versions (simple string comparison for now)
                            if (strcmp(available_version, g_ota_config.current_version) != 0) {
                                ESP_LOGI(TAG, "Update available: %s -> %s", g_ota_config.current_version, available_version);
                                cJSON_Delete(json);
                                free(response_buffer);
                                esp_http_client_cleanup(client);
                                return ESP_OK;
                            } else {
                                ESP_LOGI(TAG, "Device is up to date (version %s)", available_version);
                            }
                        }
                        cJSON_Delete(json);
                    } else {
                        ESP_LOGE(TAG, "Failed to parse version JSON response");
                    }
                }
                free(response_buffer);
            }
        } else {
            ESP_LOGW(TAG, "Version check failed with HTTP status %d", status_code);
        }
        err = ESP_ERR_NOT_FOUND; // No update available or check failed
    } else {
        ESP_LOGE(TAG, "Version check HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

esp_err_t ota_manager_start_update(bool force_update)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_ota_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire OTA mutex");
        return ESP_ERR_TIMEOUT;
    }

    if (g_ota_status != OTA_STATUS_IDLE) {
        ESP_LOGW(TAG, "OTA operation already in progress");
        xSemaphoreGive(g_ota_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    // Check for update availability unless forced
    if (!force_update) {
        char available_version[32];
        esp_err_t check_result = ota_manager_check_update(available_version, sizeof(available_version));
        if (check_result != ESP_OK) {
            ESP_LOGI(TAG, "No update available");
            xSemaphoreGive(g_ota_mutex);
            return check_result;
        }
    }

    // Create OTA task
    BaseType_t task_result = xTaskCreate(ota_task, "ota_task", OTA_TASK_STACK_SIZE, NULL, OTA_TASK_PRIORITY, &g_ota_task_handle);

    xSemaphoreGive(g_ota_mutex);

    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "OTA update started");
    return ESP_OK;
}

static void ota_task(void *pvParameter)
{
    set_status(OTA_STATUS_DOWNLOADING, 0, "Starting firmware download");

    esp_http_client_config_t config = {
        .url = g_ota_config.server_url,
        .timeout_ms = OTA_DOWNLOAD_TIMEOUT_MS,
        .skip_cert_common_name_check = g_ota_config.skip_cert_verification,
        .event_handler = http_event_handler,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
    };

    if (strlen(g_ota_config.cert_pem) > 0) {
        config.cert_pem = g_ota_config.cert_pem;
    }

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI(TAG, "Starting HTTPS OTA update from: %s", g_ota_config.server_url);

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        set_status(OTA_STATUS_SUCCESS, 100, "Update completed successfully");
        ESP_LOGI(TAG, "OTA update successful, restarting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        set_status(OTA_STATUS_FAILED, 0, "Update failed");
        ESP_LOGE(TAG, "HTTPS OTA update failed: %s", esp_err_to_name(ret));
    }

    g_ota_task_handle = NULL;
    vTaskDelete(NULL);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t ota_manager_mark_valid(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Application marked as valid, rollback cancelled");
        set_status(OTA_STATUS_IDLE, 0, "Application validated");
    } else {
        ESP_LOGE(TAG, "Failed to mark app as valid: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t ota_manager_rollback(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    set_status(OTA_STATUS_ROLLBACK, 0, "Rolling back to previous firmware");
    ESP_LOGI(TAG, "Triggering rollback to previous firmware");

    esp_err_t ret = esp_ota_mark_app_invalid_rollback_and_reboot();
    // This function should not return if successful
    return ret;
}

ota_status_t ota_manager_get_status(void)
{
    return g_ota_status;
}

esp_err_t ota_manager_get_version(char* version, size_t version_len)
{
    if (!version || version_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_initialized) {
        strncpy(version, g_ota_config.current_version, version_len - 1);
        version[version_len - 1] = '\0';
        return ESP_OK;
    }

    // Fallback to app description
    const esp_app_desc_t* app_desc = esp_app_get_description();
    if (app_desc) {
        strncpy(version, app_desc->version, version_len - 1);
        version[version_len - 1] = '\0';
        return ESP_OK;
    }

    return ESP_ERR_INVALID_STATE;
}

bool ota_manager_is_rollback_pending(void)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        return (ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    }

    return false;
}

static void set_status(ota_status_t status, int progress, const char* message)
{
    g_ota_status = status;

    ESP_LOGI(TAG, "OTA Status: %d, Progress: %d%%, Message: %s", status, progress, message ? message : "");

    if (g_progress_callback) {
        g_progress_callback(status, progress, message);
    }
}
