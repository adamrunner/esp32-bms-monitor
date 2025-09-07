# Status LEDs: Low-Power Queued Badge Implementation

This document describes the current low-power, event-driven LED system. It replaces prior continuous base/overlay animations with a queued “badge blink” design to minimize duty cycle and power drain while preserving visibility of critical/warning conditions.

## Component Architecture

```
components/status_led/
├── include/status_led.h      # Public API (low-power badge APIs)
├── CMakeLists.txt            # Component config
├── status_led.cpp            # Core LED control, event queue, takeover logic
└── led_patterns.cpp          # Pattern helpers (blink/breathe/fade/rainbow)
```

Note: Legacy files such as a separate “status_evaluator.cpp” do not exist in the current codebase. Priority and takeover logic live inside status_led.cpp.

## Key Behaviors

- Low-power idle: LED stays OFF unless a badge is playing or a takeover is active
- Event-driven: short blinks (“badges”) are enqueued and played sequentially
- Synchronized cadence: GREEN “tick” badge aligns with main loop READ_INTERVAL_MS
- Takeover dominance: critical and warning conditions pause badge playback and override the LED

## Public APIs

Declared in components/status_led/include/status_led.h:

- esp_err_t status_led_init(const status_led_config_t* cfg)
- void status_led_deinit(void)
- void status_led_notify_boot_stage(status_boot_stage_t stage)
- void status_led_notify_wifi(const status_led_wifi_t* wifi)
- void status_led_notify_ota(int status, int progress, const char* message)
- void status_led_notify_bms(const bms_led_metrics_t* m)
- void status_led_set_tick_period_ms(uint32_t period_ms)              // NEW
- void status_led_notify_net_telemetry_tx(void)                       // NEW
- void status_led_set_override(led_pattern_t pattern, uint8_t r, uint8_t g, uint8_t b)
- void status_led_clear_override(void)

### Configuration Structure

status_led_config_t:
- enabled (bool)
- gpio_pin (int)
- brightness (uint8_t)
- boot_animation (bool)
- critical_override (bool)
- overlay_enabled / overlay_period_ms / overlay_on_ms (Deprecated; ignored)

These legacy overlay fields remain for config compatibility but are unused in low-power badge mode.

## Event Sources and Queue

The internal StatusLed task has a FreeRTOS queue of Events and a std::deque of BadgeEvent:

- EV_BMS: On valid && comm_ok and no takeover, enqueue a GREEN badge
- EV_NET_TX: On telemetry publish, enqueue a BLUE badge
- EV_OTA: On checking/downloading/installing, enqueue PURPLE badges (≥500 ms rate-limit)
- OVERRIDE: Manual override takes immediate precedence (until cleared)
- Takeover logic evaluates criticals/warnings; takeovers pause badge playback

Badge playback is strictly sequential; if GREEN and BLUE are queued close together, they are shown in that order.

## Patterns

- Badges: BLINK with period_ms=200, repeats=1 (≈100 ms ON, 100 ms OFF)
  - GREEN: BMS successful read
  - BLUE: Telemetry publish (only for telemetry topic)
  - PURPLE: OTA activity (rate-limited)
- Takeovers:
  - RED Fast blink: BMS communication failure
  - RED Solid: OV/UV critical
  - RED Slow pulse: Temperature critical
  - ORANGE Slow pulse: Low SOC (< 20%)
  - ORANGE Fast blink: High cell voltage delta (> 0.20 V)
  - ORANGE Breathe: Temperature warning band
  - PURPLE Fast blink: OTA failed or rollback

## Integration Points

- main/main.cpp
  - Defines READ_INTERVAL_MS = 10000
  - Calls status_led_set_tick_period_ms(READ_INTERVAL_MS)
  - Calls status_led_notify_bms(...) each loop (drives GREEN tick)
  - Optional boot animation stopped automatically on first BMS update

- components/logging/mqtt_log_sink.cpp
  - After successful telemetry publish to config_.topic, calls status_led_notify_net_telemetry_tx() (drives BLUE badge)

- components/ota_manager/ota_status_logger.cpp
  - Forwards OTA status via status_led_notify_ota(...); purple badges are rate-limited

## Deprecated/Removed Behavior

- Continuous base animations (e.g., breathe/pulse during normal operation) are disabled
- Periodic overlay scheduler has been removed; overlay fields in config are ignored
- No Wi‑Fi RSSI-based blue overlays (only telemetry publish triggers blue)

## Power/Performance

- LED is OFF the majority of the time; badges are short, infrequent blinks
- On-device verification shows reduced on-time vs prior continuous patterns
- Uses indicator_pixel SinglePixel API (blink/breathe/fade_sequence) with minimal overhead

## Testing Guidance

- Boot: Optional rainbow if enabled; stops on first BMS update, then idle OFF
- BMS healthy: Every READ_INTERVAL_MS, one short GREEN blink
- Telemetry: Immediate BLUE blink after publish (appears after GREEN when queued)
- OTA: Short PURPLE badges during checking/downloading/installing (rate-limited); PURPLE fast blink takeover on failure/rollback
- Takeovers: RED/ORANGE states override and pause badge playback
- With no activity: LED remains OFF

This implementation prioritizes power savings while maintaining clear, event-specific feedback and critical alert visibility.
