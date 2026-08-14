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

struct PlatformFont;

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
PlatformFont* GetAppFont();
PlatformFont* GetAppFontForDpi(int dpi);
int GetAppMenuFontSize();
int GetAppMenuFontSizeForDpi(int dpi);
bool IsAppFontSizeDefault();
PlatformFont* GetAppMenuFont();
PlatformFont* GetAppMenuFontForDpi(int dpi);
PlatformFont* GetAppBiggerFont();
PlatformFont* GetAppBiggerFontForDpi(int dpi);
PlatformFont* GetAppTreeFont();
PlatformFont* GetAppTreeFontForDpi(int dpi);
PlatformFont* GetAppTreeFontEx(bool bold, bool italic);
PlatformFont* GetAppTreeFontExForDpi(int dpi, bool bold, bool italic);
PlatformFont* GetAppSidebarLabelFont();
PlatformFont* GetAppSidebarLabelFontForDpi(int dpi);
bool IsMenuFontSizeDefault();

TempStr ZoomLevelStr(float zoom);
void CollectZoomLevels(Vec<float>& out, bool forChm);
