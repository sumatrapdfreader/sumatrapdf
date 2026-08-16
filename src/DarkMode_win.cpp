/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Everything that talks to darkmodelib. The rest of the app calls the functions
// in DarkMode_win.h and never names DarkMode:: itself, so the conditions that
// used to be repeated at ~40 call sites - is the library compiled in, is it
// enabled, is the current theme the default one - live here instead.

#include "base/Base.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AppTools.h"
#include "Theme.h"
#include "gui/win/TabsCtrl.h"
#include "DarkMode_win.h"

#include "DarkModeSubclass.h"

// darkmodelib only supports the architectures we still ship it for; older
// 32-bit builds run without it
#if !defined(_DARKMODELIB_NOT_USED) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__arm64__) || defined(__arm64) || defined(_M_ARM64))
static bool gUseDarkModeLib = true;
#else
static bool gUseDarkModeLib = false;
#endif

bool DarkModeIsActive() {
    return gUseDarkModeLib && DarkMode::isEnabled();
}

Color DarkModeDialogBgColor() {
    if (DarkModeIsActive()) {
        return ThemeWindowControlBackgroundColor();
    }
    return MkGray(0xee);
}

void DarkModeInit() {
    // WindowBase::UpdateTheme() re-applies dark mode through this hook, so
    // gui/ never names darkmodelib. Installed even when the lib isn't used:
    // DarkModeApplyToWindow() no-ops then
    gWindowBaseApplyDarkMode = DarkModeApplyToWindow;
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::initDarkMode();
    DarkMode::setColorizeTitleBarConfig(true);
}

// push the current palette (which may be the system's, in high contrast mode)
// to darkmodelib, which draws the controls we don't draw ourselves
void DarkModeApplyThemeColors() {
    if (!gUseDarkModeLib) {
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
    Color ctrlBg = ThemeWindowControlBackgroundColor();
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

void DarkModeRememberTreeViewStyle() {
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::setPrevTreeViewStyle();
}

void DarkModeApplyToWindow(HWND hwnd) {
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::setDarkWndSafe(hwnd);
}

void DarkModeApplyToWindowAndEraseBg(HWND hwnd) {
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::setDarkWndSafe(hwnd);
    DarkMode::setWindowEraseBgSubclass(hwnd);
}

void DarkModeApplyToNotifyWindowAndEraseBg(HWND hwnd) {
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::setDarkWndNotifySafe(hwnd);
    DarkMode::setWindowEraseBgSubclass(hwnd);
}

void DarkModeApplyToTitleBar(HWND hwnd) {
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::setDarkTitleBarEx(hwnd, true);
}

// Some of our popup windows create their children after the window itself, and
// darkmodelib only themes the children that exist when it is called - so they
// call this once the children are there (issues #5894, #5895).
void DarkModeApplyToPopupWindow(HWND hwnd) {
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::setDarkTitleBarEx(hwnd, true);
    if (IsCurrentThemeDefault()) {
        return;
    }
    DarkMode::setChildCtrlsSubclassAndTheme(hwnd);
    DarkMode::setWindowNotifyCustomDrawSubclass(hwnd);
}

void DarkModeApplyToMenuWindow(HWND hwnd) {
    if (!DarkModeIsActive() || !hwnd) {
        return;
    }
    DarkMode::setDarkTitleBarEx(hwnd, false);
}

void DarkModeApplyToMenuBar(HWND hwndRebar) {
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::setWindowNotifyCustomDrawSubclass(hwndRebar);
    DarkMode::setChildCtrlsSubclassAndTheme(hwndRebar);
}

void DarkModeApplyToChildControls(HWND hwnd) {
    if (!gUseDarkModeLib || IsCurrentThemeDefault()) {
        return;
    }
    DarkMode::setChildCtrlsSubclassAndTheme(hwnd);
}

// Theming a tooltip applies a visual style to it, which resets its font to the
// theme's - wrong for a control we size and populate ourselves, so put our font
// back afterwards (issue #5894).
static void ApplyToInfotip(MainWindow* win) {
    if (!win || !win->infotip || !win->infotip->hwnd) {
        return;
    }
    DarkMode::setDarkTooltips(win->infotip->hwnd, (int)DarkMode::ToolTipsType::tooltip);
    win->infotip->SetFont(GetAppFont());
}

void DarkModeApplyToNewFrame(MainWindow* win) {
    if (!gUseDarkModeLib || IsCurrentThemeDefault()) {
        return;
    }
    DarkMode::setDarkTitleBarEx(win->hwndFrame, true);
    DarkMode::setChildCtrlsSubclassAndTheme(win->hwndFrame);
    DarkMode::removeTabCtrlSubclass(win->tabsCtrl->hwnd);
    DarkMode::setDarkScrollBar(win->hwndCanvas);
    DarkMode::setWindowMenuBarSubclass(win->hwndFrame);
    ApplyToInfotip(win);
}

void DarkModeApplyToFrameAfterThemeChange(MainWindow* win) {
    if (!gUseDarkModeLib) {
        return;
    }
    DarkMode::setDarkTitleBarEx(win->hwndFrame, true);
    DarkMode::setChildCtrlsTheme(win->hwndFrame);
    if (win->tabsCtrl) {
        DarkMode::removeTabCtrlSubclass(win->tabsCtrl->hwnd);
    }
    DarkMode::setDarkScrollBar(win->hwndCanvas);
    DarkMode::setWindowMenuBarSubclass(win->hwndFrame);
    ApplyToInfotip(win);
}
