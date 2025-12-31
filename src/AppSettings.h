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
void CleanUpSettings();
void RegisterSettingsForFileChanges();
void UnregisterSettingsForFileChanges();
int GetAppFontSize();
HFONT GetAppFont();
int GetAppMenuFontSize();
bool IsAppFontSizeDefault();
HFONT GetAppMenuFont();
HFONT GetAppBiggerFont();
HFONT GetAppTreeFont();
bool IsMenuFontSizeDefault();
