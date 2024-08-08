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
#include "AppSettings.h"
#include "Commands.h"
#include "DisplayMode.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "Translations.h"
#include "Toolbar.h"

#include "utils/Log.h"

constexpr COLORREF kColBlack = 0x000000;
constexpr COLORREF kColWhite = 0xFFFFFF;
// #define kColWhiteish 0xEBEBF9
// #define kColDarkGray 0x424242

struct Theme {
    // Name of the theme
    const char* name;

    // Background color of recently added, about, and properties menus
    COLORREF backgroundColor;
    // Background color of controls, menus, non-client areas, etc.
    COLORREF controlBackgroundColor;
    // Text color of recently added, about, and properties menus
    COLORREF textColor;
    // Link color on recently added, about, and properties menus
    COLORREF linkColor;

    // Style of notifications
    // Background color of the notification window
    COLORREF notifBackgroundColor;
    // Text color of the notification window
    COLORREF notifTextColor;

    // Whether or not we colorize standard Windows controls and window areas
    bool colorizeControls;
};

// clang-format off
static Theme gThemeLight = {
    // Theme Name
    _TRN("Light"),

    // Window theme
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
    RgbToCOLORREF(0x0020A0),

    // Notifications
    // Background color
    kColWhite,
    // Text color
    gThemeLight.textColor,

    // Colorize standard controls
    false
};

static Theme gThemeDark = {
    // Theme Name
    _TRN("Dark"),
    // Window theme
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

    // Notifications
    // Background color
    AdjustLightness2(gThemeDark.backgroundColor, 10),
    // Text color
    gThemeDark.textColor,
    // Colorize standard controls
    true
};

/*
// Highlight color
AdjustLightness2(RgbToCOLORREF(0x33434B), 10),
// Highlight text color
gThemeDark.textColor,

// Highlight color
AdjustLightness2(RgbToCOLORREF(0x3E3E42), 10),
// Highlight text color
gThemeDarker.textColor,
*/

static Theme gThemeDarker = {
    // Theme Name
    _TRN("Darker"),
    // Window theme
    // Main Background Color
    RgbToCOLORREF(0x2D2D30),
    // Control background Color
    RgbToCOLORREF(0x2D2D30),
    // Main Text Color
    AdjustLightness2(RgbToCOLORREF(0x2D2D30), 150),
    //kColWhite,
    // Main Link Color
    AdjustLightness2(RgbToCOLORREF(0x2D2D30), 110),

    // Notifications
    // Background color
    AdjustLightness2(gThemeDarker.backgroundColor, 10),
    // Text color
    gThemeDarker.textColor,

    // Colorize standard controls
    true
};
// clang-format on

static Theme* gThemes[] = {
    &gThemeLight,
    &gThemeDark,
    &gThemeDarker,
};

constexpr const int kThemeCount = dimofi(gThemes);

Theme* gCurrentTheme = &gThemeLight;
static int currentThemeIndex = 0;

int GetCurrentThemeIndex() {
    return currentThemeIndex;
}

extern void UpdateAfterThemeChange();

int gFirstSetThemeCmdId;
int gLastSetThemeCmdId;
int gCurrSetThemeCmdId;

void CreateThemeCommands() {
    CustomCommand* cmd;
    for (int i = 0; i < kThemeCount; i++) {
        const char* themeName = gThemes[i]->name;
        auto args = NewStringArg(kCmdArgTheme, themeName);
        cmd = CreateCustomCommand(themeName, CmdSetTheme, args);
        cmd->name = str::Format("Set theme '%s'", themeName);
        if (i == 0) {
            gFirstSetThemeCmdId = cmd->id;
        } else if (i == kThemeCount - 1) {
            gLastSetThemeCmdId = cmd->id;
        }
    }
    gCurrSetThemeCmdId = gFirstSetThemeCmdId + 0;
}

void SetThemeByIndex(int themeIdx) {
    ReportIf((themeIdx < 0) || (themeIdx >= kThemeCount));
    if (themeIdx >= kThemeCount) {
        themeIdx = 0;
    }
    currentThemeIndex = themeIdx;
    gCurrSetThemeCmdId = gFirstSetThemeCmdId + themeIdx;
    gCurrentTheme = gThemes[currentThemeIndex];
    str::ReplaceWithCopy(&gGlobalPrefs->theme, gCurrentTheme->name);
    UpdateAfterThemeChange();
};

void SelectNextTheme() {
    int newIdx = (currentThemeIndex + 1) % kThemeCount;
    SetThemeByIndex(newIdx);
}

// not case sensitive
static int GetThemeByName(const char* name) {
    for (int i = 0; i < kThemeCount; i++) {
        if (str::EqI(gThemes[i]->name, name)) {
            return i;
        }
    }
    return -1;
}

// this is the default aggressive yellow that we suppress
constexpr COLORREF kMainWinBgColDefault = (RGB(0xff, 0xf2, 0) - 0x80000000);

static bool IsDefaultMainWinColor(ParsedColor* col) {
    return col->parsedOk && col->col == kMainWinBgColDefault;
}

void SetTheme(const char* name) {
    int idx = GetThemeByName(name);
    if (idx < 0) {
        // invalid name, reset to light theme
        str::ReplaceWithCopy(&gGlobalPrefs->theme, gThemeLight.name);
        idx = 0;
    }
    SetThemeByIndex(idx);
}

// call after loading settings
void SetCurrentThemeFromSettings() {
    SetTheme(gGlobalPrefs->theme);
    ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
    bool isDefault = IsDefaultMainWinColor(bgParsed);
    if (isDefault) {
        gThemeLight.colorizeControls = false;
        gThemeLight.controlBackgroundColor = kColWhite;
    } else {
        gThemeLight.colorizeControls = true;
        gThemeLight.controlBackgroundColor = bgParsed->col;
    }
}

// if is dark, makes lighter, if light, makes darker
static COLORREF AdjustLightOrDark(COLORREF col, float n) {
    if (IsLightColor(col)) {
        col = AdjustLightness2(col, -n);
    } else {
        col = AdjustLightness2(col, n);
    }
    return col;
}

COLORREF ThemeDocumentColors(COLORREF& bg) {
    COLORREF text = kColBlack;
    bg = kColWhite;

    ParsedColor* parsedCol;
    parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.textColor);
    if (parsedCol->parsedOk) {
        text = parsedCol->col;
    }

    parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.backgroundColor);
    if (parsedCol->parsedOk) {
        bg = parsedCol->col;
    }

    if (!gGlobalPrefs->fixedPageUI.invertColors) {
        return text;
    }

    // default colors
    if (gCurrentTheme == &gThemeLight) {
        std::swap(text, bg);
        return text;
    }

    // if we're inverting in non-default themes, the colors
    // should match the colors of the window
    text = ThemeWindowTextColor();
    bg = gCurrentTheme->backgroundColor;
    bg = AdjustLightOrDark(bg, 8);
    return text;
}

COLORREF ThemeControlBackgroundColor() {
    // note: we can change it in ThemeUpdateAfterLoadSettings()
    return gCurrentTheme->controlBackgroundColor;
}

// TODO: migrate from prefs to theme.
COLORREF ThemeMainWindowBackgroundColor() {
    COLORREF bgColor = gCurrentTheme->backgroundColor;
    if (currentThemeIndex == 0) {
        // Special behavior for light theme.
        ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
        if (!IsDefaultMainWinColor(bgParsed)) {
            bgColor = bgParsed->col;
        }
    }
    return bgColor;
}

COLORREF ThemeWindowBackgroundColor() {
    return gCurrentTheme->backgroundColor;
}

COLORREF ThemeWindowTextColor() {
    return gCurrentTheme->textColor;
}

COLORREF ThemeWindowTextDisabledColor() {
    auto col = gCurrentTheme->textColor;
    // TODO: probably add textDisabledColor
    auto col2 = AdjustLightOrDark(col, 0x7f);
    return col2;
}

COLORREF ThemeWindowControlBackgroundColor() {
    return gCurrentTheme->controlBackgroundColor;
}

COLORREF ThemeWindowLinkColor() {
    return gCurrentTheme->linkColor;
}

COLORREF ThemeNotificationsBackgroundColor() {
    return gCurrentTheme->notifBackgroundColor;
}

COLORREF ThemeNotificationsTextColor() {
    return gCurrentTheme->notifTextColor;
}

COLORREF ThemeNotificationsHighlightColor() {
    return RgbToCOLORREF(0xFFEE70); // yellowish
}

COLORREF ThemeNotificationsHighlightTextColor() {
    return RgbToCOLORREF(0x8d0801); // reddish
}

COLORREF ThemeNotificationsProgressColor() {
    return gCurrentTheme->linkColor;
}

bool ThemeColorizeControls() {
    if (gCurrentTheme->colorizeControls) {
        return true;
    }
    return !IsMenuFontSizeDefault();
}

void dumpThemes() {
    logf("Themes [\n");
    for (Theme* theme : gThemes) {
        auto w = *theme;
        logf("    [\n");
        logf("        Name = '%s'\n", w.name);
        logf("        BackgroundColor = %s\n", SerializeColorTemp(w.backgroundColor));
        logf("        ControlBackgroundColor = %s\n", SerializeColorTemp(w.controlBackgroundColor));
        logf("        TextColor = %s\n", SerializeColorTemp(w.textColor));
        logf("        LinkColor = %s\n", SerializeColorTemp(w.linkColor));
        logf("        NoitfBackgroundColor = %s\n", SerializeColorTemp(w.notifBackgroundColor));
        logf("        NotifTextColor = %s\n", SerializeColorTemp(w.notifTextColor));
        logf("    ]\n");
    }
    logf("]\n");
}
