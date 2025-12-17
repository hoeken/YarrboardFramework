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
#include "controllers/AuthController.h"
#include "controllers/BaseController.h"
#include "controllers/BuzzerController.h"
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

    const char* board_name = "Yarrboard";
    const char* firmware_version = "Unknown";
    const char* hardware_version = "Unknown";
    const char* manufacturer = "Unknown";

    const char* default_hostname = "yarrboard";
    const char* default_admin_user = "admin";
    const char* default_admin_pass = "admin";
    const char* default_guest_user = "guest";
    const char* default_guest_pass = "guest";

    uint32_t update_interval = 500;

    bool enable_mfd = false;
    bool enable_http_api = false;
    bool enable_serial_api = false;
    bool enable_arduino_ota = false;
    bool enable_ssl = false;
    bool enable_mqtt = false;
    bool enable_ha_integration = false;
    bool use_hostname_as_mqtt_uuid = true;

    UserRole default_role = NOBODY;

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

    // Return a read-only reference to the vector container.
    // The vector itself is const (cannot resize), but the pointers inside are mutable.
    const etl::vector<BaseController*, YB_MAX_CONTROLLERS>& getControllers() const { return _controllers; };

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