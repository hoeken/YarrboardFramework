/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "NetworkController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"
#include "rgb.h"

NetworkController* NetworkController::_instance = nullptr;

NetworkController::NetworkController(YarrboardApp& app, ConfigManager& config) : _app(app),
                                                                                 _config(config),
                                                                                 improvSerial(&Serial),
                                                                                 apIP(8, 8, 4, 4)
{
}

bool NetworkController::setup()
{

  _instance = this; // Capture the instance for callbacks

  uint64_t chipid = ESP.getEfuseMac(); // unique 48-bit MAC base ID
  snprintf(_config.uuid, sizeof(_config.uuid), "%04X%08lX", (uint16_t)(chipid >> 32), (uint32_t)chipid);

  if (_config.is_first_boot)
    setupImprov();
  else
    setupWifi();

  return !_config.is_first_boot;
}

void NetworkController::loop()
{
}

void NetworkController::setupWifi()
{
  // which mode do we want?
  if (!strcmp(_config.wifi_mode, "client")) {
    YBP.print("Client mode: ");
    YBP.print(_config.wifi_ssid);
    YBP.print(" / ");
    YBP.println(_config.wifi_pass);

    // try and connect
    if (connectToWifi(_config.wifi_ssid, _config.wifi_pass))
      startServices();
  }
  // default to AP mode.
  else {
    YBP.print("AP mode: ");
    YBP.print(_config.wifi_ssid);
    YBP.print(" / ");
    YBP.println(_config.wifi_pass);

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

bool NetworkController::connectToWifi(const char* ssid, const char* pass)
{
#ifdef YB_HAS_STATUS_RGB
  rgb_set_status_color(CRGB::Yellow);
#endif

  // reset our wifi to a clean state
  if (WiFi.isConnected()) {
    WiFi.mode(WIFI_STA);
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
#ifdef YB_HAS_STATUS_RGB
      rgb_set_status_color(CRGB::Green);
#endif
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
  WiFi.disconnect(true, true);

#ifdef YB_HAS_STATUS_RGB
  rgb_set_status_color(CRGB::Red);
#endif

  return false;
}

void NetworkController::startServices()
{
  // some global config
  WiFi.setHostname(_config.local_hostname);

  YBP.print("Hostname: ");
  YBP.print(_config.local_hostname);
  YBP.println(".local");

  // setup our local name.
  if (!MDNS.begin(_config.local_hostname))
    YBP.println("Error starting mDNS");
  MDNS.addService("http", "tcp", 80);
}

void NetworkController::setupImprov()
{
  YBP.println("First Boot: starting Improv");

  String device_url = "http://";
  device_url.concat(_config.local_hostname);
  device_url.concat(".local");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Identify this device
  improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32,
    _config.board_name,
    YB_FIRMWARE_VERSION,
    _config.board_name,
    device_url.c_str());

  improvSerial.onImprovError(_onImprovErrorStatic);
  improvSerial.onImprovConnected(_onImprovConnectedStatic);
  improvSerial.setCustomConnectWiFi(_onImprovCustomConnectWiFiStatic);

  // Identify this device
  improvBLE.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32,
    _config.board_name,
    YB_FIRMWARE_VERSION,
    _config.board_name,
    device_url.c_str());

  improvBLE.onImprovError(_onImprovErrorStatic);
  improvBLE.onImprovConnected(_onImprovConnectedStatic);
  improvBLE.setCustomConnectWiFi(_onImprovCustomConnectWiFiStatic);
}

void NetworkController::loopImprov()
{
  improvSerial.handleSerial();
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
  YBP.printf("wifi error: %d\n", err);

#ifdef YB_HAS_STATUS_RGB
  rgb_set_status_color(CRGB::Red);
#endif
}

void NetworkController::_handleImprovConnected(const char* ssid, const char* password)
{
  YBP.printf("Improv Successful: %s / %s\n", ssid, password);

  strncpy(_config.wifi_mode, "client", sizeof(_config.wifi_mode));
  strncpy(_config.wifi_ssid, ssid, sizeof(_config.wifi_ssid));
  strncpy(_config.wifi_pass, password, sizeof(_config.wifi_pass));

  char error[128];
  _config.saveConfig(error, sizeof(error));

  // a bit hacky until I can figure out how to call app.setup from here.
  ESP.restart();
}