/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "utils/BaseUtil.h"

void SelectNextTheme();
void SetThemeByIndex(int);

void SetCurrentThemeFromSettings();

int GetCurrentThemeIndex();

// These functions take into account both gPrefs and the theme.
// Access to these colors must go through them until everything is
// configured through themes.
void GetDocumentColors(COLORREF& text, COLORREF& bg);
COLORREF GetMainWindowBackgroundColor();
COLORREF GetControlBackgroundColor();

COLORREF ThemeWindowBackgroundColor();
COLORREF ThemeWindowTextColor();
COLORREF ThemeWindowControlBackgroundColor();
COLORREF ThemeWindowLinkColor();
COLORREF ThemeNotificationsBackgroundColor();
COLORREF ThemeNotificationsTextColor();
COLORREF ThemeNotificationsHighlightColor();
COLORREF ThemeNotificationsHighlightTextColor();
COLORREF ThemeNotificationsProgressColor();
bool ThemeColorizeControls();
