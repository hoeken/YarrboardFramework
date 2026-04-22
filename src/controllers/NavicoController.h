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

#ifndef YARR_NAVICO_H
#define YARR_NAVICO_H

// Protocol reference: https://github.com/SignalK/signalk-server/blob/master/src/interfaces/mfd_webapp.ts
#include "controllers/BaseController.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>

class YarrboardApp;
class ConfigManager;

class NavicoController : public BaseController
{
  public:
    NavicoController(YarrboardApp& app);

    void loop() override;
    bool setup() override;

    bool loadConfigHook(JsonVariant config, char* error, size_t len) override;
    void generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose) override;

    bool isMfdEnabled() const { return _app_enable_mfd; }
    void setMfdEnabled(bool v) { _app_enable_mfd = v; }

  private:
    bool _app_enable_mfd;
    unsigned long _lastPublishMillis = 0;
    static constexpr int kPublishPort = 2053;
    IPAddress _multicastGroupIp;
    WiFiUDP Udp;
};

#endif /* !YARR_NAVICO_H */
