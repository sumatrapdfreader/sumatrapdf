/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

// ng: the model half of the picker (NavFilesInFolder.cpp), which the gpui
// window half (gui/NavFilesUI.cpp) shows. In orig these are static in
// NavFilesInFolder.cpp
struct NavFileEntry {
    Str name; // owned; leaf display name (dirs end with "\\"); ".." for parent
    Str path; // owned; full path (empty for "..")
    bool isDir = false;
    i64 size = 0; // file size; 0 for dirs / unknown
};

// The entries of `dir` SumatraPDF can open, plus its sub-directories, with
// ".." first. Dirs before files, each sorted naturally. An empty `dir` is the
// home view: drive roots, then Explorer's Quick access.
void CollectNavEntriesForDir(Str dir, Vec<NavFileEntry>& out);
void FreeNavEntries(Vec<NavFileEntry>& entries);
void StealNavEntries(Vec<NavFileEntry>& dst, Vec<NavFileEntry>& src);
bool SameNavEntries(const Vec<NavFileEntry>& a, const Vec<NavFileEntry>& b);
void AppendNavParentEntry(Vec<NavFileEntry>& entries);
bool NavDirHasParent(Str dir);
Str NavEntryBaseName(const NavFileEntry& e);
void ResetQuickAccessCache();

// Opens the navigate-files-in-folder picker (gui/NavFilesUI.cpp). When
// selectPath is set (absolute path to a file), browses that file's directory
// and selects it; otherwise uses the current document (or the newest history
// entry on the home page).
void ShowNavFilesInFolder(MainWindow* win, Str selectPath = {}, bool skipHistory = false);
TempStr NavFilesInFolderStateTemp(Str action, int idx, int* exitCodeOut);
