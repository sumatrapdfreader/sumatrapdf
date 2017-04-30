#include "BaseUtil.h"

#include "WinUtil.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Colors.h"

#define COL_BLACK RGB(0, 0, 0)
#define COL_BLUE_LINK RGB(0x00, 0x20, 0xa0)

// "SumatraPDF yellow" similar to the one use for icon and installer
#define ABOUT_BG_LOGO_COLOR RGB(0xFF, 0xF2, 0x00)

// it's very light gray but not white so that there's contrast between
// background and thumbnail, which often have white background because
// most PDFs have white background
#define ABOUT_BG_GRAY_COLOR RGB(0xF2, 0xF2, 0xF2)

// for backward compatibility use a value that older versions will render as yellow
#define ABOUT_BG_COLOR_DEFAULT (RGB(0xff, 0xf2, 0) - 0x80000000)

// Background color comparison:
// Adobe Reader X   0x565656 without any frame border
// Foxit Reader 5   0x9C9C9C with a pronounced frame shadow
// PDF-XChange      0xACA899 with a 1px frame and a gradient shadow
// Google Chrome    0xCCCCCC with a symmetric gradient shadow
// Evince           0xD7D1CB with a pronounced frame shadow
#ifdef DRAW_PAGE_SHADOWS
// SumatraPDF (old) 0xCCCCCC with a pronounced frame shadow
#define COL_WINDOW_BG RGB(0xCC, 0xCC, 0xCC)
#define COL_PAGE_FRAME RGB(0x88, 0x88, 0x88)
#define COL_PAGE_SHADOW RGB(0x40, 0x40, 0x40)
#else
// SumatraPDF       0x999999 without any frame border
#define COL_WINDOW_BG RGB(0x99, 0x99, 0x99)
#endif

// returns the background color for start page, About window and Properties dialog
static COLORREF GetAboutBgColor() {
    COLORREF bgColor = ABOUT_BG_GRAY_COLOR;
    if (ABOUT_BG_COLOR_DEFAULT != gGlobalPrefs->mainWindowBackground) {
        bgColor = gGlobalPrefs->mainWindowBackground;
    }
    return bgColor;
}

// returns the background color for the "SumatraPDF" logo in start page and About window
static COLORREF GetLogoBgColor() {
#ifdef ABOUT_USE_LESS_COLORS
    return ABOUT_BG_LOGO_COLOR;
#else
    return GetAboutBgColor();
#endif
}

static COLORREF GetNoDocBgColor() {
    // use the system background color if the user has non-default
    // colors for text (not black-on-white) and also wants to use them
    bool useSysColor = gGlobalPrefs->useSysColors &&
                       (GetSysColor(COLOR_WINDOWTEXT) != WIN_COL_BLACK || GetSysColor(COLOR_WINDOW) != WIN_COL_WHITE);
    if (useSysColor) {
        return GetSysColor(COLOR_BTNFACE);
    }

    return COL_WINDOW_BG;
}

COLORREF GetAppColor(AppColor col) {
    if (col == AppColor::NoDocBg) {
        // GetCurrentTheme()->document.canvasColor
        return GetNoDocBgColor();
    }
    if (col == AppColor::AboutBg) {
        return GetAboutBgColor();
    }
    if (col == AppColor::LogoBg) {
        return GetLogoBgColor();
    }
    if (col == AppColor::MainWindowText) {
        return GetSysColor(COLOR_WINDOWTEXT);
    }

    if (col == AppColor::NotificationsBg) {
        return GetSysColor(COLOR_WINDOW);
    }

    if (col == AppColor::NotificationsText) {
        return GetSysColor(COLOR_WINDOWTEXT);
    }

    if (col == AppColor::NotificationsHighlightBg) {
        return GetSysColor(COLOR_HIGHLIGHT);
    }

    if (col == AppColor::NotificationsHighlightText) {
        return GetSysColor(COLOR_HIGHLIGHTTEXT);
    }

    if (col == AppColor::NotifcationsProgress) {
        return COL_BLACK;
    }

    if (col == AppColor::Link) {
        return COL_BLUE_LINK;
    }

    // TODO: different for fixed vs. ebook page
    if (col == AppColor::DocumentBg) {
        return gGlobalPrefs->fixedPageUI.backgroundColor;
    }

    // TODO: different for fixed vs. ebook page
    if (col == AppColor::DocumentText) {
        return gGlobalPrefs->fixedPageUI.textColor;
    }

    CrashIf(false);
    return COL_WINDOW_BG;
}

void GetFixedPageUiColors(COLORREF& text, COLORREF& bg) {
#if 0
    text = GetCurrentTheme()->document.textColor;
    bg = GetCurrentTheme()->document.backgroundColor;
#endif

    if (gGlobalPrefs->useSysColors) {
        text = GetSysColor(COLOR_WINDOWTEXT);
        bg = GetSysColor(COLOR_WINDOW);
    } else {
        text = gGlobalPrefs->fixedPageUI.textColor;
        bg = gGlobalPrefs->fixedPageUI.backgroundColor;
    }
    if (gGlobalPrefs->fixedPageUI.invertColors) {
        std::swap(text, bg);
    }
}

void GetEbookUiColors(COLORREF& text, COLORREF& bg) {
#if 0
    text = GetCurrentTheme()->document.textColor;
    bg = GetCurrentTheme()->document.backgroundColor;
#endif
    if (gGlobalPrefs->useSysColors) {
        text = GetSysColor(COLOR_WINDOWTEXT);
        bg = GetSysColor(COLOR_WINDOW);
    } else {
        text = gGlobalPrefs->ebookUI.textColor;
        bg = gGlobalPrefs->ebookUI.backgroundColor;
    }
    // TODO: respect gGlobalPrefs->fixedPageUI.invertColors?
}
