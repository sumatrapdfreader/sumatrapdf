/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmEngine_h
#define ChmEngine_h

#include "BaseEngine.h"

class HtmlWindow;

class ChmEngine : public BaseEngine {
public:
    virtual void HookHwndAndDisplayIndex(HWND hwnd) = 0;
    virtual void DisplayPage(int pageNo) = 0;
    virtual void NavigateTo(PageDestination *dest) = 0;
    virtual HtmlWindow *GetHtmlWindow() const = 0;

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ChmEngine *CreateFromFileName(const TCHAR *fileName);
};

#endif
