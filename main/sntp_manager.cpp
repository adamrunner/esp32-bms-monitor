#include "sntp_manager.h"
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <ctime>

namespace sntp {

static const char* TAG = "SNTP_MANAGER";

// SNTP event handler
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronization event: %lld.%06ld", (long long)tv->tv_sec, tv->tv_usec);
}

bool SNTPManager::init(const std::string& server, const std::string& timezone) {
    if (initialized_) {
        ESP_LOGW(TAG, "SNTP already initialized");
        return true;
    }

    server_ = server;
    timezone_ = timezone;

    ESP_LOGI(TAG, "Initializing SNTP with server: %s", server.c_str());

    // Set timezone
    setenv("TZ", timezone.c_str(), 1);
    tzset();

    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, server_.c_str());
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    
    esp_sntp_init();

    initialized_ = true;
    ESP_LOGI(TAG, "SNTP initialized successfully");
    return true;
}

bool SNTPManager::isTimeSynced() const {
    if (!initialized_) {
        return false;
    }

    time_t now = time(nullptr);
    struct tm timeinfo = {0};
    
    if (localtime_r(&now, &timeinfo)) {
        // Check if time is reasonable (after 2024 and before 2030)
        if (timeinfo.tm_year + 1900 > 2024 && timeinfo.tm_year + 1900 < 2030) {
            return true;
        }
    }
    
    return false;
}

time_t SNTPManager::getCurrentTime() const {
    return time(nullptr);
}

std::string SNTPManager::getFormattedTime(const char* format) const {
    time_t now = time(nullptr);
    struct tm timeinfo = {0};
    
    char buffer[64];
    if (localtime_r(&now, &timeinfo)) {
        strftime(buffer, sizeof(buffer), format, &timeinfo);
        return std::string(buffer);
    }
    
    return "1970-01-01 00:00:00"; // Fallback
}

bool SNTPManager::waitForSync(int timeout_ms) {
    if (!initialized_) {
        ESP_LOGE(TAG, "SNTP not initialized");
        return false;
    }

    ESP_LOGI(TAG, "Waiting for time synchronization (timeout: %d ms)", timeout_ms);
    
    int64_t start_time = esp_timer_get_time() / 1000; // Convert to ms
    
    while ((esp_timer_get_time() / 1000) - start_time < timeout_ms) {
        if (isTimeSynced()) {
            ESP_LOGI(TAG, "Time synchronized successfully");
            return true;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
    }
    
    ESP_LOGW(TAG, "Time synchronization timeout");
    return false;
}

void SNTPManager::shutdown() {
    if (initialized_) {
        esp_sntp_stop();
        initialized_ = false;
        time_synced_ = false;
        ESP_LOGI(TAG, "SNTP shutdown");
    }
}

} // namespace sntp