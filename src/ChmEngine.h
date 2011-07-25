/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmEngine_h
#define ChmEngine_h

#include "BaseEngine.h"

class ChmEngine : public BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ChmEngine *CreateFromFileName(const TCHAR *fileName);
    void HookToHwndAndDisplayIndex(HWND hwnd);
};

#endif
