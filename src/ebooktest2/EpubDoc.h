/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

#ifndef EpubDoc_h
#define EpubDoc_h

#include "BaseEbookDoc.h"

class EpubDoc : public BaseEbookDoc {
public:
    static EpubDoc *ParseFile(const TCHAR *fileName);
    static bool IsSupported(const TCHAR *fileName);
};

#endif
