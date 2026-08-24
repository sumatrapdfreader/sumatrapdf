/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

TempStr GetWebView2VersionTemp();
bool HasWebView();
void WebViewShutdown();

// TODO: maybe hide those inside a private struct
typedef interface ICoreWebView2 ICoreWebView2;
typedef interface ICoreWebView2Controller ICoreWebView2Controller;

using WebViewMsgCb = Func1<Str>;

struct WebViewResourceResult {
    const u8* data = nullptr;
    size_t dataLen = 0;
    Str contentType;
    bool ownsData = true;
};

// resolveAccelCmd return value asking the webview to forward the key press
// itself (WM_KEYDOWN/UP) to the top-level window, rather than a command. For
// keys handled by the frame's key handler instead of an accelerator (e.g. Esc).
constexpr int kWebViewForwardKey = -1;

enum class WebViewProcessFailure {
    // the whole WebView2 browser process died: the control is unusable
    BrowserExited,
    // only a renderer died or hung: reloading usually recovers
    RenderExited,
    RenderUnresponsive,
    // some other helper process (GPU, utility, ...) died
    Other,
};

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
    // a WebView2 process died. Return true if handled; returning false runs the
    // default recovery (reload for a dead renderer, fail the control for a dead
    // browser process). Called on the UI thread.
    bool (*processFailed)(void* ctx, WebViewProcessFailure kind) = nullptr;
    // a JS call made through window.__sumatra__.call(name, ...) / a bound name.
    // paramsJson is the arguments as a JSON array. Reply with WebviewWnd::Resolve
    // (may be async); not replying leaves the JS promise pending forever.
    void (*jsCall)(void* ctx, Str id, Str method, Str paramsJson) = nullptr;
    // window.__sumatra__.notify(name, ...): one-way, no reply and no promise, so
    // it's the right channel for high-frequency events like scrolling
    void (*jsNotify)(void* ctx, Str method, Str paramsJson) = nullptr;
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
    // for Init: the token AddInitScript() already handed back to the caller, so
    // the script keeps its identity across the queue
    int token = 0;
};

// An init script (AddScriptToExecuteOnDocumentCreated). `id` is assigned
// asynchronously by WebView2 and is what RemoveScriptToExecuteOnDocumentCreated
// needs, so a script can only be removed once its id has arrived.
struct WebViewInitScript {
    int token = 0;
    WStr id;
    // remove as soon as the id arrives (RemoveInitScript ran before that)
    bool removePending = false;
};

struct CreateWebViewArgs {
    HWND parent = nullptr;
    Rect pos;
};

struct WebviewWnd : WindowBase {
    WebviewWnd();
    ~WebviewWnd() override;

    HWND Create(const CreateWebViewArgs&);

    void Eval(Str js);
    void SetHtml(Str html);
    void Init(Str js);
    int AddInitScript(Str js);
    void AddInitScriptWithToken(Str js, int token);
    int FindInitScript(int token) const;
    void RemoveInitScript(int token);
    void RemoveAllInitScripts();
    void OnInitScriptAdded(int token, const WCHAR* id);
    void Navigate(Str url);
    void Bind(Str name);
    void Unbind(Str name);
    void Resolve(Str id, int status, Str resultJson);
    void OnJsCall(Str msg);
    void OnJsNotify(Str msg);
    void RebuildBindScript();
    void GoBack();
    void GoForward();
    void SetZoomPercent(int zoom);
    int GetZoomPercent() const;
    bool CanGoBack() const;
    bool CanGoForward() const;
    void Focus();
    void ShowFindUI();
    void RegisterForwardingDropTarget();
    void RevokeForwardingDropTarget();
    bool Embed(WebViewMsgCb& cb);
    void OnControllerReady(ICoreWebView2Controller* controller);
    void OnProcessFailed(WebViewProcessFailure kind);
    void FailInit();
    void QueuePendingOp(PendingWebViewOp::Kind kind, Str text, int token = 0);
    void FlushPendingOps();
    void SetControllerVisible(bool visible);
    void RefreshControllerSurface();

    virtual void OnBrowserMessage(Str msg);

    void OnTimer(WindowBase::TimerEvent* ev);
    void OnSize(WindowBase::SizeEvent* ev);
    void OnActivate(WindowBase::ActivateEvent* ev);
    void OnShowWindow(WindowBase::ShowWindowEvent* ev);

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
    // desired visibility: OnControllerReady respects this so a host can create
    // the control hidden (tab probe / deferred show) without the async ready
    // callback flipping it visible again. Default true matches hosts that call
    // SetIsVisible(true) after Create; BrowserDocView clears it until show.
    bool desiredVisible = true;
    bool isVisible = true;
    bool isSuspended = false;
    bool isInSizeMove = false;
    Rect lastBounds;
    bool hasLastBounds = false;
    WStr userDataFolder;
    WStr resourceUriPrefix;
    WebViewResourceProvider resourceProvider;
    WebViewEvents events;
    bool forwardAppAccelerators = true;
    bool allowClipboardRead = false;
    Color defaultBackgroundColor = kColorTransparent;
    // when false, WebView2 won't claim external (file) drops, so they fall
    // through to the host window's drop target (e.g. to open the file)
    bool allowExternalDrop = true;
    // when true, cancel in-webview downloads and open external http(s) URLs in
    // the OS default browser instead of WebView2's download UI (issue #5920)
    bool routeDownloadsToOsBrowser = false;
    Vec<PendingWebViewOp> pendingOps;
    Vec<WebViewInitScript> initScripts;
    int nextInitScriptToken = 1;
    // names exposed to JS via Bind(); the bind script is rebuilt when this changes
    Vec<Str> boundNames;
    int bindScriptToken = 0;
};
