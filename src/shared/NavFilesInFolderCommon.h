/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by NavFilesInFolderCommon.cpp and each app's NavFilesInFolder.cpp ---

void FreeNavEntry(NavFileEntry& e);
bool CanOpenFile(Str path);
void SortNavEntries(Vec<NavFileEntry>& entries, int firstIdx);
TempStr NavLeafNameTemp(DirIterEntry* de);
extern Mutex gQuickAccessMutex;
extern bool gQuickAccessCached;
void AppendHomeDirEntry(Vec<NavFileEntry>& out, Str path);

void GetQuickAccessCached(StrVec& dirsOut, StrVec& filesOut);

// base/Win.cpp on Windows, a stub in ng elsewhere
bool ListShellQuickAccess(StrVec& dirsOut, StrVec& filesOut);
void ListDriveRoots(StrVec& out);

void CollectHomeEntries(Vec<NavFileEntry>& out);

constexpr int kNavHistoryMax = 100;
extern StrVec gNavHistory;
extern int gNavHistIdx;
void NavHistoryPush(Str dir);

// implemented by each app
void AppendHomeFileEntry(Vec<NavFileEntry>& out, Str path);
