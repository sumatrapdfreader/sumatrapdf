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
void UpdateThemeAfterSystemColorChange();
void CreateThemeCommands();

COLORREF ThemeDocumentColors(COLORREF&);
COLORREF ThemePageRenderColors(COLORREF&);
COLORREF ThemeMainWindowBackgroundColor();
COLORREF ThemeControlBackgroundColor();
COLORREF ThemeWindowBackgroundColor();
COLORREF ThemeWindowTextColor();
COLORREF ThemeWindowTextDisabledColor();
COLORREF ThemeWindowDarkerTextColor();
COLORREF ThemeWindowControlBackgroundColor();
COLORREF ThemeWindowLinkColor();
COLORREF ThemeHotBackgroundColor();
COLORREF ThemeEdgeColor();
COLORREF ThemeHotEdgeColor();
COLORREF ThemeDisabledEdgeColor();
COLORREF ThemeErrorBackgroundColor();
COLORREF ThemeNotificationsBackgroundColor();
COLORREF ThemeNotificationsTextColor();
COLORREF ThemeNotificationsHighlightColor();
COLORREF ThemeNotificationsHighlightTextColor();
COLORREF ThemeNotificationsHighlightLinkColor();
COLORREF ThemeNotificationsProgressColor();
bool ThemeColorizeControls();
bool IsCurrentThemeDefault();
void FreeThemes();
bool UseDarkModeLib();
void ApplyDarkModeToPopupWindow(HWND hwnd);
bool MigrateRenamedThemeNames();
bool GetInvertPageColors();
void SetInvertPageColors(bool);

extern int gFirstSetThemeCmdId;
extern int gLastSetThemeCmdId;
extern int gCurrSetThemeCmdId;
