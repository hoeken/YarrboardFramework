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

#ifndef YarrboardApp_h
#define YarrboardApp_h

#include "AuthController.h"
#include "ConfigManager.h"
#include "HTTPController.h"
#include "IntervalTimer.h"
#include "MQTTController.h"
#include "NavicoController.h"
#include "NetworkController.h"
#include "OTAController.h"
#include "ProtocolController.h"
#include "RGBController.h"
#include "RollingAverage.h"
#include "YarrboardDebug.h"
#include "BuzzerController.h"

class YarrboardApp
{
  public:
    ConfigManager config;
    NetworkController network;
    HTTPController http;
    ProtocolController protocol;
    AuthController auth;
    NavicoController navico;
    MQTTController mqtt;
    OTAController ota;
    RGBController rgb;
    BuzzerController buzzer;

    YarrboardApp();

    void setup();
    void loop();

    unsigned int framerate;

  private:
    WebsocketPrint networkLogger;

    // various timer things.
    IntervalTimer it;
    RollingAverage loopSpeed;
    RollingAverage framerateAvg;
    unsigned long lastLoopMicros = 0;
    unsigned long lastLoopMillis = 0;
};

#endif /* YarrboardApp_h */