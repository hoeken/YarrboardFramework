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

#ifndef YarrboardApp_h
#define YarrboardApp_h

#include "ConfigManager.h"
#include "YarrboardNetworkManager.h"

class YarrboardApp
{
  public:
    ConfigManager config;
    YarrboardNetworkManager network;

    YarrboardApp();

    void setup();
    void loop();

    void full_setup();
    void full_loop();

  private:
    unsigned int framerate;
};

#endif /* YarrboardApp_h */