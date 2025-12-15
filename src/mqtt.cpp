/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "mqtt.h"
#include "YarrboardDebug.h"

#ifdef YB_HAS_ADC_CHANNELS
  #include "adc_channel.h"
#endif

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

PsychicMqttClient mqttClient;

unsigned long previousMQTTMillis = 0;

void mqtt_setup()
{
  // are we enabled?
  if (!app_enable_mqtt)
    return;

  mqttClient.setServer(mqtt_server);
  mqttClient.setCredentials(mqtt_user, mqtt_pass);

  if (mqtt_cert.length())
    mqttClient.setCACert(mqtt_cert.c_str());

  // on connect home hook
  mqttClient.onConnect(onMqttConnect);

  // home assistant connection discovery hook.
  if (app_enable_ha_integration) {
    mqttClient.onTopic("homeassistant/status", 0, [&](const char* topic, const char* payload, int retain, int qos, bool dup) {
      if (!strcmp(payload, "online"))
        mqtt_ha_discovery();
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
      return;
    }
  }
}

void mqtt_loop()
{
  if (!mqttClient.connected())
    return;

  // periodically update our mqtt / HomeAssistant status
  unsigned int messageDelta = millis() - previousMQTTMillis;
  if (messageDelta >= 1000) {
    if (mqttClient.connected()) {

#ifdef YB_HAS_ADC_CHANNELS
      mqtt_update_channels(adc_channels);
#endif

#ifdef YB_HAS_PWM_CHANNELS
      mqtt_update_channels(pwm_channels);
#endif

#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
      mqtt_update_channels(digital_input_channels);
#endif

#ifdef YB_HAS_RELAY_CHANNELS
      mqtt_update_channels(relay_channels);
#endif

#ifdef YB_HAS_SERVO_CHANNELS
      mqtt_update_channels(servo_channels);
#endif

#ifdef YB_HAS_STEPPER_CHANNELS
      mqtt_update_channels(stepper_channels);
#endif

#ifdef YB_IS_BRINEOMATIC
      wm.updateMQTT();
#endif

      // separately update our Home Assistant status
      if (app_enable_ha_integration) {
#ifdef YB_HAS_PWM_CHANNELS
        ha_update_channels(pwm_channels);
#endif
#ifdef YB_HAS_RELAY_CHANNELS
        ha_update_channels(relay_channels);
#endif
      }
    }

    previousMQTTMillis = millis();
  }
}

void mqtt_disconnect()
{
  if (mqttClient.connected())
    mqttClient.disconnect();
}

bool mqtt_is_connected()
{
  return mqttClient.connected();
}

void mqtt_on_topic(const char* topic, int qos, OnMessageUserCallback callback)
{
  mqttClient.onTopic(topic, qos, callback);
}

void mqtt_publish(const char* topic, const char* payload, bool use_prefix)
{
  if (!mqttClient.connected())
    return;

  int ret;

  // prefix it with yarrboard or nah?
  if (use_prefix) {
    char mqtt_path[256];
    sprintf(mqtt_path, "yarrboard/%s/%s", config.local_hostname, topic);
    ret = mqttClient.publish(mqtt_path, 0, 0, payload, strlen(payload), false);
    if (ret == -1)
      YBP.printf("[mqtt] Error publishing prefix path %s\n", mqtt_path);
  } else {
    ret = mqttClient.publish(topic, 0, 0, payload, strlen(payload), false);
    if (ret == -1)
      YBP.printf("[mqtt] Error publishing topic %s\n", topic);
  }
}

void mqtt_receive_message(const char* topic, const char* payload, int retain, int qos, bool dup)
{
  YBP.printf("Received Topic: %s\r\n", topic);
  YBP.printf("Received Payload: %s\r\n", payload);
}

void onMqttConnect(bool sessionPresent)
{
  YBP.println("Connected to MQTT.");

  if (app_enable_ha_integration)
    mqtt_ha_discovery();

  // look for json messages on this path...
  char mqtt_path[128];
  sprintf(mqtt_path, "yarrboard/%s/command", config.local_hostname);
  mqttClient.onTopic(mqtt_path, 0, mqtt_receive_message);
}

void mqtt_ha_discovery()
{
  if (!mqttClient.connected())
    return;

  // how to structure our id?
  char ha_dev_uuid[128];
  if (app_use_hostname_as_mqtt_uuid)
    sprintf(ha_dev_uuid, "yarrboard_%s", config.local_hostname);
  else
    sprintf(ha_dev_uuid, "yarrboard_%s", config.uuid);

  char topic[128];
  sprintf(topic, "homeassistant/device/%s/config", ha_dev_uuid);

  // this is our device information.
  JsonDocument doc;
  JsonObject device = doc["dev"].to<JsonObject>();
  device["ids"] = ha_dev_uuid;
  device["name"] = board_name;
  device["mf"] = YB_MANUFACTURER;
  device["mdl"] = YB_HARDWARE_VERSION;
  device["sw"] = YB_FIRMWARE_VERSION;
  device["sn"] = config.uuid;
  char config_url[128];
  sprintf(config_url, "http://%s.local", config.local_hostname);
  device["configuration_url"] = config_url;

  // our origin to let HA know where it came from.
  JsonObject origin = doc["o"].to<JsonObject>();
  origin["name"] = "yarrboard";
  origin["sw"] = YB_FIRMWARE_VERSION;
  origin["url"] = "https://github.com/hoeken/yarrboard-firmware";

  // our components array
  JsonObject components = doc["cmps"].to<JsonObject>();

// let each pwm channel create its own config
#ifdef YB_HAS_PWM_CHANNELS
  ha_generate_channels_discovery(pwm_channels, components);
#endif

// let each pwm channel create its own config
#ifdef YB_HAS_RELAY_CHANNELS
  ha_generate_channels_discovery(relay_channels, components);
#endif

// let each pwm channel create its own config
#ifdef YB_HAS_DIGITAL_INPUT_CHANNELS
  ha_generate_channels_discovery(digital_input_channels, components);
#endif

// let each pwm channel create its own config
#ifdef YB_HAS_ADC_CHANNELS
  ha_generate_channels_discovery(adc_channels, components);
#endif

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

// ---- Internal helpers -------------------------------------------------------

static inline void append_to_topic(char* buf, size_t& len, size_t cap,
  const char* piece)
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

static inline void append_index_to_topic(char* buf, size_t& len, size_t cap,
  size_t index)
{
  char ibuf[16];
  // Enough for size_t up to 64-bit
  int n = snprintf(ibuf, sizeof(ibuf), "%u", static_cast<unsigned>(index));
  (void)n; // silence -Wunused-result
  append_to_topic(buf, len, cap, ibuf);
}

// Convert a primitive JsonVariant to char* payload without String
static inline const char* to_payload(JsonVariant v, char* out, size_t outcap)
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
static void traverse_impl(JsonVariant node, char* topicBuf, size_t cap, size_t curLen)
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
  mqtt_publish(topicBuf, data);
}

void mqtt_traverse_json(JsonVariant node, const char* topic_prefix)
{
  static constexpr size_t TOPIC_CAP = 256;
  char topicBuf[TOPIC_CAP] = "";
  strlcpy(topicBuf, topic_prefix, TOPIC_CAP);
  size_t len = strnlen(topicBuf, TOPIC_CAP);

  traverse_impl(node, topicBuf, TOPIC_CAP, len);
}