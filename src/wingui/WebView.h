/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

TempStr GetWebView2VersionTemp();
bool HasWebView();

// TODO: maybe hide those inside a private struct
typedef interface ICoreWebView2 ICoreWebView2;
typedef interface ICoreWebView2Controller ICoreWebView2Controller;

using WebViewMsgCb = Func1<Str>;

struct WebViewResourceResult {
    char* data = nullptr; // str-port: binary WebView2 resource payload, not a string
    size_t dataLen = 0;
    Str contentType;
    bool ownsData = true;
};

// resolveAccelCmd return value asking the webview to forward the key press
// itself (WM_KEYDOWN/UP) to the top-level window, rather than a command. For
// keys handled by the frame's key handler instead of an accelerator (e.g. Esc).
constexpr int kWebViewForwardKey = -1;

struct WebViewEvents {
    void* ctx = nullptr;
    bool (*navigationStarting)(void* ctx, Str url, bool newWindow) = nullptr;
    void (*navigationCompleted)(void* ctx, Str url, bool success) = nullptr;
    void (*historyChanged)(void* ctx, bool canGoBack, bool canGoForward) = nullptr;
    // maps an accelerator key press inside the webview to an app command id to
    // post (WM_COMMAND) to the top-level window, or 0 to leave it to the
    // webview, or kWebViewForwardKey to re-post the key itself. Lets the host
    // forward its keyboard shortcuts that the webview would otherwise swallow.
    int (*resolveAccelCmd)(void* ctx, u16 vk, bool ctrl, bool shift, bool alt) = nullptr;
};

struct WebViewResourceProvider {
    void* ctx = nullptr;
    bool (*getResource)(void* ctx, Str path, WebViewResourceResult* res) = nullptr;
};

struct PendingWebViewOp {
    enum Kind {
        Init,
        SetHtml,
        Eval,
        Navigate,
    };

    Kind kind;
    Str text;
};

struct CreateWebViewArgs {
    HWND parent = nullptr;
    Rect pos;
};

struct WebviewWnd : Wnd {
    WebviewWnd();
    ~WebviewWnd() override;

    HWND Create(const CreateWebViewArgs&);

    void Eval(Str js);
    void SetHtml(Str html);
    void Init(Str js);
    void Navigate(Str url);
    void GoBack();
    void GoForward();
    void SetZoomPercent(int zoom);
    int GetZoomPercent() const;
    bool CanGoBack() const;
    bool CanGoForward() const;
    void Focus();
    // open the WebView2 (Chromium) find-on-page bar, like a browser's Ctrl+F
    void ShowFindUI();
    void RegisterForwardingDropTarget();
    void RevokeForwardingDropTarget();
    bool Embed(WebViewMsgCb& cb);
    void OnControllerReady(ICoreWebView2Controller* controller);
    void FailInit();
    void QueuePendingOp(PendingWebViewOp::Kind kind, Str text);
    void FlushPendingOps();
    void SetControllerVisible(bool visible);

    virtual void OnBrowserMessage(Str msg);

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void UpdateWebviewSize();

    // this is where the webview2 control stores data
    // must be set before we call create
    // TODO: make Webview2CreateCustomArgs
    // with dataDir
    Str dataDir;
    // DWORD m_main_thread = GetCurrentThreadId();
    ICoreWebView2* webview = nullptr;
    ICoreWebView2Controller* controller = nullptr;
    // forwards file drops to the parent window when allowExternalDrop is false;
    // registered on the host hwnd and every WebView2 child window (the Chrome_*
    // composition windows that actually sit under the cursor)
    struct IDropTarget* dropTarget = nullptr;
    Vec<HWND> dropTargetHwnds;

    bool initStarted = false;
    bool initFailed = false;
    bool isVisible = true;
    bool isSuspended = false;
    bool isInSizeMove = false;
    RECT lastBounds = {};
    bool hasLastBounds = false;
    WStr userDataFolder;
    WStr resourceUriPrefix;
    WebViewResourceProvider resourceProvider;
    WebViewEvents events;
    bool forwardAppAccelerators = true;
    // when false, WebView2 won't claim external (file) drops, so they fall
    // through to the host window's drop target (e.g. to open the file)
    bool allowExternalDrop = true;
    Vec<PendingWebViewOp> pendingOps;
};