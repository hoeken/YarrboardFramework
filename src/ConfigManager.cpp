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

#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

ConfigManager::ConfigManager(YarrboardApp& app) : BaseController(app, "config"),
                                                  _app(app),
                                                  is_first_boot(true)
{
}

bool ConfigManager::setup()
{
  // setup some defaults
  strlcpy(board_name, _app.board_name, sizeof(board_name));
  strlcpy(local_hostname, _app.default_hostname, sizeof(local_hostname));
  strlcpy(admin_user, _app.default_admin_user, sizeof(admin_user));
  strlcpy(admin_pass, _app.default_admin_pass, sizeof(admin_pass));
  strlcpy(guest_user, _app.default_guest_user, sizeof(guest_user));
  strlcpy(guest_pass, _app.default_guest_pass, sizeof(guest_pass));
  strlcpy(startup_melody, _app.default_melody, sizeof(startup_melody));

  app_update_interval = _app.update_interval;

  app_enable_mfd = _app.enable_mfd;
  app_enable_api = _app.enable_http_api;
  app_enable_serial = _app.enable_serial_api;
  app_enable_ota = _app.enable_arduino_ota;
  app_enable_ssl = _app.enable_ssl;
  app_enable_mqtt = _app.enable_mqtt;
  app_enable_mqtt_protocol = _app.enable_mqtt_protocol;
  app_enable_ha_integration = _app.enable_ha_integration;
  app_use_hostname_as_mqtt_uuid = _app.use_hostname_as_mqtt_uuid;

  app_default_role = _app.default_role;
  serial_role = _app.default_role;
  api_role = _app.default_role;

  // our temporary preferences too.
  preferences.end(); // begin() returns false if already open.
  if (preferences.begin("yarrboard", false)) {
    YBP.printf("There are: %u entries available in the 'yarrboard' prefs table.\n", preferences.freeEntries());
  } else {
    YBP.println("Opening Preferences failed.");
    return false;
  }

  // default to first time, prove it later
  is_first_boot = true;

  // initialize error string
  char error[YB_ERROR_LENGTH] = "";

  // load our config from the json file.
  if (!loadConfigFromFile(YB_BOARD_CONFIG_PATH, error, sizeof(error))) {
    YBP.printf("CONFIG ERROR: %s\n", error);
    return false;
  }

  return true;
}

bool ConfigManager::saveConfig(char* error, size_t len)
{
  // our doc to store.
  JsonDocument config;

  // generate a full new document each time
  generateFullConfig(config);

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(config);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // now serialize it to the buffer
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(config, jsonBuffer, jsonSize + 1);
  } else {
    snprintf(error, len, "saveConfig() failed to create buffer of size %d", jsonSize);
    return false;
  }

  // write our config to local storage
  File fp = LittleFS.open(YB_BOARD_CONFIG_PATH, "w");
  if (!fp) {
    snprintf(error, len, "Failed to open %s for writing", YB_BOARD_CONFIG_PATH);
    free(jsonBuffer);
    return false;
  }

  // check write result
  size_t bytesWritten = fp.print((char*)jsonBuffer);
  if (bytesWritten == 0) {
    fp.close();
    strncpy(error, "Failed to write JSON data to file", len);
    free(jsonBuffer);
    return false;
  }

  // flush data (no return value, but still good to call)
  fp.flush();
  fp.close();

  // confirm file exists and has non-zero length
  if (!LittleFS.exists(YB_BOARD_CONFIG_PATH)) {
    strncpy(error, "File not found after write", len);
    free(jsonBuffer);
    return false;
  }

  // check for size and opening
  File verify = LittleFS.open(YB_BOARD_CONFIG_PATH, "r");
  if (!verify || verify.size() == 0) {
    verify.close();
    strncpy(error, "Wrote file but it appears empty or unreadable", len);
    free(jsonBuffer);
    return false;
  }
  verify.close();

  // free up our memory
  free(jsonBuffer);

  return true;
}

void ConfigManager::generateFullConfig(JsonVariant output)
{
  // our board specific configuration
  JsonObject board = output["board"].to<JsonObject>();
  generateBoardConfig(board);

  // yarrboard application specific configuration
  JsonObject app = output["app"].to<JsonObject>();
  generateAppConfig(app);
  app.remove("msg");

  // network connection specific configuration
  JsonObject network = output["network"].to<JsonObject>();
  generateNetworkConfig(network);
  network.remove("msg");
}

void ConfigManager::generateBoardConfig(JsonVariant output)
{
  // our identifying info
  output["name"] = board_name;
  output["uuid"] = uuid;
  output["firmware_version"] = _app.firmware_version;
  output["hardware_version"] = _app.hardware_version;
  output["hardware_url"] = _app.hardware_url;
  output["project_name"] = _app.project_name;
  output["project_url"] = _app.project_url;
  output["git_url"] = _app.git_url;
  output["esp_idf_version"] = esp_get_idf_version();
  output["arduino_version"] = ESP_ARDUINO_VERSION_STR;
  output["psychic_http_version"] = PSYCHIC_VERSION_STR;
  output["yarrboard_framework_version"] = YARRBOARD_VERSION_STR;
#ifdef GIT_HASH
  output["git_hash"] = GIT_HASH;
#endif
#ifdef BUILD_TIME
  output["build_time"] = BUILD_TIME;
#endif

  for (const auto& entry : _app.getControllers()) {
    entry.controller->generateConfigHook(output);
  }
}

void ConfigManager::generateAppConfig(JsonVariant output)
{
  // our identifying info
  output["is_first_boot"] = is_first_boot;
  output["startup_melody"] = startup_melody;
  output["default_role"] = _app.auth.getRoleText(app_default_role);
  output["admin_user"] = admin_user;
  output["admin_pass"] = admin_pass;
  output["guest_user"] = guest_user;
  output["guest_pass"] = guest_pass;
  output["app_update_interval"] = app_update_interval;
  output["app_enable_mfd"] = app_enable_mfd;
  output["app_enable_api"] = app_enable_api;
  output["app_enable_serial"] = app_enable_serial;
  output["app_enable_ota"] = app_enable_ota;
  output["app_enable_ssl"] = app_enable_ssl;
  output["app_enable_mqtt"] = app_enable_mqtt;
  output["app_enable_mqtt_protocol"] = app_enable_mqtt_protocol;
  output["app_enable_ha_integration"] = app_enable_ha_integration;
  output["app_use_hostname_as_mqtt_uuid"] = app_use_hostname_as_mqtt_uuid;
  output["mqtt_server"] = mqtt_server;
  output["mqtt_user"] = mqtt_user;
  output["mqtt_pass"] = mqtt_pass;
  output["mqtt_cert"] = mqtt_cert;
  output["server_cert"] = server_cert;
  output["server_key"] = server_key;
}

void ConfigManager::generateNetworkConfig(JsonVariant output)
{
  // our identifying info
  output["wifi_mode"] = wifi_mode;
  output["wifi_ssid"] = wifi_ssid;
  output["wifi_pass"] = wifi_pass;
  output["local_hostname"] = local_hostname;
}

bool ConfigManager::loadConfigFromFile(const char* file, char* error, size_t len)
{
  // sanity check on LittleFS
  if (!LittleFS.begin()) {
    snprintf(error, len, "LittleFS mount failed");
    return false;
  }

  // open file
  File configFile = LittleFS.open(file, "r");
  if (!configFile || !configFile.available()) {
    snprintf(error, len, "Could not open file: %s", file);
    return false;
  }

  // get size and check reasonableness
  size_t size = configFile.size();
  if (size == 0) {
    snprintf(error, len, "File %s is empty", file);
    configFile.close();
    return false;
  }
  if (size > 10000) { // arbitrary limit to prevent large loads
    snprintf(error, len, "File %s too large (%u bytes)", file, (unsigned int)size);
    configFile.close();
    return false;
  }

  // read into buffer
  char* buf = (char*)malloc(size + 1);
  if (!buf) {
    snprintf(error, len, "Memory allocation failed for %u bytes", (unsigned int)size);
    configFile.close();
    free(buf);
    return false;
  }

  size_t bytesRead = configFile.readBytes(buf, size);
  configFile.close();
  buf[bytesRead] = '\0';

  if (bytesRead != size) {
    snprintf(error, len, "Read size mismatch: expected %u, got %u", (unsigned int)size, (unsigned int)bytesRead);
    free(buf);
    return false;
  }

  // parse JSON
  JsonDocument doc; // adjust to match your configuration complexity
  DeserializationError err = deserializeJson(doc, buf);

  // no leaks
  free(buf);

  if (err) {
    snprintf(error, len, "JSON parse error: %s", err.c_str());
    return false;
  }

  // sanity check: ensure root object
  if (!doc.is<JsonObject>()) {
    snprintf(error, len, "Root element is not a JSON object");
    return false;
  }

  return loadConfigFromJSON(doc, error, len);
}

bool ConfigManager::loadConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  bool result = true;

  if (config["network"]) {
    if (!loadNetworkConfigFromJSON(config["network"], error, len)) {
      YBP.print(error);
      result = false;
    }
  } else
    YBP.println("Missing 'network' config");

  if (config["app"]) {
    if (!loadAppConfigFromJSON(config["app"], error, len)) {
      YBP.print(error);
      result = false;
    }
  } else
    YBP.println("Missing 'app' config");

  if (config["board"]) {
    if (!loadBoardConfigFromJSON(config["board"], error, len)) {
      YBP.print(error);
      result = false;
    }
  } else
    YBP.println("Missing 'board' config");

  return result;
}

bool ConfigManager::loadNetworkConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  const char* v;

  // local_hostname
  v = config["local_hostname"] | _app.default_hostname;
  strlcpy(local_hostname, v, sizeof(local_hostname));

  // wifi_ssid
  v = config["wifi_ssid"] | YB_DEFAULT_AP_SSID;
  strlcpy(wifi_ssid, v, sizeof(wifi_ssid));

  // wifi_pass
  v = config["wifi_pass"] | YB_DEFAULT_AP_PASS;
  strlcpy(wifi_pass, v, sizeof(wifi_pass));

  // wifi_mode
  v = config["wifi_mode"] | YB_DEFAULT_AP_MODE;
  strlcpy(wifi_mode, v, sizeof(wifi_mode));

  return true;
}

bool ConfigManager::loadAppConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  const char* v;

  // determines if we do our improv loop or not.
  is_first_boot = config["is_first_boot"] | false;

  // startup_melody
  v = config["startup_melody"] | _app.default_melody;
  strlcpy(startup_melody, v, sizeof(startup_melody));

  // admin_user
  v = config["admin_user"] | _app.default_admin_user;
  strlcpy(admin_user, v, sizeof(admin_user));

  // admin_pass
  v = config["admin_pass"] | _app.default_admin_pass;
  strlcpy(admin_pass, v, sizeof(admin_pass));

  // guest_user
  v = config["guest_user"] | _app.default_guest_user;
  strlcpy(guest_user, v, sizeof(guest_user));

  // guest_pass
  v = config["guest_pass"] | _app.default_guest_pass;
  strlcpy(guest_pass, v, sizeof(guest_pass));

  // MQTT fields
  v = config["mqtt_server"] | "";
  strlcpy(mqtt_server, v, sizeof(mqtt_server));

  v = config["mqtt_user"] | "";
  strlcpy(mqtt_user, v, sizeof(mqtt_user));

  v = config["mqtt_pass"] | "";
  strlcpy(mqtt_pass, v, sizeof(mqtt_pass));
  mqtt_cert = config["mqtt_cert"] | "";

  if (config["app_update_interval"]) {
    app_update_interval = config["app_update_interval"] | _app.update_interval;
    app_update_interval = max(100u, app_update_interval);
    app_update_interval = min(10000u, app_update_interval);
  }

  app_default_role = _app.default_role;
  if (config["default_role"]) {
    v = config["default_role"];
    if (!strcmp(v, "nobody"))
      app_default_role = NOBODY;
    else if (!strcmp(v, "admin"))
      app_default_role = ADMIN;
    else if (!strcmp(v, "guest"))
      app_default_role = GUEST;
  }
  serial_role = app_default_role;
  api_role = app_default_role;

  app_enable_mfd = config["app_enable_mfd"] | _app.enable_mfd;
  app_enable_api = config["app_enable_api"] | _app.enable_http_api;
  app_enable_serial = config["app_enable_serial"] | _app.enable_serial_api;
  app_enable_ota = config["app_enable_ota"] | _app.enable_arduino_ota;
  app_enable_ssl = config["app_enable_ssl"] | _app.enable_ssl;
  app_enable_mqtt = config["app_enable_mqtt"] | _app.enable_mqtt;
  app_enable_mqtt_protocol = config["app_enable_mqtt_protocol"] | _app.enable_mqtt_protocol;
  app_enable_ha_integration = config["app_enable_ha_integration"] | _app.enable_ha_integration;
  app_use_hostname_as_mqtt_uuid = config["app_use_hostname_as_mqtt_uuid"] | _app.use_hostname_as_mqtt_uuid;

  server_cert = config["server_cert"] | "";
  server_key = config["server_key"] | "";

  return true;
}

bool ConfigManager::loadBoardConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  bool result = true;

  const char* v = config["name"] | _app.board_name;
  strlcpy(board_name, v, sizeof(board_name));

  for (const auto& entry : _app.getControllers()) {
    entry.controller->loadConfigHook(config, error, len);
  }

  return result;
}