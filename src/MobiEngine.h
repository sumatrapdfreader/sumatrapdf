/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef MobiEngine_h
#define MobiEngine_h

#include "BaseEngine.h"

class MobiEngine : public BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static MobiEngine *CreateFromFile(const TCHAR *fileName);
};

#endif
