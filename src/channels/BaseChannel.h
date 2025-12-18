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
#include "controllers/ProtocolController.h"
#include "etl/array.h"
#include <cstring> // for strncpy

class BaseChannel
{
  public:
    byte id = 0;
    bool isEnabled = true;
    bool haEnabled = false;
    char name[YB_CHANNEL_NAME_LENGTH];
    char key[YB_CHANNEL_KEY_LENGTH];
    volatile bool sendFastUpdate = false;

    void setup();

    void setName(const char* name);
    void setKey(const char* key);

    virtual void init(uint8_t id);
    virtual bool loadConfig(JsonVariantConst config, char* error, size_t err_size);
    virtual void generateConfig(JsonVariant config);
    virtual void generateUpdate(JsonVariant output);
    virtual void generateStats(JsonVariant output);

    virtual void haGenerateDiscovery(JsonVariant doc, const char* uuid, MQTTController* mqtt);
    virtual void haPublishAvailable(MQTTController* mqtt);
    virtual void haPublishState(MQTTController* mqtt);
    void mqttUpdate(MQTTController* mqtt);

  protected:
    char ha_key[YB_HOSTNAME_LENGTH];
    char ha_uuid[64];
    char ha_topic_avail[128];
    const char* channel_type = "base";
};

#endif /* !YARR_BASE_CHANNEL_H */