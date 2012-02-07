/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef MobiDoc2_h
#define MobiDoc2_h

#include "BaseEbookDoc.h"

class MobiDoc2 : public BaseEbookDoc {
public:
    static MobiDoc2 *ParseFile(const TCHAR *fileName);
    static bool IsSupported(const TCHAR *fileName);
};

#endif
