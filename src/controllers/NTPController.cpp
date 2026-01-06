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

NTPController* NTPController::_instance = nullptr;

NTPController::NTPController(YarrboardApp& app) : BaseController(app, "ntp")
{
}

bool NTPController::setup()
{
  _instance = this; // Capture the instance for callbacks

  if (!WiFi.isConnected()) {
    YBP.println("WiFi not connected.");
    return false;
  }

  // Setup our NTP to get the current time.
  sntp_set_time_sync_notification_cb(_timeAvailableCallbackStatic);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  return true;
}

void NTPController::_timeAvailableCallbackStatic(struct timeval* t)
{
  if (_instance)
    _instance->timeAvailableCallback(t);
}

// Callback function (get's called when time adjusts via NTP)
void NTPController::timeAvailableCallback(struct timeval* t)
{
  ntp_is_ready = true;
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

  int64_t seconds = (int64_t)now;

  return (int64_t)now;
}