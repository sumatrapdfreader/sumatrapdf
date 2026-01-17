/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/DirIter.h"
#include "utils/FileUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "FileThumbnails.h"
#include "FileHistory.h"

#include "utils/Log.h"

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
constexpr size_t kFileHistoryMaxFiles = 1000;

FileHistory gFileHistory;

void FileHistory::Append(FileState* fs) const {
    ReportIf(!fs->filePath);
    states->Append(fs);
}

void FileHistory::Remove(FileState* fs) const {
    states->Remove(fs);
}

void FileHistory::UpdateStatesSource(Vec<FileState*>* states) {
    this->states = states;
}

void FileHistory::Clear(bool keepFavorites) const {
    if (!states) {
        return;
    }
    Vec<FileState*> keep;
    for (size_t i = 0; i < states->size(); i++) {
        if (keepFavorites && states->at(i)->favorites->size() > 0) {
            states->at(i)->openCount = 0;
            keep.Append(states->at(i));
        } else {
            DeleteFileState(states->at(i));
        }
    }
    *states = keep;
}

FileState* FileHistory::Get(size_t index) const {
    if (index < states->size()) {
        return states->at(index);
    }
    return nullptr;
}

FileState* FileHistory::FindByPath(const char* filePath) const {
    int idxExact = -1;
    int n = states->Size();
    for (int i = 0; i < n; i++) {
        FileState* fs = states->at(i);
        if (str::EqI(fs->filePath, filePath)) {
            idxExact = i;
        }
    }
    if (idxExact == -1) {
        return nullptr;
    }
    return states->at(idxExact);
}

// returns an exact match by path or match by just file name
// TODO: audit the uses of FindByName and maybe convert to FindByPath
FileState* FileHistory::FindByName(const char* filePath, size_t* idxOut) const {
    int idxExact = -1;
    int idxFileNameMatch = -1;
    TempStr fileName = path::GetBaseNameTemp(filePath);
    int n = states->Size();
    for (int i = 0; i < n; i++) {
        FileState* fs = states->at(i);
        if (str::EqI(fs->filePath, filePath)) {
            idxExact = i;
        } else if (str::EndsWithI(fs->filePath, fileName)) {
            idxFileNameMatch = i;
        }
    }
    int idFound = idxExact;
    if (idFound == -1) {
        idFound = idxFileNameMatch;
    }
    if (idFound == -1) {
        return nullptr;
    }
    if (idxOut) {
        *idxOut = (size_t)idFound;
    }
    return states->at(idFound);
}

FileState* FileHistory::MarkFileLoaded(const char* filePath) const {
    ReportIf(!filePath);
    // if a history entry with the same name already exists,
    // then reuse it. That way we don't have duplicates and
    // the file moves to the front of the list
    FileState* fs = FindByPath(filePath);
    if (!fs) {
        fs = NewFileState(filePath);
        fs->useDefaultState = true;
    } else {
        states->Remove(fs);
        fs->isMissing = false;
    }
    states->InsertAt(0, fs);
    fs->openCount++;
    return fs;
}

bool FileHistory::MarkFileInexistent(const char* filePath, bool hide) const {
    ReportIf(!filePath);
    FileState* state = FindByPath(filePath);
    if (!state) {
        return false;
    }
    // move the file history entry to the end of the list
    // of recently opened documents (if it exists at all),
    // so that the user could still try opening it again
    // and so that we don't completely forget the settings,
    // should the file reappear later on
    int newIdx = hide ? INT_MAX : kFileHistoryMaxRecent - 1;
    int idx = states->Find(state);
    if (idx < newIdx && state != states->Last()) {
        states->Remove(state);
        if (states->size() <= (size_t)newIdx) {
            states->Append(state);
        } else {
            states->InsertAt(newIdx, state);
        }
    }
    // also delete the thumbnail and move the link towards the
    // back in the Frequently Read list
    delete state->thumbnail;
    state->thumbnail = nullptr;
    state->openCount >>= 2;
    state->isMissing = hide;
    return true;
}

// sorts the most often used files first
static int cmpOpenCount(const void* a, const void* b) {
    FileState* dsA = *(FileState**)a;
    FileState* dsB = *(FileState**)b;
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

// returns a shallow copy of the file history list, sorted
// by open count (which has a pre-multiplied recency factor)
// and with all missing states filtered out
// caller needs to delete the result (but not the contained states)
void FileHistory::GetFrequencyOrder(Vec<FileState*>& list) const {
    ReportIf(list.size() > 0);
    size_t i = 0;
    for (FileState* ds : *states) {
        ds->index = i++;
        if (!ds->isMissing || ds->isPinned) {
            list.Append(ds);
        }
    }
    list.Sort(cmpOpenCount);
}

// sorts recently opened files first
static int cmpRecentlyOpened(const void* a, const void* b) {
    FileState* dsA = *(FileState**)a;
    FileState* dsB = *(FileState**)b;
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

void FileHistory::GetRecentlyOpenedOrder(Vec<FileState*>& list) const {
    ReportIf(list.size() > 0);
    size_t i = 0;
    for (FileState* ds : *states) {
        ds->index = i++;
        if (!ds->isMissing || ds->isPinned) {
            list.Append(ds);
        }
    }
    list.Sort(cmpRecentlyOpened);
}

// removes file history entries which shouldn't be saved anymore
// (see the loop below for the details)
void FileHistory::Purge(bool alwaysUseDefaultState) const {
    // minOpenCount is set to the number of times a file must have been
    // opened to be kept (provided that there is no other valuable
    // information about the file to be remembered)
    int minOpenCount = 0;
    if (alwaysUseDefaultState) {
        Vec<FileState*> frequencyList;
        GetFrequencyOrder(frequencyList);
        if (frequencyList.size() > kFileHistoryMaxFrequent) {
            auto el = frequencyList.at(kFileHistoryMaxFrequent);
            minOpenCount = el->openCount / 2;
        }
    }

    for (size_t j = states->size(); j > 0; j--) {
        FileState* state = states->at(j - 1);
        // never forget pinned documents, documents we've remembered a password for and
        // documents for which there are favorites
        if (state->isPinned || state->decryptionKey != nullptr || state->favorites->size() > 0) {
            continue;
        }
        if (state->isMissing && (alwaysUseDefaultState || state->useDefaultState)) {
            // forget about missing documents without valuable state
            states->RemoveAt(j - 1);
        } else if (j > kFileHistoryMaxFiles) {
            // forget about files last opened longer ago than the last FILE_HISTORY_MAX_FILES ones
            states->RemoveAt(j - 1);
        } else if (alwaysUseDefaultState && state->openCount < minOpenCount && j > kFileHistoryMaxRecent) {
            // forget about files that were hardly used (and without valuable state)
            states->RemoveAt(j - 1);
        } else {
            continue;
        }
        DeleteFileState(state);
    }
}

// list of recently closed documents, most recent at the end
StrVec gClosedDocuments;

int RecentlyCloseDocumentsCount() {
    return gClosedDocuments.Size();
}

void RememberRecentlyClosedDocument(const char* path) {
    if (str::IsEmptyOrWhiteSpace(path)) {
        return;
    }
    gClosedDocuments.Append(path);
}

char* PopRecentlyClosedDocument() {
    int n = gClosedDocuments.Size();
    if (n > 0) {
        return gClosedDocuments.RemoveAtFast(n - 1);
    }
    return nullptr;
}

// --- thumbnail cache delete

static bool shouldDeleteThumbnail = false;

// TODO: https://github.com/sumatrapdfreader/sumatrapdf/issues/4286
// Not sure why the behavior started changing after I re-wrote StrVec
// is the issue that files are marked as isMissing in FileExistenceCheckerThread?
// is it because we don't return enough itms if GetFrequencyOrder()? Is it a bug
// in StrVec::Remove()?
// either way, I just disabled deleting of stale thumbnail because it seems fishy
// Should probably change the logic to: remove thumbnails for files marked as missing

// removes thumbnails that don't belong to any frequently used item in file history
void CleanUpThumbnailCache() {
    const FileHistory& fileHistory = gFileHistory;
    TempStr thumbsDir = GetThumbnailCacheDirTemp();

    StrVec filePaths;
    DirIter di{thumbsDir};
    for (DirIterEntry* de : di) {
        if (path::Match(de->filePath, "*.png")) {
            filePaths.Append(de->filePath);
        }
    }
    if (filePaths.IsEmpty()) {
        return;
    }

    bool ok;
    // remove files that should not be deleted
    Vec<FileState*> list;
    fileHistory.GetFrequencyOrder(list);
    int n = 0;
    for (auto& fs : list) {
        if (n++ > kFileHistoryMaxFrequent * 2) {
            break;
        }
        TempStr path = GetThumbnailPathTemp(fs->filePath);
        if (!path) {
            continue;
        }
        ok = filePaths.Remove(path);
        if (!ok) {
            logf("CleanUpThumbnailCache: failed to remove '%s'\n", path);
        }
    }

    for (char* path : filePaths) {
        if (shouldDeleteThumbnail) {
            logf("CleanUpThumbnailCache: deleting '%s'\n", path);
            file::Delete(path);
        }
    }
}

// --- file existence check

struct FileExistenceData {
    FileExistenceData() = default;
    ~FileExistenceData() = default;

    StrVec toCheck;
    StrVec missing;
};

extern void MaybeRedrawHomePage();

// document path is either a file or a directory
// (when browsing images inside directory).
bool DocumentPathExists(const char* path) {
    if (file::Exists(path) || dir::Exists(path)) {
        return true;
    }
    auto pos = str::FindCharLast(path + 2, ':');
    if (!pos) {
        return false;
    }
    // remove information needed for pointing at embedded documents
    // (e.g. "C:\path\file.pdf:3:0") to check at least whether the
    // container document exists
    TempStr realPath = str::DupTemp(path, pos - path);
    return file::Exists(realPath);
}

static void HideMissingFiles(FileExistenceData* d) {
    for (const char* path : d->missing) {
        gFileHistory.MarkFileInexistent(path, true);
    }
    // update the Frequently Read page in case it's been displayed already
    MaybeRedrawHomePage();
    delete d;
}

static void FileExistenceCheckerAsync(FileExistenceData* d) {
    StrVec& toCheck = d->toCheck;
    // filters all file paths on network drives, removable drives and
    // all paths which still exist from the list (remaining paths will
    // be marked as inexistent in gFileHistory)
    int n = toCheck.Size();
    for (int i = 0; i < n; i++) {
        const char* path = toCheck[i];
        if (!path) {
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

    Func0 fn = MkFunc0<FileExistenceData>(HideMissingFiles, d);
    uitask::Post(fn, "TaskHideMissingFiles");
}

static void GetFilePathsToCheck(StrVec& toCheck) {
    FileState* fs;
    for (size_t i = 0; i < 2 * kFileHistoryMaxRecent && (fs = gFileHistory.Get(i)) != nullptr; i++) {
        if (!fs->isMissing) {
            char* fp = fs->filePath;
            toCheck.Append(fp);
        }
    }
    // add missing paths from the list of most frequently opened documents
    Vec<FileState*> frequencyList;
    gFileHistory.GetFrequencyOrder(frequencyList);
    size_t iMax = std::min<size_t>(2 * kFileHistoryMaxFrequent, frequencyList.size());
    for (size_t i = 0; i < iMax; i++) {
        fs = frequencyList.at(i);
        char* fp = fs->filePath;
        AppendIfNotExists(&toCheck, fp);
    }
}

void RemoveNonExistentFilesAsync() {
    auto d = new FileExistenceData();
    GetFilePathsToCheck(d->toCheck);
    if (d->toCheck.Size() == 0) {
        return;
    }
    logf("RemoveNonExistentFilesAsync: starting FileExistenceCheckerThread to check %d files\n", d->toCheck.Size());
    Func0 fn = MkFunc0<FileExistenceData>(FileExistenceCheckerAsync, d);
    RunAsync(fn, "FileExistenceThread");
}
