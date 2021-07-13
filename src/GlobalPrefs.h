/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern GlobalPrefs* gGlobalPrefs;

FileState* NewDisplayState(const WCHAR* filePath);
void DeleteDisplayState(FileState* ds);

Favorite* NewFavorite(int pageNo, const WCHAR* name, const WCHAR* pageLabel);
void DeleteFavorite(Favorite* fav);

GlobalPrefs* NewGlobalPrefs(const char* data);
std::span<u8> SerializeGlobalPrefs(GlobalPrefs* gp, const char* prevData);
void DeleteGlobalPrefs(GlobalPrefs* gp);

SessionData* NewSessionData();
TabState* NewTabState(FileState* ds);
void ResetSessionState(Vec<SessionData*>* sessionData);
ParsedColor* GetParsedColor(const char* s, ParsedColor& parsed);

#define GetPrefsColor(name) GetParsedColor(name, name##Parsed)
