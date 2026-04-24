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
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"

HTTPController::HTTPController(YarrboardApp& app) : BaseController(app, "http")
{
  defaults.api_enabled = false;
  defaults.ssl_enabled = false;
  defaults.server_cert = "";
  defaults.server_key = "";
  _config = defaults;
}

bool HTTPController::sanitizeConfigHook(JsonVariant config, char* error, size_t len)
{
  if (config["app_enable_api"].is<bool>()) {
    config["api_enabled"] = config["app_enable_api"].as<bool>();
    config.remove("app_enable_api");
  }

  if (config["app_enable_ssl"].is<bool>()) {
    config["ssl_enabled"] = config["app_enable_ssl"].as<bool>();
    config.remove("app_enable_ssl");
  }

  if (config["ssl_enabled"] && !validateCertAndKey(config["server_cert"].as<String>(), config["server_key"].as<String>())) {
    snprintf(error, len, "Invalid SSL certificate or key");
    config.remove("ssl_enabled");
    config.remove("server_cert");
    config.remove("server_key");

    return false;
  }

  return true;
}

void HTTPController::loadConfigHook(JsonVariantConst config)
{
  _config.api_enabled = config["api_enabled"] | defaults.api_enabled;
  _config.ssl_enabled = config["ssl_enabled"] | defaults.ssl_enabled;
  _config.server_cert = config["server_cert"] | defaults.server_cert.c_str();
  _config.server_key = config["server_key"] | defaults.server_key.c_str();
}

void HTTPController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  output["api_enabled"] = _config.api_enabled;
  output["ssl_enabled"] = _config.ssl_enabled;

  if (role == ADMIN && purpose != ConfigPurpose::SHAREABLE) {
    output["server_cert"] = _config.server_cert;
    output["server_key"] = _config.server_key;
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
  if (_config.ssl_enabled && _config.server_cert.length() && _config.server_key.length()) {
    if (validateCertAndKey(_config.server_cert, _config.server_key)) {
      server = new PsychicHttpsServer(443);
      server->setCertificate(_config.server_cert.c_str(), _config.server_key.c_str());
    } else {
      server = new PsychicHttpServer(80);
      YBP.println("⚠️ HTTP SSL Certificate invalid, falling back to plain HTTP");
    }
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
  _app.protocol.registerCommand(ADMIN, "generate_self_signed_cert", this, &HTTPController::handleGenerateSelfSignedCert);

  esp_err_t err = server->start();

  return err == ESP_OK;
}

bool HTTPController::validateCertAndKey(const String& cert_pem, const String& key_pem)
{
  mbedtls_x509_crt cert;
  mbedtls_pk_context key;
  mbedtls_entropy_context* entropy = (mbedtls_entropy_context*)malloc(sizeof(mbedtls_entropy_context));
  mbedtls_ctr_drbg_context ctr_drbg;
  int ret;
  bool is_valid = false;

  if (!entropy)
    return false;

  mbedtls_x509_crt_init(&cert);
  mbedtls_pk_init(&key);
  mbedtls_entropy_init(entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  const char* pers = "yarrboard_validate";
  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, entropy, (const unsigned char*)pers, strlen(pers));
  if (ret != 0) {
    YBP.printf("⚠️ Failed to seed RNG for validation. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  // 1. Parse the Certificate (PEM: length must include null terminator)
  ret = mbedtls_x509_crt_parse(&cert, (const unsigned char*)cert_pem.c_str(), cert_pem.length() + 1);
  if (ret != 0) {
    YBP.printf("⚠️ Failed to parse certificate. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  // 2. Parse the Private Key (PEM: length must include null terminator)
  ret = mbedtls_pk_parse_key(&key, (const unsigned char*)key_pem.c_str(), key_pem.length() + 1, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    YBP.printf("⚠️ Failed to parse private key. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  // 3. Check that the Certificate and Private Key match
  // mbedtls 3.x requires an RNG for ECDSA check_pair (it does a sign+verify internally)
  ret = mbedtls_pk_check_pair(&cert.pk, &key, mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    YBP.printf("⚠️ Certificate and Private Key DO NOT MATCH! Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  is_valid = true;

cleanup:
  mbedtls_x509_crt_free(&cert);
  mbedtls_pk_free(&key);
  mbedtls_entropy_free(entropy);
  free(entropy);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  return is_valid;
}

bool HTTPController::generateSelfSignedCert(String& cert_pem_out, String& key_pem_out)
{
  mbedtls_pk_context key;
  mbedtls_x509write_cert crt;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  int ret;
  bool success = false;

  // EC cert + key PEM fit well within 2KB each
  unsigned char* cert_buf = (unsigned char*)malloc(2048);
  unsigned char* key_buf = (unsigned char*)malloc(2048);
  if (!cert_buf || !key_buf) {
    free(cert_buf);
    free(key_buf);
    return false;
  }

  mbedtls_pk_init(&key);
  mbedtls_x509write_crt_init(&crt);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  const char* pers = "yarrboard_tls";
  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char*)pers, strlen(pers));
  if (ret != 0) {
    YBP.printf("⚠️ Failed to seed RNG. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  // EC secp256r1 generates in ~1-2s on ESP32; RSA-2048 takes 10-30s
  ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
  if (ret != 0) {
    YBP.printf("⚠️ Failed to set up key context. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    YBP.printf("⚠️ Failed to generate EC key. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&crt, &key);
  mbedtls_x509write_crt_set_issuer_key(&crt, &key);

  ret = mbedtls_x509write_crt_set_subject_name(&crt, "CN=Yarrboard,O=Yarrboard,C=US");
  if (ret != 0) {
    YBP.printf("⚠️ Failed to set subject name. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  ret = mbedtls_x509write_crt_set_issuer_name(&crt, "CN=Yarrboard,O=Yarrboard,C=US");
  if (ret != 0) {
    YBP.printf("⚠️ Failed to set issuer name. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  {
    unsigned char serial_raw[] = {0x01};
    ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial_raw, sizeof(serial_raw));
  }
  if (ret != 0) {
    YBP.printf("⚠️ Failed to set serial. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  // Hardcoded validity: ESP32 rarely has accurate RTC time at first boot
  ret = mbedtls_x509write_crt_set_validity(&crt, "20240101000000", "20340101000000");
  if (ret != 0) {
    YBP.printf("⚠️ Failed to set validity. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  ret = mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);
  if (ret != 0) {
    YBP.printf("⚠️ Failed to set basic constraints. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  ret = mbedtls_x509write_crt_pem(&crt, cert_buf, 2048, mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    YBP.printf("⚠️ Failed to write certificate PEM. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  ret = mbedtls_pk_write_key_pem(&key, key_buf, 2048);
  if (ret != 0) {
    YBP.printf("⚠️ Failed to write key PEM. Error: -0x%04x\n", -ret);
    goto cleanup;
  }

  cert_pem_out = String((char*)cert_buf);
  key_pem_out = String((char*)key_buf);
  success = true;

cleanup:
  free(cert_buf);
  free(key_buf);
  mbedtls_pk_free(&key);
  mbedtls_x509write_crt_free(&crt);
  mbedtls_entropy_free(&entropy);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  return success;
}

void HTTPController::handleSetWebServerConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  bool old_ssl_enabled = _config.ssl_enabled;

  NavicoController* navico = static_cast<NavicoController*>(_app.getController("navico"));
  if (navico)
    navico->setEnabled(input["navico_enabled"] | navico->defaults.enabled);

  _config.api_enabled = input["api_enabled"] | _config.api_enabled;
  _config.ssl_enabled = input["ssl_enabled"] | _config.ssl_enabled;
  _config.server_cert = input["server_cert"] | _config.server_cert.c_str();
  _config.server_key = input["server_key"] | _config.server_key.c_str();

  if (_config.ssl_enabled && !validateCertAndKey(_config.server_cert, _config.server_key))
    return _app.protocol.generateErrorJSON(output, "Invalid SSL certificate or key");

  // save it to file.
  char error[128] = "Unknown";
  if (!_cfg.saveConfig(error, sizeof(error)))
    return _app.protocol.generateErrorJSON(output, error);

  // restart the board.
  if (old_ssl_enabled != _config.ssl_enabled)
    ESP.restart();
}

void HTTPController::handleGenerateSelfSignedCert(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  String cert, key;
  if (!generateSelfSignedCert(cert, key))
    return _app.protocol.generateErrorJSON(output, "Failed to generate self-signed certificate");

  output["msg"] = "self_signed_cert";
  output["cert"] = cert;
  output["key"] = key;
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

  if (_config.api_enabled) {
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