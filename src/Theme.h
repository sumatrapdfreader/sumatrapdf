/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

void SetTheme(Str name);
void SetCurrentThemeFromSettings();
void SetThemeByIndex(int themeIdx);
int ThemeGetCount();
Str ThemeGetNameAt(int idx);
int ThemeGetCurrentIndex();
void ToggleLightDarkTheme();
Str ToggleLightDarkThemeTargetName();
void SumatraUpdateTheme();
void UpdateThemeAfterSystemColorChange();
void UpdateThemeAfterHighContrastChange();
bool ThemeUsesHighContrastColors();
void CreateThemeCommands();

Color ThemeDocumentColors(Color&);
Color ThemePageRenderColors(Color&);
Color ThemeMainWindowBackgroundColor();
Color ThemeControlBackgroundColor();
Color ThemeWindowBackgroundColor();
Color ThemeWindowTextColor();
Color ThemeWindowTextDisabledColor();
// the colors the OS draws its own UI in (Theme_win.cpp); the default theme and
// high contrast mode defer to them
Color SysWindowBgColor();
Color SysWindowTextColor();
Color SysControlTextColor();
Color SysDisabledTextColor();
Color SysLinkColor();
Color SysHighlightBgColor();
Color SysHighlightTextColor();
Color ThemeWindowDarkerTextColor();
Color ThemeWindowControlBackgroundColor();
Color ThemeActiveTabBackgroundColor();
Color ThemeInactiveTabBackgroundColor();
Color ThemeWindowLinkColor();
Color ThemeHotBackgroundColor();
Color ThemeEdgeColor();
Color ThemeHotEdgeColor();
Color ThemeDisabledEdgeColor();
Color ThemeErrorBackgroundColor();
Color ThemeNotificationsBackgroundColor();
Color ThemeNotificationsTextColor();
Color ThemeNotificationsHighlightColor();
Color ThemeNotificationsHighlightTextColor();
Color ThemeNotificationsHighlightLinkColor();
Color ThemeNotificationsProgressColor();
bool ThemeColorizeControls();
bool IsCurrentThemeDefault();
void FreeThemes();
bool MigrateRenamedThemeNames();
bool GetInvertPageColors();
void SetInvertPageColors(bool);

struct VirtButton;
struct PlatformFont;

// The buttons are virtual controls, so they take their look from the gui/ color
// defaults, which SumatraUpdateTheme() fills in from the theme: a filled box
// with a border, brighter on hover. `isDefault` is a shade stronger, like a
// native default button. Nothing has to re-style them after a theme change
VirtButton* NewThemedButton(HWND hwndForDpi, Str text, PlatformFont*, bool isDefault);

extern int gFirstSetThemeCmdId;
extern int gLastSetThemeCmdId;
extern int gCurrSetThemeCmdId;
