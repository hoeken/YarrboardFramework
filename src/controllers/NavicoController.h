/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
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

    virtual void loop() override;

  private:
    unsigned long lastNavicoPublishMillis = 0;
    const int PUBLISH_PORT = 2053;
    IPAddress MULTICAST_GROUP_IP;
    WiFiUDP Udp;
};

#endif /* !YARR_NAVICO_H */