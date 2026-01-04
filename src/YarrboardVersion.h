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

#pragma once

/** Major version number (X.x.x) */
#define YARRBOARD_VERSION_MAJOR 2
/** Minor version number (x.X.x) */
#define YARRBOARD_VERSION_MINOR 0
/** Patch version number (x.x.X) */
#define YARRBOARD_VERSION_PATCH 1

/**
 * Macro to convert Yarrboard Framework version number into an integer
 *
 * To be used in comparisons, such as YARRBOARD_VERSION >= YARRBOARD_VERSION_VAL(2, 0, 0)
 */
#define YARRBOARD_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))

/**
 * Current Yarrboard Framework version, as an integer
 *
 * To be used in comparisons, such as YARRBOARD_VERSION >= YARRBOARD_VERSION_VAL(2, 0, 0)
 */
#define YARRBOARD_VERSION YARRBOARD_VERSION_VAL(YARRBOARD_VERSION_MAJOR, YARRBOARD_VERSION_MINOR, YARRBOARD_VERSION_PATCH)

/**
 * Current Yarrboard Framework version, as string
 */
#ifndef YARRBOARD_df2xstr
  #define YARRBOARD_df2xstr(s) #s
#endif
#ifndef YARRBOARD_df2str
  #define YARRBOARD_df2str(s) YARRBOARD_df2xstr(s)
#endif
#define YARRBOARD_VERSION_STR YARRBOARD_df2str(YARRBOARD_VERSION_MAJOR) "." YARRBOARD_df2str(YARRBOARD_VERSION_MINOR) "." YARRBOARD_df2str(YARRBOARD_VERSION_PATCH)
