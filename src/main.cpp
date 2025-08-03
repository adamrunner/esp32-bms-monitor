#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rmt_led_strip.hpp"
#include "esp_log.h"

using namespace htcw;

static const char *TAG = "ws2812_example";

// WS2812 LED on GPIO8
ws2812 led_strip_instance(8, 1);

extern "C" void app_main(void)
{
    // Initialize console output
    esp_log_level_set(TAG, ESP_LOG_INFO);
    printf("WS2812 LED Blink Example\n");
    ESP_LOGI(TAG, "Starting WS2812 LED blink example");

    // Initialize the LED strip
    if (!led_strip_instance.initialize()) {
        printf("Failed to initialize LED strip\n");
        ESP_LOGE(TAG, "Failed to initialize LED strip");
        return;
    }

    printf("LED strip initialized successfully\n");
    ESP_LOGI(TAG, "LED strip initialized successfully");

    // Colors to cycle through
    const char* color_names[] = {"Red", "Green", "Blue", "White", "Off"};
    uint32_t colors[] = {
        0xFF0000, // Red
        0x00FF00, // Green
        0x0000FF, // Blue
        0xFFFFFF, // White
        0x000000  // Off
    };

    int num_colors = sizeof(colors) / sizeof(colors[0]);

    ESP_LOGI(TAG, "Starting color cycle loop");

    while (1) {
        for (int i = 0; i < num_colors; i++) {
            led_strip_instance.color(0, colors[i]);
            led_strip_instance.update();
            ESP_LOGI(TAG, "Setting color: %s", color_names[i]);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}
