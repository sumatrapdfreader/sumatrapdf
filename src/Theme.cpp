/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "base/Base.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "Translations.h"
#include "DarkModeSubclass.h"

// allow only x64 and arm64 for compatibility for older OS
#if !defined(_DARKMODELIB_NOT_USED) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__arm64__) || defined(__arm64) || defined(_M_ARM64))
bool gUseDarkModeLib = true;
#else
bool gUseDarkModeLib = false;
#endif

bool UseDarkModeLib() {
    return gUseDarkModeLib;
}

/*
preserve those translations:
_TRN("Dark")
_TRN("Darker")
_TRN("Light")
*/

constexpr COLORREF kColBlack = 0x000000;
constexpr COLORREF kColWhite = 0xFFFFFF;
constexpr COLORREF kRedColor = RgbToCOLORREF(0xff0000);

static Str themesTxt = StrL(R"(Themes [
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
    [
        Name = Solarized Dark
        TextColor = #839496
        BackgroundColor = #002b36
        ControlBackgroundColor = #073642
        LinkColor = #268bd2
        ColorizeControls = true
    ]
    [
        Name = Dracula
        TextColor = #f8f8f2
        BackgroundColor = #282a36
        ControlBackgroundColor = #44475a
        LinkColor = #8be9fd
        ColorizeControls = true
    ]
    [
        Name = Nebula
        TextColor = #CBE3E7
        BackgroundColor = #100E23
        ControlBackgroundColor = #1E1C31
        LinkColor = #91DDFF
        ColorizeControls = true
    ]
    [
        Name = Greeny
        TextColor = #FDD085
        BackgroundColor = #4F6232
        ControlBackgroundColor = #1E3304
        LinkColor = #A2E53B
        ColorizeControls = true
    ]
    [
        Name = Choco
        TextColor = #D7AD62
        BackgroundColor = #2A1104
        ControlBackgroundColor = #172736
        LinkColor = #E8CD12
        ColorizeControls = true
    ]
    [
        Name = Purpy
        TextColor = #E2C3C3
        BackgroundColor = #20222A
        ControlBackgroundColor = #1E0126
        LinkColor = #EFF0B8
        ColorizeControls = true
    ]
]
)");

extern void UpdateAfterThemeChange();

int gFirstSetThemeCmdId;
int gLastSetThemeCmdId;
int gCurrSetThemeCmdId;

static Vec<Theme*>* gThemes = nullptr;
static int gThemeCount;
static int gCurrThemeIndex = 0;
static Theme* gCurrentTheme = nullptr;
static Theme* gThemeLight = nullptr;
static Themes* gParsedThemes = nullptr;

bool IsCurrentThemeDefault() {
    return gCurrThemeIndex == 0;
}

void FreeThemes() {
    delete gThemes; // no need to free members, they are owned by gParsedThemes
    gThemes = nullptr;
    FreeParsedThemes(gParsedThemes);
    gParsedThemes = nullptr;
}

void CreateThemeCommands() {
    FreeThemes();

    gThemes = new Vec<Theme*>();
    gParsedThemes = ParseThemes(themesTxt);
    for (Theme* theme : *gParsedThemes->themes) {
        gThemes->Append(theme);
    }

    for (Theme* theme : *gGlobalPrefs->themes) {
        gThemes->Append(theme);
    }

    gThemeCount = len(*gThemes);
    if (gCurrThemeIndex >= gThemeCount) {
        gCurrThemeIndex = 0;
    }
    gCurrentTheme = (*gThemes)[gCurrThemeIndex];
    gThemeLight = (*gThemes)[0];

    CustomCommand* cmd;
    for (int i = 0; i < gThemeCount; i++) {
        Theme* theme = (*gThemes)[i];
        Str themeName = theme->name;
        auto args = NewStringArg(kCmdArgTheme, themeName);
        cmd = CreateCustomCommand(themeName, CmdSetTheme, args);
        cmd->name = str::Dup(fmt(_TRA("Set theme '%s'").s, themeName));
        if (i == 0) {
            gFirstSetThemeCmdId = cmd->id;
        } else if (i == gThemeCount - 1) {
            gLastSetThemeCmdId = cmd->id;
        }
    }
    gCurrSetThemeCmdId = gFirstSetThemeCmdId + gCurrThemeIndex;
}

// when true, the user picked "System" as the theme: we resolve it to the
// preferred light/dark theme from the OS setting and re-resolve when Windows
// switches modes; gGlobalPrefs->theme stays "System"
static bool gThemeFollowsSystem = false;

// remember the last explicitly used light and dark theme so the light/dark
// toggle and the System theme know what to switch to
static void RememberLastLightDarkTheme() {
    if (!gGlobalPrefs || !gCurrentTheme) {
        return;
    }
    if (IsLightColor(ThemeWindowBackgroundColor())) {
        str::ReplaceWithCopy(&gGlobalPrefs->lastLightTheme, gCurrentTheme->name);
    } else {
        str::ReplaceWithCopy(&gGlobalPrefs->lastDarkTheme, gCurrentTheme->name);
    }
}

void SetThemeByIndex(int themeIdx) {
    ReportIf((themeIdx < 0) || (themeIdx >= gThemeCount));
    if (themeIdx >= gThemeCount) {
        themeIdx = 0;
    }
    gThemeFollowsSystem = false;
    bool themeChanged = (gCurrThemeIndex != themeIdx);
    gCurrThemeIndex = themeIdx;
    gCurrSetThemeCmdId = gFirstSetThemeCmdId + themeIdx;
    gCurrentTheme = (*gThemes)[gCurrThemeIndex];
    str::ReplaceWithCopy(&gGlobalPrefs->theme, gCurrentTheme->name);
    RememberLastLightDarkTheme();
    if (UseDarkModeLib()) {
        // TODO: we should apply themes to every theme other than 0
        // but in Solarized Light in Find dialog's input field text is invisible i.e. black
        // UINT mode = themeIdx == 0 ? kModeClassic : kModeDark;
        const bool isDarkCol = DarkMode::isColorDark(ThemeWindowControlBackgroundColor());
        const UINT mode = static_cast<UINT>(isDarkCol         ? DarkMode::DarkModeType::dark
                                            : (themeIdx == 0) ? DarkMode::DarkModeType::classic
                                                              : DarkMode::DarkModeType::light);
        DarkMode::setDarkModeConfigEx(mode);
        DarkMode::setDefaultColors(false);

        DarkMode::setBackgroundColor(ThemeWindowBackgroundColor());
        DarkMode::setCtrlBackgroundColor(ThemeWindowControlBackgroundColor());
        COLORREF ctrlBg = ThemeWindowControlBackgroundColor();
        COLORREF hotBg = AccentColor(ctrlBg, 20);
        COLORREF edgeCol = AccentColor(ctrlBg, 40);
        DarkMode::setHotBackgroundColor(hotBg);
        DarkMode::setTextColor(ThemeWindowTextColor());
        DarkMode::setDisabledTextColor(ThemeWindowTextDisabledColor());
        DarkMode::setDlgBackgroundColor(ctrlBg);
        DarkMode::setLinkTextColor(ThemeWindowLinkColor());
        DarkMode::setEdgeColor(edgeCol);
        DarkMode::updateThemeBrushesAndPens();

        DarkMode::setViewTextColor(ThemeWindowTextColor());
        DarkMode::setViewBackgroundColor(ThemeWindowControlBackgroundColor());
        DarkMode::calculateTreeViewStyle();

        if (themeChanged) {
            UpdateAfterThemeChange();
        }

        DarkMode::setPrevTreeViewStyle();
    } else {
        if (themeChanged) {
            UpdateAfterThemeChange();
        }
    }
};

void SelectNextTheme() {
    int newIdx = (gCurrThemeIndex + 1) % gThemeCount;
    SetThemeByIndex(newIdx);
}

// not case sensitive
static int GetThemeByName(Str name) {
    for (int i = 0; i < gThemeCount; i++) {
        Theme* theme = (*gThemes)[i];
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

// true if Windows "choose your default app mode" is set to dark
static bool OsAppsUseDarkMode() {
    DWORD val = 1; // AppsUseLightTheme defaults to 1 (light)
    DWORD cb = sizeof(val);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &val, &cb);
    return val == 0;
}

static int GetPreferredLightThemeIndex() {
    int idx = GetThemeByName(gGlobalPrefs->lastLightTheme);
    if (idx >= 0) {
        return idx;
    }
    return 0; // gThemeLight
}

static int GetPreferredDarkThemeIndex() {
    int idx = GetThemeByName(gGlobalPrefs->lastDarkTheme);
    if (idx >= 0) {
        return idx;
    }
    idx = GetThemeByName(StrL("Dark"));
    return idx >= 0 ? idx : 0;
}

void SetTheme(Str name) {
    if (str::EqI(name, StrL("System"))) {
        // resolve to the preferred light/dark theme from the OS setting; keep
        // "System" in prefs so it persists and keeps following the OS
        int idx = OsAppsUseDarkMode() ? GetPreferredDarkThemeIndex() : GetPreferredLightThemeIndex();
        SetThemeByIndex(idx);
        gThemeFollowsSystem = true;
        str::ReplaceWithCopy(&gGlobalPrefs->theme, StrL("System"));
        return;
    }
    int idx = GetThemeByName(name);
    if (idx < 0) {
        // invalid name, reset to light theme
        str::ReplaceWithCopy(&gGlobalPrefs->theme, gThemeLight->name);
        idx = 0;
    }
    SetThemeByIndex(idx);
}

// switch between the last used light and dark theme (CmdToggleLightDarkTheme)
void ToggleLightDarkTheme() {
    bool isDark = !IsLightColor(ThemeWindowBackgroundColor());
    int idx = isDark ? GetPreferredLightThemeIndex() : GetPreferredDarkThemeIndex();
    SetThemeByIndex(idx);
}

// call on WM_SETTINGCHANGE "ImmersiveColorSet": re-resolves the System theme
// when the user switches Windows between light and dark mode
void UpdateThemeAfterSystemColorChange() {
    if (!gThemeFollowsSystem) {
        return;
    }
    SetTheme(StrL("System")); // no-op unless the resolved theme changed
}

// call after loading settings
void SetCurrentThemeFromSettings() {
    SetTheme(gGlobalPrefs->theme);
    ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
    bool isDefault = IsDefaultMainWinColor(bgParsed);
    if (isDefault) {
        gThemeLight->colorizeControls = false;
        gThemeLight->controlBackgroundColorParsed.wasParsed = true;
        gThemeLight->controlBackgroundColorParsed.parsedOk = true;
        gThemeLight->controlBackgroundColorParsed.col = kColWhite;
    } else if (bgParsed->parsedOk) {
        gThemeLight->colorizeControls = true;
        gThemeLight->controlBackgroundColorParsed.wasParsed = true;
        gThemeLight->controlBackgroundColorParsed.parsedOk = true;
        gThemeLight->controlBackgroundColorParsed.col = bgParsed->col;
    }
}

COLORREF AccentColor(COLORREF col, int light, int dark) {
    if (dark == 0) {
        dark = light;
    }
    if (IsLightColor(col)) {
        return AdjustLightness2(col, (float)-light);
    }
    return AdjustLightness2(col, (float)dark);
}

#define GetThemeCol(name, def) GetParsedCOLORREF(name, name##Parsed, def)

// canvas/window background color around the document pages
// not affected by FixedPageUI.TextColor/BackgroundColor (those affect page rendering)
COLORREF ThemeDocumentColors(COLORREF& bg) {
    bg = ThemeMainWindowBackgroundColor();

    if (!gGlobalPrefs->fixedPageUI.invertColors) {
        return ThemeWindowTextColor();
    }

    COLORREF text = ThemeWindowTextColor();
    bg = ThemeMainWindowBackgroundColor();

    if (gCurrThemeIndex < 3) {
        bg = AccentColor(bg, 8);
    }
    return text;
}

// colors for page bitmap recoloring (render cache)
// TextColor substitutes black, BackgroundColor substitutes white in rendered pages
COLORREF ThemePageRenderColors(COLORREF& bg) {
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
        bg = AccentColor(bg, 8);
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
        if (bgParsed->parsedOk && !IsDefaultMainWinColor(bgParsed)) {
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
    // blend text color halfway toward background so disabled text
    // is visible but clearly muted on both light and dark themes
    COLORREF txt = ThemeWindowTextColor();
    COLORREF bg = ThemeMainWindowBackgroundColor();
    u8 r = (u8)((GetRValue(txt) + GetRValue(bg)) / 2);
    u8 g = (u8)((GetGValue(txt) + GetGValue(bg)) / 2);
    u8 b = (u8)((GetBValue(txt) + GetBValue(bg)) / 2);
    return RGB(r, g, b);
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
        return AccentColor(col, 20);
    }
    return RgbToCOLORREF(0xFFEE70); // yellowish
}

COLORREF ThemeNotificationsHighlightTextColor() {
    if (gCurrentTheme->colorizeControls) {
        auto col = ThemeWindowTextColor();
        return AccentColor(col, 20);
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
