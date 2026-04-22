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

#ifndef YARR_AUTH_H
#define YARR_AUTH_H

#include "YarrboardConfig.h"
#include "controllers/AuthTypes.h"
#include "controllers/BaseController.h"
#include "controllers/ProtocolController.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <etl/vector.h>

class YarrboardApp;
class ConfigManager;

class AuthController : public BaseController
{
  public:
    AuthController(YarrboardApp& app);

    bool setup() override;

    UserRole getUserRole(JsonVariantConst input, byte mode, int socket);
    const char* getRoleText(UserRole role);
    bool hasPermission(UserRole requiredRole, UserRole userRole);

    void logSerialClientIn(UserRole role);
    void logSerialClientOut();
    bool isSerialAuthenticated();

    bool logClientIn(int socket, UserRole role);
    bool isLoggedIn(JsonVariantConst input, byte mode, int socket);
    void removeClientFromAuthList(int socket);
    bool isApiClientLoggedIn(JsonVariantConst doc);

    // Getters for external callers
    const char* getAdminPass() const;
    UserRole getDefaultRole() const;
    UserRole getSerialRole() const;
    const etl::vector<AuthenticatedClient, YB_CLIENT_LIMIT>& getAuthenticatedClients() const;

    void handleLogin(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleLogout(JsonVariantConst input, JsonVariant output, ProtocolContext context);
    void handleSetAuthenticationConfig(JsonVariantConst input, JsonVariant output, ProtocolContext context);

    // Config hooks
    void generateAuthConfig(JsonVariant output);
    void loadAuthConfig(JsonVariant config);
    bool loadAdminConfigHook(JsonVariant config, char* error, size_t len) override;
    void generateAdminConfigHook(JsonVariant output) override;

  private:
    bool is_serial_authenticated = false;

    char admin_user[YB_USERNAME_LENGTH];
    char admin_pass[YB_PASSWORD_LENGTH];
    char guest_user[YB_USERNAME_LENGTH];
    char guest_pass[YB_PASSWORD_LENGTH];
    UserRole app_default_role;
    UserRole serial_role;

    etl::vector<AuthenticatedClient, YB_CLIENT_LIMIT> authenticatedClients;

    bool addClientToAuthList(int socket, UserRole role);
    bool isWebsocketClientLoggedIn(JsonVariantConst input, int socket);
    bool isSerialClientLoggedIn(JsonVariantConst input);
    bool checkLoginCredentials(JsonVariantConst doc, UserRole& role);
    UserRole getWebsocketRole(JsonVariantConst doc, int socket);
};

#endif /* !YARR_AUTH_H */