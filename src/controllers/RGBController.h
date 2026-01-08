/*
 * Yarrboard Framework
 *
 * Copyright (c) 2025 Zach Hoeken <hoeken@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef YARR_RGB_H
#define YARR_RGB_H

#include "FastLED.h"
#include "YarrboardConfig.h"
#include "controllers/BaseController.h"

class YarrboardApp;
class ConfigManager;

// This is the "Interface" your App will talk to.
// It has NO template arguments.
class RGBControllerInterface : public BaseController
{
  public:
    RGBControllerInterface(YarrboardApp& app, const char* name) : BaseController(app, name) {}

    uint8_t maxBrightness = 50;

    virtual void setStatusColor(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void setStatusColor(const CRGB& color) = 0;
    virtual void setPixelColor(uint8_t c, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void setPixelColor(uint8_t c, const CRGB& color) = 0;
};

template <template <uint8_t, EOrder> class LED_TYPE, int DATA_PIN, EOrder COLOR_ORDER = GRB>
class RGBController : public RGBControllerInterface
{
  public:
    RGBController(YarrboardApp& app, int numLeds) : RGBControllerInterface(app, "rgb")
    {
      _numLeds = numLeds;
      _leds = new CRGB[_numLeds];

      FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(_leds, _numLeds);
      FastLED.clear();
      setStatusColor(CRGB::Blue);
      FastLED.show();
    }

    ~RGBController()
    {
      if (_leds)
        delete[] _leds;
    }

    bool setup() override
    {
      return true;
    }

    void loop() override
    {
      // 10hz refresh
      if (millis() - lastRGBUpdateMillis > 1000) {
        FastLED.setBrightness(maxBrightness * _cfg.globalBrightness);
        FastLED.show();
        lastRGBUpdateMillis = millis();
      }
    }

    void generateCapabilitiesHook(JsonVariant config) override
    {
      config["rgb"] = true;
      config["rgb_count"] = _numLeds;
    };

    void setStatusColor(uint8_t r, uint8_t g, uint8_t b) override { setPixelColor(0, r, g, b); }
    void setStatusColor(const CRGB& color) override { setPixelColor(0, color); }

    void setPixelColor(uint8_t c, uint8_t r, uint8_t g, uint8_t b) override
    {
      // out of bounds?
      if (c >= _numLeds)
        return;

      _leds[c].setRGB(r, g, b);
      FastLED.setBrightness(maxBrightness * _cfg.globalBrightness);

      if (millis() - lastRGBUpdateMillis > 100) {
        FastLED.show();
        lastRGBUpdateMillis = millis();
      }
    }

    void setPixelColor(uint8_t c, const CRGB& color) override
    {
      // out of bounds?
      if (c >= _numLeds)
        return;

      _leds[c] = color;
      FastLED.setBrightness(maxBrightness * _cfg.globalBrightness);

      if (millis() - lastRGBUpdateMillis > 100) {
        FastLED.show();
        lastRGBUpdateMillis = millis();
      }
    }

  private:
    unsigned long lastRGBUpdateMillis = 0;
    CRGB* _leds = nullptr;
    int _numLeds;
};

#endif /* !YARR_RGB_H */