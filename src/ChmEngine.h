/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmEngine_h
#define ChmEngine_h

#include "BaseEngine.h"

class ChmNavigationCallback {
public:
    virtual ~ChmNavigationCallback() { }
    // tell the UI to show the pageNo as current page (which also syncs
    // the toc with the curent page). Needed for chm ui where navigation
    // can be initiated from inside html control
    virtual void PageNoChanged(int pageNo) = 0;
    // tell the UI to launch the given URL in an external web browser
    virtual void LaunchBrowser(const WCHAR *url) = 0;
    // tell the UI to move focus back to the main window
    // (if always == false, then focus is only moved if it's inside
    // an HtmlWindow and thus outside the reach of the main UI)
    virtual void FocusFrame(bool always) = 0;
    // tell the UI to let the user save the provided data to a file
    virtual void SaveDownload(const WCHAR *url, const unsigned char *data, size_t len) = 0;
};

class ChmEngine {
public:
    virtual ~ChmEngine() { }
    virtual ChmEngine *Clone() = 0;

    virtual const WCHAR *FileName() const = 0;
    virtual int PageCount() const = 0;

    virtual WCHAR *GetProperty(DocumentProperty prop) = 0;
    virtual const WCHAR *GetDefaultFileExt() const = 0;

    virtual PageDestination *GetNamedDest(const WCHAR *name) = 0;
    virtual bool HasTocTree() const = 0;
    virtual DocTocItem *GetTocTree() = 0;

    virtual bool SetParentHwnd(HWND hwnd) = 0;
    virtual void RemoveParentHwnd() = 0;
    virtual void DisplayPage(int pageNo) = 0;
    virtual void SetNavigationCalback(ChmNavigationCallback *cb) = 0;

    virtual void GoToDestination(PageDestination *link) = 0;
    virtual RenderedBitmap *TakeScreenshot(RectI area, SizeI targetSize) = 0;

    virtual void PrintCurrentPage(bool showUI) = 0;
    virtual void FindInCurrentPage() = 0;
    virtual bool CanNavigate(int dir) = 0;
    virtual void Navigate(int dir) = 0;
    virtual void ZoomTo(float zoomLevel) = 0;
    virtual float GetZoom() = 0;
    virtual void SelectAll() = 0;
    virtual void CopySelection() = 0;
    virtual int CurrentPageNo() const = 0;
    virtual LRESULT PassUIMsg(UINT msg, WPARAM wParam, LPARAM lParam) = 0;

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static ChmEngine *CreateFromFile(const WCHAR *fileName);
};

#endif
