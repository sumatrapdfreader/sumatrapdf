/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* enum from windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remembered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

extern bool gDontSaveSettings;

extern Vec<SessionData*>* gInitialSessionData;

TempStr GetSettingsPathTemp();
TempStr GetSettingsFileNameTemp();

bool LoadSettings();
bool SaveSettings();
void ForceReloadSettings();
void ApplySettingsToOpenWindows();
void CleanUpSettings();
void RegisterSettingsForFileChanges();
void UnregisterSettingsForFileChanges();
int GetAppFontSize(HWND hwnd);
int GetAppFontSizeForDpi(int dpi);
HFONT GetAppFont(HWND hwnd);
HFONT GetAppFontForDpi(int dpi);
int GetAppMenuFontSize(HWND hwnd);
int GetAppMenuFontSizeForDpi(int dpi);
bool IsAppFontSizeDefault();
HFONT GetAppMenuFont(HWND hwnd);
HFONT GetAppMenuFontForDpi(int dpi);
HFONT GetAppBiggerFont(HWND hwnd);
HFONT GetAppBiggerFontForDpi(int dpi);
HFONT GetAppTreeFont(HWND hwnd);
HFONT GetAppTreeFontForDpi(int dpi);
HFONT GetAppTreeFontEx(HWND hwnd, bool bold, bool italic);
HFONT GetAppTreeFontExForDpi(int dpi, bool bold, bool italic);
HFONT GetAppSidebarLabelFont(HWND hwnd);
HFONT GetAppSidebarLabelFontForDpi(int dpi);
bool IsMenuFontSizeDefault();
