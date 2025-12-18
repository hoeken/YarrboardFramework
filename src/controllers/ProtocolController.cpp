/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "controllers/ProtocolController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"
#include "controllers/MQTTController.h"
#include "controllers/OTAController.h"
#include "utility.h"

ProtocolController::ProtocolController(YarrboardApp& app) : BaseController(app, "protocol")
{
}

bool ProtocolController::setup()
{
  registerCommand(NOBODY, "ping", this, &ProtocolController::handlePing);

  registerCommand(GUEST, "get_config", this, &ProtocolController::handleGetConfig);
  registerCommand(GUEST, "get_stats", this, &ProtocolController::handleGetStats);
  registerCommand(GUEST, "get_update", this, &ProtocolController::handleGetUpdate);
  registerCommand(GUEST, "set_theme", this, &ProtocolController::handleSetTheme);
  registerCommand(GUEST, "set_brightness", this, &ProtocolController::handleSetBrightness);

#ifdef YB_IS_BRINEOMATIC
  registerCommand(GUEST, "start_watermaker", this, &ProtocolController::handleStartWatermaker);
  registerCommand(GUEST, "flush_watermaker", this, &ProtocolController::handleFlushWatermaker);
  registerCommand(GUEST, "pickle_watermaker", this, &ProtocolController::handlePickleWatermaker);
  registerCommand(GUEST, "depickle_watermaker", this, &ProtocolController::handleDepickleWatermaker);
  registerCommand(GUEST, "stop_watermaker", this, &ProtocolController::handleStopWatermaker);
  registerCommand(GUEST, "idle_watermaker", this, &ProtocolController::handleIdleWatermaker);
  registerCommand(GUEST, "manual_watermaker", this, &ProtocolController::handleManualWatermaker);
  registerCommand(GUEST, "set_watermaker", this, &ProtocolController::handleSetWatermaker);
  registerCommand(GUEST, "brineomatic_save_general_config", this, &ProtocolController::handleBrineomaticSaveGeneralConfig);
  registerCommand(GUEST, "brineomatic_save_hardware_config", this, &ProtocolController::handleBrineomaticSaveHardwareConfig);
  registerCommand(GUEST, "brineomatic_save_safeguards_config", this, &ProtocolController::handleBrineomaticSaveSafeguardsConfig);
#endif

  registerCommand(ADMIN, "set_general_config", this, &ProtocolController::handleSetGeneralConfig);
  registerCommand(ADMIN, "save_config", this, &ProtocolController::handleSaveConfig);
  registerCommand(ADMIN, "get_full_config", this, &ProtocolController::handleGetFullConfig);
  registerCommand(ADMIN, "get_network_config", this, &ProtocolController::handleGetNetworkConfig);
  registerCommand(ADMIN, "get_app_config", this, &ProtocolController::handleGetAppConfig);
  registerCommand(ADMIN, "set_network_config", this, &ProtocolController::handleSetNetworkConfig);
  registerCommand(ADMIN, "set_authentication_config", this, &ProtocolController::handleSetAuthenticationConfig);
  registerCommand(ADMIN, "set_webserver_config", this, &ProtocolController::handleSetWebServerConfig);
  registerCommand(ADMIN, "set_mqtt_config", this, &ProtocolController::handleSetMQTTConfig);
  registerCommand(ADMIN, "set_misc_config", this, &ProtocolController::handleSetMiscellaneousConfig);
  registerCommand(ADMIN, "restart", this, &ProtocolController::handleRestart);
  registerCommand(ADMIN, "crashme", this, &ProtocolController::handleCrashMe);
  registerCommand(ADMIN, "factory_reset", this, &ProtocolController::handleFactoryReset);
  registerCommand(ADMIN, "ota_start", this, &ProtocolController::handleOTAStart);

  // send serial a config off the bat
  if (_cfg.app_enable_serial) {
    JsonDocument output;
    generateConfigJSON(output);
    serializeJson(output, Serial);
  }

  return true;
}

void ProtocolController::loop()
{
  // lookup our info periodically
  unsigned int messageDelta = millis() - previousMessageMillis;
  if (messageDelta >= 1000) {

    // for keeping track.
    receivedMessagesPerSecond = receivedMessages;
    receivedMessages = 0;
    sentMessagesPerSecond = sentMessages;
    sentMessages = 0;

    previousMessageMillis = millis();
  }

  // any serial port customers?
  if (_cfg.app_enable_serial) {
    if (Serial.available() > 0)
      handleSerialJson();
  }
}

bool ProtocolController::registerCommand(UserRole role, const char* command, ProtocolMessageHandler handler)
{
  if (commandMap.full()) {
    YBP.printf("❌ Error: Protocol command list is full. (%s)\n", command);
    return false;
  }

  if (hasCommand(command))
    YBP.printf("⚠️ Warning: Overwriting protocol command '%s'\n", command);

  commandMap[command] = {role, handler};
  return true;
}

bool ProtocolController::unregisterCommand(const char* command)
{
  return commandMap.erase(command) > 0;
}

bool ProtocolController::hasCommand(const char* command)
{
  return commandMap.find(command) != commandMap.end();
}

void ProtocolController::printCommands()
{
  YBP.println("Protocol Commands:");
  for (const auto& kvp : commandMap)
    YBP.printf("%-6s | %s\n", getRoleText(kvp.second.role), kvp.first);
}

void ProtocolController::incrementSentMessages()
{
  // keep track!
  sentMessages++;
  totalSentMessages++;
}

bool ProtocolController::isSerialAuthenticated()
{
  return is_serial_authenticated;
}

void ProtocolController::handleSerialJson()
{
  JsonDocument input;
  DeserializationError err = deserializeJson(input, Serial);
  JsonDocument output;

  // ignore newlines with serial.
  if (err) {
    if (strcmp(err.c_str(), "EmptyInput")) {
      char error[64];
      sprintf(error, "deserializeJson() failed with code %s", err.c_str());
      generateErrorJSON(output, error);
      serializeJson(output, Serial);
    }
  } else {
    handleReceivedJSON(input, output, YBP_MODE_SERIAL);

    // we can have empty responses
    if (output.size()) {
      serializeJson(output, Serial);

      sentMessages++;
      totalSentMessages++;
    }
  }
}

// Returns true if 'userRole' is sufficient to execute a command requiring 'requiredRole'
bool ProtocolController::hasPermission(UserRole requiredRole, UserRole userRole)
{
  // 1. ADMIN can do everything
  if (userRole == ADMIN)
    return true;

  // 2. GUEST can handle GUEST or NOBODY
  if (userRole == GUEST && (requiredRole == GUEST || requiredRole == NOBODY))
    return true;

  // 3. NOBODY can only handle NOBODY
  if (userRole == NOBODY && requiredRole == NOBODY)
    return true;

  return false;
}

void ProtocolController::handleReceivedJSON(JsonVariantConst input, JsonVariant output, YBMode mode,
  PsychicWebSocketClient* connection)
{
  // make sure its correct
  if (!input["cmd"].is<String>())
    return generateErrorJSON(output, "'cmd' is a required parameter.");

  // what is your command?
  const char* cmd = input["cmd"];

  // let the client keep track of messages
  if (input["msgid"].is<unsigned int>()) {
    unsigned int msgid = input["msgid"];
    output["status"] = "ok";
    output["msgid"] = msgid;
  }

  // keep track!
  receivedMessages++;
  totalReceivedMessages++;

  // what would you say you do around here?
  UserRole role = _app.auth.getUserRole(input, mode, connection->socket());

  // Try to find the command in the new map system
  auto it = commandMap.find(cmd);

  // If FOUND, process it here and return.
  // If NOT found, skip this block and let the legacy code handle it.
  if (it != commandMap.end()) {

    // We found the command, so we must enforce auth.
    // Do NOT fall through if unauthorized.
    if (!hasPermission(it->second.role, role)) {
      return generateErrorJSON(output, "Unauthorized.");
    }

    // Execute Handler
    if (it->second.handler) {
      it->second.handler(input, output);
      return;
    }
  }

  // login is a tricky one that we really need mode + connection information for.
  if (!strcmp(cmd, "login"))
    return handleLogin(input, output, mode, connection);
  // hello is also a tricky one since we need to let them know their role.
  else if (!strcmp(cmd, "hello"))
    return generateHelloJSON(output, role);
  // logout is another special case.
  else if (!strcmp(cmd, "logout"))
    return handleLogout(input, output, mode, connection);

  // if we got here, no bueno.
  String error = "Invalid command: " + String(cmd);
  return generateErrorJSON(output, error.c_str());
}

const char* ProtocolController::getRoleText(UserRole role)
{
  if (role == ADMIN)
    return "admin";
  else if (role == GUEST)
    return "guest";
  else
    return "nobody";
}

void ProtocolController::generateHelloJSON(JsonVariant output, UserRole role)
{
  output["msg"] = "hello";
  output["role"] = getRoleText(role);
  output["default_role"] = getRoleText(_cfg.app_default_role);
  output["name"] = _cfg.board_name;
  output["brightness"] = _cfg.globalBrightness;
  output["firmware_version"] = _app.firmware_version;
}

void ProtocolController::handleGetConfig(JsonVariantConst input, JsonVariant output)
{
  generateConfigJSON(output);
}

void ProtocolController::handleGetStats(JsonVariantConst input, JsonVariant output)
{
  generateStatsJSON(output);
}

void ProtocolController::handleGetUpdate(JsonVariantConst input, JsonVariant output)
{
  generateUpdateJSON(output);
}

void ProtocolController::handleGetFullConfig(JsonVariantConst input, JsonVariant output)
{
  generateFullConfigMessage(output);
}

void ProtocolController::handleGetNetworkConfig(JsonVariantConst input, JsonVariant output)
{
  generateNetworkConfigMessage(output);
}

void ProtocolController::handleGetAppConfig(JsonVariantConst input, JsonVariant output)
{
  generateAppConfigMessage(output);
}

void ProtocolController::handleSetGeneralConfig(JsonVariantConst input, JsonVariant output)
{
  if (!input["board_name"].is<String>())
    return generateErrorJSON(output, "'board_name' is a required parameter");

  // is it too long?
  if (strlen(input["board_name"]) > YB_BOARD_NAME_LENGTH - 1) {
    char error[50];
    sprintf(error, "Maximum board name length is %s characters.", YB_BOARD_NAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // update variable
  strlcpy(_cfg.board_name, input["board_name"] | _app.board_name, sizeof(_cfg.board_name));
  strlcpy(_cfg.startup_melody, input["startup_melody"] | _app.default_melody, sizeof(_cfg.startup_melody));

  // save it to file.
  char error[128];
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // give them the updated config
  generateConfigJSON(output);
}

void ProtocolController::handleSetNetworkConfig(JsonVariantConst input, JsonVariant output)
{
  // clear our first boot flag since they submitted the network page.
  _cfg.is_first_boot = false;

  char error[128];

  // error checking
  if (!input["wifi_mode"].is<String>())
    return generateErrorJSON(output, "'wifi_mode' is a required parameter");
  if (!input["wifi_ssid"].is<String>())
    return generateErrorJSON(output, "'wifi_ssid' is a required parameter");
  if (!input["wifi_pass"].is<String>())
    return generateErrorJSON(output, "'wifi_pass' is a required parameter");
  if (!input["local_hostname"].is<String>())
    return generateErrorJSON(output, "'local_hostname' is a required parameter");

  // is it too long?
  if (strlen(input["wifi_ssid"]) > YB_WIFI_SSID_LENGTH - 1) {
    sprintf(error, "Maximum wifi ssid length is %s characters.", YB_WIFI_SSID_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  if (strlen(input["wifi_pass"]) > YB_WIFI_PASSWORD_LENGTH - 1) {
    sprintf(error, "Maximum wifi password length is %s characters.", YB_WIFI_PASSWORD_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  if (strlen(input["local_hostname"]) > YB_HOSTNAME_LENGTH - 1) {
    sprintf(error, "Maximum hostname length is %s characters.", YB_HOSTNAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // get our data
  char new_wifi_mode[16];
  char new_wifi_ssid[YB_WIFI_SSID_LENGTH];
  char new_wifi_pass[YB_WIFI_PASSWORD_LENGTH];

  strlcpy(new_wifi_mode, input["wifi_mode"] | YB_DEFAULT_AP_MODE, sizeof(new_wifi_mode));
  strlcpy(new_wifi_ssid, input["wifi_ssid"] | YB_DEFAULT_AP_SSID, sizeof(new_wifi_ssid));
  strlcpy(new_wifi_pass, input["wifi_pass"] | YB_DEFAULT_AP_PASS, sizeof(new_wifi_pass));
  strlcpy(_cfg.local_hostname, input["local_hostname"] | _app.default_hostname, sizeof(_cfg.local_hostname));

  // make sure we can connect before we save
  if (!strcmp(new_wifi_mode, "client")) {
    // did we change username/password?
    if (strcmp(new_wifi_ssid, _cfg.wifi_ssid) || strcmp(new_wifi_pass, _cfg.wifi_pass)) {
      // try connecting.
      YBP.printf("Trying new wifi %s / %s\n", new_wifi_ssid, new_wifi_pass);
      if (_app.network.connectToWifi(new_wifi_ssid, new_wifi_pass)) {
        // changing modes?
        if (!strcmp(_cfg.wifi_mode, "ap"))
          WiFi.softAPdisconnect();

        // save for local use
        strlcpy(_cfg.wifi_mode, new_wifi_mode, sizeof(_cfg.wifi_mode));
        strlcpy(_cfg.wifi_ssid, new_wifi_ssid, sizeof(_cfg.wifi_ssid));
        strlcpy(_cfg.wifi_pass, new_wifi_pass, sizeof(_cfg.wifi_pass));

        // save it to file.
        if (!_cfg.saveConfig(error, sizeof(error)))
          return generateErrorJSON(output, error);
      }
      // nope, setup our wifi back to default.
      else {
        _app.network.connectToWifi(_cfg.wifi_ssid, _cfg.wifi_pass); // go back to our old wifi.
        _app.network.startServices();
        return generateErrorJSON(output, "Can't connect to new WiFi.");
      }
    } else {
      // save it to file.
      if (!_cfg.saveConfig(error, sizeof(error)))
        return generateErrorJSON(output, error);
    }
  }
  // okay, AP mode is easier
  else {
    // save for local use.
    strlcpy(_cfg.wifi_mode, new_wifi_mode, sizeof(_cfg.wifi_mode));
    strlcpy(_cfg.wifi_ssid, new_wifi_ssid, sizeof(_cfg.wifi_ssid));
    strlcpy(_cfg.wifi_pass, new_wifi_pass, sizeof(_cfg.wifi_pass));

    // switch us into AP mode
    _app.network.setupWifi();

    if (!_cfg.saveConfig(error, sizeof(error)))
      return generateErrorJSON(output, error);

    return generateSuccessJSON(output, "AP mode successful, please connect to new network.");
  }
}

void ProtocolController::handleSetAuthenticationConfig(JsonVariantConst input, JsonVariant output)
{
  if (!input["admin_user"].is<String>())
    return generateErrorJSON(output, "'admin_user' is a required parameter");
  if (!input["admin_pass"].is<String>())
    return generateErrorJSON(output, "'admin_pass' is a required parameter");
  if (!input["guest_user"].is<String>())
    return generateErrorJSON(output, "'guest_user' is a required parameter");
  if (!input["guest_pass"].is<String>())
    return generateErrorJSON(output, "'guest_pass' is a required parameter");
  if (!input["default_role"].is<String>())
    return generateErrorJSON(output, "'default_role' is a required parameter");

  // username length checker
  if (strlen(input["admin_user"]) > YB_USERNAME_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum admin username length is %s characters.", YB_USERNAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // password length checker
  if (strlen(input["admin_pass"]) > YB_PASSWORD_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum admin password length is %s characters.", YB_PASSWORD_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // username length checker
  if (strlen(input["guest_user"]) > YB_USERNAME_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum guest username length is %s characters.", YB_USERNAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // password length checker
  if (strlen(input["guest_pass"]) > YB_PASSWORD_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum guest password length is %s characters.", YB_PASSWORD_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // get our data
  strlcpy(_cfg.admin_user, input["admin_user"] | _app.default_admin_user, sizeof(_cfg.admin_user));
  strlcpy(_cfg.admin_pass, input["admin_pass"] | _app.default_admin_pass, sizeof(_cfg.admin_pass));
  strlcpy(_cfg.guest_user, input["guest_user"] | _app.default_guest_user, sizeof(_cfg.guest_user));
  strlcpy(_cfg.guest_pass, input["guest_pass"] | _app.default_guest_pass, sizeof(_cfg.guest_pass));

  if (input["default_role"]) {
    if (!strcmp(input["default_role"], "admin"))
      _cfg.app_default_role = ADMIN;
    else if (!strcmp(input["default_role"], "guest"))
      _cfg.app_default_role = GUEST;
    else
      _cfg.app_default_role = NOBODY;
  }

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
}

void ProtocolController::handleSetWebServerConfig(JsonVariantConst input, JsonVariant output)
{
  bool old_app_enable_ssl = _cfg.app_enable_ssl;

  _cfg.app_enable_mfd = input["app_enable_mfd"] | _app.enable_mfd;
  _cfg.app_enable_api = input["app_enable_api"] | _app.enable_http_api;
  _cfg.app_enable_ssl = input["app_enable_ssl"] | _cfg.app_enable_ssl;
  _cfg.server_cert = input["server_cert"] | "";
  _cfg.server_key = input["server_key"] | "";

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // restart the board.
  if (old_app_enable_ssl != _cfg.app_enable_ssl)
    ESP.restart();
}

void ProtocolController::handleSetMQTTConfig(JsonVariantConst input, JsonVariant output)
{
  _cfg.app_enable_mqtt = input["app_enable_mqtt"];
  _cfg.app_enable_ha_integration = input["app_enable_ha_integration"];
  _cfg.app_use_hostname_as_mqtt_uuid = input["app_use_hostname_as_mqtt_uuid"];

  strlcpy(_cfg.mqtt_server, input["mqtt_server"] | "", sizeof(_cfg.mqtt_server));
  strlcpy(_cfg.mqtt_user, input["mqtt_user"] | "", sizeof(_cfg.mqtt_user));
  strlcpy(_cfg.mqtt_pass, input["mqtt_pass"] | "", sizeof(_cfg.mqtt_pass));
  _cfg.mqtt_cert = input["mqtt_cert"].as<String>();

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // init our mqtt
  if (_cfg.app_enable_mqtt)
    _app.mqtt.setup();
  else
    _app.mqtt.disconnect();
}

void ProtocolController::handleSetMiscellaneousConfig(JsonVariantConst input, JsonVariant output)
{
  _cfg.app_enable_serial = input["app_enable_serial"] | _app.enable_serial_api;
  _cfg.app_enable_ota = input["app_enable_ota"] | _app.enable_arduino_ota;

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // init our ota.
  if (_cfg.app_enable_ota)
    _app.ota.setup();
  else
    _app.ota.end();
}

void ProtocolController::handleSaveConfig(JsonVariantConst input, JsonVariant output)
{
  char error[128] = "Unknown";

  // get the config object specifically.
  JsonDocument cfg;

  // we need one thing...
  if (!input["config"].is<String>())
    return generateErrorJSON(output, "'config' is a required parameter");

  // was there a problem, officer?
  DeserializationError err = deserializeJson(cfg, input["config"]);
  if (err) {
    snprintf(error, sizeof(error), "deserializeJson() failed with code %s", err.c_str());
    return generateErrorJSON(output, error);
  }

  // test the validity by loading it...
  if (!_cfg.loadConfigFromJSON(cfg, error, sizeof(error)))
    return generateErrorJSON(output, error);

  // write it!
  if (!_cfg.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // restart the board.
  ESP.restart();
}

void ProtocolController::handleLogin(JsonVariantConst input, JsonVariant output, YBMode mode, PsychicWebSocketClient* connection)
{
  if (!input["user"].is<String>())
    return generateErrorJSON(output, "'user' is a required parameter");

  if (!input["pass"].is<String>())
    return generateErrorJSON(output, "'pass' is a required parameter");

  // init
  char myuser[YB_USERNAME_LENGTH];
  char mypass[YB_PASSWORD_LENGTH];
  strlcpy(myuser, input["user"] | "", sizeof(myuser));
  strlcpy(mypass, input["pass"] | "", sizeof(mypass));

  // check their credentials
  bool is_authenticated = false;
  UserRole role = _cfg.app_default_role;

  if (!strcmp(_cfg.admin_user, myuser) && !strcmp(_cfg.admin_pass, mypass)) {
    is_authenticated = true;
    role = ADMIN;
    output["role"] = "admin";
  }

  if (!strcmp(_cfg.guest_user, myuser) && !strcmp(_cfg.guest_pass, mypass)) {
    is_authenticated = true;
    role = GUEST;
    output["role"] = "guest";
  }

  // okay, are we in?
  if (is_authenticated) {
    // check to see if there's room for us.
    if (mode == YBP_MODE_WEBSOCKET) {
      if (!_app.auth.logClientIn(connection->socket(), role))
        return generateErrorJSON(output, "Too many connections.");
    } else if (mode == YBP_MODE_SERIAL) {
      is_serial_authenticated = true;
      _cfg.serial_role = role;
    }

    output["msg"] = "login";
    output["role"] = getRoleText(role);
    output["message"] = "Login successful.";

    return;
  }

  // gtfo.
  return generateErrorJSON(output, "Wrong username/password.");
}

void ProtocolController::handleLogout(JsonVariantConst input, JsonVariant output, YBMode mode,
  PsychicWebSocketClient* connection)
{
  if (!_app.auth.isLoggedIn(input, mode, connection->socket()))
    return generateErrorJSON(output, "You are not logged in.");

  // what type of client are you?
  if (mode == YBP_MODE_WEBSOCKET) {
    _app.auth.removeClientFromAuthList(connection->socket());
  } else if (mode == YBP_MODE_SERIAL) {
    is_serial_authenticated = false;
    _cfg.serial_role = _cfg.app_default_role;
  }
}

void ProtocolController::handleRestart(JsonVariantConst input, JsonVariant output)
{
  YBP.println("Restarting board.");

  ESP.restart();
}

void ProtocolController::handleCrashMe(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_IS_DEVELOPMENT
  crash_me_hard();
#endif
}

void ProtocolController::handleFactoryReset(JsonVariantConst input, JsonVariant output)
{
  // delete all our prefs
  _cfg.preferences.clear();
  _cfg.preferences.end();

  // clean up littlefs
  LittleFS.format();

  // restart the board.
  ESP.restart();
}

void ProtocolController::handleOTAStart(JsonVariantConst input, JsonVariant output)
{
  if (_app.ota.checkOTA())
    _app.ota.startOTA();
  else
    return generateErrorJSON(output, "Firmware already up to date.");
}

void ProtocolController::handleSetTheme(JsonVariantConst input, JsonVariant output)
{
  if (!input["theme"].is<String>())
    return generateErrorJSON(output, "'theme' is a required parameter");

  String temp = input["theme"];

  if (temp != "light" && temp != "dark")
    return generateErrorJSON(output,
      "'theme' must either be 'light' or 'dark'");

  _cfg.app_theme = temp;

  sendThemeUpdate();
}

void ProtocolController::handleSetBrightness(JsonVariantConst input, JsonVariant output)
{
  if (input["brightness"].is<float>()) {
    float brightness = input["brightness"];

    // what do we hate?  va-li-date!
    if (brightness < 0)
      return generateErrorJSON(output, "Brightness must be >= 0");
    else if (brightness > 1)
      return generateErrorJSON(output, "Brightness must be <= 1");

    _cfg.globalBrightness = brightness;

    // TODO: need to put this on a time delay
    // preferences.putFloat("brightness", globalBrightness);

    sendBrightnessUpdate();
  } else
    return generateErrorJSON(output, "'brightness' is a required parameter.");
}

#ifdef YB_IS_BRINEOMATIC
void ProtocolController::handleStartWatermaker(JsonVariantConst input, JsonVariant output)
{
  if (strcmp(wm.getStatus(), "IDLE"))
    return generateErrorJSON(output, "Watermaker is not in IDLE mode.");

  uint64_t duration = input["duration"];
  float volume = input["volume"];

  if (duration > 0)
    wm.startDuration(duration);
  else if (volume > 0)
    wm.startVolume(volume);
  else
    wm.start();
}

void ProtocolController::handleFlushWatermaker(JsonVariantConst input, JsonVariant output)
{
  uint64_t duration = input["duration"];
  float volume = input["volume"];

  if (!strcmp(wm.getStatus(), "IDLE") || !strcmp(wm.getStatus(), "PICKLED")) {
    if (duration > 0)
      wm.flushDuration(duration);
    else if (volume > 0)
      wm.flushVolume(volume);
    else
      wm.flush();
  } else
    return generateErrorJSON(output, "Watermaker is not in IDLE or PICKLED modes.");
}

void ProtocolController::handlePickleWatermaker(JsonVariantConst input, JsonVariant output)
{
  if (!input["duration"].is<JsonVariantConst>())
    return generateErrorJSON(output, "'duration' is a required parameter");

  uint64_t duration = input["duration"];

  if (!duration)
    return generateErrorJSON(output, "'duration' must be non-zero");

  if (!strcmp(wm.getStatus(), "IDLE"))
    wm.pickle(duration);
  else
    return generateErrorJSON(output, "Watermaker is not in IDLE mode.");
}

void ProtocolController::handleDepickleWatermaker(JsonVariantConst input, JsonVariant output)
{
  if (!input["duration"].is<JsonVariantConst>())
    return generateErrorJSON(output, "'duration' is a required parameter");

  uint64_t duration = input["duration"];

  if (!duration)
    return generateErrorJSON(output, "'duration' must be non-zero");

  if (!strcmp(wm.getStatus(), "PICKLED"))
    wm.depickle(duration);
  else
    return generateErrorJSON(output, "Watermaker is not in PICKLED mode.");
}

void ProtocolController::handleStopWatermaker(JsonVariantConst input, JsonVariant output)
{
  if (!strcmp(wm.getStatus(), "RUNNING") || !strcmp(wm.getStatus(), "FLUSHING") || !strcmp(wm.getStatus(), "PICKLING") || !strcmp(wm.getStatus(), "DEPICKLING"))
    wm.stop();
  else
    return generateErrorJSON(output, "Watermaker must be in RUNNING, FLUSHING, or PICKLING mode to stop.");
}

void ProtocolController::handleIdleWatermaker(JsonVariantConst input, JsonVariant output)
{
  if (!strcmp(wm.getStatus(), "MANUAL"))
    wm.idle();
  else
    return generateErrorJSON(output, "Watermaker must be in MANUAL mode to IDLE.");
}

void ProtocolController::handleManualWatermaker(JsonVariantConst input, JsonVariant output)
{
  if (!strcmp(wm.getStatus(), "IDLE"))
    wm.manual();
  else
    return generateErrorJSON(output, "Watermaker must be in IDLE mode to switch to MANUAL.");
}

void ProtocolController::handleSetWatermaker(JsonVariantConst input, JsonVariant output)
{
  if (input["water_temperature"]) {
    float temp = input["water_temperature"];
    wm.setWaterTemperature(temp);
    return;
  }

  if (input["tank_level"]) {
    float level = input["tank_level"];
    wm.setTankLevel(level);
    return;
  }

  if (strcmp(wm.getStatus(), "MANUAL"))
    return generateErrorJSON(output, "Watermaker must be in MANUAL mode.");

  String state;

  if (input["boost_pump"]) {
    if (wm.hasBoostPump()) {
      state = input["boost_pump"] | "OFF";

      if (state.equals("TOGGLE")) {
        if (!wm.isBoostPumpOn())
          state = "ON";
      }

      if (state.equals("ON"))
        wm.enableBoostPump();
      else
        wm.disableBoostPump();
    } else
      return generateErrorJSON(output, "Watermaker does not have a boost pump.");
  }

  if (input["high_pressure_pump"]) {
    if (wm.hasHighPressurePump()) {
      state = input["high_pressure_pump"] | "OFF";

      if (state.equals("TOGGLE")) {
        if (!wm.isHighPressurePumpOn())
          state = "ON";
      }

      if (state.equals("ON"))
        wm.enableHighPressurePump();
      else
        wm.disableHighPressurePump();
    } else
      return generateErrorJSON(output, "Watermaker does not have a high pressure pump.");
  }

  if (input["diverter_valve"]) {
    if (wm.hasDiverterValve()) {
      state = input["diverter_valve"] | "CLOSE";

      if (state.equals("TOGGLE")) {
        if (!wm.isDiverterValveOpen())
          state = "OPEN";
      }

      if (state.equals("OPEN"))
        wm.openDiverterValve();
      else
        wm.closeDiverterValve();
    } else
      return generateErrorJSON(output, "Watermaker does not have a diverter valve.");
  }

  if (input["flush_valve"]) {
    if (wm.hasFlushValve()) {
      state = input["flush_valve"] | "CLOSE";

      if (state.equals("TOGGLE")) {
        if (!wm.isFlushValveOpen())
          state = "OPEN";
      }

      if (state.equals("OPEN"))
        wm.openFlushValve();
      else
        wm.closeFlushValve();
    } else
      return generateErrorJSON(output, "Watermaker does not have a flush valve.");
  }

  if (input["cooling_fan"]) {
    if (wm.hasCoolingFan()) {
      state = input["cooling_fan"] | "ON";

      if (state.equals("TOGGLE")) {
        if (!wm.isCoolingFanOn())
          state = "ON";
      }

      if (state.equals("ON"))
        wm.enableCoolingFan();
      else
        wm.disableCoolingFan();
    } else
      return generateErrorJSON(output, "Watermaker does not have a cooling fan.");
  }
}

void ProtocolController::handleBrineomaticSaveGeneralConfig(JsonVariantConst input, JsonVariant output)
{
  // we need a mutable format for the validation
  JsonDocument doc;
  doc.set(input);

  char error[128];
  if (!wm.validateGeneralConfigJSON(doc, error, sizeof(error)))
    return generateErrorJSON(output, error);

  wm.loadGeneralConfigJSON(doc);

  if (!saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
}

void ProtocolController::handleBrineomaticSaveHardwareConfig(JsonVariantConst input, JsonVariant output)
{
  // we need a mutable format for the validation
  JsonDocument doc;
  doc.set(input);

  if (strcmp(wm.getStatus(), "IDLE"))
    return generateErrorJSON(output, "Must be in IDLE mode to update hardware config.");

  char error[128];
  if (!wm.validateHardwareConfigJSON(doc, error, sizeof(error)))
    return generateErrorJSON(output, error);

  wm.loadHardwareConfigJSON(doc);

  if (!saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // easiest to just restart - lots of init.
  ESP.restart();
}

void ProtocolController::handleBrineomaticSaveSafeguardsConfig(JsonVariantConst input, JsonVariant output)
{
  // we need a mutable format for the validation
  JsonDocument doc;
  doc.set(input);

  char error[128];
  if (!wm.validateSafeguardsConfigJSON(doc, error, sizeof(error))) {
    return generateErrorJSON(output, error);
  }

  wm.loadSafeguardsConfigJSON(doc);

  if (!saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
}

#endif

void ProtocolController::generateFullConfigMessage(JsonVariant output)
{
  // build our message
  output["msg"] = "full_config";
  JsonObject cfg = output["config"].to<JsonObject>();

  // separate call to make a clean config.
  _cfg.generateFullConfig(cfg);
}

void ProtocolController::generateConfigJSON(JsonVariant output)
{
  // extra info
  output["msg"] = "config";
  output["hostname"] = _cfg.local_hostname;
  output["use_ssl"] = _cfg.app_enable_ssl;
  output["enable_ota"] = _cfg.app_enable_ota;
  output["enable_mqtt"] = _cfg.app_enable_mqtt;
  output["default_role"] = getRoleText(_cfg.app_default_role);
  output["brightness"] = _cfg.globalBrightness;
  output["git_hash"] = GIT_HASH;
  output["build_time"] = BUILD_TIME;
  output["firmware_manifest_url"] = _app.ota.firmware_manifest_url;

  _cfg.generateBoardConfig(output);

  output["is_development"] = YB_IS_DEVELOPMENT;

  // some debug info
  output["last_restart_reason"] = getResetReason();
  if (has_coredump)
    output["has_coredump"] = has_coredump;
  output["boot_log"] = startupLogger.c_str();

  // do we want to flag it for config?
  if (_cfg.is_first_boot)
    output["first_boot"] = true;
}

void ProtocolController::generateUpdateJSON(JsonVariant output)
{
  output["msg"] = "update";
  output["uptime"] = esp_timer_get_time();

  for (auto& c : _app.getControllers()) {
    c->generateUpdateHook(output);
  }

  // #ifdef YB_IS_BRINEOMATIC
  //   wm.generateUpdateJSON(output);
  // #endif
}

void ProtocolController::generateFastUpdateJSON(JsonVariant output)
{
  output["msg"] = "update";
  output["fast"] = 1;
  output["uptime"] = esp_timer_get_time();

  for (auto& c : _app.getControllers()) {
    c->generateFastUpdateHook(output);
  }
}

void ProtocolController::generateStatsJSON(JsonVariant output)
{
  // some basic statistics and info
  output["msg"] = "stats";
  output["uuid"] = _cfg.uuid;
  output["received_message_total"] = totalReceivedMessages;
  output["received_message_mps"] = receivedMessagesPerSecond;
  output["sent_message_total"] = totalSentMessages;
  output["sent_message_mps"] = sentMessagesPerSecond;
  output["websocket_client_count"] = _app.http.websocketClientCount;
  output["http_client_count"] = _app.http.httpClientCount - _app.http.websocketClientCount;
  output["fps"] = (int)_app.framerate;
  output["uptime"] = esp_timer_get_time();
  output["heap_size"] = ESP.getHeapSize();
  output["free_heap"] = ESP.getFreeHeap();
  output["min_free_heap"] = ESP.getMinFreeHeap();
  output["max_alloc_heap"] = ESP.getMaxAllocHeap();
  output["rssi"] = WiFi.RSSI();
  if (_cfg.app_enable_mqtt)
    output["mqtt_connected"] = _app.mqtt.isConnected();

  // what is our IP address?
  if (!strcmp(_cfg.wifi_mode, "ap"))
    output["ip_address"] = _app.network.apIP;
  else
    output["ip_address"] = WiFi.localIP();

  for (auto& c : _app.getControllers()) {
    c->generateStatsHook(output);
  }

#ifdef YB_IS_BRINEOMATIC
  output["brineomatic"] = true;
  output["total_cycles"] = wm.getTotalCycles();
  output["total_volume"] = wm.getTotalVolume();
  output["total_runtime"] = wm.getTotalRuntime();
#endif
}

void ProtocolController::generateNetworkConfigMessage(JsonVariant output)
{
  // our identifying info
  output["msg"] = "network_config";
  _cfg.generateNetworkConfig(output);
}

void ProtocolController::generateAppConfigMessage(JsonVariant output)
{
  // our identifying info
  output["msg"] = "app_config";
  _cfg.generateAppConfig(output);
}

void ProtocolController::generateOTAProgressUpdateJSON(JsonVariant output, float progress)
{
  output["msg"] = "ota_progress";
  output["progress"] = round2(progress);
}

void ProtocolController::generateOTAProgressFinishedJSON(JsonVariant output)
{
  output["msg"] = "ota_finished";
}

void ProtocolController::generateErrorJSON(JsonVariant output, const char* error)
{
  output["msg"] = "status";
  output["status"] = "error";
  output["message"] = error;
}

void ProtocolController::generateSuccessJSON(JsonVariant output, const char* success)
{
  output["msg"] = "status";
  output["status"] = "success";
  output["message"] = success;
}

void ProtocolController::generateLoginRequiredJSON(JsonVariant output)
{
  generateErrorJSON(output, "You must be logged in.");
}

void ProtocolController::handlePing(JsonVariantConst input, JsonVariant output)
{
  output["pong"] = millis();
}

void ProtocolController::sendThemeUpdate()
{
  JsonDocument output;
  output["msg"] = "set_theme";
  output["theme"] = _cfg.app_theme;

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(output);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(output, jsonBuffer, jsonSize + 1);
    sendToAll(jsonBuffer, NOBODY);
    free(jsonBuffer);
  } else {
    YBP.println("sendThemeUpdate() malloc failed.");
  }
}

void ProtocolController::sendBrightnessUpdate()
{
  JsonDocument output;
  output["msg"] = "set_brightness";
  output["brightness"] = _cfg.globalBrightness;

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(output);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(output, jsonBuffer, jsonSize + 1);
    sendToAll(jsonBuffer, NOBODY);
    free(jsonBuffer);
  } else {
    YBP.println("sendBrightnessUpdate() malloc failed.");
  }
}

void ProtocolController::sendFastUpdate()
{
  JsonDocument output;
  generateFastUpdateJSON(output);

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(output);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(output, jsonBuffer, jsonSize + 1);
    sendToAll(jsonBuffer, GUEST);
    free(jsonBuffer);
  } else {
    YBP.println("sendFastUpdate() malloc failed.");
  }
}

void ProtocolController::sendOTAProgressUpdate(float progress)
{
  JsonDocument output;
  generateOTAProgressUpdateJSON(output, progress);

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(output);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(output, jsonBuffer, jsonSize + 1);
    sendToAll(jsonBuffer, GUEST);
    free(jsonBuffer);
  } else {
    YBP.println("sendOTAProgressUpdate() malloc failed.");
  }
}

void ProtocolController::sendOTAProgressFinished()
{
  JsonDocument output;
  generateOTAProgressFinishedJSON(output);

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(output);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(output, jsonBuffer, jsonSize + 1);
    sendToAll(jsonBuffer, GUEST);
    free(jsonBuffer);
  } else {
    YBP.println("sendOTAProgressFinished() malloc failed.");
  }
}

void ProtocolController::sendDebug(const char* message)
{
  JsonDocument output;
  output["debug"] = message;

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(output);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(output, jsonBuffer, jsonSize + 1);
    sendToAll(jsonBuffer, NOBODY);
    free(jsonBuffer);
  } else {
    // dont call YBP b/c loops...
    Serial.println("Error allocating in sendDebug()");
  }
}

void ProtocolController::sendToAll(const char* jsonString, UserRole auth_level)
{
  _app.http.sendToAllWebsockets(jsonString, auth_level);

  if (_cfg.app_enable_serial && _cfg.serial_role >= auth_level)
    Serial.println(jsonString);
}
