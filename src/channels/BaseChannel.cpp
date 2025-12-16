/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#include "channels/BaseChannel.h"
#include "MQTTController.h"
#include "YarrboardDebug.h"

void BaseChannel::init(uint8_t id)
{
  this->id = id;
  this->isEnabled = true;
  snprintf(this->name, sizeof(this->name), "Channel %d", id);
  snprintf(this->key, sizeof(this->key), "%d", id);
}

void BaseChannel::setup()
{
}

void BaseChannel::setName(const char* name)
{
  strncpy(this->name, name, sizeof(this->name));
}

void BaseChannel::setKey(const char* key)
{
  strncpy(this->key, key, sizeof(this->key));
}

bool BaseChannel::loadConfig(JsonVariantConst config, char* error, size_t err_size)
{
  // we need a valid object
  if (!config.is<JsonObjectConst>()) {
    strlcpy(error, "No JsonObject passed to loadConfig()", err_size);
    return false;
  }

  // we need a valid id
  if (config["id"])
    this->id = (int)config["id"];
  else {
    strlcpy(error, "'id' is a required parameter for channel config.", err_size);
    return false;
  }

  // enabled.  missing defaults to false
  this->isEnabled = false;
  if (config["enabled"].is<bool>()) {
    this->isEnabled = config["enabled"];
  }

  // every channel has a name
  snprintf(this->name, sizeof(this->name), "Channel %d", this->id);
  if (config["name"]) {
    if (strlen(config["name"]) >= sizeof(this->name)) {
      snprintf(error, err_size, "Channel name max length %d characters", sizeof(this->name) - 1);
      return false;
    }
    strlcpy(this->name, config["name"], sizeof(this->name));
  }

  // key - must not be an empty string.
  snprintf(this->key, sizeof(this->key), "%d", this->id);
  const char* val = config["key"].as<const char*>();
  if (val && *val && strlen(val)) {
    if (strlen(config["key"]) >= sizeof(this->key)) {
      snprintf(error, err_size, "Channel key max length %d characters", sizeof(this->key) - 1);
      return false;
    }

    // gotta be alphanumeric plus dash or underscore
    for (const char* p = val; *p; p++) {
      if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_')) {
        snprintf(error, err_size, "Channel key contains invalid character: '%c'", *p);
        return false;
      }
    }

    // okay save it to our object
    strlcpy(this->key, val, sizeof(this->key));
  }

  return true;
}

void BaseChannel::generateConfig(JsonVariant config)
{
  config["id"] = this->id;
  config["name"] = this->name;
  config["key"] = this->key;
  config["enabled"] = this->isEnabled;
}

void BaseChannel::generateUpdate(JsonVariant config)
{
  config["id"] = this->id;
  config["key"] = this->key;
}

void BaseChannel::mqttUpdate()
{
  // JsonDocument output;
  // this->generateUpdate(output);

  // char topic[128];
  // snprintf(topic, sizeof(topic), "%s/%s", this->channel_type, this->key);
  // traverseJSON(output, topic);
}

void BaseChannel::haGenerateDiscovery(JsonVariant doc)
{
  // if (config.app_use_hostname_as_mqtt_uuid)
  //   strncpy(ha_key, config.local_hostname, sizeof(key));
  // else
  //   strncpy(ha_key, config.uuid, sizeof(key));

  // // generate our id / topics
  // sprintf(ha_uuid, "%s_%s_%s", ha_key, channel_type, this->key);
  // sprintf(ha_topic_avail, "yarrboard/%s/%s/%s/ha_availability", ha_key, channel_type, this->key);
}

void BaseChannel::haPublishAvailable()
{
  // mqtt_publish(ha_topic_avail, "online", false);
}

void BaseChannel::haPublishState()
{
  return;
}