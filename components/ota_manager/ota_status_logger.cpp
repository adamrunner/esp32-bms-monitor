#include "ota_manager.h"
#include "ota_status.h"
#include "ota_mqtt_publisher.h"
#include "log_manager.h"
#include "sntp_manager.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <string.h>

static const char *TAG = "ota_status_logger";

// Global OTA status logger instance
static bool g_ota_logger_initialized = false;
static char g_available_version[32] = {0};

extern "C" {

/**
 * @brief OTA progress callback that logs status via the logging system
 */
void ota_status_progress_callback(ota_status_t status, int progress, const char* message)
{
    if (!g_ota_logger_initialized) {
        ESP_LOGW(TAG, "OTA status logger not initialized, skipping status update");
        return;
    }

    // Create OTA status snapshot
    ota_status_snapshot_t snapshot = {0};
    
    // Get current time and uptime
    snapshot.timestamp_us = esp_timer_get_time();
    snapshot.uptime_sec = snapshot.timestamp_us / 1000000;
    
    // Set OTA status information
    snapshot.status = (int)status;
    snapshot.progress_pct = progress;
    if (message) {
        strncpy(snapshot.message, message, sizeof(snapshot.message) - 1);
    }
    
    // Get current version
    ota_manager_get_version(snapshot.current_version, sizeof(snapshot.current_version));
    
    // Copy available version if known
    if (strlen(g_available_version) > 0) {
        strncpy(snapshot.available_version, g_available_version, sizeof(snapshot.available_version) - 1);
    }
    
    // Check rollback status
    snapshot.rollback_pending = ota_manager_is_rollback_pending();
    
    // Get free heap
    snapshot.free_heap = esp_get_free_heap_size();
    
    ESP_LOGI(TAG, "OTA Status Update - Status: %d, Progress: %d%%, Message: %s", 
             snapshot.status, snapshot.progress_pct, snapshot.message);
    
    // Send to dedicated OTA MQTT topic
    esp_err_t mqtt_result = ota_mqtt_publisher_send_status(&snapshot);
    if (mqtt_result != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish OTA status to MQTT: %s", esp_err_to_name(mqtt_result));
    }
    
    // Also log locally for debugging
    ESP_LOGI(TAG, "OTA Status: status=%d, progress=%d%%, version=%s->%s, heap=%lu", 
             snapshot.status, snapshot.progress_pct, 
             snapshot.current_version, snapshot.available_version, snapshot.free_heap);
}

/**
 * @brief Initialize OTA status logger
 */
esp_err_t ota_status_logger_init(void)
{
    if (g_ota_logger_initialized) {
        ESP_LOGW(TAG, "OTA status logger already initialized");
        return ESP_OK;
    }
    
    // Initialize OTA MQTT publisher
    esp_err_t ret = ota_mqtt_publisher_init("bms/ota/status");
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize OTA MQTT publisher: %s", esp_err_to_name(ret));
        // Continue anyway, logging will still work locally
    }
    
    g_ota_logger_initialized = true;
    ESP_LOGI(TAG, "OTA status logger initialized");
    return ESP_OK;
}

/**
 * @brief Set available version for logging
 */
void ota_status_logger_set_available_version(const char* version)
{
    if (version) {
        strncpy(g_available_version, version, sizeof(g_available_version) - 1);
        g_available_version[sizeof(g_available_version) - 1] = '\0';
    } else {
        g_available_version[0] = '\0';
    }
}

} // extern "C"