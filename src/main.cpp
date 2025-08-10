#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdint.h>
#include "display.h"

static const char *TAG = "bms_monitor";


extern "C" void app_main(void)
{
#ifndef SERIAL_DISABLED
    ESP_LOGI(TAG, "Starting BMS Monitor Application");
#endif

    display_init();

    uart_config_t cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_0, &cfg);
    uart_set_pin(UART_NUM_0, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);

    static char line[128];
    int idx = 0;

    while (1) {
        uint8_t ch;
        int n = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(100));
        if (n == 1) {
            if (ch == '\n' || idx >= (int)sizeof(line) - 1) {
                line[idx] = 0;
                display_update_basic(0, 0, 0, 0, 0);
                // Simple echo of last NMEA line at top
                // Using display to print line
                // Overwrite the line content area
                // For simplicity reuse display_update_basic and then print line header
                // Draw line
                // Minimal flicker approach
                // Print line at y=120
                extern void display_print_line(const char* s);
                display_print_line(line);
                idx = 0;
            } else if (ch != '\r') {
                line[idx++] = (char)ch;
            }
        }
    }
}
