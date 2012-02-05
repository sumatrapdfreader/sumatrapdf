/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

#ifndef Fb2Doc_h
#define Fb2Doc_h

#include "BaseEbookDoc.h"

class Fb2Doc : public BaseEbookDoc {
public:
    static Fb2Doc *ParseFile(const TCHAR *fileName);
    static bool IsSupported(const TCHAR *fileName);
};

#endif
