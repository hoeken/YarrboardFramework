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

#include "controllers/BaseController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

BaseController::BaseController(YarrboardApp& app, const char* name) : _app(app),
                                                                      _cfg(app.config),
                                                                      _name(name)
{
}

bool BaseController::start()
{
  if (_started)
    return true;
  _started = this->setup();
  return _started;
}

bool BaseController::setup()
{
  // handleSetConfig is virtual, so even though we pass &BaseController::handleSetConfig here,
  // the vtable dispatch at call time will invoke the child class override if one exists.
  snprintf(_configCommand, sizeof(_configCommand), "set_%s_config", getName());
  return _app.protocol.registerCommand(ADMIN, _configCommand, this, &BaseController::handleSetConfig);
}

void BaseController::handleSetConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  char error[128] = "Unknown";
  if (!input["config"].is<JsonObjectConst>())
    return _app.protocol.generateErrorJSON(output, "'config' is a required parameter.");

  // we need a mutable format for the validation
  JsonDocument config;
  config.set(input["config"]);

  // run it through our sanitizer
  if (!sanitizeConfigHook(config, error, sizeof(error)))
    return _app.protocol.generateErrorJSON(output, error);

  // our controller callback for extra goodness
  if (!handleSetConfigSuccessCallback(config, output, context, error, sizeof(error)))
    return ProtocolController::generateErrorJSON(output, error);

  // load our data into our controller
  loadConfigHook(config);

  // now save it all.
  if (!_app.config.saveConfig(error, sizeof(error)))
    ProtocolController::generateErrorJSON(output, error);
}