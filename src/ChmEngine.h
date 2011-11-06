/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmEngine_h
#define ChmEngine_h

#include "BaseEngine.h"

class ChmNavigationCallback {
public:
    // tell the ui to show the pageNo as current page (which also syncs
    // the toc with the curent page). Needed for chm ui where navigation
    // can be initiated from inside html control
    virtual void PageNoChanged(int pageNo) = 0;
    // tell the UI to launch the given URL in an external web browser
    virtual void LaunchBrowser(const TCHAR *url) = 0;
};

class ChmEngine : public BaseEngine {
public:
    virtual void SetParentHwnd(HWND hwnd) = 0;
    virtual void DisplayPage(int pageNo) = 0;
    virtual void SetNavigationCalback(ChmNavigationCallback *cb) = 0;
    virtual RenderedBitmap *CreateThumbnail(SizeI size) = 0;

    virtual void PrintCurrentPage() = 0;
    virtual void FindInCurrentPage() = 0;
    virtual bool CanNavigate(int dir) = 0;
    virtual void Navigate(int dir) = 0;
    virtual void ZoomTo(float zoomLevel) = 0;
    virtual int CurrentPageNo() = 0;

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ChmEngine *CreateFromFileName(const TCHAR *fileName);
};

#endif
