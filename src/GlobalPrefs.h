/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern GlobalPrefs* gGlobalPrefs;

FileState* NewFileState(Str);
void DeleteFileState(FileState*);
void DeleteFileStates(Vec<FileState*>*);

Favorite* NewFavorite(int pageNo, Str name, Str pageLabel);
void DeleteFavorite(Favorite* fav);

GlobalPrefs* NewGlobalPrefs(Str);
ByteSlice SerializeGlobalPrefs(GlobalPrefs* prefs, Str prevData);
void DeleteGlobalPrefs(GlobalPrefs*);

SessionData* NewSessionData();
TabState* NewTabState(FileState*);
void DeleteTabState(TabState*);
void FreeSessionData(SessionData*);
void FreeSessionDataVec(Vec<SessionData*>*);
ParsedColor* GetParsedColor(Str s, ParsedColor& parsed);
COLORREF GetParsedCOLORREF(Str s, ParsedColor& parsed, COLORREF def);

void SetFileStatePath(FileState* fs, Str path);
void SetFileStatePath(FileState* fs, WStr path);

Themes* ParseThemes(Str);
void FreeParsedThemes(Themes*);

#define GetPrefsColor(name) GetParsedColor(name, name##Parsed)
