/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The colors Windows draws its own UI in. The default theme defers to them so
// the app follows the system appearance, and every theme defers to them in high
// contrast mode, where the user's palette is the whole point. Theme.cpp asks
// for them by name so it doesn't have to know the OS palette itself.

#include "base/Base.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "Theme.h"

Color SysWindowBgColor() {
    return GetSysColor(COLOR_WINDOW);
}

Color SysWindowTextColor() {
    return GetSysColor(COLOR_WINDOWTEXT);
}

Color SysControlTextColor() {
    return GetSysColor(COLOR_BTNTEXT);
}

Color SysDisabledTextColor() {
    return GetSysColor(COLOR_GRAYTEXT);
}

Color SysLinkColor() {
    return GetSysColor(COLOR_HOTLIGHT);
}

Color SysHighlightBgColor() {
    return GetSysColor(COLOR_HIGHLIGHT);
}

Color SysHighlightTextColor() {
    return GetSysColor(COLOR_HIGHLIGHTTEXT);
}
