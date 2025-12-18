/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
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