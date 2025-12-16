/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_NETWORK_H
#define YARR_NETWORK_H

#include "YarrboardConfig.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ImprovWiFiBLE.h>
#include <ImprovWiFiLibrary.h>
#include <WiFi.h>

class YarrboardApp;
class ConfigManager;

class NetworkController
{
  public:
    NetworkController(YarrboardApp& app, ConfigManager& config);

    bool setup();
    void loop();

    void setupImprov();
    void loopImprov();

    void setupWifi();
    bool connectToWifi(const char* ssid, const char* pass);
    void startServices();

    IPAddress apIP;

  private:
    YarrboardApp& _app;
    ConfigManager& _config;
    ImprovWiFi improvSerial;
    ImprovWiFiBLE improvBLE;

    // for making a captive portal
    //  The default android DNS
    const byte DNS_PORT = 53;
    DNSServer dnsServer;

    // --- THE CALLBACK TRAP ---
    // Libraries expecting C-style function pointers cannot take normal member functions.
    // We use a static instance pointer and static methods to bridge the gap.
    static NetworkController* _instance;

    static void _onImprovErrorStatic(ImprovTypes::Error err);
    static void _onImprovConnectedStatic(const char* ssid, const char* password);
    static bool _onImprovCustomConnectWiFiStatic(const char* ssid, const char* password);

    // The actual member functions that handle the callbacks
    void _handleImprovError(ImprovTypes::Error err);
    void _handleImprovConnected(const char* ssid, const char* password);
};

#endif /* !YARR_NETWORK_H */