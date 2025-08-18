#include <stdio.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include "bms_interface.h"
#include "daly_bms.h"
#include "jbd_bms.h"
#include "bms_snapshot.h"
#include "log_manager.h"
#include "sntp_manager.h"
#define BMS_RX_PIN 4
#define BMS_TX_PIN 5
#include "wifi_manager.h"

static const char *TAG = "bms_monitor";
static constexpr uint32_t READ_INTERVAL_MS = 10000;

// BMS instances
static bms_interface_t* bms_interface = NULL;

// SNTP manager
static sntp::SNTPManager sntp_manager;

// Function to detect BMS type (placeholder implementation)
static bool auto_detect_bms_type() {
    // For now, we'll default to JBD BMS
    // In a real implementation, this would send detection commands
    // and analyze responses to determine BMS type
    return false; // Assume JBD BMS for now
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting BMS Monitor Application");

    // Initialize WiFi manager
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    esp_err_t wifi_ret = wifi_manager_init();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(wifi_ret));
    } else {
        // Load WiFi configuration from file
        wifi_ret = wifi_manager_config_from_file("/spiffs/wifi_config.txt");
        if (wifi_ret == ESP_OK) {
            ESP_LOGI(TAG, "Starting WiFi connection...");
            wifi_ret = wifi_manager_start();
            if (wifi_ret == ESP_OK) {
                ESP_LOGI(TAG, "WiFi connected successfully");
            } else {
                ESP_LOGW(TAG, "WiFi connection failed: %s", esp_err_to_name(wifi_ret));
            }
        } else {
            ESP_LOGW(TAG, "Failed to load WiFi config: %s", esp_err_to_name(wifi_ret));
        }
    }

    // Initialize SNTP for real timestamps
    ESP_LOGI(TAG, "Initializing SNTP for real timestamps...");
    if (!sntp_manager.init("pool.ntp.org", "UTC")) {
        ESP_LOGW(TAG, "Failed to initialize SNTP, using fallback timestamps");
    } else {
        // Wait for time synchronization (non-blocking)
        ESP_LOGI(TAG, "Waiting for time synchronization...");
        if (sntp_manager.waitForSync(5000)) { // 5 second timeout
            ESP_LOGI(TAG, "Time synchronized successfully");
        } else {
            ESP_LOGW(TAG, "Time sync timeout, continuing with system time");
        }
    }

    // Initialize logging system
    ESP_LOGI(TAG, "Initializing logging manager...");
    std::string logging_config = R"({"sinks":[
        {"type":"serial","config":{"format":"csv","print_header":true,"max_cells":4,"max_temps":3}},
        {"type":"mqtt","config":{"format":"csv","topic":"bms/telemetry","qos":1}},
        {"type":"sdcard","config":{"file_prefix":"bms_data","buffer_size":32768,"flush_interval_ms":120000,"fsync_interval_ms":60000,"max_lines_per_file":10000,"enable_free_space_check":true,"min_free_space_mb":10,"spi":{"mosi_pin":23,"miso_pin":19,"clk_pin":18,"cs_pin":22,"freq_khz":10000}}}
    ]})";

    if (!LOG_INIT(logging_config)) {
        ESP_LOGE(TAG, "Failed to initialize logging system");
        // Fallback to basic serial output
        ESP_LOGI(TAG, "Using basic serial output...");
    } else {
        ESP_LOGI(TAG, "Logging system initialized with configuration: %s", logging_config.c_str());
    }

    if (auto_detect_bms_type()) {
        ESP_LOGI(TAG, "Daly BMS detected, initializing...");
        bms_interface = daly_bms_create(UART_NUM_1, BMS_RX_PIN, BMS_TX_PIN);
    } else {
        ESP_LOGI(TAG, "JBD BMS detected, initializing...");
        bms_interface = jbd_bms_create(UART_NUM_1, BMS_RX_PIN, BMS_TX_PIN);
    }

    if (!bms_interface) {
        ESP_LOGE(TAG, "Failed to create BMS interface");
        return;
    }

    ESP_LOGI(TAG, "BMS interface created successfully");

    // Configure logging format and prepare runtime CSV header sizing
    static output::OutputConfig g_log_cfg{};
    #ifdef LOG_FORMAT_CSV
    g_log_cfg.format = output::OutputFormat::CSV;
    g_log_cfg.csv_print_header_once = true;
    g_log_cfg.header_cells = output::DEFAULT_MAX_CSV_CELLS;
    g_log_cfg.header_temps = output::DEFAULT_MAX_CSV_TEMPS;
    #endif
    static bool g_csv_header_configured = false;

    // Variables for time and energy tracking
    uint64_t start_time = esp_timer_get_time();
    uint64_t last_time = start_time;
    double total_energy_wh = 0.0;

    // Main monitoring loop
    while (1) {
        // Read all BMS measurements
        if (bms_interface->readMeasurements(bms_interface->handle)) {
            // Get basic measurements
            float voltage = bms_interface->getPackVoltage(bms_interface->handle);
            float current = bms_interface->getPackCurrent(bms_interface->handle);
            float soc = bms_interface->getStateOfCharge(bms_interface->handle);
            float power = bms_interface->getPower(bms_interface->handle);
            float full_capacity = bms_interface->getFullCapacity(bms_interface->handle);

            // Time and energy accumulation
            uint64_t current_time = esp_timer_get_time();
            double elapsed_us = (double)(current_time - last_time);
            double elapsed_h = elapsed_us / 1e6 / 3600;
            total_energy_wh += power * elapsed_h;
            last_time = current_time;
            // Calculate elapsed time since start
            uint64_t total_elapsed_us = current_time - start_time;
            unsigned int elapsed_sec = total_elapsed_us / 1000000;
            unsigned int hours = elapsed_sec / 3600;
            unsigned int minutes = (elapsed_sec % 3600) / 60;
            unsigned int seconds = elapsed_sec % 60;

            // Get cell information
            int cell_count = bms_interface->getCellCount(bms_interface->handle);
            float min_cell_voltage = bms_interface->getMinCellVoltage(bms_interface->handle);
            float max_cell_voltage = bms_interface->getMaxCellVoltage(bms_interface->handle);
            float cell_voltage_delta = bms_interface->getCellVoltageDelta(bms_interface->handle);
            int min_cell_num = bms_interface->getMinCellNumber(bms_interface->handle);
            int max_cell_num = bms_interface->getMaxCellNumber(bms_interface->handle);

            // Get temperature information
            int temp_count = bms_interface->getTemperatureCount(bms_interface->handle);
            float max_temp = bms_interface->getMaxTemperature(bms_interface->handle);
            float min_temp = bms_interface->getMinTemperature(bms_interface->handle);

            // Get peak values
            float peak_current = bms_interface->getPeakCurrent(bms_interface->handle);
            float peak_power = bms_interface->getPeakPower(bms_interface->handle);

            // Get MOSFET status
            bool charging_enabled = bms_interface->isChargingEnabled(bms_interface->handle);
            bool discharging_enabled = bms_interface->isDischargingEnabled(bms_interface->handle);

            // Emit via pluggable logger (Human or CSV)
            output::BMSSnapshot s{};
            s.start_time_us = start_time;
            s.now_time_us = current_time;
            s.elapsed_sec = elapsed_sec;
            s.hours = hours;
            s.minutes = minutes;
            s.seconds = seconds;

            // Set real timestamp from SNTP (fallback to current time if not sync'd)
            s.real_timestamp = sntp_manager.getCurrentTime();

            s.total_energy_wh = total_energy_wh;

            s.pack_voltage_v = voltage;
            s.pack_current_a = current;
            s.soc_pct = soc;
            s.power_w = power;
            s.full_capacity_ah = static_cast<float>(full_capacity);

            s.peak_current_a = peak_current;
            s.peak_power_w = peak_power;

            s.cell_count = cell_count;
            s.min_cell_voltage_v = min_cell_voltage;
            s.max_cell_voltage_v = max_cell_voltage;
            s.min_cell_num = min_cell_num;
            s.max_cell_num = max_cell_num;
            s.cell_voltage_delta_v = cell_voltage_delta;

            s.temp_count = temp_count;
            s.min_temp_c = min_temp;
            s.max_temp_c = max_temp;

            s.charging_enabled = charging_enabled;
            s.discharging_enabled = discharging_enabled;

            // Populate arrays (bounded)
            {
                int cells = cell_count;
                if (cells > output::DEFAULT_MAX_CSV_CELLS) cells = output::DEFAULT_MAX_CSV_CELLS;
                for (int i = 0; i < cells; ++i) {
                    s.cell_v[static_cast<size_t>(i)] = bms_interface->getCellVoltage(bms_interface->handle, i);
                }
            }
            {
                int temps = temp_count;
                if (temps > output::DEFAULT_MAX_CSV_TEMPS) temps = output::DEFAULT_MAX_CSV_TEMPS;
                for (int i = 0; i < temps; ++i) {
                    s.temp_c[static_cast<size_t>(i)] = bms_interface->getTemperature(bms_interface->handle, i);
                }
            }

            // Configure CSV header counts once (auto-detect or build-time override) before first emission
            if (g_log_cfg.format == output::OutputFormat::CSV && !g_csv_header_configured) {
                int hc =
                #ifdef LOG_CSV_CELLS
                    LOG_CSV_CELLS;
                #else
                    cell_count;
                #endif
                if (hc < 0) hc = 0;
                if (hc > output::DEFAULT_MAX_CSV_CELLS) hc = output::DEFAULT_MAX_CSV_CELLS;

                int ht =
                #ifdef LOG_CSV_TEMPS
                    LOG_CSV_TEMPS;
                #else
                    temp_count;
                #endif
                if (ht < 0) ht = 0;
                if (ht > output::DEFAULT_MAX_CSV_TEMPS) ht = output::DEFAULT_MAX_CSV_TEMPS;

                g_log_cfg.header_cells = hc;
                g_log_cfg.header_temps = ht;
                g_csv_header_configured = true;
            }

            // Send to new modular logging system
            LOG_SEND(s);
        } else {
            ESP_LOGE(TAG, "Failed to read BMS measurements");
            // printf("ERROR: Failed to read BMS data\n");
        }

        // Check WiFi status periodically (every 10 readings)
        static int wifi_check_counter = 0;
        if (++wifi_check_counter >= 10) {
            wifi_check_counter = 0;
            wifi_status_t wifi_status;
            if (wifi_manager_get_status(&wifi_status) == ESP_OK) {
                ESP_LOGD(TAG, "WiFi Status: %s, IP: %d.%d.%d.%d, RSSI: %d dBm, Disconnects: %lu",
                         wifi_manager_get_state_string(wifi_status.state),
                         (int)(wifi_status.ip_address & 0xFF),
                         (int)((wifi_status.ip_address >> 8) & 0xFF),
                         (int)((wifi_status.ip_address >> 16) & 0xFF),
                         (int)((wifi_status.ip_address >> 24) & 0xFF),
                         wifi_status.rssi,
                         wifi_status.disconnect_count);
            }
        }

        // Wait before next reading
        vTaskDelay(pdMS_TO_TICKS(READ_INTERVAL_MS));
    }

    // Cleanup
    LOG_SHUTDOWN();
}
