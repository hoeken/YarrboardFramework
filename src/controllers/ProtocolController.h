/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
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
  YBP_MODE_WEBSOCKET,
  YBP_MODE_HTTP,
  YBP_MODE_SERIAL
} YBMode;

class YarrboardApp;
class ConfigManager;

// message handler callback definition
// void(JsonVariantConst input, JsonVariant output)
using ProtocolMessageHandler = std::function<void(JsonVariantConst, JsonVariant)>;

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
    bool registerCommand(UserRole role, const char* command, T* instance, void (T::*method)(JsonVariantConst, JsonVariant))
    {
      // No casting needed on 'this'. We use the explicitly passed 'instance'.
      return registerCommand(role, command, [instance, method](JsonVariantConst in, JsonVariant out) {
        (instance->*method)(in, out);
      });
    }

    bool hasPermission(UserRole requiredRole, UserRole userRole);
    const char* getRoleText(UserRole role);
    bool isSerialAuthenticated();

    void sendBrightnessUpdate();
    void sendThemeUpdate();
    void sendFastUpdate();
    void sendOTAProgressUpdate(float progress);
    void sendOTAProgressFinished();
    void sendDebug(const char* message);
    void sendToAll(const char* jsonString, UserRole auth_level);

    void handleReceivedJSON(JsonVariantConst input, JsonVariant output, YBMode mode, PsychicWebSocketClient* connection = NULL);
    static void generateErrorJSON(JsonVariant output, const char* error);
    static void generateSuccessJSON(JsonVariant output, const char* success);

    void incrementSentMessages();

  private:
    bool is_serial_authenticated = false;
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

    // 2. Define a Custom Comparator
    // This tells the Map to compare the TEXT, not the memory addresses.
    struct StringCompare {
        bool operator()(const char* lhs, const char* rhs) const
        {
          return strcmp(lhs, rhs) < 0;
        }
    };

    // 3. The Map now uses const char* and our custom comparator
    etl::map<const char*, CommandEntry, YB_PROTOCOL_MAX_COMMANDS, StringCompare> commandMap;

    void handleSerialJson();

    void handleGetConfig(JsonVariantConst input, JsonVariant output);
    void handleGetStats(JsonVariantConst input, JsonVariant output);
    void handleGetUpdate(JsonVariantConst input, JsonVariant output);
    void handleGetFullConfig(JsonVariantConst input, JsonVariant output);
    void handleGetNetworkConfig(JsonVariantConst input, JsonVariant output);
    void handleGetAppConfig(JsonVariantConst input, JsonVariant output);
    void handleSetGeneralConfig(JsonVariantConst input, JsonVariant output);
    void handleSetNetworkConfig(JsonVariantConst input, JsonVariant output);
    void handleSetAuthenticationConfig(JsonVariantConst input, JsonVariant output);
    void handleSetWebServerConfig(JsonVariantConst input, JsonVariant output);
    void handleSetMQTTConfig(JsonVariantConst input, JsonVariant output);
    void handleSetMiscellaneousConfig(JsonVariantConst input, JsonVariant output);
    void handleSaveConfig(JsonVariantConst input, JsonVariant output);
    void handleLogin(JsonVariantConst input, JsonVariant output, YBMode mode, PsychicWebSocketClient* connection);
    void handleLogout(JsonVariantConst input, JsonVariant output, YBMode mode, PsychicWebSocketClient* connection = NULL);
    void handleRestart(JsonVariantConst input, JsonVariant output);
    void handleCrashMe(JsonVariantConst input, JsonVariant output);
    void handleFactoryReset(JsonVariantConst input, JsonVariant output);
    void handleOTAStart(JsonVariantConst input, JsonVariant output);
    void handlePlaySound(JsonVariantConst input, JsonVariant output);
    void handleSetSwitch(JsonVariantConst input, JsonVariant output);
    void handleConfigSwitch(JsonVariantConst input, JsonVariant output);
    void handleConfigRGB(JsonVariantConst input, JsonVariant output);
    void handleSetRGB(JsonVariantConst input, JsonVariant output);
    void handleSetTheme(JsonVariantConst input, JsonVariant output);
    void handleSetBrightness(JsonVariantConst input, JsonVariant output);

    void generateHelloJSON(JsonVariant output, UserRole role);
    void generateUpdateJSON(JsonVariant output);
    void generateFastUpdateJSON(JsonVariant output);
    void generateConfigJSON(JsonVariant output);
    void generateStatsJSON(JsonVariant output);
    void generateFullConfigMessage(JsonVariant output);
    void generateNetworkConfigMessage(JsonVariant output);
    void generateAppConfigMessage(JsonVariant output);
    void generateOTAProgressUpdateJSON(JsonVariant output, float progress);
    void generateOTAProgressFinishedJSON(JsonVariant output);
    void generateLoginRequiredJSON(JsonVariant output);
    void generateInvalidChannelJSON(JsonVariant output, byte cid);
    void handlePing(JsonVariantConst input, JsonVariant output);
};

#endif /* !YARR_PROTOCOL_H */