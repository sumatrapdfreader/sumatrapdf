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
void ScheduleSaveSettings();
void FlushScheduledSaveSettings();
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
TempStr ZoomLevelStrExact(float zoom);
// the command for each level the zoom buttons step through, in that order
Vec<int>* GetZoomStepCmdIds();
void CollectZoomLevels(Vec<float>& out, bool forChm);

extern Settings* gSettings;

bool* FindSettingsBoolSetting(Str name);
void ToggleSettingsBool(bool*);

FileState* NewFileState(Str);
void DeleteFileState(FileState*);
void DeleteFileStates(Vec<FileState*>*);

// a document's per-file ebook settings are read on the thread that loads it,
// so a load that doesn't run on the UI thread needs its own copy (#4600).
// both are null-safe
FileEBookUI* NewFileEBookUI();
FileEBookUI* CopyFileEBookUI(const FileEBookUI*);
void DeleteFileEBookUI(FileEBookUI*);

Favorite* NewFavorite(int pageNo, Str name, Str pageLabel, Str bookmark = {});
void DeleteFavorite(Favorite* fav);

Settings* NewSettings(Str);
Str SerializeSettings(Settings* prefs, Str prevData);
void DeleteSettings(Settings*);

SessionData* NewSessionData();
TabState* NewTabState(FileState*);
void DeleteTabState(TabState*);
void FreeSessionData(SessionData*);
void FreeSessionDataVec(Vec<SessionData*>*);
// A color setting's parse, done on first use and cached in the setting itself.
// The Theme*Color() accessors call these on every paint, so the already-parsed
// case has to be a load and a branch, not a call into another translation unit.
inline ParsedColor* GetParsedColor(ParsedColor& parsed) {
    if (!parsed.wasParsed) {
        ParseColor(parsed);
    }
    return &parsed;
}

inline Color GetParsedColor(ParsedColor& parsed, Color def) {
    if (!parsed.wasParsed) {
        ParseColor(parsed);
    }
    return parsed.parsedOk ? parsed.col : def;
}

void SetFileStatePath(FileState* fs, Str path);
void SetFileStatePath(FileState* fs, WStr path);

Themes* ParseThemes(Str);
void FreeParsedThemes(Themes*);

#define GetPrefsColor(name) GetParsedColor(name)
