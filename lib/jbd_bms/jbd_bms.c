#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "jbd_bms.h"

static const char *TAG = "jbd_bms";

// Helper macros
#define _getshort(p) ((int16_t)((*((p)) << 8) | *((p)+1)))
#define _getushort(p) ((uint16_t)((*((p)) << 8) | *((p)+1)))
#define _putshort(p,v) { int16_t tmp = (int16_t)(v); *((p)) = (tmp >> 8) & 0xFF; *((p)+1) = tmp & 0xFF; }

// BMS Interface function implementations
static bool jbd_bms_read_measurements(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return jbd_bms_update(handle);
}

static float jbd_bms_get_pack_voltage(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.packVoltage;
}

static float jbd_bms_get_pack_current(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.packCurrent;
}

static float jbd_bms_get_soc(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.packSOC;
}

static float jbd_bms_get_power(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.power;
}

static float jbd_bms_get_full_capacity(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.fullCapacity;
}

static int jbd_bms_get_cell_count(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.cellCount;
}

static float jbd_bms_get_cell_voltage(void* bms_handle, int cell) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    if (cell >= 0 && cell < handle->data.cellCount) {
        return handle->data.cellVoltages[cell];
    }
    return 0.0f;
}

static float jbd_bms_get_min_cell_voltage(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.minCellVoltage;
}

static float jbd_bms_get_max_cell_voltage(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.maxCellVoltage;
}

static int jbd_bms_get_min_cell_number(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.minCellNumber;
}

static int jbd_bms_get_max_cell_number(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.maxCellNumber;
}

static int jbd_bms_get_temperature_count(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.temperatureCount;
}

static float jbd_bms_get_temperature(void* bms_handle, int sensor) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    if (sensor >= 0 && sensor < handle->data.temperatureCount) {
        return handle->data.temperatures[sensor];
    }
    return 0.0f;
}

static float jbd_bms_get_max_temperature(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.maxTemperature;
}

static float jbd_bms_get_min_temperature(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.minTemperature;
}

static float jbd_bms_get_peak_current(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.peakCurrent;
}

static float jbd_bms_get_peak_power(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.peakPower;
}

static bool jbd_bms_is_charging_enabled(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.chargingEnabled;
}

static bool jbd_bms_is_discharging_enabled(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.dischargingEnabled;
}

static float jbd_bms_get_cell_voltage_delta(void* bms_handle) {
    jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_handle;
    return handle->data.maxCellVoltage - handle->data.minCellVoltage;
}

// Create JBD BMS interface
bms_interface_t* jbd_bms_create(uart_port_t uart_port, int rx_pin, int tx_pin) {
    jbd_bms_handle_t* handle = calloc(1, sizeof(jbd_bms_handle_t));
    if (!handle) {
        ESP_LOGE(TAG, "Failed to allocate memory for JBD BMS handle");
        return NULL;
    }

    handle->uart_port = uart_port;

    // Initialize UART
    uart_config_t uart_config = {
        .baud_rate = JBD_BMS_BAUD_RATE,
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
    if (!jbd_bms_init(handle)) {
        ESP_LOGE(TAG, "Failed to initialize JBD BMS");
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
    interface->readMeasurements = jbd_bms_read_measurements;
    interface->getPackVoltage = jbd_bms_get_pack_voltage;
    interface->getPackCurrent = jbd_bms_get_pack_current;
    interface->getStateOfCharge = jbd_bms_get_soc;
    interface->getPower = jbd_bms_get_power;
    interface->getFullCapacity = jbd_bms_get_full_capacity;
    interface->getCellCount = jbd_bms_get_cell_count;
    interface->getCellVoltage = jbd_bms_get_cell_voltage;
    interface->getMinCellVoltage = jbd_bms_get_min_cell_voltage;
    interface->getMaxCellVoltage = jbd_bms_get_max_cell_voltage;
    interface->getMinCellNumber = jbd_bms_get_min_cell_number;
    interface->getMaxCellNumber = jbd_bms_get_max_cell_number;
    interface->getTemperatureCount = jbd_bms_get_temperature_count;
    interface->getTemperature = jbd_bms_get_temperature;
    interface->getMaxTemperature = jbd_bms_get_max_temperature;
    interface->getMinTemperature = jbd_bms_get_min_temperature;
    interface->getPeakCurrent = jbd_bms_get_peak_current;
    interface->getPeakPower = jbd_bms_get_peak_power;
    interface->isChargingEnabled = jbd_bms_is_charging_enabled;
    interface->isDischargingEnabled = jbd_bms_is_discharging_enabled;
    interface->getCellVoltageDelta = jbd_bms_get_cell_voltage_delta;

    ESP_LOGI(TAG, "JBD BMS interface created successfully");
    return interface;
}

// Destroy JBD BMS interface
void jbd_bms_destroy(bms_interface_t* bms_interface) {
    if (bms_interface) {
        if (bms_interface->handle) {
            jbd_bms_handle_t* handle = (jbd_bms_handle_t*)bms_interface->handle;
            uart_driver_delete(handle->uart_port);
            free(handle);
        }
        free(bms_interface);
    }
}

// Initialize JBD BMS
bool jbd_bms_init(jbd_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    // Initialize peak values
    handle->data.peakCurrent = 0.0f;
    handle->data.peakPower = 0.0f;

    ESP_LOGI(TAG, "JBD BMS initialized");
    return true;
}

// Update peak values
static void jbd_update_peak_values(jbd_bms_handle_t* handle) {
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

// Parse protection flags
static void jbd_parse_protection(jbd_bms_handle_t* handle, uint16_t protect_bits) {
    handle->data.protection.sover = (protect_bits & 0x0001) != 0;
    handle->data.protection.sunder = (protect_bits & 0x0002) != 0;
    handle->data.protection.gover = (protect_bits & 0x0004) != 0;
    handle->data.protection.gunder = (protect_bits & 0x0008) != 0;
    handle->data.protection.chitemp = (protect_bits & 0x0010) != 0;
    handle->data.protection.clowtemp = (protect_bits & 0x0020) != 0;
    handle->data.protection.dhitemp = (protect_bits & 0x0040) != 0;
    handle->data.protection.dlowtemp = (protect_bits & 0x0080) != 0;
    handle->data.protection.cover = (protect_bits & 0x0100) != 0;
    handle->data.protection.cunder = (protect_bits & 0x0200) != 0;
    handle->data.protection.shorted = (protect_bits & 0x0400) != 0;
    handle->data.protection.ic = (protect_bits & 0x0800) != 0;
    handle->data.protection.mos = (protect_bits & 0x1000) != 0;
}

// Calculate CRC
static uint16_t jbd_crc(uint8_t *data, int len) {
    uint16_t crc = 0;

    for(int i = 0; i < len; i++) {
        crc -= data[i];
    }

    return crc;
}

// Verify packet
static bool jbd_verify(jbd_bms_handle_t* handle, uint8_t *buf, int len, uint8_t reg) {
    uint16_t my_crc, pkt_crc;
    int i, data_length;

    // Anything less than 7 bytes is an error
    if (len < 7) return false;

    i = 0;
    // 0: Start bit
    if (buf[i++] != JBD_PKT_START) return false;
    // 1: Register
    if (buf[i++] != reg) return false;
    // 2: Status
    i++;
    // 3: Length - must be size of packet minus protocol bytes
    data_length = buf[i++];
    if (data_length != (len - 7)) return false;

    // Data
    my_crc = jbd_crc(&buf[2], data_length + 2);
    i += data_length;

    // CRC
    pkt_crc = _getshort(&buf[i]);
    if (my_crc != pkt_crc) {
        return false;
    }

    i += 2;
    // Stop bit
    if (buf[i++] != JBD_PKT_END) return false;

    return true;
}

// Create command packet
static int jbd_cmd(jbd_bms_handle_t* handle, int action, uint8_t reg, uint8_t *data, int data_len) {
    uint16_t crc;
    int idx;

    // Make sure no data in command for read operations
    if (action == JBD_CMD_READ) data_len = 0;

    memset(handle->tx_buffer, 0, JBD_XFER_BUFFER_LENGTH);
    idx = 0;
    handle->tx_buffer[idx++] = JBD_PKT_START;
    handle->tx_buffer[idx++] = action;
    handle->tx_buffer[idx++] = reg;
    handle->tx_buffer[idx++] = data_len;

    if (idx + data_len > JBD_XFER_BUFFER_LENGTH) return -1;

    if (data && data_len > 0) {
        memcpy(&handle->tx_buffer[idx], data, data_len);
    }

    crc = jbd_crc(&handle->tx_buffer[2], data_len + 2);
    idx += data_len;
    _putshort(&handle->tx_buffer[idx], crc);
    idx += 2;
    handle->tx_buffer[idx++] = JBD_PKT_END;

    return idx;
}

// Parse HWINFO response
static void jbd_parse_hwinfo(jbd_bms_handle_t* handle, uint8_t* data, int len) {
    if (len < 23) return;

    handle->data.packVoltage = (float)_getushort(&data[0]) / 100.0f;
    handle->data.packCurrent = (float)_getshort(&data[2]) / 100.0f;
    handle->data.capacity = (float)_getushort(&data[4]) / 100.0f;
    handle->data.fullCapacity = (float)_getushort(&data[6]) / 100.0f;
    handle->data.pctCapacity = data[19];

    // Use the direct percentage from BMS as SOC
    handle->data.packSOC = (float)handle->data.pctCapacity;

    // Calculate power
    handle->data.power = handle->data.packVoltage * handle->data.packCurrent;

    // Balance bits
    uint16_t balance_low = _getushort(&data[12]);
    uint16_t balance_high = _getushort(&data[14]);
    handle->data.balanceBits = balance_low | ((uint32_t)balance_high << 16);

    // Parse protection flags
    uint16_t protect_bits = _getushort(&data[16]);
    jbd_parse_protection(handle, protect_bits);

    // MOSFET status
    uint8_t fet_bits = data[20];
    handle->data.chargingEnabled = (fet_bits & 0x01) != 0;
    handle->data.dischargingEnabled = (fet_bits & 0x02) != 0;

    // Cell count and temperature count
    handle->data.cellCount = data[21];
    handle->data.temperatureCount = data[22];

    // Parse temperatures and track min/max
    handle->data.minTemperature = 1000.0f;  // Start with high value
    handle->data.maxTemperature = -1000.0f; // Start with low value

    for (int i = 0; i < handle->data.temperatureCount && i < JBD_MAX_TEMP_SENSORS; i++) {
        if (23 + (i * 2) + 1 < len) {
            int16_t temp_raw = _getshort(&data[23 + (i * 2)]);
            handle->data.temperatures[i] = (float)(temp_raw - 2731) / 10.0f;

            // Track min/max temperatures
            if (handle->data.temperatures[i] < handle->data.minTemperature) {
                handle->data.minTemperature = handle->data.temperatures[i];
            }
            if (handle->data.temperatures[i] > handle->data.maxTemperature) {
                handle->data.maxTemperature = handle->data.temperatures[i];
            }
        }
    }
}

// Parse CELLINFO response
static void jbd_parse_cellinfo(jbd_bms_handle_t* handle, uint8_t* data, int len) {
    if (len < handle->data.cellCount * 2) return;

    // Parse cell voltages and find min/max
    handle->data.minCellVoltage = 5.0f;  // Start with high value
    handle->data.maxCellVoltage = 0.0f;  // Start with low value
    handle->data.minCellNumber = 0;
    handle->data.maxCellNumber = 0;

    for (int i = 0; i < handle->data.cellCount && i < JBD_MAX_CELLS; i++) {
        if ((i * 2) + 1 < len) {
            uint16_t voltage_raw = _getushort(&data[i * 2]);
            handle->data.cellVoltages[i] = (float)voltage_raw / 1000.0f;

            // Track min/max
            if (handle->data.cellVoltages[i] < handle->data.minCellVoltage) {
                handle->data.minCellVoltage = handle->data.cellVoltages[i];
                handle->data.minCellNumber = i + 1;  // 1-based indexing
            }
            if (handle->data.cellVoltages[i] > handle->data.maxCellVoltage) {
                handle->data.maxCellVoltage = handle->data.cellVoltages[i];
                handle->data.maxCellNumber = i + 1;  // 1-based indexing
            }
        }
    }
}

// Read data from JBD BMS
bool jbd_bms_read_data(jbd_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    int cmd_len;
    int bytes_read;
    int retries = 3;

    // Read HWINFO
    cmd_len = jbd_cmd(handle, JBD_CMD_READ, JBD_CMD_HWINFO, NULL, 0);
    if (cmd_len < 0) return false;

    uart_write_bytes(handle->uart_port, (const char*)handle->tx_buffer, cmd_len);

    // Read response with retries
    while (retries-- > 0) {
        memset(handle->rx_buffer, 0, JBD_XFER_BUFFER_LENGTH);
        bytes_read = uart_read_bytes(handle->uart_port, handle->rx_buffer, JBD_XFER_BUFFER_LENGTH, pdMS_TO_TICKS(100));

        if (bytes_read > 0 && jbd_verify(handle, handle->rx_buffer, bytes_read, JBD_CMD_HWINFO)) {
            jbd_parse_hwinfo(handle, &handle->rx_buffer[4], handle->rx_buffer[3]);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (retries <= 0) return false;

    // Read CELLINFO
    retries = 3;
    cmd_len = jbd_cmd(handle, JBD_CMD_READ, JBD_CMD_CELLINFO, NULL, 0);
    if (cmd_len < 0) return false;

    uart_write_bytes(handle->uart_port, (const char*)handle->tx_buffer, cmd_len);

    while (retries-- > 0) {
        memset(handle->rx_buffer, 0, JBD_XFER_BUFFER_LENGTH);
        bytes_read = uart_read_bytes(handle->uart_port, handle->rx_buffer, JBD_XFER_BUFFER_LENGTH, pdMS_TO_TICKS(100));

        if (bytes_read > 0 && jbd_verify(handle, handle->rx_buffer, bytes_read, JBD_CMD_CELLINFO)) {
            jbd_parse_cellinfo(handle, &handle->rx_buffer[4], handle->rx_buffer[3]);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return (retries > 0);
}

// Update all JBD BMS data
bool jbd_bms_update(jbd_bms_handle_t* handle) {
    if (!handle) {
        return false;
    }

    // Read all BMS data
    if (!jbd_bms_read_data(handle)) {
        return false;
    }

    // Update peak values
    jbd_update_peak_values(handle);

    return true;
}
