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

#include "controllers/AuthController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"
#include "YarrboardDebug.h"
#include "controllers/ProtocolController.h"

AuthController::AuthController(YarrboardApp& app) : BaseController(app, "auth")
{
  strlcpy(admin_user, _app.default_admin_user, sizeof(admin_user));
  strlcpy(admin_pass, _app.default_admin_pass, sizeof(admin_pass));
  strlcpy(guest_user, _app.default_guest_user, sizeof(guest_user));
  strlcpy(guest_pass, _app.default_guest_pass, sizeof(guest_pass));
  app_default_role = serial_role = _app.default_role;
}

bool AuthController::setup()
{
  authenticatedClients.clear();

  _app.protocol.registerCommand(NOBODY, "login", this, &AuthController::handleLogin);
  _app.protocol.registerCommand(NOBODY, "logout", this, &AuthController::handleLogout);
  _app.protocol.registerCommand(ADMIN, "set_authentication_config", this, &AuthController::handleSetAuthenticationConfig);

  return true;
}

bool AuthController::logClientIn(int socket, UserRole role)
{
  // did we not find a spot?
  if (!addClientToAuthList(socket, role)) {
    YBP.println("Error: could not add to auth list.");

    // i'm pretty sure this closes our connection
    close(socket);

    return false;
  }

  return true;
}

void AuthController::logSerialClientIn(UserRole role)
{
  is_serial_authenticated = true;
  serial_role = role;
}

void AuthController::logSerialClientOut()
{
  is_serial_authenticated = false;
  serial_role = app_default_role;
}

bool AuthController::isLoggedIn(JsonVariantConst input, byte mode, int socket)
{
  // login only required for websockets.
  if (mode == YBP_MODE_WEBSOCKET)
    return isWebsocketClientLoggedIn(input, socket);
  else if (mode == YBP_MODE_HTTP)
    return isApiClientLoggedIn(input);
  else if (mode == YBP_MODE_SERIAL)
    return isSerialClientLoggedIn(input);
  else
    return false;
}

UserRole AuthController::getUserRole(JsonVariantConst input, byte mode, int socket)
{
  // login only required for websockets.
  if (mode == YBP_MODE_WEBSOCKET)
    return getWebsocketRole(input, socket);
  else if (mode == YBP_MODE_HTTP) {
    UserRole role = app_default_role;
    checkLoginCredentials(input, role);
    return role;
  } else if (mode == YBP_MODE_SERIAL)
    return serial_role;
  else if (mode == YBP_MODE_MQTT) {
    UserRole role = app_default_role;
    this->checkLoginCredentials(input, role);
    return role;
  } else
    return app_default_role;
}

bool AuthController::isWebsocketClientLoggedIn(JsonVariantConst doc, int socket)
{
  // are they in our auth array?
  for (auto& authClient : authenticatedClients)
    if (authClient.socket == socket)
      return true;

  return false;
}

UserRole AuthController::getWebsocketRole(JsonVariantConst doc, int socket)
{
  // are they in our auth array?
  for (auto& authClient : authenticatedClients)
    if (authClient.socket == socket)
      return authClient.role;

  return app_default_role;
}

bool AuthController::checkLoginCredentials(JsonVariantConst doc, UserRole& role)
{
  if (!doc["user"].is<String>())
    return false;
  if (!doc["pass"].is<String>())
    return false;

  // init
  char myuser[YB_USERNAME_LENGTH];
  char mypass[YB_PASSWORD_LENGTH];
  strlcpy(myuser, doc["user"] | "", sizeof(myuser));
  strlcpy(mypass, doc["pass"] | "", sizeof(mypass));

  // morpheus... i'm in.
  if (!strcmp(admin_user, myuser) && !strcmp(admin_pass, mypass)) {
    role = ADMIN;
    return true;
  }

  if (!strcmp(guest_user, myuser) && !strcmp(guest_pass, mypass)) {
    role = GUEST;
    return true;
  }

  // default to fail then.
  role = app_default_role;
  return false;
}

bool AuthController::isSerialClientLoggedIn(JsonVariantConst doc)
{
  if (isSerialAuthenticated())
    return true;
  else
    return checkLoginCredentials(doc, serial_role);
}

bool AuthController::isApiClientLoggedIn(JsonVariantConst doc)
{
  UserRole role = app_default_role;
  return checkLoginCredentials(doc, role);
}

bool AuthController::addClientToAuthList(int socket, UserRole role)
{
  // check if already authenticated
  for (auto& authClient : authenticatedClients) {
    if (authClient.socket == socket) {
      // update role just in case
      authClient.role = role;
      return true;
    }
  }

  // check for space
  if (authenticatedClients.full()) {
    YBP.println("ERROR: max clients reached");
    return false;
  }

  // add new client
  authenticatedClients.push_back({socket, role});
  return true;
}

void AuthController::removeClientFromAuthList(int socket)
{
  for (auto it = authenticatedClients.begin(); it != authenticatedClients.end(); ++it) {
    if (it->socket == socket) {
      authenticatedClients.erase(it);
      break;
    }
  }
}

bool AuthController::isSerialAuthenticated()
{
  return is_serial_authenticated;
}

// Returns true if 'userRole' is sufficient to execute a command requiring 'requiredRole'
bool AuthController::hasPermission(UserRole requiredRole, UserRole userRole)
{
  // 1. ADMIN can do everything
  if (userRole == ADMIN)
    return true;

  // 2. GUEST can handle GUEST or NOBODY
  if (userRole == GUEST && (requiredRole == GUEST || requiredRole == NOBODY))
    return true;

  // 3. NOBODY can only handle NOBODY
  if (userRole == NOBODY && requiredRole == NOBODY)
    return true;

  return false;
}

const char* AuthController::getRoleText(UserRole role)
{
  if (role == ADMIN)
    return "admin";
  else if (role == GUEST)
    return "guest";
  else if (role == NOBODY)
    return "nobody";
  else
    return "unknown";
}

const char* AuthController::getAdminPass() const
{
  return admin_pass;
}

UserRole AuthController::getDefaultRole() const
{
  return app_default_role;
}

UserRole AuthController::getSerialRole() const
{
  return serial_role;
}

const etl::vector<AuthenticatedClient, YB_CLIENT_LIMIT>& AuthController::getAuthenticatedClients() const
{
  return authenticatedClients;
}

void AuthController::handleLogin(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!input["user"].is<String>())
    return ProtocolController::generateErrorJSON(output, "'user' is a required parameter");
  if (!input["pass"].is<String>())
    return ProtocolController::generateErrorJSON(output, "'pass' is a required parameter");

  UserRole role;
  if (checkLoginCredentials(input, role)) {
    if (context.mode == YBP_MODE_WEBSOCKET) {
      if (!logClientIn(context.clientId, role))
        return ProtocolController::generateErrorJSON(output, "Too many connections.");
    } else if (context.mode == YBP_MODE_SERIAL) {
      logSerialClientIn(role);
    }
    output["msg"] = "login";
    output["role"] = getRoleText(role);
    output["message"] = "Login successful.";
    return;
  }

  ProtocolController::generateErrorJSON(output, "Wrong username/password.");
}

void AuthController::handleLogout(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!_app.auth.isLoggedIn(input, context.mode, context.clientId))
    return _app.protocol.generateErrorJSON(output, "You are not logged in.");

  // what type of client are you?
  if (context.mode == YBP_MODE_WEBSOCKET) {
    _app.auth.removeClientFromAuthList(context.clientId);
  } else if (context.mode == YBP_MODE_SERIAL) {
    _app.auth.logSerialClientOut();
  }

  output["msg"] = "logout";
  output["message"] = "Logout successful.";
}

void AuthController::handleSetAuthenticationConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context)
{
  if (!input["admin_user"].is<String>())
    return ProtocolController::generateErrorJSON(output, "'admin_user' is a required parameter");
  if (!input["admin_pass"].is<String>())
    return ProtocolController::generateErrorJSON(output, "'admin_pass' is a required parameter");
  if (!input["guest_user"].is<String>())
    return ProtocolController::generateErrorJSON(output, "'guest_user' is a required parameter");
  if (!input["guest_pass"].is<String>())
    return ProtocolController::generateErrorJSON(output, "'guest_pass' is a required parameter");
  if (!input["default_role"].is<String>())
    return ProtocolController::generateErrorJSON(output, "'default_role' is a required parameter");

  if (strlen(input["admin_user"]) > YB_USERNAME_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum admin username length is %d characters.", YB_USERNAME_LENGTH - 1);
    return ProtocolController::generateErrorJSON(output, error);
  }

  if (strlen(input["admin_pass"]) > YB_PASSWORD_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum admin password length is %d characters.", YB_PASSWORD_LENGTH - 1);
    return ProtocolController::generateErrorJSON(output, error);
  }

  if (strlen(input["guest_user"]) > YB_USERNAME_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum guest username length is %d characters.", YB_USERNAME_LENGTH - 1);
    return ProtocolController::generateErrorJSON(output, error);
  }

  if (strlen(input["guest_pass"]) > YB_PASSWORD_LENGTH - 1) {
    char error[60];
    sprintf(error, "Maximum guest password length is %d characters.", YB_PASSWORD_LENGTH - 1);
    return ProtocolController::generateErrorJSON(output, error);
  }

  strlcpy(admin_user, input["admin_user"] | _app.default_admin_user, sizeof(admin_user));
  strlcpy(admin_pass, input["admin_pass"] | _app.default_admin_pass, sizeof(admin_pass));
  strlcpy(guest_user, input["guest_user"] | _app.default_guest_user, sizeof(guest_user));
  strlcpy(guest_pass, input["guest_pass"] | _app.default_guest_pass, sizeof(guest_pass));

  if (input["default_role"]) {
    if (!strcmp(input["default_role"], "admin"))
      app_default_role = ADMIN;
    else if (!strcmp(input["default_role"], "guest"))
      app_default_role = GUEST;
    else if (!strcmp(input["default_role"], "nobody"))
      app_default_role = NOBODY;
    else
      return ProtocolController::generateErrorJSON(output, "Invalid 'default_role': must be 'admin', 'guest', or 'nobody'.");
  }

  char error[128] = "Unknown";
  if (!_app.config.saveConfig(error, sizeof(error)))
    ProtocolController::generateErrorJSON(output, error);
}

bool AuthController::loadConfigHook(JsonVariant config, char* error, size_t len)
{
  strlcpy(admin_user, config["admin_user"] | _app.default_admin_user, sizeof(admin_user));
  strlcpy(admin_pass, config["admin_pass"] | _app.default_admin_pass, sizeof(admin_pass));
  strlcpy(guest_user, config["guest_user"] | _app.default_guest_user, sizeof(guest_user));
  strlcpy(guest_pass, config["guest_pass"] | _app.default_guest_pass, sizeof(guest_pass));

  app_default_role = _app.default_role;
  if (config["default_role"]) {
    const char* v = config["default_role"];
    if (!strcmp(v, "nobody"))
      app_default_role = NOBODY;
    else if (!strcmp(v, "admin"))
      app_default_role = ADMIN;
    else if (!strcmp(v, "guest"))
      app_default_role = GUEST;
  }
  serial_role = app_default_role;
  return true;
}

void AuthController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  output["default_role"] = getRoleText(app_default_role);
  output["admin_user"] = admin_user;
  output["admin_pass"] = admin_pass;
  output["guest_user"] = guest_user;
  output["guest_pass"] = guest_pass;
}
