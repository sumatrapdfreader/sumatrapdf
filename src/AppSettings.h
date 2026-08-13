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
int GetAppFontSize();
int GetAppFontSizeForDpi(int dpi);
HFONT GetAppFont();
HFONT GetAppFontForDpi(int dpi);
int GetAppMenuFontSize();
int GetAppMenuFontSizeForDpi(int dpi);
bool IsAppFontSizeDefault();
HFONT GetAppMenuFont();
HFONT GetAppMenuFontForDpi(int dpi);
HFONT GetAppBiggerFont();
HFONT GetAppBiggerFontForDpi(int dpi);
HFONT GetAppTreeFont();
HFONT GetAppTreeFontForDpi(int dpi);
HFONT GetAppTreeFontEx(bool bold, bool italic);
HFONT GetAppTreeFontExForDpi(int dpi, bool bold, bool italic);
HFONT GetAppSidebarLabelFont();
HFONT GetAppSidebarLabelFontForDpi(int dpi);
bool IsMenuFontSizeDefault();
