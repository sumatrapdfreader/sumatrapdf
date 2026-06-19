/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HtmlWindowCallback;
struct WebViewResourceResult;

// Hosts CHM HTML content in WebView2 when available, otherwise in embedded IE.
class ChmDocView {
  public:
    bool canGoBack = false;
    bool canGoForward = false;

    static ChmDocView* Create(HWND hwndParent, HtmlWindowCallback* cb);
    ~ChmDocView();

    void NavigateToDataUrl(const char* url);
    void GoBack();
    void GoForward();
    void SetZoomPercent(int zoom);
    int GetZoomPercent() const;
    void PrintCurrentPage(bool showUI);
    void FindInCurrentPage();
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

    bool CreateWebView2();
    void UnsubclassParent();
    static LRESULT CALLBACK ParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                          DWORD_PTR data);
    static bool ResourceGet(void* ctx, const char* path, WebViewResourceResult* res);
    static bool NavigationStarting(void* ctx, const char* url, bool newWindow);
    static void NavigationCompleted(void* ctx, const char* url, bool success);
    static void HistoryChanged(void* ctx, bool canGoBack, bool canGoForward);
};