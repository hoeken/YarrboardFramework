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
  strlcpy(defaults.wifi_ssid, YB_DEFAULT_AP_SSID, sizeof(defaults.wifi_ssid));
  strlcpy(defaults.wifi_pass, YB_DEFAULT_AP_PASS, sizeof(defaults.wifi_pass));
  strlcpy(defaults.wifi_mode, YB_DEFAULT_AP_MODE, sizeof(defaults.wifi_mode));
  strlcpy(defaults.local_hostname, "yarrboard", sizeof(defaults.local_hostname));
  defaults.wifi_use_static_ip = false;
  defaults.wifi_static_ip[0] = '\0';
  defaults.wifi_gateway[0] = '\0';
  defaults.wifi_subnet[0] = '\0';
  defaults.wifi_dns1[0] = '\0';
  defaults.wifi_dns2[0] = '\0';
  _config = defaults;
}

bool NetworkController::setup()
{
  _instance = this; // Capture the instance for callbacks

  if (!BaseController::setup())
    return false;

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

bool NetworkController::sanitizeConfigHook(JsonVariant config, char* error, size_t len)
{
  if (config["wifi_mode"]) {
    const char* v = config["wifi_mode"];
    if (strcmp(v, "ap") && strcmp(v, "client")) {
      snprintf(error, len, "Invalid wifi_mode '%s', must be 'ap' or 'client'", v);
      config.remove("wifi_mode");
      return false;
    }
  }

  if (config["wifi_ssid"] && strlen(config["wifi_ssid"] | "") > YB_WIFI_SSID_LENGTH - 1) {
    snprintf(error, len, "wifi_ssid too long (max %d chars), will be truncated", YB_WIFI_SSID_LENGTH - 1);
    return false;
  }
  if (config["wifi_pass"] && strlen(config["wifi_pass"] | "") > YB_WIFI_PASSWORD_LENGTH - 1) {
    snprintf(error, len, "wifi_pass too long (max %d chars), will be truncated", YB_WIFI_PASSWORD_LENGTH - 1);
    return false;
  }
  if (config["local_hostname"] && strlen(config["local_hostname"] | "") > YB_HOSTNAME_LENGTH - 1) {
    snprintf(error, len, "local_hostname too long (max %d chars), will be truncated", YB_HOSTNAME_LENGTH - 1);
    return false;
  }

  bool new_use_static_ip = config["wifi_use_static_ip"] | false;
  if (new_use_static_ip) {
    if (!config["wifi_static_ip"].is<String>() || strlen(config["wifi_static_ip"]) == 0) {
      snprintf(error, len, "'wifi_static_ip' is required when wifi_use_static_ip is true");
      return false;
    }
    if (!config["wifi_gateway"].is<String>() || strlen(config["wifi_gateway"]) == 0) {
      snprintf(error, len, "'wifi_gateway' is required when wifi_use_static_ip is true");
      return false;
    }
    if (!config["wifi_subnet"].is<String>() || strlen(config["wifi_subnet"]) == 0) {
      snprintf(error, len, "'wifi_subnet' is required when wifi_use_static_ip is true");
      return false;
    }
  }

  IPAddress ip;
  const char* ip_fields[] = {"wifi_static_ip", "wifi_gateway", "wifi_subnet", "wifi_dns1", "wifi_dns2"};
  for (const char* field : ip_fields) {
    if (config[field]) {
      const char* v = config[field] | "";
      if (strlen(v) > 0 && !ip.fromString(v)) {
        snprintf(error, len, "Invalid IPv4 address for '%s': '%s'", field, v);
        config.remove(field);
        return false;
      }
    }
  }

  return true;
}

void NetworkController::loadConfigHook(JsonVariantConst config)
{
  const char* v;

  v = config["local_hostname"] | defaults.local_hostname;
  strlcpy(_config.local_hostname, v, sizeof(_config.local_hostname));

  v = config["wifi_ssid"] | defaults.wifi_ssid;
  strlcpy(_config.wifi_ssid, v, sizeof(_config.wifi_ssid));

  v = config["wifi_pass"] | defaults.wifi_pass;
  strlcpy(_config.wifi_pass, v, sizeof(_config.wifi_pass));

  v = config["wifi_mode"] | defaults.wifi_mode;
  strlcpy(_config.wifi_mode, v, sizeof(_config.wifi_mode));

  _config.wifi_use_static_ip = config["wifi_use_static_ip"] | defaults.wifi_use_static_ip;

  v = config["wifi_static_ip"] | defaults.wifi_static_ip;
  strlcpy(_config.wifi_static_ip, v, sizeof(_config.wifi_static_ip));

  v = config["wifi_gateway"] | defaults.wifi_gateway;
  strlcpy(_config.wifi_gateway, v, sizeof(_config.wifi_gateway));

  v = config["wifi_subnet"] | defaults.wifi_subnet;
  strlcpy(_config.wifi_subnet, v, sizeof(_config.wifi_subnet));

  v = config["wifi_dns1"] | defaults.wifi_dns1;
  strlcpy(_config.wifi_dns1, v, sizeof(_config.wifi_dns1));

  v = config["wifi_dns2"] | defaults.wifi_dns2;
  strlcpy(_config.wifi_dns2, v, sizeof(_config.wifi_dns2));
}

void NetworkController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  if (purpose == ConfigPurpose::UI_CONFIG) {
    output["uuid"] = getUUID();
  }

  output["local_hostname"] = _config.local_hostname;

  if (role == ADMIN) {

    if (purpose != ConfigPurpose::SHAREABLE) {
      output["wifi_ssid"] = _config.wifi_ssid;
      output["wifi_pass"] = _config.wifi_pass;
    }

    output["wifi_mode"] = _config.wifi_mode;
    output["wifi_use_static_ip"] = _config.wifi_use_static_ip;
    output["wifi_static_ip"] = _config.wifi_static_ip;
    output["wifi_gateway"] = _config.wifi_gateway;
    output["wifi_subnet"] = _config.wifi_subnet;
    output["wifi_dns1"] = _config.wifi_dns1;
    output["wifi_dns2"] = _config.wifi_dns2;
  }
}

void NetworkController::setupWifi()
{
  // which mode do we want?
  if (!strcmp(_config.wifi_mode, "client")) {
    // try and connect
    if (connectToWifi(_config.wifi_ssid, _config.wifi_pass))
      startServices();
    else
      waitForBootPress();
  }
  // default to AP mode.
  else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_config.wifi_ssid, _config.wifi_pass);
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
  if (_config.wifi_use_static_ip) {
    IPAddress ip, gw, sn, dns1, dns2;
    ip.fromString(_config.wifi_static_ip);
    gw.fromString(_config.wifi_gateway);
    sn.fromString(_config.wifi_subnet);

    dns1.fromString(strlen(_config.wifi_dns1) > 0 ? _config.wifi_dns1 : _config.wifi_gateway);
    dns2.fromString(strlen(_config.wifi_dns2) > 0 ? _config.wifi_dns2 : _config.wifi_gateway);
    WiFi.config(ip, gw, sn, dns1, dns2);

    YBP.printf("[WiFi] Static IP: %s / GW: %s / SN: %s / DNS1: %s / DNS2: %s\n",
      _config.wifi_static_ip,
      _config.wifi_gateway,
      _config.wifi_subnet,
      strlen(_config.wifi_dns1) > 0 ? _config.wifi_dns1 : _config.wifi_gateway,
      strlen(_config.wifi_dns2) > 0 ? _config.wifi_dns2 : _config.wifi_gateway);
  }

  // How long to try for?
  int tryDuration = 15000;
  int tryDelay = 50;
  int numberOfTries = tryDuration / tryDelay;

  YBP.print("[WiFi] Connecting to ");
  YBP.println(ssid);
  WiFi.setHostname(_config.local_hostname);
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
  if (!MDNS.begin(_config.local_hostname)) {
    YBP.println("[mDNS] Error starting mDNS");
  } else {
    MDNS.addService("http", "tcp", 80);
    YBP.println("[mDNS] mDNS started");
  }
}

void NetworkController::startServices()
{
  YBP.print("Hostname: ");
  YBP.print(_config.local_hostname);
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
  _app.setStatusColor(CRGB::Blue);

  YBP.println("First Boot: starting Improv");

  String device_url = "http://";
  device_url.concat(_config.local_hostname);
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
  strlcpy(_config.wifi_mode, "client", sizeof(_config.wifi_mode));
  strlcpy(_config.wifi_ssid, ssid, sizeof(_config.wifi_ssid));
  strlcpy(_config.wifi_pass, password, sizeof(_config.wifi_pass));

  // we're connected now.
  _cfg.setFirstBoot(false);
  improvDone = true;
}

bool NetworkController::handleSetConfigSuccessCallback(JsonVariantConst input, JsonVariant output, ProtocolContext context, char* error, size_t len)
{
  const char* new_mode = input["wifi_mode"] | _config.wifi_mode;

  if (!strcmp(new_mode, "client")) {
    const char* new_ssid = input["wifi_ssid"] | _config.wifi_ssid;
    const char* new_pass = input["wifi_pass"] | _config.wifi_pass;

    if (strcmp(new_ssid, _config.wifi_ssid) || strcmp(new_pass, _config.wifi_pass)) {
      YBP.printf("Trying new wifi %s / %s\n", new_ssid, new_pass);
      if (connectToWifi(new_ssid, new_pass)) {
        if (!strcmp(_config.wifi_mode, "ap"))
          WiFi.softAPdisconnect();
        startServices();
      } else {
        connectToWifi(_config.wifi_ssid, _config.wifi_pass);
        startServices();
        snprintf(error, len, "Can't connect to new WiFi.");
        return false;
      }
    }
  } else {
    // for AP mode
    setupWifi();
  }

  _cfg.setFirstBoot(false);

  return true;
}