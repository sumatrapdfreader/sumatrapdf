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

// TODO: move to a separate Favorites.[h|cpp] files

/*
A favorite is a bookmark (we call it a favorite, like Internet Explorer, to
differentiate from bookmarks inside a PDF file (which really are
table of contents)).

We can have multiple favorites per file.

Favorites are accurate to a page - it's simple and should be good enough
for the user.

A favorite is identified by a (mandatory) page number and (optional) name
(provided by the user).

Favorites do not remember presentation settings like zoom or viewing mode -
they are for navigation only. Presentation settings are remembered on a
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
        str::ReplacePtr(&this->name, name);
    }

    int     pageNo;
    TCHAR * name;
    int     menuId; // assigned in AppendFavMenuItems()
};

// list of favorites for one file
class FileFavs {

    int FindByPage(int pageNo) const
    {
        for (size_t i = 0; i < favNames.Count(); i++)
            if (favNames.At(i)->pageNo == pageNo)
                return (int)i;
        return -1;
    }

    static int sortByPageNo(const void *a, const void *b)
    {
        FavName *na = *(FavName **)a;
        FavName *nb = *(FavName **)b;
        // sort lower page numbers first
        return na->pageNo - nb->pageNo;
    }

public:
    TCHAR *         filePath;
    Vec<FavName *>  favNames;

    FileFavs(const TCHAR *fp) : filePath(str::Dup(fp)) { }

    ~FileFavs() {
        DeleteVecMembers(favNames);
        free(filePath);
    }

    bool IsEmpty() const
    {
        return favNames.Count() == 0;
    }

    void ResetMenuIds()
    {
        for (size_t i=0; i<favNames.Count(); i++)
        {
            FavName *fn = favNames.At(i);
            fn->menuId = 0;
        }
    }

    bool GetByMenuId(int menuId, size_t& idx)
    {
        for (size_t i=0; i<favNames.Count(); i++)
        {
            FavName *fn = favNames.At(i);
            if (fn->menuId == menuId) {
                idx = i;
                return true;
            }
        }
        return false;
    }

    bool HasFavName(FavName *fn)
    {
        for (size_t i=0; i<favNames.Count(); i++)
        {
            if (fn == favNames.At(i))
                return true;
        }
        return false;

    }

    bool Remove(int pageNo)
    {
        int idx = FindByPage(pageNo);
        if (-1 == idx)
            return false;

        delete favNames.At(idx);
        favNames.RemoveAt(idx);
        return true;
    }

    bool Exists(int pageNo) const
    {
        int idx = FindByPage(pageNo);
        return idx != -1;
    }

    void AddOrReplace(int pageNo, const TCHAR *name)
    {
        int idx = FindByPage(pageNo);
        if (idx != -1) {
            favNames.At(idx)->SetName(name);
            return;
        }
        FavName *fn = new FavName(pageNo, name);
        favNames.Append(fn);
        favNames.Sort(sortByPageNo);
    }
};

class Favorites {

    // filePathCache points to a string inside FileFavs, so doesn't need to free()d
    TCHAR *  filePathCache;
    size_t   idxCache;

    void RemoveFav(FileFavs *fav, size_t idx)
    {
        favs.RemoveAt(idx);
        delete fav;
        filePathCache = NULL;
        idxCache = (size_t)-1;
    }

public:
    Vec<FileFavs*> favs;

    Favorites() : filePathCache(NULL), idxCache((size_t)-1) { }
    ~Favorites() { DeleteVecMembers(favs); }

    FileFavs *GetByMenuId(int menuId, size_t& idx)
    {
        for (size_t i=0; i<favs.Count(); i++)
        {
            FileFavs *fav = favs.At(i);
            if (fav->GetByMenuId(menuId, idx))
                return fav;
        }
        return NULL;
    }

    FileFavs *GetByFavName(FavName *fn)
    {
        for (size_t i=0; i<favs.Count(); i++)
        {
            FileFavs *fav = favs.At(i);
            if (fav->HasFavName(fn))
                return fav;
        }
        return NULL;
    }

    size_t Count() const {
        return favs.Count();
    }

    void ResetMenuIds()
    {
        for (size_t i=0; i<favs.Count(); i++)
        {
            FileFavs *fav = favs.At(i);
            fav->ResetMenuIds();
        }
    }

    FileFavs *GetFavByFilePath(const TCHAR *filePath, bool createIfNotExist=false, size_t *idx=NULL)
    {
        // it's likely that we'll ask about the info for the same
        // file as in previous call, so use one element cache
        FileFavs *fav = NULL;
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
            fav = new FileFavs(filePath);
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
        FileFavs *fav = GetFavByFilePath(filePath);
        if (!fav)
            return false;
        return fav->Exists(pageNo);
    }

    void AddOrReplace(const TCHAR *filePath, int pageNo, const TCHAR *name)
    {
        FileFavs *fav = GetFavByFilePath(filePath, true);
        fav->AddOrReplace(pageNo, name);
    }

    void Remove(const TCHAR *filePath, int pageNo)
    {
        size_t idx;
        FileFavs *fav = GetFavByFilePath(filePath, false, &idx);
        if (!fav)
            return;
        fav->Remove(pageNo);
        if (fav->IsEmpty())
            RemoveFav(fav, idx);
    }

    void RemoveAllForFile(const TCHAR *filePath)
    {
        size_t idx;
        FileFavs *fav = GetFavByFilePath(filePath, false, &idx);
        if (fav)
            RemoveFav(fav, idx);
    }
};

#endif
