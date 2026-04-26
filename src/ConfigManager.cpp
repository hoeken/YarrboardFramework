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
                                                  _app(app)
{
  strlcpy(defaults.board_name, "Yarrboard", sizeof(defaults.board_name));
  defaults.is_first_boot = true;
  _config = defaults;
}

bool ConfigManager::setup()
{
  if (!BaseController::setup())
    return false;

  strlcpy(_config.board_name, defaults.board_name, sizeof(_config.board_name));

  _schema_version = 2;

  preferences.end(); // begin() returns false if already open.
  if (preferences.begin("yarrboard", false)) {
    YBP.printf("There are: %u entries available in the 'yarrboard' prefs table.\n", preferences.freeEntries());
  } else {
    YBP.println("Opening Preferences failed.");
    return false;
  }

  _config.is_first_boot = true;

  char error[YB_ERROR_LENGTH] = "";

  if (!loadConfigFromFile(YB_BOARD_CONFIG_PATH, error, sizeof(error))) {
    YBP.printf("CONFIG ERROR: %s\n", error);
    return false;
  }

  return true;
}

bool ConfigManager::sanitizeConfig(JsonVariant config, char* error, size_t len)
{
  bool result = true;

  for (const auto& entry : _app.getControllers()) {
    const char* name = entry.controller->getName();
    if (!entry.controller->sanitizeConfigHook(config[name], error, len)) {
      YBP.println(error);
      result = false;
    }
  }

  return result;
}

bool ConfigManager::saveConfig(char* error, size_t len)
{
  JsonDocument config;

  generateConfig(config, ADMIN, FIRMWARE);

  // sanitize / validate before we save to catch any weirdness
  if (!sanitizeConfig(config, error, len))
    return false;

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

bool ConfigManager::sanitizeConfigHook(JsonVariant config, char* error, size_t len)
{
  if (config["name"] && strlen(config["name"] | "") > YB_BOARD_NAME_LENGTH - 1) {
    snprintf(error, len, "name too long (max %d chars), will be truncated", YB_BOARD_NAME_LENGTH - 1);
    return false;
  }

  return true;
}

void ConfigManager::loadConfigHook(JsonVariantConst config)
{
  const char* v = config["name"] | defaults.board_name;
  strlcpy(_config.board_name, v, sizeof(_config.board_name));

  _config.is_first_boot = config["is_first_boot"] | defaults.is_first_boot;
}

bool ConfigManager::handleSetConfigSuccessCallback(JsonVariantConst config, JsonVariant output, ProtocolContext context, char* error, size_t len)
{
  // do we have a buzzer controller?
  BuzzerController* buzzer = static_cast<BuzzerController*>(_app.getController("buzzer"));
  if (buzzer) {
    // we need a mutable format for the validation
    JsonDocument doc;
    doc.set(config);

    // handle the config
    if (!buzzer->sanitizeConfigHook(doc, error, len))
      return false;
    buzzer->loadConfigHook(doc);
  }

  // other misc configs.
  _app.protocol.setSerialEnabled(config["serial_enabled"] | _app.protocol.defaults.serial_enabled);
  _app.ota.setEnabled(config["arduino_ota_enabled"] | _app.ota.defaults.arduino_ota_enabled);

  return true;
}

// -------------------------------------------------------------------------
// Generation
// -------------------------------------------------------------------------

void ConfigManager::generateConfig(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  // JsonVariant config = output["config"].to<JsonObject>();
  output["schema_version"] = _schema_version;

  for (const auto& entry : _app.getControllers()) {
    const char* name = entry.controller->getName();
    entry.controller->generateConfigHook(output[name].to<JsonObject>(), role, purpose);

    if (output[name].as<JsonObject>().size() == 0)
      output.remove(name);
  }
}

void ConfigManager::generateCapabilities(JsonVariant output)
{
  JsonVariant capabilities = output["capabilities"].to<JsonObject>();
  for (const auto& entry : _app.getControllers()) {
    const char* name = entry.controller->getName();
    entry.controller->generateCapabilitiesHook(capabilities[name].to<JsonObject>());
  }
}

void ConfigManager::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  output["name"] = _config.board_name;

  if (purpose == ConfigPurpose::FIRMWARE)
    output["is_first_boot"] = _config.is_first_boot;
  else if (purpose == ConfigPurpose::UI_CONFIG)
    output["brightness"] = getGlobalBrightness();
  else if (purpose == ConfigPurpose::SHAREABLE)
    output["is_first_boot"] = true;
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

    // validate prunes invalid entries, so it's safe to load even on error.
    // we don't want a single bad config option to nuke the whole config loading.
    if (!entry.controller->sanitizeConfigHook(config[name], error, len)) {
      YBP.println(error);
      result = false;
    }

    entry.controller->loadConfigHook(config[name]);
  }

  return result;
}

bool ConfigManager::loadV1Config(JsonVariant root, char* error, size_t len)
{
  bool result = true;

  // Map v1 board/app/network sections to v2 top-level sections
  root["config"]["name"] = root["board"]["name"];
  root["config"]["is_first_boot"] = root["app"]["is_first_boot"];

  // root["network"] keys are identical between v1 and v2 — no remapping needed

  root["ntp"]["gmt_offset_sec"] = root["app"]["gmt_offset_sec"];
  root["ntp"]["daylight_offset_sec"] = root["app"]["daylight_offset_sec"];
  root["ntp"]["ntp_server1"] = root["app"]["ntp_server1"];
  root["ntp"]["ntp_server2"] = root["app"]["ntp_server2"];

  root["http"]["app_enable_api"] = root["app"]["app_enable_api"];
  root["http"]["app_enable_ssl"] = root["app"]["app_enable_ssl"];
  root["http"]["server_cert"] = root["app"]["server_cert"];
  root["http"]["server_key"] = root["app"]["server_key"];

  root["protocol"]["app_enable_serial"] = root["app"]["app_enable_serial"];

  root["auth"]["default_role"] = root["app"]["default_role"];
  root["auth"]["admin_user"] = root["app"]["admin_user"];
  root["auth"]["admin_pass"] = root["app"]["admin_pass"];
  root["auth"]["guest_user"] = root["app"]["guest_user"];
  root["auth"]["guest_pass"] = root["app"]["guest_pass"];

  root["ota"]["app_enable_ota"] = root["app"]["app_enable_ota"];

  root["mqtt"]["app_enable_mqtt"] = root["app"]["app_enable_mqtt"];
  root["mqtt"]["app_enable_mqtt_protocol"] = root["app"]["app_enable_mqtt_protocol"];
  root["mqtt"]["app_enable_ha_integration"] = root["app"]["app_enable_ha_integration"];
  root["mqtt"]["app_use_hostname_as_mqtt_uuid"] = root["app"]["app_use_hostname_as_mqtt_uuid"];
  root["mqtt"]["mqtt_server"] = root["app"]["mqtt_server"];
  root["mqtt"]["mqtt_user"] = root["app"]["mqtt_user"];
  root["mqtt"]["mqtt_pass"] = root["app"]["mqtt_pass"];
  root["mqtt"]["mqtt_cert"] = root["app"]["mqtt_cert"];

  root["navico"]["app_enable_mfd"] = root["app"]["app_enable_mfd"];
  root["buzzer"]["startup_melody"] = root["app"]["startup_melody"];

  if (root["board"]["brineomatic"].is<JsonObject>()) {
    JsonDocument tempDoc;
    tempDoc.set(root["board"]["brineomatic"]);
    root["brineomatic"].set(tempDoc.as<JsonObject>());
    root["board"].remove("brineomatic");
  }

  // re-wrap channel arrays in new style/
  if (root["board"].is<JsonObject>()) {
    String arrayKeys[16];
    int arrayKeyCount = 0;
    for (JsonPair kv : root["board"].as<JsonObject>()) {
      if (kv.value().is<JsonArray>() && arrayKeyCount < 16) {
        arrayKeys[arrayKeyCount++] = kv.key().c_str();
      }
    }
    for (int i = 0; i < arrayKeyCount; i++) {
      const char* key = arrayKeys[i].c_str();
      JsonDocument tempDoc;
      tempDoc.set(root["board"][key]);
      root[key]["channels"].set(tempDoc.as<JsonArray>());
    }
  }

  root.remove("melodies");
  root.remove("board");
  root.remove("app");

  // for debugging only.
  // size_t jsonSize = measureJson(root);
  // char* configStr = (char*)malloc(jsonSize + 1);
  // if (configStr != NULL) {
  //   configStr[jsonSize] = '\0';
  //   serializeJson(root, configStr, jsonSize + 1);
  //   YBP.println(configStr);
  //   free(configStr);
  // }
  // delay(5000);

  // call each controllers sanitize and load.
  for (const auto& entry : _app.getControllers()) {
    const char* name = entry.controller->getName();

    if (!entry.controller->sanitizeConfigHook(root[name], error, len)) {
      YBP.println(error);
      result = false;
    }
    entry.controller->loadConfigHook(root[name]);
  }

  return result;
}