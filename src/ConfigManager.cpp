#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

ConfigManager::ConfigManager(YarrboardApp& app) : _app(app), is_first_boot(true)
{
}

bool ConfigManager::setup()
{
  if (preferences.begin("yarrboard", false)) {
    YBP.println("Prefs OK");
    YBP.printf("There are: %u entries available in the 'yarrboard' prefs table.\n", preferences.freeEntries());

  } else {
    YBP.println("Opening Preferences failed.");
    return false;
  }

  // default to first time, prove it later
  is_first_boot = true;

  // initialize error string
  char error[YB_ERROR_LENGTH] = "";

  // start with defaults
  initializeChannels();

  // load our config from the json file.
  if (loadConfigFromFile(YB_BOARD_CONFIG_PATH, error, sizeof(error))) {
    YBP.println("Configuration OK");
    return true;
  } else {
    YBP.printf("CONFIG ERROR: %s\n", error);
    return false;
  }

  return false;
}

void ConfigManager::initializeChannels()
{
#ifdef YB_HAS_ADC_CHANNELS
  initChannels(adc_channels);
#endif

#ifdef YB_HAS_PWM_CHANNELS
  initChannels(pwm_channels);
#endif

#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
  initChannels(digital_input_channels);
#endif

#ifdef YB_HAS_RELAY_CHANNELS
  initChannels(relay_channels);
#endif

#ifdef YB_HAS_SERVO_CHANNELS
  initChannels(servo_channels);
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
  initChannels(stepper_channels);
#endif
}

bool ConfigManager::saveConfig(char* error, size_t len)
{
  // our doc to store.
  JsonDocument config;

  // generate a full new document each time
  generateFullConfig(config);

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(config);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // now serialize it to the buffer
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(config, jsonBuffer, jsonSize + 1);
  } else {
    snprintf(error, len, "saveConfig() failed to create buffer of size %d", jsonSize);
    return false;
  }

  // write our config to local storage
  File fp = LittleFS.open(YB_BOARD_CONFIG_PATH, "w");
  if (!fp) {
    snprintf(error, len, "Failed to open %s for writing", YB_BOARD_CONFIG_PATH);
    free(jsonBuffer);
    return false;
  }

  // check write result
  size_t bytesWritten = fp.print((char*)jsonBuffer);
  if (bytesWritten == 0) {
    fp.close();
    strncpy(error, "Failed to write JSON data to file", len);
    free(jsonBuffer);
    return false;
  }

  // flush data (no return value, but still good to call)
  fp.flush();
  fp.close();

  // confirm file exists and has non-zero length
  if (!LittleFS.exists(YB_BOARD_CONFIG_PATH)) {
    strncpy(error, "File not found after write", len);
    free(jsonBuffer);
    return false;
  }

  // check for size and opening
  File verify = LittleFS.open(YB_BOARD_CONFIG_PATH, "r");
  if (!verify || verify.size() == 0) {
    verify.close();
    strncpy(error, "Wrote file but it appears empty or unreadable", len);
    free(jsonBuffer);
    return false;
  }
  verify.close();

  // free up our memory
  free(jsonBuffer);

  return true;
}

void ConfigManager::generateFullConfig(JsonVariant output)
{
  // our board specific configuration
  JsonObject board = output["board"].to<JsonObject>();
  generateBoardConfig(board);

  // yarrboard application specific configuration
  JsonObject app = output["app"].to<JsonObject>();
  generateAppConfig(app);
  app.remove("msg");

  // network connection specific configuration
  JsonObject network = output["network"].to<JsonObject>();
  generateNetworkConfig(network);
  network.remove("msg");
}

void ConfigManager::generateBoardConfig(JsonVariant output)
{
  // our identifying info
  output["name"] = board_name;
  output["uuid"] = uuid;
  output["firmware_version"] = YB_FIRMWARE_VERSION;
  output["hardware_version"] = YB_HARDWARE_VERSION;
  output["esp_idf_version"] = esp_get_idf_version();
  output["arduino_version"] = ESP_ARDUINO_VERSION_STR;
  output["psychic_http_version"] = PSYCHIC_VERSION_STR;
  output["git_hash"] = GIT_HASH;
  output["build_time"] = BUILD_TIME;

// output / pwm channels
#ifdef YB_HAS_PWM_CHANNELS
  JsonArray channels = output["pwm"].to<JsonArray>();
  for (auto& ch : pwm_channels) {
    JsonObject jo = channels.add<JsonObject>();
    ch.generateConfig(jo);
  }
#endif

#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
  JsonArray channels = output["dio"].to<JsonArray>();
  for (auto& ch : digital_input_channels) {
    JsonObject jo = channels.add<JsonObject>();
    ch.generateConfig(jo);
  }
#endif

#ifdef YB_HAS_RELAY_CHANNELS
  JsonArray r_channels = output["relay"].to<JsonArray>();
  for (auto& ch : relay_channels) {
    JsonObject jo = r_channels.add<JsonObject>();
    ch.generateConfig(jo);
  }
#endif

#ifdef YB_HAS_SERVO_CHANNELS
  JsonArray s_channels = output["servo"].to<JsonArray>();
  for (auto& ch : servo_channels) {
    JsonObject jo = s_channels.add<JsonObject>();
    ch.generateConfig(jo);
  }
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
  JsonArray st_channels = output["stepper"].to<JsonArray>();
  for (auto& ch : stepper_channels) {
    JsonObject jo = st_channels.add<JsonObject>();
    ch.generateConfig(jo);
  }
#endif

// input / analog ADC channels
#ifdef YB_HAS_ADC_CHANNELS
  output["adc_resolution"] = YB_ADC_RESOLUTION;
  JsonArray channels = output["adc"].to<JsonArray>();
  for (auto& ch : adc_channels) {
    JsonObject jo = channels.add<JsonObject>();
    ch.generateConfig(jo);
  }
#endif

#ifdef YB_IS_BRINEOMATIC
  wm.generateConfigJSON(output);
#endif
}

void ConfigManager::generateAppConfig(JsonVariant output)
{
  // our identifying info
  output["startup_melody"] = startup_melody;
  output["default_role"] = _app.protocol.getRoleText(app_default_role);
  output["admin_user"] = admin_user;
  output["admin_pass"] = admin_pass;
  output["guest_user"] = guest_user;
  output["guest_pass"] = guest_pass;
  output["app_update_interval"] = app_update_interval;
  output["app_enable_mfd"] = app_enable_mfd;
  output["app_enable_api"] = app_enable_api;
  output["app_enable_serial"] = app_enable_serial;
  output["app_enable_ota"] = app_enable_ota;
  output["app_enable_ssl"] = app_enable_ssl;
  output["app_enable_mqtt"] = app_enable_mqtt;
  output["app_enable_ha_integration"] = app_enable_ha_integration;
  output["app_use_hostname_as_mqtt_uuid"] = app_use_hostname_as_mqtt_uuid;
  output["mqtt_server"] = mqtt_server;
  output["mqtt_user"] = mqtt_user;
  output["mqtt_pass"] = mqtt_pass;
  output["mqtt_cert"] = mqtt_cert;
  output["server_cert"] = server_cert;
  output["server_key"] = server_key;
}

void ConfigManager::generateNetworkConfig(JsonVariant output)
{
  // our identifying info
  output["wifi_mode"] = wifi_mode;
  output["wifi_ssid"] = wifi_ssid;
  output["wifi_pass"] = wifi_pass;
  output["local_hostname"] = local_hostname;
}

bool ConfigManager::loadConfigFromFile(const char* file, char* error, size_t len)
{
  // sanity check on LittleFS
  if (!LittleFS.begin()) {
    snprintf(error, len, "LittleFS mount failed");
    return false;
  }

  // open file
  File configFile = LittleFS.open(file, "r");
  if (!configFile || !configFile.available()) {
    snprintf(error, len, "Could not open file: %s", file);
    return false;
  }

  // get size and check reasonableness
  size_t size = configFile.size();
  if (size == 0) {
    snprintf(error, len, "File %s is empty", file);
    configFile.close();
    return false;
  }
  if (size > 10000) { // arbitrary limit to prevent large loads
    snprintf(error, len, "File %s too large (%u bytes)", file, (unsigned int)size);
    configFile.close();
    return false;
  }

  // read into buffer
  char* buf = (char*)malloc(size + 1);
  if (!buf) {
    snprintf(error, len, "Memory allocation failed for %u bytes", (unsigned int)size);
    configFile.close();
    free(buf);
    return false;
  }

  size_t bytesRead = configFile.readBytes(buf, size);
  configFile.close();
  buf[bytesRead] = '\0';

  if (bytesRead != size) {
    snprintf(error, len, "Read size mismatch: expected %u, got %u", (unsigned int)size, (unsigned int)bytesRead);
    free(buf);
    return false;
  }

  // parse JSON
  JsonDocument doc; // adjust to match your configuration complexity
  DeserializationError err = deserializeJson(doc, buf);

  // no leaks
  free(buf);

  if (err) {
    snprintf(error, len, "JSON parse error: %s", err.c_str());
    return false;
  }

  // sanity check: ensure root object
  if (!doc.is<JsonObject>()) {
    snprintf(error, len, "Root element is not a JSON object");
    return false;
  }

  // if we have an existing config file, no need for first round stuff.
  is_first_boot = false;

  return loadConfigFromJSON(doc, error, len);
}

bool ConfigManager::loadConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  bool result = true;

  if (config["network"]) {
    if (!loadNetworkConfigFromJSON(config["network"], error, len)) {
      YBP.print(error);
      result = false;
    }
  } else
    YBP.println("Missing 'network' config");

  if (config["app"]) {
    if (!loadAppConfigFromJSON(config["app"], error, len)) {
      YBP.print(error);
      result = false;
    }
  } else
    YBP.println("Missing 'app' config");

  if (config["board"]) {
    if (!loadBoardConfigFromJSON(config["board"], error, len)) {
      YBP.print(error);
      result = false;
    }
  } else
    YBP.println("Missing 'board' config");

  return result;
}

bool ConfigManager::loadNetworkConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  const char* v;

  // local_hostname
  v = config["local_hostname"] | YB_DEFAULT_HOSTNAME;
  strlcpy(local_hostname, v, sizeof(local_hostname));

  // wifi_ssid
  v = config["wifi_ssid"] | YB_DEFAULT_AP_SSID;
  strlcpy(wifi_ssid, v, sizeof(wifi_ssid));

  // wifi_pass
  v = config["wifi_pass"] | YB_DEFAULT_AP_PASS;
  strlcpy(wifi_pass, v, sizeof(wifi_pass));

  // wifi_mode
  v = config["wifi_mode"] | YB_DEFAULT_AP_MODE;
  strlcpy(wifi_mode, v, sizeof(wifi_mode));

  return true;
}

bool ConfigManager::loadAppConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  const char* v;

  // startup_melody
  v = config["startup_melody"] | YB_PIEZO_DEFAULT_MELODY;
  strlcpy(startup_melody, v, sizeof(startup_melody));

  // admin_user
  v = config["admin_user"] | YB_DEFAULT_ADMIN_USER;
  strlcpy(admin_user, v, sizeof(admin_user));

  // admin_pass
  v = config["admin_pass"] | YB_DEFAULT_ADMIN_PASS;
  strlcpy(admin_pass, v, sizeof(admin_pass));

  // guest_user
  v = config["guest_user"] | YB_DEFAULT_GUEST_USER;
  strlcpy(guest_user, v, sizeof(guest_user));

  // guest_pass
  v = config["guest_pass"] | YB_DEFAULT_GUEST_PASS;
  strlcpy(guest_pass, v, sizeof(guest_pass));

  // MQTT fields
  v = config["mqtt_server"] | "";
  strlcpy(mqtt_server, v, sizeof(mqtt_server));

  v = config["mqtt_user"] | "";
  strlcpy(mqtt_user, v, sizeof(mqtt_user));

  v = config["mqtt_pass"] | "";
  strlcpy(mqtt_pass, v, sizeof(mqtt_pass));
  mqtt_cert = config["mqtt_cert"] | "";

  if (config["app_update_interval"]) {
    app_update_interval = config["app_update_interval"] | YB_DEFAULT_APP_UPDATE_INTERVAL;
    app_update_interval = max(100u, app_update_interval);
    app_update_interval = min(10000u, app_update_interval);
  }

  app_default_role = YB_DEFAULT_APP_DEFAULT_ROLE;
  if (config["default_role"]) {
    v = config["default_role"];
    if (!strcmp(v, "nobody"))
      app_default_role = NOBODY;
    else if (!strcmp(v, "admin"))
      app_default_role = ADMIN;
    else if (!strcmp(v, "guest"))
      app_default_role = GUEST;
  }
  serial_role = app_default_role;
  api_role = app_default_role;

  app_enable_mfd = config["app_enable_mfd"] | YB_DEFAULT_APP_ENABLE_MFD;
  app_enable_api = config["app_enable_api"] | YB_DEFAULT_APP_ENABLE_API;
  app_enable_serial = config["app_enable_serial"] | YB_DEFAULT_APP_ENABLE_SERIAL;
  app_enable_ota = config["app_enable_ota"] | YB_DEFAULT_APP_ENABLE_OTA;
  app_enable_ssl = config["app_enable_ssl"] | YB_DEFAULT_APP_ENABLE_SSL;
  app_enable_mqtt = config["app_enable_mqtt"] | YB_DEFAULT_APP_ENABLE_MQTT;
  app_enable_ha_integration = config["app_enable_ha_integration"] | YB_DEFAULT_APP_ENABLE_HA_INTEGRATION;
  app_use_hostname_as_mqtt_uuid = config["app_use_hostname_as_mqtt_uuid"] | YB_DEFAULT_USE_HOSTNAME_AS_MQTT_UUID;

  server_cert = config["server_cert"] | "";
  server_key = config["server_key"] | "";

  return true;
}

bool ConfigManager::loadBoardConfigFromJSON(JsonVariant config, char* error, size_t len)
{
  bool result = true;

  const char* v = config["name"] | YB_BOARD_NAME;
  strlcpy(board_name, v, sizeof(board_name));

#ifdef YB_HAS_ADC_CHANNELS
  if (config["adc"]) {
    if (!loadChannelsConfigFromJSON("adc", adc_channels, config, error, len)) {
      YBP.println(error);
      result = false;
    }
  }
#endif

#ifdef YB_HAS_PWM_CHANNELS
  if (config["pwm"]) {
    if (!loadChannelsConfigFromJSON("pwm", pwm_channels, config, error, len)) {
      YBP.println(error);
      result = false;
    }
  }
#endif

#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
  if (config["dio"]) {
    if (!loadChannelsConfigFromJSON("dio", digital_input_channels, config, error, len)) {
      YBP.println(error);
      result = false;
    }
  }
#endif

#ifdef YB_HAS_RELAY_CHANNELS
  if (config["relay"]) {
    if (!loadChannelsConfigFromJSON("relay", relay_channels, config, error, len)) {
      YBP.println(error);
      result = false;
    }
  }
#endif

#ifdef YB_HAS_SERVO_CHANNELS
  if (config["servo"]) {
    if (!loadChannelsConfigFromJSON("servo", servo_channels, config, error, len)) {
      YBP.println(error);
      result = false;
    }
  }
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
  if (config["stepper"]) {
    if (!loadChannelsConfigFromJSON("stepper", stepper_channels, config, error, len)) {
      YBP.println(error);
      result = false;
    }
  }
#endif

#ifdef YB_IS_BRINEOMATIC
  if (config["brineomatic"]) {
    JsonVariant bom = config["brineomatic"].as<JsonVariant>();

    // validate prunes invalid entries, so it's safe to load even on error.
    // we don't want a single bad config option to nuke the whole config loading.
    if (!wm.validateConfigJSON(bom, error, len)) {
      YBP.println(error);
      result = false;
    }

    wm.loadGeneralConfigJSON(bom);
    wm.loadHardwareConfigJSON(bom);
    wm.loadSafeguardsConfigJSON(bom);
  }
#endif

  return result;
}