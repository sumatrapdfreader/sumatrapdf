/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */
#ifndef HtmlWindow_h
#define HtmlWindow_h

#include "BaseUtil.h"
#include "GeomUtil.h"
#include <exdisp.h>

class FrameSite;

class HtmlWindowCallback
{
public:
    virtual bool OnBeforeNavigate(const TCHAR *url, bool newWindow) = 0;
};

class HtmlWindow
{
protected:
    friend class FrameSite;

    HWND hwnd;

    IWebBrowser2 *      webBrowser;
    IOleObject *        oleObject;
    IOleInPlaceObject * oleInPlaceObject;
    IViewObject *       viewObject;
    IConnectionPoint *  connectionPoint;
    HWND                oleObjectHwnd;

    DWORD               adviseCookie;

    bool                aboutBlankShown;

    bool                documentLoaded;

    HtmlWindowCallback *htmlWinCb;

    void EnsureAboutBlankShown();
    void CreateBrowser();

public:
    HtmlWindow(HWND hwnd, HtmlWindowCallback *cb);
    ~HtmlWindow();

    void OnSize(int dx, int dy);
    void SetVisible(bool visible);
    void NavigateToUrl(const TCHAR *url);
    void DisplayHtml(const TCHAR *html);
    void GoBack();
    void GoForward();
    void PrintCurrentPage();
    void SetZoomPercent(int zoom);
    int  GetZoomPercent();
    void FindInCurrentPage();
    bool WaitUntilLoaded(DWORD maxWaitMs);

    HBITMAP TakeScreenshot(RectI area, SizeI finalSize);
    bool OnBeforeNavigate(const TCHAR *url, bool newWindow);
    void OnDocumentComplete(const TCHAR *url);

    bool canGoBack;
    bool canGoForward;
};

#endif
