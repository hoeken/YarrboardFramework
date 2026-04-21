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

#include "controllers/MQTTController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"
#include "controllers/ProtocolController.h"

MQTTController* MQTTController::_instance = nullptr;

MQTTController::MQTTController(YarrboardApp& app) : BaseController(app, "mqtt")
{
}

bool MQTTController::setup()
{
  if (!WiFi.isConnected()) {
    YBP.println("WiFi not connected.");
    return false;
  }

  _app.protocol.registerCommand(ADMIN, "set_mqtt_config", this, &MQTTController::handleSetMQTTConfig);

  _instance = this; // Capture the instance for callbacks

  // on connect home hook
  mqttClient.onConnect(_onConnectStatic);

  // on disconnect hook
  mqttClient.onDisconnect(_onDisconnectStatic);

  // on error hook
  mqttClient.onError(_onErrorStatic);

  // home assistant connection discovery hook.
  // NOTE: we set a flag here instead of calling haDiscovery() directly, because
  // haDiscovery() calls mqtt->onTopic() which does push_back on _onMessageUserCallbacks.
  // Calling push_back while PsychicMqttClient::_onMessage is iterating that same vector
  // can trigger a reallocation, invalidating the iterator and causing a crash.
  if (_enable_ha_integration) {
    mqttClient.onTopic("homeassistant/status", 0, [&](const char* topic, const char* payload, int retain, int qos, bool dup) {
      if (!strcmp(payload, "online"))
        _pendingHaDiscovery = true;
    });
  }

  return connect(true);
}

bool MQTTController::connect(bool waitBlocking)
{
  // are we enabled?
  if (!_enable_mqtt)
    return true;

  // this is our first try.
  _firstConnection = true;

  mqttClient.setServer(_mqtt_server);
  mqttClient.setCredentials(_mqtt_user, _mqtt_pass);

  if (_mqtt_cert.length())
    mqttClient.setCACert(_mqtt_cert.c_str());

  mqttClient.connect();

  /**
   * Wait blocking until the connection is established
   */
  if (waitBlocking) {
    int tries = 0;
    while (!mqttClient.connected()) {
      vTaskDelay(pdMS_TO_TICKS(100));
      tries++;

      if (tries > 20) {
        mqttClient.forceStop();
        YBP.println("MQTT failed to connect.");
        return false;
      }
    }
  }

  return true;
}

void MQTTController::loop()
{
  if (!mqttClient.connected())
    return;

  // deferred HA discovery: triggered by homeassistant/status callback but run
  // here (outside _onMessage iteration) to avoid invalidating the callback vector.
  if (_pendingHaDiscovery && _enable_ha_integration) {
    _pendingHaDiscovery = false;
    haDiscovery();
  }

  // periodically update our mqtt / HomeAssistant status
  unsigned int messageDelta = millis() - previousMQTTMillis;
  if (messageDelta >= 1000) {
    if (mqttClient.connected()) {

      for (const auto& entry : _app.getControllers()) {
        entry.controller->mqttUpdateHook(this);
      }

      // separately update our Home Assistant status
      if (_enable_ha_integration) {
        for (const auto& entry : _app.getControllers()) {
          entry.controller->haUpdateHook(this);
        }
      }
    }

    previousMQTTMillis = millis();
  }
}

void MQTTController::handleSetMQTTConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  _enable_mqtt = input["app_enable_mqtt"];
  _enable_mqtt_protocol = input["app_enable_mqtt_protocol"];
  _enable_ha_integration = input["app_enable_ha_integration"];
  _use_hostname_as_mqtt_uuid = input["app_use_hostname_as_mqtt_uuid"];

  strlcpy(_mqtt_server, input["mqtt_server"] | "", sizeof(_mqtt_server));
  strlcpy(_mqtt_user, input["mqtt_user"] | "", sizeof(_mqtt_user));
  strlcpy(_mqtt_pass, input["mqtt_pass"] | "", sizeof(_mqtt_pass));
  _mqtt_cert = input["mqtt_cert"].as<String>();

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return _app.protocol.generateErrorJSON(output, error);

  // init our mqtt
  if (_enable_mqtt) {
    disconnect(); // reset our connection.
    if (!connect())
      return _app.protocol.generateErrorJSON(output, "Error connecting to MQTT server.");
  } else
    disconnect();
}

void MQTTController::generateStatsHook(JsonVariant output)
{
  output["mqtt_connected"] = _app.mqtt.isConnected();
}

bool MQTTController::loadMQTTConfig(JsonVariant config, char* error, size_t len)
{
  const char* v;

  v = config["mqtt_server"] | "";
  strlcpy(_mqtt_server, v, sizeof(_mqtt_server));

  v = config["mqtt_user"] | "";
  strlcpy(_mqtt_user, v, sizeof(_mqtt_user));

  v = config["mqtt_pass"] | "";
  strlcpy(_mqtt_pass, v, sizeof(_mqtt_pass));

  _mqtt_cert = config["mqtt_cert"] | "";

  _enable_mqtt = config["app_enable_mqtt"] | _app.enable_mqtt;
  _enable_mqtt_protocol = config["app_enable_mqtt_protocol"] | _app.enable_mqtt_protocol;
  _enable_ha_integration = config["app_enable_ha_integration"] | _app.enable_ha_integration;
  _use_hostname_as_mqtt_uuid = config["app_use_hostname_as_mqtt_uuid"] | _app.use_hostname_as_mqtt_uuid;

  return true;
}

void MQTTController::generateMQTTConfig(JsonVariant output)
{
  output["app_enable_mqtt"] = _enable_mqtt;
  output["app_enable_mqtt_protocol"] = _enable_mqtt_protocol;
  output["app_enable_ha_integration"] = _enable_ha_integration;
  output["app_use_hostname_as_mqtt_uuid"] = _use_hostname_as_mqtt_uuid;
  output["mqtt_server"] = _mqtt_server;
  output["mqtt_user"] = _mqtt_user;
  output["mqtt_pass"] = _mqtt_pass;
  output["mqtt_cert"] = _mqtt_cert;
}

void MQTTController::disconnect()
{
  if (mqttClient.connected())
    mqttClient.forceStop();
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
    sprintf(mqtt_path, "yarrboard/%s/%s", _app.network.getLocalHostname(), topic);
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
  // only if we're enabled.
  if (!_enable_mqtt_protocol)
    return;

  JsonDocument input;
  DeserializationError err = deserializeJson(input, payload);
  JsonDocument output;

  if (err) {
    char error[64];
    sprintf(error, "deserializeJson() failed with code %s", err.c_str());
    _app.protocol.generateErrorJSON(output, error);
  } else {
    ProtocolContext context;
    context.mode = YBP_MODE_MQTT;
    _app.protocol.handleReceivedJSON(input, output, context);
  }

  // we can have empty responses
  if (output.size()) {
    // dynamically allocate our buffer
    size_t jsonSize = measureJson(output);
    char* jsonBuffer = (char*)malloc(jsonSize + 1);

    // did we get anything?
    if (jsonBuffer != NULL) {
      jsonBuffer[jsonSize] = '\0'; // null terminate
      serializeJson(output, jsonBuffer, jsonSize + 1);

      // post our response
      this->publish("response", jsonBuffer);
      free(jsonBuffer);
    } else {
      // dont call YBP b/c loops...
      YBP.println("Error allocating in MQTTController::receiveMessage");
    }
  }
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

  // clear first connection flag on successful connection
  _firstConnection = false;

  if (_enable_ha_integration)
    haDiscovery();

  // Register the command topic once; PsychicMqttClient resubscribes automatically
  // on reconnect so there is no need to add a duplicate entry every reconnect.
  if (!_commandTopicRegistered) {
    char mqtt_path[128];
    sprintf(mqtt_path, "yarrboard/%s/command", _app.network.getLocalHostname());
    mqttClient.onTopic(mqtt_path, 0, _receiveMessageStatic);
    _commandTopicRegistered = true;
  }
}

void MQTTController::_onConnectStatic(bool sessionPresent)
{
  if (_instance) {
    _instance->onConnect(sessionPresent);
  }
}

void MQTTController::onDisconnect(bool sessionPresent)
{
  YBP.println("Disconnected from MQTT.");
}

void MQTTController::_onDisconnectStatic(bool sessionPresent)
{
  if (_instance) {
    _instance->onDisconnect(sessionPresent);
  }
}

void MQTTController::onError(esp_mqtt_error_codes_t error)
{
  YBP.printf("⚠️ MQTT Error: %d\n", error.error_type);

  // if this is the first connection attempt and we got an error
  if (_firstConnection) {
    // we dont want to keep trying
    disconnect();

    // send error message to ADMIN
    JsonDocument output;
    char errorMsg[128];
    sprintf(errorMsg, "MQTT connection error: %d", error.error_type);
    _app.protocol.generateErrorJSON(output, errorMsg);

    // send to all ADMIN clients
    _app.protocol.sendToAll(output.as<JsonVariantConst>(), ADMIN);
  }
}

void MQTTController::_onErrorStatic(esp_mqtt_error_codes_t error)
{
  if (_instance) {
    _instance->onError(error);
  }
}

const char* MQTTController::getBoardKey()
{
  if (_use_hostname_as_mqtt_uuid)
    return _app.network.getLocalHostname();
  else
    return _app.network.getUUID();
}

void MQTTController::haDiscovery()
{
  if (!mqttClient.connected())
    return;

  // how to structure our id?
  char ha_dev_uuid[128];
  sprintf(ha_dev_uuid, "yarrboard_%s", getBoardKey());

  char topic[128];
  sprintf(topic, "homeassistant/device/%s/config", ha_dev_uuid);

  // this is our device information.
  JsonDocument doc;
  JsonObject device = doc["dev"].to<JsonObject>();
  device["ids"] = ha_dev_uuid;
  device["name"] = _cfg.getBoardName();
  device["mf"] = _app.manufacturer;
  device["mdl"] = _app.hardware_version;
  device["sw"] = _app.firmware_version;
  device["sn"] = _app.network.getUUID();
  char config_url[128];
  sprintf(config_url, "http://%s.local", _app.network.getLocalHostname());
  device["configuration_url"] = config_url;

  // our origin to let HA know where it came from.
  JsonObject origin = doc["o"].to<JsonObject>();
  origin["name"] = "yarrboard";
  origin["sw"] = _app.firmware_version;
  origin["url"] = "https://github.com/hoeken/yarrboard-firmware";

  // our components array
  JsonObject components = doc["cmps"].to<JsonObject>();

  for (const auto& entry : _app.getControllers()) {
    entry.controller->haGenerateDiscoveryHook(components, ha_dev_uuid, this);
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