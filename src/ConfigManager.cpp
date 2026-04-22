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
  strlcpy(_board_name, _app.board_name, sizeof(_board_name));

  _schema_version = 2;

  preferences.end(); // begin() returns false if already open.
  if (preferences.begin("yarrboard", false)) {
    YBP.printf("There are: %u entries available in the 'yarrboard' prefs table.\n", preferences.freeEntries());
  } else {
    YBP.println("Opening Preferences failed.");
    return false;
  }

  _is_first_boot = true;

  char error[YB_ERROR_LENGTH] = "";

  if (!loadConfigFromFile(YB_BOARD_CONFIG_PATH, error, sizeof(error))) {
    YBP.printf("CONFIG ERROR: %s\n", error);
    return false;
  }

  return true;
}

bool ConfigManager::saveConfig(char* error, size_t len)
{
  JsonDocument config;

  generateConfig(config, ADMIN, FIRMWARE);

  size_t jsonSize = measureJson(config);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0';
    serializeJson(config, jsonBuffer, jsonSize + 1);
  } else {
    snprintf(error, len, "saveConfig() failed to create buffer of size %d", jsonSize);
    return false;
  }

  File fp = LittleFS.open(YB_BOARD_CONFIG_PATH, "w");
  if (!fp) {
    snprintf(error, len, "Failed to open %s for writing", YB_BOARD_CONFIG_PATH);
    free(jsonBuffer);
    return false;
  }

  size_t bytesWritten = fp.print((char*)jsonBuffer);
  if (bytesWritten == 0) {
    fp.close();
    strncpy(error, "Failed to write JSON data to file", len);
    free(jsonBuffer);
    return false;
  }

  fp.flush();
  fp.close();

  if (!LittleFS.exists(YB_BOARD_CONFIG_PATH)) {
    strncpy(error, "File not found after write", len);
    free(jsonBuffer);
    return false;
  }

  File verify = LittleFS.open(YB_BOARD_CONFIG_PATH, "r");
  if (!verify || verify.size() == 0) {
    verify.close();
    strncpy(error, "Wrote file but it appears empty or unreadable", len);
    free(jsonBuffer);
    return false;
  }
  verify.close();

  free(jsonBuffer);

  return true;
}

bool ConfigManager::loadConfigHook(JsonVariant config, char* error, size_t len)
{
  const char* v = config["name"] | _app.board_name;
  strlcpy(_board_name, v, sizeof(_board_name));

  _is_first_boot = config["is_first_boot"] | false;

  return true;
}

// -------------------------------------------------------------------------
// Generation
// -------------------------------------------------------------------------

void ConfigManager::generateConfig(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  JsonVariant config = output["config"].to<JsonObject>();
  config["schema_version"] = _schema_version;

  for (const auto& entry : _app.getControllers()) {
    const char* name = entry.controller->getName();
    entry.controller->generateConfigHook(config[name].to<JsonObject>(), role, purpose);

    if (config[name].as<JsonObject>().size() == 0)
      config.remove(name);
  }
}

void ConfigManager::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  output["name"] = _board_name;

  if (purpose == ConfigPurpose::FIRMWARE)
    output["is_first_boot"] = _is_first_boot;
  else if (purpose == ConfigPurpose::UI_VIEW)
    output["brightness"] = getGlobalBrightness();
}

// -------------------------------------------------------------------------
// Loading
// -------------------------------------------------------------------------

bool ConfigManager::loadConfigFromFile(const char* file, char* error, size_t len)
{
  if (!LittleFS.begin()) {
    snprintf(error, len, "LittleFS mount failed");
    return false;
  }

  File configFile = LittleFS.open(file, "r");
  if (!configFile || !configFile.available()) {
    snprintf(error, len, "Could not open file: %s", file);
    return false;
  }

  size_t size = configFile.size();
  if (size == 0) {
    snprintf(error, len, "File %s is empty", file);
    configFile.close();
    return false;
  }
  if (size > 10000) {
    snprintf(error, len, "File %s too large (%u bytes)", file, (unsigned int)size);
    configFile.close();
    return false;
  }

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

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, buf);

  free(buf);

  if (err) {
    snprintf(error, len, "JSON parse error: %s", err.c_str());
    return false;
  }

  if (!doc.is<JsonObject>()) {
    snprintf(error, len, "Root element is not a JSON object");
    return false;
  }

  return loadConfigFromJSON(doc, error, len);
}

bool ConfigManager::loadConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  int schemaVersion = config["schema_version"] | 1;

  if (schemaVersion == 1) {
    return loadV1Config(config, error, len);
  } else if (schemaVersion == 2) {
    return loadV2Config(config, error, len);
  } else {
    snprintf(error, len, "Unsupported schema_version: %d", schemaVersion);
    return false;
  }
}

bool ConfigManager::loadV2Config(JsonVariant config, char* error, size_t len)
{
  bool result = true;

  for (const auto& entry : _app.getControllers()) {
    const char* name = entry.controller->getName();
    if (!entry.controller->loadConfigHook(config[name], error, len)) {
      YBP.print(error);
      result = false;
    }
  }

  return result;
}

bool ConfigManager::loadV1Config(JsonVariant root, char* error, size_t len)
{
  bool result = true;

  for (const char* section : {"app", "network"}) {
    if (root[section].is<JsonObject>()) {
      for (JsonPair kv : root[section].as<JsonObject>()) {
        root["board"][kv.key()] = kv.value();
      }
    }
  }

  for (const auto& entry : _app.getControllers()) {
    const char* name = entry.controller->getName();
    JsonVariant sub;
    if (root["board"].is<JsonObject>() && !root["board"][name].isNull()) {
      sub = root["board"][name];
    } else {
      sub = root["board"];
    }
    if (!entry.controller->loadConfigHook(sub, error, len)) {
      YBP.print(error);
      result = false;
    }
  }

  return result;
}