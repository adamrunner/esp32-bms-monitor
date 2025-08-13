#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

typedef struct {
    char ssid[32];
    char password[64];
    uint32_t timeout_ms;
    uint8_t retry_count;
} wifi_manager_config_t;

typedef struct {
    wifi_state_t state;
    uint32_t ip_address;
    int8_t rssi;
    uint8_t retry_attempts;
    uint64_t connected_time_us;
    uint32_t disconnect_count;
} wifi_status_t;

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_config_from_file(const char* config_file_path);
esp_err_t wifi_manager_start(void);
esp_err_t wifi_manager_stop(void);
esp_err_t wifi_manager_get_status(wifi_status_t* status);
bool wifi_manager_is_connected(void);
const char* wifi_manager_get_state_string(wifi_state_t state);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H