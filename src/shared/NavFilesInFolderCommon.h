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
