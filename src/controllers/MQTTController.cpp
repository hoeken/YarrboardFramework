/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "controllers/MQTTController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

#ifdef YB_HAS_PWM_CHANNELS
  #include "pwm_channel.h"
#endif

#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
  #include "digital_input_channel.h"
#endif

#ifdef YB_HAS_RELAY_CHANNELS
  #include "relay_channel.h"
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
  #include "stepper_channel.h"
#endif

#ifdef YB_HAS_SERVO_CHANNELS
  #include "servo_channel.h"
#endif

MQTTController* MQTTController::_instance = nullptr;

MQTTController::MQTTController(YarrboardApp& app) : BaseController(app, "mqtt")
{
}

bool MQTTController::setup()
{
  _instance = this; // Capture the instance for callbacks

  // are we enabled?
  if (!_cfg.app_enable_mqtt)
    return true;

  mqttClient.setServer(_cfg.mqtt_server);
  mqttClient.setCredentials(_cfg.mqtt_user, _cfg.mqtt_pass);

  if (_cfg.mqtt_cert.length())
    mqttClient.setCACert(_cfg.mqtt_cert.c_str());

  // on connect home hook
  mqttClient.onConnect(_onConnectStatic);

  // home assistant connection discovery hook.
  if (_cfg.app_enable_ha_integration) {
    mqttClient.onTopic("homeassistant/status", 0, [&](const char* topic, const char* payload, int retain, int qos, bool dup) {
      if (!strcmp(payload, "online"))
        haDiscovery();
    });
  }

  mqttClient.connect();

  /**
   * Wait blocking until the connection is established
   */
  int tries = 0;
  while (!mqttClient.connected()) {
    vTaskDelay(pdMS_TO_TICKS(100));
    tries++;

    if (tries > 20) {
      YBP.println("MQTT failed to connect.");
      return false;
    }
  }

  return true;
}

void MQTTController::loop()
{
  if (!mqttClient.connected())
    return;

  // periodically update our mqtt / HomeAssistant status
  unsigned int messageDelta = millis() - previousMQTTMillis;
  if (messageDelta >= 1000) {
    if (mqttClient.connected()) {

      for (auto& c : _app.getControllers()) {
        c->mqttUpdateHook();
      }

      // separately update our Home Assistant status
      if (_cfg.app_enable_ha_integration) {
        for (auto& c : _app.getControllers()) {
          c->haUpdateHook();
        }
      }
    }

    previousMQTTMillis = millis();
  }
}

void MQTTController::disconnect()
{
  if (mqttClient.connected())
    mqttClient.disconnect();
}

bool MQTTController::isConnected()
{
  return mqttClient.connected();
}

void MQTTController::onTopic(const char* topic, int qos, OnMessageUserCallback callback)
{
  mqttClient.onTopic(topic, qos, callback);
}

void MQTTController::publish(const char* topic, const char* payload, bool use_prefix)
{
  if (!mqttClient.connected())
    return;

  int ret;

  // prefix it with yarrboard or nah?
  if (use_prefix) {
    char mqtt_path[256];
    sprintf(mqtt_path, "yarrboard/%s/%s", _cfg.local_hostname, topic);
    ret = mqttClient.publish(mqtt_path, 0, 0, payload, strlen(payload), false);
    if (ret == -1)
      YBP.printf("[mqtt] Error publishing prefix path %s\n", mqtt_path);
  } else {
    ret = mqttClient.publish(topic, 0, 0, payload, strlen(payload), false);
    if (ret == -1)
      YBP.printf("[mqtt] Error publishing topic %s\n", topic);
  }
}

void MQTTController::receiveMessage(const char* topic, const char* payload, int retain, int qos, bool dup)
{
  YBP.printf("Received Topic: %s\r\n", topic);
  YBP.printf("Received Payload: %s\r\n", payload);
}

void MQTTController::_receiveMessageStatic(const char* topic, const char* payload, int retain, int qos, bool dup)
{
  if (_instance) {
    _instance->receiveMessage(topic, payload, retain, qos, dup);
  }
}

void MQTTController::onConnect(bool sessionPresent)
{
  YBP.println("Connected to MQTT.");

  if (_cfg.app_enable_ha_integration)
    haDiscovery();

  // look for json messages on this path...
  char mqtt_path[128];
  sprintf(mqtt_path, "yarrboard/%s/command", _cfg.local_hostname);
  mqttClient.onTopic(mqtt_path, 0, _receiveMessageStatic);
}

void MQTTController::_onConnectStatic(bool sessionPresent)
{
  if (_instance) {
    _instance->onConnect(sessionPresent);
  }
}

void MQTTController::haDiscovery()
{
  if (!mqttClient.connected())
    return;

  // how to structure our id?
  char ha_dev_uuid[128];
  if (_cfg.app_use_hostname_as_mqtt_uuid)
    sprintf(ha_dev_uuid, "yarrboard_%s", _cfg.local_hostname);
  else
    sprintf(ha_dev_uuid, "yarrboard_%s", _cfg.uuid);

  char topic[128];
  sprintf(topic, "homeassistant/device/%s/config", ha_dev_uuid);

  // this is our device information.
  JsonDocument doc;
  JsonObject device = doc["dev"].to<JsonObject>();
  device["ids"] = ha_dev_uuid;
  device["name"] = _cfg.board_name;
  device["mf"] = _app.manufacturer;
  device["mdl"] = _app.hardware_version;
  device["sw"] = _app.firmware_version;
  device["sn"] = _cfg.uuid;
  char config_url[128];
  sprintf(config_url, "http://%s.local", _cfg.local_hostname);
  device["configuration_url"] = config_url;

  // our origin to let HA know where it came from.
  JsonObject origin = doc["o"].to<JsonObject>();
  origin["name"] = "yarrboard";
  origin["sw"] = _app.firmware_version;
  origin["url"] = "https://github.com/hoeken/yarrboard-firmware";

  // our components array
  JsonObject components = doc["cmps"].to<JsonObject>();

  for (auto& c : _app.getControllers()) {
    c->haGenerateDiscoveryHook(components);
  }

  // dynamically allocate our buffer
  size_t jsonSize = measureJson(doc);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);

  // did we get anything?
  if (jsonBuffer != NULL) {
    jsonBuffer[jsonSize] = '\0'; // null terminate
    serializeJson(doc, jsonBuffer, jsonSize + 1);
    int ret = mqttClient.publish(topic, 2, false, jsonBuffer, strlen(jsonBuffer), false);

    if (ret == -1)
      YBP.printf("[mqtt] Error publishing HA topic %s\n", topic);

    // no leaks
    free(jsonBuffer);
  } else {
    YBP.println("MQTT malloc failed.");
  }
}

void MQTTController::traverseJSON(JsonVariant node, const char* topic_prefix)
{
  static constexpr size_t TOPIC_CAP = 256;
  char topicBuf[TOPIC_CAP] = "";
  strlcpy(topicBuf, topic_prefix, TOPIC_CAP);
  size_t len = strnlen(topicBuf, TOPIC_CAP);

  traverse_impl(node, topicBuf, TOPIC_CAP, len);
}

// ---- Internal helpers -------------------------------------------------------

void MQTTController::append_to_topic(char* buf, size_t& len, size_t cap, const char* piece)
{
  if (!piece || !piece[0])
    return;
  // Add separator if we already have content
  if (len > 0 && buf[len - 1] != '/' && len + 1 < cap) {
    buf[len++] = '/';
  }
  // Append piece (truncate safely if needed)
  while (*piece && len + 1 < cap) {
    buf[len++] = *piece++;
  }
  buf[len] = '\0';
}

void MQTTController::append_index_to_topic(char* buf, size_t& len, size_t cap, size_t index)
{
  char ibuf[16];
  // Enough for size_t up to 64-bit
  int n = snprintf(ibuf, sizeof(ibuf), "%u", static_cast<unsigned>(index));
  (void)n; // silence -Wunused-result
  append_to_topic(buf, len, cap, ibuf);
}

// Convert a primitive JsonVariant to char* payload without String
const char* MQTTController::to_payload(JsonVariant v, char* out, size_t outcap)
{
  if (v.isNull()) {
    // Publish literal "null"
    if (outcap > 0) {
      strncpy(out, "null", outcap - 1);
      out[outcap - 1] = '\0';
      return out;
    }
    return "";
  }

  // Strings: publish raw C-string (no extra quotes)
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    if (!s)
      return "";
    // Copy into out so caller owns stable storage
    if (outcap > 0) {
      strncpy(out, s, outcap - 1);
      out[outcap - 1] = '\0';
      return out;
    }
    return "";
  }

  // Booleans
  if (v.is<bool>()) {
    const char* s = v.as<bool>() ? "true" : "false";
    if (outcap > 0) {
      strncpy(out, s, outcap - 1);
      out[outcap - 1] = '\0';
      return out;
    }
    return "";
  }

  // Integers (prefer widest to avoid overflow)
  if (v.is<long long>()) {
    (void)snprintf(out, outcap, "%lld", v.as<long long>());
    return out;
  }
  if (v.is<unsigned long long>()) {
    (void)snprintf(out, outcap, "%llu", v.as<unsigned long long>());
    return out;
  }
  if (v.is<long>()) {
    (void)snprintf(out, outcap, "%ld", v.as<long>());
    return out;
  }
  if (v.is<unsigned long>()) {
    (void)snprintf(out, outcap, "%lu", v.as<unsigned long>());
    return out;
  }
  if (v.is<int>()) {
    (void)snprintf(out, outcap, "%d", v.as<int>());
    return out;
  }
  if (v.is<unsigned int>()) {
    (void)snprintf(out, outcap, "%u", v.as<unsigned int>());
    return out;
  }

  // Floating point
  if (v.is<double>()) {
    // %.9g gives compact form while preserving good precision
    (void)snprintf(out, outcap, "%.9g", v.as<double>());
    return out;
  }
  if (v.is<float>()) {
    (void)snprintf(out, outcap, "%.7g", v.as<float>());
    return out;
  }

  // Fallback: serialize JSON representation into out (covers other primitive-ish cases)
  // (This will include quotes for strings, but we already handled strings above.)
  serializeJson(v, out, outcap);
  return out;
}

// Depth-first traversal with an in-place topic buffer
void MQTTController::traverse_impl(JsonVariant node, char* topicBuf, size_t cap, size_t curLen)
{
  // YBP.printf("traverse_impl: %s\n", topicBuf);

  // Objects
  if (node.is<JsonObject>()) {
    JsonObject obj = node.as<JsonObject>();
    for (JsonPair kv : obj) { // ArduinoJson v7-compatible
      // Save current length so we can restore after recursion
      size_t savedLen = curLen;

      // Append key
      append_to_topic(topicBuf, curLen, cap, kv.key().c_str());

      // Recurse
      traverse_impl(kv.value(), topicBuf, cap, curLen);

      // Restore topic
      curLen = savedLen;
      topicBuf[curLen] = '\0';
    }
    return;
  }

  // Arrays
  if (node.is<JsonArray>()) {
    JsonArray arr = node.as<JsonArray>();
    size_t idx = 0;
    for (JsonVariant v : arr) {
      size_t savedLen = curLen;
      append_index_to_topic(topicBuf, curLen, cap, idx++);
      traverse_impl(v, topicBuf, cap, curLen);
      curLen = savedLen;
      topicBuf[curLen] = '\0';
    }
    return;
  }

  // Primitive leaf -> publish
  char payload[256];
  const char* data = to_payload(node, payload, sizeof(payload));

  // Ensure non-null topic string (can be empty if caller passed "")
  publish(topicBuf, data);
}