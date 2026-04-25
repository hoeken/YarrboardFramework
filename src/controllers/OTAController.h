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

#ifndef YARR_OTA_H
#define YARR_OTA_H

#include "controllers/BaseController.h"
#include "controllers/ProtocolController.h"
#include "utility.h"
#include <ArduinoOTA.h>

#define DISABLE_ALL_LIBRARY_WARNINGS
#include <esp32FOTA.hpp>

struct OTAConfig {
    bool arduino_ota_enabled;
};

class YarrboardApp;
class ConfigManager;

class OTAController : public BaseController
{
  public:
    OTAConfig defaults;

    OTAController(YarrboardApp& app);
    ~OTAController();

    bool setup() override;
    void loop() override;

    void end();
    bool checkOTA();
    void startOTA();

    const char* firmware_manifest_url = "";
    bool validate_firmware = true;
    const char* public_key = "";

    bool sanitizeConfigHook(JsonVariant config, char* error, size_t len) override;
    void loadConfigHook(JsonVariantConst config) override;
    void generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose) override;
    bool isEnabled() const { return _config.arduino_ota_enabled; }
    void setEnabled(bool v);

  private:
    OTAConfig _config;

    esp32FOTA* FOTA = nullptr;
    CryptoMemAsset* MyPubKey = nullptr;
    bool doOTAUpdate = false;
    unsigned long ota_last_message = 0;

    // --- THE CALLBACK TRAP ---
    // Libraries expecting C-style function pointers cannot take normal member functions.
    // We use a static instance pointer and static methods to bridge the gap.
    static OTAController* _instance;
    static void _updateBeginFailCallbackStatic(int partition);
    static void _progressCallbackStatic(size_t progress, size_t size);
    static void _updateEndCallbackStatic(int partition);
    static void _updateCheckFailCallbackStatic(int partition, int error_code);

    void _updateBeginFailCallback(int partition);
    void _progressCallback(size_t progress, size_t size);
    void _updateEndCallback(int partition);
    // error_code: -1 = partition not found
    //             -2 = signature check failed
    void _updateCheckFailCallback(int partition, int error_code);

    void handleOTAStart(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void sendOTAProgressUpdate(float progress);
    void sendOTAProgressFinished();
};

#endif /* !YARR_OTA_H */