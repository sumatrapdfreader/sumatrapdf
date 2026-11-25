/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the model half of orig's NavFilesInFolder.cpp: listing a folder's (or
// the home view's) openable entries, sorted. The window half - orig's
// NavFilesInFolderWnd, built of win32 virtual controls - is gpui and lives in
// gui/NavFilesUI.cpp.

#include "base/Base.h"
#include "base/DirScan.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Timer.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SumatraPDF.h"
#include "NavFilesInFolder.h"
#include "NavFilesInFolderCommon.h"

#include "SumatraLog.h"

// A modeless directory browser listing sub-directories and files SumatraPDF
// can open (judged by extension). Enter / double-click replaces the document
// in the current tab or descends into a directory; Ctrl + Enter / Ctrl +
// double-click switches to the tab already showing the file or opens it in a
// new tab; ".." goes one directory up. Back / Forward walk the folders visited
// in this session, Up goes to the parent, Home lists the drives and Explorer's
// Quick access. The window stays open so you can open several files in
// succession; Esc or the close button dismisses it.

// ng: NavFileEntry moved to NavFilesInFolder.h

#if !OS_WIN
// ng: orig's are in base/Win.cpp. Off Windows the "drives" are the root and
// the user's home directory, and there is no Quick access.
void ListDriveRoots(StrVec& out) {
    out.Append(StrL("/"));
    TempStr homeDir = GetHomeDirTemp();
    if (len(homeDir) > 1 && dir::Exists(homeDir)) {
        out.Append(homeDir);
    }
}

bool ListShellQuickAccess(StrVec&, StrVec&) {
    return false;
}
#endif

bool NavFiles_UnitTestHidden() {
#if OS_WIN
    return true;
#else
    TempStr root = GetTempFilePathTemp(StrL("nav-hidden-ut"));
    file::Delete(root);
    if (!dir::Create(root)) {
        return false;
    }
    TempStr hidden = path::JoinTemp(root, StrL(".hidden.pdf"));
    TempStr visible = path::JoinTemp(root, StrL("visible.pdf"));
    bool ok = file::WriteFile(hidden, StrL("hidden")) && file::WriteFile(visible, StrL("visible"));

    Vec<NavFileEntry> entries;
    if (ok) {
        CollectNavEntriesForDir(root, entries);
        bool foundHidden = false;
        bool foundVisible = false;
        for (const NavFileEntry& e : entries) {
            foundHidden = foundHidden || str::Eq(e.name, StrL(".hidden.pdf"));
            foundVisible = foundVisible || str::Eq(e.name, StrL("visible.pdf"));
        }
        ok = !foundHidden && foundVisible;
    }
    FreeNavEntries(entries);
    ok = dir::RemoveAll(root) && ok;
    return ok;
#endif
}
