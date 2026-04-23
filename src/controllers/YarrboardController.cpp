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

#include "controllers/YarrboardController.h"
#include "ConfigManager.h"
#include "YarrboardApp.h"

YarrboardController::YarrboardController(YarrboardApp& app) : BaseController(app, "yarrboard")
{
}

void YarrboardController::generateConfigHook(JsonVariant output, UserRole role, ConfigPurpose purpose)
{
  // this is just static information for the UI or for shareable configs.
  if (purpose == ConfigPurpose::FIRMWARE)
    return;

  output["project_name"] = _app.project_name;
  output["hardware_url"] = _app.hardware_url;
  output["project_url"] = _app.project_url;
  output["git_url"] = _app.git_url;
  output["firmware_manifest_url"] = _app.ota.firmware_manifest_url;

  output["firmware_version"] = _app.firmware_version;
  output["hardware_version"] = _app.hardware_version;
  output["esp_idf_version"] = esp_get_idf_version();
  output["arduino_version"] = ESP_ARDUINO_VERSION_STR;
  output["psychic_http_version"] = PSYCHIC_VERSION_STR;
  output["yarrboard_framework_version"] = YARRBOARD_VERSION_STR;
#ifdef GIT_HASH
  output["git_hash"] = GIT_HASH;
#endif
#ifdef BUILD_TIME
  output["build_time"] = BUILD_TIME;
#endif
  output["is_development"] = YB_IS_DEVELOPMENT;
}
