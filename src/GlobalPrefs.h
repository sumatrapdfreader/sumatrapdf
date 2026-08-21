/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern GlobalPrefs* gGlobalPrefs;

bool* FindGlobalPrefsBoolSetting(Str name);

FileState* NewFileState(Str);
void DeleteFileState(FileState*);
void DeleteFileStates(Vec<FileState*>*);

// a document's per-file ebook settings are read on the thread that loads it,
// so a load that doesn't run on the UI thread needs its own copy (#4600).
// both are null-safe
FileEBookUI* NewFileEBookUI();
FileEBookUI* CopyFileEBookUI(const FileEBookUI*);
void DeleteFileEBookUI(FileEBookUI*);

Favorite* NewFavorite(int pageNo, Str name, Str pageLabel);
void DeleteFavorite(Favorite* fav);

GlobalPrefs* NewGlobalPrefs(Str);
Str SerializeGlobalPrefs(GlobalPrefs* prefs, Str prevData);
void DeleteGlobalPrefs(GlobalPrefs*);

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
