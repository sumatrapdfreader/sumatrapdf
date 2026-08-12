/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct SimpleBrowserCreateArgs {
    Str title;
    Rect pos; // if empty, will use CW_USEDEFAULT
    Str url;
    Str dataDir;
    WebViewResourceProvider resourceProvider;
    WStr resourceUriPrefix;
};

struct VirtButton;
struct VirtText;
struct PlatformFont;

struct SimpleBrowserWindow : WindowBase {
    WebviewWnd* webView = nullptr;
    VirtButton* btnBack = nullptr;
    VirtButton* btnForward = nullptr;
    VirtText* urlText = nullptr;
    PlatformFont* font = nullptr; // not owned, interned
    bool webViewFocusSet = false;

    HWND Create(const SimpleBrowserCreateArgs&);
    void WndProc(WindowBase::WndProcEvent* ev);
    void PreTranslate(WindowBase::PreTranslateEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    ~SimpleBrowserWindow() override;
};

SimpleBrowserWindow* SimpleBrowserWindowCreate(const SimpleBrowserCreateArgs&);