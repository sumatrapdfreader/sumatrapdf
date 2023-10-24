/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

/* Adding themes instructions:
Add one to kThemeCount (Theme.h)
If kThemeCount > 20, you will have to update IDM_CHANGE_THEME_LAST (resource.h)
Copy one of the theme declarations below
Rename it to whatever and change all of the properties as desired
Add a pointer to your new struct to the g_themes array below

Try not to enter a color code twice. If you use it more than once in a theme,
reference it through the theme struct the second time. See g_themeDark.document for example.
You can also use methods like AdjustLightness2 to modify existing colors. If you use a
color in multiple themes, you may want to define it in the color definitions section.This
makes themes easier to modify and update.

Note: Colors are in format 0xBBGGRR, recommended to use RgbToCOLORREF
*/

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "Translations.h"

constexpr COLORREF kColBlack = 0x000000;
constexpr COLORREF kColWhite = 0xFFFFFF;
// #define kColWhiteish 0xEBEBF9
// #define kColDarkGray 0x424242

// clang-format off
static Theme gThemeLight = {
    // Theme Name
    _TRN("Light"),
    // Window theme
    {
        // Main Background Color
        // Background color comparison:
        // Adobe Reader X   0x565656 without any frame border
        // Foxit Reader 5   0x9C9C9C with a pronounced frame shadow
        // PDF-XChange      0xACA899 with a 1px frame and a gradient shadow
        // Google Chrome    0xCCCCCC with a symmetric gradient shadow
        // Evince           0xD7D1CB with a pronounced frame shadow
        // SumatraPDF (old) 0xCCCCCC with a pronounced frame shadow

        // it's very light gray but not white so that there's contrast between
        // background and thumbnail, which often have white background because
        // most PDFs have white background.
        RgbToCOLORREF(0xF2F2F2),
        // Control background Color
        kColWhite,
        // Main Text Color
        kColBlack,
        // Main Link Color
        RgbToCOLORREF(0x0020A0)
    },
    // Notifications
    {
        // Background color
        kColWhite,
        // Text color
        gThemeLight.window.textColor,
        // Highlight color
        RgbToCOLORREF(0xFFEE70),
        // Highlight text color
        RgbToCOLORREF(0x8d0801),
        // Progress bar color
        gThemeLight.window.linkColor
    },
    // Colorize standard controls
    false
};

static Theme gThemeDark = {
    // Theme Name
    _TRN("Dark"),
    // Window theme
    {
        // Main Background Color
        RgbToCOLORREF(0x263238),
         // Control background Color
        RgbToCOLORREF(0x263238),
        // Main Text Color
        //kColWhite,
        AdjustLightness2(RgbToCOLORREF(0x263238), 150),
        // Main Link Color
        //RgbToCOLORREF(0x80CBAD)
        AdjustLightness2(RgbToCOLORREF(0x263238), 110),
    },
    // Notifications
    {
        // Background color
        AdjustLightness2(gThemeDark.window.backgroundColor, 10),
        // Text color
        gThemeDark.window.textColor,
        // Highlight color
        /*AdjustLightness2*/(RgbToCOLORREF(0x33434B), 10),
        // Highlight text color
        gThemeDark.window.textColor,
        // Progress bar color
        gThemeDark.window.linkColor
    },
    // Colorize standard controls
    true
};

static Theme gThemeDarker = {
    // Theme Name
    _TRN("Darker"),
    // Window theme
    {
        // Main Background Color
        RgbToCOLORREF(0x2D2D30),
         // Control background Color
        RgbToCOLORREF(0x2D2D30),
        // Main Text Color
        AdjustLightness2(RgbToCOLORREF(0x2D2D30), 150),
        //kColWhite,
        // Main Link Color
        AdjustLightness2(RgbToCOLORREF(0x2D2D30), 110),
    },
    // Notifications
    {
        // Background color
        AdjustLightness2(gThemeDarker.window.backgroundColor, 10),
        // Text color
        gThemeDarker.window.textColor,
        // Highlight color
        AdjustLightness2(RgbToCOLORREF(0x3E3E42), 10),
        // Highlight text color
        gThemeDarker.window.textColor,
        // Progress bar color
        gThemeDarker.window.linkColor
    },
    // Colorize standard controls
    true
};
// clang-format on

static Theme* gThemes[kThemeCount] = {
    &gThemeLight,
    &gThemeDark,
    &gThemeDarker,
};

Theme* gCurrentTheme = &gThemeLight;
static int currentThemeIndex = 0;

int GetCurrentThemeIndex() {
    return currentThemeIndex;
}

extern void UpdateAfterThemeChange();

void SetThemeByIndex(int themeIdx) {
    CrashIf((themeIdx < 0) || (themeIdx >= kThemeCount));
    currentThemeIndex = themeIdx;
    gCurrentTheme = gThemes[currentThemeIndex];
    str::ReplaceWithCopy(&gGlobalPrefs->theme, gCurrentTheme->name);
    UpdateAfterThemeChange();
};

void SelectNextTheme() {
    int newIdx = (currentThemeIndex + 1) % kThemeCount;
    SetThemeByIndex(newIdx);
}

// not case sensitive
static Theme* GetThemeByName(const char* name, int& idx) {
    for (int i = 0; i < kThemeCount; i++) {
        if (str::EqI(gThemes[i]->name, name)) {
            idx = i;
            return gThemes[i];
        }
    }
    return nullptr;
}

// this is the default aggressive yellow that we suppress
constexpr COLORREF kMainWinBgColDefault = (RGB(0xff, 0xf2, 0) - 0x80000000);

static bool IsDefaultMainWinColor(ParsedColor* col) {
    return col->parsedOk && col->col == kMainWinBgColDefault;
}

// call after loading settings
void SetCurrentThemeFromSettings() {
    const char* name = gGlobalPrefs->theme;
    int idx = 0;
    auto theme = GetThemeByName(name, idx);
    if (!theme) {
        // invalid name, reset to light theme
        str::ReplaceWithCopy(&gGlobalPrefs->theme, gThemeLight.name);
        return;
    }
    SetThemeByIndex(idx);

    ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
    bool isDefault = IsDefaultMainWinColor(bgParsed);
    if (isDefault) {
        gThemeLight.colorizeControls = false;
        gThemeLight.window.controlBackgroundColor = kColWhite;
    } else {
        gThemeLight.colorizeControls = true;
        gThemeLight.window.controlBackgroundColor = bgParsed->col;
    }
}

void GetDocumentColors(COLORREF& text, COLORREF& bg) {
    text = kColBlack;
    bg = kColWhite;

    if (currentThemeIndex == 0) {
        // for backwards compat light theme respects the old customization colors
        ParsedColor* parsedCol;
        if (gGlobalPrefs->fixedPageUI.invertColors) {
            parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.textColor);
        } else {
            parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.backgroundColor);
        }
        bg = parsedCol->col;
        if (gGlobalPrefs->fixedPageUI.invertColors) {
            parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.backgroundColor);
        } else {
            parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.textColor);
        }
        text = parsedCol->col;
        return;
    }

    if (gGlobalPrefs->fixedPageUI.invertColors) {
        // if we're inverting in non-default themes, the colors
        // should match the colors of the window
        // TODO: this probably only makes sense for dark themes
        text = gCurrentTheme->window.textColor;
        bg = gCurrentTheme->window.backgroundColor;
        if (IsLightColor(bg)) {
            bg = AdjustLightness2(bg, -8);
        } else {
            bg = AdjustLightness2(bg, 8);
        }
    }
}

COLORREF GetControlBackgroundColor() {
    // note: we can change it in ThemeUpdateAfterLoadSettings()
    return gCurrentTheme->window.controlBackgroundColor;
}

// TODO: migrate from prefs to theme.
COLORREF GetMainWindowBackgroundColor() {
    COLORREF bgColor = gCurrentTheme->window.backgroundColor;
    if (currentThemeIndex == 0) {
        // Special behavior for light theme.
        ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
        if (!IsDefaultMainWinColor(bgParsed)) {
            bgColor = bgParsed->col;
        }
    }
    return bgColor;
}
