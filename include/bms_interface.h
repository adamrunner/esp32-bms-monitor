#ifndef BMS_INTERFACE_H
#define BMS_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// BMS data structure
typedef struct {
    float packVoltage;
    float packCurrent;
    float packSOC;
    float power;
    int cellCount;
    float* cellVoltages;
    int temperatureCount;
    float* temperatures;
    float minCellVoltage;
    float maxCellVoltage;
    int minCellNumber;
    int maxCellNumber;
    float maxTemperature;
    float minTemperature;
    float peakCurrent;
    float peakPower;
    bool chargingEnabled;
    bool dischargingEnabled;
    float cellVoltageDelta;  // Cell voltage difference (max - min)
} bms_data_t;

// BMS Interface function pointer types
typedef bool (*bms_read_measurements_func_t)(void* bms_handle);
typedef float (*bms_get_pack_voltage_func_t)(void* bms_handle);
typedef float (*bms_get_pack_current_func_t)(void* bms_handle);
typedef float (*bms_get_soc_func_t)(void* bms_handle);
typedef float (*bms_get_power_func_t)(void* bms_handle);
typedef float (*bms_get_full_capacity_func_t)(void* bms_handle);
typedef int (*bms_get_cell_count_func_t)(void* bms_handle);
typedef float (*bms_get_cell_voltage_func_t)(void* bms_handle, int cell);
typedef float (*bms_get_min_cell_voltage_func_t)(void* bms_handle);
typedef float (*bms_get_max_cell_voltage_func_t)(void* bms_handle);
typedef int (*bms_get_min_cell_number_func_t)(void* bms_handle);
typedef int (*bms_get_max_cell_number_func_t)(void* bms_handle);
typedef int (*bms_get_temperature_count_func_t)(void* bms_handle);
typedef float (*bms_get_temperature_func_t)(void* bms_handle, int sensor);
typedef float (*bms_get_max_temperature_func_t)(void* bms_handle);
typedef float (*bms_get_min_temperature_func_t)(void* bms_handle);
typedef float (*bms_get_peak_current_func_t)(void* bms_handle);
typedef float (*bms_get_peak_power_func_t)(void* bms_handle);
typedef bool (*bms_is_charging_enabled_func_t)(void* bms_handle);
typedef bool (*bms_is_discharging_enabled_func_t)(void* bms_handle);
typedef float (*bms_get_cell_voltage_delta_func_t)(void* bms_handle);

// BMS Interface structure
typedef struct {
    void* handle;
    bms_read_measurements_func_t readMeasurements;
    bms_get_pack_voltage_func_t getPackVoltage;
    bms_get_pack_current_func_t getPackCurrent;
    bms_get_soc_func_t getStateOfCharge;
    bms_get_power_func_t getPower;
    bms_get_full_capacity_func_t getFullCapacity;
    bms_get_cell_count_func_t getCellCount;
    bms_get_cell_voltage_func_t getCellVoltage;
    bms_get_min_cell_voltage_func_t getMinCellVoltage;
    bms_get_max_cell_voltage_func_t getMaxCellVoltage;
    bms_get_min_cell_number_func_t getMinCellNumber;
    bms_get_max_cell_number_func_t getMaxCellNumber;
    bms_get_temperature_count_func_t getTemperatureCount;
    bms_get_temperature_func_t getTemperature;
    bms_get_max_temperature_func_t getMaxTemperature;
    bms_get_min_temperature_func_t getMinTemperature;
    bms_get_peak_current_func_t getPeakCurrent;
    bms_get_peak_power_func_t getPeakPower;
    bms_is_charging_enabled_func_t isChargingEnabled;
    bms_is_discharging_enabled_func_t isDischargingEnabled;
    bms_get_cell_voltage_delta_func_t getCellVoltageDelta;
} bms_interface_t;

// BMS type enumeration
typedef enum {
    BMS_TYPE_UNKNOWN = 0,
    BMS_TYPE_DALY,
    BMS_TYPE_JBD
} bms_type_t;

// Function prototypes
bms_type_t detect_bms_type();
bms_interface_t* create_daly_bms();
bms_interface_t* create_jbd_bms();

#ifdef __cplusplus
}
#endif

#endif // BMS_INTERFACE_H
