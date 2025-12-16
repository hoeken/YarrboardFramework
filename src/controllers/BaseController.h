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

class YarrboardApp;
class ConfigManager;

class BaseController
{
  public:
    BaseController(YarrboardApp& app, const char* name);

    virtual bool setup();
    virtual void loop();

    const char* getName() { return _name; }

  protected:
    YarrboardApp& _app;
    ConfigManager& _cfg;
    const char* _name;
};

#endif