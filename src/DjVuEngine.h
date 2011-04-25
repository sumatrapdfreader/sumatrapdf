/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DjVuEngine_h
#define DjVuEngine_h

#include "BaseEngine.h"

class DjVuEngine : public BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName) {
        return Str::EndsWithI(fileName, _T(".djvu"));
    }
    static DjVuEngine *CreateFromFileName(const TCHAR *fileName);
};

#endif
