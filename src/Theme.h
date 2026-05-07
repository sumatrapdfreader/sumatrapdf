/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

void SetTheme(const char* name);
void SetCurrentThemeFromSettings();
void SelectNextTheme();
void CreateThemeCommands();

COLORREF ThemeDocumentColors(COLORREF&);
COLORREF ThemePageRenderColors(COLORREF&);
COLORREF ThemeMainWindowBackgroundColor();
COLORREF ThemeControlBackgroundColor();
COLORREF ThemeWindowBackgroundColor();
COLORREF ThemeWindowTextColor();
COLORREF ThemeWindowTextDisabledColor();
COLORREF ThemeWindowControlBackgroundColor();
COLORREF ThemeWindowLinkColor();
bool PrettyStyleEnabled();
COLORREF PrettySurfaceColor();
COLORREF PrettySurfaceAltColor();
COLORREF PrettyBorderColor();
COLORREF PrettyAccentColor();
COLORREF ThemeNotificationsBackgroundColor();
COLORREF ThemeNotificationsTextColor();
COLORREF ThemeNotificationsHighlightColor();
COLORREF ThemeNotificationsHighlightTextColor();
COLORREF ThemeNotificationsProgressColor();
COLORREF ThemeTabBgHighlight();
COLORREF ThemeTabStroke();
COLORREF ThemeTabActiveStrip();
bool ThemeColorizeControls();
bool IsCurrentThemeDefault();
COLORREF AccentColor(COLORREF col, int light, int dark = 0);
void FreeThemes();
bool UseDarkModeLib();

struct TabColorTokens {
    COLORREF bgHighlight;  // fondo de tab seleccionada (highlight cálido)
    COLORREF stroke;       // borde de tab seleccionada
    COLORREF activeStrip;  // franja inferior activa
};

TabColorTokens GetTabColorTokens();

extern int gFirstSetThemeCmdId;
extern int gLastSetThemeCmdId;
extern int gCurrSetThemeCmdId;
