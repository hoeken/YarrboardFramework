/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_MQTT_H
#define YARR_MQTT_H

#include "YarrboardConfig.h"
#include "controllers/BaseController.h"
#include <ArduinoJson.h>
#include <PsychicMqttClient.h>

class YarrboardApp;
class ConfigManager;

class MQTTController : public BaseController
{
  public:
    MQTTController(YarrboardApp& app);

    bool setup() override;
    void loop() override;

    void disconnect();
    bool isConnected();

    void onTopic(const char* topic, int qos, OnMessageUserCallback callback);
    void onConnect(bool sessionPresent);
    void publish(const char* topic, const char* payload, bool use_prefix = true);
    void traverseJSON(JsonVariant node, const char* topic_prefix);

  private:
    PsychicMqttClient mqttClient;
    unsigned long previousMQTTMillis = 0;

    void haDiscovery();
    void receiveMessage(const char* topic, const char* payload, int retain, int qos, bool dup);

    // --- THE CALLBACK TRAP ---
    // Libraries expecting C-style function pointers cannot take normal member functions.
    // We use a static instance pointer and static methods to bridge the gap.
    static MQTTController* _instance;
    static void _onConnectStatic(bool sessionPresent);
    static void _receiveMessageStatic(const char* topic, const char* payload, int retain, int qos, bool dup);

    void append_to_topic(char* buf, size_t& len, size_t cap, const char* piece);
    void append_index_to_topic(char* buf, size_t& len, size_t cap, size_t index);
    const char* to_payload(JsonVariant v, char* out, size_t outcap);
    void traverse_impl(JsonVariant node, char* topicBuf, size_t cap, size_t curLen);
};

#endif /* !YARR_MQTT_H */