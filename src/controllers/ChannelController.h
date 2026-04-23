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

#ifndef YARR_CHANNEL_CONTROLLER_H
#define YARR_CHANNEL_CONTROLLER_H

#include "YarrboardApp.h"
#include "YarrboardConfig.h"
#include "YarrboardDebug.h"
#include "channels/BaseChannel.h"
#include "controllers/BaseController.h"
#include <Arduino.h>
#include <type_traits>

template <typename ChannelType, size_t COUNT>
class ChannelController : public BaseController
{
    static_assert(std::is_base_of<BaseChannel, ChannelType>::value,
      "ChannelType must derive from BaseChannel");

  protected:
    etl::array<ChannelType, COUNT> _channels;

  public:
    ChannelController(YarrboardApp& app, const char* name) : BaseController(app, name)
    {
      for (size_t i = 0; i < COUNT; i++)
        _channels[i].init(i + 1);
    }

    etl::array<ChannelType, COUNT>& getChannels()
    {
      return _channels;
    }

    bool sanitizeConfigHook(JsonVariant config, char* error, size_t len)
    {
      bool ret = true;

      JsonArray channels = config["channels"].as<JsonArray>();

      // now iterate over our initialized channels
      for (auto& ch : _channels) {

        // loop over our json config to see if we find a match
        for (size_t i = 0; i < channels.size(); i++) {
          JsonVariant ch_config = channels[i];

          // channels are one indexed for humans
          if (ch_config["id"] == ch.id) {

            // did we get a non-empty key?
            const char* val = ch_config["key"].as<const char*>();
            if (val && *val) {
              for (auto& test_ch : _channels) {
                // did we find any with a different id?
                if (!strcmp(val, test_ch.key) && ch.id != test_ch.id) {
                  channels.remove(i);
                  snprintf(error, len, "%s channel #%d - duplicate key: %d/%s", _name, ch.id, test_ch.id, val);
                  ret = false;
                  break;
                }
              }
            }

            // sanitize each object too
            ch.sanitizeConfig(ch_config, error, len);
          }
        }
      }

      return ret;
    }

    void loadConfigHook(JsonVariant config) override
    {
      JsonArray channels = config["channels"].as<JsonArray>();

      // now iterate over our initialized channels
      for (auto& ch : _channels) {
        for (JsonVariant ch_config : channels) {
          if (ch_config["id"] == ch.id) {
            ch.loadConfig(ch_config);
          }
        }
      }
    }

    void generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose) override
    {
      JsonArray channels = output["channels"].to<JsonArray>();
      for (auto& ch : _channels) {
        JsonObject jo = channels.add<JsonObject>();
        ch.generateConfig(jo, role, purpose);
      }
    };

    void generateCapabilitiesHook(JsonVariant output) override
    {
      output["count"] = _channels.size();
    };

    void handleConfigCommand(JsonVariant input, JsonVariant output)
    {
      char error[128];

      // load our channel
      auto* ch = lookupChannel(input, output);
      if (!ch)
        return;

      if (!input["config"].is<JsonObject>()) {
        snprintf(error, sizeof(error), "'config' is required parameter");
        return _app.protocol.generateErrorJSON(output, error);
      }

      if (!ch->sanitizeConfig(input["config"], error, sizeof(error))) {
        return _app.protocol.generateErrorJSON(output, error);
      }

      ch->loadConfig(input["config"]);

      // write it to file
      if (!_app.config.saveConfig(error, sizeof(error)))
        return _app.protocol.generateErrorJSON(output, error);

      ch->onConfigUpdatedHook();
    }

    void generateUpdateHook(JsonVariant output) override
    {
      JsonArray channels = output[_name].to<JsonArray>();
      for (auto& ch : _channels) {
        JsonObject jo = channels.add<JsonObject>();
        ch.generateUpdate(jo);
      }
    }

    bool needsFastUpdate() override
    {
      for (auto& ch : _channels) {
        if (ch.sendFastUpdate)
          return true;
      }

      return false;
    }

    void generateFastUpdateHook(JsonVariant output) override
    {
      JsonArray channels = output[_name].to<JsonArray>();
      for (auto& ch : _channels) {
        if (ch.sendFastUpdate) {
          JsonObject jo = channels.add<JsonObject>();
          ch.generateUpdate(jo);
          ch.sendFastUpdate = false;
        }
      }
    }

    void mqttUpdateHook(MQTTController* mqtt) override
    {
      for (auto& ch : _channels) {
        if (ch.isEnabled) {
          ch.mqttUpdate(mqtt);
        }
      }
    }

    void haUpdateHook(MQTTController* mqtt) override
    {
      for (auto& ch : _channels) {
        if (ch.isEnabled) {
          ch.haPublishAvailable(mqtt);
          ch.haPublishState(mqtt);
        }
      }
    }

    void haGenerateDiscoveryHook(JsonVariant components, const char* uuid, MQTTController* mqtt) override
    {
      for (auto& ch : _channels) {
        if (ch.isEnabled)
          ch.haGenerateDiscovery(components, uuid, mqtt);
      }
    }

    ChannelType* getChannelById(uint8_t id)
    {
      for (auto& ch : _channels) {
        if (ch.id == id)
          return &ch;
      }
      return nullptr;
    }

    ChannelType* getChannelByKey(const char* key)
    {
      for (auto& ch : _channels) {
        if (ch.key && key && !strcmp(key, ch.key))
          return &ch;
      }
      return nullptr;
    }

    ChannelType* lookupChannel(JsonVariantConst input, JsonVariant output)
    {
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
            ProtocolController::generateErrorJSON(output, "Parameter 'id' must be an integer or numeric string");
            return nullptr;
          }
        } else {
          ProtocolController::generateErrorJSON(output, "Parameter 'id' must be an integer or numeric string");
          return nullptr;
        }

        ChannelType* ch = getChannelById(id);
        if (!ch) {
          ProtocolController::generateErrorJSON(output, "Invalid channel id");
          return nullptr;
        }
        return ch;
      }

      if (!vKey.isNull()) {
        if (!vKey.is<const char*>()) {
          ProtocolController::generateErrorJSON(output, "Parameter 'key' must be a string");
          return nullptr;
        }
        const char* key = vKey.as<const char*>();
        ChannelType* ch = getChannelByKey(key);
        if (!ch) {
          ProtocolController::generateErrorJSON(output, "Invalid channel key");
          return nullptr;
        }
        return ch;
      }

      ProtocolController::generateErrorJSON(output, "You must pass in either 'id' or 'key' as a required parameter");
      return nullptr;
    }
};

#endif