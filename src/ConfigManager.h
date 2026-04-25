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

struct ConfigManagerConfig {
    bool is_first_boot;
    char board_name[YB_BOARD_NAME_LENGTH];
};

class YarrboardApp;

class ConfigManager : public BaseController
{
  public:
    // Public to allow controllers direct access to the ESP32 Preferences API.
    Preferences preferences;

    ConfigManagerConfig defaults;

    ConfigManager(YarrboardApp& app);

    // Lifecycle
    bool setup() override;

    // Core Config Logic
    void generateConfig(JsonVariant output, UserRole role, ConfigPurpose purpose);
    void generateCapabilities(JsonVariant output);
    bool sanitizeConfig(JsonVariant config, char* error, size_t len);
    bool saveConfig(char* error, size_t len);
    bool loadConfigFromFile(const char* file, char* error, size_t len);

    // JSON Loading
    bool loadConfigFromJSON(JsonVariant config, char* error, size_t len);
    bool loadV1Config(JsonVariant root, char* error, size_t len);
    bool loadV2Config(JsonVariant root, char* error, size_t len);

    // our personal hooks as a controller.
    bool sanitizeConfigHook(JsonVariant config, char* error, size_t len) override;
    void generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose) override;
    void loadConfigHook(JsonVariantConst config) override;
    bool handleSetConfigSuccessCallback(JsonVariantConst input, JsonVariant output, ProtocolContext context, char* error, size_t len) override;

    bool isFirstBoot() const { return _config.is_first_boot; }
    void setFirstBoot(bool v) { _config.is_first_boot = v; }

    const char* getBoardName() const { return _config.board_name; }
    void setBoardName(const char* name) { strlcpy(_config.board_name, name, sizeof(_config.board_name)); }

    const String& getAppTheme() const { return _app_theme; }
    void setAppTheme(const String& theme) { _app_theme = theme; }

    float getGlobalBrightness() const { return _global_brightness; }
    void setGlobalBrightness(float b) { _global_brightness = b; }

    uint32_t getSchemaVersion() const { return _schema_version; }

  private:
    YarrboardApp& _app;

    ConfigManagerConfig _config;

    String _app_theme = "light";
    float _global_brightness = 1.0;
    uint32_t _schema_version = 2;
};

#endif
