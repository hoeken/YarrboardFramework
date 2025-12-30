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

AuthController::AuthController(YarrboardApp& app) : BaseController(app, "auth")
{
}

bool AuthController::setup()
{
  // init our authentication stuff
  authenticatedClients.clear();
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
  _cfg.serial_role = role;
}

void AuthController::logSerialClientOut()
{
  is_serial_authenticated = false;
  _cfg.serial_role = _cfg.app_default_role;
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
  else if (mode == YBP_MODE_HTTP)
    return _cfg.api_role;
  else if (mode == YBP_MODE_SERIAL)
    return _cfg.serial_role;
  else if (mode == YBP_MODE_MQTT) {
    UserRole role = _cfg.app_default_role;
    this->checkLoginCredentials(input, role);
    return role;
  } else
    return _cfg.app_default_role;
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

  return _cfg.app_default_role;
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
  strlcpy(mypass, doc["pass"] | "", sizeof(myuser));

  // morpheus... i'm in.
  if (!strcmp(_cfg.admin_user, myuser) && !strcmp(_cfg.admin_pass, mypass)) {
    role = ADMIN;
    return true;
  }

  if (!strcmp(_cfg.guest_user, myuser) && !strcmp(_cfg.guest_pass, mypass)) {
    role = GUEST;
    return true;
  }

  // default to fail then.
  role = _cfg.app_default_role;
  return false;
}

bool AuthController::isSerialClientLoggedIn(JsonVariantConst doc)
{
  if (isSerialAuthenticated())
    return true;
  else
    return checkLoginCredentials(doc, _cfg.serial_role);
}

bool AuthController::isApiClientLoggedIn(JsonVariantConst doc)
{
  return checkLoginCredentials(doc, _cfg.api_role);
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
