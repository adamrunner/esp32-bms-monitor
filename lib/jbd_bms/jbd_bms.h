#ifndef JBD_BMS_H
#define JBD_BMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "driver/uart.h"
#include "bms_interface.h"

// JBD BMS specific definitions
#define JBD_BMS_UART_PORT UART_NUM_1
#define JBD_BMS_RX_PIN 16
#define JBD_BMS_TX_PIN 17
#define JBD_BMS_BAUD_RATE 9600
#define JBD_XFER_BUFFER_LENGTH 256
#define JBD_MAX_CELLS 48
#define JBD_MAX_TEMP_SENSORS 16
#define JBD_PKT_START 0xDD
#define JBD_PKT_END 0x77
#define JBD_CMD_READ 0xA5
#define JBD_CMD_WRITE 0x5A

// JBD BMS commands
typedef enum {
    JBD_CMD_HWINFO = 0x03,
    JBD_CMD_CELLINFO = 0x04,
    JBD_CMD_HWVER = 0x05,
    JBD_CMD_MOS = 0xE1
} jbd_command_t;

// JBD BMS protection flags
typedef struct {
    unsigned sover: 1;      // Single overvoltage protection
    unsigned sunder: 1;     // Single undervoltage protection
    unsigned gover: 1;      // Whole group overvoltage protection
    unsigned gunder: 1;     // Whole group undervoltage protection
    unsigned chitemp: 1;    // Charge over temperature protection
    unsigned clowtemp: 1;   // Charge low temperature protection
    unsigned dhitemp: 1;    // Discharge over temperature protection
    unsigned dlowtemp: 1;   // Discharge low temperature protection
    unsigned cover: 1;      // Charge overcurrent protection
    unsigned cunder: 1;     // Discharge overcurrent protection
    unsigned shorted: 1;    // Short circuit protection
    unsigned ic: 1;         // Front detection IC error
    unsigned mos: 1;        // Software lock MOS
} jbd_protect_t;

// JBD BMS data structure
typedef struct {
    float packVoltage;
    float packCurrent;
    float packSOC;          // Will be calculated from capacity
    float power;
    float capacity;         // Residual capacity
    int cellCount;
    float cellVoltages[JBD_MAX_CELLS];
    int temperatureCount;
    float temperatures[JBD_MAX_TEMP_SENSORS];
    float minCellVoltage;
    float maxCellVoltage;
    int minCellNumber;
    int maxCellNumber;
    float maxTemperature;
    float minTemperature;
    uint32_t balanceBits;
    bool chargingEnabled;
    bool dischargingEnabled;
    bool balancingActive;
    int chargeCycles;

    // Peak tracking
    float peakCurrent;
    float peakPower;

    // Protection status
    jbd_protect_t protection;
} jbd_bms_data_t;

// JBD BMS handle structure
typedef struct {
    uart_port_t uart_port;
    jbd_bms_data_t data;
    uint8_t tx_buffer[JBD_XFER_BUFFER_LENGTH];
    uint8_t rx_buffer[JBD_XFER_BUFFER_LENGTH];
} jbd_bms_handle_t;

// Function prototypes
bms_interface_t* jbd_bms_create(uart_port_t uart_port, int rx_pin, int tx_pin);
void jbd_bms_destroy(bms_interface_t* bms_interface);
bool jbd_bms_init(jbd_bms_handle_t* handle);
bool jbd_bms_update(jbd_bms_handle_t* handle);
bool jbd_bms_read_data(jbd_bms_handle_t* handle);

// Internal functions

#ifdef __cplusplus
}
#endif

#endif // JBD_BMS_H
