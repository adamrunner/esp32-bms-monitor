#include <Arduino.h>
#include <stdint.h>
#include "bms_interface.h"
#include "daly_bms.h"
#include "jbd_bms.h"
#include "logging.h"
#include "wifi_manager.h"
#include "mqtt_sink.h"

static constexpr uint32_t READ_INTERVAL_MS = 1000;

// BMS instances
static bms_interface_t* bms_interface = NULL;

// Function to detect BMS type (placeholder implementation)
static bool auto_detect_bms_type() {
    // For now, we'll default to JBD BMS
    // In a real implementation, this would send detection commands
    // and analyze responses to determine BMS type
    return false; // Assume JBD BMS for now
}

// Variables for time and energy tracking
static uint64_t start_time = 0;
static uint64_t last_time = 0;
static double total_energy_wh = 0.0;

// Configure logging format and prepare runtime CSV header sizing
static logging::LogConfig g_log_cfg{};
static bool g_csv_header_configured = false;
static logging::mqtt_sink* g_mqtt = nullptr;

void setup()
{
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    
    Serial.println("Starting BMS Monitor Application");

    // Auto-detect BMS type
    // UART1: RX=GPIO27, TX=GPIO14
    if (auto_detect_bms_type()) {
        Serial.println("Daly BMS detected, initializing...");
        bms_interface = daly_bms_create(1, 27, 14);  // UART1 with GPIO27(RX), GPIO14(TX)
    } else {
        Serial.println("JBD BMS detected, initializing...");
        bms_interface = jbd_bms_create(1, 27, 14);
    }

    if (!bms_interface) {
        Serial.println("ERROR: Failed to create BMS interface");
        return;
    }

    Serial.println("BMS interface created successfully");

    // Initialize WiFi
    Serial.println("Initializing WiFi...");
    if (wifi_manager::initialize()) {
        if (wifi_manager::connect()) {
            Serial.printf("WiFi connected: %s\n", wifi_manager::getLocalIP().c_str());
        } else {
            Serial.printf("WiFi connection failed: %s\n", wifi_manager::getStatusString().c_str());
        }
    } else {
        Serial.println("WiFi initialization failed");
    }

    // MQTT sink setup (configurable via build flags)
    const char* mqtt_host =
    #ifdef MQTT_HOST
        MQTT_HOST;
    #else
        "192.168.1.218";
    #endif
    const uint16_t mqtt_port =
    #ifdef MQTT_PORT
        MQTT_PORT;
    #else
        1883;
    #endif
    const char* mqtt_topic =
    #ifdef MQTT_TOPIC
        MQTT_TOPIC;
    #else
        "bms/telemetry";
    #endif
    const bool mqtt_enabled =
    #ifdef MQTT_ENABLED
        true;
    #else
        true;
    #endif
    g_mqtt = new logging::mqtt_sink(mqtt_host, mqtt_port, mqtt_topic, mqtt_enabled);
    g_mqtt->begin();

    #ifdef LOG_FORMAT_CSV
    g_log_cfg.format = logging::LogFormat::CSV;
    g_log_cfg.csv_print_header_once = true;
    g_log_cfg.header_cells = logging::DEFAULT_MAX_CSV_CELLS;
    g_log_cfg.header_temps = logging::DEFAULT_MAX_CSV_TEMPS;
    #endif

    // Initialize timing variables
    start_time = millis() * 1000;  // Convert to microseconds
    last_time = start_time;
}

void loop()
{

        // Read all BMS measurements
        if (bms_interface->readMeasurements(bms_interface->handle)) {
            // Get basic measurements
            float voltage = bms_interface->getPackVoltage(bms_interface->handle);
            float current = bms_interface->getPackCurrent(bms_interface->handle);
            float soc = bms_interface->getStateOfCharge(bms_interface->handle);
            float power = bms_interface->getPower(bms_interface->handle);
            float full_capacity = bms_interface->getFullCapacity(bms_interface->handle);

            // Time and energy accumulation
            uint64_t current_time = millis() * 1000;  // Convert to microseconds
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
            logging::MeasurementSnapshot s{};
            s.start_time_us = start_time;
            s.now_time_us = current_time;
            s.elapsed_sec = elapsed_sec;
            s.hours = hours;
            s.minutes = minutes;
            s.seconds = seconds;

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
                if (cells > logging::DEFAULT_MAX_CSV_CELLS) cells = logging::DEFAULT_MAX_CSV_CELLS;
                for (int i = 0; i < cells; ++i) {
                    s.cell_v[static_cast<size_t>(i)] = bms_interface->getCellVoltage(bms_interface->handle, i);
                }
            }
            {
                int temps = temp_count;
                if (temps > logging::DEFAULT_MAX_CSV_TEMPS) temps = logging::DEFAULT_MAX_CSV_TEMPS;
                for (int i = 0; i < temps; ++i) {
                    s.temp_c[static_cast<size_t>(i)] = bms_interface->getTemperature(bms_interface->handle, i);
                }
            }

            // Configure CSV header counts once (auto-detect or build-time override) before first emission
            if (g_log_cfg.format == logging::LogFormat::CSV && !g_csv_header_configured) {
                int hc =
                #ifdef LOG_CSV_CELLS
                    LOG_CSV_CELLS;
                #else
                    cell_count;
                #endif
                if (hc < 0) hc = 0;
                if (hc > logging::DEFAULT_MAX_CSV_CELLS) hc = logging::DEFAULT_MAX_CSV_CELLS;

                int ht =
                #ifdef LOG_CSV_TEMPS
                    LOG_CSV_TEMPS;
                #else
                    temp_count;
                #endif
                if (ht < 0) ht = 0;
                if (ht > logging::DEFAULT_MAX_CSV_TEMPS) ht = logging::DEFAULT_MAX_CSV_TEMPS;

                g_log_cfg.header_cells = hc;
                g_log_cfg.header_temps = ht;
                g_csv_header_configured = true;
            }

            // Emit to Serial
            logging::log_emit(s, g_log_cfg);
            // Emit to MQTT if configured (CSV line)
            if (g_log_cfg.format == logging::LogFormat::CSV && g_mqtt) {
                String line;
                // Build CSV row compatible with header counts
                format_csv_row(line, s, g_log_cfg);
                g_mqtt->write(line);
            }

        } else {
            Serial.println("ERROR: Failed to read BMS data");
        }

        // Service MQTT client
        if (g_mqtt) g_mqtt->tick();

        // Periodic MQTT diagnostics (every ~10s)
        static uint32_t last_diag = 0;
        uint32_t now_ms = millis();
        if (now_ms - last_diag > 10000) {
            last_diag = now_ms;
            if (g_mqtt) {
                Serial.printf("[MQTT] ok=%lu fail=%lu drop=%lu reconnects=%lu state=%ld\n",
                              g_mqtt->publish_ok(), g_mqtt->publish_fail(), g_mqtt->dropped(),
                              g_mqtt->reconnect_attempts(), g_mqtt->last_state());
            }
        }
        // Wait before next reading
        delay(READ_INTERVAL_MS);
}
