#ifndef DEVICE_ID_H
#define DEVICE_ID_H

#include <esp_err.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize device ID subsystem
 *
 * Attempts to read custom device ID from /spiffs/device_config.txt
 * Falls back to ESP32 chip eFUSE MAC address if not configured
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t device_id_init(void);

/**
 * Get the device ID string
 *
 * Returns the cached device ID (custom or eFUSE-based)
 * Must call device_id_init() first
 *
 * @param buffer Output buffer (min 33 bytes)
 * @param buffer_size Size of buffer
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t device_id_get(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_ID_H
