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

#include "controllers/HTTPController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"
#include "controllers/NavicoController.h"
#include "controllers/ProtocolController.h"

HTTPController::HTTPController(YarrboardApp& app) : BaseController(app, "http")
{
}

bool HTTPController::loadConfigHook(JsonVariant config, char* error, size_t len)
{
  _app_enable_api = config["app_enable_api"] | _app.enable_http_api;
  _app_enable_ssl = config["app_enable_ssl"] | _app.enable_ssl;
  _server_cert = config["server_cert"] | "";
  _server_key = config["server_key"] | "";
  return true;
}

void HTTPController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  output["app_enable_api"] = _app_enable_api;
  output["app_enable_ssl"] = _app_enable_ssl;

  if (role == ADMIN) {
    output["server_cert"] = _server_cert;
    output["server_key"] = _server_key;
  }
}

esp_err_t HTTPController::sendJsonResponse(PsychicResponse* response, JsonDocument& doc, const char* contentType)
{
  size_t jsonSize = measureJson(doc);
  char* jsonBuffer = (char*)malloc(jsonSize + 1);
  if (jsonBuffer == NULL) {
    YBP.println("Error allocating JSON buffer");
    return response->send(503, contentType, "{}");
  }
  serializeJson(doc, jsonBuffer, jsonSize + 1);
  jsonBuffer[jsonSize] = '\0';
  response->setContentType(contentType);
  response->setContent(jsonBuffer);
  esp_err_t err = response->send();
  free(jsonBuffer);
  return err;
}

void HTTPController::registerGulpedFile(const GulpedFile* file, const char* path /* = nullptr */)
{
  if (file != nullptr && file->filename != nullptr) {
    const char* key = (path != nullptr) ? path : file->filename;
    gulpedFiles[key] = file;
  }
}

void HTTPController::registerGulpedFiles(const GulpedFile* files[], int count)
{
  for (int i = 0; i < count; i++) {
    registerGulpedFile(files[i]);
  }
}

bool HTTPController::setup()
{
  if (!WiFi.isConnected()) {
    YBP.println("WiFi not connected.");
    return false;
  }

  sendMutex = xSemaphoreCreateMutex();
  if (sendMutex == NULL) {
    YBP.println("Failed to create send mutex");
    return false;
  }

  // prepare our message queue
  wsRequests = xQueueCreate(YB_RECEIVE_BUFFER_COUNT, sizeof(WebsocketRequest));
  if (wsRequests == 0) {
    YBP.printf("Failed to create queue= %p\n", wsRequests);
    return false;
  }

  // do we want secure or not?
  if (_app_enable_ssl && _server_cert.length() && _server_key.length()) {
    server = new PsychicHttpsServer(443);
    server->setCertificate(_server_cert.c_str(), _server_key.c_str());
    // YBP.println("SSL enabled");
  } else {
    server = new PsychicHttpServer(80);
    // YBP.println("SSL disabled");
  }

  server->config.max_open_sockets = YB_CLIENT_LIMIT;
  server->config.lru_purge_enable = true;
  server->config.stack_size = 8192;

  // Populate the last modification date based on build datetime
  sprintf(last_modified, "%s %s GMT", __DATE__, __TIME__);

  // Register all gulped file routes
  for (auto& pair : gulpedFiles) {
    // YBP.printf("Registered gulp file at %s\n", pair.first);
    server->on(pair.first, HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
      return handleGulpedFile(request, response);
    });
  }

  // index shortcut to index.html
  server->on("/", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    return handleGulpedFile(request, response);
  });

  server->on("/site.webmanifest", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument doc;

    doc["short_name"] = _cfg.getBoardName();
    doc["name"] = _cfg.getBoardName();

    JsonArray icons = doc["icons"].to<JsonArray>();
    JsonObject icon = icons.add<JsonObject>();
    icon["src"] = "logo.png";
    icon["sizes"] = "512x512";
    icon["type"] = "image/png";

    doc["start_url"] = ".";
    doc["display"] = "standalone";
    doc["theme_color"] = "#000000";
    doc["background_color"] = "#ffffff";

    return sendJsonResponse(response, doc, "application/manifest+json");
  });

  // Our websocket handler
  websocketHandler.onFrame([this](PsychicWebSocketRequest* request, httpd_ws_frame* frame) {
    handleWebSocketMessage(request, frame->payload, frame->len);
    return ESP_OK;
  });
  websocketHandler.onOpen([this](PsychicWebSocketClient* client) {
    // YBP.printf("[socket] connection #%u connected from %s\n",
    //               client->socket(), client->remoteIP().toString());
    websocketClientCount++;
  });
  websocketHandler.onClose([this](PsychicWebSocketClient* client) {
    // YBP.printf("[socket] connection #%u closed from %s\n", client->socket(),
    //               client->remoteIP().toString());
    _app.auth.removeClientFromAuthList(client->socket());
    websocketClientCount--;
  });
  server->on("/ws", &websocketHandler);

  server->onOpen([this](PsychicClient* client) { httpClientCount++; });

  server->onClose([this](PsychicClient* client) { httpClientCount--; });

  // our main api connection
  server->on("/api/endpoint", HTTP_ANY, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument json;

    String body = request->body();
    if (deserializeJson(json, body))
      return response->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");

    return handleWebServerRequest(json, request, response);
  });

  // send config json
  server->on("/api/config", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument json;
    json["cmd"] = "get_config";

    handleWebServerRequest(json, request, response);

    return ESP_OK;
  });

  // send stats json
  server->on("/api/stats", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument json;
    json["cmd"] = "get_stats";

    handleWebServerRequest(json, request, response);

    return ESP_OK;
  });

  // send update json
  server->on("/api/update", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument json;
    json["cmd"] = "get_update";

    handleWebServerRequest(json, request, response);

    return ESP_OK;
  });

  // downloadable coredump file
  server->on("/coredump.bin", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    _app.debug.deleteCoreDump(); // clear ESP flash dump

    if (!LittleFS.exists("/coredump.bin")) {
      response->setCode(404);
      response->setContent("Coredump not found.");
      return response->send();
    }

    File fp = LittleFS.open("/coredump.bin");
    PsychicFileResponse fileResponse(response, fp, "/coredump.bin", "application/octet-stream", true);
    return fileResponse.send();
  });

  _app.protocol.registerCommand(ADMIN, "set_webserver_config", this, &HTTPController::handleSetWebServerConfig);

  server->start();

  return true;
}

void HTTPController::handleSetWebServerConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  bool old_app_enable_ssl = _app_enable_ssl;

  NavicoController* navico = static_cast<NavicoController*>(_app.getController("navico"));
  if (navico)
    navico->setMfdEnabled(input["app_enable_mfd"] | _app.enable_mfd);
  _app_enable_api = input["app_enable_api"] | _app.enable_http_api;
  _app_enable_ssl = input["app_enable_ssl"] | _app_enable_ssl;
  _server_cert = input["server_cert"] | _server_cert.c_str();
  _server_key = input["server_key"] | _server_key.c_str();

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return _app.protocol.generateErrorJSON(output, error);

  // restart the board.
  if (old_app_enable_ssl != _app_enable_ssl)
    ESP.restart();
}

void HTTPController::loop()
{
  // process our websockets outside the callback.
  WebsocketRequest request;
  while (xQueueReceive(wsRequests, &request, 0) == pdTRUE) {
    handleWebsocketMessageLoop(&request);

    // make sure to release our memory!
    free(request.buffer);
  }
}

void HTTPController::sendToAllWebsockets(const char* jsonString, UserRole auth_level)
{
  // if the mutex hasn't been created yet, we're not ready to send
  if (sendMutex == NULL) {
    return;
  }

  // make sure we're allowed to see the message
  if (auth_level > _app.auth.getDefaultRole()) {
    for (auto& authClient : _app.auth.getAuthenticatedClients()) {
      if (authClient.socket) {
        // make sure its a valid client
        PsychicWebSocketClient* client =
          websocketHandler.getClient(authClient.socket);
        if (client == NULL)
          continue;

        if (authClient.role >= auth_level) {
          if (xSemaphoreTake(sendMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            client->sendMessage(jsonString);
            xSemaphoreGive(sendMutex);
          } else {
            // dont use YBP here because it will get recursive.
            Serial.println("client->sendMessage mutex fail");
          }
        }
      }
    }
  }
  // nope, just send it to all.
  else {
    if (xSemaphoreTake(sendMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      websocketHandler.sendAll(jsonString);
      xSemaphoreGive(sendMutex);
    } else {
      // dont use YBP here because it will get recursive.
      Serial.println("websocketHandler.sendAll mutex fail");
    }
  }
}

esp_err_t HTTPController::handleWebServerRequest(JsonVariant input, PsychicRequest* request, PsychicResponse* response)
{
  JsonDocument output;

  if (request->hasParam("user"))
    input["user"] = request->getParam("user")->value();
  if (request->hasParam("pass"))
    input["pass"] = request->getParam("pass")->value();

  if (_app_enable_api) {
    _app.auth.isApiClientLoggedIn(input);

    ProtocolContext context;
    context.mode = YBP_MODE_HTTP;
    context.clientId = request->client()->socket();

    _app.protocol.handleReceivedJSON(input, output, context);
  } else
    _app.protocol.generateErrorJSON(output, "Web API is disabled.");

  if (output.size())
    return sendJsonResponse(response, output);

  return response->send(200, "application/json", "{}");
}

void HTTPController::handleWebSocketMessage(PsychicWebSocketRequest* request, uint8_t* data,
  size_t len)
{
  // build our websocket request - copy the existing one
  // we are allocating memory here, and the worker will free it
  WebsocketRequest wr;
  wr.socket = request->client()->socket();
  wr.len = len + 1;
  wr.buffer = (char*)malloc(len + 1);

  // did we flame out?
  if (wr.buffer == NULL) {
    YBP.printf("Queue message: unable to allocate %d bytes\n", len + 1);
    return;
  }

  // okay, copy it over
  memcpy(wr.buffer, data, len);
  wr.buffer[len] = '\0';

  // throw it in our queue
  if (xQueueSend(wsRequests, &wr, 1) != pdTRUE) {
    // request->client()->close();
    YBP.printf("[socket] queue full #%d\n", wr.socket);

    // free the memory... no worker to do it for us.
    free(wr.buffer);
  }

  // send a throttle message if we're full
  if (!uxQueueSpacesAvailable(wsRequests))
    request->reply("{\"error\":\"Queue Full\"}");
}

void HTTPController::handleWebsocketMessageLoop(WebsocketRequest* request)
{
  // make sure our client is still good.
  PsychicWebSocketClient* client = websocketHandler.getClient(request->socket);
  if (client == NULL) {
    // YBP.printf("[socket] client #%d bad, bailing\n", request->socket);
    return;
  }

  JsonDocument output;
  JsonDocument input;

  // was there a problem, officer?
  DeserializationError err = deserializeJson(input, request->buffer);
  if (err) {
    char error[64];
    sprintf(error, "deserializeJson() failed with code %s", err.c_str());
    _app.protocol.generateErrorJSON(output, error);
  } else {
    ProtocolContext context;
    context.mode = YBP_MODE_WEBSOCKET;
    context.clientId = client->socket();
    _app.protocol.handleReceivedJSON(input, output, context);
  }

  // empty messages are valid, so don't send a response
  if (output.size()) {
    // allocate memory for this output
    size_t jsonSize = measureJson(output);
    char* jsonBuffer = (char*)malloc(jsonSize + 1);

    // did we get anything?
    if (jsonBuffer != NULL) {
      serializeJson(output, jsonBuffer, jsonSize + 1);
      jsonBuffer[jsonSize] = '\0'; // null terminate

      if (xSemaphoreTake(sendMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        client->sendMessage(jsonBuffer);
        xSemaphoreGive(sendMutex);
      } else {
        Serial.println("handleWebsocketMessageLoop send mutex fail");
      }

      _app.protocol.incrementSentMessages();

      free(jsonBuffer);
    } else {
      YBP.println("Error allocating in handleWebsocketMessageLoop()");
    }
  }
}

esp_err_t HTTPController::handleGulpedFile(PsychicRequest* request, PsychicResponse* response)
{
  // special case for index
  String path;
  if (request->path().equals("/"))
    path = "/index.html";
  else
    path = request->path();

  // Look up the file in our map
  auto it = gulpedFiles.find(path.c_str());
  if (it == gulpedFiles.end()) {
    YBP.printf("Gulped file %s does not exist.\n", path.c_str());
    return response->send(404);
  }

  const GulpedFile* file = it->second;

  // Check if the client already has the same version and respond with a 304
  // (Not modified)
  if (request->header("If-Modified-Since").indexOf(last_modified) >= 0)
    return response->send(304);
  // What about our ETag?
  else if (request->header("If-None-Match").equals(file->sha256))
    return response->send(304);
  else {
    response->setCode(200);
    response->setContentType(file->mimetype);

    // Tell the browswer the contemnt is Gzipped
    response->addHeader("Content-Encoding", "gzip");

    // And set the last-modified datetime so we can check if we need to send
    // it again next time or not
    response->addHeader("Last-Modified", last_modified);
    response->addHeader("ETag", file->sha256);

    // add our actual content
    response->setContent(file->data, file->length);

    return response->send();
  }
}