/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_RGB_H
#define YARR_RGB_H

#include "FastLED.h"
#include "YarrboardConfig.h"

class YarrboardApp;
class ConfigManager;

class RGBController
{
  public:
    RGBController(YarrboardApp& app, ConfigManager& config);

    void setup();
    void loop();

    void setStatusColor(uint8_t r, uint8_t g, uint8_t b);
    void setStatusColor(const CRGB& color);
    void setPixelColor(uint8_t c, uint8_t r, uint8_t g, uint8_t b);
    void setPixelColor(uint8_t c, const CRGB& color);

  private:
    YarrboardApp& _app;
    ConfigManager& _config;

    unsigned long lastRGBUpdateMillis = 0;
};

#endif /* !YARR_RGB_H */