#ifndef LGFX_SETUP_H
#define LGFX_SETUP_H

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7735S _panel;
  lgfx::Bus_SPI _bus;

public:
  LGFX()
  {
    auto cfg = _bus.config();
    cfg.spi_host = SPI2_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 27000000;
    cfg.pin_sclk = 10;
    cfg.pin_mosi = 11;
    cfg.pin_miso = -1;
    cfg.pin_dc = 13;
    _bus.config(cfg);
    _panel.setBus(&_bus);

    auto pcfg = _panel.config();
    pcfg.pin_cs = 12;
    pcfg.pin_rst = 14;
    pcfg.panel_width = 80;
    pcfg.panel_height = 160;
    pcfg.offset_x = 26;
    pcfg.offset_y = 1;
    pcfg.invert = true;
    _panel.config(pcfg);
    setPanel(&_panel);
  }
};

#endif
