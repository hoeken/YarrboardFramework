#include "HTTPController.h"
#include "ConfigManager.h"
#include "ProtocolController.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"

HTTPController* HTTPController::_instance = nullptr;

HTTPController::HTTPController(YarrboardApp& app, ConfigManager& config) : _app(app),
                                                                           _config(config)
{
}

void HTTPController::setup()
{
  sendMutex = xSemaphoreCreateMutex();
  if (sendMutex == NULL)
    YBP.println("Failed to create send mutex");

  // init our authentication stuff
  for (byte i = 0; i < YB_CLIENT_LIMIT; i++) {
    authenticatedClients[i].socket = 0;
    authenticatedClients[i].role = _config.app_default_role;
  }

  // prepare our message queue
  wsRequests = xQueueCreate(YB_RECEIVE_BUFFER_COUNT, sizeof(WebsocketRequest));
  if (wsRequests == 0)
    YBP.printf("Failed to create queue= %p\n", wsRequests);

  // do we want secure or not?
  if (_config.app_enable_ssl && _config.server_cert.length() && _config.server_key.length()) {
    server = new PsychicHttpsServer(443);
    server->setCertificate(_config.server_cert.c_str(), _config.server_key.c_str());
    YBP.println("SSL enabled");
  } else {
    server = new PsychicHttpServer(80);
    YBP.println("SSL disabled");
  }

  server->config.max_open_sockets = YB_CLIENT_LIMIT;
  server->config.lru_purge_enable = true;
  server->config.stack_size = 8192;

  // Populate the last modification date based on build datetime
  sprintf(last_modified, "%s %s GMT", __DATE__, __TIME__);

  server->on("/", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    // Check if the client already has the same version and respond with a 304
    // (Not modified)
    if (request->header("If-Modified-Since").indexOf(last_modified) > 0)
      return response->send(304);
    // What about our ETag?
    else if (request->header("If-None-Match").equals(index_html_gz_sha))
      return response->send(304);
    else {
      response->setCode(200);
      response->setContentType("text/html");

      // Tell the browswer the contemnt is Gzipped
      response->addHeader("Content-Encoding", "gzip");

      // And set the last-modified datetime so we can check if we need to send
      // it again next time or not
      response->addHeader("Last-Modified", last_modified);
      response->addHeader("ETag", index_html_gz_sha);

      // add our actual content
      response->setContent(index_html_gz, index_html_gz_len);

      return response->send();
    }
  });

  server->on("/logo.png", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    response->setCode(200);
    response->setContentType("image/png");
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Last-Modified", last_modified);
    response->addHeader("ETag", logo_gz_sha);
    response->setContent(logo_gz, logo_gz_len);
    return response->send();
  });

  server->on("/site.webmanifest", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    esp_err_t err = ESP_OK;
    JsonDocument doc;

    // Root values
    doc["short_name"] = _config.board_name;
    doc["name"] = _config.board_name;

    // icons array
    JsonArray icons = doc["icons"].to<JsonArray>();
    JsonObject icon = icons.add<JsonObject>();
    icon["src"] = "logo.png";
    icon["sizes"] = "512x512";
    icon["type"] = "image/png";

    doc["start_url"] = ".";
    doc["display"] = "standalone";
    doc["theme_color"] = "#000000";
    doc["background_color"] = "#ffffff";

    // we can have empty messages
    if (doc.size()) {
      // allocate memory for this output
      size_t jsonSize = measureJson(doc);
      char* jsonBuffer = (char*)malloc(jsonSize + 1);

      // did we get anything?
      if (jsonBuffer != NULL) {
        jsonBuffer[jsonSize] = '\0'; // null terminate
        response->setContentType("application/manifest+json");
        serializeJson(doc.as<JsonObject>(), jsonBuffer, jsonSize + 1);
        response->setContent(jsonBuffer);
        err = response->send();

        // no leaks!
        free(jsonBuffer);
      }
      // send overloaded response
      else {
        YBP.println("Error allocating in web.manifest");
        err = response->send(503, "application/manifest+json", "{}");
      }
    }

    return err;
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
    removeClientFromAuthList(client);
    websocketClientCount--;
  });
  server->on("/ws", &websocketHandler);

  server->onOpen([this](PsychicClient* client) { httpClientCount++; });

  server->onClose([this](PsychicClient* client) { httpClientCount--; });

  // our main api connection
  server->on("/api/endpoint", HTTP_ANY, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument json;

    String body = request->body();
    DeserializationError err = deserializeJson(json, body);

    handleWebServerRequest(json, request, response);

    return ESP_OK;
  });

  // send config json
  server->on("/api/config", HTTP_ANY, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument json;
    json["cmd"] = "get_config";

    handleWebServerRequest(json, request, response);

    return ESP_OK;
  });

  // send stats json
  server->on("/api/stats", HTTP_ANY, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument json;
    json["cmd"] = "get_stats";

    handleWebServerRequest(json, request, response);

    return ESP_OK;
  });

  // send update json
  server->on("/api/update", HTTP_ANY, [this](PsychicRequest* request, PsychicResponse* response) {
    JsonDocument json;
    json["cmd"] = "get_update";

    handleWebServerRequest(json, request, response);

    return ESP_OK;
  });

  // downloadable coredump file
  server->on("/coredump.bin", HTTP_GET, [this](PsychicRequest* request, PsychicResponse* response) {
    deleteCoreDump(); // clear ESP flash dump
    has_coredump = false;

    if (!LittleFS.exists("/coredump.bin")) {
      response->setCode(404);
      response->setContent("Coredump not found.");
      return response->send();
    }

    File fp = LittleFS.open("/coredump.bin");
    PsychicFileResponse fileResponse(response, fp, "/coredump.bin", "application/octet-stream", true);
    return fileResponse.send();
  });

  server->start();
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
  // make sure we're allowed to see the message
  if (auth_level > _config.app_default_role) {
    for (byte i = 0; i < YB_CLIENT_LIMIT; i++) {
      if (authenticatedClients[i].socket) {
        // make sure its a valid client
        PsychicWebSocketClient* client =
          websocketHandler.getClient(authenticatedClients[i].socket);
        if (client == NULL)
          continue;

        if (authenticatedClients[i].role >= auth_level) {
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

bool HTTPController::logClientIn(PsychicWebSocketClient* client, UserRole role)
{
  // did we not find a spot?
  if (!addClientToAuthList(client, role)) {
    YBP.println("Error: could not add to auth list.");

    // i'm pretty sure this closes our connection
    close(client->socket());

    return false;
  }

  return true;
}

esp_err_t HTTPController::handleWebServerRequest(JsonVariant input, PsychicRequest* request, PsychicResponse* response)
{
  esp_err_t err = ESP_OK;
  JsonDocument output;

  if (request->hasParam("user"))
    input["user"] = request->getParam("user")->value();
  if (request->hasParam("pass"))
    input["pass"] = request->getParam("pass")->value();

  if (_config.app_enable_api) {
    isApiClientLoggedIn(input);
    _app.protocol.handleReceivedJSON(input, output, YBP_MODE_HTTP);
  } else
    _app.protocol.generateErrorJSON(output, "Web API is disabled.");

  // we can have empty messages
  if (output.size()) {
    // allocate memory for this output
    size_t jsonSize = measureJson(output);
    char* jsonBuffer = (char*)malloc(jsonSize + 1);

    // did we get anything?
    if (jsonBuffer != NULL) {
      jsonBuffer[jsonSize] = '\0'; // null terminate
      response->setContentType("application/json");
      serializeJson(output.as<JsonObject>(), jsonBuffer, jsonSize + 1);
      response->setContent(jsonBuffer);
      err = response->send();
    }
    // send overloaded response
    else {
      YBP.println("Error allocating in handleWebServerRequest()");
      err = response->send(503, "application/json", "{}");
    }

    // no leaks!
    free(jsonBuffer);
  }
  // give them valid json at least
  else
    err = response->send(200, "application/json", "{}");

  return err;
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
  memcpy(wr.buffer, data, len + 1);

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
  } else
    _app.protocol.handleReceivedJSON(input, output, YBP_MODE_WEBSOCKET, client);

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

bool HTTPController::isLoggedIn(JsonVariantConst input, byte mode,
  PsychicWebSocketClient* connection)
{
  // login only required for websockets.
  if (mode == YBP_MODE_WEBSOCKET)
    return isWebsocketClientLoggedIn(input, connection);
  else if (mode == YBP_MODE_HTTP)
    return isApiClientLoggedIn(input);
  else if (mode == YBP_MODE_SERIAL)
    return isSerialClientLoggedIn(input);
  else
    return false;
}

UserRole HTTPController::getUserRole(JsonVariantConst input, byte mode,
  PsychicWebSocketClient* connection)
{
  // login only required for websockets.
  if (mode == YBP_MODE_WEBSOCKET)
    return getWebsocketRole(input, connection);
  else if (mode == YBP_MODE_HTTP)
    return _config.api_role;
  else if (mode == YBP_MODE_SERIAL)
    return _config.serial_role;
  else
    return _config.app_default_role;
}

bool HTTPController::isWebsocketClientLoggedIn(JsonVariantConst doc,
  PsychicWebSocketClient* client)
{
  // are they in our auth array?
  for (byte i = 0; i < YB_CLIENT_LIMIT; i++)
    if (authenticatedClients[i].socket == client->socket())
      return true;

  return false;
}

UserRole HTTPController::getWebsocketRole(JsonVariantConst doc,
  PsychicWebSocketClient* client)
{
  // are they in our auth array?
  for (byte i = 0; i < YB_CLIENT_LIMIT; i++)
    if (authenticatedClients[i].socket == client->socket())
      return authenticatedClients[i].role;

  return _config.app_default_role;
}

bool HTTPController::checkLoginCredentials(JsonVariantConst doc, UserRole& role)
{
  if (!doc["user"].is<String>())
    return false;
  if (!doc["pass"].is<String>())
    return false;

  // init
  char myuser[YB_USERNAME_LENGTH];
  char mypass[YB_PASSWORD_LENGTH];
  strlcpy(myuser, doc["user"] | "", sizeof(myuser));
  strlcpy(mypass, doc["pass"] | "", sizeof(myuser));

  // morpheus... i'm in.
  if (!strcmp(_config.admin_user, myuser) && !strcmp(_config.admin_pass, mypass)) {
    role = ADMIN;
    return true;
  }

  if (!strcmp(_config.guest_user, myuser) && !strcmp(_config.guest_pass, mypass)) {
    role = GUEST;
    return true;
  }

  // default to fail then.
  role = _config.app_default_role;
  return false;
}

bool HTTPController::isSerialClientLoggedIn(JsonVariantConst doc)
{
  if (_app.protocol.isSerialAuthenticated())
    return true;
  else
    return checkLoginCredentials(doc, _config.serial_role);
}

bool HTTPController::isApiClientLoggedIn(JsonVariantConst doc)
{
  return checkLoginCredentials(doc, _config.api_role);
}

bool HTTPController::addClientToAuthList(PsychicWebSocketClient* client, UserRole role)
{
  byte i;
  for (i = 0; i < YB_CLIENT_LIMIT; i++) {
    // did we find an empty slot?
    if (!authenticatedClients[i].socket) {
      authenticatedClients[i].socket = client->socket();
      authenticatedClients[i].role = role;
      break;
    }

    // are we already authenticated?
    if (authenticatedClients[i].socket == client->socket())
      break;
  }

  // did we not find a spot?
  if (i == YB_CLIENT_LIMIT) {
    return false;
    YBP.println("ERROR: max clients reached");
  } else
    return true;
}

void HTTPController::removeClientFromAuthList(PsychicWebSocketClient* client)
{
  byte i;
  for (i = 0; i < YB_CLIENT_LIMIT; i++) {
    // did we find an empty slot?
    if (authenticatedClients[i].socket == client->socket()) {
      authenticatedClients[i].socket = 0;
      authenticatedClients[i].role = _config.app_default_role;
    }
  }
}