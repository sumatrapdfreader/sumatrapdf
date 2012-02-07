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
        mobi = MobiDoc::ParseFile(fileName);
        if (!mobi)
            return false;
        for (size_t i = 0; i < mobi->imagesCount; i++) {
            ImageData2 data = { 0 };
            data.data = (char *)mobi->images[i].imgData;
            data.len = mobi->images[i].imgDataLen;
            data.id = NULL; // recindex
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

    virtual ImageData2 *GetImageData(size_t index) {
        if (index > images.Count())
            return NULL;
        if (!images.At(index).data)
            return NULL;
        return &images.At(index);
    }

    virtual ImageData2 *GetImageData(const char *id) {
        int idx = atoi(id);
        return GetImageData(idx);
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
    // TODO: also .pdb?
    return str::EndsWithI(fileName, _T(".mobi")) ||
           str::EndsWithI(fileName, _T(".azw"))  ||
           str::EndsWithI(fileName, _T(".azw1")) ||
           str::EndsWithI(fileName, _T(".prc"));
}
