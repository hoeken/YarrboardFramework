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

#ifndef YARR_DEBUG_CONTROLLER_H
#define YARR_DEBUG_CONTROLLER_H

#include "IntervalTimer.h"
#include "controllers/BaseController.h"

class YarrboardApp;
class ConfigManager;

class DebugController : public BaseController
{
  public:
    DebugController(YarrboardApp& app);

    IntervalTimer it; // intentionally public: other controllers call it.time() for loop profiling

    bool setup() override;
    void loop() override;
    void generateStatsHook(JsonVariant output) override;

    void handleCrashMe(JsonVariantConst input, JsonVariant output);

    String getResetReason();
    bool checkCoreDump();
    bool saveCoreDumpToFile(const char* path);
    bool deleteCoreDump();
    bool hasCoredump() { return has_coredump; }

    static int vprintf(const char* fmt, va_list args);

  private:
    bool has_coredump = false;

    void crashMeHard();
};

#endif /* !YARR_DEBUG_CONTROLLER_H */