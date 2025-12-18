/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_CHANNEL_CONTROLLER_H
#define YARR_CHANNEL_CONTROLLER_H

#include "YarrboardConfig.h"
#include "YarrboardDebug.h"
#include "controllers/BaseController.h"
#include <Arduino.h>

class YarrboardApp;

template <typename ChannelType, size_t COUNT>
class ChannelController : public BaseController
{
  protected:
    etl::array<ChannelType, COUNT> _channels;

  public:
    ChannelController(YarrboardApp& app, const char* name) : BaseController(app, name)
    {
      // init everything with defaults
      byte i = 0;
      for (auto& ch : _channels) {
        ch.init(i + 1);
        i++;
      }
    }

    etl::array<ChannelType, COUNT>& getChannels()
    {
      return _channels;
    }

    bool loadConfigHook(JsonVariant config, char* error, size_t len) override
    {
      // did we get a config entry?
      if (config[_name]) {

        // now iterate over our initialized channels
        for (auto& ch : _channels) {
          bool found = false;

          // loop over our json config to see if we find a match
          for (JsonVariantConst ch_config : config[_name].as<JsonArrayConst>()) {

            // channels are one indexed for humans
            if (ch_config["id"] == ch.id) {

              // did we get a non-empty key?
              const char* val = ch_config["key"].as<const char*>();
              if (val && *val) {
                for (auto& test_ch : _channels) {
                  // did we find any with a different id?
                  if (!strcmp(val, test_ch.key) && ch.id != test_ch.id) {
                    snprintf(error, len, "%s channel #%d - duplicate key: %d/%s", _name, ch.id, test_ch.id, val);
                    return false;
                  }
                }
              }

              // okay, attempt to load our config.
              if (ch.loadConfig(ch_config, error, len))
                found = true;
              else
                return false;
            }
          }

          if (!found) {
            snprintf(error, len, "Missing 'board.%s' #%d config", _name, ch.id);
            return false;
          }
        }
      } else {
        snprintf(error, len, "Missing 'board.%s' config", _name);
        return false;
      }

      return true;
    };

    void generateConfigHook(JsonVariant output) override
    {
      JsonArray channels = output[_name].to<JsonArray>();
      for (auto& ch : _channels) {
        JsonObject jo = channels.add<JsonObject>();
        ch.generateConfig(jo);
      }
    };

    void generateUpdateHook(JsonVariant output) override
    {
      JsonArray channels = output[_name].to<JsonArray>();
      for (auto& ch : _channels) {
        JsonObject jo = channels.add<JsonObject>();
        ch.generateUpdate(jo);
      }
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

    void generateStatsHook(JsonVariant output) override
    {
      // info about each of our channels
      JsonArray channels = output[_name].to<JsonArray>();
      for (auto& ch : _channels) {
        JsonObject jo = channels.add<JsonObject>();
        ch.generateStats(jo);
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
      static_assert(std::is_base_of<BaseChannel, ChannelType>::value,
        "ChannelType must derive from BaseChannel");

      for (auto& ch : _channels) {
        if (ch.id == id)
          return &ch;
      }
      return nullptr;
    }

    ChannelType* getChannelByKey(const char* key)
    {
      static_assert(std::is_base_of<BaseChannel, ChannelType>::value,
        "ChannelType must derive from BaseChannel");

      for (auto& ch : _channels) {
        if (ch.key && key && !strcmp(key, ch.key))
          return &ch;
      }
      return nullptr;
    }

    ChannelType* lookupChannel(JsonVariantConst input, JsonVariant output)
    {
      static_assert(std::is_base_of<BaseChannel, ChannelType>::value,
        "ChannelType must derive from BaseChannel");

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