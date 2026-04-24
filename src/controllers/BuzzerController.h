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

#ifndef YARR_BUZZER_H
#define YARR_BUZZER_H

#include "YarrboardConfig.h"
#include "controllers/BaseController.h"
#include "controllers/ProtocolController.h"
#include "driver/ledc.h"
#include <ArduinoJson.h>

struct Note {
    uint16_t freqHz;
    uint16_t ms;
};

struct Melody {
    const char* name;
    const Note* seq;
    size_t len;
};

#define YB_MAX_MELODY_LENGTH 100
#define LEDC_RES_BITS        10
#define BUZZER_DUTY          512
#define MELODY_ENTRY(x)      {#x, x, sizeof(x) / sizeof(Note)}

struct BuzzerConfig {
    char startup_melody[YB_MELODY_LENGTH];
};

class YarrboardApp;
class ConfigManager;

// Forward declaration
void BuzzerTask(void* pv);

class BuzzerController : public BaseController
{
  public:
    BuzzerConfig defaults;

    BuzzerController(YarrboardApp& app);

    bool setup() override;
    bool sanitizeConfigHook(JsonVariant config, char* error, size_t len) override;
    void loadConfigHook(JsonVariantConst config) override;
    void generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose) override;
    void generateCapabilitiesHook(JsonVariant config) override;

    const char* getStartupMelody() const { return _config.startup_melody; }
    void setStartupMelody(const char* melody) { strlcpy(_config.startup_melody, melody, sizeof(_config.startup_melody)); }

    bool hasMelody(const char* melody);
    bool playMelodyByName(const char* melody);
    void generateMelodyJSON(JsonVariant output);

    void handlePlaySound(JsonVariantConst input, JsonVariant output, ProtocolContext context);

    byte getBuzzerPin() const { return _buzzerPin; }
    bool isActive() const { return _isActive; }
    void setBuzzerPin(byte pin) { _buzzerPin = pin; }
    void setActive(bool active);

    // Make the task a friend so it can access private static members
    friend void BuzzerTask(void* pv);

  private:
    byte _buzzerPin = 0;
    bool _isActive = false;
    BuzzerConfig _config;

    const Melody* _melodyTable = nullptr;
    size_t _melodyCount = 0;

    void playMelody(const Note* seq, size_t len);
    void buzzerMute();
    void buzzerTone(uint16_t freqHz);
};

#endif /* !YARR_BUZZER_H */