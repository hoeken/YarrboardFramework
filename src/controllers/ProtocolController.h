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

#ifndef YARR_PROTOCOL_H
#define YARR_PROTOCOL_H

#include "YarrboardConfig.h"
#include "controllers/AuthController.h"
#include "controllers/BaseController.h"
#include "utility.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <cstring>
#include <etl/map.h>
#include <functional>

typedef enum {
  YBP_MODE_NONE,
  YBP_MODE_WEBSOCKET,
  YBP_MODE_HTTP,
  YBP_MODE_SERIAL
} YBMode;

class YarrboardApp;
class ConfigManager;

struct ProtocolContext {
    YBMode mode = YBP_MODE_NONE;
    UserRole role = NOBODY;
    uint32_t clientId = 0;
};

// message handler callback definition
// void(JsonVariantConst input, JsonVariant output)
using ProtocolMessageHandler = std::function<void(JsonVariantConst, JsonVariant, ProtocolContext)>;

class ProtocolController : public BaseController
{
  public:
    ProtocolController(YarrboardApp& app);

    bool setup() override;
    void loop() override;

    bool unregisterCommand(const char* command);
    bool hasCommand(const char* command);
    void printCommands();

    // Overload 1: Free Functions, Static Functions, Lambdas
    // Accepts any callable that matches the signature
    bool registerCommand(UserRole role, const char* command, ProtocolMessageHandler handler);

    // Overload 2: Member Function Helper
    template <typename T>
    bool registerCommand(UserRole role, const char* command, T* instance, void (T::*method)(JsonVariantConst, JsonVariant, ProtocolContext))
    {
      // No casting needed on 'this'. We use the explicitly passed 'instance'.
      return registerCommand(role, command, [instance, method](JsonVariantConst in, JsonVariant out, ProtocolContext context) {
        (instance->*method)(in, out, context);
      });
    }

    void sendBrightnessUpdate();
    void sendThemeUpdate();
    void sendFastUpdate();
    void sendOTAProgressUpdate(float progress);
    void sendOTAProgressFinished();
    void sendDebug(const char* message);
    void sendToAll(JsonVariantConst output, UserRole auth_level);
    void sendToAll(const char* jsonString, UserRole auth_level);

    void handleReceivedJSON(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    static void generateErrorJSON(JsonVariant output, const char* error);
    static void generateSuccessJSON(JsonVariant output, const char* success);

    void incrementSentMessages();

  private:
    unsigned long previousMessageMillis = 0;
    unsigned int receivedMessages = 0;
    unsigned int receivedMessagesPerSecond = 0;
    unsigned long totalReceivedMessages = 0;
    unsigned int sentMessages = 0;
    unsigned int sentMessagesPerSecond = 0;
    unsigned long totalSentMessages = 0;

    // -------------------------------------------------------------------------
    // Dynamic command handler registry
    // -------------------------------------------------------------------------
    struct CommandEntry {
        UserRole role;
        ProtocolMessageHandler handler;
    };

    // This tells the Map to compare the TEXT, not the memory addresses.
    struct StringCompare {
        bool operator()(const char* lhs, const char* rhs) const
        {
          return strcmp(lhs, rhs) < 0;
        }
    };

    // Command map - list of allowed commands, required role, and their callbacks
    etl::map<const char*, CommandEntry, YB_PROTOCOL_MAX_COMMANDS, StringCompare> commandMap;

    void handleSerialJson();

    void handleHello(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleLogin(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleLogout(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handlePing(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleGetConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleGetStats(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleGetUpdate(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleGetFullConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleGetNetworkConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleGetAppConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetGeneralConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetNetworkConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetAuthenticationConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetWebServerConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetMQTTConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetMiscellaneousConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSaveConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleRestart(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleCrashMe(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleFactoryReset(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleOTAStart(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetTheme(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetBrightness(JsonVariantConst input, JsonVariant output, ProtocolContext context);

    void generateConfigMessage(JsonVariant output);
};

#endif /* !YARR_PROTOCOL_H */