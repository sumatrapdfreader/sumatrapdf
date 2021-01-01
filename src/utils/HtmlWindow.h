/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class FrameSite;
class HtmlMoniker;

bool IsBlankUrl(const WCHAR*);

// HtmlWindowCallback allows HtmlWindow to notify other code about notable
// events or delegate some of the functionality.
class HtmlWindowCallback {
  public:
    // called when we're about to show a given url. Returning false will
    // stop loading this url
    virtual bool OnBeforeNavigate(const WCHAR* url, bool newWindow) = 0;

    // called after html document has been completely loaded
    virtual void OnDocumentComplete(const WCHAR* url) = 0;

    // allows for providing data for a given url.
    // returning nullptr means data wasn't provided.
    virtual std::span<u8> GetDataForUrl(const WCHAR* url) = 0;

    // called when left mouse button is clicked in the web control window.
    // we use it to maintain proper focus (since it's stolen by left click)
    virtual void OnLButtonDown() = 0;

    // called when a file can't be displayed and has to be downloaded instead
    virtual void DownloadData(const WCHAR* url, std::span<u8> data) = 0;

    virtual ~HtmlWindowCallback() {
    }
};

// HtmlWindow embeds a web browser (Internet Explorer) control
// inside provided HWND so that an app can display html content.
class HtmlWindow {
  protected:
    friend class FrameSite;

    int windowId;
    HWND hwndParent;
    IWebBrowser2* webBrowser;
    IOleObject* oleObject;
    IOleInPlaceObject* oleInPlaceObject;
    IViewObject* viewObject;
    IConnectionPoint* connectionPoint;
    HtmlMoniker* htmlContent;
    HWND oleObjectHwnd;

    const char* htmlSetInProgress;
    const WCHAR* htmlSetInProgressUrl;

    DWORD adviseCookie;
    bool blankWasShown;

    AutoFreeWstr currentURL;

    HtmlWindow(HWND hwndParent, HtmlWindowCallback* cb);

    void NavigateToAboutBlank();
    bool CreateBrowser();

    void SubclassHwnd();
    void UnsubclassHwnd();
    void SetScrollbarToAuto();
    void SetHtmlReal(std::span<u8>);
    void FreeHtmlSetInProgressData();

  public:
    ~HtmlWindow();

    void OnSize(Size size);
    void SetVisible(bool visible);
    void NavigateToUrl(const WCHAR* url);
    void NavigateToDataUrl(const WCHAR* url);
    void SetHtml(std::span<u8>, const WCHAR* url = nullptr);
    void GoBack();
    void GoForward();
    void PrintCurrentPage(bool showUI);
    void SetZoomPercent(int zoom);
    int GetZoomPercent();
    void FindInCurrentPage();
    void SelectAll();
    void CopySelection();
    LRESULT SendMsg(uint msg, WPARAM wp, LPARAM lp);
    void OnLButtonDown() const;
    HBITMAP TakeScreenshot(Rect area, Size finalSize);

    bool canGoBack;
    bool canGoForward;

    // TODO: not for public use
    WNDPROC wndProcBrowserPrev;
    LONG_PTR userDataBrowserPrev;
    HtmlWindowCallback* htmlWinCb;

    bool OnBeforeNavigate(const WCHAR* url, bool newWindow);
    void OnDocumentComplete(const WCHAR* url);
    HRESULT OnDragEnter(IDataObject* dataObj);
    HRESULT OnDragDrop(IDataObject* dataObj);

    static HtmlWindow* Create(HWND hwndParent, HtmlWindowCallback* cb);
};
