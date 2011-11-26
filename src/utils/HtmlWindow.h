/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */
#ifndef HtmlWindow_h
#define HtmlWindow_h

#include "BaseUtil.h"
#include "GeomUtil.h"
#include <exdisp.h>

class FrameSite;

bool InHtmlNestedMessagePump();

// HtmlWindowCallback allows HtmlWindow to notify other code about notable
// events or delegate some of the functionality.
class HtmlWindowCallback
{
public:
    // called when we're about to show a given url. Returning false will
    // stop loading this url
    virtual bool OnBeforeNavigate(const TCHAR *url, bool newWindow) = 0;

    // allows for providing html data for a given url by other code.
    // returning false means data wasn't provided.
    // returning true means the data (and it's len) is provided under data/len.
    virtual bool GetHtmlForUrl(const TCHAR *url, char **data, size_t *len) = 0;

    // called when left mouse button is clicked in the web control window.
    // we use it to maintain proper focus (since it's stolen by left click)
    virtual void OnLButtonDown() = 0;

    // called when a file has been dropped on the HTML canvas
    virtual void OnDragDrop(HDROP hDrop) = 0;
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
    bool                blankWasShown;

    ScopedMem<TCHAR>    currentURL;

    HtmlWindowCallback *htmlWinCb;

    void EnsureAboutBlankShown();
    void CreateBrowser();

    void SubclassHwnd();
    void UnsubclassHwnd();

    void WriteHtml(SAFEARRAY* htmlArr);

public:
    WNDPROC wndProcBrowserPrev;

    HtmlWindow(HWND hwndParent, HtmlWindowCallback *cb);
    ~HtmlWindow();

    void OnSize(int dx, int dy);
    void SetVisible(bool visible);
    void NavigateToUrl(const TCHAR *url);
    void DisplayHtml(const WCHAR *html);
    void DisplayHtml(const char *html);
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
    bool OnDragEnter(IDataObject *dataObj);
    bool OnDragDrop(IDataObject *dataObj);

    bool canGoBack;
    bool canGoForward;
};

#endif
