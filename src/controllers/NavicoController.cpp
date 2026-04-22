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

#include "controllers/NavicoController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

NavicoController::NavicoController(YarrboardApp& app) : BaseController(app, "navico"),
                                                        _multicastGroupIp(239, 2, 1, 1)
{
  _app_enable_mfd = app.enable_mfd;
}

void NavicoController::loop()
{
  if (!_app_enable_mfd)
    return;

  if (millis() - _lastPublishMillis > 10000) {

    if (!WiFi.isConnected())
      return;

    IPAddress ip = WiFi.localIP();
    char urlBuf[48];
    snprintf(urlBuf, sizeof(urlBuf), _app.http.isSSLEnabled() ? "https://%u.%u.%u.%u:443" : "http://%u.%u.%u.%u:80", ip[0], ip[1], ip[2], ip[3]);

    char iconUrl[64];
    char webUrl[64];
    snprintf(iconUrl, sizeof(iconUrl), "%s/logo.png", urlBuf);
    snprintf(webUrl, sizeof(webUrl), "%s/", urlBuf);

    JsonDocument doc;

    doc["Version"] = "1";
    doc["Source"] = _cfg.getBoardName();
    doc["IP"] = WiFi.localIP();
    doc["FeatureName"] = _cfg.getBoardName();

    JsonObject Text_0 = doc["Text"].add<JsonObject>();
    Text_0["Language"] = "en";
    Text_0["Name"] = _cfg.getBoardName();
    Text_0["Description"] = _cfg.getBoardName();

    doc["Icon"] = iconUrl;
    doc["URL"] = webUrl;
    doc["OnlyShowOnClientIP"] = true;

    JsonObject BrowserPanel = doc["BrowserPanel"].to<JsonObject>();
    BrowserPanel["Enable"] = true;
    BrowserPanel["ProgressBarEnable"] = true;

    JsonObject BrowserPanel_MenuText_0 = BrowserPanel["MenuText"].add<JsonObject>();
    BrowserPanel_MenuText_0["Language"] = "en";
    BrowserPanel_MenuText_0["Name"] = "Home";

    if (Udp.beginPacket(_multicastGroupIp, kPublishPort)) {
      serializeJson(doc, Udp);
      Udp.endPacket();
    } else {
      YBP.println("UDP beginPacket failed");
    }

    _lastPublishMillis = millis();
  }
}

bool NavicoController::setup()
{
  Udp.begin(0);
  return true;
}

bool NavicoController::loadConfigHook(JsonVariant config, char* error, size_t len)
{
  _app_enable_mfd = config["app_enable_mfd"] | _app.enable_mfd;
  return true;
}

void NavicoController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  output["app_enable_mfd"] = _app_enable_mfd;
}
