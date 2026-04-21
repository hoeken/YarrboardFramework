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

#include "ConfigManager.h"
#include "FastLED.h"
#include "YarrboardConfig.h"
#include "controllers/BaseController.h"

class YarrboardApp;

// This is the "Interface" your App will talk to.
// It has NO template arguments.
class RGBControllerInterface : public BaseController
{
  public:
    RGBControllerInterface(YarrboardApp& app, const char* name) : BaseController(app, name) {}

    void setMaxBrightness(uint8_t v) { _maxBrightness = v; }
    uint8_t getMaxBrightness() const { return _maxBrightness; }

    virtual void setStatusColor(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void setStatusColor(const CRGB& color) = 0;
    virtual void setPixelColor(uint16_t c, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void setPixelColor(uint16_t c, const CRGB& color) = 0;

  protected:
    uint8_t _maxBrightness = 50;
};

template <template <uint8_t, EOrder> class LED_TYPE, int DATA_PIN, EOrder COLOR_ORDER = GRB>
class RGBController : public RGBControllerInterface
{
  public:
    RGBController(YarrboardApp& app, uint16_t numLeds) : RGBControllerInterface(app, "rgb")
    {
      _numLeds = numLeds;
      _leds = new CRGB[_numLeds];

      FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(_leds, _numLeds);
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
      // 1hz refresh
      if (millis() - _lastRGBUpdateMillis > 1000) {
        FastLED.show();
        _lastRGBUpdateMillis = millis();
      }
    }

    void updateBrightnessHook(float brightness) override
    {
      FastLED.setBrightness((uint8_t)(_maxBrightness * brightness));
    }

    void generateCapabilitiesHook(JsonVariant config) override
    {
      config["rgb"]["count"] = _numLeds;
    };

    void setStatusColor(uint8_t r, uint8_t g, uint8_t b) override { setPixelColor(0, r, g, b); }
    void setStatusColor(const CRGB& color) override { setPixelColor(0, color); }

    void setPixelColor(uint16_t c, uint8_t r, uint8_t g, uint8_t b) override
    {
      if (c >= _numLeds)
        return;

      _leds[c].setRGB(r, g, b);
      _showIfReady();
    }

    void setPixelColor(uint16_t c, const CRGB& color) override
    {
      if (c >= _numLeds)
        return;

      _leds[c] = color;
      _showIfReady();
    }

  private:
    void _showIfReady()
    {
      if (millis() - _lastRGBUpdateMillis > 100) {
        FastLED.show();
        _lastRGBUpdateMillis = millis();
      }
    }

    unsigned long _lastRGBUpdateMillis = 0;
    CRGB* _leds = nullptr;
    uint16_t _numLeds;
};

#endif /* !YARR_RGB_H */
