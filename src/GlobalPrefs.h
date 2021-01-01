/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern GlobalPrefs* gGlobalPrefs;

DisplayState* NewDisplayState(const WCHAR* filePath);
void DeleteDisplayState(DisplayState* ds);

Favorite* NewFavorite(int pageNo, const WCHAR* name, const WCHAR* pageLabel);
void DeleteFavorite(Favorite* fav);

GlobalPrefs* NewGlobalPrefs(const char* data);
std::span<u8> SerializeGlobalPrefs(GlobalPrefs* gp, const char* prevData);
void DeleteGlobalPrefs(GlobalPrefs* gp);

SessionData* NewSessionData();
TabState* NewTabState(DisplayState* ds);
void ResetSessionState(Vec<SessionData*>* sessionData);
