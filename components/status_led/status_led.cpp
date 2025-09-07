#include "status_led.h"
#include "led_patterns.h"
#include "indicator_pixel.hpp"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
}

#include <string.h>
#include <algorithm>
#include <deque>

using namespace indicator_pixel;
using namespace status_led_internal;

static const char* TAG = "status_led";

namespace {

// Thresholds (tunable)
static constexpr float SOC_WARN_PCT      = 20.0f;
static constexpr float CELL_DELTA_WARN_V = 0.20f;
static constexpr float TEMP_WARN_C       = 55.0f;
static constexpr float TEMP_CRIT_C       = 70.0f;

// Local mirror of OTA status codes (keep in sync with ota_manager.h)
static constexpr int kOTA_IDLE        = 0;
static constexpr int kOTA_CHECKING    = 1;
static constexpr int kOTA_DOWNLOADING = 2;
static constexpr int kOTA_INSTALLING  = 3;
static constexpr int kOTA_SUCCESS     = 4;
static constexpr int kOTA_FAILED      = 5;
static constexpr int kOTA_ROLLBACK    = 6;

struct DesiredIndicator {
  led_pattern_t pattern;
  uint8_t r, g, b;
  bool operator==(const DesiredIndicator& o) const {
    return pattern == o.pattern && r == o.r && g == o.g && b == o.b;
  }
  bool operator!=(const DesiredIndicator& o) const { return !(*this == o); }
};

enum EventKind : uint8_t {
  EV_BOOT_STAGE,
  EV_WIFI,
  EV_OTA,
  EV_BMS,
  EV_NET_TX,
  EV_OVERRIDE_SET,
  EV_OVERRIDE_CLEAR
};

struct EvBoot { status_boot_stage_t stage; };
struct EvWifi { status_led_wifi_t wifi; };
struct EvOta  { int status; int progress; };
struct EvBms  { bms_led_metrics_t m; };
struct EvOverrideSet { led_pattern_t pat; uint8_t r,g,b; };

struct Event {
  EventKind kind;
  union {
    EvBoot boot;
    EvWifi wifi;
    EvOta  ota;
    EvBms  bms;
    EvOverrideSet ov_set;
  } data;
};

struct BadgeEvent { uint8_t r, g, b; uint16_t period_ms; int32_t repeats; };

struct Snapshot {
  // Config (with runtime defaults applied in init)
  status_led_config_t cfg{true, 8, 64, true, true, true, 5000, 400};

  // Inputs (latest)
  bool have_boot = false;
  status_boot_stage_t boot_stage = STATUS_BOOT_STAGE_BOOT;

  bool have_wifi = false;
  status_led_wifi_t wifi{false, 0};

  bool have_ota = false;
  int ota_status = kOTA_IDLE;
  int ota_progress = 0;

  bool have_bms = false;
  bms_led_metrics_t bms{};

  // Manual override
  bool override_active = false;
  led_pattern_t ov_pat = LED_PATTERN_OFF;
  uint8_t ov_r = 0, ov_g = 0, ov_b = 0;


  // Last applied DI
  DesiredIndicator last{LED_PATTERN_OFF, 0, 0, 0};
};

class StatusLed {
public:
  static StatusLed& I() {
    static StatusLed s;
    return s;
  }

  esp_err_t init(const status_led_config_t* cfg) {
    if (initialized_) return ESP_OK;

    // Apply provided config
    if (cfg) snap_.cfg = *cfg;
    // Legacy overlay config is ignored in low-power badge mode.

    if (!snap_.cfg.enabled) {
      ESP_LOGW(TAG, "Status LED disabled by config");
      initialized_ = true; // allow APIs but no-ops
      return ESP_OK;
    }

    SinglePixelConfig pcfg{
      .gpio = static_cast<gpio_num_t>(snap_.cfg.gpio_pin),
      .resolution_hz = 10'000'000,
      .with_dma = false,
      .max_leds = 1
    };
    pixel_ = new SinglePixel(pcfg);
    if (!pixel_) {
      ESP_LOGE(TAG, "Allocation failed");
      return ESP_ERR_NO_MEM;
    }
    esp_err_t err = pixel_->init();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "indicator_pixel init failed: %s", esp_err_to_name(err));
      delete pixel_; pixel_ = nullptr;
      return err;
    }

    q_ = xQueueCreate(16, sizeof(Event));
    if (!q_) {
      ESP_LOGE(TAG, "Failed to create event queue");
      pixel_->deinit(); delete pixel_; pixel_ = nullptr;
      return ESP_ERR_NO_MEM;
    }

    auto ok = xTaskCreate(&StatusLed::task_trampoline, "status_led", 4096, this, 4, &task_);
    if (ok != pdPASS) {
      ESP_LOGE(TAG, "Failed to create task");
      vQueueDelete(q_); q_ = nullptr;
      pixel_->deinit(); delete pixel_; pixel_ = nullptr;
      return ESP_ERR_NO_MEM;
    }

    initialized_ = true;

    // Optional boot animation immediately
    if (snap_.cfg.boot_animation) {
      DesiredIndicator d{LED_PATTERN_RAINBOW, 255, 255, 255}; // color ignored for rainbow
      apply_(d);
    } else {
      DesiredIndicator d{LED_PATTERN_OFF, 0, 0, 0};
      apply_(d);
    }
    boot_anim_active_ = snap_.cfg.boot_animation;


    ESP_LOGI(TAG, "Status LED initialized on GPIO %d, brightness %u, low-power badge mode",
             snap_.cfg.gpio_pin, snap_.cfg.brightness);
    return ESP_OK;
  }

  void deinit() {
    if (!initialized_) return;
    if (task_) { vTaskDelete(task_); task_ = nullptr; }
    if (q_) { vQueueDelete(q_); q_ = nullptr; }
    if (pixel_) {
      (void)pixel_->set_color(NamedColor::OFF, 0);
      pixel_->deinit();
      delete pixel_;
      pixel_ = nullptr;
    }
    initialized_ = false;
  }

  void notify(const Event& ev) {
    if (!initialized_) return;
    if (!snap_.cfg.enabled) return;
    if (!q_) return;
    (void)xQueueSend(q_, &ev, 0);
  }

  void set_tick_period_ms(uint32_t p) { tick_period_ms_ = p; }

private:
  static void task_trampoline(void* arg) {
    static_cast<StatusLed*>(arg)->run_();
  }

  void run_() {
    Event ev{};
    for (;;) {
      bool got = (xQueueReceive(q_, &ev, pdMS_TO_TICKS(100)) == pdTRUE);
      if (got) {
        switch (ev.kind) {
          case EV_BOOT_STAGE:
            snap_.have_boot = true;
            snap_.boot_stage = ev.data.boot.stage;
            break;
          case EV_WIFI:
            snap_.have_wifi = true;
            snap_.wifi = ev.data.wifi.wifi;
            break;
          case EV_OTA:
            snap_.have_ota = true;
            snap_.ota_status = ev.data.ota.status;
            snap_.ota_progress = ev.data.ota.progress;
            // Enqueue short PURPLE badge for active OTA states (rate-limited)
            if (snap_.ota_status == kOTA_CHECKING ||
                snap_.ota_status == kOTA_DOWNLOADING ||
                snap_.ota_status == kOTA_INSTALLING) {
              TickType_t now = xTaskGetTickCount();
              if (now - last_ota_badge_tick_ >= pdMS_TO_TICKS(500)) {
                enqueue_badge_(128, 0, 128, 200, 1);
                last_ota_badge_tick_ = now;
              }
            }
            break;
          case EV_BMS:
            snap_.have_bms = ev.data.bms.m.valid;
            snap_.bms = ev.data.bms.m;
            // Stop boot animation on first BMS update
            if (boot_anim_active_) {
              if (pixel_) pixel_->stop();
              boot_anim_active_ = false;
              // Ensure OFF baseline after stopping boot anim
              apply_if_changed_(DesiredIndicator{LED_PATTERN_OFF, 0, 0, 0});
            }
            // Queue GREEN tick only when comm_ok and no takeover
            {
              DesiredIndicator tmp{};
              bool takeover = compute_takeover_(tmp);
              if (snap_.bms.valid && snap_.bms.comm_ok && !takeover) {
                enqueue_badge_(0, 255, 0, 200, 1);
              }
            }
            break;
          case EV_NET_TX:
            // Telemetry publish badge (BLUE)
            enqueue_badge_(0, 0, 255, 200, 1);
            break;
          case EV_OVERRIDE_SET:
            snap_.override_active = true;
            snap_.ov_pat = ev.data.ov_set.pat;
            snap_.ov_r = ev.data.ov_set.r;
            snap_.ov_g = ev.data.ov_set.g;
            snap_.ov_b = ev.data.ov_set.b;
            break;
          case EV_OVERRIDE_CLEAR:
            snap_.override_active = false;
            break;
        }
      }

      // Manual override always wins
      if (snap_.override_active) {
        // Cancel any running badge animation
        if (pixel_ && pixel_->is_animating()) {
          pixel_->stop();
        }
        apply_if_changed_(DesiredIndicator{snap_.ov_pat, snap_.ov_r, snap_.ov_g, snap_.ov_b});
        continue;
      }

      // Takeover (critical/warning/ota-failure) dominates and pauses badges
      DesiredIndicator di_takeover{};
      if (compute_takeover_(di_takeover)) {
        if (pixel_ && pixel_->is_animating()) {
          pixel_->stop();
        }
        apply_if_changed_(di_takeover);
        continue;
      }

      // Badge playback: sequential short blinks; idle is OFF
      if (pixel_ && !pixel_->is_animating()) {
        if (!pending_badges_.empty()) {
          BadgeEvent b = pending_badges_.front();
          pending_badges_.pop_front();
          indicator_pixel::LedColor c = scaled_rgb_(b.r, b.g, b.b);
          BlinkOptions opt{ .color = c, .period_ms = static_cast<uint32_t>(b.period_ms), .repeat = b.repeats };
          (void)pixel_->blink(opt);
        } else {
          // Ensure boot animation is stopped and LED is OFF when idle
          if (boot_anim_active_) {
            pixel_->stop();
            boot_anim_active_ = false;
          }
          apply_if_changed_(DesiredIndicator{LED_PATTERN_OFF, 0, 0, 0});
        }
      }
    }
  }

  // Compute a takeover DI (critical RED, warnings ORANGE, OTA failed/rollback). Return true if present.
  bool compute_takeover_(DesiredIndicator& out) const {
    // OTA failure/rollback is a takeover (PURPLE fast blink)
    if (snap_.have_ota) {
      if (snap_.ota_status == kOTA_FAILED || snap_.ota_status == kOTA_ROLLBACK) {
        out = DesiredIndicator{LED_PATTERN_FAST_BLINK, 128, 0, 128};
        return true;
      }
    }

    if (snap_.have_bms) {
      const auto& b = snap_.bms;

      // Critical RED
      if (!b.comm_ok) {
        out = DesiredIndicator{LED_PATTERN_FAST_BLINK, 255, 0, 0}; // comm fail
        return true;
      }
      if (b.ov_critical || b.uv_critical) {
        out = DesiredIndicator{LED_PATTERN_SOLID, 255, 0, 0}; // hard assert; keep solid for severity
        return true;
      }
      if (b.max_temp_c >= TEMP_CRIT_C) {
        out = DesiredIndicator{LED_PATTERN_SLOW_PULSE, 255, 0, 0};
        return true;
      }
      if (b.mosfet_fault) {
        out = DesiredIndicator{LED_PATTERN_FAST_BLINK, 255, 0, 0};
        return true;
      }

      // Warnings ORANGE (still takeovers so they aren't missed)
      if (b.soc_pct >= 0.0f && b.soc_pct < SOC_WARN_PCT) {
        out = DesiredIndicator{LED_PATTERN_SLOW_PULSE, 255, 165, 0}; // Low SOC
        return true;
      }
      if (b.cell_delta_v > CELL_DELTA_WARN_V) {
        out = DesiredIndicator{LED_PATTERN_FAST_BLINK, 255, 165, 0}; // High delta
        return true;
      }
      if (b.max_temp_c >= TEMP_WARN_C && b.max_temp_c < TEMP_CRIT_C) {
        out = DesiredIndicator{LED_PATTERN_BREATHE, 255, 165, 0}; // Thermal warn band
        return true;
      }
    }

    return false;
  }



  void apply_(const DesiredIndicator& d) {
    if (!pixel_) return;
    apply_pattern(*pixel_, snap_.cfg.brightness, d.pattern, d.r, d.g, d.b);
  }

  void apply_if_changed_(const DesiredIndicator& d) {
    if (d != snap_.last) {
      apply_(d);
      snap_.last = d;
    }
  }

private:
  SinglePixel*   pixel_ = nullptr;
  QueueHandle_t  q_ = nullptr;
  TaskHandle_t   task_ = nullptr;
  Snapshot       snap_{};
  bool           initialized_ = false;

  std::deque<BadgeEvent> pending_badges_;
  uint32_t tick_period_ms_ = 10000;
  TickType_t last_ota_badge_tick_ = 0;
  bool boot_anim_active_ = false;

  inline indicator_pixel::LedColor scaled_rgb_(uint8_t r, uint8_t g, uint8_t b) const {
    auto scale = [](uint8_t v, uint8_t br) -> uint8_t {
      uint32_t scaled = (static_cast<uint32_t>(v) * static_cast<uint32_t>(br) + 127u) / 255u;
      if (scaled > 255u) scaled = 255u;
      return static_cast<uint8_t>(scaled);
    };
    return indicator_pixel::LedColor{ scale(r, snap_.cfg.brightness), scale(g, snap_.cfg.brightness), scale(b, snap_.cfg.brightness) };
  }

  inline void enqueue_badge_(uint8_t r, uint8_t g, uint8_t b, uint16_t period_ms, int32_t repeats) {
    pending_badges_.push_back(BadgeEvent{r, g, b, period_ms, repeats});
  }
};

} // namespace

extern "C" {

esp_err_t status_led_init(const status_led_config_t* cfg) {
  return StatusLed::I().init(cfg);
}

void status_led_deinit(void) {
  StatusLed::I().deinit();
}

void status_led_notify_boot_stage(status_boot_stage_t stage) {
  Event ev{}; ev.kind = EV_BOOT_STAGE; ev.data.boot.stage = stage;
  StatusLed::I().notify(ev);
}

void status_led_notify_wifi(const status_led_wifi_t* wifi) {
  if (!wifi) return;
  Event ev{}; ev.kind = EV_WIFI; ev.data.wifi.wifi = *wifi;
  StatusLed::I().notify(ev);
}

void status_led_notify_ota(int status, int progress, const char* /*message*/) {
  Event ev{}; ev.kind = EV_OTA; ev.data.ota.status = status; ev.data.ota.progress = progress;
  StatusLed::I().notify(ev);
}

void status_led_notify_bms(const bms_led_metrics_t* m) {
  if (!m) return;
  Event ev{}; ev.kind = EV_BMS; ev.data.bms.m = *m;
  StatusLed::I().notify(ev);
}

void status_led_set_tick_period_ms(uint32_t period_ms) {
  StatusLed::I().set_tick_period_ms(period_ms);
}

void status_led_notify_net_telemetry_tx(void) {
  Event ev{}; ev.kind = EV_NET_TX;
  StatusLed::I().notify(ev);
}

void status_led_set_override(led_pattern_t pattern, uint8_t r, uint8_t g, uint8_t b) {
  Event ev{}; ev.kind = EV_OVERRIDE_SET; ev.data.ov_set.pat = pattern; ev.data.ov_set.r = r; ev.data.ov_set.g = g; ev.data.ov_set.b = b;
  StatusLed::I().notify(ev);
}

void status_led_clear_override(void) {
  Event ev{}; ev.kind = EV_OVERRIDE_CLEAR;
  StatusLed::I().notify(ev);
}

} // extern "C"
