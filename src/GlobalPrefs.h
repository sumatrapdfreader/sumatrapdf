/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern GlobalPrefs* gGlobalPrefs;

FileState* NewFileState(const char*);
void DeleteFileState(FileState*);
void DeleteFileStates(Vec<FileState*>*);

Favorite* NewFavorite(int pageNo, const char* name, const char* pageLabel);
void DeleteFavorite(Favorite* fav);

GlobalPrefs* NewGlobalPrefs(const char*);
ByteSlice SerializeGlobalPrefs(GlobalPrefs* prefs, const char* prevData);
void DeleteGlobalPrefs(GlobalPrefs*);

SessionData* NewSessionData();
TabState* NewTabState(FileState*);
void FreeSessionData(SessionData*);
void FreeSessionDataVec(Vec<SessionData*>*);
ParsedColor* GetParsedColor(const char* s, ParsedColor& parsed);
COLORREF GetParsedCOLORREF(const char* s, ParsedColor& parsed, COLORREF def);

void SetFileStatePath(FileState* fs, const char* path);

Themes* ParseThemes(const char*);
void FreeParsedThemes(Themes*);

#define GetPrefsColor(name) GetParsedColor(name, name##Parsed)
