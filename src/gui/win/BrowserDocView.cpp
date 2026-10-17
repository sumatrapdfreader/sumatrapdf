/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/JsonParser.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/win/HtmlWindow.h"
#include "gui/win/WebView.h"

#include "AppTools.h"
#include "Accelerators.h"
#include "gui/BrowserView.h"
#include "gui/win/BrowserDocView.h"
#include "BrowserViewCommon.h"

constexpr const char* kChmVirtualHost = "https://sumatrapdf.chm/";
constexpr const WCHAR* kChmVirtualHostW = L"https://sumatrapdf.chm/";

// Reports the document scroll position back to the host so GetScrollPos() can
// answer synchronously (WebView2 script eval is async and can't return a value).
// Posted through chrome.webview, not __sumatra__.notify: AddScriptToExecuteOnDocumentCreated
// is async and often loses the race with the first Navigate, so the bridge
// script may be missing on this document. Hash navigation often never fires a
// 'scroll' event either, which left GetScrollPos() at (-1,-1). So we post
// immediately, on scroll/hashchange/load/resize, and again shortly after.
constexpr const char* kReportScrollJs = R"JS((function(){
  var post = function() {
    try {
      var wv = window.chrome && window.chrome.webview;
      if (!wv) { return; }
      var x = Math.round(window.scrollX || window.pageXOffset || 0);
      var y = Math.round(window.scrollY || window.pageYOffset || 0);
      wv.postMessage(JSON.stringify({__sumatraNotify:1,method:"scroll",params:JSON.stringify([x,y])}));
    } catch (e) {}
  };
  if (!window.__sumatraScrollHooked) {
    window.__sumatraScrollHooked = true;
    window.addEventListener("scroll", post, true);
    window.addEventListener("hashchange", post);
    window.addEventListener("load", post);
    window.addEventListener("resize", post);
  }
  post();
  setTimeout(post, 0);
  setTimeout(post, 50);
})();)JS";

// How kFindInPageJs reports to the host: through the JS bridge.
constexpr const char* kFindNotifyJs = R"JS(window.__sumatraFindNotify = function() {
  window.__sumatra__.notify.apply(window.__sumatra__, arguments);
};)JS";

// WebView2 host for the scroll-position and find-result notifications sent by
// kReportScrollJs / kFindInPageJs through the JS bridge.
struct BrowserWebviewWnd : WebviewWnd {
    BrowserDocView* owner = nullptr;
    // BrowserWebviewWnd is a friend of BrowserDocView, and friendship covers its
    // members, so this can reach the private state the notifications update
    static void OnJsNotifyCb(void* ctx, Str method, Str paramsJson);
};

namespace {
// Pulls the positional arguments out of a bridge notification's params, which
// arrive as a JSON array. Collect single-segment array paths i0, i1, ...
struct NotifyArgsState {
    static constexpr int kMaxArgs = 4;
    TempStr args[kMaxArgs] = {};

    int Int(int idx) const {
        Str s = Text(idx);
        if (len(s) == 0) {
            return 0;
        }
        int v = 0;
        str::Parse(s, "%d", &v);
        return v;
    }
    Str Text(int idx) const {
        if (idx < 0 || idx >= kMaxArgs || len(args[idx]) == 0) {
            return {};
        }
        return args[idx];
    }
};

} // namespace

void BrowserWebviewWnd::OnJsNotifyCb(void* ctx, Str method, Str paramsJson) {
    auto* self = (BrowserDocView*)ctx;
    if (!self) {
        return;
    }
    NotifyArgsState st;
    json::Reader params(paramsJson);
    for (json::Value& v : params) {
        // the top-level array's elements, by index
        int idx = json::PathSegIndex(v.path);
        if (idx >= 0 && idx < NotifyArgsState::kMaxArgs && v.path && !v.path->next) {
            st.args[idx] = v.value;
        }
    }
    if (!params.Ok()) {
        logf("BrowserOnJsNotify: bad params for '%s': %s\n", method, paramsJson);
        return;
    }
    if (str::Eq(method, StrL("scroll"))) {
        int x = std::max(0, st.Int(0));
        int y = std::max(0, st.Int(1));
        self->webviewScrollPos = Point(x, y);
        return;
    }
    if (!self->cb) {
        return;
    }
    if (str::Eq(method, StrL("findResult"))) {
        self->cb->OnFindResult(st.Int(0), st.Int(1), st.Int(2));
        return;
    }
    if (str::Eq(method, StrL("findAllResult"))) {
        // downstream (BrowserFindAllResultReceived) still parses the flat
        // "<gen> <total> <recs>" form, so reassemble it here
        self->cb->OnFindAllResult(fmt("%d %d %s", st.Int(0), st.Int(1), st.Text(2)));
        return;
    }
    logf("BrowserOnJsNotify: unhandled '%s'\n", method);
}

bool BrowserDocView::ResourceGet(void* ctx, Str path, WebViewResourceResult* res) {
    auto* view = (BrowserDocView*)ctx;
    if (!view || !view->cb || !res || len(path) == 0) {
        return false;
    }
    Str data = view->cb->GetDataForUrl(path);
    if (len(data) == 0) {
        return false;
    }
    res->data = (u8*)data.s;
    res->dataLen = (size_t)data.len;
    res->contentType = str::Dup(ChmMimeFromPathTemp(path, data));
    res->ownsData = false;
    return true;
}

bool BrowserDocView::NavigationStarting(void* ctx, Str url, bool newWindow) {
    auto* view = (BrowserDocView*)ctx;
    if (!view || !view->cb) {
        return false;
    }
    bool allow = view->cb->OnBeforeNavigate(url, newWindow);
    // drop the previous page's position so GetScrollPos() does not keep a stale
    // y while the new document (or hash) is still applying
    if (allow && !newWindow) {
        view->webviewScrollPos = Point(-1, -1);
    }
    return allow;
}

void BrowserDocView::NavigationCompleted(void* ctx, Str url, bool success) {
    auto* view = (BrowserDocView*)ctx;
    if (!view || !view->cb || !success || len(url) == 0) {
        return;
    }
    // the WebView2 child windows that receive drops are created lazily and can
    // be recreated on navigation, so (re)install our forwarding drop target now
    if (view->wv) {
        view->wv->RegisterForwardingDropTarget();
    }
    view->cb->OnDocumentComplete(url);
    // after zoom / restore have applied: init scripts may have missed this
    // document, and hash navigation often never fires a 'scroll' event
    if (view->wv) {
        view->wv->Eval(Str(kReportScrollJs));
    }
}

void BrowserDocView::HistoryChanged(void* ctx, bool canGoBack, bool canGoForward) {
    auto* view = (BrowserDocView*)ctx;
    if (!view) {
        return;
    }
    view->canGoBack = canGoBack;
    view->canGoForward = canGoForward;
}

// Maps a key pressed while the WebView2-hosted CHM has focus to the app command
// to run, so app keyboard shortcuts (Ctrl+F, F1, F2, F3, F5, ...) keep working
// when focus is in the document (issue #5735). Esc isn't an accelerator -- it's
// handled by the frame's key handler -- so forward the key itself.
static int ChmResolveAccelCmd(void* /*user*/, u16 vk, bool ctrl, bool shift, bool alt) {
    if (vk == VK_ESCAPE) {
        return kWebViewForwardKey;
    }
    return SafeAcceleratorCmd(vk, ctrl, shift, alt);
}

void BrowserDocView::SubclassParent() {
    if (subclassId || !hwndParent) {
        return;
    }
    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwndParent, ParentWndProc, subclassId, (DWORD_PTR)this);
    if (!ok) {
        logf("BrowserDocView: SetWindowSubclass() failed, err: %d\n", (int)GetLastError());
        subclassId = 0;
        return;
    }
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)this);
}

void BrowserDocView::UnsubclassParent() {
    if (!subclassId || !hwndParent) {
        return;
    }
    RemoveWindowSubclass(hwndParent, ParentWndProc, subclassId);
    auto* curr = (BrowserDocView*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);
    if (curr == this) {
        SetWindowLongPtr(hwndParent, GWLP_USERDATA, 0);
    }
    subclassId = 0;
}

bool BrowserDocView::IsVisible() const {
    return visible;
}

void BrowserDocView::RefreshControllerSurface() {
    if (visible && backend == Backend::WebView2 && wv) {
        wv->RefreshControllerSurface();
    }
}

// show/hide without destroying WebView2 / IE. Creating a browser is slow, so
// tab switches hide the view and show it again instead of recreate.
void BrowserDocView::SetVisible(bool show) {
    if (visible == show) {
        if (show) {
            // still refresh size in case the canvas resized while we were hidden
            if (backend == Backend::WebView2 && wv && wv->hwnd && hwndParent) {
                Rect rc = HwndClientRect(hwndParent);
                wv->SetPos(&rc);
                wv->UpdateWebviewSize();
            }
        }
        return;
    }
    visible = show;
    if (show) {
        SubclassParent();
        if (backend == Backend::WebView2 && wv) {
            if (wv->hwnd && hwndParent) {
                Rect rc = HwndClientRect(hwndParent);
                wv->SetPos(&rc);
                ::ShowWindow(wv->hwnd, SW_SHOW);
            }
            wv->SetControllerVisible(true);
            wv->UpdateWebviewSize();
            wv->RefreshControllerSurface();
        } else if (backend == Backend::IE && ie) {
            ie->SetVisible(true);
        }
        return;
    }
    if (backend == Backend::WebView2 && wv) {
        wv->SetControllerVisible(false);
        if (wv->hwnd) {
            ::ShowWindow(wv->hwnd, SW_HIDE);
        }
    } else if (backend == Backend::IE && ie) {
        ie->SetVisible(false);
    }
    UnsubclassParent();
}

LRESULT BrowserDocView::ParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*subclassId*/,
                                      DWORD_PTR data) {
    auto* view = reinterpret_cast<BrowserDocView*>(data);
    if (!view) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_SIZE:
            if (view->wv && view->wv->hwnd) {
                Rect rc = HwndClientRect(hwnd);
                view->wv->SetPos(&rc);
                view->wv->UpdateWebviewSize();
            }
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            // WebView2 often delivers the wheel to this parent; swallowing it
            // left markdown/CHM unscrollable when the browser wasn't focused (#6142).
            if (view->wv && view->wv->hwnd) {
                if (view->forwardingWheel) {
                    return 0;
                }
                view->forwardingWheel = true;
                LRESULT res = view->SendMsg(msg, wp, lp);
                view->forwardingWheel = false;
                return res;
            }
            break;

        case WM_VSCROLL:
            if (view->wv) {
                return view->SendMsg(msg, wp, lp);
            }
            break;

        case WM_PARENTNOTIFY:
            if (LOWORD(wp) == WM_LBUTTONDOWN && view->cb) {
                view->cb->OnLButtonDown();
            }
            break;
    }

    // note: WM_DROPFILES is intentionally not handled here so it passes through
    // to the canvas WndProc, which opens dropped files (forwarded from the
    // WebView2 via ForwardingDropTarget)
    return DefSubclassProc(hwnd, msg, wp, lp);
}

bool BrowserDocView::CreateWebView2() {
    auto* chmWv = new BrowserWebviewWnd();
    chmWv->owner = this;
    wv = chmWv;
    wv->dataDir = str::Dup(GetWebViewDataDirTemp());
    wv->resourceUriPrefix = wstr::Dup(virtualHostW);
    wv->resourceProvider.ctx = this;
    wv->resourceProvider.getResource = ResourceGet;
    wv->events.ctx = this;
    wv->events.navigationStarting = NavigationStarting;
    wv->events.navigationCompleted = NavigationCompleted;
    wv->events.historyChanged = HistoryChanged;
    wv->events.resolveAccelCmd = ChmResolveAccelCmd;
    wv->events.jsNotify = BrowserWebviewWnd::OnJsNotifyCb;
    wv->allowClipboardRead = false;
    wv->defaultBackgroundColor = kColWhite;
    // forward app accelerators (Ctrl+W close tab, Ctrl+K command palette, etc.)
    // to the main window so they work while the WebView2 has keyboard focus
    wv->forwardAppAccelerators = true;
    // let file drops fall through to the canvas (to open the dropped file)
    // instead of being swallowed by the WebView2 control
    wv->allowExternalDrop = false;
    // don't host downloads inside the document viewer (issue #5920)
    wv->routeDownloadsToOsBrowser = true;
    // stay hidden until SetVisible(true); OnControllerReady is async and must
    // not flash over another tab while this one is still being set up
    wv->desiredVisible = false;

    Rect rc = HwndClientRect(hwndParent);
    CreateWebViewArgs cargs;
    cargs.parent = hwndParent;
    cargs.pos = rc;
    if (!wv->Create(cargs)) {
        delete wv;
        wv = nullptr;
        return false;
    }

    wv->Init(Str(kReportScrollJs));
    wv->Init(Str(kFindInPageJs));
    wv->Init(Str(kFindNotifyJs));

    backend = Backend::WebView2;
    return true;
}

static HtmlBackend gHtmlBackend = HtmlBackend::Auto;

// -html-backend forces IE or WebView2 so tests can exercise both paths.
void SetHtmlBackend(HtmlBackend backend) {
    gHtmlBackend = backend;
}

BrowserDocView* BrowserDocView::Create(HWND hwndParent, HtmlWindowCallback* cb, Str virtualHostPrefix) {
    if (!hwndParent || !cb) {
        return nullptr;
    }

    auto* view = new BrowserDocView();
    view->hwndParent = hwndParent;
    view->cb = cb;
    if (virtualHostPrefix) {
        view->virtualHost = str::Dup(virtualHostPrefix);
        view->virtualHostW = wstr::Dup(ToWStrTemp(virtualHostPrefix));
    } else {
        view->virtualHost = str::Dup(Str(kChmVirtualHost));
        view->virtualHostW = wstr::Dup(kChmVirtualHostW);
    }

#ifdef _MSC_VER
    bool wantWebView = gHtmlBackend != HtmlBackend::IE && HasWebView();
    if (wantWebView && view->CreateWebView2()) {
        // leave hidden; caller shows with SetVisible(true) when the tab is active
        return view;
    }
#endif

    view->ie = HtmlWindow::Create(hwndParent, cb);
    if (view->ie) {
        view->backend = Backend::IE;
        // hide IE until the tab is shown
        view->ie->SetVisible(false);
        return view;
    }

    delete view;
    return nullptr;
}

BrowserDocView::~BrowserDocView() {
    UnsubclassParent();
    delete wv;
    delete ie;
    str::Free(virtualHost);
    wstr::Free(virtualHostW);
}

void BrowserDocView::NavigateToDataUrl(Str url) {
    if (len(url) == 0) {
        return;
    }
    if (backend == Backend::WebView2 && wv) {
        TempStr fullUrl = url;
        if (!str::StartsWith(url, virtualHost)) {
            fullUrl = str::JoinTemp(virtualHost, url);
        }
        wv->Navigate(fullUrl);
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->NavigateToDataUrl(url);
    }
}

void BrowserDocView::GoBack() {
    if (backend == Backend::WebView2 && wv) {
        wv->GoBack();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->GoBack();
    }
}

void BrowserDocView::GoForward() {
    if (backend == Backend::WebView2 && wv) {
        wv->GoForward();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->GoForward();
    }
}

void BrowserDocView::SetZoomPercent(int zoom) {
    zoomPercent = zoom;
    if (backend == Backend::WebView2 && wv) {
        wv->SetZoomPercent(zoom);
        zoomPercent = wv->GetZoomPercent();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->SetZoomPercent(zoom);
        zoomPercent = ie->GetZoomPercent();
    }
}

int BrowserDocView::GetZoomPercent() const {
    if (backend == Backend::WebView2 && wv) {
        return wv->GetZoomPercent();
    }
    if (backend == Backend::IE && ie) {
        return ie->GetZoomPercent();
    }
    return zoomPercent;
}

Point BrowserDocView::GetScrollPos() const {
    if (backend == Backend::WebView2 && wv) {
        return webviewScrollPos;
    }
    if (backend == Backend::IE && ie) {
        return ie->GetScrollPos();
    }
    return {-1, -1};
}

void BrowserDocView::SetScrollPos(Point pos) {
    if (pos.x < 0 && pos.y < 0) {
        return;
    }
    pos.x = std::max(pos.x, 0);
    pos.y = std::max(pos.y, 0);
    if (backend == Backend::WebView2 && wv) {
        TempStr js = fmt("window.scrollTo(%d, %d);", pos.x, pos.y);
        wv->Eval(js);
        webviewScrollPos = pos;
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->SetScrollPos(pos);
    }
}

void BrowserDocView::PrintCurrentPage(bool showUI) {
    if (backend == Backend::WebView2 && wv) {
        if (!wv->ShowPrintUI()) {
            wv->Eval(StrL("window.print()"));
        }
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->PrintCurrentPage(showUI);
    }
}

void BrowserDocView::FindInCurrentPage() {
    if (backend == Backend::WebView2 && wv) {
        // trigger the WebView2 (Chromium) find-on-page bar, like IE's own find
        wv->ShowFindUI();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->FindInCurrentPage();
    }
}

bool BrowserDocView::CanFindInPage() const {
    return backend == Backend::WebView2 && wv != nullptr;
}

// highlight matches of term on the current page; gotoIdx >= 0 makes that
// match current (used after navigating to a match on another page), -1 picks
// the first match at/below the current scroll position
void BrowserDocView::FindStart(Str term, bool matchCase, bool wholeWord, int gen, int gotoIdx) {
    if (!CanFindInPage()) {
        return;
    }
    TempStr esc = JsEscapeTemp(term);
    Str sTrue = StrL("true");
    Str sFalse = StrL("false");
    // no {} in the js passed to fmt(): it would parse them as positional args.
    // if the injected script is somehow missing, post a diagnostic message
    // that the default OnBrowserMessage() handler logs
    TempStr js =
        fmt("window.__sumatraFind ? __sumatraFind.start('%s', %s, %s, %d, %d) : "
            "window.chrome.webview.postMessage('mdfind-missing');",
            esc, matchCase ? sTrue : sFalse, wholeWord ? sTrue : sFalse, gen, gotoIdx);
    wv->Eval(js);
}

// search all pages of the document (pageUrls in page order); the match list
// comes back asynchronously as one findAllResult notification
void BrowserDocView::FindAllPages(const StrVec& pageUrls, Str term, bool matchCase, bool wholeWord, int gen) {
    if (!CanFindInPage()) {
        return;
    }
    str::Builder js;
    js.Append(StrL("window.__sumatraFind && __sumatraFind.searchAll(["));
    int n = len(pageUrls);
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            js.AppendChar(',');
        }
        // page urls may be internal document paths (e.g. chm pages); prefix
        // the virtual host like NavigateToDataUrl() does to make them fetchable
        TempStr fullUrl = pageUrls.At(i);
        if (!str::StartsWith(fullUrl, virtualHost)) {
            if (len(fullUrl) > 0 && fullUrl.s[0] == '/') {
                fullUrl = Str(fullUrl.s + 1, fullUrl.len - 1);
            }
            fullUrl = str::JoinTemp(virtualHost, fullUrl);
        }
        js.AppendChar('\'');
        js.Append(JsEscapeTemp(fullUrl));
        js.AppendChar('\'');
    }
    TempStr esc = JsEscapeTemp(term);
    Str sTrue = StrL("true");
    Str sFalse = StrL("false");
    js.Append(fmt("], '%s', %s, %s, %d);", esc, matchCase ? sTrue : sFalse, wholeWord ? sTrue : sFalse, gen));
    wv->Eval(ToStrTemp(js));
}

// jump to the idx-th match on the current page (as counted by FindStart)
void BrowserDocView::FindGoto(int idx) {
    if (!CanFindInPage()) {
        return;
    }
    TempStr js = fmt("window.__sumatraFind && __sumatraFind.gotoMatch(%d);", idx);
    wv->Eval(js);
}

void BrowserDocView::FindClear() {
    if (!CanFindInPage()) {
        return;
    }
    wv->Eval(StrL("window.__sumatraFind && __sumatraFind.clear();"));
}

void BrowserDocView::Eval(Str js) {
    if (backend == Backend::WebView2 && wv) {
        wv->Eval(js);
    }
}

void BrowserDocView::SelectAll() {
    if (backend == Backend::WebView2 && wv) {
        wv->Eval(StrL("document.execCommand('selectAll', false, null)"));
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->SelectAll();
    }
}

void BrowserDocView::CopySelection() {
    if (backend == Backend::WebView2 && wv) {
        wv->Eval(StrL("document.execCommand('copy', false, null)"));
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->CopySelection();
    }
}

LRESULT BrowserDocView::SendMsg(UINT msg, WPARAM wp, LPARAM lp) {
    if (backend == Backend::WebView2 && wv && wv->hwnd) {
        if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
            // Chromium ignores WM_MOUSEWHEEL on the host hwnd. When the frame
            // (focused) or canvas parent gets the wheel, scroll via JS (#6142).
            if ((LOWORD(wp) & MK_CONTROL) || IsCtrlPressed()) {
                return SendMessageW(wv->hwnd, msg, wp, lp);
            }
            short delta = GET_WHEEL_DELTA_WPARAM(wp);
            bool horiz = (msg == WM_MOUSEHWHEEL) || (LOWORD(wp) & MK_SHIFT) || IsShiftPressed();
            int d = -(int)delta;
            wv->Eval(horiz ? fmt("window.scrollBy(%d, 0)", d) : fmt("window.scrollBy(0, %d)", d));
            return 0;
        }
        return SendMessageW(wv->hwnd, msg, wp, lp);
    }
    if (backend == Backend::IE && ie) {
        return ie->SendMsg(msg, wp, lp);
    }
    return 0;
}

struct BrowserViewCallbackAdapter : HtmlWindowCallback {
    BrowserViewCallback* cb = nullptr;

    bool OnBeforeNavigate(Str url, bool newWindow) override { return cb->OnBeforeNavigate(url, newWindow); }
    void OnDocumentComplete(Str url) override { cb->OnDocumentComplete(url); }
    Str GetDataForUrl(Str url) override { return cb->GetDataForUrl(url); }
    void OnLButtonDown() override { cb->OnLButtonDown(); }
    void DownloadData(Str url, Str data) override { cb->DownloadData(url, data); }
    void OnFindResult(int gen, int current, int total) override { cb->OnFindResult(gen, current, total); }
    void OnFindAllResult(Str payload) override { cb->OnFindAllResult(payload); }
};

struct BrowserView {
    MainWindow* win = nullptr;
    BrowserDocView* view = nullptr;
    BrowserViewCallbackAdapter adapter;
};

bool BrowserViewAvailable() {
    return true;
}

BrowserView* BrowserViewCreate(MainWindow* win, HWND hwndParent, BrowserViewCallback* cb, Str virtualHost) {
    if (!hwndParent || !cb) {
        return nullptr;
    }

    auto* res = new BrowserView();
    res->win = win;
    res->adapter.cb = cb;
    res->view = BrowserDocView::Create(hwndParent, &res->adapter, virtualHost);
    if (!res->view) {
        delete res;
        return nullptr;
    }
    return res;
}

void BrowserViewDelete(BrowserView* bv) {
    if (!bv) {
        return;
    }
    delete bv->view;
    delete bv;
}

MainWindow* BrowserViewWindow(BrowserView* bv) {
    return bv ? bv->win : nullptr;
}

void BrowserViewSetVisible(BrowserView* bv, bool visible) {
    if (bv) {
        bv->view->SetVisible(visible);
    }
}

bool BrowserViewIsVisible(BrowserView* bv) {
    return bv && bv->view->IsVisible();
}

void BrowserViewRefreshSurface(BrowserView* bv) {
    if (bv) {
        bv->view->RefreshControllerSurface();
    }
}

void BrowserViewNavigate(BrowserView* bv, Str url) {
    if (bv) {
        bv->view->NavigateToDataUrl(url);
    }
}

void BrowserViewGoBack(BrowserView* bv) {
    if (bv) {
        bv->view->GoBack();
    }
}

void BrowserViewGoForward(BrowserView* bv) {
    if (bv) {
        bv->view->GoForward();
    }
}

bool BrowserViewCanGoBack(BrowserView* bv) {
    return bv && bv->view->canGoBack;
}

bool BrowserViewCanGoForward(BrowserView* bv) {
    return bv && bv->view->canGoForward;
}

void BrowserViewSetZoomPercent(BrowserView* bv, int zoom) {
    if (bv) {
        bv->view->SetZoomPercent(zoom);
    }
}

int BrowserViewGetZoomPercent(BrowserView* bv) {
    return bv ? bv->view->GetZoomPercent() : 100;
}

Point BrowserViewGetScrollPos(BrowserView* bv) {
    return bv ? bv->view->GetScrollPos() : Point(-1, -1);
}

void BrowserViewSetScrollPos(BrowserView* bv, Point pos) {
    if (bv) {
        bv->view->SetScrollPos(pos);
    }
}

void BrowserViewEval(BrowserView* bv, Str js) {
    if (bv) {
        bv->view->Eval(js);
    }
}

void BrowserViewSelectAll(BrowserView* bv) {
    if (bv) {
        bv->view->SelectAll();
    }
}

void BrowserViewCopySelection(BrowserView* bv) {
    if (bv) {
        bv->view->CopySelection();
    }
}

void BrowserViewPrint(BrowserView* bv, bool showUI) {
    if (bv) {
        bv->view->PrintCurrentPage(showUI);
    }
}

void BrowserViewFindInPageUI(BrowserView* bv) {
    if (bv) {
        bv->view->FindInCurrentPage();
    }
}

bool BrowserViewCanFindInPage(BrowserView* bv) {
    return bv && bv->view->CanFindInPage();
}

void BrowserViewFindStart(BrowserView* bv, Str term, bool matchCase, bool wholeWord, int gen, int gotoIdx) {
    if (bv) {
        bv->view->FindStart(term, matchCase, wholeWord, gen, gotoIdx);
    }
}

void BrowserViewFindAllPages(BrowserView* bv, const StrVec& urls, Str term, bool matchCase, bool wholeWord, int gen) {
    if (bv) {
        bv->view->FindAllPages(urls, term, matchCase, wholeWord, gen);
    }
}

void BrowserViewFindGoto(BrowserView* bv, int idx) {
    if (bv) {
        bv->view->FindGoto(idx);
    }
}

void BrowserViewFindClear(BrowserView* bv) {
    if (bv) {
        bv->view->FindClear();
    }
}

LRESULT BrowserViewPassUIMsg(BrowserView* bv, UINT msg, WPARAM wp, LPARAM lp) {
    return bv ? bv->view->SendMsg(msg, wp, lp) : 0;
}

gpui::El* BrowserViewBuild(BrowserView*, gpui::Ctx*, int) {
    return nullptr;
}

void BrowserViewCreatePending(MainWindow*, gpui::Ctx*) {}
void BrowserViewSetOwnHost(BrowserView*) {}
void BrowserViewCreatePendingIn(BrowserView*, gpui::Ctx*) {}
void BrowserViewSetBackground(BrowserView*, Color) {}
