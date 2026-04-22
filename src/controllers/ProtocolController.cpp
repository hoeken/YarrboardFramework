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

#include "controllers/ProtocolController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"
#include "controllers/BuzzerController.h"
#include "controllers/OTAController.h"
#include "utility.h"

ProtocolController::ProtocolController(YarrboardApp& app) : BaseController(app, "protocol")
{
  _enable_serial = app.enable_serial_api;
}

bool ProtocolController::setup()
{
  registerCommand(NOBODY, "ping", this, &ProtocolController::handlePing);
  registerCommand(NOBODY, "hello", this, &ProtocolController::handleHello);

  registerCommand(GUEST, "get_config", this, &ProtocolController::handleGetConfig);
  registerCommand(GUEST, "get_stats", this, &ProtocolController::handleGetStats);
  registerCommand(GUEST, "get_update", this, &ProtocolController::handleGetUpdate);
  registerCommand(GUEST, "set_theme", this, &ProtocolController::handleSetTheme);
  registerCommand(GUEST, "set_brightness", this, &ProtocolController::handleSetBrightness);

  registerCommand(ADMIN, "set_general_config", this, &ProtocolController::handleSetGeneralConfig);
  registerCommand(ADMIN, "save_config", this, &ProtocolController::handleSaveConfig);
  registerCommand(ADMIN, "get_full_config", this, &ProtocolController::handleGetFullConfig);
  registerCommand(ADMIN, "set_runtime_config", this, &ProtocolController::handleSetMiscellaneousConfig);
  registerCommand(ADMIN, "restart", this, &ProtocolController::handleRestart);
  registerCommand(ADMIN, "factory_reset", this, &ProtocolController::handleFactoryReset);

  return true;
}

void ProtocolController::loop()
{
  // lookup our info periodically
  unsigned int messageDelta = millis() - previousMessageMillis;
  if (messageDelta >= 1000) {

    // for keeping track.
    receivedMessagesPerSecond = receivedMessages;
    receivedMessages = 0;
    sentMessagesPerSecond = sentMessages;
    sentMessages = 0;

    previousMessageMillis = millis();
  }

  // check to see if we need to send one.
  bool doFastUpdate = false;
  for (const auto& entry : _app.getControllers()) {
    if (entry.controller->needsFastUpdate()) {
      doFastUpdate = true;
      break;
    }
  }

  if (doFastUpdate) {
    unsigned int fastUpdateDelta = millis() - previousFastUpdateMillis;
    if (fastUpdateDelta >= YB_FAST_UPDATE_MIN_INTERVAL_MS) {
      previousFastUpdateMillis = millis();
      sendFastUpdate();
    }
  }

  // any serial port customers?
  if (_enable_serial) {
    if (Serial.available() > 0)
      handleSerialJson();
  }
}

bool ProtocolController::registerCommand(UserRole role, const char* command, ProtocolMessageHandler handler)
{
  if (commandMap.full()) {
    YBP.printf("❌ Error: Protocol command list is full. (%s)\n", command);
    return false;
  }

  if (hasCommand(command))
    YBP.printf("⚠️ Warning: Overwriting protocol command '%s'\n", command);

  commandMap[command] = {role, handler};
  return true;
}

bool ProtocolController::unregisterCommand(const char* command)
{
  return commandMap.erase(command) > 0;
}

bool ProtocolController::hasCommand(const char* command)
{
  return commandMap.find(command) != commandMap.end();
}

void ProtocolController::printCommands()
{
  YBP.println("Protocol Commands:");
  for (const auto& kvp : commandMap)
    YBP.printf("%-6s | %s\n", _app.auth.getRoleText(kvp.second.role), kvp.first);
}

void ProtocolController::incrementSentMessages()
{
  // keep track!
  sentMessages++;
  totalSentMessages++;
}

void ProtocolController::handleSerialJson()
{
  JsonDocument input;
  DeserializationError err = deserializeJson(input, Serial);
  JsonDocument output;

  // ignore newlines with serial.
  if (err) {
    if (strcmp(err.c_str(), "EmptyInput")) {
      char error[64];
      sprintf(error, "deserializeJson() failed with code %s", err.c_str());
      generateErrorJSON(output, error);
      serializeJson(output, Serial);
    }
  } else {
    ProtocolContext context;
    context.mode = YBP_MODE_SERIAL;
    handleReceivedJSON(input, output, context);

    // we can have empty responses
    if (output.size()) {
      serializeJson(output, Serial);

      incrementSentMessages();
    }
  }
}

void ProtocolController::handleReceivedJSON(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  // make sure its correct
  if (!input["cmd"].is<String>())
    return generateErrorJSON(output, "'cmd' is a required parameter.");

  // what is your command?
  const char* cmd = input["cmd"];

  // let the client keep track of messages
  if (input["msgid"].is<unsigned int>()) {
    unsigned int msgid = input["msgid"];
    output["status"] = "ok";
    output["msgid"] = msgid;
  }

  // keep track!
  receivedMessages++;
  totalReceivedMessages++;

  // what would you say you do around here?
  context.role = _app.auth.getUserRole(input, context.mode, context.clientId);

  // Try to find the command in the new map system
  auto it = commandMap.find(cmd);

  // If FOUND, process it here and return.
  // If NOT found, skip this block and let the legacy code handle it.
  if (it != commandMap.end()) {

    // We found the command, so we must enforce auth.
    if (!_app.auth.hasPermission(it->second.role, context.role)) {
      String error = "Unauthorized for " + String(cmd);
      return generateErrorJSON(output, error.c_str());
    }

    // Execute Handler
    if (it->second.handler) {
      it->second.handler(input, output, context);
      return;
    }
  }

  // if we got here, no bueno.
  String error = "Invalid command: " + String(cmd);
  return generateErrorJSON(output, error.c_str());
}

void ProtocolController::handleHello(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  output["msg"] = "hello";
  output["role"] = _app.auth.getRoleText(context.role);
  output["default_role"] = _app.auth.getRoleText(_app.auth.getDefaultRole());
  output["name"] = _cfg.getBoardName();
  output["brightness"] = _cfg.getGlobalBrightness();
  output["firmware_version"] = _app.firmware_version;
}

void ProtocolController::handleGetConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  output["msg"] = "config";

  // v2 config sections
  _app.generateAppConfig(output);
  _cfg.generateConfig(output, context.role, ConfigPurpose::UI_VIEW);
}

void ProtocolController::handleGetStats(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  // some basic statistics and info
  output["msg"] = "stats";
  output["uuid"] = _app.network.getUUID();
  output["received_message_total"] = totalReceivedMessages;
  output["received_message_mps"] = receivedMessagesPerSecond;
  output["sent_message_total"] = totalSentMessages;
  output["sent_message_mps"] = sentMessagesPerSecond;
  output["websocket_client_count"] = _app.http.websocketClientCount.load();
  output["http_client_count"] = _app.http.httpClientCount.load() - _app.http.websocketClientCount.load();
  output["fps"] = (int)_app.framerate;
  output["uptime"] = esp_timer_get_time();
  output["heap_size"] = ESP.getHeapSize();
  output["free_heap"] = ESP.getFreeHeap();
  output["min_free_heap"] = ESP.getMinFreeHeap();
  output["max_alloc_heap"] = ESP.getMaxAllocHeap();
  output["rssi"] = WiFi.RSSI();

  // what is our IP address?
  if (!strcmp(_app.network.getWifiMode(), "ap"))
    output["ip_address"] = _app.network.getApIP();
  else
    output["ip_address"] = WiFi.localIP();

  for (const auto& entry : _app.getControllers()) {
    entry.controller->generateStatsHook(output);
  }
}

void ProtocolController::handleGetUpdate(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  output["msg"] = "update";
  output["uptime"] = esp_timer_get_time();

  for (const auto& entry : _app.getControllers()) {
    entry.controller->generateUpdateHook(output);
  }
}

void ProtocolController::handleGetFullConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  output["msg"] = "full_config";
  _cfg.generateConfig(output, context.role, ConfigPurpose::FIRMWARE);
}

void ProtocolController::handleSetGeneralConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!input["board_name"].is<String>())
    return generateErrorJSON(output, "'board_name' is a required parameter");

  // is it too long?
  if (strlen(input["board_name"]) > YB_BOARD_NAME_LENGTH - 1) {
    char error[50];
    sprintf(error, "Maximum board name length is %d characters.", YB_BOARD_NAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  _cfg.setBoardName(input["board_name"] | _app.board_name);

  BuzzerController* buzzer = static_cast<BuzzerController*>(_app.getController("buzzer"));
  if (buzzer && input["startup_melody"].is<const char*>())
    buzzer->setStartupMelody(input["startup_melody"]);
  else if (buzzer)
    buzzer->setStartupMelody(_app.default_melody);

  // save it to file.
  char error[128];
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // give them the updated config
  _cfg.generateConfig(output, context.role, ConfigPurpose::UI_VIEW);
}

void ProtocolController::handleSetMiscellaneousConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  char error[128] = "Unknown";

  _enable_serial = input["app_enable_serial"] | _app.enable_serial_api;
  _app.ota.setEnabled(input["app_enable_ota"] | _app.enable_arduino_ota);

  // save it to file.
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // init our ota.
  if (_app.ota.isEnabled())
    _app.ota.setup();
  else
    _app.ota.end();

  generateSuccessJSON(output, "runtimeellaneous config saved.");
}

void ProtocolController::handleSaveConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  char error[128] = "Unknown";

  // get the config object specifically.
  JsonDocument cfg;

  // we need one thing...
  if (!input["config"].is<String>())
    return generateErrorJSON(output, "'config' is a required parameter");

  // was there a problem, officer?
  DeserializationError err = deserializeJson(cfg, input["config"]);
  if (err) {
    snprintf(error, sizeof(error), "deserializeJson() failed with code %s", err.c_str());
    return generateErrorJSON(output, error);
  }

  // test the validity by loading it...
  if (!_cfg.loadConfigFromJSON(cfg, error, sizeof(error)))
    return generateErrorJSON(output, error);

  // write it!
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // restart the board.
  ESP.restart();
}

void ProtocolController::handleRestart(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  YBP.println("Restarting board.");

  ESP.restart();
}

void ProtocolController::handleFactoryReset(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  // delete all our prefs
  _cfg.preferences.clear();
  _cfg.preferences.end();

  // clean up littlefs
  LittleFS.format();

  // restart the board.
  ESP.restart();
}

void ProtocolController::handleSetTheme(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!input["theme"].is<String>())
    return generateErrorJSON(output, "'theme' is a required parameter");

  String temp = input["theme"];

  if (temp != "light" && temp != "dark")
    return generateErrorJSON(output,
      "'theme' must either be 'light' or 'dark'");

  _cfg.setAppTheme(temp);

  sendThemeUpdate();
}

void ProtocolController::handleSetBrightness(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (input["brightness"].is<float>()) {
    float brightness = input["brightness"];

    // what do we hate?  va-li-date!
    if (brightness < 0)
      return generateErrorJSON(output, "Brightness must be >= 0");
    else if (brightness > 1)
      return generateErrorJSON(output, "Brightness must be <= 1");

    _cfg.setGlobalBrightness(brightness);

    // TODO: need to put this on a time delay
    // preferences.putFloat("brightness", globalBrightness);

    for (const auto& entry : _app.getControllers()) {
      entry.controller->updateBrightnessHook(brightness);
    }
    sendBrightnessUpdate();
  } else
    return generateErrorJSON(output, "'brightness' is a required parameter.");
}

bool ProtocolController::loadConfigHook(JsonVariant config, char* error, size_t len)
{
  _enable_serial = config["app_enable_serial"] | _app.enable_serial_api;
  return true;
}

void ProtocolController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  if (role == ADMIN)
    output["app_enable_serial"] = _enable_serial;
}

void ProtocolController::generateErrorJSON(JsonVariant output, const char* error)
{
  output["msg"] = "status";
  output["status"] = "error";
  output["message"] = error;
}

void ProtocolController::generateSuccessJSON(JsonVariant output, const char* success)
{
  output["msg"] = "status";
  output["status"] = "success";
  output["message"] = success;
}

void ProtocolController::handlePing(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  output["pong"] = millis();
}

void ProtocolController::sendThemeUpdate()
{
  JsonDocument output;
  output["msg"] = "set_theme";
  output["theme"] = _cfg.getAppTheme();

  sendToAll(output, NOBODY);
}

void ProtocolController::sendBrightnessUpdate()
{
  JsonDocument output;
  output["msg"] = "set_brightness";
  output["brightness"] = _cfg.getGlobalBrightness();

  sendToAll(output, NOBODY);
}

void ProtocolController::sendFastUpdate()
{
  JsonDocument output;

  output["msg"] = "update";
  output["fast"] = 1;
  output["uptime"] = esp_timer_get_time();

  for (const auto& entry : _app.getControllers()) {
    entry.controller->generateFastUpdateHook(output);
  }

  sendToAll(output, GUEST);
}

void ProtocolController::sendDebug(const char* message)
{
  JsonDocument output;
  output["debug"] = message;

  sendToAll(output, NOBODY);
}

void ProtocolController::sendToAll(JsonVariantConst output, UserRole min_receiver_role)
{
  // dynamically allocate our buffer
  size_t jsonSize = measureJson(output);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(output, jsonBuffer, jsonSize + 1);
    sendToAll(jsonBuffer, min_receiver_role);
    free(jsonBuffer);
  } else {
    // dont call YBP b/c loops...
    Serial.println("Error allocating in ProtocolController::sendToAll");
  }
}

void ProtocolController::sendToAll(const char* jsonString, UserRole min_receiver_role)
{
  _app.http.sendToAllWebsockets(jsonString, min_receiver_role);

  if (_enable_serial && _app.auth.getSerialRole() >= min_receiver_role)
    Serial.println(jsonString);
}
