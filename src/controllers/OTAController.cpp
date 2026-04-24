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

#include "OTAController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

OTAController* OTAController::_instance = nullptr;

OTAController::OTAController(YarrboardApp& app) : BaseController(app, "ota")
{
  defaults.arduino_ota_enabled = false;
  _config = defaults;
}

OTAController::~OTAController()
{
  delete FOTA;
  delete MyPubKey;
}

bool OTAController::setup()
{
  _instance = this; // Capture the instance for callbacks

  _app.protocol.registerCommand(ADMIN, "ota_start", this, &OTAController::handleOTAStart);

  if (_config.arduino_ota_enabled) {
    ArduinoOTA.setHostname(_app.network.getLocalHostname());
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setPassword(_app.auth.getAdminPass());
    ArduinoOTA.begin();
  }

  if (!strlen(firmware_manifest_url)) {
    YBP.println("⚠️ No ota.firmware_manifest_url set, disabling OTA firmware downloading.");
    return false;
  }

  FOTA = new esp32FOTA(_app.hardware_version, _app.firmware_version, validate_firmware);
  FOTA->setManifestURL(firmware_manifest_url);

  if (strlen(public_key)) {
    MyPubKey = new CryptoMemAsset("RSA Key", public_key, strlen(public_key) + 1);
    FOTA->setPubKey(MyPubKey);
  } else
    YBP.println("⚠️ No ota.public_key set, will not check firmware signature.");

  FOTA->useBundledCerts();
  FOTA->setUpdateBeginFailCb(_updateBeginFailCallbackStatic);
  FOTA->setProgressCb(_progressCallbackStatic);
  FOTA->setUpdateEndCb(_updateEndCallbackStatic);
  FOTA->setUpdateCheckFailCb(_updateCheckFailCallbackStatic);

  return true;
}

void OTAController::loop()
{
  if (doOTAUpdate && FOTA) {
    FOTA->handle();
    doOTAUpdate = false;
  }

  if (_config.arduino_ota_enabled) {
    ArduinoOTA.handle();
  }
}

void OTAController::handleOTAStart(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (_app.ota.checkOTA())
    _app.ota.startOTA();
  else
    return _app.protocol.generateErrorJSON(output, "Firmware already up to date.");
}

void OTAController::end()
{
  // called after config disables OTA, so _config.arduino_ota_enabled is already false here
  if (!_config.arduino_ota_enabled)
    ArduinoOTA.end();
}

bool OTAController::checkOTA()
{
  if (strlen(firmware_manifest_url))
    return FOTA->execHTTPcheck();
  return false;
}

void OTAController::startOTA()
{
  YBP.printf("Starting OTA.");
  doOTAUpdate = true;
}

bool OTAController::sanitizeConfigHook(JsonVariant config, char* error, size_t len)
{
  if (config["app_enable_ota"].is<bool>()) {
    config["arduino_ota_enabled"] = config["app_enable_ota"].as<bool>();
    config.remove("app_enable_ota");
  }

  return true;
}

void OTAController::loadConfigHook(JsonVariantConst config)
{
  _config.arduino_ota_enabled = config["arduino_ota_enabled"] | defaults.arduino_ota_enabled;
}

void OTAController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  output["arduino_ota_enabled"] = _config.arduino_ota_enabled;
}

void OTAController::_updateBeginFailCallback(int partition)
{
  YBP.printf("[ota] Update could not begin with %s partition\n", partition == U_SPIFFS ? "spiffs" : "firmware");
}

void OTAController::_progressCallback(size_t progress, size_t size)
{
  if (progress == size || progress == 0)
    YBP.println();
  YBP.print(".");

  // let the clients know every second and at the end
  if (millis() - ota_last_message > 1000 || progress == size) {
    float percent = (float)progress / (float)size * 100.0;
    sendOTAProgressUpdate(percent);
    ota_last_message = millis();
  }
}

void OTAController::_updateEndCallback(int partition)
{
  YBP.printf("[ota] Update ended with %s partition\n", partition == U_SPIFFS ? "spiffs" : "firmware");
  sendOTAProgressFinished();
}

void OTAController::_updateCheckFailCallback(int partition, int error_code)
{
  YBP.printf("[ota] Update could not validate %s partition (error %d)\n", partition == U_SPIFFS ? "spiffs" : "firmware", error_code);
}

void OTAController::_updateBeginFailCallbackStatic(int partition)
{
  if (_instance)
    _instance->_updateBeginFailCallback(partition);
}

void OTAController::_progressCallbackStatic(size_t progress, size_t size)
{
  if (_instance)
    _instance->_progressCallback(progress, size);
}

void OTAController::_updateEndCallbackStatic(int partition)
{
  if (_instance)
    _instance->_updateEndCallback(partition);
}

void OTAController::_updateCheckFailCallbackStatic(int partition, int error_code)
{
  if (_instance)
    _instance->_updateCheckFailCallback(partition, error_code);
}

void OTAController::sendOTAProgressUpdate(float progress)
{
  JsonDocument output;
  output["msg"] = "ota_progress";
  output["progress"] = round2(progress);

  _app.protocol.sendToAll(output, GUEST);
}

void OTAController::sendOTAProgressFinished()
{
  JsonDocument output;
  output["msg"] = "ota_finished";

  _app.protocol.sendToAll(output, GUEST);
}
