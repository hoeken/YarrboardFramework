/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_BASE_CONTROLLER_H
#define YARR_BASE_CONTROLLER_H

#include "YarrboardConfig.h"
#include <Arduino.h>
#include <ArduinoJson.h>

class YarrboardApp;
class ConfigManager;
class MQTTController;

class BaseController
{
  public:
    BaseController(YarrboardApp& app, const char* name);

    virtual bool setup() { return true; }
    virtual void loop() {}
    const char* getName() { return _name; }

    virtual bool loadConfigHook(JsonVariant config, char* error, size_t len) { return true; };
    virtual void generateConfigHook(JsonVariant config) {};
    virtual void generateUpdateHook(JsonVariant output) {};
    virtual void generateFastUpdateHook(JsonVariant output) {};
    virtual void generateStatsHook(JsonVariant output) {};
    virtual void mqttUpdateHook(MQTTController* mqtt) {};
    virtual void haUpdateHook(MQTTController* mqtt) {};
    virtual void haGenerateDiscoveryHook(JsonVariant components, const char* uuid, MQTTController* mqtt) {};

  protected:
    YarrboardApp& _app;
    ConfigManager& _cfg;
    const char* _name;
};

#endif