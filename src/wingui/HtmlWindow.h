/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class FrameSite;
class HtmlMoniker;

bool IsBlankUrl(Str url);
bool IsBlankUrl(WStr url);

// HtmlWindowCallback allows HtmlWindow to notify other code about notable
// events or delegate some of the functionality.
struct HtmlWindowCallback {
    // called when we're about to show a given url. Returning false will
    // stop loading this url
    virtual bool OnBeforeNavigate(Str url, bool newWindow) = 0;

    // called after html document has been completely loaded
    virtual void OnDocumentComplete(Str url) = 0;

    // allows for providing data for a given url.
    // returning nullptr means data wasn't provided.
    virtual Str GetDataForUrl(Str url) = 0;

    // called when left mouse button is clicked in the web control window.
    // we use it to maintain proper focus (since it's stolen by left click)
    virtual void OnLButtonDown() = 0;

    // called when a file can't be displayed and has to be downloaded instead
    virtual void DownloadData(Str url, Str data) = 0;

    // in-page find result (ChmDocView WebView2 backend only): 1-based current
    // match and total match count
    virtual void OnFindResult(int, int) {}

    virtual ~HtmlWindowCallback() = default;
};

// HtmlWindow embeds a web browser (Internet Explorer) control
// inside provided HWND so that an app can display html content.
class HtmlWindow {
  public:
    int windowId = 0;
    HWND hwndParent = nullptr;
    IWebBrowser2* webBrowser = nullptr;
    IOleObject* oleObject = nullptr;
    IOleInPlaceObject* oleInPlaceObject = nullptr;
    IViewObject* viewObject = nullptr;
    IConnectionPoint* connectionPoint = nullptr;
    HtmlMoniker* htmlContent = nullptr;
    HWND oleObjectHwnd = nullptr;
    int zoomDPI = 96;

    Str htmlSetInProgress;
    Str htmlSetInProgressUrl;

    DWORD adviseCookie = 0;
    bool blankWasShown = false;

    Str currentURL;

    HtmlWindow(HWND hwndParent, HtmlWindowCallback* cb);

    void NavigateToAboutBlank();
    bool CreateBrowser();

    void SubclassHwnd();
    void UnsubclassHwnd();
    void SetScrollbarToAuto();
    void SetHtmlReal(Str);
    void FreeHtmlSetInProgressData();

    ~HtmlWindow();

    void OnSize(Size size);
    void SetVisible(bool visible);
    void NavigateToUrl(Str url);
    void NavigateToDataUrl(Str url);
    void SetHtml(Str, Str url = nullptr);
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
    HBITMAP TakeScreenshot(Rect area, Size finalSize);

    bool canGoBack = false;
    bool canGoForward = false;

    HtmlWindowCallback* htmlWinCb = nullptr;

    UINT_PTR subclassId = 0;

    bool OnBeforeNavigate(Str url, bool newWindow);
    void OnDocumentComplete(Str url);
    HRESULT OnDragEnter(IDataObject* dataObj);
    HRESULT OnDragDrop(IDataObject* dataObj);

    static HtmlWindow* Create(HWND hwndParent, HtmlWindowCallback* cb);
};
