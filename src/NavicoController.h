/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_NAVICO_H
#define YARR_NAVICO_H

#include <Arduino.h>
#include <WiFi.h>

class YarrboardApp;
class ConfigManager;

class NavicoController
{
  public:
    NavicoController(YarrboardApp& app, ConfigManager& config);

    void setup();
    void loop();

  private:
    YarrboardApp& _app;
    ConfigManager& _config;

    unsigned long lastNavicoPublishMillis = 0;
    const int PUBLISH_PORT = 2053;
    IPAddress MULTICAST_GROUP_IP;
    WiFiUDP Udp;
};

#endif /* !YARR_NAVICO_H */