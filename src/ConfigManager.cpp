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
                                                  _is_first_boot(true)
{
}

bool ConfigManager::setup()
{
  // setup some defaults
  strlcpy(board_name, _app.board_name, sizeof(board_name));
  strlcpy(startup_melody, _app.default_melody, sizeof(startup_melody));

  app_update_interval = _app.update_interval;

  app_enable_mfd = _app.enable_mfd;

  // our temporary preferences too.
  preferences.end(); // begin() returns false if already open.
  if (preferences.begin("yarrboard", false)) {
    YBP.printf("There are: %u entries available in the 'yarrboard' prefs table.\n", preferences.freeEntries());
  } else {
    YBP.println("Opening Preferences failed.");
    return false;
  }

  // default to first time, prove it later
  _is_first_boot = true;

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
  output["uuid"] = _app.network.getUUID();
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

  // hook for our hardware capabilities
  JsonObject capabilities = output["capabilities"].to<JsonObject>();
  for (const auto& entry : _app.getControllers()) {
    entry.controller->generateCapabilitiesHook(capabilities);
  }

  // hook for each controller
  for (const auto& entry : _app.getControllers()) {
    entry.controller->generateConfigHook(output);
  }
}

void ConfigManager::generateAppConfig(JsonVariant output)
{
  // our identifying info
  output["is_first_boot"] = _is_first_boot;
  output["startup_melody"] = startup_melody;
  output["app_update_interval"] = app_update_interval;
  output["app_enable_mfd"] = app_enable_mfd;
  _app.ota.generateOTAConfig(output);
  _app.auth.generateAuthConfig(output);
  _app.http.generateHTTPConfig(output);
  _app.protocol.generateSerialConfig(output);
  _app.mqtt.generateMQTTConfig(output);
}

void ConfigManager::generateNetworkConfig(JsonVariant output)
{
  _app.network.generateNetworkConfig(output);
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
  return _app.network.loadNetworkConfig(config, error, len);
}

bool ConfigManager::loadAppConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  const char* v;

  // determines if we do our improv loop or not.
  _is_first_boot = config["is_first_boot"] | false;

  // startup_melody
  v = config["startup_melody"] | _app.default_melody;
  strlcpy(startup_melody, v, sizeof(startup_melody));

  if (config["app_update_interval"]) {
    app_update_interval = config["app_update_interval"] | _app.update_interval;
    app_update_interval = max(100u, app_update_interval);
    app_update_interval = min(10000u, app_update_interval);
  }

  app_enable_mfd = config["app_enable_mfd"] | _app.enable_mfd;
  _app.ota.loadOTAConfig(config);

  _app.auth.loadAuthConfig(config);
  _app.http.loadHTTPConfig(config);
  _app.protocol.loadSerialConfig(config);
  _app.mqtt.loadMQTTConfig(config, error, len);

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