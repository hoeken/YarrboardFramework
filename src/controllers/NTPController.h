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

#ifndef YARR_NTP_H
#define YARR_NTP_H

#include "YarrboardConfig.h"
#include "controllers/BaseController.h"
#include "esp_sntp.h"
#include "time.h"
#include <Arduino.h>
#include <ArduinoJson.h>

struct NTPConfig {
    char ntp_server1[YB_NTP_SERVER_LENGTH];
    char ntp_server2[YB_NTP_SERVER_LENGTH];
    long gmt_offset_sec;
    int daylight_offset_sec;
};

class YarrboardApp;
class ConfigManager;

class NTPController : public BaseController
{
  public:
    NTPConfig defaults;

    NTPController(YarrboardApp& app);

    bool setup() override;
    void loop() override;

    bool isReady() { return ntp_is_ready; }
    int64_t getTime();
    void printLocalTime();

    void generateStatsHook(JsonVariant output) override;

    bool sanitizeConfigHook(JsonVariant config, char* error, size_t len) override;
    void loadConfigHook(JsonVariantConst config) override;
    void generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose) override;

  private:
    NTPConfig _config;

    bool ntp_is_ready = false;
    bool _ntpStarted = false;
    int64_t _last_sync_time = 0;

    void _startNTP();

    // --- THE CALLBACK TRAP ---
    // Libraries expecting C-style function pointers cannot take normal member functions.
    // We use a static instance pointer and static methods to bridge the gap.
    static NTPController* _instance;
    static void _timeAvailableCallbackStatic(struct timeval* t);
    void timeAvailableCallback(struct timeval* t);
};

#endif /* !YARR_NTP_H */
