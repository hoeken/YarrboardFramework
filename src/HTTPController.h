/*
  Yarrboard

  Author: Zach Hoeken <hoeken@gmail.com>
  Website: https://github.com/hoeken/yarrboard
  License: GPLv3
*/

#ifndef YARR_SERVER_H
#define YARR_SERVER_H

#include "YarrboardConfig.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <PsychicHttpsServer.h>
#include <freertos/queue.h>

// generated at build by running "gulp" in the firmware directory.
#include "index.html.gz.h"
#include "logo.png.gz.h"

#ifdef YB_HAS_FANS
  #include "fans.h"
#endif

typedef struct {
    int socket;
    UserRole role;
} AuthenticatedClient;

typedef struct {
    int socket;
    char* buffer;
    size_t len;
} WebsocketRequest;

extern String server_cert;
extern String server_key;

void server_setup();
void server_loop();

class YarrboardApp;
class ConfigManager;

class HTTPController
{
  public:
    HTTPController(YarrboardApp& app, ConfigManager& config);

    void setup();
    void loop();

    UserRole getUserRole(JsonVariantConst input, byte mode, PsychicWebSocketClient* connection);
    bool logClientIn(PsychicWebSocketClient* connection, UserRole role);
    bool isLoggedIn(JsonVariantConst input, byte mode, PsychicWebSocketClient* connection);
    void removeClientFromAuthList(PsychicWebSocketClient* connection);
    void sendToAllWebsockets(const char* jsonString, UserRole auth_level);

    unsigned int websocketClientCount = 0;
    unsigned int httpClientCount = 0;

  private:
    YarrboardApp& _app;
    ConfigManager& _config;

    PsychicHttpServer* server;
    PsychicWebSocketHandler websocketHandler;
    char last_modified[50];
    QueueHandle_t wsRequests;
    SemaphoreHandle_t sendMutex;
    AuthenticatedClient authenticatedClients[YB_CLIENT_LIMIT];

    bool addClientToAuthList(PsychicWebSocketClient* connection, UserRole role);
    bool isWebsocketClientLoggedIn(JsonVariantConst input, PsychicWebSocketClient* connection);
    bool isApiClientLoggedIn(JsonVariantConst doc);
    bool isSerialClientLoggedIn(JsonVariantConst input);
    bool checkLoginCredentials(JsonVariantConst doc, UserRole& role);
    UserRole getWebsocketRole(JsonVariantConst doc, PsychicWebSocketClient* connection);

    void handleWebsocketMessageLoop(WebsocketRequest* request);

    esp_err_t handleWebServerRequest(JsonVariant input, PsychicRequest* request, PsychicResponse* response);
    void handleWebSocketMessage(PsychicWebSocketRequest* request, uint8_t* data, size_t len);

    // --- THE CALLBACK TRAP ---
    // Libraries expecting C-style function pointers cannot take normal member functions.
    // We use a static instance pointer and static methods to bridge the gap.
    static HTTPController* _instance;
    // static void _updateBeginFailCallbackStatic(int partition);
};

#endif /* !YARR_SERVER_H */