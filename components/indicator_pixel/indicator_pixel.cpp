#include "indicator_pixel.hpp"

#include <cmath>
#include <cstdlib>

extern "C" {
#include "sdkconfig.h"
#include "esp_log.h"
#include "led_strip_rmt.h"
}


#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#  define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_VERBOSE
#endif

namespace indicator_pixel {

static const char* TAG = "indicator_pixel";
static constexpr uint32_t kStepMs = 10;  // granularity for animations (will be used in later steps)

struct Command {
  enum Kind : uint8_t { STOP, SET_COLOR, BLINK, BREATHE, FADE_SEQ } kind;
  bool exit_task;  // true: break task loop and exit
  union {
    struct { LedColor dst; uint32_t transition_ms; } set;
    BlinkOptions blink;
    BreatheOptions breathe;
    struct { LedColor* colors; size_t count; uint32_t transition_ms; uint32_t hold_ms; int32_t repeat; bool heap_owned; } fade;
  } data;
};

SinglePixel::SinglePixel(const SinglePixelConfig& cfg)
    : resolution_hz_(cfg.resolution_hz),
      gpio_(cfg.gpio),
      max_leds_(cfg.max_leds),
      with_dma_(cfg.with_dma) {}

SinglePixel::~SinglePixel() {
  deinit();
}

esp_err_t SinglePixel::init() {
  if (handle_ != nullptr || task_ != nullptr || q_ != nullptr) {
    return ESP_ERR_INVALID_STATE;
  }

  mutex_ = xSemaphoreCreateMutex();
  if (!mutex_) {
    ESP_LOGE(TAG, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  q_ = xQueueCreate(8, sizeof(Command));
  if (!q_) {
    ESP_LOGE(TAG, "Failed to create command queue");
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
    return ESP_ERR_NO_MEM;
  }

  led_strip_config_t strip_cfg = {};
  strip_cfg.strip_gpio_num = gpio_;
  strip_cfg.max_leds = max_leds_;

  led_strip_rmt_config_t rmt_cfg = {};
  rmt_cfg.resolution_hz = resolution_hz_;
  rmt_cfg.flags.with_dma = with_dma_;

  esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
    vQueueDelete(q_);
    q_ = nullptr;
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
    return err;
  }

  // Start worker task
  BaseType_t ok = xTaskCreate(&SinglePixel::worker_trampoline_, "indicator_pixel",
                              4096, this, 5, &task_);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create worker task");
    led_strip_del(handle_);
    handle_ = nullptr;
    vQueueDelete(q_);
    q_ = nullptr;
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
    return ESP_ERR_NO_MEM;
  }

  // Initialize LED to OFF
  current_ = {0, 0, 0};
  write_pixel_(current_);
  return ESP_OK;
}

esp_err_t SinglePixel::deinit() {
  // Ask the worker to exit
  if (q_ && task_) {
    Command cmd{};
    cmd.kind = Command::STOP;
    cmd.exit_task = true;
    (void)xQueueSend(q_, &cmd, pdMS_TO_TICKS(10));
  }
  // Give the worker a chance to cleanly exit itself
  if (task_) {
    for (int i = 0; i < 10; ++i) { // ~100ms total
      if (task_ == nullptr) break;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (task_) {
      vTaskDelete(task_);
      task_ = nullptr;
    }
  }

  if (handle_) {
    // Turn off LED before deleting
    LedColor off{0, 0, 0};
    (void)write_pixel_(off);
    led_strip_del(handle_);
    handle_ = nullptr;
  }

  if (q_) {
    vQueueDelete(q_);
    q_ = nullptr;
  }

  if (mutex_) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }

  running_ = AnimationType::NONE;
  return ESP_OK;
}

esp_err_t SinglePixel::write_pixel_(const LedColor& c) {
  if (!handle_) return ESP_ERR_INVALID_STATE;
  esp_err_t err = led_strip_set_pixel(handle_, 0, c.r, c.g, c.b);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "led_strip_set_pixel failed: %s", esp_err_to_name(err));
    return err;
  }
  err = led_strip_refresh(handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "led_strip_refresh failed: %s", esp_err_to_name(err));
    return err;
  }
  return ESP_OK;
}

void SinglePixel::stop() {
  if (!q_) return;
  Command cmd{Command::STOP, false};
  (void)xQueueSend(q_, &cmd, 0);
}

bool SinglePixel::is_animating() const {
  if (!mutex_) return false;
  bool anim = false;
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    anim = (running_ != AnimationType::NONE);
    xSemaphoreGive(mutex_);
  }
  return anim;
}

LedColor SinglePixel::current_color() const {
  LedColor c{0, 0, 0};
  if (!mutex_) return c;
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
    c = current_;
    xSemaphoreGive(mutex_);
  }
  return c;
}

void SinglePixel::worker_trampoline_(void* arg) {
  auto* self = static_cast<SinglePixel*>(arg);
  self->worker_task_();
}

void SinglePixel::worker_task_() {
  Command cmd{};
  for (;;) {
    if (xQueueReceive(q_, &cmd, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (cmd.kind == Command::STOP) {
      if (mutex_) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        running_ = AnimationType::NONE;
        xSemaphoreGive(mutex_);
      }
      if (cmd.exit_task) {
        break;
      }
      // For non-exit STOP, we simply mark not running.
      continue;
    } else if (cmd.kind == Command::SET_COLOR) {
      run_set_color_(cmd.data.set.dst, cmd.data.set.transition_ms);
      continue;
    } else if (cmd.kind == Command::BLINK) {
      run_blink_(cmd.data.blink);
      continue;
    } else if (cmd.kind == Command::BREATHE) {
      run_breathe_(cmd.data.breathe);
      continue;
    } else if (cmd.kind == Command::FADE_SEQ) {
      // Reconstruct options and ensure any heap-allocated color buffer is freed after use
      FadeSequenceOptions opts{
        .colors = cmd.data.fade.colors,
        .count = cmd.data.fade.count,
        .transition_ms = cmd.data.fade.transition_ms,
        .hold_ms = cmd.data.fade.hold_ms,
        .repeat = cmd.data.fade.repeat
      };
      run_fade_seq_(opts);
      if (cmd.data.fade.heap_owned && cmd.data.fade.colors) {
        free(cmd.data.fade.colors);
      }
      continue;
    }
  }
  task_ = nullptr;
  vTaskDelete(nullptr);
}

// Placeholders/utilities for future animation work
void SinglePixel::run_set_color_(const LedColor& dst, uint32_t transition_ms) {
  // Snapshot start color
  LedColor start;
  if (mutex_) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    start = current_;
    xSemaphoreGive(mutex_);
  } else {
    start = current_;
  }

  if (transition_ms == 0) {
    (void)write_pixel_(dst);
    if (mutex_) {
      xSemaphoreTake(mutex_, portMAX_DELAY);
      current_ = dst;
      xSemaphoreGive(mutex_);
    } else {
      current_ = dst;
    }
    return;
  }

  uint32_t steps = (transition_ms + (kStepMs / 2)) / kStepMs;
  if (steps == 0) steps = 1;

  for (uint32_t i = 1; i <= steps; ++i) {
    // If a new command is waiting, exit early to allow prompt interrupt
    if (uxQueueMessagesWaiting(q_) > 0) {
      return;
    }
    float t = static_cast<float>(i) / static_cast<float>(steps);
    LedColor c = lerp_(start, dst, t);
    (void)write_pixel_(c);
    if (mutex_) {
      xSemaphoreTake(mutex_, portMAX_DELAY);
      current_ = c;
      xSemaphoreGive(mutex_);
    } else {
      current_ = c;
    }
    vTaskDelay(pdMS_TO_TICKS(kStepMs));
  }

  // Ensure exact destination at the end
  (void)write_pixel_(dst);
  if (mutex_) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    current_ = dst;
    xSemaphoreGive(mutex_);
  } else {
    current_ = dst;
  }
}

void SinglePixel::run_blink_(const BlinkOptions& opt) {
  // Validate
  if (opt.period_ms == 0) {
    return;
  }
  // Mark running
  if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); running_ = AnimationType::BLINK; xSemaphoreGive(mutex_); }

  const uint32_t half_ms = opt.period_ms / 2;
  const bool infinite = (opt.repeat < 0);
  int32_t remaining = opt.repeat;

  auto delay_interruptible = [&](uint32_t total_ms) {
    uint32_t elapsed = 0;
    while (elapsed < total_ms) {
      if (uxQueueMessagesWaiting(q_) > 0) return false; // interrupted
      uint32_t slice = kStepMs;
      if (slice > (total_ms - elapsed)) slice = total_ms - elapsed;
      vTaskDelay(pdMS_TO_TICKS(slice));
      elapsed += slice;
    }
    return true;
  };

  while (infinite || remaining > 0) {
    // ON
    (void)write_pixel_(opt.color);
    if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); current_ = opt.color; xSemaphoreGive(mutex_); }
    if (!delay_interruptible(half_ms)) break;

    // OFF
    LedColor off{0,0,0};
    (void)write_pixel_(off);
    if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); current_ = off; xSemaphoreGive(mutex_); }
    if (!delay_interruptible(half_ms)) break;

    if (!infinite) --remaining;
  }

  if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); running_ = AnimationType::NONE; xSemaphoreGive(mutex_); }
}

void SinglePixel::run_breathe_(const BreatheOptions& opt) {
  if (opt.cycle_ms == 0) return;

  if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); running_ = AnimationType::BREATHE; xSemaphoreGive(mutex_); }

  const bool infinite = (opt.repeat < 0);
  int32_t remaining = opt.repeat;

  auto apply_scale = [&](float s) -> LedColor {
    auto sc = [s](uint8_t v) -> uint8_t { float f = v * s; if (f < 0.f) f = 0.f; if (f > 255.f) f = 255.f; return static_cast<uint8_t>(f + 0.5f); };
    return { sc(opt.color.r), sc(opt.color.g), sc(opt.color.b) };
  };

  const uint32_t half_ms = opt.cycle_ms / 2;
  uint32_t steps = (half_ms + (kStepMs / 2)) / kStepMs;
  if (steps == 0) steps = 1;

  while (infinite || remaining > 0) {
    // Fade in 0->1
    for (uint32_t i = 0; i <= steps; ++i) {
      if (uxQueueMessagesWaiting(q_) > 0) goto done;
      float t = static_cast<float>(i) / static_cast<float>(steps);
      float s = ease_sine_(t);
      LedColor c = apply_scale(s);
      (void)write_pixel_(c);
      if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); current_ = c; xSemaphoreGive(mutex_); }
      vTaskDelay(pdMS_TO_TICKS(kStepMs));
    }
    // Fade out 1->0
    for (uint32_t i = 0; i <= steps; ++i) {
      if (uxQueueMessagesWaiting(q_) > 0) goto done;
      float t = static_cast<float>(i) / static_cast<float>(steps);
      float s = ease_sine_(1.0f - t);
      LedColor c = apply_scale(s);
      (void)write_pixel_(c);
      if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); current_ = c; xSemaphoreGive(mutex_); }
      vTaskDelay(pdMS_TO_TICKS(kStepMs));
    }
    if (!infinite) --remaining;
  }

done:
  if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); running_ = AnimationType::NONE; xSemaphoreGive(mutex_); }
}

void SinglePixel::run_fade_seq_(const FadeSequenceOptions& opt) {
  if (opt.colors == nullptr || opt.count < 2) return;

  if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); running_ = AnimationType::FADE_SEQUENCE; xSemaphoreGive(mutex_); }

  const bool infinite = (opt.repeat < 0);
  int32_t remaining = opt.repeat;

  auto hold_interruptible = [&](uint32_t total_ms) {
    uint32_t elapsed = 0;
    while (elapsed < total_ms) {
      if (uxQueueMessagesWaiting(q_) > 0) return false;
      uint32_t slice = kStepMs;
      if (slice > (total_ms - elapsed)) slice = total_ms - elapsed;
      vTaskDelay(pdMS_TO_TICKS(slice));
      elapsed += slice;
    }
    return true;
  };

  size_t idx = 0;
  while (infinite || remaining > 0) {
    size_t next = (idx + 1) % opt.count;
    LedColor start = opt.colors[idx];
    LedColor end = opt.colors[next];

    uint32_t steps = (opt.transition_ms + (kStepMs / 2)) / kStepMs;
    if (steps == 0) steps = 1;

    for (uint32_t i = 1; i <= steps; ++i) {
      if (uxQueueMessagesWaiting(q_) > 0) goto done;
      float t = static_cast<float>(i) / static_cast<float>(steps);
      LedColor c = lerp_(start, end, t);
      (void)write_pixel_(c);
      if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); current_ = c; xSemaphoreGive(mutex_); }
      vTaskDelay(pdMS_TO_TICKS(kStepMs));
    }

    // Optional hold at destination color
    if (opt.hold_ms > 0) {
      if (!hold_interruptible(opt.hold_ms)) goto done;
    }

    idx = next;
    // Completed a full cycle when we looped back to 0
    if (idx == 0 && !infinite) --remaining;
  }

done:
  if (mutex_) { xSemaphoreTake(mutex_, portMAX_DELAY); running_ = AnimationType::NONE; xSemaphoreGive(mutex_); }
}

LedColor SinglePixel::lerp_(const LedColor& a, const LedColor& b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  auto lerp8 = [t](uint8_t x, uint8_t y) -> uint8_t {
    return static_cast<uint8_t>(x + (y - x) * t + 0.5f);
  };
  return {lerp8(a.r, b.r), lerp8(a.g, b.g), lerp8(a.b, b.b)};
}

float SinglePixel::ease_sine_(float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  const float PI = 3.14159265358979323846f;
  // Ease in/out: 0.5 - 0.5*cos(pi*t)
  return 0.5f - 0.5f * std::cos(PI * t);
}

esp_err_t SinglePixel::set_color(NamedColor color, uint32_t transition_ms) {
  return set_color(from_named(color), transition_ms);
}

esp_err_t SinglePixel::set_color(const LedColor& rgb, uint32_t transition_ms) {
  if (!q_) return ESP_ERR_INVALID_STATE;

  // Interrupt any ongoing work
  Command stop{};
  stop.kind = Command::STOP;
  stop.exit_task = false;
  (void)xQueueSend(q_, &stop, 0);

  Command cmd{};
  cmd.kind = Command::SET_COLOR;
  cmd.exit_task = false;
  cmd.data.set.dst = rgb;
  cmd.data.set.transition_ms = transition_ms;

  if (xQueueSend(q_, &cmd, pdMS_TO_TICKS(10)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}

esp_err_t SinglePixel::blink(const BlinkOptions& opt) {
  if (!q_) return ESP_ERR_INVALID_STATE;
  if (opt.period_ms == 0) return ESP_ERR_INVALID_ARG;

  // Interrupt any ongoing work
  Command stop{}; stop.kind = Command::STOP; stop.exit_task = false;
  (void)xQueueSend(q_, &stop, 0);

  Command cmd{};
  cmd.kind = Command::BLINK;
  cmd.exit_task = false;
  cmd.data.blink = opt;

  if (xQueueSend(q_, &cmd, pdMS_TO_TICKS(10)) != pdTRUE) return ESP_ERR_TIMEOUT;
  return ESP_OK;
}

esp_err_t SinglePixel::breathe(const BreatheOptions& opt) {
  if (!q_) return ESP_ERR_INVALID_STATE;
  if (opt.cycle_ms == 0) return ESP_ERR_INVALID_ARG;

  Command stop{}; stop.kind = Command::STOP; stop.exit_task = false;
  (void)xQueueSend(q_, &stop, 0);

  Command cmd{};
  cmd.kind = Command::BREATHE;
  cmd.exit_task = false;
  cmd.data.breathe = opt;

  if (xQueueSend(q_, &cmd, pdMS_TO_TICKS(10)) != pdTRUE) return ESP_ERR_TIMEOUT;
  return ESP_OK;
}

esp_err_t SinglePixel::fade_sequence(const FadeSequenceOptions& opt) {
  if (!q_) return ESP_ERR_INVALID_STATE;
  if (opt.colors == nullptr || opt.count < 2) return ESP_ERR_INVALID_ARG;

  Command stop{}; stop.kind = Command::STOP; stop.exit_task = false;
  (void)xQueueSend(q_, &stop, 0);

  // Copy colors to heap to ensure lifetime beyond caller's scope
  LedColor* copy = static_cast<LedColor*>(malloc(sizeof(LedColor) * opt.count));
  if (!copy) return ESP_ERR_NO_MEM;
  for (size_t i = 0; i < opt.count; ++i) copy[i] = opt.colors[i];

  Command cmd{};
  cmd.kind = Command::FADE_SEQ;
  cmd.exit_task = false;
  cmd.data.fade.colors = copy;
  cmd.data.fade.count = opt.count;
  cmd.data.fade.transition_ms = opt.transition_ms;
  cmd.data.fade.hold_ms = opt.hold_ms;
  cmd.data.fade.repeat = opt.repeat;
  cmd.data.fade.heap_owned = true;

  if (xQueueSend(q_, &cmd, pdMS_TO_TICKS(10)) != pdTRUE) {
    free(copy);
    return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}

}  // namespace indicator_pixel
