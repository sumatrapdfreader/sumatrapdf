/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern GlobalPrefs* gGlobalPrefs;

FileState* NewDisplayState(const char* filePath);
void DeleteDisplayState(FileState* fs);

Favorite* NewFavorite(int pageNo, const WCHAR* name, const WCHAR* pageLabel);
void DeleteFavorite(Favorite* fav);

GlobalPrefs* NewGlobalPrefs(const char* data);
ByteSlice SerializeGlobalPrefs(GlobalPrefs* prefs, const char* prevData);
void DeleteGlobalPrefs(GlobalPrefs* gp);

SessionData* NewSessionData();
TabState* NewTabState(FileState* fs);
void ResetSessionState(Vec<SessionData*>* sessionData);
ParsedColor* GetParsedColor(const char* s, ParsedColor& parsed);

void SetFileStatePath(FileState* fs, const char* path);
void SetFileStatePath(FileState* fs, const WCHAR* path);

#define GetPrefsColor(name) GetParsedColor(name, name##Parsed)
