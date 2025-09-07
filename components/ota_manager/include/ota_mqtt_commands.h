#ifndef OTA_MQTT_COMMANDS_H
#define OTA_MQTT_COMMANDS_H

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize OTA MQTT command handler
 * 
 * @param command_topic MQTT topic for OTA commands (default: "bms/ota/command")
 * @return ESP_OK on success
 */
esp_err_t ota_mqtt_commands_init(const char* command_topic);

/**
 * @brief Shutdown OTA MQTT command handler
 */
void ota_mqtt_commands_shutdown(void);

/**
 * @brief Check if MQTT commands are connected
 * 
 * @return true if connected
 */
bool ota_mqtt_commands_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_MQTT_COMMANDS_H