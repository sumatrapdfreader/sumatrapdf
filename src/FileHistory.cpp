/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/UITask.h"

#include "Settings.h"
#include "AppSettings.h"
#include "FileThumbnails.h"
#include "HomePage.h"
#include "FileHistory.h"

/* Handling of file history list.

We keep a mostly infinite list of all (still existing in the file system)
files that a user has ever opened. For each file we also keep a bunch of
attributes describing the display state at the time the file was closed.

We persist this list inside preferences file to something looking like this:

FileStates [
FilePath =  C:\path\to\file.pdf
DisplayMode = single page
PageNo =  1
ZoomVirtual = 123.4567
Window State = 2
...
]
etc...

We deserialize this info at startup and serialize when the application
quits.
*/

// maximum number of files to remember in total
// (to keep the settings file within reasonable bounds)
constexpr int kFileHistoryMaxFiles = 1000;

// maximum number of most frequently used files that will be shown on the
// Frequent Read list (space permitting)
constexpr int kFileHistoryMaxFrequent = 1000;

// owned by gSettings->fileStates
static Vec<FileState*>* gStates;

Vec<FileState*>* FileHistoryStates() {
    return gStates;
}

void FileHistorySetStates(Vec<FileState*>* states) {
    gStates = states;
}

void FileHistoryAppend(FileState* fs) {
    ReportIf(len(fs->filePath) == 0);
    VecAppend(*gStates, fs);
}

// the home page layout cache holds raw FileState* from this list, so it has to
// be dropped whenever an entry leaves it (the caller usually frees the
// FileState right after; crash 8c7b045cb). It is rebuilt on the next paint
void FileHistoryRemove(FileState* fs) {
    HomePageInvalidateLayoutCache();
    VecRemove(*gStates, fs);
}

void FileHistoryClear(bool keepFavorites) {
    if (!gStates) {
        return;
    }
    HomePageInvalidateLayoutCache();
    Vec<FileState*> keep;
    for (int i = 0; i < len(*gStates); i++) {
        if (keepFavorites && len(*(*gStates)[i]->favorites) > 0) {
            (*gStates)[i]->openCount = 0;
            VecAppend(keep, (*gStates)[i]);
        } else {
            DeleteFileState((*gStates)[i]);
        }
    }
    *gStates = keep;
}

FileState* FileHistoryGet(int index) {
    if (index < 0 || index >= len(*gStates)) {
        return nullptr;
    }
    return (*gStates)[index];
}

FileState* FileHistoryFindByPath(Str filePath) {
    int n = len(*gStates);
    for (int i = n - 1; i >= 0; i--) {
        FileState* fs = (*gStates)[i];
        if (str::EqI(fs->filePath, filePath)) {
            return fs;
        }
    }
    return nullptr;
}

FileState* FileHistoryMarkFileLoaded(Str filePath) {
    ReportIf(len(filePath) == 0);
    // if a history entry with the same name already exists,
    // then reuse it. That way we don't have duplicates and
    // the file moves to the front of the list
    FileState* fs = FileHistoryFindByPath(filePath);
    if (!fs) {
        fs = NewFileState(filePath);
        fs->useDefaultState = true;
    } else {
        VecRemove(*gStates, fs);
        fs->isMissing = false;
    }
    VecInsertAt(*gStates, 0, fs);
    fs->openCount++;
    return fs;
}

bool FileHistoryMarkFileInexistent(Str filePath, bool hide) {
    ReportIf(len(filePath) == 0);
    FileState* state = FileHistoryFindByPath(filePath);
    if (!state) {
        // keep a record so IsMissing can be persisted in settings (fixes #5585)
        state = NewFileState(filePath);
        VecAppend(*gStates, state);
    }
    // move the file history entry to the end of the list
    // of recently opened documents (if it exists at all),
    // so that the user could still try opening it again
    // and so that we don't completely forget the settings,
    // should the file reappear later on
    int newIdx = hide ? INT_MAX : kFileHistoryMaxRecent - 1;
    int idx = VecFind(*gStates, state);
    if (idx < newIdx && state != VecLast(*gStates)) {
        VecRemove(*gStates, state);
        if (len(*gStates) <= newIdx) {
            VecAppend(*gStates, state);
        } else {
            VecInsertAt(*gStates, newIdx, state);
        }
    }
    // also delete the thumbnail and move the link towards the
    // back in the Frequently Read list
    FreePixmap(state->thumbnail);
    state->thumbnail = nullptr;
    state->openCount >>= 2;
    state->isMissing = hide;
    logf("MarkFileInexistent: '%s', isMissing: %d\n", filePath, (int)hide);
    return true;
}

// sorts the most often used files first
static int cmpOpenCount(FileState* const* a, FileState* const* b) {
    FileState* dsA = *a;
    FileState* dsB = *b;
    // sort pinned documents before unpinned ones
    if (dsA->isPinned != dsB->isPinned) {
        return dsA->isPinned ? -1 : 1;
    }
    // sort pinned documents alphabetically
    if (dsA->isPinned) {
        return str::CmpNatural(path::GetBaseNameTemp(dsA->filePath), path::GetBaseNameTemp(dsB->filePath));
    }
    // sort often opened documents first
    if (dsA->openCount != dsB->openCount) {
        return dsB->openCount - dsA->openCount;
    }
    // use recency as the criterion in case of equal open counts
    return dsA->index < dsB->index ? -1 : 1;
}

// fills `list` with a shallow copy of the file history list (the states stay
// owned by the history), with all missing states filtered out, sorted by `cmp`
static void GetSortedStates(Vec<FileState*>& list, VecSortCmp<FileState*>::Fn cmp) {
    ReportIf(len(list) > 0);
    int i = 0;
    for (FileState* ds : *gStates) {
        ds->index = i++;
        if (!ds->isMissing || ds->isPinned) {
            VecAppend(list, ds);
        }
    }
    VecSort(list, cmp);
}

// sorted by open count (which has a pre-multiplied recency factor)
void FileHistoryGetFrequencyOrder(Vec<FileState*>& list) {
    GetSortedStates(list, cmpOpenCount);
}

// sorts recently opened files first
static int cmpRecentlyOpened(FileState* const* a, FileState* const* b) {
    FileState* dsA = *a;
    FileState* dsB = *b;
    // sort pinned documents before unpinned ones
    if (dsA->isPinned != dsB->isPinned) {
        return dsA->isPinned ? -1 : 1;
    }
    // sort pinned documents alphabetically
    if (dsA->isPinned) {
        return str::CmpNatural(path::GetBaseNameTemp(dsA->filePath), path::GetBaseNameTemp(dsB->filePath));
    }
    // use recency as the criterion in case of equal open counts
    return dsA->index < dsB->index ? -1 : 1;
}

void FileHistoryGetRecentlyOpenedOrder(Vec<FileState*>& list) {
    GetSortedStates(list, cmpRecentlyOpened);
}

// removes file history entries which shouldn't be saved anymore
// (see the loop below for the details)
void FileHistoryPurge(bool alwaysUseDefaultState) {
    // minOpenCount is set to the number of times a file must have been
    // opened to be kept (provided that there is no other valuable
    // information about the file to be remembered)
    int minOpenCount = 0;
    if (alwaysUseDefaultState) {
        Vec<FileState*> frequencyList;
        FileHistoryGetFrequencyOrder(frequencyList);
        if (len(frequencyList) > kFileHistoryMaxFrequent) {
            auto* el = frequencyList[kFileHistoryMaxFrequent];
            minOpenCount = el->openCount / 2;
        }
    }

    for (int j = len(*gStates); j > 0; j--) {
        FileState* state = (*gStates)[j - 1];
        // never forget pinned documents, documents we've remembered a password for and
        // documents for which there are favorites
        if (state->isPinned || len(state->decryptionKey) > 0 || len(*state->favorites) > 0) {
            continue;
        }
        // NOLINTNEXTLINE(bugprone-branch-clone): each branch documents a different reason to forget
        if (state->isMissing && (alwaysUseDefaultState || state->useDefaultState)) {
            // forget about missing documents without valuable state
            VecRemoveAt(*gStates, j - 1);
        } else if (j > kFileHistoryMaxFiles) {
            // forget about files last opened longer ago than the last FILE_HISTORY_MAX_FILES ones
            VecRemoveAt(*gStates, j - 1);
        } else if (alwaysUseDefaultState && state->openCount < minOpenCount && j > kFileHistoryMaxRecent) {
            // forget about files that were hardly used (and without valuable state)
            VecRemoveAt(*gStates, j - 1);
        } else {
            continue;
        }
        // SaveSettings() purges on every document load / tab close, so this
        // can run while the home page is up and pointing at `state`
        HomePageInvalidateLayoutCache();
        DeleteFileState(state);
    }
}

// list of recently closed documents, most recent at the end
static StrVec gClosedDocuments;

int RecentlyCloseDocumentsCount() {
    return len(gClosedDocuments);
}

void RememberRecentlyClosedDocument(Str path) {
    if (str::IsEmptyOrWhiteSpace(path)) {
        return;
    }
    gClosedDocuments.Append(path);
}

Str PopRecentlyClosedDocument() {
    int n = len(gClosedDocuments);
    if (n > 0) {
        return Str(gClosedDocuments.RemoveAtFast(n - 1));
    }
    return {};
}

// --- thumbnail cache delete

// Delete cached thumbnails for file-history entries marked missing (issue #4286).
// The old "delete any thumb not in the frequent list" logic was disabled after
// a StrVec rewrite: missing files in GetFrequencyOrder and Remove() behavior
// made it too aggressive. Only purge thumbs for states we already know are gone.
void CleanUpThumbnailCache() {
    if (!gStates) {
        return;
    }
    for (FileState* fs : *gStates) {
        if (!fs || !fs->isMissing || len(fs->filePath) == 0) {
            continue;
        }
        // Keep pinned entries' thumbs; they still show on the home page.
        if (fs->isPinned) {
            continue;
        }
        logf("CleanUpThumbnailCache: deleting thumb for missing '%s'\n", fs->filePath);
        DeleteThumbnailForFile(fs->filePath);
        FreePixmap(fs->thumbnail);
        fs->thumbnail = nullptr;
    }
}

// --- file existence check

extern void MaybeRedrawHomePage();

// document path is either a file or a directory
// (when browsing images inside directory).
bool DocumentPathExists(Str path) {
    if (file::Exists(path) || dir::Exists(path)) {
        return true;
    }
    Str pos = str::SliceFromCharLast(Str(path.s + 2, path.len - 2), ':');
    if (len(pos) == 0) {
        return false;
    }
    // remove information needed for pointing at embedded documents
    // (e.g. "C:\path\file.pdf:3:0") to check at least whether the
    // container document exists
    TempStr realPath = str::DupTemp(Str(path.s, (int)(pos.s - path.s)));
    return file::Exists(realPath);
}

struct CheckFilesExistData {
    CheckFilesExistData() = default;
    ~CheckFilesExistData() = default;

    StrVec toCheck;
    StrVec missing;
};

static void HideMissingFiles(CheckFilesExistData* d) {
    for (Str path : d->missing) {
        FileHistoryMarkFileInexistent(path, true);
    }
    // update the Frequently Read page in case it's been displayed already
    MaybeRedrawHomePage();
    delete d;
}

static void CheckFilesExistAsync(CheckFilesExistData* d) {
    StrVec& toCheck = d->toCheck;
    // filters all file paths on network drives, removable drives and
    // all paths which still exist from the list (remaining paths will
    // be marked as inexistent in the file history)
    int n = len(toCheck);
    for (int i = 0; i < n; i++) {
        Str path = toCheck[i];
        if (len(path) == 0) {
            continue;
        }
        // files on network / removable drives can be temporarily missing
        if (!path::IsOnFixedDrive(path)) {
            continue;
        }
        if (DocumentPathExists(path)) {
            continue;
        }
        d->missing.Append(path);
        logf("FileExistenceChecker: missing '%s' at %d\n", path, i + 1);
    }

    Func0 fn = MkFunc0<CheckFilesExistData>(HideMissingFiles, d);
    uitask::Post(fn, "HideMissingFiles");
}

static void GetFilePathsToCheck(StrVec& toCheck) {
    FileState* fs;
    for (int i = 0; i < 2 * kFileHistoryMaxRecent && (fs = FileHistoryGet(i)) != nullptr; i++) {
        if (!fs->isMissing) {
            toCheck.Append(fs->filePath);
        }
    }
    // add missing paths from the list of most frequently opened documents
    Vec<FileState*> frequencyList;
    FileHistoryGetFrequencyOrder(frequencyList);
    int iMax = std::min(2 * kFileHistoryMaxFrequent, len(frequencyList));
    for (int i = 0; i < iMax; i++) {
        fs = frequencyList[i];
        AppendIfNotExists(&toCheck, fs->filePath);
    }
}

void RemoveNonExistentFilesAsync() {
    auto* d = new CheckFilesExistData();
    GetFilePathsToCheck(d->toCheck);
    if (len(d->toCheck) == 0) {
        // nothing to check, so no CheckFilesExistAsync to hand ownership to
        delete d;
        return;
    }
    logf("RemoveNonExistentFilesAsync: starting CheckFilesExistAsync to check %d files\n", len(d->toCheck));
    Func0 fn = MkFunc0<CheckFilesExistData>(CheckFilesExistAsync, d);
    RunAsync(fn, StrL("CheckFilesExistAsync"));
}
