/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/HtmlWindow.h"
#include "wingui/WebView.h"
#include "wingui/ChmDocView.h"

#include "AppTools.h"
#include "Accelerators.h"

constexpr const char* kChmVirtualHost = "https://sumatrapdf.chm/";
constexpr const WCHAR* kChmVirtualHostW = L"https://sumatrapdf.chm/";

// Injected on every WebView2 navigation. Reports the document scroll position
// back to the host on each scroll so ChmDocView can answer GetScrollPos()
// synchronously (WebView2 script eval is async and can't return a value here).
constexpr const char* kReportScrollJs =
    "(function(){var post=function(){try{var x=Math.round(window.scrollX||window.pageXOffset||0);"
    "var y=Math.round(window.scrollY||window.pageYOffset||0);"
    "window.chrome.webview.postMessage('chmscroll '+x+' '+y);}catch(e){}};"
    "window.addEventListener('scroll',post,true);})();";

// WebView2 host that captures scroll-position messages posted by kReportScrollJs.
struct ChmWebviewWnd : WebviewWnd {
    ChmDocView* owner = nullptr;
    void OnBrowserMessage(Str msg) override;
};

void ChmWebviewWnd::OnBrowserMessage(Str msg) {
    int x = 0;
    int y = 0;
    if (owner && !str::IsNull(str::Parse(msg, "chmscroll %d %d", &x, &y))) {
        if (x < 0) {
            x = 0;
        }
        if (y < 0) {
            y = 0;
        }
        owner->webviewScrollPos = Point(x, y);
        return;
    }
    WebviewWnd::OnBrowserMessage(msg);
}

static TempStr ChmMimeFromPathTemp(Str path, Str data) {
    Str ext = str::SliceFromCharLast(path, '.');
    if (str::ContainsChar(ext, ';')) {
        Str semi = str::SliceFromChar(ext, ';');
        TempStr trimmed = str::DupTemp(Str(path.s, (int)(semi.s - path.s)));
        return ChmMimeFromPathTemp(trimmed, data);
    }

    TempStr imgExt = GfxFileExtFromDataTemp(data);
    TempStr mime = MimeTypeFromExtTemp(ext, imgExt);
    if (!mime) {
        mime = "text/html";
    }
    return mime;
}

bool ChmDocView::ResourceGet(void* ctx, Str path, WebViewResourceResult* res) {
    auto* view = (ChmDocView*)ctx;
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

bool ChmDocView::NavigationStarting(void* ctx, Str url, bool newWindow) {
    auto* view = (ChmDocView*)ctx;
    if (!view || !view->cb) {
        return false;
    }
    return view->cb->OnBeforeNavigate(url, newWindow);
}

void ChmDocView::NavigationCompleted(void* ctx, Str url, bool success) {
    auto* view = (ChmDocView*)ctx;
    if (!view || !view->cb || !success || len(url) == 0) {
        return;
    }
    // the WebView2 child windows that receive drops are created lazily and can
    // be recreated on navigation, so (re)install our forwarding drop target now
    if (view->wv) {
        view->wv->RegisterForwardingDropTarget();
    }
    view->cb->OnDocumentComplete(url);
}

void ChmDocView::HistoryChanged(void* ctx, bool canGoBack, bool canGoForward) {
    auto* view = (ChmDocView*)ctx;
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
static int ChmResolveAccelCmd(void*, u16 vk, bool ctrl, bool shift, bool alt) {
    if (vk == VK_ESCAPE) {
        return kWebViewForwardKey;
    }
    return SafeAcceleratorCmd(vk, ctrl, shift, alt);
}

void ChmDocView::UnsubclassParent() {
    if (!subclassId || !hwndParent) {
        return;
    }
    RemoveWindowSubclass(hwndParent, ParentWndProc, subclassId);
    auto curr = (ChmDocView*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);
    if (curr == this) {
        SetWindowLongPtr(hwndParent, GWLP_USERDATA, 0);
    }
    subclassId = 0;
}

LRESULT ChmDocView::ParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    auto* view = reinterpret_cast<ChmDocView*>(data);
    if (!view) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_SIZE:
            if (view->wv && view->wv->hwnd) {
                Rect rc = ClientRect(hwnd);
                view->wv->SetBounds(rc);
                view->wv->UpdateWebviewSize();
            }
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            if (view->wv && view->wv->hwnd) {
                return 0;
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

bool ChmDocView::CreateWebView2() {
    auto* chmWv = new ChmWebviewWnd();
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
    // forward app accelerators (Ctrl+W close tab, Ctrl+K command palette, etc.)
    // to the main window so they work while the WebView2 has keyboard focus
    wv->forwardAppAccelerators = true;
    // let file drops fall through to the canvas (to open the dropped file)
    // instead of being swallowed by the WebView2 control
    wv->allowExternalDrop = false;

    Rect rc = ClientRect(hwndParent);
    CreateWebViewArgs cargs;
    cargs.parent = hwndParent;
    cargs.pos = rc;
    if (!wv->Create(cargs)) {
        delete wv;
        wv = nullptr;
        return false;
    }

    wv->Init(kReportScrollJs);

    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwndParent, ParentWndProc, subclassId, (DWORD_PTR)this);
    ReportIf(!ok);
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)this);
    backend = Backend::WebView2;
    return true;
}

ChmDocView* ChmDocView::Create(HWND hwndParent, HtmlWindowCallback* cb, Str virtualHostPrefix) {
    if (!hwndParent || !cb) {
        return nullptr;
    }

    auto* view = new ChmDocView();
    view->hwndParent = hwndParent;
    view->cb = cb;
    if (virtualHostPrefix) {
        view->virtualHost = str::Dup(virtualHostPrefix);
        view->virtualHostW = wstr::Dup(ToWStrTemp(virtualHostPrefix));
    } else {
        view->virtualHost = str::Dup(kChmVirtualHost);
        view->virtualHostW = wstr::Dup(kChmVirtualHostW);
    }

#ifdef _MSC_VER
    if (HasWebView() && view->CreateWebView2()) {
        return view;
    }
#endif

    view->ie = HtmlWindow::Create(hwndParent, cb);
    if (view->ie) {
        view->backend = Backend::IE;
        return view;
    }

    delete view;
    return nullptr;
}

ChmDocView::~ChmDocView() {
    UnsubclassParent();
    delete wv;
    delete ie;
    str::Free(virtualHost);
    wstr::Free(virtualHostW);
}

void ChmDocView::NavigateToDataUrl(Str url) {
    if (!url) {
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

void ChmDocView::GoBack() {
    if (backend == Backend::WebView2 && wv) {
        wv->GoBack();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->GoBack();
    }
}

void ChmDocView::GoForward() {
    if (backend == Backend::WebView2 && wv) {
        wv->GoForward();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->GoForward();
    }
}

void ChmDocView::SetZoomPercent(int zoom) {
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

int ChmDocView::GetZoomPercent() const {
    if (backend == Backend::WebView2 && wv) {
        return wv->GetZoomPercent();
    }
    if (backend == Backend::IE && ie) {
        return ie->GetZoomPercent();
    }
    return zoomPercent;
}

Point ChmDocView::GetScrollPos() const {
    if (backend == Backend::WebView2 && wv) {
        return webviewScrollPos;
    }
    if (backend == Backend::IE && ie) {
        return ie->GetScrollPos();
    }
    return Point(-1, -1);
}

void ChmDocView::SetScrollPos(Point pos) {
    if (pos.x < 0 && pos.y < 0) {
        return;
    }
    if (pos.x < 0) {
        pos.x = 0;
    }
    if (pos.y < 0) {
        pos.y = 0;
    }
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

void ChmDocView::PrintCurrentPage(bool showUI) {
    if (backend == Backend::WebView2 && wv) {
        if (showUI) {
            wv->Eval("window.print()");
        } else {
            wv->Eval("window.print()");
        }
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->PrintCurrentPage(showUI);
    }
}

void ChmDocView::FindInCurrentPage() {
    if (backend == Backend::WebView2 && wv) {
        // trigger the WebView2 (Chromium) find-on-page bar, like IE's own find
        wv->ShowFindUI();
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->FindInCurrentPage();
    }
}

void ChmDocView::SelectAll() {
    if (backend == Backend::WebView2 && wv) {
        wv->Eval("document.execCommand('selectAll', false, null)");
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->SelectAll();
    }
}

void ChmDocView::CopySelection() {
    if (backend == Backend::WebView2 && wv) {
        wv->Eval("document.execCommand('copy', false, null)");
        return;
    }
    if (backend == Backend::IE && ie) {
        ie->CopySelection();
    }
}

LRESULT ChmDocView::SendMsg(UINT msg, WPARAM wp, LPARAM lp) {
    if (backend == Backend::WebView2 && wv && wv->hwnd) {
        return SendMessageW(wv->hwnd, msg, wp, lp);
    }
    if (backend == Backend::IE && ie) {
        return ie->SendMsg(msg, wp, lp);
    }
    return 0;
}
