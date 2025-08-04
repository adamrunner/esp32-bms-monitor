# ESP32 BMS Monitor Project Plan

## Project Overview
ESP32-based Battery Management System (BMS) monitor supporting multiple BMS types with abstraction layer architecture. Initially supporting Daly BMS and JBD BMS units.

## Architecture

### 1. Core Abstraction Layer

**Base Interface (`include/bms_interface.h`):**
```cpp
class BMSInterface {
public:
    virtual ~BMSInterface() = default;
    
    // Core measurements
    virtual bool readMeasurements() = 0;
    virtual float getPackVoltage() const = 0;
    virtual float getPackCurrent() const = 0;
    virtual float getStateOfCharge() const = 0;
    virtual float getPower() const = 0;  // Calculated: V * I
    
    // Cell data
    virtual int getCellCount() const = 0;
    virtual float getCellVoltage(int cell) const = 0;
    virtual float getMinCellVoltage() const = 0;
    virtual float getMaxCellVoltage() const = 0;
    virtual int getMinCellNumber() const = 0;
    virtual int getMaxCellNumber() const = 0;
    
    // Temperature data
    virtual int getTemperatureCount() const = 0;
    virtual float getTemperature(int sensor) const = 0;
    virtual float getMaxTemperature() const = 0;
    virtual float getMinTemperature() const = 0;
    
    // Peak tracking
    virtual float getPeakCurrent() const = 0;
    virtual float getPeakPower() const = 0;
    
    // Status and alarms
    virtual bool isChargingEnabled() const = 0;
    virtual bool isDischargingEnabled() const = 0;
    
protected:
    mutable float peakCurrent = 0;
    mutable float peakPower = 0;
};
```

### 2. Implementation Strategy

**Daly BMS Implementation (`lib/daly_bms/`):**
- Adapt Arduino C++ library for ESP-IDF
- Convert HardwareSerial to ESP-IDF UART driver
- Maintain same data structures and command set
- Add peak tracking functionality

**JBD BMS Implementation (`lib/jbd_bms/`):**
- Implement packet protocol from scratch
- Handle CRC calculations
- Support UART interface (CAN bus support optional)
- Extract data parsing logic from reference implementation

### 3. Key Features

**Core Monitoring Features:**
- Peak current measurement tracking
- Real-time current measurement
- Overall pack voltage measurement
- Individual cell voltage measurements
- Power (wattage) calculation: voltage × current
- Peak power tracking
- Temperature sensor readings
- MOSFET state monitoring (charge/discharge enable)
- Cell balancing status
- Protection alarm detection

**Enhanced Features:**
- Cell voltage imbalance detection
- Temperature gradient monitoring
- Moving average filters for stable readings
- Charge/discharge cycle counting
- Residual capacity tracking
- BMS firmware/version information

**Communication Robustness:**
- Retry mechanisms for failed communications
- Timeout handling
- Checksum/CRC validation
- Error recovery procedures

### 4. Data Logging Structure (Future SD Card Implementation)

```cpp
struct BMSLogEntry {
    uint32_t timestamp;
    float packVoltage;
    float packCurrent;
    float packSOC;
    float power;
    float temperatures[16];
    float cellVoltages[48];
    uint32_t balanceState;
    uint16_t alarmFlags;
};
```

### 5. Main Application Flow

The `src/main.cpp` will:
1. (Future) Auto-detect connected BMS type
2. Initialize appropriate BMS driver
3. Configure serial output for monitoring
4. Implement main polling loop
5. Display real-time data
6. Track peaks and statistics
7. (Future) Log data to SD card

## Reference Implementations

### Daly BMS (Arduino C++ library):
- **Protocol**: Simple request/response over UART
- **Key Commands**: 
  - `0x90` - Voltage, Current, SOC
  - `0x91` - Min/Max cell voltage
  - `0x92` - Temperature readings
  - `0x95` - Individual cell voltages
  - `0x96` - Cell temperatures
  - `0x98` - Failure/alarm codes
- **Data Structure**: Comprehensive struct with all BMS parameters
- **Communication**: 13-byte packets with checksum validation

### JBD BMS (Linux C program):
- **Protocol**: Packet structure with CRC: `0xDD` [action] [register] [length] [data] [CRC] `0x77`
- **Key Features**:
  - Packet format handling
  - CRC calculations and validation
  - CAN bus support (optional for ESP32)
- **Commands**: Read/write registers for different data types
- **Data Handling**: Float conversions and bit manipulation for protection flags

## Implementation Status
- [x] Create project architecture files
- [x] Implement BMS abstraction interface
- [x] Adapt Daly BMS library for ESP-IDF
- [x] Implement JBD BMS protocol
- [x] Create main application loop
- [x] Implement peak tracking functionality
- [x] Add auto-detection of BMS type
- [x] Configure serial output for monitoring
- [x] Test with actual BMS hardware
- [ ] Figure out state of charge for JBD BMS
- [ ] (Future) Implement BMS sniffing to automatically detect connected BMS type
- [ ] (Future) Implement SD card data logging
- [ ] (Future) Add display support for new ESP32 module

## Next Steps
1. Create header files for BMS interface
2. Set up Daly BMS library adaptation
3. Implement JBD protocol from reference code
4. Build main application structure
