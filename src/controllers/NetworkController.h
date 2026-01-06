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

#ifndef YARR_NETWORK_H
#define YARR_NETWORK_H

#include "YarrboardConfig.h"
#include "controllers/BaseController.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#ifdef IMPROV_WIFI_BLE_ENABLED
  #include <ImprovWiFiBLE.h>
#endif
#include <ImprovWiFiLibrary.h>
#include <WiFi.h>

class YarrboardApp;
class ConfigManager;

class NetworkController : public BaseController
{
  public:
    NetworkController(YarrboardApp& app);

    bool setup() override;
    void loop() override;

    void setupImprov();

    void setupWifi();
    bool connectToWifi(const char* ssid, const char* pass);
    void startServices();

    IPAddress apIP;
    bool improvDone = false;

  private:
    ImprovWiFi improvSerial;

#ifdef IMPROV_WIFI_BLE_ENABLED
    ImprovWiFiBLE improvBLE;
#endif

    // for making a captive portal
    //  The default android DNS
    const byte DNS_PORT = 53;
    DNSServer dnsServer;

    // --- THE CALLBACK TRAP ---
    // Libraries expecting C-style function pointers cannot take normal member functions.
    // We use a static instance pointer and static methods to bridge the gap.
    static NetworkController* _instance;

    void waitForBootPress();

    static void _onImprovErrorStatic(ImprovTypes::Error err);
    static void _onImprovConnectedStatic(const char* ssid, const char* password);
    static bool _onImprovCustomConnectWiFiStatic(const char* ssid, const char* password);

    // The actual member functions that handle the callbacks
    void _handleImprovError(ImprovTypes::Error err);
    void _handleImprovConnected(const char* ssid, const char* password);
};

#endif /* !YARR_NETWORK_H */