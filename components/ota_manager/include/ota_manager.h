#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA update status
 */
typedef enum {
    OTA_STATUS_IDLE = 0,
    OTA_STATUS_CHECKING,
    OTA_STATUS_DOWNLOADING,
    OTA_STATUS_INSTALLING,
    OTA_STATUS_SUCCESS,
    OTA_STATUS_FAILED,
    OTA_STATUS_ROLLBACK
} ota_status_t;

/**
 * @brief OTA configuration structure
 */
typedef struct {
    char server_url[256];
    char cert_pem[2048];
    bool skip_cert_verification;
    uint32_t timeout_ms;
    char current_version[32];
    bool auto_rollback_enabled;
} ota_config_t;

/**
 * @brief OTA progress callback function type
 * 
 * @param status Current OTA status
 * @param progress Progress percentage (0-100)
 * @param message Status message
 */
typedef void (*ota_progress_callback_t)(ota_status_t status, int progress, const char* message);

/**
 * @brief Initialize OTA manager
 * 
 * @param config OTA configuration
 * @param callback Progress callback function (optional)
 * @return ESP_OK on success
 */
esp_err_t ota_manager_init(const ota_config_t* config, ota_progress_callback_t callback);

/**
 * @brief Load OTA configuration from SPIFFS
 * 
 * @param config_path Path to configuration file
 * @param config Output configuration structure
 * @return ESP_OK on success
 */
esp_err_t ota_manager_load_config(const char* config_path, ota_config_t* config);

/**
 * @brief Check for available firmware updates
 * 
 * @param available_version Output buffer for available version string
 * @param version_len Length of version buffer
 * @return ESP_OK if update available, ESP_ERR_NOT_FOUND if no update
 */
esp_err_t ota_manager_check_update(char* available_version, size_t version_len);

/**
 * @brief Start firmware update process
 * 
 * @param force_update Skip version checking if true
 * @return ESP_OK on success
 */
esp_err_t ota_manager_start_update(bool force_update);

/**
 * @brief Mark current application as valid (cancel rollback)
 * 
 * @return ESP_OK on success
 */
esp_err_t ota_manager_mark_valid(void);

/**
 * @brief Trigger rollback to previous firmware
 * 
 * @return ESP_OK on success
 */
esp_err_t ota_manager_rollback(void);

/**
 * @brief Get current OTA status
 * 
 * @return Current status
 */
ota_status_t ota_manager_get_status(void);

/**
 * @brief Get current firmware version
 * 
 * @param version Output buffer for version string
 * @param version_len Length of version buffer
 * @return ESP_OK on success
 */
esp_err_t ota_manager_get_version(char* version, size_t version_len);

/**
 * @brief Check if rollback is pending
 * 
 * @return true if rollback is pending
 */
bool ota_manager_is_rollback_pending(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_MANAGER_H