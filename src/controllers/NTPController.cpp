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

#include "controllers/NTPController.h"
#include "YarrboardDebug.h"
#include <WiFi.h>

NTPController* NTPController::_instance = nullptr;

NTPController::NTPController(YarrboardApp& app) : BaseController(app, "ntp")
{
  strlcpy(defaults.ntp_server1, "pool.ntp.org", sizeof(defaults.ntp_server1));
  strlcpy(defaults.ntp_server2, "time.nist.gov", sizeof(defaults.ntp_server2));
  defaults.gmt_offset_sec = 0;
  defaults.daylight_offset_sec = 0;
  _config = defaults;
}

bool NTPController::setup()
{
  _instance = this; // Capture the instance for callbacks

  sntp_set_time_sync_notification_cb(_timeAvailableCallbackStatic);

  if (WiFi.isConnected())
    _startNTP();

  return true;
}

void NTPController::loop()
{
  if (!_ntpStarted && WiFi.isConnected())
    _startNTP();
}

void NTPController::_startNTP()
{
  configTime(_config.gmt_offset_sec, _config.daylight_offset_sec, _config.ntp_server1, _config.ntp_server2);
  _ntpStarted = true;
}

void NTPController::_timeAvailableCallbackStatic(struct timeval* t)
{
  if (_instance)
    _instance->timeAvailableCallback(t);
}

void NTPController::timeAvailableCallback(struct timeval* t)
{
  ntp_is_ready = true;
  _last_sync_time = (int64_t)t->tv_sec;
}

void NTPController::printLocalTime()
{
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    YBP.println("Failed to obtain time");
    return;
  }

  char buffer[40];
  strftime(buffer, 40, "%FT%T%z", &timeinfo);
  YBP.println(buffer);
}

int64_t NTPController::getTime()
{
  time_t now;
  time(&now);
  return (int64_t)now;
}

void NTPController::generateStatsHook(JsonVariant output)
{
  output["ntp_is_ready"] = ntp_is_ready;
  output["ntp_last_sync"] = _last_sync_time;
  output["ntp_time"] = getTime();
}

void NTPController::loadConfigHook(JsonVariantConst config)
{
  const char* v;

  v = config["ntp_server1"] | defaults.ntp_server1;
  strlcpy(_config.ntp_server1, v, sizeof(_config.ntp_server1));

  v = config["ntp_server2"] | defaults.ntp_server2;
  strlcpy(_config.ntp_server2, v, sizeof(_config.ntp_server2));

  _config.gmt_offset_sec = config["gmt_offset_sec"] | defaults.gmt_offset_sec;
  _config.daylight_offset_sec = config["daylight_offset_sec"] | defaults.daylight_offset_sec;
}

void NTPController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  output["gmt_offset_sec"] = _config.gmt_offset_sec;
  output["daylight_offset_sec"] = _config.daylight_offset_sec;

  if (role == ADMIN) {
    output["ntp_server1"] = _config.ntp_server1;
    output["ntp_server2"] = _config.ntp_server2;
  }
}