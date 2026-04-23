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

#include "channels/BaseChannel.h"
#include "YarrboardDebug.h"
#include "controllers/MQTTController.h"

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

bool BaseChannel::sanitizeConfig(JsonVariant config, char* error, size_t err_size)
{
  bool valid = true;

  if (!config.is<JsonObject>()) {
    strlcpy(error, "No JsonObject passed to sanitizeConfig()", err_size);
    return false;
  }

  if (!config["id"]) {
    strlcpy(error, "'id' is a required parameter for channel config.", err_size);
    valid = false;
  }

  if (config["name"]) {
    if (strlen(config["name"].as<const char*>()) >= sizeof(this->name)) {
      snprintf(error, err_size, "Channel name max length %d characters", sizeof(this->name) - 1);
      config.remove("name");
      valid = false;
    }
  }

  const char* val = config["key"].as<const char*>();
  if (val && *val) {
    bool keyValid = true;
    if (strlen(val) >= sizeof(this->key)) {
      snprintf(error, err_size, "Channel key max length %d characters", sizeof(this->key) - 1);
      keyValid = false;
    } else {
      for (const char* p = val; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_')) {
          snprintf(error, err_size, "Channel key contains invalid character: '%c'", *p);
          keyValid = false;
          break;
        }
      }
    }
    if (!keyValid) {
      config.remove("key");
      valid = false;
    }
  }

  return valid;
}

void BaseChannel::loadConfig(JsonVariantConst config)
{
  if (config["id"])
    this->id = (int)config["id"];

  this->isEnabled = false;
  if (config["enabled"].is<bool>())
    this->isEnabled = config["enabled"];

  snprintf(this->name, sizeof(this->name), "Channel %d", this->id);
  if (config["name"])
    strlcpy(this->name, config["name"], sizeof(this->name));

  snprintf(this->key, sizeof(this->key), "%d", this->id);
  const char* val = config["key"].as<const char*>();
  if (val && *val)
    strlcpy(this->key, val, sizeof(this->key));
}

void BaseChannel::generateConfig(JsonVariant config, UserRole role, ConfigPurpose purpose)
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

void BaseChannel::generateStats(JsonVariant config)
{
}

void BaseChannel::mqttUpdate(MQTTController* mqtt)
{
  JsonDocument output;
  this->generateUpdate(output);

  char topic[128];
  snprintf(topic, sizeof(topic), "%s/%s", this->channel_type, this->key);
  mqtt->traverseJSON(output, topic);
}

void BaseChannel::haGenerateDiscovery(JsonVariant doc, const char* uuid, MQTTController* mqtt)
{
  // how to structure our id?
  sprintf(ha_key, "%s", mqtt->getBoardKey());

  // generate our id / topics
  sprintf(ha_uuid, "%s_%s_%s", uuid, channel_type, this->key);
  sprintf(ha_topic_avail, "yarrboard/%s/%s/%s/ha_availability", ha_key, channel_type, this->key);
}

void BaseChannel::haPublishAvailable(MQTTController* mqtt)
{
  mqtt->publish(ha_topic_avail, "online", false);
}

void BaseChannel::haPublishState(MQTTController* mqtt)
{
  return;
}