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

#ifndef YARR_BASE_CONTROLLER_H
#define YARR_BASE_CONTROLLER_H

#include "YarrboardConfig.h"
#include <Arduino.h>
#include <ArduinoJson.h>

class YarrboardApp;
class ConfigManager;
class MQTTController;

class BaseController
{
  public:
    BaseController(YarrboardApp& app, const char* name);
    virtual ~BaseController() = default;

    bool start();
    bool isStarted() const { return _started; }

    virtual bool setup() { return true; }
    virtual void loop() {}
    const char* getName() const { return _name; }

    // Guest-level config hooks (pre-scoped to guest[controller->getName()])
    virtual bool loadConfigHook(JsonVariant config, char* error, size_t len) { return true; };
    virtual void generateConfigHook(JsonVariant output) {};
    virtual void generateCapabilitiesHook(JsonVariant config) {};

    // Admin-level config hooks (pre-scoped to admin[controller->getName()])
    virtual bool loadAdminConfigHook(JsonVariant config, char* error, size_t len) { return true; }
    virtual void generateAdminConfigHook(JsonVariant output) {}

    // Validation hooks — called before load and before save; may trim/clamp values
    virtual bool validateConfigJSON(JsonVariant config, char* error, size_t len) { return true; }
    virtual bool validateAdminConfigJSON(JsonVariant config, char* error, size_t len) { return true; }

    virtual void generateUpdateHook(JsonVariant output) {};
    virtual bool needsFastUpdate() { return false; }
    virtual void generateFastUpdateHook(JsonVariant output) {};
    virtual void generateStatsHook(JsonVariant output) {};
    virtual void mqttUpdateHook(MQTTController* mqtt) {};
    virtual void haUpdateHook(MQTTController* mqtt) {};
    virtual void haGenerateDiscoveryHook(JsonVariant components, const char* uuid, MQTTController* mqtt) {};
    // many boards control lights or have displays — brightness is a universal concept here
    virtual void updateBrightnessHook(float brightness) {};

  protected:
    YarrboardApp& _app;
    ConfigManager& _cfg; // ergonomic alias for _app.config — avoids repetitive _app.config. chains in subclasses
    const char* _name;
    bool _started = false;
};

#endif