#pragma once

#include "indicator_pixel.hpp"
#include "status_led.h"

namespace status_led_internal {

// Apply a pattern with RGB (0-255) and global brightness scalar (0-255)
void apply_pattern(indicator_pixel::SinglePixel& pixel,
                   uint8_t brightness,
                   led_pattern_t pattern,
                   uint8_t r, uint8_t g, uint8_t b);

// Immediately stop any running animation on the pixel
void stop(indicator_pixel::SinglePixel& pixel);

} // namespace status_led_internal
