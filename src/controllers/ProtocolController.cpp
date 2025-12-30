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
#include "controllers/MQTTController.h"
#include "controllers/OTAController.h"
#include "utility.h"

ProtocolController::ProtocolController(YarrboardApp& app) : BaseController(app, "protocol")
{
}

bool ProtocolController::setup()
{
  registerCommand(NOBODY, "ping", this, &ProtocolController::handlePing);
  registerCommand(NOBODY, "hello", this, &ProtocolController::handleHello);
  registerCommand(NOBODY, "login", this, &ProtocolController::handleLogin);
  registerCommand(NOBODY, "logout", this, &ProtocolController::handleLogout);

  registerCommand(GUEST, "get_config", this, &ProtocolController::handleGetConfig);
  registerCommand(GUEST, "get_stats", this, &ProtocolController::handleGetStats);
  registerCommand(GUEST, "get_update", this, &ProtocolController::handleGetUpdate);
  registerCommand(GUEST, "set_theme", this, &ProtocolController::handleSetTheme);
  registerCommand(GUEST, "set_brightness", this, &ProtocolController::handleSetBrightness);

  registerCommand(ADMIN, "set_general_config", this, &ProtocolController::handleSetGeneralConfig);
  registerCommand(ADMIN, "save_config", this, &ProtocolController::handleSaveConfig);
  registerCommand(ADMIN, "get_full_config", this, &ProtocolController::handleGetFullConfig);
  registerCommand(ADMIN, "get_network_config", this, &ProtocolController::handleGetNetworkConfig);
  registerCommand(ADMIN, "get_app_config", this, &ProtocolController::handleGetAppConfig);
  registerCommand(ADMIN, "set_network_config", this, &ProtocolController::handleSetNetworkConfig);
  registerCommand(ADMIN, "set_authentication_config", this, &ProtocolController::handleSetAuthenticationConfig);
  registerCommand(ADMIN, "set_webserver_config", this, &ProtocolController::handleSetWebServerConfig);
  registerCommand(ADMIN, "set_mqtt_config", this, &ProtocolController::handleSetMQTTConfig);
  registerCommand(ADMIN, "set_misc_config", this, &ProtocolController::handleSetMiscellaneousConfig);
  registerCommand(ADMIN, "restart", this, &ProtocolController::handleRestart);
  registerCommand(ADMIN, "factory_reset", this, &ProtocolController::handleFactoryReset);
  registerCommand(ADMIN, "ota_start", this, &ProtocolController::handleOTAStart);

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

  if (doFastUpdate)
    sendFastUpdate();

  // any serial port customers?
  if (_cfg.app_enable_serial) {
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

      sentMessages++;
      totalSentMessages++;
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
  output["default_role"] = _app.auth.getRoleText(_cfg.app_default_role);
  output["name"] = _cfg.board_name;
  output["brightness"] = _cfg.globalBrightness;
  output["firmware_version"] = _app.firmware_version;
}

void ProtocolController::handleGetConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  generateConfigMessage(output);
}

void ProtocolController::handleGetStats(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  // some basic statistics and info
  output["msg"] = "stats";
  output["uuid"] = _cfg.uuid;
  output["received_message_total"] = totalReceivedMessages;
  output["received_message_mps"] = receivedMessagesPerSecond;
  output["sent_message_total"] = totalSentMessages;
  output["sent_message_mps"] = sentMessagesPerSecond;
  output["websocket_client_count"] = _app.http.websocketClientCount;
  output["http_client_count"] = _app.http.httpClientCount - _app.http.websocketClientCount;
  output["fps"] = (int)_app.framerate;
  output["uptime"] = esp_timer_get_time();
  output["heap_size"] = ESP.getHeapSize();
  output["free_heap"] = ESP.getFreeHeap();
  output["min_free_heap"] = ESP.getMinFreeHeap();
  output["max_alloc_heap"] = ESP.getMaxAllocHeap();
  output["rssi"] = WiFi.RSSI();
  if (_cfg.app_enable_mqtt)
    output["mqtt_connected"] = _app.mqtt.isConnected();

  // what is our IP address?
  if (!strcmp(_cfg.wifi_mode, "ap"))
    output["ip_address"] = _app.network.apIP;
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
  // build our message
  output["msg"] = "full_config";
  JsonObject cfg = output["config"].to<JsonObject>();

  // separate call to make a clean config.
  _cfg.generateFullConfig(cfg);
}

void ProtocolController::handleGetNetworkConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  output["msg"] = "network_config";
  _cfg.generateNetworkConfig(output);
}

void ProtocolController::handleGetAppConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  output["msg"] = "app_config";
  _cfg.generateAppConfig(output);
}

void ProtocolController::handleSetGeneralConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!input["board_name"].is<String>())
    return generateErrorJSON(output, "'board_name' is a required parameter");

  // is it too long?
  if (strlen(input["board_name"]) > YB_BOARD_NAME_LENGTH - 1) {
    char error[50];
    sprintf(error, "Maximum board name length is %s characters.", YB_BOARD_NAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // update variable
  strlcpy(_cfg.board_name, input["board_name"] | _app.board_name, sizeof(_cfg.board_name));
  strlcpy(_cfg.startup_melody, input["startup_melody"] | _app.default_melody, sizeof(_cfg.startup_melody));

  // save it to file.
  char error[128];
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // give them the updated config
  generateConfigMessage(output);
}

void ProtocolController::handleSetNetworkConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  // clear our first boot flag since they submitted the network page.
  _cfg.is_first_boot = false;

  char error[128];

  // error checking
  if (!input["wifi_mode"].is<String>())
    return generateErrorJSON(output, "'wifi_mode' is a required parameter");
  if (!input["wifi_ssid"].is<String>())
    return generateErrorJSON(output, "'wifi_ssid' is a required parameter");
  if (!input["wifi_pass"].is<String>())
    return generateErrorJSON(output, "'wifi_pass' is a required parameter");
  if (!input["local_hostname"].is<String>())
    return generateErrorJSON(output, "'local_hostname' is a required parameter");

  // is it too long?
  if (strlen(input["wifi_ssid"]) > YB_WIFI_SSID_LENGTH - 1) {
    sprintf(error, "Maximum wifi ssid length is %s characters.", YB_WIFI_SSID_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  if (strlen(input["wifi_pass"]) > YB_WIFI_PASSWORD_LENGTH - 1) {
    sprintf(error, "Maximum wifi password length is %s characters.", YB_WIFI_PASSWORD_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  if (strlen(input["local_hostname"]) > YB_HOSTNAME_LENGTH - 1) {
    sprintf(error, "Maximum hostname length is %s characters.", YB_HOSTNAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // get our data
  char new_wifi_mode[16];
  char new_wifi_ssid[YB_WIFI_SSID_LENGTH];
  char new_wifi_pass[YB_WIFI_PASSWORD_LENGTH];

  strlcpy(new_wifi_mode, input["wifi_mode"] | YB_DEFAULT_AP_MODE, sizeof(new_wifi_mode));
  strlcpy(new_wifi_ssid, input["wifi_ssid"] | YB_DEFAULT_AP_SSID, sizeof(new_wifi_ssid));
  strlcpy(new_wifi_pass, input["wifi_pass"] | YB_DEFAULT_AP_PASS, sizeof(new_wifi_pass));
  strlcpy(_cfg.local_hostname, input["local_hostname"] | _app.default_hostname, sizeof(_cfg.local_hostname));

  // make sure we can connect before we save
  if (!strcmp(new_wifi_mode, "client")) {
    // did we change username/password?
    if (strcmp(new_wifi_ssid, _cfg.wifi_ssid) || strcmp(new_wifi_pass, _cfg.wifi_pass)) {
      // try connecting.
      YBP.printf("Trying new wifi %s / %s\n", new_wifi_ssid, new_wifi_pass);
      if (_app.network.connectToWifi(new_wifi_ssid, new_wifi_pass)) {
        // changing modes?
        if (!strcmp(_cfg.wifi_mode, "ap"))
          WiFi.softAPdisconnect();

        // save for local use
        strlcpy(_cfg.wifi_mode, new_wifi_mode, sizeof(_cfg.wifi_mode));
        strlcpy(_cfg.wifi_ssid, new_wifi_ssid, sizeof(_cfg.wifi_ssid));
        strlcpy(_cfg.wifi_pass, new_wifi_pass, sizeof(_cfg.wifi_pass));

        // save it to file.
        if (!_cfg.saveConfig(error, sizeof(error)))
          return generateErrorJSON(output, error);
      }
      // nope, setup our wifi back to default.
      else {
        _app.network.connectToWifi(_cfg.wifi_ssid, _cfg.wifi_pass); // go back to our old wifi.
        _app.network.startServices();
        return generateErrorJSON(output, "Can't connect to new WiFi.");
      }
    } else {
      // save it to file.
      if (!_cfg.saveConfig(error, sizeof(error)))
        return generateErrorJSON(output, error);
    }
  }
  // okay, AP mode is easier
  else {
    // save for local use.
    strlcpy(_cfg.wifi_mode, new_wifi_mode, sizeof(_cfg.wifi_mode));
    strlcpy(_cfg.wifi_ssid, new_wifi_ssid, sizeof(_cfg.wifi_ssid));
    strlcpy(_cfg.wifi_pass, new_wifi_pass, sizeof(_cfg.wifi_pass));

    // switch us into AP mode
    _app.network.setupWifi();

    if (!_cfg.saveConfig(error, sizeof(error)))
      return generateErrorJSON(output, error);

    return generateSuccessJSON(output, "AP mode successful, please connect to new network.");
  }
}

void ProtocolController::handleSetAuthenticationConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!input["admin_user"].is<String>())
    return generateErrorJSON(output, "'admin_user' is a required parameter");
  if (!input["admin_pass"].is<String>())
    return generateErrorJSON(output, "'admin_pass' is a required parameter");
  if (!input["guest_user"].is<String>())
    return generateErrorJSON(output, "'guest_user' is a required parameter");
  if (!input["guest_pass"].is<String>())
    return generateErrorJSON(output, "'guest_pass' is a required parameter");
  if (!input["default_role"].is<String>())
    return generateErrorJSON(output, "'default_role' is a required parameter");

  // username length checker
  if (strlen(input["admin_user"]) > YB_USERNAME_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum admin username length is %s characters.", YB_USERNAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // password length checker
  if (strlen(input["admin_pass"]) > YB_PASSWORD_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum admin password length is %s characters.", YB_PASSWORD_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // username length checker
  if (strlen(input["guest_user"]) > YB_USERNAME_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum guest username length is %s characters.", YB_USERNAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // password length checker
  if (strlen(input["guest_pass"]) > YB_PASSWORD_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum guest password length is %s characters.", YB_PASSWORD_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // get our data
  strlcpy(_cfg.admin_user, input["admin_user"] | _app.default_admin_user, sizeof(_cfg.admin_user));
  strlcpy(_cfg.admin_pass, input["admin_pass"] | _app.default_admin_pass, sizeof(_cfg.admin_pass));
  strlcpy(_cfg.guest_user, input["guest_user"] | _app.default_guest_user, sizeof(_cfg.guest_user));
  strlcpy(_cfg.guest_pass, input["guest_pass"] | _app.default_guest_pass, sizeof(_cfg.guest_pass));

  if (input["default_role"]) {
    if (!strcmp(input["default_role"], "admin"))
      _cfg.app_default_role = ADMIN;
    else if (!strcmp(input["default_role"], "guest"))
      _cfg.app_default_role = GUEST;
    else
      _cfg.app_default_role = NOBODY;
  }

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
}

void ProtocolController::handleSetWebServerConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  bool old_app_enable_ssl = _cfg.app_enable_ssl;

  _cfg.app_enable_mfd = input["app_enable_mfd"] | _app.enable_mfd;
  _cfg.app_enable_api = input["app_enable_api"] | _app.enable_http_api;
  _cfg.app_enable_ssl = input["app_enable_ssl"] | _cfg.app_enable_ssl;
  _cfg.server_cert = input["server_cert"] | "";
  _cfg.server_key = input["server_key"] | "";

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // restart the board.
  if (old_app_enable_ssl != _cfg.app_enable_ssl)
    ESP.restart();
}

void ProtocolController::handleSetMQTTConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  _cfg.app_enable_mqtt = input["app_enable_mqtt"];
  _cfg.app_enable_mqtt_protocol = input["app_enable_mqtt_protocol"];
  _cfg.app_enable_ha_integration = input["app_enable_ha_integration"];
  _cfg.app_use_hostname_as_mqtt_uuid = input["app_use_hostname_as_mqtt_uuid"];

  strlcpy(_cfg.mqtt_server, input["mqtt_server"] | "", sizeof(_cfg.mqtt_server));
  strlcpy(_cfg.mqtt_user, input["mqtt_user"] | "", sizeof(_cfg.mqtt_user));
  strlcpy(_cfg.mqtt_pass, input["mqtt_pass"] | "", sizeof(_cfg.mqtt_pass));
  _cfg.mqtt_cert = input["mqtt_cert"].as<String>();

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // init our mqtt
  if (_cfg.app_enable_mqtt)
    _app.mqtt.setup();
  else
    _app.mqtt.disconnect();
}

void ProtocolController::handleSetMiscellaneousConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  _cfg.app_enable_serial = input["app_enable_serial"] | _app.enable_serial_api;
  _cfg.app_enable_ota = input["app_enable_ota"] | _app.enable_arduino_ota;

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // init our ota.
  if (_cfg.app_enable_ota)
    _app.ota.setup();
  else
    _app.ota.end();
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

void ProtocolController::handleLogin(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!input["user"].is<String>())
    return generateErrorJSON(output, "'user' is a required parameter");

  if (!input["pass"].is<String>())
    return generateErrorJSON(output, "'pass' is a required parameter");

  // init
  char myuser[YB_USERNAME_LENGTH];
  char mypass[YB_PASSWORD_LENGTH];
  strlcpy(myuser, input["user"] | "", sizeof(myuser));
  strlcpy(mypass, input["pass"] | "", sizeof(mypass));

  // check their credentials
  bool is_authenticated = false;
  UserRole role = _cfg.app_default_role;

  if (!strcmp(_cfg.admin_user, myuser) && !strcmp(_cfg.admin_pass, mypass)) {
    is_authenticated = true;
    role = ADMIN;
    output["role"] = "admin";
  }

  if (!strcmp(_cfg.guest_user, myuser) && !strcmp(_cfg.guest_pass, mypass)) {
    is_authenticated = true;
    role = GUEST;
    output["role"] = "guest";
  }

  // okay, are we in?
  if (is_authenticated) {
    // check to see if there's room for us.
    if (context.mode == YBP_MODE_WEBSOCKET) {
      if (!_app.auth.logClientIn(context.clientId, role))
        return generateErrorJSON(output, "Too many connections.");
    } else if (context.mode == YBP_MODE_SERIAL) {
      _app.auth.logSerialClientIn(role);
    }

    output["msg"] = "login";
    output["role"] = _app.auth.getRoleText(role);
    output["message"] = "Login successful.";

    return;
  }

  // gtfo.
  return generateErrorJSON(output, "Wrong username/password.");
}

void ProtocolController::handleLogout(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!_app.auth.isLoggedIn(input, context.mode, context.clientId))
    return generateErrorJSON(output, "You are not logged in.");

  // what type of client are you?
  if (context.mode == YBP_MODE_WEBSOCKET) {
    _app.auth.removeClientFromAuthList(context.clientId);
  } else if (context.mode == YBP_MODE_SERIAL) {
    _app.auth.logSerialClientOut();
  }
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

void ProtocolController::handleOTAStart(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (_app.ota.checkOTA())
    _app.ota.startOTA();
  else
    return generateErrorJSON(output, "Firmware already up to date.");
}

void ProtocolController::handleSetTheme(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!input["theme"].is<String>())
    return generateErrorJSON(output, "'theme' is a required parameter");

  String temp = input["theme"];

  if (temp != "light" && temp != "dark")
    return generateErrorJSON(output,
      "'theme' must either be 'light' or 'dark'");

  _cfg.app_theme = temp;

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

    _cfg.globalBrightness = brightness;

    // TODO: need to put this on a time delay
    // preferences.putFloat("brightness", globalBrightness);

    for (const auto& entry : _app.getControllers()) {
      entry.controller->updateBrightnessHook(brightness);
    }
    sendBrightnessUpdate();
  } else
    return generateErrorJSON(output, "'brightness' is a required parameter.");
}

void ProtocolController::generateConfigMessage(JsonVariant output)
{
  // extra info
  output["msg"] = "config";
  output["hostname"] = _cfg.local_hostname;
  output["use_ssl"] = _cfg.app_enable_ssl;
  output["enable_ota"] = _cfg.app_enable_ota;
  output["enable_mqtt"] = _cfg.app_enable_mqtt;
  output["default_role"] = _app.auth.getRoleText(_cfg.app_default_role);
  output["brightness"] = _cfg.globalBrightness;
  output["git_hash"] = GIT_HASH;
  output["build_time"] = BUILD_TIME;
  output["firmware_manifest_url"] = _app.ota.firmware_manifest_url;

  _cfg.generateBoardConfig(output);

  output["is_development"] = YB_IS_DEVELOPMENT;

  // some debug info
  output["last_restart_reason"] = _app.debug.getResetReason();
  if (_app.debug.hasCoredump())
    output["has_coredump"] = _app.debug.hasCoredump();
  output["boot_log"] = startupLogger.c_str();

  // do we want to flag it for config?
  if (_cfg.is_first_boot)
    output["first_boot"] = true;
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
  output["theme"] = _cfg.app_theme;

  sendToAll(output, NOBODY);
}

void ProtocolController::sendBrightnessUpdate()
{
  JsonDocument output;
  output["msg"] = "set_brightness";
  output["brightness"] = _cfg.globalBrightness;

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

void ProtocolController::sendOTAProgressUpdate(float progress)
{
  JsonDocument output;
  output["msg"] = "ota_progress";
  output["progress"] = round2(progress);

  sendToAll(output, GUEST);
}

void ProtocolController::sendOTAProgressFinished()
{
  JsonDocument output;
  output["msg"] = "ota_finished";

  sendToAll(output, GUEST);
}

void ProtocolController::sendDebug(const char* message)
{
  JsonDocument output;
  output["debug"] = message;

  sendToAll(output, NOBODY);
}

void ProtocolController::sendToAll(JsonVariantConst output, UserRole auth_level)
{
  // dynamically allocate our buffer
  size_t jsonSize = measureJson(output);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(output, jsonBuffer, jsonSize + 1);
    sendToAll(jsonBuffer, auth_level);
    free(jsonBuffer);
  } else {
    // dont call YBP b/c loops...
    Serial.println("Error allocating in ProtocolController::sendToAll");
  }
}

void ProtocolController::sendToAll(const char* jsonString, UserRole auth_level)
{
  _app.http.sendToAllWebsockets(jsonString, auth_level);

  if (_cfg.app_enable_serial && _cfg.serial_role >= auth_level)
    Serial.println(jsonString);
}
