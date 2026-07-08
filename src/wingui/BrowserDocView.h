/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HtmlWindowCallback;
struct WebViewResourceResult;
struct BrowserWebviewWnd;

// Hosts a document's HTML content (CHM, markdown) in an embedded browser:
// WebView2 when available, otherwise IE.
class BrowserDocView {
    friend struct BrowserWebviewWnd;

  public:
    bool canGoBack = false;
    bool canGoForward = false;

    static BrowserDocView* Create(HWND hwndParent, HtmlWindowCallback* cb, Str virtualHostPrefix = {});
    ~BrowserDocView();

    void NavigateToDataUrl(Str url);
    void GoBack();
    void GoForward();
    void SetZoomPercent(int zoom);
    int GetZoomPercent() const;
    // Scroll position is in CSS pixels of the hosted document.
    // Returns (-1, -1) if it isn't known (e.g. nothing scrolled yet).
    Point GetScrollPos() const;
    void SetScrollPos(Point pos);
    void PrintCurrentPage(bool showUI);
    void FindInCurrentPage();
    // in-page find with our own UI (WebView2 backend only)
    bool CanFindInPage() const;
    void FindStart(Str term, bool matchCase, bool wholeWord, int gen, int gotoIdx);
    void FindAllPages(const StrVec& pageUrls, Str term, bool matchCase, bool wholeWord, int gen);
    void FindGoto(int idx);
    void FindClear();
    void SelectAll();
    void CopySelection();
    LRESULT SendMsg(UINT msg, WPARAM wp, LPARAM lp);

  private:
    enum class Backend {
        None,
        IE,
        WebView2,
    };

    Backend backend = Backend::None;
    HWND hwndParent = nullptr;
    HtmlWindowCallback* cb = nullptr;
    class HtmlWindow* ie = nullptr;
    struct WebviewWnd* wv = nullptr;
    UINT_PTR subclassId = 0;
    int zoomPercent = 100;
    // last scroll position reported by the WebView2 content (in CSS pixels);
    // (-1, -1) means "not known yet". The IE backend reads scroll state
    // synchronously instead and doesn't use this.
    Point webviewScrollPos = Point(-1, -1);
    Str virtualHost;
    WStr virtualHostW;

    bool CreateWebView2();
    void UnsubclassParent();
    static LRESULT CALLBACK ParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                          DWORD_PTR data);
    static bool ResourceGet(void* ctx, Str path, WebViewResourceResult* res);
    static bool NavigationStarting(void* ctx, Str url, bool newWindow);
    static void NavigationCompleted(void* ctx, Str url, bool success);
    static void HistoryChanged(void* ctx, bool canGoBack, bool canGoForward);
};