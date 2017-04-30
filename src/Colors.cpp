#include "BaseUtil.h"

#include "WinUtil.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Colors.h"

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
#if defined(DRAW_PAGE_SHADOWS)
// SumatraPDF (old) 0xCCCCCC with a pronounced frame shadow
#define COL_WINDOW_BG RGB(0xCC, 0xCC, 0xCC)
#define COL_PAGE_FRAME RGB(0x88, 0x88, 0x88)
#define COL_PAGE_SHADOW RGB(0x40, 0x40, 0x40)
#else
// SumatraPDF       0x999999 without any frame border
#define COL_WINDOW_BG RGB(0x99, 0x99, 0x99)
#endif

static COLORREF rgb_to_bgr(COLORREF rgb) {
    return ((rgb & 0x0000FF) << 16) | (rgb & 0x00FF00) | ((rgb & 0xFF0000) >> 16);
}

COLORREF AdjustLightness(COLORREF c, float factor) {
    BYTE R = GetRValueSafe(c), G = GetGValueSafe(c), B = GetBValueSafe(c);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Hue_and_chroma
    BYTE M = std::max(std::max(R, G), B), m = std::min(std::min(R, G), B);
    if (M == m) {
        // for grayscale values, lightness is proportional to the color value
        BYTE X = (BYTE)limitValue((int)floorf(M * factor + 0.5f), 0, 255);
        return RGB(X, X, X);
    }
    BYTE C = M - m;
    BYTE Ha = (BYTE)abs(M == R ? G - B : M == G ? B - R : R - G);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Lightness
    float L2 = (float)(M + m);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Saturation
    float S = C / (L2 > 255.0f ? 510.0f - L2 : L2);

    L2 = limitValue(L2 * factor, 0.0f, 510.0f);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#From_HSL
    float C1 = (L2 > 255.0f ? 510.0f - L2 : L2) * S;
    float X1 = C1 * Ha / C;
    float m1 = (L2 - C1) / 2;
    R = (BYTE)floorf((M == R ? C1 : m != R ? X1 : 0) + m1 + 0.5f);
    G = (BYTE)floorf((M == G ? C1 : m != G ? X1 : 0) + m1 + 0.5f);
    B = (BYTE)floorf((M == B ? C1 : m != B ? X1 : 0) + m1 + 0.5f);
    return RGB(R, G, B);
}

// Adjusts lightness by 1/255 units.
COLORREF AdjustLightness2(COLORREF c, float units) {
    float lightness = GetLightness(c);
    units = limitValue(units, -lightness, 255.0f - lightness);
    if (0.0f == lightness)
        return RGB(BYTE(units + 0.5f), BYTE(units + 0.5f), BYTE(units + 0.5f));
    return AdjustLightness(c, 1.0f + units / lightness);
}

// cf. http://en.wikipedia.org/wiki/HSV_color_space#Lightness
float GetLightness(COLORREF c) {
    BYTE R = GetRValueSafe(c), G = GetGValueSafe(c), B = GetBValueSafe(c);
    BYTE M = std::max(std::max(R, G), B), m = std::min(std::min(R, G), B);
    return (M + m) / 2.0f;
}

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

    if (col == AppColor::MainWindowBg) {
        return ABOUT_BG_GRAY_COLOR;
    }

    if (col == AppColor::MainWindowText) {
        return COL_BLACK;
    }

    if (col == AppColor::MainWindowLink) {
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

    if (col == AppColor::NotificationsBg) {
        return GetAppColor(AppColor::MainWindowBg);
    }

    if (col == AppColor::NotificationsText) {
        return GetAppColor(AppColor::MainWindowText);
    }

    if (col == AppColor::NotificationsHighlightBg) {
        return rgb_to_bgr(0x3399ff);
    }

    if (col == AppColor::NotificationsHighlightText) {
        return COL_WHITE;
    }

    if (col == AppColor::NotifcationsProgress) {
        return GetAppColor(AppColor::MainWindowLink);
    }

    if (col == AppColor::TabSelectedBg) {
        return COL_WHITE;
    }

    if (col == AppColor::TabSelectedText) {
        return COL_DARK_GRAY;
    }

    if (col == AppColor::TabSelectedCloseX) {
        auto c = GetAppColor(AppColor::TabBackgroundBg);
        return AdjustLightness2(c, -60);
    }

    if (col == AppColor::TabSelectedCloseCircle) {
        return rgb_to_bgr(0xC13535);
    }

    if (col == AppColor::TabBackgroundBg) {
        return COL_LIGHTER_GRAY;
    }

    if (col == AppColor::TabBackgroundText) {
        return COL_DARK_GRAY;
    }

    if (col == AppColor::TabBackgroundCloseX) {
        return GetAppColor(AppColor::TabSelectedCloseX);
    }

    if (col == AppColor::TabBackgroundCloseCircle) {
        return GetAppColor(AppColor::TabSelectedCloseCircle);
    }

    if (col == AppColor::TabHighlightedBg) {
        return COL_LIGHT_GRAY;
    }

    if (col == AppColor::TabHighlightedText) {
        return COL_BLACK;
    }

    if (col == AppColor::TabHighlightedCloseX) {
        return GetAppColor(AppColor::TabSelectedCloseX);
    }

    if (col == AppColor::TabHighlightedCloseCircle) {
        return GetAppColor(AppColor::TabSelectedCloseCircle);
    }

    if (col == AppColor::TabHoveredCloseX) {
        return COL_WHITEISH;
    }

    if (col == AppColor::TabHoveredCloseCircle) {
        return GetAppColor(AppColor::TabSelectedCloseCircle);
    }

    if (col == AppColor::TabClickedCloseX) {
        return GetAppColor(AppColor::TabHoveredCloseX);
    }

    if (col == AppColor::TabClickedCloseCircle) {
        auto c = GetAppColor(AppColor::TabSelectedCloseCircle);
        AdjustLightness2(c, -10);
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
