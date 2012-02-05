/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Fb2Doc_h
#define Fb2Doc_h

#include "BaseUtil.h"
#include "Vec.h"
#include "Scoped.h"

class Fb2Doc {
    ScopedMem<TCHAR> fileName;
    str::Str<char> htmlData;

    bool Load();

public:
    Fb2Doc(const TCHAR *fileName);

    // the result is owned by Fb2Doc
    char *GetBookHtmlData(size_t& lenOut);

    static Fb2Doc *ParseFile(const TCHAR *fileName);
};

#endif
