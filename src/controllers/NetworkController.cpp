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

#include "controllers/NetworkController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

NetworkController* NetworkController::_instance = nullptr;

NetworkController::NetworkController(YarrboardApp& app) : BaseController(app, "network"),
                                                          improvSerial(&Serial),
                                                          apIP(192, 168, 4, 1)
{
}

bool NetworkController::setup()
{
  _instance = this; // Capture the instance for callbacks

  _app.protocol.registerCommand(ADMIN, "set_network_config", this, &NetworkController::handleSetNetworkConfig);

  uint64_t chipid = ESP.getEfuseMac(); // unique 48-bit MAC base ID
  snprintf(_uuid, sizeof(_uuid), "%04X%08lX", (uint16_t)(chipid >> 32), (uint32_t)chipid);

  // pin 0 is boot pin
  pinMode(YB_BOOT_PIN, INPUT);

  if (_cfg.isFirstBoot())
    setupImprov();
  else
    setupWifi();

  return true;
}

void NetworkController::loop()
{
  if (_cfg.isFirstBoot()) {
    improvSerial.handleSerial();
  }
}

bool NetworkController::loadAdminConfigHook(JsonVariant config, char* error, size_t len)
{
  const char* v;

  v = config["local_hostname"] | _app.default_hostname;
  strlcpy(_local_hostname, v, sizeof(_local_hostname));

  v = config["wifi_ssid"] | YB_DEFAULT_AP_SSID;
  strlcpy(_wifi_ssid, v, sizeof(_wifi_ssid));

  v = config["wifi_pass"] | YB_DEFAULT_AP_PASS;
  strlcpy(_wifi_pass, v, sizeof(_wifi_pass));

  v = config["wifi_mode"] | YB_DEFAULT_AP_MODE;
  strlcpy(_wifi_mode, v, sizeof(_wifi_mode));

  _wifi_use_static_ip = config["wifi_use_static_ip"] | false;

  v = config["wifi_static_ip"] | "";
  strlcpy(_wifi_static_ip, v, sizeof(_wifi_static_ip));

  v = config["wifi_gateway"] | "";
  strlcpy(_wifi_gateway, v, sizeof(_wifi_gateway));

  v = config["wifi_subnet"] | "";
  strlcpy(_wifi_subnet, v, sizeof(_wifi_subnet));

  v = config["wifi_dns1"] | "";
  strlcpy(_wifi_dns1, v, sizeof(_wifi_dns1));

  v = config["wifi_dns2"] | "";
  strlcpy(_wifi_dns2, v, sizeof(_wifi_dns2));

  return true;
}

void NetworkController::generateAdminConfigHook(JsonVariant output)
{
  TRACE();

  output["wifi_mode"] = _wifi_mode;
  output["wifi_ssid"] = _wifi_ssid;
  output["wifi_pass"] = _wifi_pass;
  output["local_hostname"] = _local_hostname;
  output["wifi_use_static_ip"] = _wifi_use_static_ip;
  output["wifi_static_ip"] = _wifi_static_ip;
  output["wifi_gateway"] = _wifi_gateway;
  output["wifi_subnet"] = _wifi_subnet;
  output["wifi_dns1"] = _wifi_dns1;
  output["wifi_dns2"] = _wifi_dns2;
}

void NetworkController::setupWifi()
{
  // which mode do we want?
  if (!strcmp(_wifi_mode, "client")) {
    YBP.print("Client mode: ");
    YBP.print(_wifi_ssid);
    YBP.print(" / ");
    YBP.println(_wifi_pass);

    // try and connect
    if (connectToWifi(_wifi_ssid, _wifi_pass))
      startServices();
    else
      waitForBootPress();
  }
  // default to AP mode.
  else {
    YBP.print("AP mode: ");
    YBP.print(_wifi_ssid);
    YBP.print(" / ");
    YBP.println(_wifi_pass);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(_wifi_ssid, _wifi_pass);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    YBP.print("AP IP address: ");
    YBP.println(apIP);

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
  }
}

void NetworkController::waitForBootPress()
{
  unsigned long pressStartTime = 0;
  bool buttonPressed = false;

  while (true) {
    // Read the boot pin (LOW when pressed)
    if (digitalRead(YB_BOOT_PIN) == LOW) {
      if (!buttonPressed) {
        // Button just pressed, record the time
        buttonPressed = true;
        pressStartTime = millis();
      } else {
        // Button is being held, check if 5 seconds have elapsed
        if (millis() - pressStartTime >= 5000) {
          YBP.println("Boot button held for 5 seconds - resetting to first boot");
          _cfg.setFirstBoot(true);

          char error[128];
          _cfg.saveConfig(error, sizeof(error));
          ESP.restart();
        }
      }
    } else {
      // Button released, reset the tracking
      buttonPressed = false;
    }

    delay(100);
    yield();
  }
}

bool NetworkController::connectToWifi(const char* ssid, const char* pass)
{
  _app.setStatusColor(CRGB::Yellow);

  // reset our wifi to a clean state
  if (WiFi.isConnected()) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false); // Stop auto-reconnect attempts
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(100);
  }

  // some tuning
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false); // optional but usually helps reliability

  // apply static IP config before connecting, if requested
  if (_wifi_use_static_ip && strlen(_wifi_static_ip) > 0) {
    IPAddress ip, gw, sn, dns1, dns2;
    if (ip.fromString(_wifi_static_ip) &&
        gw.fromString(_wifi_gateway) &&
        sn.fromString(_wifi_subnet)) {
      dns1.fromString(strlen(_wifi_dns1) > 0 ? _wifi_dns1 : _wifi_gateway);
      dns2.fromString(strlen(_wifi_dns2) > 0 ? _wifi_dns2 : _wifi_gateway);
      WiFi.config(ip, gw, sn, dns1, dns2);
      YBP.printf("[WiFi] Static IP: %s / GW: %s / SN: %s / DNS1: %s / DNS2: %s\n",
        _wifi_static_ip,
        _wifi_gateway,
        _wifi_subnet,
        strlen(_wifi_dns1) > 0 ? _wifi_dns1 : _wifi_gateway,
        strlen(_wifi_dns2) > 0 ? _wifi_dns2 : _wifi_gateway);
    } else {
      YBP.println("[WiFi] Static IP config invalid, falling back to DHCP");
    }
  }

  // How long to try for?
  int tryDuration = 15000;
  int tryDelay = 50;
  int numberOfTries = tryDuration / tryDelay;

  YBP.print("[WiFi] Connecting to ");
  YBP.println(ssid);
  WiFi.setHostname(_local_hostname);
  WiFi.begin(ssid, pass);

  // attempt to connect
  while (numberOfTries > 0) {
    if (WiFi.status() == WL_CONNECTED) {
      YBP.println("\n[WiFi] WiFi is connected!");
      YBP.print("[WiFi] IP address: ");
      YBP.println(WiFi.localIP());

      _app.setStatusColor(CRGB::Green);

      return true;
    }

    if (WiFi.status() == WL_NO_SSID_AVAIL) {
      YBP.println("[WiFi] SSID not found");
      break;
    }

    YBP.print(".");

    numberOfTries--;

    delay(tryDelay);
    yield();
  }

  YBP.println("\n[WiFi] WiFi failed to connect");
  WiFi.setAutoReconnect(false); // Stop auto-reconnect attempts
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  _app.setStatusColor(CRGB::Red);

  return false;
}

void NetworkController::startMDNS()
{
  if (!WiFi.isConnected()) {
    YBP.println("[mDNS] Skipping restart - WiFi not connected");
    return;
  }
  if (_mdnsStarted) {
    MDNS.end();
    delay(100);
  }
  _mdnsStarted = true;
  if (!MDNS.begin(_local_hostname)) {
    YBP.println("[mDNS] Error starting mDNS");
  } else {
    MDNS.addService("http", "tcp", 80);
    YBP.println("[mDNS] mDNS started");
  }
}

void NetworkController::startServices()
{
  YBP.print("Hostname: ");
  YBP.print(_local_hostname);
  YBP.println(".local");

  // Restart mDNS whenever WiFi re-associates and gets an IP, so the .local
  // hostname recovers automatically from connection drops.
  if (!_mdnsEventRegistered) {
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (_instance)
        _instance->startMDNS();
    },
      ARDUINO_EVENT_WIFI_STA_GOT_IP);
    _mdnsEventRegistered = true;
  }

  startMDNS();
}

void NetworkController::setupImprov()
{
  YBP.println("First Boot: starting Improv");

  String device_url = "http://";
  device_url.concat(_local_hostname);
  device_url.concat(".local");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // Stop auto-reconnect attempts
  WiFi.disconnect();

  // Serial Configuration
  improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32,
    _cfg.getBoardName(),
    _app.firmware_version,
    _cfg.getBoardName(),
    device_url.c_str());

  improvSerial.onImprovError(_onImprovErrorStatic);
  improvSerial.setCustomConnectWiFi(_onImprovCustomConnectWiFiStatic);
  improvSerial.onImprovConnected(_onImprovConnectedStatic);

  // Bluetooth Configuration
#ifdef IMPROV_WIFI_BLE_ENABLED
  improvBLE.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32,
    _cfg.getBoardName(),
    _app.firmware_version,
    _cfg.getBoardName(),
    device_url.c_str());

  improvBLE.onImprovError(_onImprovErrorStatic);
  improvBLE.setCustomConnectWiFi(_onImprovCustomConnectWiFiStatic);
  improvBLE.onImprovConnected(_onImprovConnectedStatic);
#endif
}

// ==========================================================
//  Static Proxy Callbacks (The Bridge)
// ==========================================================

void NetworkController::_onImprovErrorStatic(ImprovTypes::Error err)
{
  if (_instance) {
    _instance->_handleImprovError(err);
  }
}

void NetworkController::_onImprovConnectedStatic(const char* ssid, const char* password)
{
  if (_instance) {
    _instance->_handleImprovConnected(ssid, password);
  }
}

bool NetworkController::_onImprovCustomConnectWiFiStatic(const char* ssid, const char* password)
{
  if (_instance) {
    return _instance->connectToWifi(ssid, password);
  }
  return false;
}

void NetworkController::_handleImprovError(ImprovTypes::Error err)
{
  YBP.printf("[improv] WiFi failed to connect (err=%d).\n", err);

  _app.setStatusColor(CRGB::Red);

  // reset our wifi.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // Stop auto-reconnect attempts
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(250);

  _app.setStatusColor(CRGB::Blue);
}

void NetworkController::_handleImprovConnected(const char* ssid, const char* password)
{
  YBP.printf("Improv Successful: %s / %s\n", ssid, password);

  // save our creds
  strlcpy(_wifi_mode, "client", sizeof(_wifi_mode));
  strlcpy(_wifi_ssid, ssid, sizeof(_wifi_ssid));
  strlcpy(_wifi_pass, password, sizeof(_wifi_pass));

  // we're connected now.
  _cfg.setFirstBoot(false);
  improvDone = true;
}

void NetworkController::handleSetNetworkConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  _cfg.setFirstBoot(false);

  char error[128];

  if (!input["wifi_mode"].is<String>())
    return _app.protocol.generateErrorJSON(output, "'wifi_mode' is a required parameter");
  if (!input["wifi_ssid"].is<String>())
    return _app.protocol.generateErrorJSON(output, "'wifi_ssid' is a required parameter");
  if (!input["wifi_pass"].is<String>())
    return _app.protocol.generateErrorJSON(output, "'wifi_pass' is a required parameter");
  if (!input["local_hostname"].is<String>())
    return _app.protocol.generateErrorJSON(output, "'local_hostname' is a required parameter");

  if (strlen(input["wifi_ssid"]) > YB_WIFI_SSID_LENGTH - 1) {
    sprintf(error, "Maximum wifi ssid length is %d characters.", YB_WIFI_SSID_LENGTH - 1);
    return _app.protocol.generateErrorJSON(output, error);
  }

  if (strlen(input["wifi_pass"]) > YB_WIFI_PASSWORD_LENGTH - 1) {
    sprintf(error, "Maximum wifi password length is %d characters.", YB_WIFI_PASSWORD_LENGTH - 1);
    return _app.protocol.generateErrorJSON(output, error);
  }

  if (strlen(input["local_hostname"]) > YB_HOSTNAME_LENGTH - 1) {
    sprintf(error, "Maximum hostname length is %d characters.", YB_HOSTNAME_LENGTH - 1);
    return _app.protocol.generateErrorJSON(output, error);
  }

  bool new_use_static_ip = input["wifi_use_static_ip"] | false;
  if (new_use_static_ip) {
    if (!input["wifi_static_ip"].is<String>() || strlen(input["wifi_static_ip"]) == 0)
      return _app.protocol.generateErrorJSON(output, "'wifi_static_ip' is required when wifi_use_static_ip is true");
    if (!input["wifi_gateway"].is<String>() || strlen(input["wifi_gateway"]) == 0)
      return _app.protocol.generateErrorJSON(output, "'wifi_gateway' is required when wifi_use_static_ip is true");
    if (!input["wifi_subnet"].is<String>() || strlen(input["wifi_subnet"]) == 0)
      return _app.protocol.generateErrorJSON(output, "'wifi_subnet' is required when wifi_use_static_ip is true");

    IPAddress testIP;
    if (!testIP.fromString(input["wifi_static_ip"] | ""))
      return _app.protocol.generateErrorJSON(output, "'wifi_static_ip' is not a valid IP address");
    if (!testIP.fromString(input["wifi_gateway"] | ""))
      return _app.protocol.generateErrorJSON(output, "'wifi_gateway' is not a valid IP address");
    if (!testIP.fromString(input["wifi_subnet"] | ""))
      return _app.protocol.generateErrorJSON(output, "'wifi_subnet' is not a valid IP address");
    if (input["wifi_dns1"].is<String>() && strlen(input["wifi_dns1"]) > 0) {
      if (!testIP.fromString(input["wifi_dns1"] | ""))
        return _app.protocol.generateErrorJSON(output, "'wifi_dns1' is not a valid IP address");
    }
    if (input["wifi_dns2"].is<String>() && strlen(input["wifi_dns2"]) > 0) {
      if (!testIP.fromString(input["wifi_dns2"] | ""))
        return _app.protocol.generateErrorJSON(output, "'wifi_dns2' is not a valid IP address");
    }
  }

  char new_wifi_mode[16];
  char new_wifi_ssid[YB_WIFI_SSID_LENGTH];
  char new_wifi_pass[YB_WIFI_PASSWORD_LENGTH];

  strlcpy(new_wifi_mode, input["wifi_mode"] | YB_DEFAULT_AP_MODE, sizeof(new_wifi_mode));
  strlcpy(new_wifi_ssid, input["wifi_ssid"] | YB_DEFAULT_AP_SSID, sizeof(new_wifi_ssid));
  strlcpy(new_wifi_pass, input["wifi_pass"] | YB_DEFAULT_AP_PASS, sizeof(new_wifi_pass));

  // Snapshot current state so we can roll back if the new wifi connection fails
  char old_hostname[YB_HOSTNAME_LENGTH];
  bool old_use_static_ip = _wifi_use_static_ip;
  char old_static_ip[YB_IP_ADDRESS_LENGTH];
  char old_gateway[YB_IP_ADDRESS_LENGTH];
  char old_subnet[YB_IP_ADDRESS_LENGTH];
  char old_dns1[YB_IP_ADDRESS_LENGTH];
  char old_dns2[YB_IP_ADDRESS_LENGTH];
  strlcpy(old_hostname, _local_hostname, sizeof(old_hostname));
  strlcpy(old_static_ip, _wifi_static_ip, sizeof(old_static_ip));
  strlcpy(old_gateway, _wifi_gateway, sizeof(old_gateway));
  strlcpy(old_subnet, _wifi_subnet, sizeof(old_subnet));
  strlcpy(old_dns1, _wifi_dns1, sizeof(old_dns1));
  strlcpy(old_dns2, _wifi_dns2, sizeof(old_dns2));

  strlcpy(_local_hostname, input["local_hostname"] | _app.default_hostname, sizeof(_local_hostname));
  _wifi_use_static_ip = new_use_static_ip;
  strlcpy(_wifi_static_ip, input["wifi_static_ip"] | "", sizeof(_wifi_static_ip));
  strlcpy(_wifi_gateway, input["wifi_gateway"] | "", sizeof(_wifi_gateway));
  strlcpy(_wifi_subnet, input["wifi_subnet"] | "", sizeof(_wifi_subnet));
  strlcpy(_wifi_dns1, input["wifi_dns1"] | "", sizeof(_wifi_dns1));
  strlcpy(_wifi_dns2, input["wifi_dns2"] | "", sizeof(_wifi_dns2));

  if (!strcmp(new_wifi_mode, "client")) {
    if (strcmp(new_wifi_ssid, getWifiSSID()) || strcmp(new_wifi_pass, getWifiPass())) {
      YBP.printf("Trying new wifi %s / %s\n", new_wifi_ssid, new_wifi_pass);
      if (connectToWifi(new_wifi_ssid, new_wifi_pass)) {
        if (!strcmp(getWifiMode(), "ap"))
          WiFi.softAPdisconnect();

        strlcpy(_wifi_mode, new_wifi_mode, sizeof(_wifi_mode));
        strlcpy(_wifi_ssid, new_wifi_ssid, sizeof(_wifi_ssid));
        strlcpy(_wifi_pass, new_wifi_pass, sizeof(_wifi_pass));

        if (!_cfg.saveConfig(error, sizeof(error)))
          return _app.protocol.generateErrorJSON(output, error);
      } else {
        strlcpy(_local_hostname, old_hostname, sizeof(_local_hostname));
        _wifi_use_static_ip = old_use_static_ip;
        strlcpy(_wifi_static_ip, old_static_ip, sizeof(_wifi_static_ip));
        strlcpy(_wifi_gateway, old_gateway, sizeof(_wifi_gateway));
        strlcpy(_wifi_subnet, old_subnet, sizeof(_wifi_subnet));
        strlcpy(_wifi_dns1, old_dns1, sizeof(_wifi_dns1));
        strlcpy(_wifi_dns2, old_dns2, sizeof(_wifi_dns2));
        connectToWifi(getWifiSSID(), getWifiPass());
        startServices();
        return _app.protocol.generateErrorJSON(output, "Can't connect to new WiFi.");
      }
    } else {
      if (!_cfg.saveConfig(error, sizeof(error)))
        return _app.protocol.generateErrorJSON(output, error);
    }
  } else {
    strlcpy(_wifi_mode, new_wifi_mode, sizeof(_wifi_mode));
    strlcpy(_wifi_ssid, new_wifi_ssid, sizeof(_wifi_ssid));
    strlcpy(_wifi_pass, new_wifi_pass, sizeof(_wifi_pass));

    setupWifi();

    if (!_cfg.saveConfig(error, sizeof(error)))
      return _app.protocol.generateErrorJSON(output, error);

    return _app.protocol.generateSuccessJSON(output, "AP mode successful, please connect to new network.");
  }
}