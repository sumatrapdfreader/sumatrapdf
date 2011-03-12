/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FileHistory_h
#define FileHistory_h

#include "DisplayState.h"
#include "TStrUtil.h"
#include "Vec.h"

/* Handling of file history list.

   We keep an infinite list of all (still existing in the file system) PDF
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

class FileHistoryList {
    Vec<DisplayState *> states;

public:
    FileHistoryList() { }
    ~FileHistoryList() { Clear(); }
    void            Clear(void) { DeleteVecMembers(states); }

    void            Prepend(DisplayState *state) { states.InsertAt(0, state); }
    void            Append(DisplayState *state) { states.Append(state); }
    void            Remove(DisplayState *state) { states.Remove(state); }
    bool            IsEmpty(void) const { return states.Count() == 0; }

    DisplayState *  Get(size_t index) const {
                        if (index < states.Count())
                            return states[index];
                        return NULL;
                    }
    DisplayState *  Find(const TCHAR *filePath) const {
                        for (size_t i = 0; i < states.Count(); i++)
                            if (tstr_ieq(states[i]->filePath, filePath))
                                return states[i];
                        return NULL;
                    }

    void            MarkFileLoaded(const TCHAR *filePath) {
                        assert(filePath);
                        // if a history entry with the same name already exists,
                        // then reuse it. That way we don't have duplicates and
                        // the file moves to the front of the list
                        DisplayState *state = Find(filePath);
                        if (!state) {
                            state = new DisplayState();
                            if (!state)
                                return;
                            state->filePath = StrCopy(filePath);
                        }
                        else
                            Remove(state);
                        Prepend(state);
                    }
    void            MarkFileInexistent(const TCHAR *filePath) {
                        assert(filePath);
                        // move the file history entry to the very end of the list
                        // (if it exists at all), so that we don't completely forget
                        // the settings, should the file reappear later on - but
                        // make space for other documents first
                        DisplayState *state = Find(filePath);
                        if (!state)
                            return;
                        Remove(state);
                        Append(state);
                    }
};

#endif
