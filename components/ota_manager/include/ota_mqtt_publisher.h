#ifndef OTA_MQTT_PUBLISHER_H
#define OTA_MQTT_PUBLISHER_H

#include <esp_err.h>
#include <stdbool.h>
#include "ota_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize OTA MQTT publisher
 * 
 * @param topic MQTT topic for OTA status (default: "bms/ota/status")
 * @return ESP_OK on success
 */
esp_err_t ota_mqtt_publisher_init(const char* topic);

/**
 * @brief Send OTA status snapshot to MQTT
 * 
 * @param status OTA status snapshot to publish
 * @return ESP_OK on success
 */
esp_err_t ota_mqtt_publisher_send_status(const ota_status_snapshot_t* status);

/**
 * @brief Shutdown OTA MQTT publisher
 */
void ota_mqtt_publisher_shutdown(void);

/**
 * @brief Check if MQTT is connected
 * 
 * @return true if connected
 */
bool ota_mqtt_publisher_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_MQTT_PUBLISHER_H