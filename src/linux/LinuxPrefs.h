/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DocumentView;

void LinuxPrefsInit();
void LinuxPrefsShutdown();
void LinuxPrefsOpenView(DocumentView* view, Str path);
void LinuxPrefsSaveView(DocumentView* view, Str path);
int LinuxPrefsRecentCount();
Str LinuxPrefsRecentPath(int index);
bool LinuxPrefsAddFavorite(Str path, int pageNo);
bool LinuxPrefsRemoveFavorite(Str path, int pageNo);
int LinuxPrefsFavoriteCount();
Str LinuxPrefsFavoritePath(int index);
Str LinuxPrefsFavoriteLabel(int index);
int LinuxPrefsFavoritePageNo(int index);
