#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "daly_bms.h"

static const char *TAG = "daly_bms";

// BMS Interface function implementations
static bool daly_bms_read_measurements(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return daly_bms_update(handle);
}

static float daly_bms_get_pack_voltage(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.packVoltage;
}

static float daly_bms_get_pack_current(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.packCurrent;
}

static float daly_bms_get_soc(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.packSOC;
}

static float daly_bms_get_power(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.power;
}

static float daly_bms_get_full_capacity(void* bms_handle) {
    // Daly BMS doesn't directly provide full capacity in the standard protocol
    // This would need to be calculated or configured separately
    return 0.0f;
}

static int daly_bms_get_cell_count(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.numberOfCells;
}

static float daly_bms_get_cell_voltage(void* bms_handle, int cell) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    if (cell >= 0 && cell < handle->data.numberOfCells) {
        return handle->data.cellVmV[cell] / 1000.0f; // Convert mV to V
    }
    return 0.0f;
}

static float daly_bms_get_min_cell_voltage(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.minCellmV / 1000.0f; // Convert mV to V
}

static float daly_bms_get_max_cell_voltage(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.maxCellmV / 1000.0f; // Convert mV to V
}

static int daly_bms_get_min_cell_number(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.minCellVNum;
}

static int daly_bms_get_max_cell_number(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.maxCellVNum;
}

static int daly_bms_get_temperature_count(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.numOfTempSensors;
}

static float daly_bms_get_temperature(void* bms_handle, int sensor) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    if (sensor >= 0 && sensor < handle->data.numOfTempSensors) {
        return (float)handle->data.cellTemperature[sensor];
    }
    return 0.0f;
}

static float daly_bms_get_max_temperature(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return (float)handle->data.tempMax;
}

static float daly_bms_get_min_temperature(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return (float)handle->data.tempMin;
}

static float daly_bms_get_peak_current(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.peakCurrent;
}

static float daly_bms_get_peak_power(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.peakPower;
}

static bool daly_bms_is_charging_enabled(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.chargeFetState;
}

static bool daly_bms_is_discharging_enabled(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.disChargeFetState;
}

static float daly_bms_get_cell_voltage_delta(void* bms_handle) {
    daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_handle;
    return handle->data.cellDiff / 1000.0f; // Convert mV to V
}

// Create Daly BMS interface
bms_interface_t* daly_bms_create(uart_port_t uart_port, int rx_pin, int tx_pin) {
    daly_bms_handle_t* handle = calloc(1, sizeof(daly_bms_handle_t));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate memory for Daly BMS handle");
        return NULL;
    }

    handle->uart_port = uart_port;

    // Initialize UART
    uart_config_t uart_config = {
        .baud_rate = DALY_BMS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t err = uart_param_config(uart_port, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(err));
        free(handle);
        return NULL;
    }

    err = uart_set_pin(uart_port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(err));
        free(handle);
        return NULL;
    }

    err = uart_driver_install(uart_port, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
        free(handle);
        return NULL;
    }

    // Initialize the BMS
    if (!daly_bms_init(handle)) {
        ESP_LOGE(TAG, "Failed to initialize Daly BMS");
        uart_driver_delete(uart_port);
        free(handle);
        return NULL;
    }

    // Create and populate interface structure
    bms_interface_t* interface = calloc(1, sizeof(bms_interface_t));
    if (!interface) {
        ESP_LOGE(TAG, "Failed to allocate memory for BMS interface");
        uart_driver_delete(uart_port);
        free(handle);
        return NULL;
    }

    interface->handle = handle;
    interface->readMeasurements = daly_bms_read_measurements;
    interface->getPackVoltage = daly_bms_get_pack_voltage;
    interface->getPackCurrent = daly_bms_get_pack_current;
    interface->getStateOfCharge = daly_bms_get_soc;
    interface->getPower = daly_bms_get_power;
    interface->getFullCapacity = daly_bms_get_full_capacity;
    interface->getCellCount = daly_bms_get_cell_count;
    interface->getCellVoltage = daly_bms_get_cell_voltage;
    interface->getMinCellVoltage = daly_bms_get_min_cell_voltage;
    interface->getMaxCellVoltage = daly_bms_get_max_cell_voltage;
    interface->getMinCellNumber = daly_bms_get_min_cell_number;
    interface->getMaxCellNumber = daly_bms_get_max_cell_number;
    interface->getTemperatureCount = daly_bms_get_temperature_count;
    interface->getTemperature = daly_bms_get_temperature;
    interface->getMaxTemperature = daly_bms_get_max_temperature;
    interface->getMinTemperature = daly_bms_get_min_temperature;
    interface->getPeakCurrent = daly_bms_get_peak_current;
    interface->getPeakPower = daly_bms_get_peak_power;
    interface->isChargingEnabled = daly_bms_is_charging_enabled;
    interface->isDischargingEnabled = daly_bms_is_discharging_enabled;
    interface->getCellVoltageDelta = daly_bms_get_cell_voltage_delta;

    ESP_LOGI(TAG, "Daly BMS interface created successfully");
    return interface;
}

// Destroy Daly BMS interface
void daly_bms_destroy(bms_interface_t* bms_interface) {
    if (bms_interface) {
        if (bms_interface->handle) {
            daly_bms_handle_t* handle = (daly_bms_handle_t*)bms_interface->handle;
            uart_driver_delete(handle->uart_port);
            free(handle);
        }
        free(bms_interface);
    }
}

// Initialize Daly BMS
bool daly_bms_init(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    // Pre-load the transmit buffer with command-independent bytes
    handle->tx_buffer[0] = 0xA5;  // Start byte
    handle->tx_buffer[1] = 0x01;  // Host address
    // Bytes 2-11 will be command-specific
    handle->tx_buffer[12] = 0x00; // Checksum placeholder

    // Initialize peak values
    handle->data.peakCurrent = 0.0f;
    handle->data.peakPower = 0.0f;

    ESP_LOGI(TAG, "Daly BMS initialized");
    return true;
}

// Update peak values
void daly_bms_update_peak_values(daly_bms_handle_t* handle) {
    if (!handle) {
        return;
    }

    // Update peak current (absolute value)
    float abs_current = fabsf(handle->data.packCurrent);
    if (abs_current > handle->data.peakCurrent) {
        handle->data.peakCurrent = abs_current;
    }

    // Update peak power (absolute value)
    float abs_power = fabsf(handle->data.power);
    if (abs_power > handle->data.peakPower) {
        handle->data.peakPower = abs_power;
    }
}

// Send command to BMS
void daly_bms_send_command(daly_bms_handle_t* handle, daly_command_t cmd_id) {
    if (!handle) {
        return;
    }

    // Set command byte
    handle->tx_buffer[2] = (uint8_t)cmd_id;

    // Clear data bytes
    for (int i = 3; i < 12; i++) {
        handle->tx_buffer[i] = 0x00;
    }

    // Calculate checksum
    uint8_t checksum = 0;
    for (int i = 0; i < 12; i++) {
        checksum += handle->tx_buffer[i];
    }
    handle->tx_buffer[12] = checksum;

    // Send command
    uart_write_bytes(handle->uart_port, (const char*)handle->tx_buffer, DALY_XFER_BUFFER_LENGTH);
}

// Receive bytes from BMS
bool daly_bms_receive_bytes(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    // Clear receive buffer
    memset(handle->rx_buffer, 0, DALY_XFER_BUFFER_LENGTH);

    // Read response
    int bytes_read = uart_read_bytes(handle->uart_port, handle->rx_buffer, DALY_XFER_BUFFER_LENGTH, pdMS_TO_TICKS(100));

    if (bytes_read == DALY_XFER_BUFFER_LENGTH) {
        // Validate checksum
        uint8_t checksum = 0;
        for (int i = 0; i < 12; i++) {
            checksum += handle->rx_buffer[i];
        }
        return (checksum == handle->rx_buffer[12]);
    }

    return false;
}

// Validate checksum
bool daly_bms_validate_checksum(daly_bms_handle_t* handle) {
    return daly_bms_receive_bytes(handle);
}

// Update all BMS data
bool daly_bms_update(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    // Get basic measurements
    if (!daly_bms_get_pack_measurements(handle)) {
        return false;
    }

    // Get min/max cell voltages
    daly_bms_get_min_max_cell_voltage(handle);

    // Get temperature data
    daly_bms_get_pack_temp(handle);

    // Get cell voltages
    daly_bms_get_cell_voltages(handle);

    // Get cell temperatures
    daly_bms_get_cell_temperature(handle);

    // Get cell balance state
    daly_bms_get_cell_balance_state(handle);

    // Get failure codes
    daly_bms_get_failure_codes(handle);

    // Get status info
    daly_bms_get_status_info(handle);

    // Get charge/discharge MOS status
    daly_bms_get_discharge_charge_mos_status(handle);

    // Update peak values
    daly_bms_update_peak_values(handle);

    return true;
}

// Get pack measurements (V, I, SOC)
bool daly_bms_get_pack_measurements(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_VOUT_IOUT_SOC);

    if (daly_bms_receive_bytes(handle)) {
        // Parse voltage (0.1V scale)
        uint16_t voltage_raw = (handle->rx_buffer[4] << 8) | handle->rx_buffer[5];
        handle->data.packVoltage = (float)voltage_raw / 10.0f;

        // Parse current (0.1A scale, signed)
        int16_t current_raw = (handle->rx_buffer[8] << 8) | handle->rx_buffer[9];
        handle->data.packCurrent = (float)current_raw / 10.0f;

        // Parse SOC (1% scale)
        uint16_t soc_raw = (handle->rx_buffer[10] << 8) | handle->rx_buffer[11];
        handle->data.packSOC = (float)soc_raw / 100.0f;

        // Calculate power
        handle->data.power = handle->data.packVoltage * handle->data.packCurrent;

        return true;
    }

    return false;
}

// Get pack temperature
bool daly_bms_get_pack_temp(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_MIN_MAX_TEMPERATURE);

    if (daly_bms_receive_bytes(handle)) {
        // Parse temperatures (°C scale)
        handle->data.tempMax = (int8_t)handle->rx_buffer[4];
        handle->data.tempMin = (int8_t)handle->rx_buffer[6];
        handle->data.tempAverage = (float)(handle->data.tempMax + handle->data.tempMin) / 2.0f;

        return true;
    }

    return false;
}

// Get min/max cell voltage
bool daly_bms_get_min_max_cell_voltage(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_MIN_MAX_CELL_VOLTAGE);

    if (daly_bms_receive_bytes(handle)) {
        // Parse min/max cell voltages (mV scale)
        uint16_t max_voltage_raw = (handle->rx_buffer[4] << 8) | handle->rx_buffer[5];
        uint16_t min_voltage_raw = (handle->rx_buffer[7] << 8) | handle->rx_buffer[8];

        handle->data.maxCellmV = (float)max_voltage_raw;
        handle->data.minCellmV = (float)min_voltage_raw;
        handle->data.maxCellVNum = handle->rx_buffer[6];
        handle->data.minCellVNum = handle->rx_buffer[9];
        handle->data.cellDiff = handle->data.maxCellmV - handle->data.minCellmV;

        return true;
    }

    return false;
}

// Get status info
bool daly_bms_get_status_info(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_STATUS_INFO);

    if (daly_bms_receive_bytes(handle)) {
        handle->data.numberOfCells = handle->rx_buffer[4];
        handle->data.numOfTempSensors = handle->rx_buffer[5];
        handle->data.chargeState = (handle->rx_buffer[6] == 1);
        handle->data.loadState = (handle->rx_buffer[7] == 1);
        handle->data.bmsCycles = (handle->rx_buffer[10] << 8) | handle->rx_buffer[11];

        return true;
    }

    return false;
}

// Get cell voltages
bool daly_bms_get_cell_voltages(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_CELL_VOLTAGES);

    if (daly_bms_receive_bytes(handle)) {
        // Parse cell voltages (mV scale)
        for (int i = 0; i < 3 && (i * 3 + 3) <= handle->data.numberOfCells; i++) {
            int base_index = 4 + (i * 3);
            if (base_index + 2 < DALY_XFER_BUFFER_LENGTH) {
                uint16_t voltage_raw = (handle->rx_buffer[base_index] << 8) | handle->rx_buffer[base_index + 1];
                handle->data.cellVmV[i * 3] = (float)voltage_raw;
                voltage_raw = (handle->rx_buffer[base_index + 1] << 8) | handle->rx_buffer[base_index + 2];
                handle->data.cellVmV[i * 3 + 1] = (float)voltage_raw;
                voltage_raw = (handle->rx_buffer[base_index + 2] << 8) | handle->rx_buffer[base_index + 3];
                handle->data.cellVmV[i * 3 + 2] = (float)voltage_raw;
            }
        }

        return true;
    }

    return false;
}

// Get cell temperatures
bool daly_bms_get_cell_temperature(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_CELL_TEMPERATURE);

    if (daly_bms_receive_bytes(handle)) {
        // Parse cell temperatures
        for (int i = 0; i < 7 && i < handle->data.numOfTempSensors; i++) {
            handle->data.cellTemperature[i] = (int8_t)handle->rx_buffer[4 + i];
        }

        return true;
    }

    return false;
}

// Get cell balance state
bool daly_bms_get_cell_balance_state(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_CELL_BALANCE_STATE);

    if (daly_bms_receive_bytes(handle)) {
        // Parse balance states
        uint32_t balance_raw = (handle->rx_buffer[4] << 24) | (handle->rx_buffer[5] << 16) |
                              (handle->rx_buffer[6] << 8) | handle->rx_buffer[7];

        for (int i = 0; i < 32 && i < handle->data.numberOfCells; i++) {
            handle->data.cellBalanceState[i] = (balance_raw & (1 << i)) != 0;
        }

        handle->data.cellBalanceActive = (balance_raw != 0);

        return true;
    }

    return false;
}

// Get failure codes
bool daly_bms_get_failure_codes(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_FAILURE_CODES);

    if (daly_bms_receive_bytes(handle)) {
        // Parse alarm/failure codes
        uint8_t alarm_byte = handle->rx_buffer[4];
        handle->alarm.levelOneCellVoltageTooHigh = (alarm_byte & 0x01) != 0;
        handle->alarm.levelTwoCellVoltageTooHigh = (alarm_byte & 0x02) != 0;
        handle->alarm.levelOneCellVoltageTooLow = (alarm_byte & 0x04) != 0;
        handle->alarm.levelTwoCellVoltageTooLow = (alarm_byte & 0x08) != 0;
        handle->alarm.levelOnePackVoltageTooHigh = (alarm_byte & 0x10) != 0;
        handle->alarm.levelTwoPackVoltageTooHigh = (alarm_byte & 0x20) != 0;
        handle->alarm.levelOnePackVoltageTooLow = (alarm_byte & 0x40) != 0;
        handle->alarm.levelTwoPackVoltageTooLow = (alarm_byte & 0x80) != 0;

        alarm_byte = handle->rx_buffer[5];
        handle->alarm.levelOneChargeTempTooHigh = (alarm_byte & 0x01) != 0;
        handle->alarm.levelTwoChargeTempTooHigh = (alarm_byte & 0x02) != 0;
        handle->alarm.levelOneChargeTempTooLow = (alarm_byte & 0x04) != 0;
        handle->alarm.levelTwoChargeTempTooLow = (alarm_byte & 0x08) != 0;
        handle->alarm.levelOneDischargeTempTooHigh = (alarm_byte & 0x10) != 0;
        handle->alarm.levelTwoDischargeTempTooHigh = (alarm_byte & 0x20) != 0;
        handle->alarm.levelOneDischargeTempTooLow = (alarm_byte & 0x40) != 0;
        handle->alarm.levelTwoDischargeTempTooLow = (alarm_byte & 0x80) != 0;

        // Continue parsing other alarm bytes...
        // (Implementation would continue for all alarm bytes)

        return true;
    }

    return false;
}

// Set discharge MOS state
bool daly_bms_set_discharge_mos(daly_bms_handle_t* handle, bool sw) {
    if (!handle) {
        return false;
    }

    // Pre-load transmit buffer
    memset(&handle->tx_buffer[3], 0, 9);
    handle->tx_buffer[3] = sw ? 0x01 : 0x00;

    daly_bms_send_command(handle, DALY_CMD_DISCHRG_FET);

    // No response expected for this command
    vTaskDelay(pdMS_TO_TICKS(100));

    return true;
}

// Set charge MOS state
bool daly_bms_set_charge_mos(daly_bms_handle_t* handle, bool sw) {
    if (!handle) {
        return false;
    }

    // Pre-load transmit buffer
    memset(&handle->tx_buffer[3], 0, 9);
    handle->tx_buffer[3] = sw ? 0x01 : 0x00;

    daly_bms_send_command(handle, DALY_CMD_CHRG_FET);

    // No response expected for this command
    vTaskDelay(pdMS_TO_TICKS(100));

    return true;
}

// Get charge/discharge MOS status
bool daly_bms_get_discharge_charge_mos_status(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_DISCHARGE_CHARGE_MOS_STATUS);

    if (daly_bms_receive_bytes(handle)) {
        handle->data.chargeFetState = (handle->rx_buffer[4] == 1);
        handle->data.disChargeFetState = (handle->rx_buffer[5] == 1);
        handle->data.bmsHeartBeat = handle->rx_buffer[6];

        uint16_t capacity_raw = (handle->rx_buffer[8] << 8) | handle->rx_buffer[9];
        handle->data.resCapacitymAh = (int)capacity_raw;

        return true;
    }

    return false;
}

// Reset BMS
bool daly_bms_reset(daly_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    daly_bms_send_command(handle, DALY_CMD_BMS_RESET);

    // No response expected for this command
    vTaskDelay(pdMS_TO_TICKS(1000));

    return true;
}
