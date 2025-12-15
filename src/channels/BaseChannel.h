/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_BASE_CHANNEL_H
#define YARR_BASE_CHANNEL_H

#include "ArduinoJson.h"
#include "YarrboardConfig.h"
#include "etl/array.h"
#include "mqtt.h"
#include <cstring> // for strncpy

class BaseChannel
{
  public:
    byte id = 0;
    bool isEnabled = true;
    bool haEnabled = false;
    char name[YB_CHANNEL_NAME_LENGTH];
    char key[YB_CHANNEL_KEY_LENGTH];

    void setup();

    void setName(const char* name);
    void setKey(const char* key);

    virtual void init(uint8_t id);
    virtual bool loadConfig(JsonVariantConst config, char* error, size_t err_size);
    virtual void generateConfig(JsonVariant config);
    virtual void generateUpdate(JsonVariant config);

    virtual void haGenerateDiscovery(JsonVariant doc);
    virtual void haPublishAvailable();
    virtual void haPublishState();

    void mqttUpdate();

  protected:
    char ha_key[YB_HOSTNAME_LENGTH];
    char ha_uuid[64];
    char ha_topic_avail[128];
    const char* channel_type = "base";
};

template <typename Channel, size_t N>
Channel* getChannelById(uint8_t id, etl::array<Channel, N>& channels)
{
  static_assert(std::is_base_of<BaseChannel, Channel>::value,
    "Channel must derive from BaseChannel");

  for (auto& ch : channels) {
    if (ch.id == id)
      return &ch;
  }
  return nullptr;
}

template <typename Channel, size_t N>
Channel* getChannelByKey(const char* key, etl::array<Channel, N>& channels)
{
  static_assert(std::is_base_of<BaseChannel, Channel>::value,
    "Channel must derive from BaseChannel");

  for (auto& ch : channels) {
    if (!strcmp(key, ch.key))
      return &ch;
  }
  return nullptr;
}

template <typename Channel, size_t N>
Channel* lookupChannel(JsonVariantConst input, JsonVariant output, etl::array<Channel, N>& channels)
{
  static_assert(std::is_base_of<BaseChannel, Channel>::value,
    "Channel must derive from BaseChannel");

  // Prefer 'id' if present
  JsonVariantConst vId = input["id"];
  JsonVariantConst vKey = input["key"];

  if (!vId.isNull()) {
    unsigned int id = 0;

    if (vId.is<unsigned int>()) {
      // direct integer
      id = vId.as<unsigned int>();
    } else if (vId.is<const char*>()) {
      // string, attempt to parse
      const char* idStr = vId.as<const char*>();
      char* endPtr = nullptr;
      id = strtoul(idStr, &endPtr, 10);
      if (endPtr == idStr || *endPtr != '\0') {
        generateErrorJSON(output, "Parameter 'id' must be an integer or numeric string");
        return nullptr;
      }
    } else {
      generateErrorJSON(output, "Parameter 'id' must be an integer or numeric string");
      return nullptr;
    }

    Channel* ch = getChannelById(id, channels);
    if (!ch) {
      generateErrorJSON(output, "Invalid channel id");
      return nullptr;
    }
    return ch;
  }

  if (!vKey.isNull()) {
    if (!vKey.is<const char*>()) {
      generateErrorJSON(output, "Parameter 'key' must be a string");
      return nullptr;
    }
    const char* key = vKey.as<const char*>();
    Channel* ch = getChannelByKey(key, channels);
    if (!ch) {
      generateErrorJSON(output, "Invalid channel key");
      return nullptr;
    }
    return ch;
  }

  generateErrorJSON(output, "You must pass in either 'id' or 'key' as a required parameter");
  return nullptr;
}

template <typename Channel, size_t N>
void mqtt_update_channels(etl::array<Channel, N>& channels)
{
  static_assert(std::is_base_of<BaseChannel, Channel>::value,
    "Channel must derive from BaseChannel");

  for (auto& ch : channels) {
    if (ch.isEnabled) {
      ch.mqttUpdate();
    }
  }
}

template <typename Channel, size_t N>
void ha_update_channels(etl::array<Channel, N>& channels)
{
  static_assert(std::is_base_of<BaseChannel, Channel>::value,
    "Channel must derive from BaseChannel");

  for (auto& ch : channels) {
    if (ch.isEnabled) {
      ch.haPublishAvailable();
      ch.haPublishState();
    }
  }
}

template <typename Channel, size_t N>
void ha_generate_channels_discovery(etl::array<Channel, N>& channels, JsonVariant components)
{
  static_assert(std::is_base_of<BaseChannel, Channel>::value,
    "Channel must derive from BaseChannel");

  for (auto& ch : channels) {
    if (ch.isEnabled)
      ch.haGenerateDiscovery(components);
  }
}
#endif /* !YARR_BASE_CHANNEL_H */