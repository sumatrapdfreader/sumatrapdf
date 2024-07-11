/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppColors.h"
#include "Theme.h"

// For reference of what used to be:
// https://github.com/sumatrapdfreader/sumatrapdf/commit/74aca9e1b78f833b0886db5b050c96045c0071a0

#define COL_WHITE RGB(0xff, 0xff, 0xff)
#define COL_WHITEISH RGB(0xEB, 0xEB, 0xF9);
#define COL_BLACK RGB(0, 0, 0)
#define COL_BLUE_LINK RGB(0x00, 0x20, 0xa0)

// for tabs
#define COL_RED RGB(0xff, 0x00, 0x00)
#define COL_LIGHT_GRAY RGB(0xde, 0xde, 0xde)
#define COL_LIGHTER_GRAY RGB(0xee, 0xee, 0xee)
#define COL_DARK_GRAY RGB(0x42, 0x42, 0x42)

// "SumatraPDF yellow" similar to the one use for icon and installer
#define ABOUT_BG_LOGO_COLOR RGB(0xFF, 0xF2, 0x00)

#define ABOUT_BG_GRAY_COLOR RGB(0xF2, 0xF2, 0xF2)

COLORREF GetAppColor(AppColor) {
    ReportIf(true);
    return 0;
}
