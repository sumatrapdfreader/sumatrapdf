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
struct VirtMouseEvent;
struct PlatformFont;

struct SimpleBrowserWindow : WindowBase {
    WebviewWnd* webView = nullptr;
    VirtButton* btnBack = nullptr;
    VirtButton* btnForward = nullptr;
    VirtText* urlText = nullptr;
    PlatformFont* font = nullptr; // not owned, interned
    bool webViewFocusSet = false;

    HWND Create(const SimpleBrowserCreateArgs&);
    void OnFocus(WindowBase::FocusEvent*);
    void OnSize(WindowBase::SizeEvent*);
    void OnBack(VirtMouseEvent* ev = nullptr);
    void OnForward(VirtMouseEvent* ev = nullptr);
    ~SimpleBrowserWindow() override;
};

SimpleBrowserWindow* SimpleBrowserWindowCreate(const SimpleBrowserCreateArgs&);