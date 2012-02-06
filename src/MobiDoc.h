/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiDoc_h
#define MobiDoc_h

#include "BaseEbookDoc.h"

class MobiDoc : public BaseEbookDoc {
public:
    static MobiDoc *ParseFile(const TCHAR *fileName);
    static bool IsSupported(const TCHAR *fileName);
};

#endif
