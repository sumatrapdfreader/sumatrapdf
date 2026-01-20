/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct SimpleBrowserCreateArgs {
    const char* title = nullptr;
    Rect pos{}; // if empty, will use CW_USEDEFAULT
    const char* url = nullptr;
    const char* dataDir = nullptr;
};

struct SimpleBrowserWindow : Wnd {
    WebviewWnd* webView = nullptr;
    HWND Create(const SimpleBrowserCreateArgs&);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    ~SimpleBrowserWindow() override;
};

SimpleBrowserWindow* SimpleBrowserWindowCreate(const SimpleBrowserCreateArgs&);
