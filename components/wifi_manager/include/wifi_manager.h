#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi.h"

// WiFi configuration constants
#define WIFI_SSID_MAX_LEN           32
#define WIFI_PASSWORD_MAX_LEN       64
#define WIFI_CONFIG_LINE_MAX_LEN    128
#define WIFI_MIN_TIMEOUT_MS         1000
#define WIFI_MAX_TIMEOUT_MS         60000
#define WIFI_MIN_RETRY_COUNT        1
#define WIFI_MAX_RETRY_COUNT        10
#define WIFI_DEFAULT_TIMEOUT_MS     10000
#define WIFI_DEFAULT_RETRY_COUNT    3

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
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
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

/**
 * @brief Initialize the WiFi manager
 * 
 * Initializes NVS flash, network interface, WiFi driver, event handlers,
 * and SPIFFS for configuration storage.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Deinitialize the WiFi manager and clean up all resources
 * 
 * Stops WiFi, unregisters event handlers, deletes semaphores and event groups,
 * unmounts SPIFFS, and clears sensitive data from memory.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Load WiFi configuration from a file
 * 
 * Reads WiFi credentials and settings from a configuration file.
 * File format: ssid=value, password=value, timeout_ms=value, retry_count=value
 * 
 * @param config_file_path Path to the configuration file
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_config_from_file(const char* config_file_path);

/**
 * @brief Store WiFi credentials securely in encrypted NVS
 * 
 * @param ssid WiFi network SSID
 * @param password WiFi network password
 * @param timeout_ms Connection timeout in milliseconds
 * @param retry_count Number of retry attempts
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_store_credentials(const char* ssid, const char* password, 
                                       uint32_t timeout_ms, uint8_t retry_count);

/**
 * @brief Load WiFi credentials from encrypted NVS
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_load_credentials(void);

/**
 * @brief Start WiFi connection
 * 
 * Configures and starts WiFi station mode, attempts to connect to the
 * configured network with timeout and retry logic.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Stop WiFi connection
 * 
 * Stops WiFi and clears sensitive data from memory.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_stop(void);

/**
 * @brief Get current WiFi status
 * 
 * Thread-safe function to retrieve current WiFi connection status,
 * including state, IP address, RSSI, and connection statistics.
 * 
 * @param status Pointer to status structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_get_status(wifi_status_t* status);

/**
 * @brief Check if WiFi is connected
 * 
 * Thread-safe function to check connection status.
 * 
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get string representation of WiFi state
 * 
 * @param state WiFi state enum value
 * @return String representation of the state
 */
const char* wifi_manager_get_state_string(wifi_state_t state);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H