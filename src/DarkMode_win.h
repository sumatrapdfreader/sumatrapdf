/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The only place that talks to darkmodelib (ext/darkmodelib). Callers say what
// they want done to a window, not how: whether the library is compiled in at
// all, whether it is currently drawing a dark theme, and whether the current
// theme is the default one are all decided on the other side of this header.

struct MainWindow;

// darkmodelib is in use and drawing something other than the classic look. For
// the few places that have to draw differently themselves
bool DarkModeIsActive();
// background for a dialog we create ourselves
Color DarkModeDialogBgColor();

void DarkModeInit();
// push the current theme's palette into darkmodelib
void DarkModeApplyThemeColors();
void DarkModeRememberTreeViewStyle();

// a dialog or other window: theme it and its children
void DarkModeApplyToWindow(HWND);
// ... and have it erase its own background, which stops the flicker
void DarkModeApplyToWindowAndEraseBg(HWND);
// ... for a window that also wants owner-draw notifications
void DarkModeApplyToNotifyWindowAndEraseBg(HWND);
// only the title bar, for a window that themes its children itself
void DarkModeApplyToTitleBar(HWND);
// a popup window, once its children exist
void DarkModeApplyToPopupWindow(HWND);
// the window a menu opens in
void DarkModeApplyToMenuWindow(HWND);
// the menu bar's rebar
void DarkModeApplyToMenuBar(HWND hwndRebar);
// a panel whose child controls need theming
void DarkModeApplyToChildControls(HWND);

// the main frame, right after it was created / after the theme changed
void DarkModeApplyToNewFrame(MainWindow*);
void DarkModeApplyToFrameAfterThemeChange(MainWindow*);

// ChooseColorW, themed when darkmodelib is drawing
struct tagCHOOSECOLORW;
bool DarkModeChooseColor(tagCHOOSECOLORW*);
