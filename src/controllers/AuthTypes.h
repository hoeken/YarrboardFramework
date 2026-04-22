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

#ifndef YARR_AUTH_TYPES_H
#define YARR_AUTH_TYPES_H

typedef enum {
  NOBODY,
  GUEST,
  ADMIN
} UserRole;

typedef enum {
  UI_VIEW, // Building the web interface (includes read-only data & runtime state)
  FIRMWARE // Downloading the config.json (only editable, persisted fields)
} ConfigPurpose;

typedef struct {
    int socket;
    UserRole role;
} AuthenticatedClient;

#endif /* !YARR_AUTH_TYPES_H */
