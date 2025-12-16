/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "controllers/NavicoController.h"
#include "ConfigManager.h"
#include "YarrboardDebug.h"

NavicoController::NavicoController(YarrboardApp& app) : BaseController(app, "navico"),
                                                        MULTICAST_GROUP_IP(239, 2, 1, 1)
{
}

// This code borrowed from the SignalK project:
// https://github.com/SignalK/signalk-server/blob/master/src/interfaces/mfd_webapp.ts
void NavicoController::loop()
{
  String url = "http://192.168.2.150:80";
  String protocol;
  int port;

  if (millis() - lastNavicoPublishMillis > 10000) {

    if (!WiFi.isConnected())
      return;

    char urlBuf[48];
    IPAddress ip = WiFi.localIP();
    snprintf(urlBuf, sizeof(urlBuf), _cfg.app_enable_ssl ? "https://%u.%u.%u.%u:443" : "http://%u.%u.%u.%u:80", ip[0], ip[1], ip[2], ip[3]);
    url = urlBuf; // assign once

    // generate our config JSON
    JsonDocument doc;

    doc["Version"] = "1";
    doc["Source"] = _cfg.board_name;
    doc["IP"] = WiFi.localIP();
    doc["FeatureName"] = String(_cfg.board_name) + " Webapp";

    JsonObject Text_0 = doc["Text"].add<JsonObject>();
    Text_0["Language"] = "en";
    Text_0["Name"] = _cfg.board_name;
    Text_0["Description"] = String(_cfg.board_name) + " Webapp";
    doc["Icon"] = url + "/logo.png";
    doc["URL"] = url + "/";
    doc["OnlyShowOnClientIP"] = "true";

    JsonObject BrowserPanel = doc["BrowserPanel"].to<JsonObject>();
    BrowserPanel["Enable"] = true;
    BrowserPanel["ProgressBarEnable"] = true;

    JsonObject BrowserPanel_MenuText_0 = BrowserPanel["MenuText"].add<JsonObject>();
    BrowserPanel_MenuText_0["Language"] = "en";
    BrowserPanel_MenuText_0["Name"] = "Home";

    // make our dynamic buffer for the output
    size_t jsonSize = measureJson(doc);
    char* jsonBuffer = (char*)malloc(jsonSize + 1);
    if (!jsonBuffer) {
      YBP.println("Navico malloc failed!");
      return;
    }

    jsonBuffer[jsonSize] = '\0'; // null terminate

    // did we get anything?
    serializeJson(doc, jsonBuffer, jsonSize + 1);

    if (Udp.beginPacket(MULTICAST_GROUP_IP, PUBLISH_PORT)) {
      Udp.write((const uint8_t*)jsonBuffer, jsonSize);
      Udp.endPacket();
    } else {
      YBP.println("UDP beginPacket failed");
    }

    free(jsonBuffer);

    lastNavicoPublishMillis = millis();
  }
}