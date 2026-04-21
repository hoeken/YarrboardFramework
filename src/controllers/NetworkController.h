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
#include "controllers/ProtocolController.h"
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

    const char* getUUID() const { return _uuid; }

    const char* getWifiSSID() const { return _wifi_ssid; }
    const char* getWifiPass() const { return _wifi_pass; }
    const char* getWifiMode() const { return _wifi_mode; }
    const char* getLocalHostname() const { return _local_hostname; }
    bool getWifiUseStaticIP() const { return _wifi_use_static_ip; }
    const char* getWifiStaticIP() const { return _wifi_static_ip; }
    const char* getWifiGateway() const { return _wifi_gateway; }
    const char* getWifiSubnet() const { return _wifi_subnet; }
    const char* getWifiDNS1() const { return _wifi_dns1; }
    const char* getWifiDNS2() const { return _wifi_dns2; }

    bool isImprovDone() const { return improvDone; }
    void setImprovDone(bool v) { improvDone = v; }
    const IPAddress& getApIP() const { return apIP; }

    bool loadNetworkConfig(JsonVariant config, char* error, size_t len);
    void generateNetworkConfig(JsonVariant output);

    void handleGetNetworkConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetNetworkConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);

  private:
    char _uuid[YB_UUID_LENGTH] = {};
    char _wifi_ssid[YB_WIFI_SSID_LENGTH] = YB_DEFAULT_AP_SSID;
    char _wifi_pass[YB_WIFI_PASSWORD_LENGTH] = YB_DEFAULT_AP_PASS;
    char _wifi_mode[YB_WIFI_MODE_LENGTH] = YB_DEFAULT_AP_MODE;
    char _local_hostname[YB_HOSTNAME_LENGTH] = {};
    bool _wifi_use_static_ip = false;
    char _wifi_static_ip[YB_IP_ADDRESS_LENGTH] = {};
    char _wifi_gateway[YB_IP_ADDRESS_LENGTH] = {};
    char _wifi_subnet[YB_IP_ADDRESS_LENGTH] = {};
    char _wifi_dns1[YB_IP_ADDRESS_LENGTH] = {};
    char _wifi_dns2[YB_IP_ADDRESS_LENGTH] = {};
    IPAddress apIP;
    bool improvDone = false;

    ImprovWiFi improvSerial;

#ifdef IMPROV_WIFI_BLE_ENABLED
    ImprovWiFiBLE improvBLE;
#endif

    bool _mdnsEventRegistered = false;
    bool _mdnsStarted = false;

    // for making a captive portal
    //  The default android DNS
    const byte DNS_PORT = 53;
    DNSServer dnsServer;

    // --- THE CALLBACK TRAP ---
    // Libraries expecting C-style function pointers cannot take normal member functions.
    // We use a static instance pointer and static methods to bridge the gap.
    static NetworkController* _instance;

    void startMDNS();
    void waitForBootPress();

    static void _onImprovErrorStatic(ImprovTypes::Error err);
    static void _onImprovConnectedStatic(const char* ssid, const char* password);
    static bool _onImprovCustomConnectWiFiStatic(const char* ssid, const char* password);

    // The actual member functions that handle the callbacks
    void _handleImprovError(ImprovTypes::Error err);
    void _handleImprovConnected(const char* ssid, const char* password);
};

#endif /* !YARR_NETWORK_H */