#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "display.h"
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7796     _panel_instance;
  lgfx::Bus_SPI          _bus_instance;
  lgfx::Light_PWM        _light_instance;
public:
  LGFX()
  {
    auto bus_cfg = _bus_instance.config();
    bus_cfg.spi_host = SPI2_HOST;
    bus_cfg.spi_mode = 0;
    bus_cfg.freq_write = 40000000;
    bus_cfg.freq_read  = 16000000;
    bus_cfg.spi_3wire  = false;
    bus_cfg.use_lock   = true;
    bus_cfg.dma_channel = SPI_DMA_CH_AUTO;
    bus_cfg.pin_sclk = 14;
    bus_cfg.pin_mosi = 13;
    bus_cfg.pin_miso = 12;
    bus_cfg.pin_dc   = 2;
    _bus_instance.config(bus_cfg);
    _panel_instance.setBus(&_bus_instance);

    auto panel_cfg = _panel_instance.config();
    panel_cfg.pin_cs   = 15;
    panel_cfg.pin_rst  = -1;
    panel_cfg.pin_busy = -1;
    panel_cfg.panel_width  = 320;
    panel_cfg.panel_height = 480;
    panel_cfg.memory_width  = 320;
    panel_cfg.memory_height = 480;
    panel_cfg.offset_x = 0;
    panel_cfg.offset_y = 0;
    panel_cfg.offset_rotation = 0;
    panel_cfg.dummy_read_pixel = 8;
    panel_cfg.dummy_read_bits  = 1;
    panel_cfg.readable   = true;
    panel_cfg.invert     = false;
    panel_cfg.rgb_order  = false;
    panel_cfg.dlen_16bit = false;
    panel_cfg.bus_shared = true;
    _panel_instance.config(panel_cfg);

    auto light_cfg = _light_instance.config();
    light_cfg.pin_bl = 27;
    light_cfg.invert = false;
    light_cfg.freq   = 12000;
    light_cfg.pwm_channel = 7;
    _light_instance.config(light_cfg);
    _panel_instance.setLight(&_light_instance);

    setPanel(&_panel_instance);
  }
};

static LGFX tft;

extern "C" void display_init()
{
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.println("BMS Monitor");
}

extern "C" void display_update_basic(float v, float i, float soc, float p, double ewh)
{
    tft.fillRect(0, 24, 240, 80, TFT_BLACK);
    tft.setCursor(0, 24);
    tft.printf("V: %.2f V\n", v);
    tft.printf("I: %.2f A\n", i);
    tft.printf("SOC: %.1f %%\n", soc);
    tft.printf("P: %.2f W\n", p);
    tft.printf("E: %.3f Wh\n", ewh);
}

extern "C" void display_print_line(const char* s)
{
    tft.fillRect(0, 120, 320, 16, TFT_BLACK);
    tft.setCursor(0, 120);
    tft.print(s);
}
