/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_PROTOCOL_H
#define YARR_PROTOCOL_H

#include "YarrboardConfig.h"
#include "utility.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef YB_HAS_FANS
  #include "fans.h"
#endif

#ifdef YB_HAS_BUS_VOLTAGE
  #include "bus_voltage.h"
#endif

#ifdef YB_IS_BRINEOMATIC
  #include "brineomatic.h"
#endif

class YarrboardApp;
class ConfigManager;

class ProtocolController
{
  public:
    ProtocolController(YarrboardApp& app, ConfigManager& config);

    void setup();
    void loop();

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
    YarrboardApp& _app;
    ConfigManager& _config;

    bool is_serial_authenticated = false;
    unsigned long previousMessageMillis = 0;
    unsigned int receivedMessages = 0;
    unsigned int receivedMessagesPerSecond = 0;
    unsigned long totalReceivedMessages = 0;
    unsigned int sentMessages = 0;
    unsigned int sentMessagesPerSecond = 0;
    unsigned long totalSentMessages = 0;

    void handleSerialJson();

    void handleSetGeneralConfig(JsonVariantConst input, JsonVariant output);
    void handleSetNetworkConfig(JsonVariantConst input, JsonVariant output);
    void handleSetAuthenticationConfig(JsonVariantConst input, JsonVariant output);
    void handleSetWebServerConfig(JsonVariantConst input, JsonVariant output);
    void handleSetMQTTConfig(JsonVariantConst input, JsonVariant output);
    void handleSetMiscellaneousConfig(JsonVariantConst input, JsonVariant output);
    void handleSaveConfig(JsonVariantConst input, JsonVariant output);
    void handleLogin(JsonVariantConst input, JsonVariant output, YBMode mode, PsychicWebSocketClient* connection = NULL);
    void handleLogout(JsonVariantConst input, JsonVariant output, YBMode mode, PsychicWebSocketClient* connection = NULL);
    void handleRestart(JsonVariantConst input, JsonVariant output);
    void handleCrashMe(JsonVariantConst input, JsonVariant output);
    void handleFactoryReset(JsonVariantConst input, JsonVariant output);
    void handleOTAStart(JsonVariantConst input, JsonVariant output);
    void handleConfigPWMChannel(JsonVariantConst input, JsonVariant output);
    void handlePlaySound(JsonVariantConst input, JsonVariant output);
    void handleSetPWMChannel(JsonVariantConst input, JsonVariant output);
    void handleTogglePWMChannel(JsonVariantConst input, JsonVariant output);
    void handleConfigRelayChannel(JsonVariantConst input, JsonVariant output);
    void handleSetRelayChannel(JsonVariantConst input, JsonVariant output);
    void handleToggleRelayChannel(JsonVariantConst input, JsonVariant output);
    void handleConfigServoChannel(JsonVariantConst input, JsonVariant output);
    void handleSetServoChannel(JsonVariantConst input, JsonVariant output);
    void handleConfigStepperChannel(JsonVariantConst input, JsonVariant output);
    void handleSetStepperChannel(JsonVariantConst input, JsonVariant output);
    void handleSetSwitch(JsonVariantConst input, JsonVariant output);
    void handleConfigSwitch(JsonVariantConst input, JsonVariant output);
    void handleConfigRGB(JsonVariantConst input, JsonVariant output);
    void handleSetRGB(JsonVariantConst input, JsonVariant output);
    void handleConfigADC(JsonVariantConst input, JsonVariant output);
    void handleSetTheme(JsonVariantConst input, JsonVariant output);
    void handleSetBrightness(JsonVariantConst input, JsonVariant output);

#ifdef YB_IS_BRINEOMATIC
    void handleStartWatermaker(JsonVariantConst input, JsonVariant output);
    void handleFlushWatermaker(JsonVariantConst input, JsonVariant output);
    void handlePickleWatermaker(JsonVariantConst input, JsonVariant output);
    void handleDepickleWatermaker(JsonVariantConst input, JsonVariant output);
    void handleStopWatermaker(JsonVariantConst input, JsonVariant output);
    void handleIdleWatermaker(JsonVariantConst input, JsonVariant output);
    void handleManualWatermaker(JsonVariantConst input, JsonVariant output);
    void handleSetWatermaker(JsonVariantConst input, JsonVariant output);
    void handleBrineomaticSaveGeneralConfig(JsonVariantConst input, JsonVariant output);
    void handleBrineomaticSaveHardwareConfig(JsonVariantConst input, JsonVariant output);
    void handleBrineomaticSaveSafeguardsConfig(JsonVariantConst input, JsonVariant output);
#endif

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
    void generatePongJSON(JsonVariant output);
};

#endif /* !YARR_PROTOCOL_H */