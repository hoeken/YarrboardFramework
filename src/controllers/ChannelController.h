/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_CHANNEL_CONTROLLER_H
#define YARR_CHANNEL_CONTROLLER_H

#include "YarrboardConfig.h"
#include "controllers/BaseController.h"
#include <Arduino.h>

class YarrboardApp;

template <typename ChannelType, size_t COUNT>
class ChannelController : public BaseController
{
  protected:
    etl::array<ChannelType, COUNT> _channels;

  public:
    ChannelController(YarrboardApp& app, const char* name) : BaseController(app, name) {}

    bool loadConfigHook(JsonVariant config) override
    {
      // did we get a config entry?
      if (config[_name]) {
        // populate our exact channel count
        for (byte i = 0; i < N; i++) {
          channels[i].init(i + 1); // load default values per channel.  channels are 1 indexed for humans.
        }

        // now iterate over our initialized channels
        for (auto& ch : channels) {
          bool found = false;

          // loop over our json config to see if we find a match
          for (JsonVariantConst ch_config : config[_name].as<JsonArrayConst>()) {

            // channels are one indexed for humans
            if (ch_config["id"] == ch.id) {

              // did we get a non-empty key?
              const char* val = ch_config["key"].as<const char*>();
              if (val && *val) {
                for (auto& test_ch : channels) {
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
            snprintf(error, len, "Missing 'board.%s' #%d config", channel_key, ch.id);
            return false;
          }
        }
      } else {
        snprintf(error, len, "Missing 'board.%s' config", channel_key);
        return false;
      }

      return true;
    };

    void generateConfigHook(JsonVariant config) override
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

    void mqttUpdateHook() override
    {
      for (auto& ch : _channels) {
        if (ch.isEnabled) {
          ch.mqttUpdate();
        }
      }
    }

    void haUpdateHook() override
    {
      for (auto& ch : _channels) {
        if (ch.isEnabled) {
          ch.haPublishAvailable();
          ch.haPublishState();
        }
      }
    }

    void haGenerateDiscoveryHook(JsonVariant components) override
    {
      for (auto& ch : _channels) {
        if (ch.isEnabled)
          ch.haGenerateDiscovery(components);
      }
    }

    // Channel* getChannelById(uint8_t id, etl::array<Channel, N>& channels)
    // Channel* getChannelByKey(const char* key, etl::array<Channel, N>& channels)
    // Channel* lookupChannel(JsonVariantConst input, JsonVariant output, etl::array<Channel, N>& channels)
};

#endif