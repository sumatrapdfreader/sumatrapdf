/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

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
#include "DarkModeSubclass.h"

#include "utils/Log.h"

// allow only x64 and arm64 for compatibility for older OS
#if !defined(_DARKMODELIB_NOT_USED) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__arm64__) || defined(__arm64) || defined(_M_ARM64))
bool gUseDarkModeLib = true;
#else
bool gUseDarkModeLib = false;
#endif

/*
preserve those translations:
_TRN("Dark")
_TRN("Darker")
_TRN("Light")
*/

constexpr COLORREF kColBlack = 0x000000;
constexpr COLORREF kColWhite = 0xFFFFFF;
constexpr COLORREF kRedColor = RgbToCOLORREF(0xff0000);

static const char* themesTxt = R"(Themes [
    [
        Name = Light
        TextColor = #000000
        BackgroundColor = #f2f2f2
        ControlBackgroundColor = #ffffff
        LinkColor = #0020a0
        ColorizeControls = false
    ]
    [
        Name = Dark from 3.5
        TextColor = #bac9d0
        BackgroundColor = #263238
        ControlBackgroundColor = #263238
        LinkColor = #8aa3b0
        ColorizeControls = true
    ]
    [
        Name = Darker
        TextColor = #c3c3c6
        BackgroundColor = #2d2d30
        ControlBackgroundColor = #2d2d30
        LinkColor = #9999a0
        ColorizeControls = true
    ]
    [
        Name = Dark
        TextColor = #F9FAFB
        BackgroundColor = #000000
        ControlBackgroundColor = #000000
        LinkColor = #6B7280
        ColorizeControls = true
    ]
    [
        Name = Dark background Bright text
        TextColor = #ffffff
        BackgroundColor = #2d2d30
        ControlBackgroundColor = #2d2d30
        LinkColor = #9999a0
        ColorizeControls = true
    ]
    [
        Name = Solarized Light
        TextColor = #212323
        BackgroundColor = #fdf6e3
        ControlBackgroundColor = #eee8d5
        LinkColor = #9999a0
        ColorizeControls = true
    ]
]
)";

extern void UpdateAfterThemeChange();

int gFirstSetThemeCmdId;
int gLastSetThemeCmdId;
int gCurrSetThemeCmdId;

static Vec<Theme*>* gThemes = nullptr;
static int gThemeCount;
static int gCurrThemeIndex = 0;
static Theme* gCurrentTheme = nullptr;
static Theme* gThemeLight = nullptr;

bool IsCurrentThemeDefault() {
    return gCurrThemeIndex == 0;
}

void CreateThemeCommands() {
    delete gThemes;
    gThemes = new Vec<Theme*>();
    auto themes = ParseThemes(themesTxt);
    for (Theme* theme : *themes->themes) {
        gThemes->Append(theme);
    }

    for (Theme* theme : *gGlobalPrefs->themes) {
        gThemes->Append(theme);
    }

    gThemeCount = gThemes->Size();
    if (gCurrThemeIndex >= gThemeCount) {
        gCurrThemeIndex = 0;
    }
    gCurrentTheme = gThemes->At(gCurrThemeIndex);
    gThemeLight = gThemes->At(0);

    CustomCommand* cmd;
    for (int i = 0; i < gThemeCount; i++) {
        Theme* theme = gThemes->At(i);
        const char* themeName = theme->name;
        auto args = NewStringArg(kCmdArgTheme, themeName);
        cmd = CreateCustomCommand(themeName, CmdSetTheme, args);
        cmd->name = str::Format("Set theme '%s'", themeName);
        if (i == 0) {
            gFirstSetThemeCmdId = cmd->id;
        } else if (i == gThemeCount - 1) {
            gLastSetThemeCmdId = cmd->id;
        }
    }
    gCurrSetThemeCmdId = gFirstSetThemeCmdId + gCurrThemeIndex;
}

void SetThemeByIndex(int themeIdx) {
    ReportIf((themeIdx < 0) || (themeIdx >= gThemeCount));
    if (themeIdx >= gThemeCount) {
        themeIdx = 0;
    }
    gCurrThemeIndex = themeIdx;
    gCurrSetThemeCmdId = gFirstSetThemeCmdId + themeIdx;
    gCurrentTheme = gThemes->At(gCurrThemeIndex);
    str::ReplaceWithCopy(&gGlobalPrefs->theme, gCurrentTheme->name);
    if (gUseDarkModeLib) {
        // TODO: we should apply themes to every theme other than 0
        // but in Solarized Light in Find dialog's input field text is invisible i.e. black
        // UINT mode = themeIdx == 0 ? kModeClassic : kModeDark;
        const bool isDarkCol = DarkMode::isColorDark(ThemeWindowControlBackgroundColor());
        const UINT mode = static_cast<UINT>(isDarkCol         ? DarkMode::DarkModeType::dark
                                            : (themeIdx == 0) ? DarkMode::DarkModeType::classic
                                                              : DarkMode::DarkModeType::light);
        DarkMode::setDarkModeConfig(mode);
        DarkMode::setDefaultColors(false);

        DarkMode::setBackgroundColor(ThemeWindowBackgroundColor());
        DarkMode::setTextColor(ThemeWindowTextColor());
        DarkMode::setDisabledTextColor(ThemeWindowTextDisabledColor());
        DarkMode::setDlgBackgroundColor(ThemeWindowControlBackgroundColor());
        DarkMode::setLinkTextColor(ThemeWindowLinkColor());
        DarkMode::updateThemeBrushesAndPens();

        DarkMode::setViewTextColor(ThemeWindowTextColor());
        DarkMode::setViewBackgroundColor(ThemeWindowControlBackgroundColor());
        DarkMode::calculateTreeViewStyle();

        UpdateAfterThemeChange();

        DarkMode::setPrevTreeViewStyle();
    } else {
        UpdateAfterThemeChange();
    }
};

void SelectNextTheme() {
    int newIdx = (gCurrThemeIndex + 1) % gThemeCount;
    SetThemeByIndex(newIdx);
}

// not case sensitive
static int GetThemeByName(const char* name) {
    for (int i = 0; i < gThemeCount; i++) {
        Theme* theme = gThemes->At(i);
        if (str::EqI(theme->name, name)) {
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
        str::ReplaceWithCopy(&gGlobalPrefs->theme, gThemeLight->name);
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
        gThemeLight->colorizeControls = false;
        gThemeLight->controlBackgroundColorParsed.col = kColWhite;
    } else {
        gThemeLight->colorizeControls = true;
        gThemeLight->controlBackgroundColorParsed.col = bgParsed->col;
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

#define GetThemeCol(name, def) GetParsedCOLORREF(name, name##Parsed, def)

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

    // if user did change those colors in advanced settings, respect them
    bool userDidChange = text != kColBlack || bg != kColWhite;
    if (userDidChange) {
        std::swap(text, bg);
        return text;
    }

    // default colors
    if (gCurrentTheme == gThemeLight) {
        std::swap(text, bg);
        return text;
    }

    // if we're inverting in non-default themes, the colors
    // should match the colors of the window
    text = ThemeWindowTextColor();
    bg = ThemeMainWindowBackgroundColor();

    if (gCurrThemeIndex < 3) {
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4465
        // this is probably not expected for custom colors but we used to do
        // it for built-in themes
        // so do it for legacy themes but not for custom themes or new Dark theme
        bg = AdjustLightOrDark(bg, 8);
    }
    return text;
}

COLORREF ThemeControlBackgroundColor() {
    // note: we can change it in ThemeUpdateAfterLoadSettings()
    auto col = GetThemeCol(gCurrentTheme->controlBackgroundColor, kRedColor);
    return col;
}

COLORREF ThemeMainWindowBackgroundColor() {
    COLORREF bgColor = GetThemeCol(gCurrentTheme->backgroundColor, kRedColor);
    if (gCurrThemeIndex == 0) {
        // Special behavior for light theme.
        ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
        if (!IsDefaultMainWinColor(bgParsed)) {
            bgColor = bgParsed->col;
        }
    }
    return bgColor;
}

COLORREF ThemeWindowBackgroundColor() {
    auto col = GetThemeCol(gCurrentTheme->backgroundColor, kRedColor);
    return col;
}

COLORREF ThemeWindowTextColor() {
    auto col = GetThemeCol(gCurrentTheme->textColor, kRedColor);
    return col;
}

COLORREF ThemeWindowTextDisabledColor() {
    auto col = ThemeWindowTextColor();
    // TODO: probably add textDisabledColor
    auto col2 = AdjustLightOrDark(col, 0x7f);
    return col2;
}

COLORREF ThemeWindowControlBackgroundColor() {
    auto col = GetThemeCol(gCurrentTheme->controlBackgroundColor, kRedColor);
    return col;
}

COLORREF ThemeWindowLinkColor() {
    auto col = GetThemeCol(gCurrentTheme->linkColor, kRedColor);
    return col;
}

COLORREF ThemeNotificationsBackgroundColor() {
    auto col = ThemeWindowBackgroundColor();
    return AdjustLightness2(col, 10);
}

COLORREF ThemeNotificationsTextColor() {
    return ThemeWindowTextColor();
}

COLORREF ThemeNotificationsHighlightColor() {
    if (gCurrentTheme->colorizeControls) {
        auto col = ThemeWindowBackgroundColor();
        return AdjustLightOrDark(col, 20);
    }
    return RgbToCOLORREF(0xFFEE70); // yellowish
}

COLORREF ThemeNotificationsHighlightTextColor() {
    if (gCurrentTheme->colorizeControls) {
        auto col = ThemeWindowTextColor();
        return AdjustLightOrDark(col, 20);
    }
    return RgbToCOLORREF(0x8d0801); // reddish
}

COLORREF ThemeNotificationsProgressColor() {
    return ThemeWindowLinkColor();
}

bool ThemeColorizeControls() {
    if (gCurrentTheme->colorizeControls) {
        return true;
    }
    return !IsMenuFontSizeDefault();
}

#if 0
void dumpThemes() {
    logf("Themes [\n");
    for (ThemeOld* theme : gThemes) {
        auto w = *theme;
        logf("    [\n");
        logf("        Name = %s\n", w.name);
        logf("        TextColor = %s\n", SerializeColorTemp(w.textColor));
        logf("        BackgroundColor = %s\n", SerializeColorTemp(w.backgroundColor));
        logf("        ControlBackgroundColor = %s\n", SerializeColorTemp(w.controlBackgroundColor));
        logf("        LinkColor = %s\n", SerializeColorTemp(w.linkColor));
        logf("        ColorizeControls = %s\n", w.colorizeControls ? "true" : "false");
        logf("    ]\n");
    }
    logf("]\n");
}
#endif
