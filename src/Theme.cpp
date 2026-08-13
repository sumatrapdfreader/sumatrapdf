/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "Translations.h"
#include "DarkModeSubclass.h"
#include "PdfDarkMode.h"

// allow only x64 and arm64 for compatibility for older OS
#if !defined(_DARKMODELIB_NOT_USED) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__arm64__) || defined(__arm64) || defined(_M_ARM64))
static bool gUseDarkModeLib = true;
#else
bool gUseDarkModeLib = false;
#endif

bool UseDarkModeLib() {
    return gUseDarkModeLib;
}

// The installer and uninstaller never load settings, so CreateThemeCommands()
// doesn't run and there is no current theme - every Theme*Color() accessor
// dereferences gCurrentTheme and would crash. They still create buttons and
// edits, and wingui asks the app for their colors, so the two hooks that need a
// theme have to be able to answer without one. Defined next to gCurrentTheme.
static bool HasCurrentTheme();

// CreateMainWindow hands the frame to darkmodelib (dark caption, themed
// children, toolbar custom draw) but a top-level window we create ourselves is
// not a child of the frame, so none of that reaches it: buttons and edits keep
// their stock light borders and hover highlights, list boxes and multi-line
// edits keep a light scrollbar, and the caption stays white. Every such window
// - the find bar, the find window, the advanced settings dialog - calls this
// once its children exist (issues #5894, #5895).
void ApplyDarkModeToPopupWindow(HWND hwnd) {
    if (!UseDarkModeLib()) {
        return;
    }
    DarkMode::setDarkTitleBarEx(hwnd, true);
    if (IsCurrentThemeDefault()) {
        return;
    }
    DarkMode::setChildCtrlsSubclassAndTheme(hwnd);
    DarkMode::setWindowNotifyCustomDrawSubclass(hwnd);
}

void StyleThemedButton(VirtButton* b, bool isDefault) {
    COLORREF bg = ThemeWindowControlBackgroundColor();
    b->textColor = ThemeWindowTextColor();
    b->textColorDisabled = ThemeWindowTextDisabledColor();
    b->bgColor = AccentColor(bg, isDefault ? 26 : 14);
    b->bgColorHover = AccentColor(bg, isDefault ? 40 : 28);
    b->borderColor = isDefault ? ThemeHotEdgeColor() : ThemeEdgeColor();
}

VirtButton* NewThemedButton(HWND hwndForDpi, Str text, PlatformFont* font, bool isDefault) {
    DpiSetFromHwnd(hwndForDpi);
    auto* b = new VirtButton(text, font);
    StyleThemedButton(b, isDefault);
    b->textPadding = DpiScaledInsets(5, 12);
    return b;
}

// The underline under a borderless Edit is a separator, so it takes the edge
// color like every other border and divider. wingui used to blend the control's
// own text color toward its background, which lands wherever those two happen
// to be rather than anywhere in the palette: on the Dark theme's black sidebar
// that gave a bright #535353 line instead of the theme's #374151 (issue #5893).
// ThemeEdgeColor() derives a color from the control background when the theme
// doesn't name one, so there is always something sensible to draw.
// Color of the 1px underline an Edit created withBottomBorder draws, so wingui
// doesn't have to know about the app's palette. The app implements it (Sumatra
// returns the theme's edge color); an app that doesn't theme anything can
// return a fixed gray.
COLORREF EditBottomBorderColor() {
    if (!HasCurrentTheme()) {
        return GetSysColor(COLOR_WINDOWFRAME);
    }
    return ThemeEdgeColor();
}

/*
preserve those translations:
_TRN("Dark")
_TRN("Light")
_TRN("Charcoal")
*/

constexpr COLORREF kColBlack = 0x000000;
constexpr COLORREF kColWhite = 0xFFFFFF;
constexpr COLORREF kRedColor = RgbToCOLORREF(0xff0000);

// Optional colors (DisabledTextColor … NotificationHighlightTextColor) fix
// muddy derived hues when TextColor is not neutral gray (e.g. Dracula #f8f8f2).
// Empty optional fields still fall back to AccentColor / blend of the four base colors.
static Str themesTxt = StrL(R"(Themes [
    [
        Name = Light
        TextColor = #000000
        BackgroundColor = #f2f2f2
        ControlBackgroundColor = #ffffff
        LinkColor = #0020a0
        DisabledTextColor = #808080
        DarkerTextColor = #404040
        HotBackgroundColor = #e8e8e8
        EdgeColor = #c0c0c0
        HotEdgeColor = #808080
        DisabledEdgeColor = #d0d0d0
        ErrorBackgroundColor = #ffe0e0
        NotificationBackgroundColor = #fafafa
        NotificationHighlightColor = #ffee70
        NotificationHighlightTextColor = #8d0801
        ColorizeControls = false
    ]
    [
        Name = Dark
        TextColor = #F9FAFB
        BackgroundColor = #000000
        ControlBackgroundColor = #000000
        LinkColor = #6B7280
        DisabledTextColor = #6B7280
        DarkerTextColor = #9CA3AF
        HotBackgroundColor = #1F2937
        EdgeColor = #374151
        HotEdgeColor = #6B7280
        DisabledEdgeColor = #1F2937
        ErrorBackgroundColor = #7F1D1D
        NotificationBackgroundColor = #111827
        NotificationHighlightColor = #422006
        NotificationHighlightTextColor = #fde68a
        ColorizeControls = true
    ]
    [
        Name = Light Warm
        TextColor = #333333
        BackgroundColor = #ebe6da
        ControlBackgroundColor = #f5f1e8
        LinkColor = #0020a0
        DisabledTextColor = #8a8578
        DarkerTextColor = #5c574c
        HotBackgroundColor = #e8e2d4
        EdgeColor = #c9c2b0
        HotEdgeColor = #8a8578
        DisabledEdgeColor = #ddd6c6
        ErrorBackgroundColor = #f5d6d0
        NotificationBackgroundColor = #f8f4ea
        NotificationHighlightColor = #e8d48b
        NotificationHighlightTextColor = #5c3a0a
        ColorizeControls = true
    ]
    [
        Name = Dark from 3.5
        TextColor = #bac9d0
        BackgroundColor = #263238
        ControlBackgroundColor = #263238
        LinkColor = #8aa3b0
        DisabledTextColor = #6b7c85
        DarkerTextColor = #8aa3b0
        HotBackgroundColor = #324047
        EdgeColor = #37474f
        HotEdgeColor = #546e7a
        DisabledEdgeColor = #1e272c
        ErrorBackgroundColor = #5c2b2b
        NotificationBackgroundColor = #2e3c43
        NotificationHighlightColor = #4a3a12
        NotificationHighlightTextColor = #ffdf9e
        ColorizeControls = true
    ]
    [
        Name = Charcoal
        TextColor = #ffffff
        BackgroundColor = #2d2d30
        ControlBackgroundColor = #2d2d30
        LinkColor = #9999a0
        DisabledTextColor = #808088
        DarkerTextColor = #b0b0b8
        HotBackgroundColor = #3e3e42
        EdgeColor = #505058
        HotEdgeColor = #808088
        DisabledEdgeColor = #252528
        ErrorBackgroundColor = #5a1d1d
        NotificationBackgroundColor = #38383c
        NotificationHighlightColor = #4a3c16
        NotificationHighlightTextColor = #ffe2a0
        ColorizeControls = true
    ]
    [
        Name = Solarized Light
        TextColor = #212323
        BackgroundColor = #fdf6e3
        ControlBackgroundColor = #eee8d5
        LinkColor = #268bd2
        DisabledTextColor = #93a1a1
        DarkerTextColor = #586e75
        HotBackgroundColor = #e6dfc8
        EdgeColor = #93a1a1
        HotEdgeColor = #657b83
        DisabledEdgeColor = #eee8d5
        ErrorBackgroundColor = #f8d0c8
        NotificationBackgroundColor = #f5efdc
        NotificationHighlightColor = #f3e2b3
        NotificationHighlightTextColor = #5c4405
        ColorizeControls = true
    ]
    [
        Name = Solarized Dark
        TextColor = #839496
        BackgroundColor = #002b36
        ControlBackgroundColor = #073642
        LinkColor = #268bd2
        DisabledTextColor = #586e75
        DarkerTextColor = #657b83
        HotBackgroundColor = #0a4a58
        EdgeColor = #586e75
        HotEdgeColor = #839496
        DisabledEdgeColor = #002b36
        ErrorBackgroundColor = #5c1a1a
        NotificationBackgroundColor = #003543
        NotificationHighlightColor = #3d3208
        NotificationHighlightTextColor = #eec97a
        ColorizeControls = true
    ]
    [
        Name = Dracula
        TextColor = #f8f8f2
        BackgroundColor = #282a36
        ControlBackgroundColor = #44475a
        LinkColor = #8be9fd
        DisabledTextColor = #6272a4
        DarkerTextColor = #6272a4
        HotBackgroundColor = #565a73
        EdgeColor = #6272a4
        HotEdgeColor = #bd93f9
        DisabledEdgeColor = #343746
        ErrorBackgroundColor = #ff5555
        NotificationBackgroundColor = #343746
        NotificationHighlightColor = #4a3c14
        NotificationHighlightTextColor = #f1fa8c
        ColorizeControls = true
    ]
    [
        Name = Nebula
        TextColor = #CBE3E7
        BackgroundColor = #100E23
        ControlBackgroundColor = #1E1C31
        LinkColor = #91DDFF
        DisabledTextColor = #6b6b8a
        DarkerTextColor = #a0a0c0
        HotBackgroundColor = #2a2745
        EdgeColor = #3e3a5c
        HotEdgeColor = #91DDFF
        DisabledEdgeColor = #15132a
        ErrorBackgroundColor = #5c1a2e
        NotificationBackgroundColor = #1a1830
        NotificationHighlightColor = #3d2f12
        NotificationHighlightTextColor = #f0d9a0
        ColorizeControls = true
    ]
    [
        Name = Greeny
        TextColor = #FDD085
        BackgroundColor = #4F6232
        ControlBackgroundColor = #1E3304
        LinkColor = #A2E53B
        DisabledTextColor = #8a9a60
        DarkerTextColor = #c0c878
        HotBackgroundColor = #2a4210
        EdgeColor = #6a7a40
        HotEdgeColor = #A2E53B
        DisabledEdgeColor = #152808
        ErrorBackgroundColor = #5c2810
        NotificationBackgroundColor = #3a4a28
        NotificationHighlightColor = #5c4a18
        NotificationHighlightTextColor = #fde7b0
        ColorizeControls = true
    ]
    [
        Name = Choco
        TextColor = #D7AD62
        BackgroundColor = #2A1104
        ControlBackgroundColor = #172736
        LinkColor = #E8CD12
        DisabledTextColor = #8a7040
        DarkerTextColor = #b09050
        HotBackgroundColor = #243848
        EdgeColor = #3a4a58
        HotEdgeColor = #E8CD12
        DisabledEdgeColor = #0e1820
        ErrorBackgroundColor = #5c2010
        NotificationBackgroundColor = #1e2e3c
        NotificationHighlightColor = #4a3208
        NotificationHighlightTextColor = #f5d89b
        ColorizeControls = true
    ]
    [
        Name = Purpy
        TextColor = #E2C3C3
        BackgroundColor = #20222A
        ControlBackgroundColor = #1E0126
        LinkColor = #EFF0B8
        DisabledTextColor = #8a7088
        DarkerTextColor = #b0a0b0
        HotBackgroundColor = #2e1838
        EdgeColor = #4a3060
        HotEdgeColor = #EFF0B8
        DisabledEdgeColor = #140018
        ErrorBackgroundColor = #5c1a2a
        NotificationBackgroundColor = #28203a
        NotificationHighlightColor = #46360f
        NotificationHighlightTextColor = #f0d9a8
        ColorizeControls = true
    ]
    [
        Name = One Dark
        TextColor = #abb2bf
        BackgroundColor = #282c34
        ControlBackgroundColor = #21252b
        LinkColor = #61afef
        DisabledTextColor = #5c6370
        DarkerTextColor = #7f848e
        HotBackgroundColor = #2c313c
        EdgeColor = #181a1f
        HotEdgeColor = #528bff
        DisabledEdgeColor = #1b1d23
        ErrorBackgroundColor = #be5046
        NotificationBackgroundColor = #2c313a
        NotificationHighlightColor = #40351a
        NotificationHighlightTextColor = #e5c07b
        ColorizeControls = true
    ]
    [
        Name = Monokai
        TextColor = #f8f8f2
        BackgroundColor = #272822
        ControlBackgroundColor = #3e3d32
        LinkColor = #66d9ef
        DisabledTextColor = #75715e
        DarkerTextColor = #a6a68a
        HotBackgroundColor = #49483e
        EdgeColor = #75715e
        HotEdgeColor = #a6e22e
        DisabledEdgeColor = #1e1f1c
        ErrorBackgroundColor = #f92672
        NotificationBackgroundColor = #34352f
        NotificationHighlightColor = #46411c
        NotificationHighlightTextColor = #e6db74
        ColorizeControls = true
    ]
    [
        Name = Nord
        TextColor = #d8dee9
        BackgroundColor = #2e3440
        ControlBackgroundColor = #3b4252
        LinkColor = #88c0d0
        DisabledTextColor = #4c566a
        DarkerTextColor = #81a1c1
        HotBackgroundColor = #434c5e
        EdgeColor = #4c566a
        HotEdgeColor = #88c0d0
        DisabledEdgeColor = #2e3440
        ErrorBackgroundColor = #bf616a
        NotificationBackgroundColor = #3b4252
        NotificationHighlightColor = #4a3f26
        NotificationHighlightTextColor = #ebcb8b
        ColorizeControls = true
    ]
    [
        Name = GitHub Dark
        TextColor = #e6edf3
        BackgroundColor = #0d1117
        ControlBackgroundColor = #161b22
        LinkColor = #2f81f7
        DisabledTextColor = #6e7681
        DarkerTextColor = #8b949e
        HotBackgroundColor = #21262d
        EdgeColor = #30363d
        HotEdgeColor = #58a6ff
        DisabledEdgeColor = #21262d
        ErrorBackgroundColor = #da3633
        NotificationBackgroundColor = #161b22
        NotificationHighlightColor = #3d2a04
        NotificationHighlightTextColor = #e3b341
        ColorizeControls = true
    ]
    [
        Name = Catppuccin Mocha
        TextColor = #cdd6f4
        BackgroundColor = #1e1e2e
        ControlBackgroundColor = #181825
        LinkColor = #89b4fa
        DisabledTextColor = #6c7086
        DarkerTextColor = #a6adc8
        HotBackgroundColor = #313244
        EdgeColor = #45475a
        HotEdgeColor = #cba6f7
        DisabledEdgeColor = #11111b
        ErrorBackgroundColor = #f38ba8
        NotificationBackgroundColor = #181825
        NotificationHighlightColor = #45391f
        NotificationHighlightTextColor = #f9e2af
        ColorizeControls = true
    ]
    [
        Name = Tokyo Night
        TextColor = #c0caf5
        BackgroundColor = #1a1b26
        ControlBackgroundColor = #16161e
        LinkColor = #7aa2f7
        DisabledTextColor = #565f89
        DarkerTextColor = #a9b1d6
        HotBackgroundColor = #292e42
        EdgeColor = #3b4261
        HotEdgeColor = #7dcfff
        DisabledEdgeColor = #0f0f14
        ErrorBackgroundColor = #f7768e
        NotificationBackgroundColor = #16161e
        NotificationHighlightColor = #3d3117
        NotificationHighlightTextColor = #e0af68
        ColorizeControls = true
    ]
    [
        Name = Gruvbox
        TextColor = #ebdbb2
        BackgroundColor = #282828
        ControlBackgroundColor = #3c3836
        LinkColor = #83a598
        DisabledTextColor = #928374
        DarkerTextColor = #a89984
        HotBackgroundColor = #504945
        EdgeColor = #665c54
        HotEdgeColor = #fabd2f
        DisabledEdgeColor = #1d2021
        ErrorBackgroundColor = #fb4934
        NotificationBackgroundColor = #3c3836
        NotificationHighlightColor = #4a3a1a
        NotificationHighlightTextColor = #fabd2f
        ColorizeControls = true
    ]
    [
        Name = Night Owl
        TextColor = #d6deeb
        BackgroundColor = #011627
        ControlBackgroundColor = #0b2942
        LinkColor = #82aaff
        DisabledTextColor = #5f7e97
        DarkerTextColor = #7fdbca
        HotBackgroundColor = #1d3b53
        EdgeColor = #122d42
        HotEdgeColor = #c792ea
        DisabledEdgeColor = #01111d
        ErrorBackgroundColor = #ef5350
        NotificationBackgroundColor = #0b2942
        NotificationHighlightColor = #3a2d16
        NotificationHighlightTextColor = #ecc48d
        ColorizeControls = true
    ]
    [
        Name = Ayu
        TextColor = #bfbdb6
        BackgroundColor = #0b0e14
        ControlBackgroundColor = #0d1017
        LinkColor = #59c2ff
        DisabledTextColor = #565b66
        DarkerTextColor = #acb6bf
        HotBackgroundColor = #1b2733
        EdgeColor = #1b2733
        HotEdgeColor = #e6b450
        DisabledEdgeColor = #06070a
        ErrorBackgroundColor = #f07178
        NotificationBackgroundColor = #0d1017
        NotificationHighlightColor = #362a12
        NotificationHighlightTextColor = #e6b450
        ColorizeControls = true
    ]
    [
        Name = Palenight
        TextColor = #a6accd
        BackgroundColor = #292d3e
        ControlBackgroundColor = #1b1e2b
        LinkColor = #82aaff
        DisabledTextColor = #676e95
        DarkerTextColor = #8796b0
        HotBackgroundColor = #32374d
        EdgeColor = #3c435e
        HotEdgeColor = #c792ea
        DisabledEdgeColor = #151820
        ErrorBackgroundColor = #ff5370
        NotificationBackgroundColor = #1b1e2b
        NotificationHighlightColor = #3f3520
        NotificationHighlightTextColor = #ffcb6b
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

static bool HasCurrentTheme() {
    return gCurrentTheme != nullptr;
}

bool IsCurrentThemeDefault() {
    return gCurrThemeIndex == 0;
}

// Windows high contrast mode. The user picked a system-wide palette because
// they need it to read the screen, so an app is expected to use those colors
// instead of its own. We only do that for the default theme: choosing any
// other theme is an explicit decision about colors and it wins (issue #2124).
static bool gIsHighContrast = false;

// true when the UI colors have to come from the system palette instead of the
// theme. Every color accessor tests this, so it's computed once by
// RecalcUseHighContrast() instead of on every call. Page rendering deliberately
// does not consult it: recoloring the document is not part of high contrast
// mode and inverting images was the original complaint in #2124.
static bool gUseHighContrast = false;

static void DetectHighContrastMode() {
    HIGHCONTRASTW hc{};
    hc.cbSize = sizeof(hc);
    if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
        gIsHighContrast = false;
        return;
    }
    gIsHighContrast = (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

// call whenever the OS high contrast setting or the current theme changes
static void RecalcUseHighContrast() {
    gUseHighContrast = gIsHighContrast && HasCurrentTheme() && IsCurrentThemeDefault();
}

// exported so the few places that hardcode a color for the default theme (a
// white page box on the toolbar, say) can defer to the palette instead
bool ThemeUsesHighContrastColors() {
    return gUseHighContrast;
}

void FreeThemes() {
    delete gThemes; // no need to free members, they are owned by gParsedThemes
    gThemes = nullptr;
    FreeParsedThemes(gParsedThemes);
    gParsedThemes = nullptr;
}

void CreateThemeCommands() {
    FreeThemes();
    DetectHighContrastMode();

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
    RecalcUseHighContrast();

    CustomCommand* cmd;
    for (int i = 0; i < gThemeCount; i++) {
        Theme* theme = (*gThemes)[i];
        Str themeName = theme->name;
        auto* args = NewStringArg(kCmdArgTheme, themeName);
        cmd = CreateCustomCommand(themeName, CmdSetTheme, args, fmt(_TRA("Set theme '%s'").s, themeName));
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
    if (gUseHighContrast) {
        // the theme's own colors are not in use, so they say nothing about
        // whether the user's light or dark preference is this theme
        return;
    }
    if (IsLightColor(ThemeWindowBackgroundColor())) {
        str::ReplaceWithCopy(&gGlobalPrefs->lastLightTheme, gCurrentTheme->name);
    } else {
        str::ReplaceWithCopy(&gGlobalPrefs->lastDarkTheme, gCurrentTheme->name);
    }
}

int ThemeGetCount() {
    return gThemeCount;
}

Str ThemeGetNameAt(int idx) {
    if (idx < 0 || idx >= gThemeCount) {
        return nullptr;
    }
    return (*gThemes)[idx]->name;
}

int ThemeGetCurrentIndex() {
    return gCurrThemeIndex;
}

// push the current palette (which may be the system's, in high contrast mode)
// to darkmodelib, which draws the controls we don't draw ourselves
static void ApplyThemeColorsToDarkMode() {
    if (!UseDarkModeLib()) {
        return;
    }
    // TODO: we should apply themes to every theme other than 0
    // but in Solarized Light in Find dialog's input field text is invisible i.e. black
    // UINT mode = themeIdx == 0 ? kModeClassic : kModeDark;
    const bool isDarkCol = DarkMode::isColorDark(ThemeWindowControlBackgroundColor());
    DarkMode::DarkModeType modeType = DarkMode::DarkModeType::light;
    if (isDarkCol) {
        modeType = DarkMode::DarkModeType::dark;
    } else if (IsCurrentThemeDefault()) {
        modeType = DarkMode::DarkModeType::classic;
    }
    const UINT mode = static_cast<UINT>(modeType);
    DarkMode::setDarkModeConfigEx(mode);
    DarkMode::setDefaultColors(false);

    DarkMode::setBackgroundColor(ThemeWindowBackgroundColor());
    DarkMode::setCtrlBackgroundColor(ThemeWindowControlBackgroundColor());
    COLORREF ctrlBg = ThemeWindowControlBackgroundColor();
    DarkMode::setHotBackgroundColor(ThemeHotBackgroundColor());
    DarkMode::setTextColor(ThemeWindowTextColor());
    DarkMode::setDarkerTextColor(ThemeWindowDarkerTextColor());
    DarkMode::setDisabledTextColor(ThemeWindowTextDisabledColor());
    DarkMode::setDlgBackgroundColor(ctrlBg);
    DarkMode::setLinkTextColor(ThemeWindowLinkColor());
    DarkMode::setEdgeColor(ThemeEdgeColor());
    DarkMode::setHotEdgeColor(ThemeHotEdgeColor());
    DarkMode::setDisabledEdgeColor(ThemeDisabledEdgeColor());
    DarkMode::setErrorBackgroundColor(ThemeErrorBackgroundColor());
    DarkMode::updateThemeBrushesAndPens();

    DarkMode::setViewTextColor(ThemeWindowTextColor());
    DarkMode::setViewBackgroundColor(ThemeWindowControlBackgroundColor());
    DarkMode::calculateTreeViewStyle();
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
    RecalcUseHighContrast(); // it depends on which theme is current
    str::ReplaceWithCopy(&gGlobalPrefs->theme, gCurrentTheme->name);
    RememberLastLightDarkTheme();
    ApplyThemeColorsToDarkMode();
    if (themeChanged) {
        UpdateAfterThemeChange();
    }
    if (UseDarkModeLib()) {
        DarkMode::setPrevTreeViewStyle();
    }
};

// Map removed / renamed themes so existing settings keep working.
static Str ResolveThemeAlias(Str name) {
    if (str::EqI(name, StrL("Darker")) || str::EqI(name, StrL("Dark background Bright text"))) {
        return StrL("Charcoal");
    }
    return name;
}

// a Themes[] entry the user wrote themselves; that theme does exist, whatever
// we once shipped under the same name, so it must not be migrated away
static bool HasCustomThemeNamed(Str name) {
    if (!gGlobalPrefs || !gGlobalPrefs->themes) {
        return false;
    }
    for (Theme* theme : *gGlobalPrefs->themes) {
        if (str::EqI(theme->name, name)) {
            return true;
        }
    }
    return false;
}

// Rewrite the theme names dropped in 971623174 to what replaced them.
// ResolveThemeAlias() already makes them work, but only in memory: the settings
// file keeps naming a theme that no longer exists, is saved back that way every
// time, and shows a stale name to anyone who looks (#5887). Returns true if
// anything changed, so the caller can save.
bool MigrateRenamedThemeNames() {
    if (!gGlobalPrefs) {
        return false;
    }
    Str* names[] = {&gGlobalPrefs->theme, &gGlobalPrefs->lastLightTheme, &gGlobalPrefs->lastDarkTheme};
    bool changed = false;
    for (Str* name : names) {
        if (str::IsEmptyOrWhiteSpace(*name)) {
            continue;
        }
        Str newName = ResolveThemeAlias(*name);
        if (str::EqI(*name, newName) || HasCustomThemeNamed(*name)) {
            continue;
        }
        logf("MigrateRenamedThemeNames: '%s' -> '%s'\n", *name, newName);
        str::ReplaceWithCopy(name, newName);
        changed = true;
    }
    return changed;
}

// not case sensitive
static int GetThemeByName(Str name) {
    name = ResolveThemeAlias(name);
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
    name = ResolveThemeAlias(name);
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

// name of the theme ToggleLightDarkTheme() would switch to, for the command
// palette: ": set to true" says nothing useful when a toggle picks a theme
Str ToggleLightDarkThemeTargetName() {
    bool isDark = !IsLightColor(ThemeWindowBackgroundColor());
    int idx = isDark ? GetPreferredLightThemeIndex() : GetPreferredDarkThemeIndex();
    return ThemeGetNameAt(idx);
}

// call on WM_SETTINGCHANGE "ImmersiveColorSet": re-resolves the System theme
// when the user switches Windows between light and dark mode
void UpdateThemeAfterSystemColorChange() {
    if (!gThemeFollowsSystem) {
        return;
    }
    SetTheme(StrL("System")); // no-op unless the resolved theme changed
}

// call on WM_SETTINGCHANGE: the user can turn high contrast on and off at any
// time (Alt+Shift+PrtScr), which swaps the whole palette out from under us
void UpdateThemeAfterHighContrastChange() {
    bool wasUsingHighContrast = gUseHighContrast;
    DetectHighContrastMode();
    RecalcUseHighContrast();
    if (wasUsingHighContrast == gUseHighContrast) {
        // also the common case of toggling it while a custom theme is current,
        // which changes nothing we draw
        return;
    }
    logf("UpdateThemeAfterHighContrastChange: using high contrast colors: %d\n", (int)gUseHighContrast);
    ApplyThemeColorsToDarkMode();
    UpdateAfterThemeChange();
    if (UseDarkModeLib()) {
        DarkMode::setPrevTreeViewStyle();
    }
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

#define GetThemeCol(name, def) GetParsedCOLORREF(name, name##Parsed, def)

// canvas/window background color around the document pages
// not affected by FixedPageUI.TextColor/BackgroundColor (those affect page rendering)
COLORREF ThemeDocumentColors(COLORREF& bg) {
    bg = ThemeMainWindowBackgroundColor();

    if (!DocumentColorsFollowThemeEnabled()) {
        return ThemeWindowTextColor();
    }

    COLORREF text = ThemeWindowTextColor();
    bg = ThemeMainWindowBackgroundColor();

    // the system palette is exact: tinting it away from COLOR_WINDOW is the
    // kind of "close enough" color high contrast mode exists to avoid
    if (gCurrThemeIndex < 3 && !gUseHighContrast) {
        bg = AccentColor(bg, 8);
    }
    return text;
}

// CmdInvertColors: session-only, not a setting. Before 3.7 it swapped the page
// colors outright and had nothing to do with theming; 37f920ff0 reduced it to a
// DocumentColorsFollowTheme toggle, which does nothing at all when the page
// colors don't come from the theme - custom FixedPageUI colors, for instance,
// are used as-is in every mode, so pressing it only repainted the same pixels
// (issue #5887). Swapping the effective page colors works whatever they are.
static bool gInvertPageColors = false;

bool GetInvertPageColors() {
    return gInvertPageColors;
}

void SetInvertPageColors(bool invert) {
    gInvertPageColors = invert;
}

// colors for page bitmap recoloring (render cache)
// TextColor substitutes black, BackgroundColor substitutes white in rendered pages
static COLORREF ThemePageRenderColorsNoInvert(COLORREF& bg) {
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

    if (!DocumentColorsFollowThemeEnabled()) {
        return text;
    }

    // Custom FixedPageUI colors: use them as recolor targets (no invert).
    // DocumentColorsFollowTheme means "match the UI theme", not "swap black/white"
    // — the old InvertColors path swapped here and forced white-on-black for the
    // Light theme (#5821).
    bool userDidChange = text != kColBlack || bg != kColWhite;
    if (userDidChange) {
        return text;
    }

    if (gUseHighContrast) {
        // High contrast mode is about the app's own UI. Recoloring the document
        // is not part of it - that's what made images come out inverted for no
        // reason (#2124) - so the page keeps its black-on-white default.
        return text;
    }

    // Defaults: page colors follow the window theme (light theme → dark text on
    // light paper; dark theme → light text on dark paper).
    text = ThemeWindowTextColor();
    bg = ThemeMainWindowBackgroundColor();

    if (gCurrThemeIndex < 3) {
        bg = AccentColor(bg, 8);
    }
    return text;
}

COLORREF ThemePageRenderColors(COLORREF& bg) {
    COLORREF text = ThemePageRenderColorsNoInvert(bg);
    if (!gInvertPageColors) {
        return text;
    }
    std::swap(text, bg);
    return text;
}

COLORREF ThemeControlBackgroundColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_WINDOW);
    }
    // note: we can change it in ThemeUpdateAfterLoadSettings()
    auto col = GetThemeCol(gCurrentTheme->controlBackgroundColor, kRedColor);
    return col;
}

COLORREF ThemeMainWindowBackgroundColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_WINDOW);
    }
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
    if (gUseHighContrast) {
        return GetSysColor(COLOR_WINDOW);
    }
    auto col = GetThemeCol(gCurrentTheme->backgroundColor, kRedColor);
    return col;
}

COLORREF ThemeWindowTextColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_WINDOWTEXT);
    }
    auto col = GetThemeCol(gCurrentTheme->textColor, kRedColor);
    return col;
}

static COLORREF BlendTextAndBgHalfway() {
    // fallback when DisabledTextColor is unset: mute text toward background
    COLORREF txt = ThemeWindowTextColor();
    COLORREF bg = ThemeMainWindowBackgroundColor();
    u8 r = (u8)((GetRValue(txt) + GetRValue(bg)) / 2);
    u8 g = (u8)((GetGValue(txt) + GetGValue(bg)) / 2);
    u8 b = (u8)((GetBValue(txt) + GetBValue(bg)) / 2);
    return RGB(r, g, b);
}

COLORREF ThemeWindowTextDisabledColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_GRAYTEXT);
    }
    return GetThemeCol(gCurrentTheme->disabledTextColor, BlendTextAndBgHalfway());
}

COLORREF ThemeWindowDarkerTextColor() {
    if (gUseHighContrast) {
        // high contrast has no muted text: muting it is the opposite of the point
        return GetSysColor(COLOR_WINDOWTEXT);
    }
    // fallback: slightly muted primary text (not as flat as disabled)
    COLORREF fallback = AccentColor(ThemeWindowTextColor(), 40);
    return GetThemeCol(gCurrentTheme->darkerTextColor, fallback);
}

COLORREF ThemeWindowControlBackgroundColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_WINDOW);
    }
    auto col = GetThemeCol(gCurrentTheme->controlBackgroundColor, kRedColor);
    return col;
}

COLORREF ThemeWindowLinkColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_HOTLIGHT);
    }
    auto col = GetThemeCol(gCurrentTheme->linkColor, kRedColor);
    return col;
}

COLORREF ThemeHotBackgroundColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_HIGHLIGHT);
    }
    COLORREF fallback = AccentColor(ThemeWindowControlBackgroundColor(), 20);
    return GetThemeCol(gCurrentTheme->hotBackgroundColor, fallback);
}

COLORREF ThemeEdgeColor() {
    if (gUseHighContrast) {
        // borders have to stay visible, so they take the text color
        return GetSysColor(COLOR_WINDOWTEXT);
    }
    COLORREF fallback = AccentColor(ThemeWindowControlBackgroundColor(), 40);
    return GetThemeCol(gCurrentTheme->edgeColor, fallback);
}

COLORREF ThemeHotEdgeColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_HIGHLIGHT);
    }
    COLORREF fallback = AccentColor(ThemeEdgeColor(), 30);
    return GetThemeCol(gCurrentTheme->hotEdgeColor, fallback);
}

COLORREF ThemeDisabledEdgeColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_GRAYTEXT);
    }
    COLORREF fallback = AccentColor(ThemeWindowControlBackgroundColor(), 15);
    return GetThemeCol(gCurrentTheme->disabledEdgeColor, fallback);
}

COLORREF ThemeErrorBackgroundColor() {
    if (gUseHighContrast) {
        // no error color in the system palette; the text carries the message
        return GetSysColor(COLOR_WINDOW);
    }
    // soft red tint of control background when unset
    COLORREF fallback = RgbToCOLORREF(0x5c1a1a);
    if (IsLightColor(ThemeWindowControlBackgroundColor())) {
        fallback = RgbToCOLORREF(0xffe0e0);
    }
    return GetThemeCol(gCurrentTheme->errorBackgroundColor, fallback);
}

COLORREF ThemeNotificationsBackgroundColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_WINDOW);
    }
    COLORREF fallback = AdjustLightness2(ThemeWindowBackgroundColor(), 10);
    return GetThemeCol(gCurrentTheme->notificationBackgroundColor, fallback);
}

COLORREF ThemeNotificationsTextColor() {
    return ThemeWindowTextColor();
}

// Warning notifications are meant to read as warnings, so the fallback is amber
// in both directions: light themes get the classic yellow, dark themes a muted
// dark amber. Deriving it from the theme's own accent (as we used to) produced
// saturated, unrelated hues -- Dracula's warnings came out bright purple.
COLORREF ThemeNotificationsHighlightColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_HIGHLIGHT);
    }
    COLORREF fallback;
    if (IsLightColor(ThemeNotificationsBackgroundColor())) {
        fallback = RgbToCOLORREF(0xFFEE70); // yellowish
    } else {
        fallback = RgbToCOLORREF(0x422006); // dark amber
    }
    return GetThemeCol(gCurrentTheme->notificationHighlightColor, fallback);
}

COLORREF ThemeNotificationsHighlightTextColor() {
    if (gUseHighContrast) {
        return GetSysColor(COLOR_HIGHLIGHTTEXT);
    }
    COLORREF fallback;
    if (IsLightColor(ThemeNotificationsBackgroundColor())) {
        fallback = RgbToCOLORREF(0x8d0801); // reddish
    } else {
        fallback = RgbToCOLORREF(0xFDE68A); // light amber
    }
    return GetThemeCol(gCurrentTheme->notificationHighlightTextColor, fallback);
}

// Links inside a warning notification. The theme's link color is picked to sit on
// the window background and can vanish on the amber warning background (Dracula's
// cyan, Choco's yellow). Links are underlined, so reusing the warning text color
// stays legible and still reads as a link.
COLORREF ThemeNotificationsHighlightLinkColor() {
    return ThemeNotificationsHighlightTextColor();
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
