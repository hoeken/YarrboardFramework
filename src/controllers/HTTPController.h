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

#ifndef YARR_HTTP_CONTROLLER_H
#define YARR_HTTP_CONTROLLER_H

#include "YarrboardConfig.h"

#include "GulpedFile.h"
#include "controllers/AuthController.h"
#include "controllers/BaseController.h"
#include "controllers/ProtocolController.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <PsychicHttpsServer.h>
#include <atomic>
#include <etl/map.h>
#include <freertos/queue.h>

#define MAX_GULPED_FILES 32

typedef struct {
    int socket;
    char* buffer;
    size_t len;
} WebsocketRequest;

class YarrboardApp;
class ConfigManager;

class HTTPController : public BaseController
{
  public:
    HTTPController(YarrboardApp& app);

    bool setup() override;
    void loop() override;

    bool validateCertAndKey(const String& cert_pem, const String& key_pem);

    bool loadConfigHook(JsonVariant config, char* error, size_t len) override;
    void generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose) override;

    void sendToAllWebsockets(const char* jsonString, UserRole auth_level);
    void registerGulpedFile(const GulpedFile* file, const char* path = nullptr);
    void registerGulpedFiles(const GulpedFile* files[], int count);
    PsychicHttpServer* getServer() { return server; }
    bool isSSLEnabled() const { return _app_enable_ssl; }
    bool isAPIEnabled() const { return _app_enable_api; }

    std::atomic<unsigned int> websocketClientCount{0};
    std::atomic<unsigned int> httpClientCount{0};

  private:
    bool _app_enable_api = false;
    bool _app_enable_ssl = false;
    String _server_cert;
    String _server_key;

    PsychicHttpServer* server = nullptr;
    PsychicWebSocketHandler websocketHandler;
    char last_modified[50];
    QueueHandle_t wsRequests;
    SemaphoreHandle_t sendMutex;

    struct CStringCompare {
        bool operator()(const char* a, const char* b) const
        {
          return strcmp(a, b) < 0;
        }
    };
    etl::map<const char*, const GulpedFile*, MAX_GULPED_FILES, CStringCompare> gulpedFiles;

    void handleWebsocketMessageLoop(WebsocketRequest* request);
    esp_err_t handleWebServerRequest(JsonVariant input, PsychicRequest* request, PsychicResponse* response);
    esp_err_t sendJsonResponse(PsychicResponse* response, JsonDocument& doc, const char* contentType = "application/json");
    void handleWebSocketMessage(PsychicWebSocketRequest* request, uint8_t* data, size_t len);
    esp_err_t handleGulpedFile(PsychicRequest* request, PsychicResponse* response);
    void handleSetWebServerConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);
};

#endif /* !YARR_HTTP_CONTROLLER_H */
