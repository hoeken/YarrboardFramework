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

#ifndef YARR_MQTT_H
#define YARR_MQTT_H

#include "YarrboardConfig.h"
#include "controllers/BaseController.h"
#include "controllers/ProtocolController.h"
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

    bool connect();
    void disconnect();
    bool isConnected();

    void onTopic(const char* topic, int qos, OnMessageUserCallback callback);
    void onConnect(bool sessionPresent);
    void publish(const char* topic, const char* payload, bool use_prefix = true);
    void traverseJSON(JsonVariant node, const char* topic_prefix);

    void handleSetMQTTConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void generateStatsHook(JsonVariant output) override;

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