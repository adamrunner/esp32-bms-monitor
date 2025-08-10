# ESP32-32E Pin Allocation for E32R35T/E32N35T Display Module

## Hardware Overview
- **Module**: 3.5-inch ESP32-32E display module 
- **Resolution**: 320x480
- **Display Driver**: ST7796
- **MCU**: ESP32-WROOM-32E (240MHz, 2.4G WiFi + Bluetooth)

## Pin Allocation Table

### LCD Display (ST7796)
| Function | GPIO Pin | Direction | Description |
|----------|----------|-----------|-------------|
| TFT_CS   | IO15     | Output    | LCD chip select (active low) |
| TFT_RS   | IO2      | Output    | Command/Data select (High=data, Low=command) |
| TFT_SCK  | IO14     | Output    | SPI clock (shared with touch) |
| TFT_MOSI | IO13     | Output    | SPI MOSI (shared with touch) |
| TFT_MISO | IO12     | Input     | SPI MISO (shared with touch) |
| TFT_RST  | EN       | Output    | Reset signal (active low, shared with ESP32 reset) |
| TFT_BL   | IO27     | Output    | Backlight control (High=on, Low=off) |

### Resistive Touch Panel (RTP)
| Function | GPIO Pin | Direction | Description |
|----------|----------|-----------|-------------|
| TP_SCK   | IO14     | Output    | SPI clock (shared with LCD) |
| TP_DIN   | IO13     | Output    | SPI MOSI (shared with LCD) |
| TP_DOUT  | IO12     | Input     | SPI MISO (shared with LCD) |
| TP_CS    | IO33     | Output    | Touch chip select (active low) |
| TP_IRQ   | IO36     | Input     | Touch interrupt (active low) |

### RGB LED (Common Anode)
| Function   | GPIO Pin | Direction | Description |
|------------|----------|-----------|-------------|
| LED_RED    | IO22     | Output    | Red LED (active low) |
| LED_GREEN  | IO16     | Output    | Green LED (active low) |
| LED_BLUE   | IO17     | Output    | Blue LED (active low) |

### SD Card
| Function | GPIO Pin | Direction | Description |
|----------|----------|-----------|-------------|
| SD_CS    | IO5      | Output    | SD card chip select (active low) |
| SD_MOSI  | IO23     | Output    | SD card SPI MOSI |
| SD_SCK   | IO18     | Output    | SD card SPI clock |
| SD_MISO  | IO19     | Input     | SD card SPI MISO |

### Audio
| Function      | GPIO Pin | Direction | Description |
|---------------|----------|-----------|-------------|
| Audio_ENABLE  | IO4      | Output    | Audio amplifier enable (active low) |
| Audio_DAC     | IO26     | Output    | Audio DAC output |

### Battery Monitoring
| Function | GPIO Pin | Direction | Description |
|----------|----------|-----------|-------------|
| BAT_ADC  | IO34     | Input     | Battery voltage ADC (analog input) |

### User Input
| Function  | GPIO Pin | Direction | Description |
|-----------|----------|-----------|-------------|
| BOOT_KEY  | IO0      | Input     | Boot/Download button (active low) |
| RESET_KEY | EN       | Input     | Reset button (active low, shared with LCD reset) |

### Serial Communication
| Function | GPIO Pin | Direction | Description |
|----------|----------|-----------|-------------|
| RX0      | RXD0     | Input     | UART0 receive |
| TX0      | TXD0     | Output    | UART0 transmit |

### Power
| Function        | Pin | Description |
|-----------------|-----|-------------|
| TYPE-C_POWER    | -   | 5V power input via USB-C |

## SPI Bus Configuration

### Display & Touch (Shared SPI Bus)
- **SCK**: GPIO14
- **MOSI**: GPIO13  
- **MISO**: GPIO12
- **LCD CS**: GPIO15
- **Touch CS**: GPIO33

### SD Card (Separate SPI Bus)
- **SCK**: GPIO18
- **MOSI**: GPIO23
- **MISO**: GPIO19
- **CS**: GPIO5

## Important Notes for Firmware Development

1. **Shared SPI Bus**: LCD and touch screen share the same SPI bus (GPIO14, 13, 12)
2. **Active Low Signals**: Most chip selects and control signals are active low
3. **RGB LED**: Common anode configuration - use LOW to turn on LEDs
4. **Touch Interrupt**: GPIO36 is input-only, suitable for interrupt handling
5. **Battery Monitoring**: GPIO34 is ADC1_CH6, input-only pin
6. **Audio**: GPIO26 is DAC2 output pin
7. **Reset Sharing**: EN pin controls both ESP32 and LCD reset

## Arduino Pin Definitions Example
```cpp
// LCD pins
#define TFT_CS    15
#define TFT_DC    2   // Data/Command (same as TFT_RS)
#define TFT_MOSI  13
#define TFT_MISO  12
#define TFT_SCLK  14
#define TFT_RST   -1  // Connected to EN pin
#define TFT_BL    27

// Touch pins
#define TOUCH_CS  33
#define TOUCH_IRQ 36

// RGB LED pins (active low)
#define LED_R     22
#define LED_G     16
#define LED_B     17

// SD Card pins
#define SD_CS     5
#define SD_MOSI   23
#define SD_MISO   19
#define SD_SCK    18

// Audio pins
#define AUDIO_EN  4
#define AUDIO_OUT 26

// Other pins
#define BOOT_BTN  0
#define BAT_ADC   34
```