/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by NavFilesInFolderCommon.cpp and each app's NavFilesInFolder.cpp ---

void FreeNavEntry(NavFileEntry& e);
bool CanOpenFile(Str path);

// base/Win.cpp on Windows, a stub in ng elsewhere
bool ListShellQuickAccess(StrVec& dirsOut, StrVec& filesOut);
void ListDriveRoots(StrVec& out);

constexpr int kNavHistoryMax = 100;
extern StrVec gNavHistory;
extern int gNavHistIdx;
void NavHistoryPush(Str dir);
