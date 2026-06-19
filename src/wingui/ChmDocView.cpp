/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"
#include "utils/GuessFileType.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/HtmlWindow.h"
#include "wingui/WebView.h"
#include "wingui/ChmDocView.h"

#include "AppTools.h"

constexpr const char* kChmVirtualHost = "https://sumatrapdf.chm/";
constexpr const WCHAR* kChmVirtualHostW = L"https://sumatrapdf.chm/";

static char* ChmMimeFromPath(const char* path, const ByteSlice& data) {
    const char* ext = str::FindCharLast(path, '.');
    if (ext && str::FindChar(ext, ';')) {
        TempStr trimmed = str::DupTemp(path, str::FindChar(ext, ';') - path);
        return ChmMimeFromPath(trimmed, data);
    }

    static const struct {
        const char* ext;
        const char* mimetype;
    } mimeTypes[] = {
        {".html", "text/html"},     {".htm", "text/html"},     {".gif", "image/gif"},  {".png", "image/png"},
        {".jpg", "image/jpeg"},     {".jpeg", "image/jpeg"},   {".bmp", "image/bmp"},  {".css", "text/css"},
        {".js", "text/javascript"}, {".svg", "image/svg+xml"}, {".txt", "text/plain"},
    };

    if (ext) {
        for (int i = 0; i < dimof(mimeTypes); i++) {
            if (str::EqI(ext, mimeTypes[i].ext)) {
                if (str::StartsWith(mimeTypes[i].mimetype, "image/")) {
                    const char* imgExt = GfxFileExtFromData(data);
                    if (imgExt) {
                        for (int j = 0; j < dimof(mimeTypes); j++) {
                            if (str::Eq(imgExt, mimeTypes[j].ext)) {
                                return str::Dup(mimeTypes[j].mimetype);
                            }
                        }
                    }
                }
                return str::Dup(mimeTypes[i].mimetype);
            }
        }
    }
    return str::Dup("text/html");
}

bool ChmDocView::ResourceGet(void* ctx, const char* path, WebViewResourceResult* res) {
    auto* view = (ChmDocView*)ctx;
    if (!view || !view->cb || !res || str::IsEmpty(path)) {
        return false;
    }
    ByteSlice data = view->cb->GetDataForUrl(path);
    if (data.empty()) {
        return false;
    }
    res->data = (char*)data.data();
    res->dataLen = data.size();
    res->contentType = ChmMimeFromPath(path, data);
    res->ownsData = false;
    return true;
}

bool ChmDocView::NavigationStarting(void* ctx, const char* url, bool newWindow) {
    auto* view = (ChmDocView*)ctx;
    if (!view || !view->cb) {
        return false;
    }
    return view->cb->OnBeforeNavigate(url, newWindow);
}

void ChmDocView::NavigationCompleted(void* ctx, const char* url, bool success) {
    auto* view = (ChmDocView*)ctx;
    if (!view || !view->cb || !success || str::IsEmpty(url)) {
        return;
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

        case WM_DROPFILES:
            return DefWindowProc(hwnd, msg, wp, lp);
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

bool ChmDocView::CreateWebView2() {
    wv = new WebviewWnd();
    wv->dataDir = str::Dup(GetPathInAppDataDirTemp("chmWebViewData"));
    wv->resourceUriPrefix = str::Dup(kChmVirtualHostW);
    wv->resourceProvider.ctx = this;
    wv->resourceProvider.getResource = ResourceGet;
    wv->events.ctx = this;
    wv->events.navigationStarting = NavigationStarting;
    wv->events.navigationCompleted = NavigationCompleted;
    wv->events.historyChanged = HistoryChanged;
    wv->forwardAppAccelerators = false;

    Rect rc = ClientRect(hwndParent);
    CreateWebViewArgs cargs;
    cargs.parent = hwndParent;
    cargs.pos = rc;
    if (!wv->Create(cargs)) {
        delete wv;
        wv = nullptr;
        return false;
    }

    subclassId = NextSubclassId();
    BOOL ok = SetWindowSubclass(hwndParent, ParentWndProc, subclassId, (DWORD_PTR)this);
    ReportIf(!ok);
    SetWindowLongPtr(hwndParent, GWLP_USERDATA, (LONG_PTR)this);
    backend = Backend::WebView2;
    return true;
}

ChmDocView* ChmDocView::Create(HWND hwndParent, HtmlWindowCallback* cb) {
    if (!hwndParent || !cb) {
        return nullptr;
    }

    auto* view = new ChmDocView();
    view->hwndParent = hwndParent;
    view->cb = cb;

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
}

void ChmDocView::NavigateToDataUrl(const char* url) {
    if (!url) {
        return;
    }
    if (backend == Backend::WebView2 && wv) {
        TempStr fullUrl = str::JoinTemp(kChmVirtualHost, url);
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

static void PostCtrlKey(HWND hwnd, int vk) {
    if (!hwnd) {
        return;
    }
    LPARAM lpDown = 1 | (MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16);
    LPARAM lpUp = lpDown | (1 << 30) | (1 << 31);
    PostMessageW(hwnd, WM_KEYDOWN, VK_CONTROL, lpDown);
    PostMessageW(hwnd, WM_KEYDOWN, (WPARAM)vk, lpDown);
    PostMessageW(hwnd, WM_KEYUP, (WPARAM)vk, lpUp);
    PostMessageW(hwnd, WM_KEYUP, VK_CONTROL, lpUp);
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
        wv->Focus();
        PostCtrlKey(wv->hwnd, 'F');
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