/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "controllers/BaseController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

BaseController::BaseController(YarrboardApp& app, const char* name) : _app(app),
                                                                      _cfg(app.config),
                                                                      _name(name)
{
}

bool BaseController::setup()
{
  return true;
}

void BaseController::loop()
{
}