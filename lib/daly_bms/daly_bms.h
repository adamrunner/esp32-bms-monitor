#ifndef DALY_BMS_H
#define DALY_BMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <driver/uart.h>
#include "bms_interface.h"

// Daly BMS specific definitions
#define DALY_BMS_UART_PORT UART_NUM_1
#define DALY_BMS_RX_PIN 16
#define DALY_BMS_TX_PIN 17
#define DALY_BMS_BAUD_RATE 9600
#define DALY_XFER_BUFFER_LENGTH 13
#define DALY_MAX_NUMBER_CELLS 48
#define DALY_MAX_NUMBER_TEMP_SENSORS 16

// Daly BMS commands
typedef enum {
    DALY_CMD_VOUT_IOUT_SOC = 0x90,
    DALY_CMD_MIN_MAX_CELL_VOLTAGE = 0x91,
    DALY_CMD_MIN_MAX_TEMPERATURE = 0x92,
    DALY_CMD_DISCHARGE_CHARGE_MOS_STATUS = 0x93,
    DALY_CMD_STATUS_INFO = 0x94,
    DALY_CMD_CELL_VOLTAGES = 0x95,
    DALY_CMD_CELL_TEMPERATURE = 0x96,
    DALY_CMD_CELL_BALANCE_STATE = 0x97,
    DALY_CMD_FAILURE_CODES = 0x98,
    DALY_CMD_DISCHRG_FET = 0xD9,
    DALY_CMD_CHRG_FET = 0xDA,
    DALY_CMD_BMS_RESET = 0x00,
} daly_command_t;

// Daly BMS data structure
typedef struct {
    // data from 0x90
    float packVoltage; // Total pack voltage (0.1 V)
    float packCurrent; // Current in (+) or out (-) of pack (0.1 A)
    float packSOC;     // State Of Charge
    float power;       // Calculated power (W)

    // data from 0x91
    float maxCellmV; // Maximum cell voltage (mV)
    int maxCellVNum; // Number of cell with highest voltage
    float minCellmV; // Minimum cell voltage (mV)
    int minCellVNum; // Number of cell with lowest voltage
    float cellDiff;  // Difference between min and max cell voltages

    // data from 0x92
    int tempMax;       // Maximum temperature sensor reading (°C)
    int tempMin;       // Minimum temperature sensor reading (°C)
    float tempAverage; // Average of temp sensors

    // data from 0x93
    int chargeDischargeStatus; // charge/discharge status (0 stationary, 1 charge, 2 discharge)
    bool chargeFetState;       // charging MOSFET status
    bool disChargeFetState;    // discharge MOSFET state
    int bmsHeartBeat;          // BMS life (0~255 cycles)?
    int resCapacitymAh;        // residual capacity mAH

    // data from 0x94
    int numberOfCells;    // Cell count
    int numOfTempSensors; // Temp sensor count
    bool chargeState;     // charger status 0 = disconnected 1 = connected
    bool loadState;       // Load Status 0=disconnected 1=connected
    bool dIO[8];          // No information about this
    int bmsCycles;        // charge / discharge cycles

    // data from 0x95
    float cellVmV[DALY_MAX_NUMBER_CELLS]; // Store Cell Voltages (mV)

    // data from 0x96
    int cellTemperature[DALY_MAX_NUMBER_TEMP_SENSORS]; // array of cell Temperature sensors

    // data from 0x97
    bool cellBalanceState[DALY_MAX_NUMBER_CELLS]; // bool array of cell balance states
    bool cellBalanceActive;                       // bool is cell balance active

    // peak tracking
    float peakCurrent;
    float peakPower;
} daly_bms_data_t;

// Daly BMS alarm structure
typedef struct {
    // data from 0x98
    /* 0x00 */
    bool levelOneCellVoltageTooHigh;
    bool levelTwoCellVoltageTooHigh;
    bool levelOneCellVoltageTooLow;
    bool levelTwoCellVoltageTooLow;
    bool levelOnePackVoltageTooHigh;
    bool levelTwoPackVoltageTooHigh;
    bool levelOnePackVoltageTooLow;
    bool levelTwoPackVoltageTooLow;

    /* 0x01 */
    bool levelOneChargeTempTooHigh;
    bool levelTwoChargeTempTooHigh;
    bool levelOneChargeTempTooLow;
    bool levelTwoChargeTempTooLow;
    bool levelOneDischargeTempTooHigh;
    bool levelTwoDischargeTempTooHigh;
    bool levelOneDischargeTempTooLow;
    bool levelTwoDischargeTempTooLow;

    /* 0x02 */
    bool levelOneChargeCurrentTooHigh;
    bool levelTwoChargeCurrentTooHigh;
    bool levelOneDischargeCurrentTooHigh;
    bool levelTwoDischargeCurrentTooHigh;
    bool levelOneStateOfChargeTooHigh;
    bool levelTwoStateOfChargeTooHigh;
    bool levelOneStateOfChargeTooLow;
    bool levelTwoStateOfChargeTooLow;

    /* 0x03 */
    bool levelOneCellVoltageDifferenceTooHigh;
    bool levelTwoCellVoltageDifferenceTooHigh;
    bool levelOneTempSensorDifferenceTooHigh;
    bool levelTwoTempSensorDifferenceTooHigh;

    /* 0x04 */
    bool chargeFETTemperatureTooHigh;
    bool dischargeFETTemperatureTooHigh;
    bool failureOfChargeFETTemperatureSensor;
    bool failureOfDischargeFETTemperatureSensor;
    bool failureOfChargeFETAdhesion;
    bool failureOfDischargeFETAdhesion;
    bool failureOfChargeFETTBreaker;
    bool failureOfDischargeFETBreaker;

    /* 0x05 */
    bool failureOfAFEAcquisitionModule;
    bool failureOfVoltageSensorModule;
    bool failureOfTemperatureSensorModule;
    bool failureOfEEPROMStorageModule;
    bool failureOfRealtimeClockModule;
    bool failureOfPrechargeModule;
    bool failureOfVehicleCommunicationModule;
    bool failureOfIntranetCommunicationModule;

    /* 0x06 */
    bool failureOfCurrentSensorModule;
    bool failureOfMainVoltageSensorModule;
    bool failureOfShortCircuitProtection;
    bool failureOfLowVoltageNoCharging;
} daly_bms_alarm_t;

// Daly BMS handle structure
typedef struct {
    uart_port_t uart_port;
    daly_bms_data_t data;
    daly_bms_alarm_t alarm;
    uint8_t tx_buffer[DALY_XFER_BUFFER_LENGTH];
    uint8_t rx_buffer[DALY_XFER_BUFFER_LENGTH];
} daly_bms_handle_t;

// Function prototypes
bms_interface_t* daly_bms_create(uart_port_t uart_port, int rx_pin, int tx_pin);
void daly_bms_destroy(bms_interface_t* bms_interface);
bool daly_bms_init(daly_bms_handle_t* handle);
bool daly_bms_update(daly_bms_handle_t* handle);
bool daly_bms_get_pack_measurements(daly_bms_handle_t* handle);
bool daly_bms_get_pack_temp(daly_bms_handle_t* handle);
bool daly_bms_get_min_max_cell_voltage(daly_bms_handle_t* handle);
bool daly_bms_get_status_info(daly_bms_handle_t* handle);
bool daly_bms_get_cell_voltages(daly_bms_handle_t* handle);
bool daly_bms_get_cell_temperature(daly_bms_handle_t* handle);
bool daly_bms_get_cell_balance_state(daly_bms_handle_t* handle);
bool daly_bms_get_failure_codes(daly_bms_handle_t* handle);
bool daly_bms_set_discharge_mos(daly_bms_handle_t* handle, bool sw);
bool daly_bms_set_charge_mos(daly_bms_handle_t* handle, bool sw);
bool daly_bms_get_discharge_charge_mos_status(daly_bms_handle_t* handle);
bool daly_bms_reset(daly_bms_handle_t* handle);

// Internal functions
void daly_bms_send_command(daly_bms_handle_t* handle, daly_command_t cmd_id);
bool daly_bms_receive_bytes(daly_bms_handle_t* handle);
bool daly_bms_validate_checksum(daly_bms_handle_t* handle);
void daly_bms_update_peak_values(daly_bms_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif // DALY_BMS_H
