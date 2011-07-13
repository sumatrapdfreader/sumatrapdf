/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "Favorites.h"

int FileFavs::FindByPage(int pageNo) const
{
    for (size_t i = 0; i < favNames.Count(); i++)
        if (favNames.At(i)->pageNo == pageNo)
            return (int)i;
    return -1;
}

int FileFavs::SortByPageNo(const void *a, const void *b)
{
    FavName *na = *(FavName **)a;
    FavName *nb = *(FavName **)b;
    // sort lower page numbers first
    return na->pageNo - nb->pageNo;
}

void FileFavs::ResetMenuIds()
{
    for (size_t i=0; i<favNames.Count(); i++)
    {
        FavName *fn = favNames.At(i);
        fn->menuId = 0;
    }
}

bool FileFavs::GetByMenuId(int menuId, size_t& idx)
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

bool FileFavs::HasFavName(FavName *fn)
{
    for (size_t i=0; i<favNames.Count(); i++)
    {
        if (fn == favNames.At(i))
            return true;
    }
    return false;

}

bool FileFavs::Remove(int pageNo)
{
    int idx = FindByPage(pageNo);
    if (-1 == idx)
        return false;

    delete favNames.At(idx);
    favNames.RemoveAt(idx);
    return true;
}

void FileFavs::AddOrReplace(int pageNo, const TCHAR *name)
{
    int idx = FindByPage(pageNo);
    if (idx != -1) {
        favNames.At(idx)->SetName(name);
        return;
    }
    FavName *fn = new FavName(pageNo, name);
    favNames.Append(fn);
    favNames.Sort(SortByPageNo);
}

void Favorites::RemoveFav(FileFavs *fav, size_t idx)
{
    favs.RemoveAt(idx);
    delete fav;
    filePathCache = NULL;
    idxCache = (size_t)-1;
}

FileFavs *Favorites::GetByMenuId(int menuId, size_t& idx)
{
    for (size_t i=0; i<favs.Count(); i++)
    {
        FileFavs *fav = favs.At(i);
        if (fav->GetByMenuId(menuId, idx))
            return fav;
    }
    return NULL;
}

FileFavs *Favorites::GetByFavName(FavName *fn)
{
    for (size_t i=0; i<favs.Count(); i++)
    {
        FileFavs *fav = favs.At(i);
        if (fav->HasFavName(fn))
            return fav;
    }
    return NULL;
}

void Favorites::ResetMenuIds()
{
    for (size_t i=0; i<favs.Count(); i++)
    {
        FileFavs *fav = favs.At(i);
        fav->ResetMenuIds();
    }
}

FileFavs *Favorites::GetFavByFilePath(const TCHAR *filePath, bool createIfNotExist, size_t *idx)
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

bool Favorites::IsPageInFavorites(const TCHAR *filePath, int pageNo) 
{
    FileFavs *fav = GetFavByFilePath(filePath);
    if (!fav)
        return false;
    return fav->Exists(pageNo);
}

void Favorites::AddOrReplace(const TCHAR *filePath, int pageNo, const TCHAR *name)
{
    FileFavs *fav = GetFavByFilePath(filePath, true);
    fav->AddOrReplace(pageNo, name);
}

void Favorites::Remove(const TCHAR *filePath, int pageNo)
{
    size_t idx;
    FileFavs *fav = GetFavByFilePath(filePath, false, &idx);
    if (!fav)
        return;
    fav->Remove(pageNo);
    if (fav->IsEmpty())
        RemoveFav(fav, idx);
}

void Favorites::RemoveAllForFile(const TCHAR *filePath)
{
    size_t idx;
    FileFavs *fav = GetFavByFilePath(filePath, false, &idx);
    if (fav)
        RemoveFav(fav, idx);
}

