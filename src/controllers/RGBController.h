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
#include "controllers/BaseController.h"

class YarrboardApp;
class ConfigManager;

class RGBController : BaseController
{
  public:
    RGBController(YarrboardApp& app);

    bool setup() override;
    void loop() override;

    void setStatusColor(uint8_t r, uint8_t g, uint8_t b);
    void setStatusColor(const CRGB& color);
    void setPixelColor(uint8_t c, uint8_t r, uint8_t g, uint8_t b);
    void setPixelColor(uint8_t c, const CRGB& color);

  private:
    unsigned long lastRGBUpdateMillis = 0;
};

#endif /* !YARR_RGB_H */