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
                                                          apIP(8, 8, 4, 4)
{
}

bool NetworkController::setup()
{
  _instance = this; // Capture the instance for callbacks

  uint64_t chipid = ESP.getEfuseMac(); // unique 48-bit MAC base ID
  snprintf(_cfg.uuid, sizeof(_cfg.uuid), "%04X%08lX", (uint16_t)(chipid >> 32), (uint32_t)chipid);

  // pin 0 is boot pin
  pinMode(YB_BOOT_PIN, INPUT);

  if (_cfg.is_first_boot)
    setupImprov();
  else
    setupWifi();

  return true;
}

void NetworkController::loop()
{
  if (_cfg.is_first_boot) {
    improvSerial.handleSerial();
  }
}

void NetworkController::setupWifi()
{
  // which mode do we want?
  if (!strcmp(_cfg.wifi_mode, "client")) {
    YBP.print("Client mode: ");
    YBP.print(_cfg.wifi_ssid);
    YBP.print(" / ");
    YBP.println(_cfg.wifi_pass);

    // try and connect
    if (connectToWifi(_cfg.wifi_ssid, _cfg.wifi_pass))
      startServices();
    else
      waitForBootPress();
  }
  // default to AP mode.
  else {
    YBP.print("AP mode: ");
    YBP.print(_cfg.wifi_ssid);
    YBP.print(" / ");
    YBP.println(_cfg.wifi_pass);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(_cfg.wifi_ssid, _cfg.wifi_pass);
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
          _cfg.is_first_boot = true;

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

  // How long to try for?
  int tryDuration = 15000;
  int tryDelay = 50;
  int numberOfTries = tryDuration / tryDelay;

  YBP.print("[WiFi] Connecting to ");
  YBP.println(ssid);
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

void NetworkController::startServices()
{
  // some global config
  WiFi.setHostname(_cfg.local_hostname);

  YBP.print("Hostname: ");
  YBP.print(_cfg.local_hostname);
  YBP.println(".local");

  // setup our local name.
  if (!MDNS.begin(_cfg.local_hostname))
    YBP.println("Error starting mDNS");
  MDNS.addService("http", "tcp", 80);
}

void NetworkController::setupImprov()
{
  YBP.println("First Boot: starting Improv");

  String device_url = "http://";
  device_url.concat(_cfg.local_hostname);
  device_url.concat(".local");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // Stop auto-reconnect attempts
  WiFi.disconnect();

  // Serial Configuration
  improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32,
    _cfg.board_name,
    _app.firmware_version,
    _cfg.board_name,
    device_url.c_str());

  improvSerial.onImprovError(_onImprovErrorStatic);
  improvSerial.setCustomConnectWiFi(_onImprovCustomConnectWiFiStatic);
  improvSerial.onImprovConnected(_onImprovConnectedStatic);

  // Bluetooth Configuration
#ifdef IMPROV_WIFI_BLE_ENABLED
  improvBLE.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32,
    _cfg.board_name,
    _app.firmware_version,
    _cfg.board_name,
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
  YBP.printf("[improv] WiFi failed to connect.\n", err);

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
  strncpy(_cfg.wifi_mode, "client", sizeof(_cfg.wifi_mode));
  strncpy(_cfg.wifi_ssid, ssid, sizeof(_cfg.wifi_ssid));
  strncpy(_cfg.wifi_pass, password, sizeof(_cfg.wifi_pass));

  // we're connected now.
  _cfg.is_first_boot = false;
  improvDone = true;
}