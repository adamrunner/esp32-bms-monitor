#include "led_patterns.h"

#include <algorithm>

using namespace indicator_pixel;

namespace status_led_internal {

static inline uint8_t scale_ch(uint8_t v, uint8_t brightness) {
  // gamma-less linear scale; clamp to 255
  uint32_t scaled = (static_cast<uint32_t>(v) * static_cast<uint32_t>(brightness) + 127u) / 255u;
  if (scaled > 255u) scaled = 255u;
  return static_cast<uint8_t>(scaled);
}

static inline LedColor make_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  return LedColor{ scale_ch(r, brightness), scale_ch(g, brightness), scale_ch(b, brightness) };
}

void stop(SinglePixel& pixel) {
  pixel.stop();
}

void apply_pattern(SinglePixel& pixel,
                   uint8_t brightness,
                   led_pattern_t pattern,
                   uint8_t r, uint8_t g, uint8_t b) {
  switch (pattern) {
    case LED_PATTERN_OFF: {
      (void)pixel.set_color(NamedColor::OFF, 0);
      break;
    }
    case LED_PATTERN_SOLID: {
      LedColor c = make_rgb(r, g, b, brightness);
      (void)pixel.set_color(c, 0);
      break;
    }
    case LED_PATTERN_SLOW_PULSE: {
      LedColor c = make_rgb(r, g, b, brightness);
      BlinkOptions opt{ .color = c, .period_ms = 1000, .repeat = -1 };
      (void)pixel.blink(opt);
      break;
    }
    case LED_PATTERN_FAST_BLINK: {
      LedColor c = make_rgb(r, g, b, brightness);
      BlinkOptions opt{ .color = c, .period_ms = 200, .repeat = -1 };
      (void)pixel.blink(opt);
      break;
    }
    case LED_PATTERN_BREATHE: {
      LedColor c = make_rgb(r, g, b, brightness);
      BreatheOptions opt{ .color = c, .cycle_ms = 2000, .repeat = -1 };
      (void)pixel.breathe(opt);
      break;
    }
    case LED_PATTERN_RAINBOW: {
      static const LedColor seq[] = {
        {255, 0, 0},     // Red
        {255, 69, 0},    // Orange-Red
        {255, 165, 0},   // Orange
        {255, 255, 0},   // Yellow
        {0, 255, 0},     // Green
        {0, 0, 255},     // Blue
        {128, 0, 128},   // Purple
        {255, 255, 255}  // White
      };
      // Build a scaled copy so we can apply brightness
      static LedColor scaled[sizeof(seq)/sizeof(seq[0])];
      for (size_t i = 0; i < sizeof(seq)/sizeof(seq[0]); ++i) {
        scaled[i] = make_rgb(seq[i].r, seq[i].g, seq[i].b, brightness);
      }
      FadeSequenceOptions opt{
        .colors = scaled,
        .count = static_cast<size_t>(sizeof(scaled) / sizeof(scaled[0])),
        .transition_ms = 500,
        .hold_ms = 0,
        .repeat = -1
      };
      (void)pixel.fade_sequence(opt);
      break;
    }
    default: {
      // Fallback to off
      (void)pixel.set_color(NamedColor::OFF, 0);
      break;
    }
  }
}

} // namespace status_led_internal
