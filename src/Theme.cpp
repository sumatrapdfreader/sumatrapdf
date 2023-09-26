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
    // Main window theme
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
    // Document style
    {
        // Canvas Color
        RgbToCOLORREF(0x999999),
        // Background Color
        kColWhite,
        // Text color
        kColBlack
    },
    // Notifications
    {
        // Background color
        kColWhite,
        // Text color
        gThemeLight.mainWindow.textColor,
        // Highlight color
        RgbToCOLORREF(0xFFEE70),
        // Highlight text color
        RgbToCOLORREF(0x8d0801),
        // Progress bar color
        gThemeLight.mainWindow.linkColor
    },
    // Colorize standard controls
    false
};

static Theme gThemeDark = {
    // Theme Name
    _TRN("Dark"),
    // Main window theme
    {
        // Main Background Color
        RgbToCOLORREF(0x263238),
         // Control background Color
        RgbToCOLORREF(0x263238),
        // Main Text Color
        kColWhite,
        // Main Link Color
        RgbToCOLORREF(0x80CBAD)
       
    },
    // Document style
    {
        // Canvas Color
        RgbToCOLORREF(0x1E272C),
        // Background Color
        gThemeDark.mainWindow.backgroundColor,
        // Text color
        gThemeDark.mainWindow.textColor
    },
    // Notifications
    {
        // Background color
        AdjustLightness2(gThemeDark.mainWindow.backgroundColor, 10),
        // Text color
        gThemeDark.mainWindow.textColor,
        // Highlight color
        AdjustLightness2(RgbToCOLORREF(0x33434B), 10),
        // Highlight text color
        gThemeDark.mainWindow.textColor,
        // Progress bar color
        gThemeDark.mainWindow.linkColor
    },
    // Colorize standard controls
    true
};

static Theme gThemeDarker = {
    // Theme Name
    _TRN("Darker"),
    // Main window theme
    {
        // Main Background Color
        RgbToCOLORREF(0x2D2D30),
         // Control background Color
        RgbToCOLORREF(0x2D2D30),
        // Main Text Color
        kColWhite,
        // Main Link Color
        RgbToCOLORREF(0x3081D4)
    },
    // Document style
    {
        // Canvas Color
        RgbToCOLORREF(0x1E1E1E),
        // Background Color
        gThemeDarker.mainWindow.backgroundColor,
        // Text color
        gThemeDarker.mainWindow.textColor
    },
    // Notifications
    {
        // Background color
        AdjustLightness2(gThemeDarker.mainWindow.backgroundColor, 10),
        // Text color
        gThemeDarker.mainWindow.textColor,
        // Highlight color
        AdjustLightness2(RgbToCOLORREF(0x3E3E42), 10),
        // Highlight text color
        gThemeDarker.mainWindow.textColor,
        // Progress bar color
        gThemeDarker.mainWindow.linkColor
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
#if 0
    // for light theme set invertColors
    // don't invert for light theme, invert for dark themes
    gGlobalPrefs->fixedPageUI.invertColors = (themeIdx == 0);
#endif
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
}

void GetDocumentColors(COLORREF& text, COLORREF& bg) {
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

    bg = gCurrentTheme->document.backgroundColor;
    text = gCurrentTheme->document.textColor;
    if (gGlobalPrefs->fixedPageUI.invertColors) {
        std::swap(bg, text);
    }
}

COLORREF GetMainWindowBackgroundColor() {
    COLORREF bgColor = gCurrentTheme->mainWindow.backgroundColor;
    // Special behavior for light theme.
    // TODO: migrate from prefs to theme.
    if (currentThemeIndex == 0) {
// for backward compatibility use a value that older versions will render as yellow
#define MAIN_WINDOW_BG_COLOR_DEFAULT (RGB(0xff, 0xf2, 0) - 0x80000000)

        ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
        if (MAIN_WINDOW_BG_COLOR_DEFAULT != bgParsed->col) {
            bgColor = bgParsed->col;
        }
    }
    return bgColor;
}
