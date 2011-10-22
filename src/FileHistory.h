/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FileHistory_h
#define FileHistory_h

#include "DisplayState.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Vec.h"

/* Handling of file history list.

   We keep an infinite list of all (still existing in the file system)
   files that a user has ever opened. For each file we also keep a bunch of
   attributes describing the display state at the time the file was closed.

   We persist this list inside preferences file to something looking like this:

File History:
{
   File: C:\path\to\file.pdf
   Display Mode: single page
   Page: 1
   ZoomVirtual: 123.4567
   Window State: 2
   ...
}
etc...

    We deserialize this info at startup and serialize when the application
    quits.
*/

// number of most recently used files that will be shown in the menu
// (and remembered in the preferences file, if just filenames are
//  to be remembered and not individual view settings per document)
#define FILE_HISTORY_MAX_RECENT     10

// maximum number of most frequently used files that will be shown on the
// Frequent Read list (space permitting)
#define FILE_HISTORY_MAX_FREQUENT   10

class FileHistory {
    Vec<DisplayState *> states;

    // sorts the most often used files first
    static int cmpOpenCount(const void *a, const void *b) {
        DisplayState *dsA = *(DisplayState **)a;
        DisplayState *dsB = *(DisplayState **)b;
        // sort pinned documents before unpinned ones
        if (dsA->isPinned != dsB->isPinned)
            return dsA->isPinned ? -1 : 1;
        // sort pinned documents alphabetically
        if (dsA->isPinned && dsB->isPinned)
            return str::CmpNatural(path::GetBaseName(dsA->filePath), path::GetBaseName(dsB->filePath));
        if (dsA->openCount != dsB->openCount)
            return dsB->openCount - dsA->openCount;
        // use recency as the criterion in case of equal open counts
        return dsA->index < dsB->index ? -1 : 1;
    }

public:
    FileHistory() { }
    ~FileHistory() { Clear(); }
    void  Clear() { DeleteVecMembers(states); }

    void  Prepend(DisplayState *state) { states.InsertAt(0, state); }
    void  Append(DisplayState *state) { states.Append(state); }
    void  Remove(DisplayState *state) { states.Remove(state); }
    bool  IsEmpty() const { return states.Count() == 0; }

    DisplayState *Get(size_t index) const {
        if (index < states.Count())
            return states.At(index);
        return NULL;
    }

    DisplayState *Find(const TCHAR *filePath) const {
        for (size_t i = 0; i < states.Count(); i++)
            if (str::EqI(states.At(i)->filePath, filePath))
                return states.At(i);
        return NULL;
    }

    void MarkFileLoaded(const TCHAR *filePath) {
        assert(filePath);
        // if a history entry with the same name already exists,
        // then reuse it. That way we don't have duplicates and
        // the file moves to the front of the list
        DisplayState *state = Find(filePath);
        if (!state) {
            state = new DisplayState();
            state->filePath = str::Dup(filePath);
        }
        else
            Remove(state);
        Prepend(state);
        state->openCount++;
    }

    bool MarkFileInexistent(const TCHAR *filePath) {
        assert(filePath);
        DisplayState *state = Find(filePath);
        if (!state)
            return false;
        // move the file history entry to the end of the list
        // of recently opened documents (if it exists at all),
        // so that the user could still try opening it again
        // and so that we don't completely forget the settings,
        // should the file reappear later on
        int ix = states.Find(state);
        if (ix < FILE_HISTORY_MAX_RECENT) {
            states.Remove(state);
            if (states.Count() < FILE_HISTORY_MAX_RECENT)
                states.Append(state);
            else
                states.InsertAt(FILE_HISTORY_MAX_RECENT - 1, state);
        }
        // also delete the thumbnail and move the link towards the
        // back in the Frequently Read list
        delete state->thumbnail;
        state->thumbnail = NULL;
        state->openCount >>= 2;
        return true;
    }

    // appends history to this one, leaving the other history emptied
    void ExtendWith(FileHistory& other) {
        while (!other.IsEmpty()) {
            DisplayState *state = other.Get(0);
            Append(state);
            other.Remove(state);
        }
    }

    // returns a shallow copy of the file history list, sorted
    // by open count (which has a pre-multiplied recency factor)
    // caller needs to delete the result (but not the contained states)
    Vec<DisplayState *> *GetFrequencyOrder() {
        Vec<DisplayState *> *list = new Vec<DisplayState *>(states);
        for (size_t i = 0; i < list->Count(); i++)
            list->At(i)->index = i;
        list->Sort(cmpOpenCount);
        return list;
    }
};

#endif
