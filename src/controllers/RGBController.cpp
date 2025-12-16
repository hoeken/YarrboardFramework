/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "controllers/RGBController.h"
#include "ConfigManager.h"

#ifdef YB_HAS_STATUS_RGB
CRGB _leds[YB_STATUS_RGB_COUNT];
#endif

RGBController::RGBController(YarrboardApp& app) : BaseController(app, "rgb")
{
}

bool RGBController::setup()
{
#ifdef YB_HAS_STATUS_RGB
  FastLED.addLeds<YB_STATUS_RGB_TYPE, YB_STATUS_RGB_PIN, YB_STATUS_RGB_ORDER>(_leds, YB_STATUS_RGB_COUNT);
  FastLED.setBrightness(32);
  FastLED.clear();
  rgb_set_status_color(CRGB::Blue);
  FastLED.show();
#endif
  return true;
}

void RGBController::loop()
{
#ifdef YB_HAS_STATUS_RGB
  // 10hz refresh
  if (millis() - lastRGBUpdateMillis > 100) {
    FastLED.show();
    lastRGBUpdateMillis = millis();
  }
#endif
}

void RGBController::setStatusColor(uint8_t r, uint8_t g, uint8_t b)
{
  setPixelColor(0, r, g, b);
}

void RGBController::setStatusColor(const CRGB& color)
{
  setPixelColor(0, color);
}

void RGBController::setPixelColor(uint8_t c, uint8_t r, uint8_t g, uint8_t b)
{
#ifdef YB_HAS_STATUS_RGB
  // out of bounds?
  if (c >= YB_STATUS_RGB_COUNT)
    return;

  _leds[c].setRGB(r, g, b);

  if (millis() - lastRGBUpdateMillis > 100) {
    FastLED.show();
    lastRGBUpdateMillis = millis();
  }
#endif
}

void RGBController::setPixelColor(uint8_t c, const CRGB& color)
{
#ifdef YB_HAS_STATUS_RGB
  _leds[c] = color;

  if (millis() - lastRGBUpdateMillis > 100) {
    FastLED.show();
    lastRGBUpdateMillis = millis();
  }
#endif
}