/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "SettingsStructs.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"

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
#define FILE_HISTORY_MAX_FILES 1000

// sorts the most often used files first
static int cmpOpenCount(const void* a, const void* b) {
    DisplayState* dsA = *(DisplayState**)a;
    DisplayState* dsB = *(DisplayState**)b;
    // sort pinned documents before unpinned ones
    if (dsA->isPinned != dsB->isPinned)
        return dsA->isPinned ? -1 : 1;
    // sort pinned documents alphabetically
    if (dsA->isPinned)
        return str::CmpNatural(path::GetBaseNameNoFree(dsA->filePath), path::GetBaseNameNoFree(dsB->filePath));
    // sort often opened documents first
    if (dsA->openCount != dsB->openCount)
        return dsB->openCount - dsA->openCount;
    // use recency as the criterion in case of equal open counts
    return dsA->index < dsB->index ? -1 : 1;
}

void FileHistory::Clear(bool keepFavorites) {
    if (!states)
        return;
    Vec<DisplayState*> keep;
    for (size_t i = 0; i < states->size(); i++) {
        if (keepFavorites && states->at(i)->favorites->size() > 0) {
            states->at(i)->openCount = 0;
            keep.Append(states->at(i));
        } else {
            DeleteDisplayState(states->at(i));
        }
    }
    *states = keep;
}

DisplayState* FileHistory::Get(size_t index) const {
    if (index < states->size())
        return states->at(index);
    return nullptr;
}

DisplayState* FileHistory::Find(const WCHAR* filePath, size_t* idxOut) const {
    for (size_t i = 0; i < states->size(); i++) {
        if (str::EqI(states->at(i)->filePath, filePath)) {
            if (idxOut)
                *idxOut = i;
            return states->at(i);
        }
    }
    return nullptr;
}

DisplayState* FileHistory::MarkFileLoaded(const WCHAR* filePath) {
    CrashIf(!filePath);
    // if a history entry with the same name already exists,
    // then reuse it. That way we don't have duplicates and
    // the file moves to the front of the list
    DisplayState* state = Find(filePath, nullptr);
    if (!state) {
        state = NewDisplayState(filePath);
        state->useDefaultState = true;
    } else {
        states->Remove(state);
        state->isMissing = false;
    }
    states->InsertAt(0, state);
    state->openCount++;
    return state;
}

bool FileHistory::MarkFileInexistent(const WCHAR* filePath, bool hide) {
    CrashIf(!filePath);
    DisplayState* state = Find(filePath, nullptr);
    if (!state)
        return false;
    // move the file history entry to the end of the list
    // of recently opened documents (if it exists at all),
    // so that the user could still try opening it again
    // and so that we don't completely forget the settings,
    // should the file reappear later on
    int newIdx = hide ? INT_MAX : FILE_HISTORY_MAX_RECENT - 1;
    int idx = states->Find(state);
    if (idx < newIdx && state != states->Last()) {
        states->Remove(state);
        if (states->size() <= (size_t)newIdx)
            states->Append(state);
        else
            states->InsertAt(newIdx, state);
    }
    // also delete the thumbnail and move the link towards the
    // back in the Frequently Read list
    delete state->thumbnail;
    state->thumbnail = nullptr;
    state->openCount >>= 2;
    state->isMissing = hide;
    return true;
}

// returns a shallow copy of the file history list, sorted
// by open count (which has a pre-multiplied recency factor)
// and with all missing states filtered out
// caller needs to delete the result (but not the contained states)
void FileHistory::GetFrequencyOrder(Vec<DisplayState*>& list) const {
    CrashIf(list.size() > 0);
    size_t i = 0;
    for (DisplayState* ds : *states) {
        ds->index = i++;
        if (!ds->isMissing || ds->isPinned)
            list.Append(ds);
    }
    list.Sort(cmpOpenCount);
}

// removes file history entries which shouldn't be saved anymore
// (see the loop below for the details)
void FileHistory::Purge(bool alwaysUseDefaultState) {
    // minOpenCount is set to the number of times a file must have been
    // opened to be kept (provided that there is no other valuable
    // information about the file to be remembered)
    int minOpenCount = 0;
    if (alwaysUseDefaultState) {
        Vec<DisplayState*> frequencyList;
        GetFrequencyOrder(frequencyList);
        if (frequencyList.size() > FILE_HISTORY_MAX_RECENT)
            minOpenCount = frequencyList.at(FILE_HISTORY_MAX_FREQUENT)->openCount / 2;
    }

    for (size_t j = states->size(); j > 0; j--) {
        DisplayState* state = states->at(j - 1);
        // never forget pinned documents, documents we've remembered a password for and
        // documents for which there are favorites
        if (state->isPinned || state->decryptionKey != nullptr || state->favorites->size() > 0)
            continue;
        // forget about missing documents without valuable state
        if (state->isMissing && (alwaysUseDefaultState || state->useDefaultState))
            states->RemoveAt(j - 1);
        // forget about files last opened longer ago than the last FILE_HISTORY_MAX_FILES ones
        else if (j > FILE_HISTORY_MAX_FILES)
            states->RemoveAt(j - 1);
        // forget about files that were hardly used (and without valuable state)
        else if (alwaysUseDefaultState && state->openCount < minOpenCount && j > FILE_HISTORY_MAX_RECENT)
            states->RemoveAt(j - 1);
        else
            continue;
        DeleteDisplayState(state);
    }
}
