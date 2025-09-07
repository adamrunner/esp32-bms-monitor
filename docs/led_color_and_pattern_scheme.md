# LED Badge Behavior (Low-Power Mode)

This device uses a low-power, event-driven, queued “badge blink” system. The LED is OFF most of the time and only blinks briefly to indicate events. Critical and warning conditions still take over the LED to ensure visibility.

Key principles:
- Event-driven: short badges queued and played sequentially
- Idle OFF: no continuous base/overlay animations to reduce duty cycle
- Same cadence as BMS loop: status tick aligns with READ_INTERVAL_MS
- Takeover dominance: critical/warning alerts pause badge playback

## Sources and APIs

- BMS tick cadence: READ_INTERVAL_MS (main/main.cpp) and status_led_set_tick_period_ms(READ_INTERVAL_MS)
- BMS status event: status_led_notify_bms(...)
- Telemetry publish event: status_led_notify_net_telemetry_tx()
- OTA progress event: status_led_notify_ota(...)

Only telemetry publishes should trigger the blue “TX” badge.

## Badge Blinks (Queued, Low-Power)

- Green (✅ BMS OK tick)
  - Trigger: valid=true, comm_ok=true and no active takeover
  - Blink: 200 ms period, 1 repeat (≈100 ms ON, 100 ms OFF)
  - Frequency: once per READ_INTERVAL_MS (10s by default)

- Blue (🔵 Telemetry published)
  - Trigger: MQTT publish to the telemetry topic (config_.topic)
  - Blink: 200 ms period, 1 repeat
  - Only telemetry publishes (not OTA/status/control)

- Purple (🟣 OTA activity)
  - Trigger: OTA checking/downloading/installing
  - Blink: 200 ms period, 1 repeat
  - Rate-limited: ≥ 500 ms between OTA badges to avoid spam

Badges are queued and shown in order, e.g., Green tick then Blue TX.

## Takeover Alerts (Highest Priority)

Takeovers dominate the LED and pause badge playback until cleared.

- RED (🔴 Critical)
  - Fast blink: BMS communication failure
  - Solid: Cell overvoltage/undervoltage critical
  - Slow pulse: Temperature critical
  - Fast blink: MOSFET fault

- ORANGE (🟠 Warnings)
  - Slow pulse: Low SOC (< 20%)
  - Fast blink: High cell voltage delta (> 0.20 V)
  - Breathe: Temperature warning band

- OTA Failure/Rollback (🟣)
  - Fast blink: OTA failed/rollback takeover

## Boot and Idle

- Optional boot animation: Rainbow sequence (if enabled), stopped on first BMS update
- Idle: LED OFF when no badges are playing and no takeover is active

## Deprecated Behavior

The previous base+overlay animation system is disabled in low-power mode:
- No continuous base animations (e.g., GREEN breathe/pulse during normal operation)
- No Wi‑Fi signal overlays (RSSI-based blue pulses)
- Deprecated config fields (kept for compatibility but ignored):
  - status_led_config_t.overlay_enabled
  - status_led_config_t.overlay_period_ms
  - status_led_config_t.overlay_on_ms

## Quick Reference

- Normal: Short GREEN tick every READ_INTERVAL_MS when BMS is healthy
- Network TX: Short BLUE blink only when telemetry publishes
- OTA: Short PURPLE badges on activity; PURPLE fast blink takeover on failure/rollback
- Errors/Warnings: RED/ORANGE takeover as above
- Otherwise: OFF (low power)
