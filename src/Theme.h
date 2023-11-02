/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "utils/BaseUtil.h"

void SelectNextTheme();
void SetThemeByIndex(int);

void SetCurrentThemeFromSettings();

int GetCurrentThemeIndex();

COLORREF ThemeDocumentColors(COLORREF&);
COLORREF ThemeMainWindowBackgroundColor();
COLORREF ThemeControlBackgroundColor();
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
