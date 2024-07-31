/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

TempStr GetWebView2VersionTemp();
bool HasWebView();

// TODO: maybe hide those inside a private struct
typedef interface ICoreWebView2 ICoreWebView2;
typedef interface ICoreWebView2Controller ICoreWebView2Controller;

using WebViewMsgCb = Func1<const char*>;

struct CreateWebViewArgs {
    HWND parent = nullptr;
    Rect pos;
};

struct WebviewWnd : Wnd {
    WebviewWnd();
    ~WebviewWnd() override;

    HWND Create(const CreateWebViewArgs&);

    void Eval(const char* js);
    void SetHtml(const char* html);
    void Init(const char* js);
    void Navigate(const char* url);
    bool Embed(WebViewMsgCb& cb);

    virtual void OnBrowserMessage(const char* msg);

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void UpdateWebviewSize();

    // this is where the webview2 control stores data
    // must be set before we call create
    // TODO: make Webview2CreateCustomArgs
    // with dataDir
    char* dataDir = nullptr;
    // DWORD m_main_thread = GetCurrentThreadId();
    ICoreWebView2* webview = nullptr;
    ICoreWebView2Controller* controller = nullptr;

    // TODO: not sure if flag needs to be atomic i.e. is CreateCoreWebView2EnvironmentWithOptions()
    // called on a different thread?
    volatile LONG flag = 0;
};
