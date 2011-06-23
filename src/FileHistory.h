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
        return dsA->_index < dsB->_index ? -1 : 1;
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
            return states[index];
        return NULL;
    }

    DisplayState *Find(const TCHAR *filePath) const {
        for (size_t i = 0; i < states.Count(); i++)
            if (str::EqI(states[i]->filePath, filePath))
                return states[i];
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
            list->At(i)->_index = i;
        list->Sort(cmpOpenCount);
        return list;
    }
};

/*
A favorite is a bookmark (we call it a favorite, like Internet Explorer, because
we have to differentiate from bookmarks inside a PDF file (which really are
table of content)).

We can have multiple favorites per file.

Favorites are accurate to a page - it's simple and should be good enough
for the user.

A favorite is identified by a (mandatory) page number and (optional) name
(provided by the user).

Favorites do not remember presentation settings like zoom or viewing mode. 
Favorites are for navigation only. Presentation settings are remembered on a
per-file basis in FileHistory.
*/

class FavName {
public:
    FavName(int pageNo, const TCHAR *name) : pageNo(pageNo), name(NULL)
    {
        SetName(name);
    }

    ~FavName()
    {
        free(name);
    }

    void SetName(const TCHAR *name)
    {
        free(this->name);
        this->name = (name == NULL) ? NULL : str::Dup(name);
    }

    int     pageNo;
    TCHAR * name;
};

class Fav {

    Vec<FavName *>  favNames;

    FavName *FindByPage(int pageNo, size_t *idx=NULL) const
    {
        for (size_t i=0; i<favNames.Count(); i++)
        {
            FavName *fn = favNames.At(i);
            if (fn->pageNo == pageNo)
            {
                if (idx)
                    *idx = i;
                return fn;
            }
        }
        return NULL;
    }

public:
    TCHAR *         filePath;

    Fav(const TCHAR *fp)
    {
        filePath = str::Dup(fp);
    }

    ~Fav() {
        DeleteVecMembers(favNames);
        free(filePath);
    }

    size_t Count() const
    {
        return favNames.Count();
    }

    bool Remove(int pageNo)
    {
        size_t idx;
        FavName *fn = FindByPage(pageNo, &idx);
        if (fn)
        {
            delete fn;
            favNames.RemoveAt(idx);
            return true;
        }
        return false;
    }

    bool Exists(int pageNo) const
    {
        FavName *fn = FindByPage(pageNo);
        return fn != NULL;
    }

    void AddOrReplace(int pageNo, const TCHAR *name)
    {
        FavName *fn = FindByPage(pageNo);
        if (fn) {
            fn->SetName(name);
            return;
        }
        fn = new FavName(pageNo, name);
        favNames.Append(fn);
    }
};

class Favorites {
    Vec<Fav*> favs;

    // filePathCache points to a string inside Fav, so doesn't need to free()d
    TCHAR *  filePathCache;
    size_t   idxCache;

public:
    Favorites() : filePathCache(NULL)
    {}
    ~Favorites() { DeleteVecMembers(favs); }

    Fav *GetFavByFilePath(const TCHAR *filePath, bool createIfNotExist=false, size_t *idx=NULL)
    {
        // it's likely that we'll ask about the info for the same
        // file as in previous call, so use one element cache
        Fav *fav = NULL;
        bool found = false;
        if (str::Eq(filePath, filePathCache)) {
            fav = favs.At(idxCache);
            found = true;
        } else {
            for (size_t i=0; i<favs.Count(); i++)
            {
                fav = favs.At(i);
                if (str::Eq(filePath, fav->filePath))
                {
                    idxCache = i;
                    filePathCache = fav->filePath;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            if (!createIfNotExist)
                return NULL;
            fav = new Fav(filePath);
            favs.Append(fav);
            filePathCache = fav->filePath;
            idxCache = favs.Count() - 1;
        }
        if (idx)
            *idx = idxCache;
        assert(fav != NULL);
        return fav;
    }

    bool IsPageInFavorites(const TCHAR *filePath, int pageNo) 
    {
        Fav *fav = GetFavByFilePath(filePath);
        if (!fav)
            return false;
        return fav->Exists(pageNo);
    }

    void AddOrReplace(const TCHAR *filePath, int pageNo, const TCHAR *name)
    {
        Fav *fav = GetFavByFilePath(filePath, true);
        assert(fav);
        fav->AddOrReplace(pageNo, name);
    }

    void Remove(const TCHAR *filePath, int pageNo)
    {
        size_t idx;
        Fav *fav = GetFavByFilePath(filePath, false, &idx);
        if (!fav)
            return;
        fav->Remove(pageNo);
        if (0 == fav->Count())
            favs.RemoveAt(idx);
    }
};

#endif
