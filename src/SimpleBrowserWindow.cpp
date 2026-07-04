/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"

#include "Settings.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"

#include "SimpleBrowserWindow.h"

constexpr int kNavRowPadding = 6;
constexpr int kNavBtnGap = 4;

static LRESULT CALLBACK UrlStaticSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void SetCurrentUrl(SimpleBrowserWindow* w, Str url) {
    if (!w || !w->hwndUrl) {
        return;
    }
    HwndSetText(w->hwndUrl, url);
}

static void UpdateNavButtons(SimpleBrowserWindow* w) {
    if (!w || !w->webView) {
        return;
    }
    if (w->btnBack) {
        w->btnBack->SetIsEnabled(w->webView->CanGoBack());
    }
    if (w->btnForward) {
        w->btnForward->SetIsEnabled(w->webView->CanGoForward());
    }
}

static void LayoutControls(SimpleBrowserWindow* w) {
    if (!w || !w->hwnd || !w->btnBack || !w->btnForward || !w->hwndUrl) {
        return;
    }

    Rect rc = ClientRect(w->hwnd);
    int pad = DpiScale(w->hwnd, kNavRowPadding);
    int gap = DpiScale(w->hwnd, kNavBtnGap);
    int y = pad;
    int x = pad;

    Size backSize = w->btnBack->GetIdealSize();
    Size fwdSize = w->btnForward->GetIdealSize();
    int rowH = backSize.dy;
    if (fwdSize.dy > rowH) {
        rowH = fwdSize.dy;
    }

    MoveWindow(w->btnBack->hwnd, x, y, backSize.dx, backSize.dy, TRUE);
    x += backSize.dx + gap;
    MoveWindow(w->btnForward->hwnd, x, y, fwdSize.dx, fwdSize.dy, TRUE);
    x += fwdSize.dx + gap;

    int urlX = x;
    int urlDx = rc.dx - urlX - pad;
    if (urlDx < 0) {
        urlDx = 0;
    }
    int urlDy = FontDyPx(w->hwnd, w->hFont);
    if (urlDy <= 0) {
        urlDy = rowH;
    }
    int urlY = y + (rowH - urlDy) / 2;
    MoveWindow(w->hwndUrl, urlX, urlY, urlDx, urlDy, TRUE);

    int navRowDy = rowH + 2 * pad;
    int webDy = rc.dy - navRowDy - pad;
    if (webDy < 0) {
        webDy = 0;
    }
    int webDx = rc.dx - 2 * pad;
    if (webDx < 0) {
        webDx = 0;
    }
    if (w->webView) {
        w->webView->SetBounds({pad, navRowDy, webDx, webDy});
        w->webView->UpdateWebviewSize();
    }
}

static void OnBack(SimpleBrowserWindow* w) {
    if (w && w->webView) {
        w->webView->GoBack();
    }
}

static void OnForward(SimpleBrowserWindow* w) {
    if (w && w->webView) {
        w->webView->GoForward();
    }
}

// an absolute http(s)/mailto URL is "non-internal": it points outside the
// content we serve from our virtual host (UrlForWebViewEvent strips the host
// prefix off internal pages, so those arrive as a bare path without a scheme)
static bool IsExternalUrl(Str url) {
    return str::StartsWithI(url, "http://") || str::StartsWithI(url, "https://") || str::StartsWithI(url, "mailto:");
}

static bool NavigationStarting(void* ctx, Str url, bool newWindow) {
    auto* w = (SimpleBrowserWindow*)ctx;
    if (!w) {
        return true;
    }
    // When we host internal content (the manual) from a virtual host, its
    // non-internal links are rendered with target="_blank", which arrives here as
    // a new-window request. Open those (and any external in-window navigation) in
    // the user's default browser instead of the in-app webview. A plain browser
    // window (no virtual host) keeps normal in-window navigation.
    bool servesInternalContent = w->webView && len(w->webView->resourceUriPrefix) > 0;
    if (newWindow || (servesInternalContent && IsExternalUrl(url))) {
        SumatraLaunchBrowser(url);
        return false;
    }
    SetCurrentUrl(w, url);
    return true;
}

static void NavigationCompleted(void* ctx, Str url, bool success) {
    auto* w = (SimpleBrowserWindow*)ctx;
    if (!w || !success) {
        return;
    }
    SetCurrentUrl(w, url);
    UpdateNavButtons(w);
    if (!w->webViewFocusSet && w->webView) {
        w->webView->Focus();
        w->webViewFocusSet = true;
    }
}

static void HistoryChanged(void* ctx, bool canGoBack, bool canGoForward) {
    auto* w = (SimpleBrowserWindow*)ctx;
    if (!w) {
        return;
    }
    if (w->btnBack) {
        w->btnBack->SetIsEnabled(canGoBack);
    }
    if (w->btnForward) {
        w->btnForward->SetIsEnabled(canGoForward);
    }
}

SimpleBrowserWindow::~SimpleBrowserWindow() {
    delete btnBack;
    delete btnForward;
    delete webView;
}

LRESULT SimpleBrowserWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_SETFOCUS) {
        if (webView) {
            webView->Focus();
        }
        return 0;
    }
    if (msg == WM_SIZE) {
        LayoutControls(this);
        return 0;
    }
    if (msg == WM_CTLCOLORSTATIC && (HWND)lparam == hwndUrl) {
        HDC hdc = (HDC)wparam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        HBRUSH br = BackgroundBrush();
        if (!br) {
            br = (HBRUSH)GetStockObject(WHITE_BRUSH);
        }
        return (LRESULT)br;
    }
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

HWND SimpleBrowserWindow::Create(const SimpleBrowserCreateArgs& args) {
    HWND frameHwnd = nullptr;
    {
        CreateCustomArgs cargs;
        cargs.pos = args.pos;
        if (cargs.pos.IsZero()) {
            cargs.pos = {CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT};
        }
        cargs.title = args.title;
        if (!cargs.title) {
            cargs.title = "Browser Window";
        }
        HMODULE h = GetModuleHandleW(nullptr);
        WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
        cargs.icon = LoadIconW(h, iconName);
        // TODO: if set, navigate to url doesn't work
        // args.visible = false;
        frameHwnd = CreateCustom(cargs);
        ReportIf(!frameHwnd);
    }

    hFont = GetDefaultGuiFont();

    {
        Button::CreateArgs bargs;
        bargs.parent = frameHwnd;
        bargs.font = hFont;
        bargs.text = _TRA("Back");
        btnBack = new Button();
        btnBack->Create(bargs);
        btnBack->onClick = MkFunc0<SimpleBrowserWindow>(OnBack, this);
        btnBack->SetIsEnabled(false);
    }
    {
        Button::CreateArgs bargs;
        bargs.parent = frameHwnd;
        bargs.font = hFont;
        bargs.text = _TRA("Forward");
        btnForward = new Button();
        btnForward->Create(bargs);
        btnForward->onClick = MkFunc0<SimpleBrowserWindow>(OnForward, this);
        btnForward->SetIsEnabled(false);
    }
    {
        HINSTANCE inst = GetInstance();
        hwndUrl = CreateWindowExW(0, WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS, 0, 0, 0, 0,
                                  frameHwnd, nullptr, inst, nullptr);
        SendMessageW(hwndUrl, WM_SETFONT, (WPARAM)hFont, TRUE);
        SetWindowSubclass(hwndUrl, UrlStaticSubclassProc, NextSubclassId(), 0);
    }

    {
        webView = new WebviewWnd();
        Str dataDir = args.dataDir;
        if (!dataDir) {
            dataDir = GetWebViewDataDirTemp();
        }
        webView->dataDir = str::Dup(dataDir);
        webView->resourceProvider = args.resourceProvider;
        wstr::Free(webView->resourceUriPrefix);
        webView->resourceUriPrefix = wstr::Dup(args.resourceUriPrefix);
        webView->events.ctx = this;
        webView->events.navigationStarting = NavigationStarting;
        webView->events.navigationCompleted = NavigationCompleted;
        webView->events.historyChanged = HistoryChanged;
        webView->forwardAppAccelerators = false;

        CreateWebViewArgs cargs;
        cargs.parent = frameHwnd;
        cargs.pos = ClientRect(frameHwnd);
        if (!webView->Create(cargs)) {
            return nullptr;
        }
        webView->SetIsVisible(true);
    }

    SetCurrentUrl(this, args.url);
    LayoutControls(this);

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    webView->Navigate(args.url);
    SetIsVisible(true);
    if (webView) {
        webView->Focus();
    }
    return frameHwnd;
}

SimpleBrowserWindow* SimpleBrowserWindowCreate(const SimpleBrowserCreateArgs& args) {
    if (!HasWebView()) {
        return nullptr;
    }
    auto res = new SimpleBrowserWindow();
    auto hwnd = res->Create(args);
    ReportIfFast(!hwnd);
    if (!hwnd) {
        delete res;
        return nullptr;
    }
    return res;
}