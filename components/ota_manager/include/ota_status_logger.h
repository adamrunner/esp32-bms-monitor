#ifndef OTA_STATUS_LOGGER_H
#define OTA_STATUS_LOGGER_H

#include <esp_err.h>
#include "ota_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize OTA status logger
 * 
 * @return ESP_OK on success
 */
esp_err_t ota_status_logger_init(void);

/**
 * @brief OTA progress callback that logs status via the logging system
 * 
 * @param status Current OTA status
 * @param progress Progress percentage (0-100)
 * @param message Status message
 */
void ota_status_progress_callback(ota_status_t status, int progress, const char* message);

/**
 * @brief Set available version for logging
 * 
 * @param version Available version string
 */
void ota_status_logger_set_available_version(const char* version);

#ifdef __cplusplus
}
#endif

#endif // OTA_STATUS_LOGGER_H