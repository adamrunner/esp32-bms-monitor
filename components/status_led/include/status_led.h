#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  LED_PATTERN_OFF = 0,
  LED_PATTERN_SOLID,
  LED_PATTERN_SLOW_PULSE,   // ~1 Hz
  LED_PATTERN_FAST_BLINK,   // ~5 Hz
  LED_PATTERN_BREATHE,
  LED_PATTERN_RAINBOW       // boot sequence
} led_pattern_t;

typedef struct {
  bool enabled;             // allow disabling via config (no-ops if false)
  int gpio_pin;             // default 8
  uint8_t brightness;       // 0-255 scalar applied to RGB
  bool boot_animation;      // rainbow on boot
  bool critical_override;   // always show critical alerts
  // Deprecated (ignored in low-power badge mode): legacy overlay scheduler fields retained for config compatibility
  bool overlay_enabled;       // Deprecated; ignored
  uint16_t overlay_period_ms; // Deprecated; ignored
  uint16_t overlay_on_ms;     // Deprecated; ignored
} status_led_config_t;

typedef enum {
  STATUS_BOOT_STAGE_BOOT = 0,
  STATUS_BOOT_STAGE_WIFI_CONNECTING,
  STATUS_BOOT_STAGE_BMS_INIT,
  STATUS_BOOT_STAGE_TIME_SYNC
} status_boot_stage_t;

// Minimal Wi-Fi info used by the evaluator (decoupled from wifi_manager)
typedef struct {
  bool connected;
  int8_t rssi;              // dBm (valid only if connected)
} status_led_wifi_t;

// Minimal BMS metrics used by the evaluator
typedef struct {
  bool valid;                    // true when metrics filled
  bool comm_ok;                  // BMS read ok this cycle
  float soc_pct;
  bool charging_enabled;
  bool discharging_enabled;
  float max_temp_c;
  float min_temp_c;
  float cell_delta_v;           // max-min cell voltage
  bool mosfet_fault;            // if available; else leave false
  bool ov_critical;             // optional
  bool uv_critical;             // optional
} bms_led_metrics_t;

/**
 * @brief Initialize the status LED subsystem
 *
 * Creates internal task/queue and initializes the LED driver. If cfg is NULL,
 * sensible defaults are used.
 *
 * @param cfg Configuration (may be NULL)
 * @return ESP_OK on success
 */
esp_err_t status_led_init(const status_led_config_t* cfg);

/**
 * @brief Deinitialize and stop the status LED subsystem
 */
void status_led_deinit(void);

/**
 * @brief Notify boot stage progress
 */
void status_led_notify_boot_stage(status_boot_stage_t stage);

/**
 * @brief Notify Wi-Fi status (call when status changes or periodically)
 */
void status_led_notify_wifi(const status_led_wifi_t* wifi);

/**
 * @brief Notify OTA status (forward from ota_status_progress_callback)
 */
void status_led_notify_ota(int status, int progress, const char* message);

/**
 * @brief Notify BMS metrics (call each read cycle or on change)
 */
void status_led_notify_bms(const bms_led_metrics_t* m);

/**
 * @brief Set the nominal status "tick" period in milliseconds.
 * Align this with the BMS READ_INTERVAL_MS so LED cadence is in lockstep.
 */
void status_led_set_tick_period_ms(uint32_t period_ms);

/**
 * @brief Notify that a telemetry message was published over MQTT.
 * Triggers a short blue "TX" badge blink (queued).
 */
void status_led_notify_net_telemetry_tx(void);

/**
 * @brief Set a manual override (forces pattern/color until cleared)
 */
void status_led_set_override(led_pattern_t pattern, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Clear any manual override
 */
void status_led_clear_override(void);

#ifdef __cplusplus
}
#endif

#endif // STATUS_LED_H
