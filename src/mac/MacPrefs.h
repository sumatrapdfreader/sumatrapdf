/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MacPrefsViewState {
    bool valid;
    bool continuous;
    double zoomVirtual;
    int rotation;
    int pageNo;
};

void MacPrefsInit(const char* settingsPath);
void MacPrefsShutdown();
bool MacPrefsOpenDocument(const char* path, MacPrefsViewState* state);
void MacPrefsSaveDocument(const char* path, const MacPrefsViewState* state);
void MacPrefsSaveSession(const char* path, const MacPrefsViewState* state);
char* MacPrefsCopySessionPath(MacPrefsViewState* state);
void MacPrefsBeginSession();
void MacPrefsAppendSession(const char* path, const MacPrefsViewState* state);
void MacPrefsFinishSession(int activeTab);
int MacPrefsSessionCount();
char* MacPrefsCopySessionTab(int index, MacPrefsViewState* state);
int MacPrefsSessionActiveTab();
int MacPrefsRecentCount();
char* MacPrefsCopyRecentPath(int index);
bool MacPrefsAddFavorite(const char* path, int pageNo);
bool MacPrefsRemoveFavorite(const char* path, int pageNo);
bool MacPrefsHasFavorite(const char* path, int pageNo);
int MacPrefsFavoriteCount();
char* MacPrefsCopyFavoritePath(int index);
int MacPrefsFavoritePage(int index);
