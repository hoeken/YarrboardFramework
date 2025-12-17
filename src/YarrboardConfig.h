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

#include "YarrboardVersion.h"

#ifndef YB_FRAMEWORK_CONFIG_H
  #define YB_FRAMEWORK_CONFIG_H

  // basic board defines.
  #ifndef YB_IS_DEVELOPMENT
    #define YB_IS_DEVELOPMENT false
  #endif
  #ifndef YB_PIEZO_DEFAULT_MELODY
    #define YB_PIEZO_DEFAULT_MELODY "NONE"
  #endif
  #ifndef YB_DEFAULT_AP_MODE
    #define YB_DEFAULT_AP_MODE "ap"
  #endif
  #ifndef YB_DEFAULT_AP_SSID
    #define YB_DEFAULT_AP_SSID "Yarrboard"
  #endif
  #ifndef YB_DEFAULT_AP_PASS
    #define YB_DEFAULT_AP_PASS ""
  #endif

  // time before saving fade pwm to preserve flash
  #define YB_DUTY_SAVE_TIMEOUT 5000

  // if we have a status led, default it to one.
  #ifdef YB_HAS_STATUS_RGB
    #ifndef YB_STATUS_RGB_COUNT
      #define YB_STATUS_RGB_COUNT 1
    #endif
    #ifndef YB_STATUS_RGB_TYPE
      #define YB_STATUS_RGB_TYPE WS2812B
    #endif
    #ifndef YB_STATUS_RGB_ORDER
      #define YB_STATUS_RGB_ORDER GRB
    #endif
  #endif

  // default to 400khz
  #ifndef YB_I2C_SPEED
    #define YB_I2C_SPEED 400000
  #endif

  // max http clients - cannot go any higher than this without a custom esp-idf config
  #ifndef YB_CLIENT_LIMIT
    #define YB_CLIENT_LIMIT 13
  #endif

  // for handling messages outside of the loop
  #define YB_RECEIVE_BUFFER_COUNT 100

  // various string lengths
  #define YB_PREF_KEY_LENGTH      16
  #define YB_BOARD_NAME_LENGTH    32
  #define YB_USERNAME_LENGTH      32
  #define YB_PASSWORD_LENGTH      64
  #define YB_CHANNEL_NAME_LENGTH  64
  #define YB_CHANNEL_KEY_LENGTH   64
  #define YB_TYPE_LENGTH          32
  #define YB_WIFI_SSID_LENGTH     33
  #define YB_WIFI_PASSWORD_LENGTH 64
  #define YB_WIFI_MODE_LENGTH     16
  #define YB_HOSTNAME_LENGTH      64
  #define YB_MQTT_SERVER_LENGTH   128
  #define YB_ERROR_LENGTH         128
  #define YB_UUID_LENGTH          17
  #define YB_BOARD_CONFIG_PATH    "/yarrboard.json"

  #ifndef GIT_HASH
    #define GIT_HASH "???"
  #endif

  #ifndef BUILD_TIME
    #define BUILD_TIME "???"
  #endif

  #ifndef RA_DEFAULT_SIZE
    #define RA_DEFAULT_SIZE 50
  #endif

  #ifndef RA_DEFAULT_WINDOW
    #define RA_DEFAULT_WINDOW 1000
  #endif

  #ifndef YB_INPUT_DEBOUNCE_RATE_MS
    #define YB_INPUT_DEBOUNCE_RATE_MS 20
  #endif

  // Detect whether this build environment provides UsbSerial
  #if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE == 1
    #define YB_USB_SERIAL 1
  #endif

  #ifndef YB_MAX_CONTROLLERS
    #define YB_MAX_CONTROLLERS 30
  #endif

  #ifndef YB_PROTOCOL_MAX_COMMANDS
    #define YB_PROTOCOL_MAX_COMMANDS 50
  #endif

#endif // YARR_CONFIG_H