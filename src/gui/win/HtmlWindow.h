/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class FrameSite;
struct BrowserViewCallback;

// HtmlWindow embeds a web browser (Internet Explorer) control
// inside provided HWND so that an app can display html content.
class HtmlWindow {
  public:
    int windowId = 0;
    HWND hwndParent = nullptr;
    IWebBrowser2* webBrowser = nullptr;
    IOleObject* oleObject = nullptr;
    IOleInPlaceObject* oleInPlaceObject = nullptr;
    IConnectionPoint* connectionPoint = nullptr;
    FrameSite* frameSite = nullptr;
    HWND oleObjectHwnd = nullptr;
    HWND browserControlHwnd = nullptr;
    int zoomDPI = 96;

    DWORD adviseCookie = 0;

    HtmlWindow(HWND hwndParent, BrowserViewCallback* cb);

    bool CreateBrowser();

    void SubclassHwnd();
    void UnsubclassHwnd();
    void SetScrollbarToAuto();

    ~HtmlWindow();

    void OnSize(Size size);
    void SetVisible(bool visible);
    void NavigateToUrl(Str url);
    void NavigateToDataUrl(Str url);
    void GoBack();
    void GoForward();
    void PrintCurrentPage(bool showUI);
    void SetZoomPercent(int zoom);
    int GetZoomPercent();
    Point GetScrollPos();
    void SetScrollPos(Point pos);
    void FindInCurrentPage();
    void SelectAll();
    void CopySelection();
    LRESULT SendMsg(UINT msg, WPARAM wp, LPARAM lp);
    void OnLButtonDown() const;

    bool canGoBack = false;
    bool canGoForward = false;

    BrowserViewCallback* htmlWinCb = nullptr;

    UINT_PTR subclassId = 0;
    bool forwardingWheel = false;

    bool OnBeforeNavigate(Str url, bool newWindow);
    void OnDocumentComplete(Str url);
    HRESULT OnDragEnter(IDataObject* dataObj);
    HRESULT OnDragDrop(IDataObject* dataObj);

    // The caller keeps cb alive until the window is destroyed.
    static HtmlWindow* Create(HWND hwndParent, BrowserViewCallback* cb);
};
