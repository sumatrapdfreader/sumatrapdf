/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/WebView.h"

#include "Settings.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Theme.h"

#include "SimpleBrowserWindow.h"

constexpr int kNavRowPadding = 6;
constexpr int kNavBtnGap = 4;

static void SetCurrentUrl(SimpleBrowserWindow* w, Str url) {
    if (!w || !w->urlText) {
        return;
    }
    w->urlText->SetText(url);
    w->urlText->Invalidate();
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
    if (!w || !w->hwnd || !w->layout) {
        return;
    }

    Rect rc = HwndClientRect(w->hwnd);
    // the nav row is as wide as the window and as tall as its tallest child
    Size navSize = w->layout->Layout(ExpandHeight(rc.dx));
    w->layout->SetBounds({0, 0, rc.dx, navSize.dy});
    // the row's controls are virtual: pick them up so we paint them and they
    // get their input. The root covers the client area, which is what the
    // window coordinates in the tree are relative to
    RefreshVirtTops(w->hwnd, w->layout, rc, &w->vroot);

    int pad = DpiScale(kNavRowPadding);
    int webDy = rc.dy - navSize.dy - pad;
    webDy = std::max(webDy, 0);
    int webDx = rc.dx - (2 * pad);
    webDx = std::max(webDx, 0);
    if (w->webView) {
        Rect webRc = {pad, navSize.dy, webDx, webDy};
        w->webView->SetPos(&webRc);
        w->webView->UpdateWebviewSize();
    }
}

void SimpleBrowserWindow::OnBack(VirtMouseEvent*) {
    if (webView) {
        webView->GoBack();
    }
}

void SimpleBrowserWindow::OnForward(VirtMouseEvent*) {
    if (webView) {
        webView->GoForward();
    }
}

// an absolute http(s)/mailto URL is "non-internal": it points outside the
// content we serve from our virtual host (UrlForWebViewEvent strips the host
// prefix off internal pages, so those arrive as a bare path without a scheme)
static bool IsExternalUrl(Str url) {
    return str::StartsWithI(url, StrL("http://")) || str::StartsWithI(url, StrL("https://")) ||
           str::StartsWithI(url, StrL("mailto:"));
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
    // ~WindowBase deletes `layout`, which owns the buttons and the url label
    delete webView;
}

void SimpleBrowserWindow::OnFocus(WindowBase::FocusEvent*) {
    if (webView) {
        webView->Focus();
    }
}

void SimpleBrowserWindow::OnSize(WindowBase::SizeEvent* ev) {
    if (ev->msg != WM_SIZE) {
        return;
    }
    LayoutControls(this);
}

HWND SimpleBrowserWindow::Create(const SimpleBrowserCreateArgs& args) {
    // LayoutControls sizes the nav row to its natural height and the webview
    // into the leftover client area, not a full-client DoLayout
    autoLayout = false;
    // docs window: Ctrl+W closes; Esc does not (search dialog, issue #5942)
    closeOnCtrlW = true;
    onFocus = MkMethod1<SimpleBrowserWindow, WindowBase::FocusEvent*, &SimpleBrowserWindow::OnFocus>(this);
    onSize = MkMethod1<SimpleBrowserWindow, WindowBase::SizeEvent*, &SimpleBrowserWindow::OnSize>(this);
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

    // the nav row is painted by us (it holds virtual buttons), so the window
    // needs a background color of its own - without one it paints black
    SetColors(ThemeWindowTextColor(), ThemeWindowBackgroundColor());

    font = GetDefaultGuiFont();

    {
        // Back | Forward | url, the whole row inset by kNavRowPadding. All
        // three are virtual controls, so the window paints them itself
        btnBack = NewThemedButton(frameHwnd, _TRA("Back"), font, false);
        btnBack->onClick = MkMethod1<SimpleBrowserWindow, VirtMouseEvent*, &SimpleBrowserWindow::OnBack>(this);
        btnBack->SetIsEnabled(false);
        btnForward = NewThemedButton(frameHwnd, _TRA("Forward"), font, false);
        btnForward->onClick = MkMethod1<SimpleBrowserWindow, VirtMouseEvent*, &SimpleBrowserWindow::OnForward>(this);
        btnForward->SetIsEnabled(false);
        urlText = NewVirtText({
            .font = font,
            .textColor = ThemeWindowTextColor(),
            // keep the file name visible when the url doesn't fit
            .pathEllipsis = true,
        });

        int pad = DpiScale(kNavRowPadding);
        Insets gap = DpiScaledInsets(0, 0, 0, kNavBtnGap);
        auto* row = new HBox();
        row->alignCross = CrossAxisAlign::CrossCenter;
        row->AddChild(btnBack);
        row->AddChild(new Padding(btnForward, gap));
        row->AddChild(new Padding(urlText, gap), 1);
        layout = new Padding(row, Insets{pad, pad, pad, pad});
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
        webView->forwardAppAccelerators = true;
        // in-app manual (virtual host): route downloads to the OS browser
        if (len(args.resourceUriPrefix) > 0) {
            webView->routeDownloadsToOsBrowser = true;
        }

        CreateWebViewArgs cargs;
        cargs.parent = frameHwnd;
        cargs.pos = HwndClientRect(frameHwnd);
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
    auto* res = new SimpleBrowserWindow();
    auto* hwnd = res->Create(args);
    ReportIfFast(!hwnd);
    if (!hwnd) {
        delete res;
        return nullptr;
    }
    return res;
}
