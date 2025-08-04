#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "bms_interface.h"
#include "daly_bms.h"
#include "jbd_bms.h"

static const char *TAG = "bms_monitor";

// BMS instances
static bms_interface_t* bms_interface = NULL;

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

    // Auto-detect BMS type
    // Assume 16/17 are the RX/TX pins for UART communication
    if (auto_detect_bms_type()) {
        ESP_LOGI(TAG, "Daly BMS detected, initializing...");
        bms_interface = daly_bms_create(UART_NUM_1, 16, 17);
    } else {
        ESP_LOGI(TAG, "JBD BMS detected, initializing...");
        bms_interface = jbd_bms_create(UART_NUM_1, 16, 17);
    }

    if (!bms_interface) {
        ESP_LOGE(TAG, "Failed to create BMS interface");
        return;
    }

    ESP_LOGI(TAG, "BMS interface created successfully");

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

            // Display measurements via serial
            printf("\n=== BMS Monitor Data ===\n");
            printf("Pack Voltage: %.2f V\n", voltage);
            printf("Pack Current: %.2f A\n", current);
            printf("State of Charge: %.1f%%\n", soc);
            printf("Power: %.2f W\n", power);
            if (full_capacity > 0) {
                printf("Full Capacity: %.2f Ah\n", full_capacity);
            }
            printf("Peak Current: %.2f A\n", peak_current);
            printf("Peak Power: %.2f W\n", peak_power);
            printf("Cell Count: %d\n", cell_count);
            printf("Cell Voltage Range: %.3f V (Cell %d) - %.3f V (Cell %d)\n",
                   min_cell_voltage, min_cell_num, max_cell_voltage, max_cell_num);
            printf("Cell Voltage Delta: %.3f V\n", cell_voltage_delta);
            printf("Temperature Count: %d\n", temp_count);
            printf("Temperature Range: %.1f°C - %.1f°C\n", min_temp, max_temp);
            printf("Charging Enabled: %s\n", charging_enabled ? "YES" : "NO");
            printf("Discharging Enabled: %s\n", discharging_enabled ? "YES" : "NO");
            printf("========================\n");

            // Display individual cell voltages
            printf("Individual Cell Voltages:\n");
            for (int i = 0; i < cell_count && i < 16; i++) { // Limit to first 16 cells for readability
                float cell_voltage = bms_interface->getCellVoltage(bms_interface->handle, i);
                printf("  Cell %d: %.3f V\n", i + 1, cell_voltage);
            }

            // Display individual temperatures
            printf("Individual Temperatures:\n");
            for (int i = 0; i < temp_count && i < 8; i++) { // Limit to first 8 sensors for readability
                float temperature = bms_interface->getTemperature(bms_interface->handle, i);
                printf("  Temp %d: %.1f°C\n", i + 1, temperature);
            }

        } else {
            ESP_LOGE(TAG, "Failed to read BMS measurements");
            printf("ERROR: Failed to read BMS data\n");
        }

        // Wait before next reading
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
