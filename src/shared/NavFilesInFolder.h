/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

struct NavFileEntry {
    Str name;
    Str path;
    bool isDir = false;
    i64 size = 0;
};

void CollectNavEntriesForDir(Str dir, Vec<NavFileEntry>& out);
void FreeNavEntries(Vec<NavFileEntry>& entries);
void StealNavEntries(Vec<NavFileEntry>& dst, Vec<NavFileEntry>& src);
bool SameNavEntries(const Vec<NavFileEntry>& a, const Vec<NavFileEntry>& b);
void AppendNavParentEntry(Vec<NavFileEntry>& entries);
bool NavDirHasParent(Str dir);
Str NavEntryBaseName(const NavFileEntry& e);
void ResetQuickAccessCache();

void ShowNavFilesInFolder(MainWindow* win, Str selectPath = {}, bool skipHistory = false);
TempStr NavFilesInFolderStateTemp(Str action, int idx, int* exitCodeOut);
