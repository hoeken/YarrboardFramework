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
#include "RollingAverage.h"
#include "YarrboardDebug.h"
#include "controllers/AppController.h"
#include "controllers/AuthController.h"
#include "controllers/BaseController.h"
#include "controllers/BuzzerController.h"
#include "controllers/DebugController.h"
#include "controllers/HTTPController.h"
#include "controllers/MQTTController.h"
#include "controllers/NTPController.h"
#include "controllers/NetworkController.h"
#include "controllers/OTAController.h"
#include "controllers/ProtocolController.h"
#include "controllers/RGBController.h"

#include <cstring>         // For strcmp
#include <etl/algorithm.h> // For finding/removing
#include <etl/vector.h>

class YarrboardApp
{
  public:
    struct ControllerEntry {
        BaseController* controller;
        uint8_t order;

        ControllerEntry() : controller(nullptr), order(0) {}
        ControllerEntry(BaseController* c, uint8_t o) : controller(c), order(o) {}

        bool operator<(const ControllerEntry& other) const { return order < other.order; }
    };

    const char* firmware_version = "Unknown";
    const char* hardware_version = "Unknown";
    const char* manufacturer = "Unknown";

    const char* hardware_url = "";
    const char* project_name = "Yarrboard";
    const char* project_url = "https://github.com/hoeken/yarrboard";
    const char* git_url = "";

    uint32_t update_interval = 500;

    ConfigManager config;
    DebugController debug;
    NetworkController network;
    HTTPController http;
    ProtocolController protocol;
    AuthController auth;
    MQTTController mqtt;
    OTAController ota;
    NTPController ntp;
    AppController app;

    YarrboardApp();

    void setup();
    void loop();

    unsigned int framerate;

    // Register a controller instance (non-owning).
    // Returns false if full or name duplicate.
    // Controllers are sorted by order (lower values run first).
    bool registerController(BaseController& controller, uint8_t order = 100);

    // Lookup by name (nullptr if not found)
    BaseController* getController(const char* name);
    const BaseController* getController(const char* name) const;

    // Return a read-only reference to the vector container.
    // The vector itself is const (cannot resize), but the entries inside are mutable.
    const etl::vector<ControllerEntry, YB_MAX_CONTROLLERS>& getControllers() const { return _controllers; };

    // Remove by name (returns true if removed)
    bool removeController(const char* name);

    ConfigManager& getConfig() { return config; }
    const ConfigManager& getConfig() const { return config; }

    void setDefaultBoardName(const char* name) { strlcpy(config.defaults.board_name, name, sizeof(config.defaults.board_name)); }
    void setDefaultHostname(const char* hostname) { strlcpy(network.defaults.local_hostname, hostname, sizeof(network.defaults.local_hostname)); }

    void setStatusColor(uint8_t r, uint8_t g, uint8_t b);
    void setStatusColor(const CRGB& color);

    void playMelody(const char* melody);

  private:
    WebsocketPrint networkLogger;

    // various timer things.
    RollingAverage loopSpeed;
    RollingAverage framerateAvg;
    unsigned long lastLoopMicros = 0;
    unsigned long lastLoopMillis = 0;

    etl::vector<ControllerEntry, YB_MAX_CONTROLLERS> _controllers;

    void _handleImprov();
    BaseController* _findController(const char* name);
};

#endif /* YarrboardApp_h */