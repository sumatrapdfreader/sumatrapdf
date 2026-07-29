// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// mingw-w64 headers before v12 (e.g. Ubuntu 24.04's 11.0.1) predate the
// Windows 10 20H1 / Windows 11 DWM additions this library uses
#ifdef __MINGW32__
#include <_mingw.h>
#if __MINGW64_VERSION_MAJOR < 12
typedef enum {
    DWMWCP_DEFAULT = 0,
    DWMWCP_DONOTROUND = 1,
    DWMWCP_ROUND = 2,
    DWMWCP_ROUNDSMALL = 3,
} DWM_WINDOW_CORNER_PREFERENCE;
typedef enum {
    DWMSBT_AUTO = 0,
    DWMSBT_NONE = 1,
    DWMSBT_MAINWINDOW = 2,
    DWMSBT_TRANSIENTWINDOW = 3,
    DWMSBT_TABBEDWINDOW = 4,
} DWM_SYSTEMBACKDROP_TYPE;
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#define DWMWA_BORDER_COLOR 34
#define DWMWA_CAPTION_COLOR 35
#define DWMWA_TEXT_COLOR 36
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#define DWMWA_COLOR_DEFAULT 0xFFFFFFFF
#define DWMWA_COLOR_NONE 0xFFFFFFFE
#endif
#endif
