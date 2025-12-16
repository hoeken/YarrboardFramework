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

#include "ConfigManager.h"
#include "IntervalTimer.h"
#include "MQTTController.h"
#include "NTPController.h"
#include "NetworkController.h"
#include "OTAController.h"
#include "ProtocolController.h"
#include "RGBController.h"
#include "RollingAverage.h"
#include "YarrboardDebug.h"
#include "controllers/AuthController.h"
#include "controllers/BaseController.h"
#include "controllers/BuzzerController.h"
#include "controllers/HTTPController.h"

#include <cstring>         // For strcmp
#include <etl/algorithm.h> // For finding/removing
#include <etl/vector.h>

class YarrboardApp
{
  public:
    ConfigManager config;
    NetworkController network;
    HTTPController http;
    ProtocolController protocol;
    AuthController auth;
    MQTTController mqtt;
    OTAController ota;
    RGBController rgb;
    BuzzerController buzzer;
    NTPController ntp;

    YarrboardApp();

    void setup();
    void loop();

    unsigned int framerate;

    static constexpr size_t MAX_CONTROLLERS = 16;

    // Register a controller instance (non-owning).
    // Returns false if full or name duplicate.
    bool registerController(BaseController& controller);

    // Lookup by name (nullptr if not found)
    BaseController* getController(const char* name);
    const BaseController* getController(const char* name) const;

    // Remove by name (returns true if removed)
    bool removeController(const char* name);

    ConfigManager& getConfig() { return config; }
    const ConfigManager& getConfig() const { return config; }

  private:
    WebsocketPrint networkLogger;

    // various timer things.
    IntervalTimer it;
    RollingAverage loopSpeed;
    RollingAverage framerateAvg;
    unsigned long lastLoopMicros = 0;
    unsigned long lastLoopMillis = 0;

    etl::vector<BaseController*, YB_MAX_CONTROLLERS> _controllers;
};

#endif /* YarrboardApp_h */