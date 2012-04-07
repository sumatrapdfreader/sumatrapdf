/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DjVuEngine_h
#define DjVuEngine_h

#include "BaseEngine.h"

class DjVuEngine : public BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static DjVuEngine *CreateFromFile(const TCHAR *fileName);
};

#endif
