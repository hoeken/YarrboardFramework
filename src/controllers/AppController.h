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

#ifndef YARR_APP_CONTROLLER_H
#define YARR_APP_CONTROLLER_H

#include "controllers/BaseController.h"
#include <Arduino.h>
#include <ArduinoJson.h>

class YarrboardApp;
class ConfigManager;

class AppController : public BaseController
{
  public:
    AppController(YarrboardApp& app);

    void generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose) override;
};

#endif /* !YARR_APP_CONTROLLER_H */
