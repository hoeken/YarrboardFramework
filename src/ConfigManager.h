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

#ifndef ConfigManager_h
#define ConfigManager_h

#include "YarrboardConfig.h"
#include "controllers/AuthController.h"
#include "controllers/BaseController.h"
#include "etl/array.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

class YarrboardApp;

class ConfigManager : public BaseController
{
  public:
    Preferences preferences;

    ConfigManager(YarrboardApp& app);

    // Lifecycle
    bool setup() override;

    // Core Config Logic
    bool saveConfig(char* error, size_t len);
    bool loadConfigFromFile(const char* file, char* error, size_t len);

    // JSON Loading
    bool loadConfigFromJSON(JsonVariant config, char* error, size_t len);
    bool loadNetworkConfigFromJSON(JsonVariant config, char* error, size_t len);
    bool loadAppConfigFromJSON(JsonVariant config, char* error, size_t len);
    bool loadBoardConfigFromJSON(JsonVariant config, char* error, size_t len);

    // JSON Generation
    void generateFullConfig(JsonVariant output);
    void generateBoardConfig(JsonVariant output);
    void generateAppConfig(JsonVariant output);
    void generateNetworkConfig(JsonVariant output);

    bool isFirstBoot() const { return _is_first_boot; }
    void setFirstBoot(bool v) { _is_first_boot = v; }

    const char* getBoardName() const { return _board_name; }
    void setBoardName(const char* name) { strlcpy(_board_name, name, sizeof(_board_name)); }

    const String& getAppTheme() const { return _app_theme; }
    void setAppTheme(const String& theme) { _app_theme = theme; }

    float getGlobalBrightness() const { return _global_brightness; }
    void setGlobalBrightness(float b) { _global_brightness = b; }

    const char* getStartupMelody() const { return _startup_melody; }
    void setStartupMelody(const char* melody) { strlcpy(_startup_melody, melody, sizeof(_startup_melody)); }

    bool isAppMfdEnabled() const { return _app_enable_mfd; }
    void setAppMfdEnabled(bool v) { _app_enable_mfd = v; }

  private:
    YarrboardApp& _app;
    bool _is_first_boot;
    char _board_name[YB_BOARD_NAME_LENGTH];
    unsigned int _app_update_interval;
    String _app_theme = "light";
    float _global_brightness = 1.0;
    char _startup_melody[YB_BOARD_NAME_LENGTH];
    bool _app_enable_mfd;
};

#endif