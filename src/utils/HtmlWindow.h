/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */
#ifndef HtmlWindow_h
#define HtmlWindow_h

#include "BaseUtil.h"
#include "GeomUtil.h"
#include <exdisp.h>

class FrameSite;

bool InHtmlNestedMessagePump();

class HtmlWindowCallback
{
public:
    virtual bool OnBeforeNavigate(const TCHAR *url, bool newWindow) = 0;
    virtual void OnLButtonDown() = 0;
};

class HtmlWindow
{
protected:
    friend class FrameSite;

    HWND                hwndParent;
    IWebBrowser2 *      webBrowser;
    IOleObject *        oleObject;
    IOleInPlaceObject * oleInPlaceObject;
    IViewObject *       viewObject;
    IConnectionPoint *  connectionPoint;
    HWND                oleObjectHwnd;

    DWORD               adviseCookie;

    ScopedMem<TCHAR>    currentURL;

    HtmlWindowCallback *htmlWinCb;

    void EnsureAboutBlankShown();
    void CreateBrowser();

    void SubclassHwnd();
    void UnsubclassHwnd();

public:
    WNDPROC wndProcBrowserPrev;

    HtmlWindow(HWND hwndParent, HtmlWindowCallback *cb);
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
    void SelectAll();
    void CopySelection();
    bool WaitUntilLoaded(DWORD maxWaitMs, const TCHAR *url=NULL);
    void SendMsg(UINT msg, WPARAM wp, LPARAM lp);
    void OnLButtonDown() const;

    HBITMAP TakeScreenshot(RectI area, SizeI finalSize);
    bool OnBeforeNavigate(const TCHAR *url, bool newWindow);
    void OnDocumentComplete(const TCHAR *url);

    bool canGoBack;
    bool canGoForward;
};

#endif
