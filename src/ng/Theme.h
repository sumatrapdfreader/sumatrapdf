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
// the colors the OS draws its own UI in (Theme.cpp); the default theme and
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

// ng: orig also declares NewThemedButton() here; it makes a win32 virtual
// control, so it lands with the gpui UI (step 12)

extern int gFirstSetThemeCmdId;
extern int gLastSetThemeCmdId;
extern int gCurrSetThemeCmdId;
