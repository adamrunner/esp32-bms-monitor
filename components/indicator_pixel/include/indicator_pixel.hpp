#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {
#include "hal/gpio_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"
}

namespace indicator_pixel {

struct LedColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

enum class NamedColor : uint8_t {
  OFF,
  RED,
  ORANGE,
  YELLOW,
  GREEN,
  BLUE,
  PURPLE,
  WHITE
};

inline LedColor from_named(NamedColor c) {
  switch (c) {
    case NamedColor::OFF: return {0, 0, 0};
    case NamedColor::RED: return {255, 0, 0};
    case NamedColor::ORANGE: return {255, 165, 0};
    case NamedColor::YELLOW: return {255, 255, 0};
    case NamedColor::GREEN: return {0, 255, 0};
    case NamedColor::BLUE: return {0, 0, 255};
    case NamedColor::PURPLE: return {128, 0, 128};
    case NamedColor::WHITE: return {255, 255, 255};
    default: return {0, 0, 0};
  }
}

enum class AnimationType : uint8_t {
  NONE,
  BLINK,
  BREATHE,
  FADE_SEQUENCE
};

struct SinglePixelConfig {
  gpio_num_t gpio;
  uint32_t resolution_hz = 10'000'000;  // RMT clock
  bool with_dma = false;
  uint8_t max_leds = 1;                 // we only write pixel 0
};

struct BlinkOptions {
  LedColor color;
  uint32_t period_ms;   // full period; 50% duty (on = period/2)
  int32_t repeat;       // -1 = infinite; else number of periods
};

struct BreatheOptions {
  LedColor color;
  uint32_t cycle_ms;    // full in+out cycle
  int32_t repeat;       // -1 = infinite; else number of cycles
};

struct FadeSequenceOptions {
  const LedColor* colors;   // pointer to at least 2 entries
  size_t count;             // number of colors (>= 2)
  uint32_t transition_ms;   // duration per transition
  uint32_t hold_ms = 0;     // optional hold at each color
  int32_t repeat;           // -1 = infinite; else number of full list cycles
};

class SinglePixel {
public:
  explicit SinglePixel(const SinglePixelConfig& cfg);
  ~SinglePixel();

  esp_err_t init();     // create led_strip device, spawn worker
  esp_err_t deinit();   // stop worker, destroy device

  esp_err_t set_color(NamedColor color, uint32_t transition_ms = 350);
  esp_err_t set_color(const LedColor& rgb, uint32_t transition_ms = 350);

  esp_err_t blink(const BlinkOptions& opt);              // 50% duty
  esp_err_t breathe(const BreatheOptions& opt);          // full cycle duration
  esp_err_t fade_sequence(const FadeSequenceOptions& opt); // per-transition, optional hold

  void stop();                 // cancel running animation immediately
  bool is_animating() const;   // thread-safe query
  LedColor current_color() const;

private:
  SinglePixel(const SinglePixel&) = delete;
  SinglePixel& operator=(const SinglePixel&) = delete;

  // Device/config
  led_strip_handle_t handle_ = nullptr;
  TaskHandle_t task_ = nullptr;
  QueueHandle_t q_ = nullptr;
  mutable SemaphoreHandle_t mutex_ = nullptr;
  LedColor current_{0, 0, 0};
  AnimationType running_ = AnimationType::NONE;
  uint32_t resolution_hz_;
  gpio_num_t gpio_;
  uint8_t max_leds_;
  bool with_dma_;

  // Helpers
  esp_err_t write_pixel_(const LedColor& c);
  static void worker_trampoline_(void* arg);
  void worker_task_();
  void run_set_color_(const LedColor& dst, uint32_t transition_ms);
  void run_blink_(const BlinkOptions& opt);
  void run_breathe_(const BreatheOptions& opt);
  void run_fade_seq_(const FadeSequenceOptions& opt);
  static LedColor lerp_(const LedColor& a, const LedColor& b, float t);
  static float ease_sine_(float t); // 0..1
};

} // namespace indicator_pixel
