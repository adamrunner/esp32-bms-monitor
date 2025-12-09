#include "device_id.h"

#include <string.h>
#include <ctype.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_spiffs.h>

static const char* TAG = "DEVICE_ID";

// Cached device ID (max 32 chars + null terminator)
static char cached_device_id[33] = {0};
static bool initialized = false;

// Validation: alphanumeric, hyphen, underscore only
static bool is_valid_device_id(const char* id)
{
    if (!id || strlen(id) == 0 || strlen(id) > 32) {
        return false;
    }

    for (size_t i = 0; i < strlen(id); i++) {
        char c = id[i];
        if (!isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }

    return true;
}

// Try to load custom device ID from SPIFFS
static esp_err_t load_custom_device_id(char* buffer, size_t buffer_size)
{
    FILE* config_file = fopen("/spiffs/device_config.txt", "r");
    if (!config_file) {
        ESP_LOGD(TAG, "No custom device config found in SPIFFS");
        return ESP_ERR_NOT_FOUND;
    }

    char line[128];
    bool found = false;

    while (fgets(line, sizeof(line), config_file)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = '\0';

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        // Look for device_id=value
        char* eq_pos = strchr(line, '=');
        if (eq_pos) {
            *eq_pos = '\0';
            char* key = line;
            char* value = eq_pos + 1;

            // Trim whitespace from key
            while (*key && isspace(*key)) key++;
            char* key_end = key + strlen(key) - 1;
            while (key_end > key && isspace(*key_end)) *key_end-- = '\0';

            // Trim whitespace from value
            while (*value && isspace(*value)) value++;
            char* value_end = value + strlen(value) - 1;
            while (value_end > value && isspace(*value_end)) *value_end-- = '\0';

            if (strcmp(key, "device_id") == 0) {
                if (is_valid_device_id(value)) {
                    snprintf(buffer, buffer_size, "%s", value);
                    found = true;
                    ESP_LOGI(TAG, "Loaded custom device_id: %s", buffer);
                    break;
                } else {
                    ESP_LOGW(TAG, "Invalid device_id in config (must be alphanumeric/hyphen/underscore, max 32 chars): %s", value);
                }
            }
        }
    }

    fclose(config_file);

    return found ? ESP_OK : ESP_ERR_NOT_FOUND;
}

// Generate device ID from eFUSE MAC address
static esp_err_t generate_efuse_device_id(char* buffer, size_t buffer_size)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_efuse_mac_get_default(mac);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read eFUSE MAC: 0x%x", err);
        return err;
    }

    snprintf(buffer, buffer_size, "bms-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Generated device_id from eFUSE MAC: %s", buffer);
    return ESP_OK;
}

esp_err_t device_id_init(void)
{
    if (initialized) {
        ESP_LOGD(TAG, "Device ID already initialized: %s", cached_device_id);
        return ESP_OK;
    }

    // Try custom device ID first (priority)
    esp_err_t err = load_custom_device_id(cached_device_id, sizeof(cached_device_id));

    if (err != ESP_OK) {
        // Fall back to eFUSE MAC-based ID
        err = generate_efuse_device_id(cached_device_id, sizeof(cached_device_id));

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize device ID");
            return err;
        }
    }

    initialized = true;
    ESP_LOGI(TAG, "Device ID initialized: %s", cached_device_id);
    return ESP_OK;
}

esp_err_t device_id_get(char *buffer, size_t buffer_size)
{
    if (!initialized) {
        ESP_LOGW(TAG, "device_id_get() called before device_id_init()");
        return ESP_ERR_INVALID_STATE;
    }

    if (!buffer || buffer_size < 33) {
        ESP_LOGE(TAG, "Invalid buffer (need at least 33 bytes)");
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(buffer, buffer_size, "%s", cached_device_id);
    return ESP_OK;
}
