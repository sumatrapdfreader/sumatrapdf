/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#ifndef SUMATRA_THEME
#define SUMATRA_THEME

#if defined(ENABLE_THEME)

#include "utils/BaseUtil.h"

// The number of themes
#define THEME_COUNT 3

struct MainWindowStyle {
    // Background color of recently added, about, and properties menus
    COLORREF backgroundColor;
    // Text color of recently added, about, and properties menus
    COLORREF textColor;
    // Link color on recently added, about, and properties menus
    COLORREF linkColor;
};

struct DocumentStyle {
    // Background color behind the open document
    COLORREF canvasColor;
    // Color value with which white (background) will be substituted
    COLORREF backgroundColor;
    // Color value with which black (text) will be substituted
    COLORREF textColor;
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

struct TabCloseStyle {
    // Color of the X button
    COLORREF xColor;
    // Color of the circle surrounding the X button
    COLORREF circleColor;
};

struct TabStyle {
    // Background color of the tab
    COLORREF backgroundColor;
    // Text color of the tab
    COLORREF textColor;
    // Style of the close (X button) by default
    TabCloseStyle close;
};

struct TabTheme {
    // Height of the tab bar
    int height;
    // Style of the current file tab
    TabStyle current;
    // Style of background tabs
    TabStyle background;
    // Style of the highlighted tab (hovered over)
    TabStyle highlighted;
    // Whether or not the circle around the tab X is displayed on hover
    bool closeCircleEnabled;
    // The width of the pen drawing the tab X
    float closePenWidth;
    // Style of the close (X button) when the mouse hovers over it
    TabCloseStyle hoveredClose;
    // Style of the close (X button) when the mouse is clicked down over it
    TabCloseStyle clickedClose;
};

struct Theme {
    // Name of the theme
    char* name;
    // Style of the main window
    MainWindowStyle mainWindow;
    // Style of documents
    DocumentStyle document;
    // Style of tabs
    TabTheme tab;
    // Style of notifications
    NotificationStyle notifications;
};

// Function definitions
Theme* GetThemeByName(char* name);
Theme* GetThemeByIndex(int index);
Theme* GetCurrentTheme();

int GetThemeIndex(Theme* theme);
int GetCurrentThemeIndex();

#endif

#endif // !THEME
