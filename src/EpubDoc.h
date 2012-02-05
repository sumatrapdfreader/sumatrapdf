/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef EpubDoc_h
#define EpubDoc_h

#include "BaseUtil.h"
#include "Vec.h"
#include "Scoped.h"
#include "ZipUtil.h"

class EpubDoc {
    ScopedMem<TCHAR> fileName;
    ZipFile zip;
    str::Str<char> htmlData;

    bool Load();

public:
    EpubDoc(const TCHAR *fileName);

    // the result is owned by EpubDoc
    char *GetBookHtmlData(size_t& lenOut);

    static EpubDoc *ParseFile(const TCHAR *fileName);
};

#endif
