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

#include "controllers/BaseController.h"
#include <Arduino.h>
#include <WiFi.h>

class YarrboardApp;
class ConfigManager;

class NavicoController : public BaseController
{
  public:
    NavicoController(YarrboardApp& app);

    void loop() override;
    bool setup() override;

  private:
    unsigned long lastNavicoPublishMillis = 0;
    const int PUBLISH_PORT = 2053;
    IPAddress MULTICAST_GROUP_IP;
    WiFiUDP Udp;
};

#endif /* !YARR_NAVICO_H */