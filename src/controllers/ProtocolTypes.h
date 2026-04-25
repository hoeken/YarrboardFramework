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

#ifndef YARR_PROTOCOL_TYPES_H
#define YARR_PROTOCOL_TYPES_H

#include "controllers/AuthTypes.h"
#include <stdint.h>

typedef enum {
  YBP_MODE_NONE,
  YBP_MODE_WEBSOCKET,
  YBP_MODE_HTTP,
  YBP_MODE_SERIAL,
  YBP_MODE_MQTT
} YBMode;

struct ProtocolContext {
    YBMode mode = YBP_MODE_NONE;
    UserRole role = NOBODY;
    uint32_t clientId = 0;
};

#endif /* !YARR_PROTOCOL_TYPES_H */
