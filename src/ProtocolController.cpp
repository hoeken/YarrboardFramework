/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "ProtocolController.h"
#include "ConfigManager.h"
#include "MQTTController.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"
#include "utility.h"

#ifdef YB_HAS_ADC_CHANNELS
  #include "adc_channel.h"
#endif

#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
  #include "digital_input_channel.h"
#endif

#ifdef YB_HAS_PWM_CHANNELS
  #include "pwm_channel.h"
#endif

#ifdef YB_HAS_RELAY_CHANNELS
  #include "relay_channel.h"
#endif

#ifdef YB_HAS_SERVO_CHANNELS
  #include "servo_channel.h"
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
  #include "stepper_channel.h"
#endif

#ifdef YB_HAS_PIEZO
  #include "piezo.h"
#endif

ProtocolController::ProtocolController(YarrboardApp& app, ConfigManager& config) : _app(app),
                                                                                   _config(config)
{
}

//
//
//.        OLD CODE BELOW.
//
//

void ProtocolController::setup()
{
  // send serial a config off the bat
  if (_config.app_enable_serial) {
    JsonDocument output;
    generateConfigJSON(output);
    serializeJson(output, Serial);
  }
}

void ProtocolController::loop()
{
  // lookup our info periodically
  unsigned int messageDelta = millis() - previousMessageMillis;
  if (messageDelta >= 1000) {
// TODO: move this to pwm loop
#ifdef YB_HAS_PWM_CHANNELS
    // update our averages, etc.
    for (auto& ch : pwm_channels)
      ch.calculateAverages(messageDelta);
#endif

    // for keeping track.
    receivedMessagesPerSecond = receivedMessages;
    receivedMessages = 0;
    sentMessagesPerSecond = sentMessages;
    sentMessages = 0;

    previousMessageMillis = millis();
  }

  // any serial port customers?
  if (_config.app_enable_serial) {
    if (Serial.available() > 0)
      handleSerialJson();
  }
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
  UserRole role = _app.http.getUserRole(input, mode, connection);

  // only pages with no login requirements
  if (!strcmp(cmd, "login"))
    return handleLogin(input, output, mode, connection);
  else if (!strcmp(cmd, "ping"))
    return generatePongJSON(output);
  else if (!strcmp(cmd, "hello"))
    return generateHelloJSON(output, role);

  // TODO: we don't actually need to require login
  // need to be logged in from here on out.
  //  if (!isLoggedIn(input, mode, connection))
  //      return generateLoginRequiredJSON(output);

  // commands for using the boards
  if (role == GUEST || role == ADMIN) {
    if (!strcmp(cmd, "get_config"))
      return generateConfigJSON(output);
    else if (!strcmp(cmd, "get_stats"))
      return generateStatsJSON(output);
    else if (!strcmp(cmd, "get_update"))
      return generateUpdateJSON(output);
    else if (!strcmp(cmd, "play_sound"))
      return handlePlaySound(input, output);
    else if (!strcmp(cmd, "set_pwm_channel"))
      return handleSetPWMChannel(input, output);
    else if (!strcmp(cmd, "toggle_pwm_channel"))
      return handleTogglePWMChannel(input, output);
    else if (!strcmp(cmd, "set_relay_channel"))
      return handleSetRelayChannel(input, output);
    else if (!strcmp(cmd, "toggle_relay_channel"))
      return handleToggleRelayChannel(input, output);
    else if (!strcmp(cmd, "set_servo_channel"))
      return handleSetServoChannel(input, output);
    else if (!strcmp(cmd, "set_stepper_channel"))
      return handleSetStepperChannel(input, output);
    else if (!strcmp(cmd, "set_theme"))
      return handleSetTheme(input, output);
    else if (!strcmp(cmd, "set_brightness"))
      return handleSetBrightness(input, output);
#ifdef YB_IS_BRINEOMATIC
    else if (!strcmp(cmd, "start_watermaker"))
      return handleStartWatermaker(input, output);
    else if (!strcmp(cmd, "flush_watermaker"))
      return handleFlushWatermaker(input, output);
    else if (!strcmp(cmd, "pickle_watermaker"))
      return handlePickleWatermaker(input, output);
    else if (!strcmp(cmd, "depickle_watermaker"))
      return handleDepickleWatermaker(input, output);
    else if (!strcmp(cmd, "stop_watermaker"))
      return handleStopWatermaker(input, output);
    else if (!strcmp(cmd, "idle_watermaker"))
      return handleIdleWatermaker(input, output);
    else if (!strcmp(cmd, "manual_watermaker"))
      return handleManualWatermaker(input, output);
    else if (!strcmp(cmd, "set_watermaker"))
      return handleSetWatermaker(input, output);
    else if (!strcmp(cmd, "brineomatic_save_general_config"))
      return handleBrineomaticSaveGeneralConfig(input, output);
    else if (!strcmp(cmd, "brineomatic_save_hardware_config"))
      return handleBrineomaticSaveHardwareConfig(input, output);
    else if (!strcmp(cmd, "brineomatic_save_safeguards_config"))
      return handleBrineomaticSaveSafeguardsConfig(input, output);
#endif
    else if (!strcmp(cmd, "logout"))
      return handleLogout(input, output, mode, connection);
  }

  // commands for changing settings
  if (role == ADMIN) {
    if (!strcmp(cmd, "set_general_config"))
      return handleSetGeneralConfig(input, output);
    else if (!strcmp(cmd, "save_config"))
      return handleSaveConfig(input, output);
    else if (!strcmp(cmd, "get_full_config"))
      return generateFullConfigMessage(output);
    else if (!strcmp(cmd, "get_network_config"))
      return generateNetworkConfigMessage(output);
    else if (!strcmp(cmd, "set_network_config"))
      return handleSetNetworkConfig(input, output);
    else if (!strcmp(cmd, "get_app_config"))
      return generateAppConfigMessage(output);
    else if (!strcmp(cmd, "set_authentication_config"))
      return handleSetAuthenticationConfig(input, output);
    else if (!strcmp(cmd, "set_webserver_config"))
      return handleSetWebServerConfig(input, output);
    else if (!strcmp(cmd, "set_mqtt_config"))
      return handleSetMQTTConfig(input, output);
    else if (!strcmp(cmd, "set_misc_config"))
      return handleSetMiscellaneousConfig(input, output);
    else if (!strcmp(cmd, "restart"))
      return handleRestart(input, output);
    else if (!strcmp(cmd, "crashme"))
      return handleCrashMe(input, output);
    else if (!strcmp(cmd, "factory_reset"))
      return handleFactoryReset(input, output);
    else if (!strcmp(cmd, "ota_start"))
      return handleOTAStart(input, output);
    else if (!strcmp(cmd, "config_pwm_channel"))
      return handleConfigPWMChannel(input, output);
    else if (!strcmp(cmd, "config_relay_channel"))
      return handleConfigRelayChannel(input, output);
    else if (!strcmp(cmd, "config_servo_channel"))
      return handleConfigServoChannel(input, output);
    else if (!strcmp(cmd, "config_stepper_channel"))
      return handleConfigStepperChannel(input, output);
    else if (!strcmp(cmd, "config_adc"))
      return handleConfigADC(input, output);
  }

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
  output["default_role"] = getRoleText(_config.app_default_role);
  output["name"] = _config.board_name;
  output["brightness"] = _config.globalBrightness;
  output["firmware_version"] = YB_FIRMWARE_VERSION;
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
  strlcpy(_config.board_name, input["board_name"] | YB_BOARD_NAME, sizeof(_config.board_name));
  strlcpy(_config.startup_melody, input["startup_melody"] | YB_PIEZO_DEFAULT_MELODY, sizeof(_config.startup_melody));

  // save it to file.
  char error[128];
  if (!_config.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // give them the updated config
  generateConfigJSON(output);
}

void ProtocolController::handleSetNetworkConfig(JsonVariantConst input, JsonVariant output)
{
  // clear our first boot flag since they submitted the network page.
  _config.is_first_boot = false;

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
  strlcpy(_config.local_hostname, input["local_hostname"] | YB_DEFAULT_HOSTNAME, sizeof(_config.local_hostname));

  // make sure we can connect before we save
  if (!strcmp(new_wifi_mode, "client")) {
    // did we change username/password?
    if (strcmp(new_wifi_ssid, _config.wifi_ssid) || strcmp(new_wifi_pass, _config.wifi_pass)) {
      // try connecting.
      YBP.printf("Trying new wifi %s / %s\n", new_wifi_ssid, new_wifi_pass);
      if (_app.network.connectToWifi(new_wifi_ssid, new_wifi_pass)) {
        // changing modes?
        if (!strcmp(_config.wifi_mode, "ap"))
          WiFi.softAPdisconnect();

        // save for local use
        strlcpy(_config.wifi_mode, new_wifi_mode, sizeof(_config.wifi_mode));
        strlcpy(_config.wifi_ssid, new_wifi_ssid, sizeof(_config.wifi_ssid));
        strlcpy(_config.wifi_pass, new_wifi_pass, sizeof(_config.wifi_pass));

        // save it to file.
        if (!_config.saveConfig(error, sizeof(error)))
          return generateErrorJSON(output, error);
      }
      // nope, setup our wifi back to default.
      else {
        _app.network.connectToWifi(_config.wifi_ssid, _config.wifi_pass); // go back to our old wifi.
        _app.network.startServices();
        return generateErrorJSON(output, "Can't connect to new WiFi.");
      }
    } else {
      // save it to file.
      if (!_config.saveConfig(error, sizeof(error)))
        return generateErrorJSON(output, error);
    }
  }
  // okay, AP mode is easier
  else {
    // save for local use.
    strlcpy(_config.wifi_mode, new_wifi_mode, sizeof(_config.wifi_mode));
    strlcpy(_config.wifi_ssid, new_wifi_ssid, sizeof(_config.wifi_ssid));
    strlcpy(_config.wifi_pass, new_wifi_pass, sizeof(_config.wifi_pass));

    // switch us into AP mode
    _app.network.setupWifi();

    if (!_config.saveConfig(error, sizeof(error)))
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
  strlcpy(_config.admin_user, input["admin_user"] | YB_DEFAULT_ADMIN_USER, sizeof(_config.admin_user));
  strlcpy(_config.admin_pass, input["admin_pass"] | YB_DEFAULT_ADMIN_PASS, sizeof(_config.admin_pass));
  strlcpy(_config.guest_user, input["guest_user"] | YB_DEFAULT_GUEST_USER, sizeof(_config.guest_user));
  strlcpy(_config.guest_pass, input["guest_pass"] | YB_DEFAULT_GUEST_PASS, sizeof(_config.guest_pass));

  if (input["default_role"]) {
    if (!strcmp(input["default_role"], "admin"))
      _config.app_default_role = ADMIN;
    else if (!strcmp(input["default_role"], "guest"))
      _config.app_default_role = GUEST;
    else
      _config.app_default_role = NOBODY;
  }

  // save it to file.
  char error[128] = "Unknown";
  if (!_config.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
}

void ProtocolController::handleSetWebServerConfig(JsonVariantConst input, JsonVariant output)
{
  bool old_app_enable_ssl = _config.app_enable_ssl;

  _config.app_enable_mfd = input["app_enable_mfd"] | YB_DEFAULT_APP_ENABLE_MFD;
  _config.app_enable_api = input["app_enable_api"] | YB_DEFAULT_APP_ENABLE_API;
  _config.app_enable_ssl = input["app_enable_ssl"] | _config.app_enable_ssl;
  _config.server_cert = input["server_cert"] | "";
  _config.server_key = input["server_key"] | "";

  // save it to file.
  char error[128] = "Unknown";
  if (!_config.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // restart the board.
  if (old_app_enable_ssl != _config.app_enable_ssl)
    ESP.restart();
}

void ProtocolController::handleSetMQTTConfig(JsonVariantConst input, JsonVariant output)
{
  _config.app_enable_mqtt = input["app_enable_mqtt"];
  _config.app_enable_ha_integration = input["app_enable_ha_integration"];
  _config.app_use_hostname_as_mqtt_uuid = input["app_use_hostname_as_mqtt_uuid"];

  strlcpy(_config.mqtt_server, input["mqtt_server"] | "", sizeof(_config.mqtt_server));
  strlcpy(_config.mqtt_user, input["mqtt_user"] | "", sizeof(_config.mqtt_user));
  strlcpy(_config.mqtt_pass, input["mqtt_pass"] | "", sizeof(_config.mqtt_pass));
  _config.mqtt_cert = input["mqtt_cert"].as<String>();

  // save it to file.
  char error[128] = "Unknown";
  if (!_config.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // init our mqtt
  if (_config.app_enable_mqtt)
    _app.mqtt.setup();
  else
    _app.mqtt.disconnect();
}

void ProtocolController::handleSetMiscellaneousConfig(JsonVariantConst input, JsonVariant output)
{
  _config.app_enable_serial = input["app_enable_serial"] | YB_DEFAULT_APP_ENABLE_SERIAL;
  _config.app_enable_ota = input["app_enable_ota"] | YB_DEFAULT_APP_ENABLE_OTA;

  // save it to file.
  char error[128] = "Unknown";
  if (!_config.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // init our ota.
  if (_config.app_enable_ota)
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
  if (!_config.loadConfigFromJSON(cfg, error, sizeof(error)))
    return generateErrorJSON(output, error);

  // write it!
  if (!_config.saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);

  // restart the board.
  ESP.restart();
}

void ProtocolController::handleLogin(JsonVariantConst input, JsonVariant output, YBMode mode,
  PsychicWebSocketClient* connection)
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
  UserRole role = _config.app_default_role;

  if (!strcmp(_config.admin_user, myuser) && !strcmp(_config.admin_pass, mypass)) {
    is_authenticated = true;
    role = ADMIN;
    output["role"] = "admin";
  }

  if (!strcmp(_config.guest_user, myuser) && !strcmp(_config.guest_pass, mypass)) {
    is_authenticated = true;
    role = GUEST;
    output["role"] = "guest";
  }

  // okay, are we in?
  if (is_authenticated) {
    // check to see if there's room for us.
    if (mode == YBP_MODE_WEBSOCKET) {
      if (!_app.http.logClientIn(connection, role))
        return generateErrorJSON(output, "Too many connections.");
    } else if (mode == YBP_MODE_SERIAL) {
      is_serial_authenticated = true;
      _config.serial_role = role;
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
  if (!_app.http.isLoggedIn(input, mode, connection))
    return generateErrorJSON(output, "You are not logged in.");

  // what type of client are you?
  if (mode == YBP_MODE_WEBSOCKET) {
    _app.http.removeClientFromAuthList(connection);

    // we don't actually want to close the connection, bad UI
    // connection->close();
  } else if (mode == YBP_MODE_SERIAL) {
    is_serial_authenticated = false;
    _config.serial_role = _config.app_default_role;
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
  _config.preferences.clear();
  _config.preferences.end();

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

void ProtocolController::handlePlaySound(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_PIEZO
  // expect: "melody": "name"
  if (input["melody"]) {
    const char* melody = input["melody"].as<const char*>();
    if (!melody)
      return generateErrorJSON(output, "'melody' must be a string");

    if (!strcmp(melody, "NONE"))
      return;

    if (playMelodyByName(melody))
      return;
    else
      return generateErrorJSON(output, "Unknown melody name");
  }

  // Expect: { "notes": [ { "freq": 440, "ms": 200 }, ... ] }
  if (!input["notes"])
    return generateErrorJSON(output, "Missing 'notes' array");

  JsonVariantConst notesVar = input["notes"];
  if (!notesVar.is<JsonArrayConst>())
    return generateErrorJSON(output, "'notes' must be an array");

  JsonArrayConst notes = notesVar.as<JsonArrayConst>();
  size_t count = notes.size();

  if (count == 0)
    return generateErrorJSON(output, "'notes' array is empty");

  if (count > YB_MAX_MELODY_LENGTH) {
    char error[128];
    snprintf(error, sizeof(error), "Max notes length of %d", YB_MAX_MELODY_LENGTH);
    return generateErrorJSON(output, error);
  }

  // Allocate dynamic Note sequence
  Note* seq = new Note[count];
  size_t idx = 0;

  for (JsonVariantConst nv : notes) {
    if (!nv["ms"]) {
      delete[] seq;
      return generateErrorJSON(output, "Each note must contain 'ms'");
    }

    // freq is optional (default = 0 meaning rest)
    uint16_t freq = nv["freq"] | 0;
    uint16_t ms = nv["ms"].as<uint16_t>();

    seq[idx++] = {freq, ms};
  }

  playMelody(seq, count);

  delete[] seq;
#else
  return generateErrorJSON(output, "Piezo not supported on this board");
#endif
}

void ProtocolController::handleSetPWMChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_PWM_CHANNELS

  // load our channel
  auto* ch = lookupChannel(input, output, pwm_channels);
  if (!ch)
    return;

  // is it enabled?
  if (!ch->isEnabled)
    return generateErrorJSON(output, "Channel is not enabled.");

  // our duty cycle
  if (input["duty"].is<float>()) {
    // is it enabled?
    if (!ch->isEnabled)
      return generateErrorJSON(output, "Channel is not enabled.");

    float duty = input["duty"];

    // what do we hate?  va-li-date!
    if (duty < 0)
      return generateErrorJSON(output, "Duty cycle must be >= 0");
    else if (duty > 1)
      return generateErrorJSON(output, "Duty cycle must be <= 1");

    // okay, we're good.
    ch->setDuty(duty);

    // change our output pin to reflect
    ch->updateOutput(true);
  }

  // change state
  if (input["state"].is<String>()) {
    // source is required
    if (!input["source"].is<String>())
      return generateErrorJSON(output, "'source' is a required parameter");

    // check the length
    char error[50];
    if (strlen(input["source"]) > YB_HOSTNAME_LENGTH - 1) {
      sprintf(error, "Maximum source length is %s characters.", YB_HOSTNAME_LENGTH - 1);
      return generateErrorJSON(output, error);
    }

    // get our data
    strlcpy(ch->source, input["source"] | local_hostname, sizeof(ch->source));

    // okay, set our state
    char state[10];
    strlcpy(state, input["state"] | "OFF", sizeof(state));

    // update our pwm channel
    ch->setState(state);

    // get that update out ASAP... if its our own update
    if (!strcmp(ch->source, local_hostname))
      sendFastUpdate();
  }
#else
  return generateErrorJSON(output, "Board does not have output channels.");
#endif
}

void ProtocolController::handleConfigPWMChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_PWM_CHANNELS
  char error[128];

  // load our channel
  auto* ch = lookupChannel(input, output, pwm_channels);
  if (!ch)
    return;

  if (!input["config"].is<JsonObjectConst>()) {
    snprintf(error, sizeof(error), "'config' is required parameter");
    return generateErrorJSON(output, error);
  }

  if (!ch->loadConfig(input["config"], error, sizeof(error))) {
    return generateErrorJSON(output, error);
  }

  // write it to file
  if (!saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
#else
  return generateErrorJSON(output, "Board does not have pwm channels.");
#endif
}

void ProtocolController::handleTogglePWMChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_PWM_CHANNELS

  // load our channel
  auto* ch = lookupChannel(input, output, pwm_channels);
  if (!ch)
    return;

  // source is required
  if (!input["source"].is<String>())
    return generateErrorJSON(output, "'source' is a required parameter");

  // check the length
  char error[50];
  if (strlen(input["source"]) > YB_HOSTNAME_LENGTH - 1) {
    sprintf(error, "Maximum source length is %s characters.", YB_HOSTNAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // save our source
  strlcpy(ch->source, input["source"] | local_hostname, sizeof(ch->source));

  // these states should all change to off
  if (!strcmp(ch->getStatus(), "ON") || !strcmp(ch->getStatus(), "TRIPPED") || !strcmp(ch->getStatus(), "BLOWN"))
    ch->setState("OFF");
  // OFF and BYPASS can be turned on.
  else
    ch->setState("ON");
#else
  return generateErrorJSON(output, "Board does not have output channels.");
#endif
}

void ProtocolController::handleConfigRelayChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_RELAY_CHANNELS
  char error[128];

  // load our channel
  auto* ch = lookupChannel(input, output, relay_channels);
  if (!ch)
    return;

  if (!input["config"].is<JsonObjectConst>()) {
    snprintf(error, sizeof(error), "'config' is required parameter");
    return generateErrorJSON(output, error);
  }

  if (!ch->loadConfig(input["config"], error, sizeof(error))) {
    return generateErrorJSON(output, error);
  }

  // write it to file
  if (!saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
#else
  return generateErrorJSON(output, "Board does not have relay channels.");
#endif
}

void ProtocolController::handleSetRelayChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_RELAY_CHANNELS
  char prefIndex[YB_PREF_KEY_LENGTH];

  // load our channel
  auto* ch = lookupChannel(input, output, relay_channels);
  if (!ch)
    return;

  // is it enabled?
  if (!ch->isEnabled)
    return generateErrorJSON(output, "Channel is not enabled.");

  // change state
  if (input["state"].is<String>()) {
    // source is required
    if (!input["source"].is<String>())
      return generateErrorJSON(output, "'source' is a required parameter");

    // check the length
    char error[50];
    if (strlen(input["source"]) > YB_HOSTNAME_LENGTH - 1) {
      sprintf(error, "Maximum source length is %s characters.", YB_HOSTNAME_LENGTH - 1);
      return generateErrorJSON(output, error);
    }

    // get our data
    strlcpy(ch->source, input["source"] | local_hostname, sizeof(ch->source));

    // okay, set our state
    char state[10];
    strlcpy(state, input["state"] | "OFF", sizeof(state));

    // update our pwm channel
    ch->setState(state);

    // get that update out ASAP... if its our own update
    if (!strcmp(ch->source, local_hostname))
      sendFastUpdate();
  }
#else
  return generateErrorJSON(output, "Board does not have relay channels.");
#endif
}

void ProtocolController::handleToggleRelayChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_RELAY_CHANNELS
  // load our channel
  auto* ch = lookupChannel(input, output, relay_channels);
  if (!ch)
    return;

  // source is required
  if (!input["source"].is<String>())
    return generateErrorJSON(output, "'source' is a required parameter");

  // check the length
  char error[50];
  if (strlen(input["source"]) > YB_HOSTNAME_LENGTH - 1) {
    sprintf(error, "Maximum source length is %s characters.", YB_HOSTNAME_LENGTH - 1);
    return generateErrorJSON(output, error);
  }

  // save our source
  strlcpy(ch->source, input["source"] | local_hostname, sizeof(ch->source));

  // relays are simple on/off.
  if (!strcmp(ch->getStatus(), "ON"))
    ch->setState("OFF");
  else
    ch->setState("ON");
#else
  return generateErrorJSON(output, "Board does not have relay channels.");
#endif
}

void ProtocolController::handleConfigServoChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_SERVO_CHANNELS
  char error[128];

  // load our channel
  auto* ch = lookupChannel(input, output, servo_channels);
  if (!ch)
    return;

  if (!input["config"].is<JsonObjectConst>()) {
    snprintf(error, sizeof(error), "'config' is required parameter");
    return generateErrorJSON(output, error);
  }

  if (!ch->loadConfig(input["config"], error, sizeof(error))) {
    return generateErrorJSON(output, error);
  }

  // write it to file
  if (!saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
#else
  return generateErrorJSON(output, "Board does not have servo channels.");
#endif
}

void ProtocolController::handleSetServoChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_SERVO_CHANNELS
  // load our channel
  auto* ch = lookupChannel(input, output, servo_channels);
  if (!ch)
    return;

  // is it enabled?
  if (!ch->isEnabled)
    return generateErrorJSON(output, "Channel is not enabled.");

  if (input["angle"].is<JsonVariantConst>()) {
    float angle = input["angle"];

    if (angle >= 0 && angle <= 180)
      ch->write(angle);
    else
      return generateErrorJSON(output, "'angle' must be between 0 and 180");
  } else
    return generateErrorJSON(output, "'angle' parameter is required.");
#else
  return generateErrorJSON(output, "Board does not have servo channels.");
#endif
}

void ProtocolController::handleConfigStepperChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_STEPPER_CHANNELS
  char error[128];

  // load our channel
  auto* ch = lookupChannel(input, output, stepper_channels);
  if (!ch)
    return;

  if (!input["config"].is<JsonObjectConst>()) {
    snprintf(error, sizeof(error), "'config' is required parameter");
    return generateErrorJSON(output, error);
  }

  if (!ch->loadConfig(input["config"], error, sizeof(error)))
    return generateErrorJSON(output, error);

  // write it to file
  if (!saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
#else
  return generateErrorJSON(output, "Board does not have stepper channels.");
#endif
}

void ProtocolController::handleSetStepperChannel(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_STEPPER_CHANNELS
  // load our channel
  auto* ch = lookupChannel(input, output, stepper_channels);
  if (!ch)
    return;

  // is it enabled?
  if (!ch->isEnabled)
    return generateErrorJSON(output, "Channel is not enabled.");

  // update our rpm
  if (input["rpm"]) {
    float rpm = input["rpm"];
    ch->setSpeed(rpm);
  }

  // start a homing operation
  if (input["home"]) {
    ch->home();
    return;
  }

  // move to an angle
  if (input["angle"].is<JsonVariantConst>()) {
    float angle = input["angle"];
    ch->gotoAngle(angle);
  }
#else
  return generateErrorJSON(output, "Board does not have stepper channels.");
#endif
}

void ProtocolController::handleConfigADC(JsonVariantConst input, JsonVariant output)
{
#ifdef YB_HAS_ADC_CHANNELS
  char error[128] = "Unknown";

  // load our channel
  auto* ch = lookupChannel(input, output, adc_channels);
  if (!ch)
    return;

  // channel name
  if (input["name"].is<String>()) {
    // is it too long?
    if (strlen(input["name"]) > YB_CHANNEL_NAME_LENGTH - 1) {
      sprintf(error, "Maximum channel name length is %s characters.", YB_CHANNEL_NAME_LENGTH - 1);
      return generateErrorJSON(output, error);
    }

    // save to our storage
    strlcpy(ch->name, input["name"] | "ADC ?", sizeof(ch->name));
  }

  // enabled
  if (input["enabled"].is<bool>()) {
    // save right nwo.
    bool enabled = input["enabled"];
    ch->isEnabled = enabled;
  }

  // channel type
  if (input["type"].is<String>()) {
    // is it too long?
    if (strlen(input["type"]) > YB_TYPE_LENGTH - 1) {
      sprintf(error, "Maximum channel type length is %s characters.", YB_CHANNEL_NAME_LENGTH - 1);
      return generateErrorJSON(output, error);
    }

    // save to our storage
    strlcpy(ch->type, input["type"] | "other", sizeof(ch->type));
  }

  // channel type
  if (input["display_decimals"].is<int>()) {
    int8_t display_decimals = input["display_decimals"];

    if (display_decimals < 0 || display_decimals > 4)
      return generateErrorJSON(output, "Must be between 0 and 4");

    // change our channel.
    ch->displayDecimals = display_decimals;
  }

  // useCalibrationTable
  if (input["useCalibrationTable"].is<bool>()) {
    // save right nwo.
    bool enabled = input["useCalibrationTable"];
    ch->useCalibrationTable = enabled;
  }

  // are we using a calibration table?
  if (ch->useCalibrationTable) {
    // calibratedUnits
    if (input["calibratedUnits"].is<String>()) {
      // is it too long?
      if (strlen(input["calibratedUnits"]) > YB_ADC_UNIT_LENGTH - 1) {
        sprintf(error, "Maximum calibrated units length is %s characters.", YB_ADC_UNIT_LENGTH - 1);
        return generateErrorJSON(output, error);
      }

      // save to our storage
      strlcpy(ch->calibratedUnits, input["calibratedUnits"] | "", sizeof(ch->calibratedUnits));
    }

    // calibratedUnits
    if (input["add_calibration"].is<JsonArrayConst>()) {
      JsonArrayConst pair = input["add_calibration"].as<JsonArrayConst>();
      if (pair.size() != 2)
        return generateErrorJSON(output, "Each calibration entry must have exactly 2 elements [v, y]");

      float v = pair[0].as<float>();
      float y = pair[1].as<float>();

      // add it in
      if (!ch->addCalibrationValue({v, y}))
        return generateErrorJSON(output, "Failed to add calibration entry");
    }

    // calibratedUnits
    if (input["remove_calibration"].is<JsonArrayConst>()) {
      JsonArrayConst pair = input["remove_calibration"].as<JsonArrayConst>();
      if (pair.size() != 2)
        return generateErrorJSON(output, "Each calibration entry must have exactly 2 elements [v, y]");

      float v = pair[0].as<float>();
      float y = pair[1].as<float>();

      // Use std::isfinite(v) if <cmath> is available; otherwise isfinite(v) with <math.h>
      if (!std::isfinite(v) || !std::isfinite(y))
        return generateErrorJSON(output, "Non-finite number in table");

      // remove any matching elements
      for (auto it = ch->calibrationTable.begin(); it != ch->calibrationTable.end();) {
        if (it->voltage == v && it->calibrated == y) {
          // erase returns the next valid iterator
          it = ch->calibrationTable.erase(it);
        } else {
          ++it;
        }
      }
    }
  }

  // write it to file
  if (!saveConfig(error, sizeof(error)))
    return generateErrorJSON(output, error);
#else
  return generateErrorJSON(output, "Board does not have ADC channels.");
#endif
}

void ProtocolController::handleSetTheme(JsonVariantConst input, JsonVariant output)
{
  if (!input["theme"].is<String>())
    return generateErrorJSON(output, "'theme' is a required parameter");

  String temp = input["theme"];

  if (temp != "light" && temp != "dark")
    return generateErrorJSON(output,
      "'theme' must either be 'light' or 'dark'");

  _config.app_theme = temp;

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

    _config.globalBrightness = brightness;

// TODO: need to put this on a time delay
// preferences.putFloat("brightness", globalBrightness);

// loop through all our light stuff and update outputs
#ifdef YB_HAS_PWM_CHANNELS
    for (auto& ch : pwm_channels)
      ch.updateOutput(true);
#endif

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
  _config.generateFullConfig(cfg);
}

void ProtocolController::generateConfigJSON(JsonVariant output)
{
  // extra info
  output["msg"] = "config";
  output["hostname"] = _config.local_hostname;
  output["use_ssl"] = _config.app_enable_ssl;
  output["enable_ota"] = _config.app_enable_ota;
  output["enable_mqtt"] = _config.app_enable_mqtt;
  output["default_role"] = getRoleText(_config.app_default_role);
  output["brightness"] = _config.globalBrightness;
  output["git_hash"] = GIT_HASH;
  output["build_time"] = BUILD_TIME;

#ifdef YB_HAS_PIEZO
  generateMelodyJSON(output);
#endif
  _config.generateBoardConfig(output);

  output["is_development"] = YB_IS_DEVELOPMENT;

  // some debug info
  output["last_restart_reason"] = getResetReason();
  if (has_coredump)
    output["has_coredump"] = has_coredump;
  output["boot_log"] = startupLogger.c_str();

#ifdef YB_HAS_BUS_VOLTAGE
  output["bus_voltage"] = true;
#endif

  // do we want to flag it for config?
  if (_config.is_first_boot)
    output["first_boot"] = true;
}

void ProtocolController::generateUpdateJSON(JsonVariant output)
{
  output["msg"] = "update";
  output["uptime"] = esp_timer_get_time();

#ifdef YB_HAS_BUS_VOLTAGE
  output["bus_voltage"] = getBusVoltage();
#endif

#ifdef YB_HAS_PWM_CHANNELS
  JsonArray channels = output["pwm"].to<JsonArray>();
  for (auto& ch : pwm_channels) {
    JsonObject jo = channels.add<JsonObject>();
    ch.generateUpdate(jo);
  }
#endif

#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
  JsonArray channels = output["dio"].to<JsonArray>();
  for (auto& ch : digital_input_channels) {
    JsonObject jo = channels.add<JsonObject>();
    ch.generateUpdate(jo);
  }
#endif

#ifdef YB_HAS_RELAY_CHANNELS
  JsonArray r_channels = output["relay"].to<JsonArray>();
  for (auto& ch : relay_channels) {
    JsonObject jo = r_channels.add<JsonObject>();
    ch.generateUpdate(jo);
  }
#endif

#ifdef YB_HAS_SERVO_CHANNELS
  JsonArray servo_array = output["servo"].to<JsonArray>();
  for (auto& ch : servo_channels) {
    JsonObject jo = servo_array.add<JsonObject>();
    ch.generateUpdate(jo);
  }
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
  JsonArray stepper_array = output["stepper"].to<JsonArray>();
  for (auto& ch : stepper_channels) {
    JsonObject jo = stepper_array.add<JsonObject>();
    ch.generateUpdate(jo);
  }
#endif

// input / analog ADC channels
#ifdef YB_HAS_ADC_CHANNELS
  JsonArray channels = output["adc"].to<JsonArray>();
  for (auto& ch : adc_channels) {
    JsonObject jo = channels.add<JsonObject>();
    ch.generateUpdate(jo);
  }
#endif

#ifdef YB_IS_BRINEOMATIC
  wm.generateUpdateJSON(output);
#endif
}

void ProtocolController::generateFastUpdateJSON(JsonVariant output)
{
  output["msg"] = "update";
  output["fast"] = 1;
  output["uptime"] = esp_timer_get_time();

#ifdef YB_HAS_BUS_VOLTAGE
  output["bus_voltage"] = getBusVoltage();
#endif

  byte j;

#ifdef YB_HAS_PWM_CHANNELS
  JsonArray channels = output["pwm"].to<JsonArray>();
  for (auto& ch : pwm_channels) {
    if (ch.sendFastUpdate) {
      JsonObject jo = channels.add<JsonObject>();
      ch.generateUpdate(jo);
      ch.sendFastUpdate = false;
    }
  }
#endif

#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
  JsonArray channels = output["dio"].to<JsonArray>();
  for (auto& ch : digital_input_channels) {
    if (ch.sendFastUpdate) {
      JsonObject jo = channels.add<JsonObject>();
      ch.generateUpdate(jo);
      ch.sendFastUpdate = false;
    }
  }
#endif
}

void ProtocolController::generateStatsJSON(JsonVariant output)
{
  // some basic statistics and info
  output["msg"] = "stats";
  output["uuid"] = _config.uuid;
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
  if (_config.app_enable_mqtt)
    output["mqtt_connected"] = _app.mqtt.isConnected();

  // what is our IP address?
  if (!strcmp(_config.wifi_mode, "ap"))
    output["ip_address"] = _app.network.apIP;
  else
    output["ip_address"] = WiFi.localIP();

#ifdef YB_HAS_BUS_VOLTAGE
  output["bus_voltage"] = getBusVoltage();
#endif

#ifdef YB_HAS_PWM_CHANNELS
  // info about each of our channels
  JsonArray channels = output["pwm"].to<JsonArray>();
  for (auto& ch : pwm_channels) {
    JsonObject jo = channels.add<JsonObject>();
    jo["id"] = ch.id;
    jo["name"] = ch.name;
    jo["aH"] = ch.ampHours;
    jo["wH"] = ch.wattHours;
    jo["state_change_count"] = ch.stateChangeCount;
    jo["soft_fuse_trip_count"] =
      ch.softFuseTripCount;
  }
#endif

#ifdef YB_HAS_FANS
  // info about each of our fans
  for (byte i = 0; i < YB_FAN_COUNT; i++)
    output["fans"][i]["rpm"] = fans_last_rpm[i];
#endif

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
  _config.generateNetworkConfig(output);
}

void ProtocolController::generateAppConfigMessage(JsonVariant output)
{
  // our identifying info
  output["msg"] = "app_config";
  _config.generateAppConfig(output);
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

void ProtocolController::generatePongJSON(JsonVariant output) { output["pong"] = millis(); }

void ProtocolController::sendThemeUpdate()
{
  JsonDocument output;
  output["msg"] = "set_theme";
  output["theme"] = _config.app_theme;

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
  output["brightness"] = _config.globalBrightness;

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

  if (_config.app_enable_serial && _config.serial_role >= auth_level)
    Serial.println(jsonString);
}
