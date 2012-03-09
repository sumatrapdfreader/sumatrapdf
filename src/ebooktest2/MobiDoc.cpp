/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// reuse MobiDoc as far and as long as possible
#include "../MobiDoc.cpp"

#include "MobiDoc.h"
#include "Scoped.h"

class MobiDoc2Impl : public MobiDoc2 {
    friend MobiDoc2;

    ScopedMem<TCHAR>    fileName;
    MobiDoc *           mobi;

    Vec<ImageData2>     images;

    bool Load() {
        mobi = MobiDoc::CreateFromFile(fileName);
        if (!mobi)
            return false;
        ImageData *cover = mobi->GetCoverImage();
        if (cover) {
            ImageData2 data = { 0 };
            data.data = cover->data;
            data.len = cover->len;
            data.idx = (size_t)-1; // invalid index for other formats
            images.Append(data);
        }
        for (size_t i = 1; i < mobi->imagesCount; i++) {
            ImageData *mobiImg = mobi->GetImage(i);
            if (!mobiImg || !mobiImg->data)
                continue;
            ImageData2 data = { 0 };
            data.data = mobiImg->data;
            data.len = mobiImg->len;
            data.idx = i; // recindex
            images.Append(data);
        }
        return true;
    }

public:
    MobiDoc2Impl(const TCHAR *fileName) : fileName(str::Dup(fileName)) { }
    virtual ~MobiDoc2Impl() {
        delete mobi;
    }

    virtual const TCHAR *GetFilepath() {
        return fileName;
    }

    virtual const char *GetBookHtmlData(size_t& lenOut) {
        lenOut = mobi->doc->Size();
        return mobi->doc->Get();
    }

    virtual ImageData2 *GetImageData(const char *id) {
        int idx = atoi(id);
        for (size_t i = 0; i < images.Count(); i++) {
            if (images.At(i).idx == idx)
                return &images.At(i);
        }
        return NULL;
    }

    virtual ImageData2 *GetImageData(size_t index) {
        if ((size_t)-1 == index && images.Count() > 0 && images.At(0).idx == (size_t)-1)
            index = 0; // cover image
        if (images.Count() >= index)
            return NULL;
        return &images.At(index);
    }
};

MobiDoc2 *MobiDoc2::ParseFile(const TCHAR *fileName)
{
    MobiDoc2Impl *mb = new MobiDoc2Impl(fileName);
    if (!mb || !mb->Load()) {
        delete mb;
        mb = NULL;
    }
    return mb;
}

bool MobiDoc2::IsSupported(const TCHAR *fileName)
{
    return str::EndsWithI(fileName, _T(".mobi")) ||
           str::EndsWithI(fileName, _T(".azw"))  ||
           str::EndsWithI(fileName, _T(".prc"));
}
