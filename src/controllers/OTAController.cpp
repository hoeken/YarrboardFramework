/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "OTAController.h"
#include "ConfigManager.h"
#include "ProtocolController.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

OTAController* OTAController::_instance = nullptr;

OTAController::OTAController(YarrboardApp& app) : BaseController(app, "ota"),
                                                  FOTA(YB_HARDWARE_VERSION, YB_FIRMWARE_VERSION, YB_VALIDATE_FIRMWARE_SIGNATURE)
{
  MyPubKey = new CryptoMemAsset("RSA Key", public_key, strlen(public_key) + 1);
}

bool OTAController::setup()
{
  if (_cfg.app_enable_ota) {
    ArduinoOTA.setHostname(_cfg.local_hostname);
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setPassword(_cfg.admin_pass);
    ArduinoOTA.begin();
  }

  FOTA.setManifestURL("https://raw.githubusercontent.com/hoeken/yarrboard-firmware/main/firmware.json");
  FOTA.setPubKey(MyPubKey);
  FOTA.useBundledCerts();

  FOTA.setUpdateBeginFailCb(_updateBeginFailCallbackStatic);
  FOTA.setProgressCb(_progressCallbackStatic);
  FOTA.setUpdateEndCb(_updateEndCallbackStatic);
  FOTA.setUpdateCheckFailCb(_updateCheckFailCallbackStatic);

  FOTA.printConfig();

  return true;
}

void OTAController::loop()
{
  if (doOTAUpdate) {
    FOTA.handle();
    doOTAUpdate = false;
  }

  if (_cfg.app_enable_ota) {
    ArduinoOTA.handle();
  }
}

void OTAController::end()
{
  if (!_cfg.app_enable_ota)
    ArduinoOTA.end();
}

bool OTAController::checkOTA()
{
  return FOTA.execHTTPcheck();
}

void OTAController::startOTA()
{
  doOTAUpdate = true;
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
    _app.protocol.sendOTAProgressUpdate(percent);
    ota_last_message = millis();
  }
}

void OTAController::_updateEndCallback(int partition)
{
  YBP.printf("[ota] Update ended with %s partition\n", partition == U_SPIFFS ? "spiffs" : "firmware");
  _app.protocol.sendOTAProgressFinished();
}

void OTAController::_updateCheckFailCallback(int partition, int error_code)
{
  YBP.printf("[ota] Update could not validate %s partition (error %d)\n", partition == U_SPIFFS ? "spiffs" : "firmware", error_code);
  // error codes:
  //  -1 : partition not found
  //  -2 : validation (signature check) failed
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