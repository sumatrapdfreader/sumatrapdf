/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmEngine_h
#define ChmEngine_h

#include "BaseEngine.h"

class ChmToCItem : public DocToCItem {
public:
    TCHAR *url;
    TCHAR *imageNumber;

    // takes ownership of url and imageNumber
    ChmToCItem(TCHAR *title, TCHAR *url, TCHAR *imageNumber) :
        DocToCItem(title), url(url), imageNumber(imageNumber)
    {
    }

    virtual ~ChmToCItem() {
        free(url);
        free(imageNumber);
    }

    virtual PageDestination *GetLink() { return NULL; }
};

class ChmEngine : public BaseEngine {
public:
    virtual void HookToHwndAndDisplayIndex(HWND hwnd) = 0;
    virtual void DisplayPage(int pageNo) = 0;
    virtual void DisplayPageByUrl(const TCHAR *url) = 0;

public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ChmEngine *CreateFromFileName(const TCHAR *fileName);
};

#endif
