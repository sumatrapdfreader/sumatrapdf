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
void UpdateThemeAfterHighContrastChange();
bool ThemeUsesHighContrastColors();
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

struct VirtButton;
struct PlatformFont;

// The buttons are virtual controls, so they take their look from the theme
// rather than from the system: a filled box with a border, brighter on hover.
// `isDefault` is a shade stronger, like a native default button.
// StyleThemedButton() is what a window calls again after a theme change
void StyleThemedButton(VirtButton*, bool isDefault);
VirtButton* NewThemedButton(HWND hwndForDpi, Str text, PlatformFont*, bool isDefault);

extern int gFirstSetThemeCmdId;
extern int gLastSetThemeCmdId;
extern int gCurrSetThemeCmdId;
