/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "utils/BaseUtil.h"

// The number of themes
constexpr const int kThemeCount = 3;

struct MainWindowStyle {
    // Background color of recently added, about, and properties menus
    COLORREF backgroundColor;
    // Background color of controls, menus, non-client areas, etc.
    COLORREF controlBackgroundColor;
    // Text color of recently added, about, and properties menus
    COLORREF textColor;
    // Link color on recently added, about, and properties menus
    COLORREF linkColor;
};

struct NotificationStyle {
    // Background color of the notification window
    COLORREF backgroundColor;
    // Text color of the notification window
    COLORREF textColor;
    // Color of the highlight box that surrounds the text when a notification is highlighted
    COLORREF highlightColor;
    // Color of the text when a notification is highlighted
    COLORREF highlightTextColor;
    // Background color of the progress bar in the notification window
    COLORREF progressColor;
};

struct Theme {
    // Name of the theme
    const char* name;
    // Style of the main window
    MainWindowStyle window;
    // Style of notifications
    NotificationStyle notifications;
    // Whether or not we colorize standard Windows controls and window areas
    bool colorizeControls;
};

extern Theme* gCurrentTheme;
void SelectNextTheme();
void SetThemeByIndex(int);

void SetCurrentThemeFromSettings();

int GetCurrentThemeIndex();

// These functions take into account both gPrefs and the theme.
// Access to these colors must go through them until everything is
// configured through themes.
void GetDocumentColors(COLORREF& text, COLORREF& bg);
COLORREF GetMainWindowBackgroundColor();

constexpr COLORREF RgbToCOLORREF(COLORREF rgb) {
    return ((rgb & 0x0000FF) << 16) | (rgb & 0x00FF00) | ((rgb & 0xFF0000) >> 16);
}
