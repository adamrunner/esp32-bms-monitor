# indicator_pixel

A reusable C++ ESP-IDF component that controls a single addressable LED (WS2812 or compatible) with a clean API for:
- Initialization and teardown
- Setting colors using named constants or RGB
- Optional transitions (default 350ms, 0ms for immediate)
- Simple animations:
  - blink (50% duty, period_ms, repeat or infinite)
  - breathe (full cycle duration, sine easing, repeat or infinite)
  - fade sequence (list of colors, per-transition duration, optional hold at each color, repeat or infinite)

This component wraps Espressif's official `led_strip` v3 component (RMT backend), uses a small FreeRTOS worker task with a command queue for interruptible animations, and is intended to be dropped into ESP-IDF projects.

## Features

- Minimal footprint: `max_leds=1`, only pixel index 0 is written
- Interruptible animations: any new API call cancels the current animation safely
- No menuconfig required; configure via constructor parameters
- Default transition for set_color is 350ms, 0ms allowed for immediate change

## Installation

Ensure you are using IDF Component Manager in your project (ESP-IDF v5+).

This component declares its dependency:

```
components/indicator_pixel/idf_component.yml
dependencies:
  espressif/led_strip: "^3.0.0"
```

Include `components/indicator_pixel` in your project tree (as a local component).

## Public API

Header: `components/indicator_pixel/include/indicator_pixel.hpp`

- Types:
  - `struct LedColor { uint8_t r, g, b; }`
  - `enum class NamedColor { OFF, RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, WHITE }`
  - `struct SinglePixelConfig { gpio_num_t gpio; uint32_t resolution_hz=10'000'000; bool with_dma=false; uint8_t max_leds=1; }`
  - `struct BlinkOptions { LedColor color; uint32_t period_ms; int32_t repeat; }`
  - `struct BreatheOptions { LedColor color; uint32_t cycle_ms; int32_t repeat; }`
  - `struct FadeSequenceOptions { const LedColor* colors; size_t count; uint32_t transition_ms; uint32_t hold_ms=0; int32_t repeat; }`

- Class:
  - `class SinglePixel`
    - `explicit SinglePixel(const SinglePixelConfig& cfg);`
    - `esp_err_t init();`
    - `esp_err_t deinit();`
    - `esp_err_t set_color(NamedColor color, uint32_t transition_ms = 350);`
    - `esp_err_t set_color(const LedColor& rgb, uint32_t transition_ms = 350);`
    - `esp_err_t blink(const BlinkOptions& opt);`
    - `esp_err_t breathe(const BreatheOptions& opt);`
    - `esp_err_t fade_sequence(const FadeSequenceOptions& opt);`
    - `void stop();`
    - `bool is_animating() const;`
    - `LedColor current_color() const;`

## Quickstart Example

```cpp
#include "indicator_pixel.hpp"
extern "C" void app_main(void);

using namespace indicator_pixel;

extern "C" void app_main(void) {
  SinglePixel led({ .gpio = static_cast<gpio_num_t>(8) });
  led.init();

  // Named color with default transition (350ms)
  led.set_color(NamedColor::BLUE);

  // Immediate change to RED
  led.set_color(LedColor{255, 0, 0}, 0);

  // Blink RED at 1Hz (1000ms period), 3 times
  led.blink(BlinkOptions{
    .color = LedColor{255, 0, 0},
    .period_ms = 1000,
    .repeat = 3
  });

  // Breathe GREEN with a 2-second full cycle, indefinitely
  led.breathe(BreatheOptions{
    .color = LedColor{0, 255, 0},
    .cycle_ms = 2000,
    .repeat = -1
  });

  // Fade between colors with 500ms per transition and 200ms hold, indefinitely
  static const LedColor seq[] = {
    {255, 0, 0}, {0, 255, 0}, {0, 0, 255}
  };
  led.fade_sequence(FadeSequenceOptions{
    .colors = seq,
    .count = 3,
    .transition_ms = 500,
    .hold_ms = 200,
    .repeat = -1
  });
}
```

Notes:
- Any new call (e.g., `set_color`, `blink`, `breathe`, `fade_sequence`, `stop`) interrupts the current animation/transition safely.
- `repeat = -1` means run indefinitely.

## Build

- Add the component to your project and ensure your app targets a board pin with a single WS2812-compatible LED connected.
- Build with `idf.py build` as usual. The component is registered via `components/indicator_pixel/CMakeLists.txt`.

## License

MIT (same terms as this repository unless otherwise specified).
