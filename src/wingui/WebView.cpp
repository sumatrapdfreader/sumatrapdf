/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

// #include "Theme.h"

#ifdef _MSC_VER
#include "WebView2EnvironmentOptions.h"
#endif
#include "wingui/WebView.h"


Kind kindWebView = "webView";

#ifndef _MSC_VER
TempStr GetWebView2VersionTemp() {
    return {};
}
bool HasWebView() {
    return false;
}
#else
TempStr GetWebView2VersionTemp() {
    WCHAR* ver = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
    if (FAILED(hr) || (ver == nullptr)) {
        return {};
    }
    TempStr res = ToUtf8Temp(ver);
    CoTaskMemFree((void*)ver);
    return res;
}

bool HasWebView() {
    WCHAR* ver = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
    if (FAILED(hr) || len(ver) == 0) {
        logf("WebView2 is not available\n");
        return false;
    }
    return true;
}
#endif // _MSC_VER

#ifdef _MSC_VER

// ICoreWebView2Controller4 (with AllowExternalDrop) was added in a WebView2 SDK
// newer than the one we vendor, so declare it here. QueryInterface for it
// succeeds on Edge runtime 102+ and fails gracefully on older runtimes.
#ifndef __ICoreWebView2Controller4_INTERFACE_DEFINED__
#define __ICoreWebView2Controller4_INTERFACE_DEFINED__
MIDL_INTERFACE("97d418d5-a426-4e49-a151-e1a10f327d9e")
ICoreWebView2Controller4 : public ICoreWebView2Controller3 {
  public:
    virtual HRESULT STDMETHODCALLTYPE get_AllowExternalDrop(BOOL * value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AllowExternalDrop(BOOL value) = 0;
};
#endif // __ICoreWebView2Controller4_INTERFACE_DEFINED__

// IDropTarget that forwards a file drop (CF_HDROP) to a target window as a
// WM_DROPFILES message. Used so file drops over a WebView2 (with external drop
// disabled) reach the host window's normal drop handling.
class ForwardingDropTarget : public IDropTarget {
    LONG refCount = 1;
    HWND forwardTo = nullptr;

  public:
    explicit ForwardingDropTarget(HWND forwardTo) : forwardTo(forwardTo) {}

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG res = InterlockedDecrement(&refCount);
        if (res == 0) {
            delete this;
        }
        return res;
    }

    static bool HasFiles(IDataObject* dataObj) {
        if (!dataObj) {
            return false;
        }
        FORMATETC fe = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        return dataObj->QueryGetData(&fe) == S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* dataObj, DWORD, POINTL, DWORD* pdwEffect) override {
        *pdwEffect = HasFiles(dataObj) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* dataObj, DWORD, POINTL, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_COPY;
        if (!dataObj || !forwardTo) {
            return S_OK;
        }
        FORMATETC fe = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM stg{};
        if (FAILED(dataObj->GetData(&fe, &stg))) {
            return S_OK;
        }
        // The CF_HDROP medium's hGlobal is the HDROP handle; pass it directly
        // (DragQueryFile locks it internally). lp=1 means dragFinish=false so the
        // handler doesn't DragFinish() it - ReleaseStgMedium frees it instead.
        if (stg.hGlobal) {
            SendMessageW(forwardTo, WM_DROPFILES, (WPARAM)stg.hGlobal, 1);
        }
        ReleaseStgMedium(&stg);
        return S_OK;
    }
};

namespace {

enum class SharedWebViewEnvState {
    NotStarted,
    Creating,
    Ready,
    Failed,
};

SharedWebViewEnvState gSharedEnvState = SharedWebViewEnvState::NotStarted;
ICoreWebView2Environment* gSharedEnvironment = nullptr;
WStr gSharedUserDataFolder;
Vec<WebviewWnd*> gPendingWebviews;

void FreePendingOps(Vec<PendingWebViewOp>& ops) {
    for (PendingWebViewOp& op : ops) {
        str::Free(op.text);
    }
    ops.Reset();
}

void RemovePendingWebview(WebviewWnd* wv) {
    int i = gPendingWebviews.Find(wv);
    if (i >= 0) {
        gPendingWebviews.RemoveAt(i);
    }
}

static bool ShouldWebviewBeVisible(HWND hwnd) {
    if (!hwnd) {
        return false;
    }
    HWND parent = GetParent(hwnd);
    if (parent && !IsWindowVisible(parent)) {
        return false;
    }
    HWND root = GetAncestor(hwnd, GA_ROOT);
    if (root && IsIconic(root)) {
        return false;
    }
    return true;
}

} // namespace

class webview2_com_handler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                             public ICoreWebView2WebMessageReceivedEventHandler,
                             public ICoreWebView2PermissionRequestedEventHandler {
    using webview2_com_handler_cb_t = Func1<ICoreWebView2Controller*>;

  public:
    webview2_com_handler(HWND hwnd, WebViewMsgCb& msgCb, webview2_com_handler_cb_t cb)
        : m_window(hwnd), msgCb(msgCb), m_cb(cb) {}
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
        } else if (riid == __uuidof(ICoreWebView2WebMessageReceivedEventHandler)) {
            *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
        } else if (riid == __uuidof(ICoreWebView2PermissionRequestedEventHandler)) {
            *ppv = static_cast<ICoreWebView2PermissionRequestedEventHandler*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Controller* controller) {
        if (FAILED(res) || !controller) {
            m_cb.Call(nullptr);
            return E_FAIL;
        }

        controller->AddRef();

        ICoreWebView2* webview = nullptr;
        ::EventRegistrationToken token = {};
        controller->get_CoreWebView2(&webview);
        if (!webview) {
            controller->Release();
            m_cb.Call(nullptr);
            return E_FAIL;
        }
        webview->add_WebMessageReceived(this, &token);
        webview->add_PermissionRequested(this, &token);
        webview->Release();

        m_cb.Call(controller);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2WebMessageReceivedEventArgs* args) {
        WCHAR* message = nullptr;
        args->TryGetWebMessageAsString(&message);
        if (!message) {
            return S_OK;
        }
        TempStr s = ToUtf8Temp(message);
        msgCb.Call(s);
        CoTaskMemFree(message);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2PermissionRequestedEventArgs* args) {
        COREWEBVIEW2_PERMISSION_KIND kind;
        args->get_PermissionKind(&kind);
        if (kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ) {
            args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
        } else {
            args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
        }
        return S_OK;
    }

  private:
    HWND m_window;
    WebViewMsgCb msgCb;
    webview2_com_handler_cb_t m_cb;
    ULONG m_refCount = 1;
};

// Intercept accelerator keys so the host can handle app shortcuts.
class webview2_accel_handler : public ICoreWebView2AcceleratorKeyPressedEventHandler {
  public:
    webview2_accel_handler(HWND hwnd, WebviewWnd* wnd) : m_hwnd(hwnd), m_wnd(wnd) {}
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2AcceleratorKeyPressedEventHandler)) {
            *ppv = static_cast<ICoreWebView2AcceleratorKeyPressedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2Controller* /*sender*/,
                                     ICoreWebView2AcceleratorKeyPressedEventArgs* args) {
        if (!args) {
            return S_OK;
        }
        COREWEBVIEW2_KEY_EVENT_KIND kind;
        if (FAILED(args->get_KeyEventKind(&kind))) {
            return S_OK;
        }
        if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN) {
            return S_OK;
        }

        UINT vk = 0;
        if (FAILED(args->get_VirtualKey(&vk))) {
            return S_OK;
        }

        if (m_wnd && m_wnd->forwardAppAccelerators && m_wnd->events.resolveAccelCmd) {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            int cmd = m_wnd->events.resolveAccelCmd(m_wnd->events.ctx, (u16)vk, ctrl, shift, alt);
            if (cmd != 0) {
                args->put_Handled(TRUE);
                HWND root = GetAncestor(m_hwnd, GA_ROOT);
                if (root && ::IsWindow(root)) {
                    if (cmd == kWebViewForwardKey) {
                        // let the frame's key handler process it (e.g. Esc)
                        PostMessageW(root, WM_KEYDOWN, (WPARAM)vk, 0);
                        PostMessageW(root, WM_KEYUP, (WPARAM)vk, 0);
                    } else {
                        // invoke the command directly (avoids relying on the
                        // posted key still carrying its modifier state)
                        PostMessageW(root, WM_COMMAND, (WPARAM)cmd, 0);
                    }
                }
                return S_OK;
            }
        }
        return S_OK;
    }

  private:
    ULONG m_refCount = 1;
    HWND m_hwnd = nullptr;
    WebviewWnd* m_wnd = nullptr;
};

static TempWStr UriPathFromPrefix(WStr uri, WStr prefix);

static TempStr UrlForWebViewEvent(WStr uri, WStr prefix) {
    if (!uri) {
        return {};
    }
    if (prefix && wstr::StartsWith(uri, prefix)) {
        TempWStr pathW = UriPathFromPrefix(uri, prefix);
        if (!pathW) {
            return {};
        }
        TempStr path = ToUtf8Temp(pathW);
        wstr::Free(pathW);
        return path;
    }
    return ToUtf8Temp(uri);
}

static void UpdateWebViewHistory(WebviewWnd* wnd) {
    if (!wnd || !wnd->events.historyChanged || !wnd->webview) {
        return;
    }
    BOOL canGoBack = FALSE;
    BOOL canGoForward = FALSE;
    wnd->webview->get_CanGoBack(&canGoBack);
    wnd->webview->get_CanGoForward(&canGoForward);
    wnd->events.historyChanged(wnd->events.ctx, canGoBack != FALSE, canGoForward != FALSE);
}

class webview2_navigation_starting_handler : public ICoreWebView2NavigationStartingEventHandler {
  public:
    explicit webview2_navigation_starting_handler(WebviewWnd* wnd) : m_wnd(wnd) {}
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2NavigationStartingEventHandler)) {
            *ppv = static_cast<ICoreWebView2NavigationStartingEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2NavigationStartingEventArgs* args) {
        if (!args || !m_wnd || !m_wnd->events.navigationStarting) {
            return S_OK;
        }
        WCHAR* uri = nullptr;
        if (FAILED(args->get_Uri(&uri)) || !uri) {
            return S_OK;
        }
        TempStr url = UrlForWebViewEvent(WStr(uri), m_wnd->resourceUriPrefix);
        CoTaskMemFree(uri);
        if (!url) {
            return S_OK;
        }
        bool allow = m_wnd->events.navigationStarting(m_wnd->events.ctx, url, false);
        if (!allow) {
            args->put_Cancel(TRUE);
        }
        return S_OK;
    }

  private:
    WebviewWnd* m_wnd = nullptr;
    ULONG m_refCount = 1;
};

class webview2_navigation_completed_handler : public ICoreWebView2NavigationCompletedEventHandler {
  public:
    explicit webview2_navigation_completed_handler(WebviewWnd* wnd) : m_wnd(wnd) {}
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2NavigationCompletedEventHandler)) {
            *ppv = static_cast<ICoreWebView2NavigationCompletedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2NavigationCompletedEventArgs* args) {
        if (!args || !m_wnd || !m_wnd->events.navigationCompleted) {
            return S_OK;
        }
        BOOL success = FALSE;
        args->get_IsSuccess(&success);
        WCHAR* uri = nullptr;
        ICoreWebView2* webview = m_wnd->webview;
        if (webview) {
            webview->get_Source(&uri);
        }
        TempStr url = UrlForWebViewEvent(WStr(uri), m_wnd->resourceUriPrefix);
        if (uri) {
            CoTaskMemFree(uri);
        }
        if (url) {
            m_wnd->events.navigationCompleted(m_wnd->events.ctx, url, success != FALSE);
        }
        UpdateWebViewHistory(m_wnd);
        return S_OK;
    }

  private:
    WebviewWnd* m_wnd = nullptr;
    ULONG m_refCount = 1;
};

class webview2_history_changed_handler : public ICoreWebView2HistoryChangedEventHandler {
  public:
    explicit webview2_history_changed_handler(WebviewWnd* wnd) : m_wnd(wnd) {}
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2HistoryChangedEventHandler)) {
            *ppv = static_cast<ICoreWebView2HistoryChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, IUnknown* /*args*/) {
        UpdateWebViewHistory(m_wnd);
        return S_OK;
    }

  private:
    WebviewWnd* m_wnd = nullptr;
    ULONG m_refCount = 1;
};

class webview2_new_window_handler : public ICoreWebView2NewWindowRequestedEventHandler {
  public:
    explicit webview2_new_window_handler(WebviewWnd* wnd) : m_wnd(wnd) {}
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2NewWindowRequestedEventHandler)) {
            *ppv = static_cast<ICoreWebView2NewWindowRequestedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2NewWindowRequestedEventArgs* args) {
        if (!args || !m_wnd || !m_wnd->events.navigationStarting) {
            return S_OK;
        }
        WCHAR* uri = nullptr;
        if (FAILED(args->get_Uri(&uri)) || !uri) {
            return S_OK;
        }
        TempStr url = UrlForWebViewEvent(WStr(uri), m_wnd->resourceUriPrefix);
        CoTaskMemFree(uri);
        args->put_Handled(TRUE);
        if (url) {
            m_wnd->events.navigationStarting(m_wnd->events.ctx, url, true);
        }
        return S_OK;
    }

  private:
    WebviewWnd* m_wnd = nullptr;
    ULONG m_refCount = 1;
};

class webview2_env_handler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
  public:
    using env_ready_cb_t = void (*)(HRESULT, ICoreWebView2Environment*);

    explicit webview2_env_handler(env_ready_cb_t cb) : m_cb(cb) {}

    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Environment* env) {
        if (m_cb) {
            m_cb(res, env);
        }
        return S_OK;
    }

  private:
    env_ready_cb_t m_cb;
    ULONG m_refCount = 1;
};

static TempWStr MimeHeaderFromContentType(Str contentType) {
    if (len(contentType) == 0) {
        contentType = "text/html";
    }
    TempWStr contentTypeW = ToWStrTemp(contentType);
    return str::JoinTemp(WStrL(L"Content-Type: "), contentTypeW);
}

static TempWStr UriPathFromPrefix(WStr uri, WStr prefix) {
    if (!uri || !prefix || !wstr::StartsWith(uri, prefix)) {
        return {};
    }
    int pathOff = prefix.len;
    while (pathOff < uri.len && uri.s[pathOff] == L'/') {
        pathOff++;
    }
    if (pathOff >= uri.len) {
        return {};
    }
    WStr path = WStr(uri.s + pathOff, uri.len - pathOff);
    int q = wstr::IndexOfChar(path, L'?');
    if (q >= 0) {
        path = WStr(path.s, q);
    }
    int h = wstr::IndexOfChar(path, L'#');
    if (h >= 0) {
        path = WStr(path.s, h);
    }
    return wstr::Dup(path);
}

static bool CreateWebResourceResponseFromData(ICoreWebView2WebResourceRequestedEventArgs* args, Str data,
                                              Str contentType, int statusCode) {
    if (!args || !gSharedEnvironment) {
        return false;
    }

    IStream* stream = nullptr;
    if (len(data) > 0) {
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (size_t)data.len);
        if (!hMem) {
            return false;
        }
        void* mem = GlobalLock(hMem);
        if (!mem) {
            GlobalFree(hMem);
            return false;
        }
        memcpy(mem, (u8*)data.s, (size_t)data.len);
        GlobalUnlock(hMem);
        HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &stream);
        if (FAILED(hr)) {
            return false;
        }
    }

    TempWStr headers = MimeHeaderFromContentType(contentType);
    ICoreWebView2WebResourceResponse* response = nullptr;
    HRESULT hr = gSharedEnvironment->CreateWebResourceResponse(
        stream, statusCode, statusCode == 200 ? L"OK" : L"Not Found", headers.s, &response);
    if (stream) {
        stream->Release();
    }
    if (FAILED(hr) || !response) {
        return false;
    }
    args->put_Response(response);
    response->Release();
    return true;
}

class webview2_resource_handler : public ICoreWebView2WebResourceRequestedEventHandler {
  public:
    explicit webview2_resource_handler(WebviewWnd* wnd) : m_wnd(wnd) {}

    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2WebResourceRequestedEventHandler)) {
            *ppv = static_cast<ICoreWebView2WebResourceRequestedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2WebResourceRequestedEventArgs* args) {
        if (!args || !m_wnd || !m_wnd->resourceProvider.getResource || !m_wnd->resourceUriPrefix) {
            return S_OK;
        }

        ICoreWebView2WebResourceRequest* request = nullptr;
        HRESULT hr = args->get_Request(&request);
        if (FAILED(hr) || !request) {
            return S_OK;
        }

        WCHAR* uri = nullptr;
        hr = request->get_Uri(&uri);
        request->Release();
        if (FAILED(hr) || !uri) {
            return S_OK;
        }

        TempWStr pathW = UriPathFromPrefix(WStr(uri), m_wnd->resourceUriPrefix);
        CoTaskMemFree(uri);
        if (!pathW) {
            return S_OK;
        }

        TempStr path = ToUtf8Temp(pathW);
        wstr::Free(pathW);
        WebViewResourceResult res;
        if (!m_wnd->resourceProvider.getResource(m_wnd->resourceProvider.ctx, path, &res)) {
            CreateWebResourceResponseFromData(args, {}, "text/plain", 404);
            return S_OK;
        }

        CreateWebResourceResponseFromData(args, Str((char*)res.data, (int)res.dataLen), res.contentType, 200);
        if (res.ownsData) {
            free((void*)res.data);
        }
        str::Free(res.contentType);
        return S_OK;
    }

  private:
    WebviewWnd* m_wnd = nullptr;
    ULONG m_refCount = 1;
};

class webview2_try_suspend_handler : public ICoreWebView2TrySuspendCompletedHandler {
  public:
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }

    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2TrySuspendCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2TrySuspendCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT /*errorCode*/, BOOL /*isSuccessful*/) { return S_OK; }

  private:
    ULONG m_refCount = 1;
};

// trivial completion handler for ICoreWebView2Find::Start (we don't need the result)
class webview2_find_start_handler : public ICoreWebView2FindStartCompletedHandler {
  public:
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG n = --m_refCount;
        if (n == 0) {
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2FindStartCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2FindStartCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT /*errorCode*/) { return S_OK; }

  private:
    ULONG m_refCount = 1;
};

WebviewWnd::WebviewWnd() {
    kind = kindWebView;
}

void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind kind, Str text) {
    if (initFailed) {
        return;
    }
    PendingWebViewOp op;
    op.kind = kind;
    op.text = str::Dup(text ? text : StrL(""));
    pendingOps.Append(op);
}

void WebviewWnd::FlushPendingOps() {
    if (!webview || initFailed) {
        return;
    }
    // Vec copies PendingWebViewOp by value (shallow copy of text pointers), so only
    // free op.text once -- in the loop below.
    Vec<PendingWebViewOp> ops = pendingOps;
    pendingOps.Reset();
    for (PendingWebViewOp& op : ops) {
        switch (op.kind) {
            case PendingWebViewOp::Init:
                Init(op.text);
                break;
            case PendingWebViewOp::SetHtml:
                SetHtml(op.text);
                break;
            case PendingWebViewOp::Eval:
                Eval(op.text);
                break;
            case PendingWebViewOp::Navigate:
                Navigate(op.text);
                break;
        }
        str::Free(op.text);
    }
}

void WebviewWnd::FailInit() {
    initFailed = true;
    FreePendingOps(pendingOps);
    RemovePendingWebview(this);
    if (webview) {
        webview->Release();
        webview = nullptr;
    }
    if (controller) {
        controller->Release();
        controller = nullptr;
    }
    HwndDestroyWindowSafe(&hwnd);
}

void WebviewWnd::SetControllerVisible(bool visible) {
    if (visible == isVisible) {
        if ((visible && !isSuspended) || (!visible && isSuspended)) {
            return;
        }
    }

    isVisible = visible;
    if (controller) {
        controller->put_IsVisible(visible ? TRUE : FALSE);
    }
    if (!webview) {
        return;
    }

    ICoreWebView2_3* webview3 = nullptr;
    HRESULT hr = webview->QueryInterface(IID_PPV_ARGS(&webview3));
    if (FAILED(hr) || !webview3) {
        return;
    }

    if (visible) {
        if (isSuspended) {
            webview3->Resume();
            isSuspended = false;
        }
    } else if (!isSuspended) {
        auto* handler = new webview2_try_suspend_handler();
        hr = webview3->TrySuspend(handler);
        handler->Release();
        if (SUCCEEDED(hr)) {
            isSuspended = true;
        }
    }
    webview3->Release();
}

void WebviewWnd::OnControllerReady(ICoreWebView2Controller* ctrl) {
    if (!ctrl) {
        FailInit();
        return;
    }
    controller = ctrl;

    auto style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_OVERLAPPEDWINDOW);
    SetWindowLong(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    isVisible = false;
    controller->put_IsVisible(FALSE);

    ICoreWebView2Controller2* controller2 = nullptr;
    HRESULT bgHr = controller->QueryInterface(IID_PPV_ARGS(&controller2));
    if (SUCCEEDED(bgHr) && controller2) {
        COREWEBVIEW2_COLOR bg = {0, 0, 0, 0};
        controller2->put_DefaultBackgroundColor(bg);
        controller2->Release();
    }

    if (!allowExternalDrop) {
        ICoreWebView2Controller4* controller4 = nullptr;
        HRESULT dropHr = controller->QueryInterface(IID_PPV_ARGS(&controller4));
        if (SUCCEEDED(dropHr) && controller4) {
            controller4->put_AllowExternalDrop(FALSE);
            controller4->Release();
        }
        // With external drop disabled, WebView2 routes drops to the IDropTarget
        // registered on its host hwnd. Register one that forwards dropped files to
        // the parent window (the canvas), which opens them like a normal file drop.
        RegisterForwardingDropTarget();
    }

    isSuspended = false;
    RECT bounds = ClientRECT(hwnd);
    controller->put_Bounds(bounds);
    lastBounds = bounds;
    hasLastBounds = true;
    controller->get_CoreWebView2(&webview);
    if (!webview) {
        FailInit();
        return;
    }

    ICoreWebView2Settings* settings = nullptr;
    HRESULT hr = webview->get_Settings(&settings);
    if (hr == S_OK && settings) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
        settings->Release();
    }

    {
        webview2_accel_handler* accelHandler = new webview2_accel_handler(hwnd, this);
        ::EventRegistrationToken token = {};
        if (SUCCEEDED(controller->add_AcceleratorKeyPressed(accelHandler, &token))) {
            accelHandler->Release();
        } else {
            accelHandler->Release();
        }
    }

    if (resourceProvider.getResource && resourceUriPrefix) {
        TempWStr filter = str::JoinTemp(resourceUriPrefix, WStrL(L"*"));
        webview->AddWebResourceRequestedFilter(filter.s, COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        auto* resourceHandler = new webview2_resource_handler(this);
        ::EventRegistrationToken resourceToken = {};
        if (SUCCEEDED(webview->add_WebResourceRequested(resourceHandler, &resourceToken))) {
            resourceHandler->Release();
        } else {
            resourceHandler->Release();
        }
    }

    if (events.navigationStarting || events.navigationCompleted || events.historyChanged) {
        ::EventRegistrationToken token = {};
        if (events.navigationStarting) {
            auto* handler = new webview2_navigation_starting_handler(this);
            if (SUCCEEDED(webview->add_NavigationStarting(handler, &token))) {
                handler->Release();
            } else {
                handler->Release();
            }
            auto* newWindowHandler = new webview2_new_window_handler(this);
            if (SUCCEEDED(webview->add_NewWindowRequested(newWindowHandler, &token))) {
                newWindowHandler->Release();
            } else {
                newWindowHandler->Release();
            }
        }
        if (events.navigationCompleted) {
            auto* handler = new webview2_navigation_completed_handler(this);
            if (SUCCEEDED(webview->add_NavigationCompleted(handler, &token))) {
                handler->Release();
            } else {
                handler->Release();
            }
        }
        if (events.historyChanged) {
            auto* handler = new webview2_history_changed_handler(this);
            if (SUCCEEDED(webview->add_HistoryChanged(handler, &token))) {
                handler->Release();
            } else {
                handler->Release();
            }
        }
    }

    Init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
    initStarted = true;
    FlushPendingOps();

    bool wantVisible = ShouldWebviewBeVisible(hwnd);
    if (wantVisible) {
        ::ShowWindow(hwnd, SW_SHOW);
    }
    SetControllerVisible(wantVisible);
}

void WebviewWnd::UpdateWebviewSize() {
    if (controller == nullptr) {
        return;
    }
    RECT bounds = ClientRECT(hwnd);
    if (hasLastBounds && EqualRect(&bounds, &lastBounds)) {
        return;
    }
    lastBounds = bounds;
    hasLastBounds = true;
    controller->put_Bounds(bounds);
}

void WebviewWnd::Eval(Str js) {
    if (initFailed || len(js) == 0) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::Eval, js);
        return;
    }
    WCHAR* ws = CWStrTemp(js);
    webview->ExecuteScript(ws, nullptr);
}

void WebviewWnd::SetHtml(Str html) {
    if (initFailed || len(html) == 0) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::SetHtml, html);
        return;
    }
    WCHAR* html2 = CWStrTemp(html);
    webview->NavigateToString(html2);
}

void WebviewWnd::Init(Str js) {
    if (initFailed || len(js) == 0) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::Init, js);
        return;
    }
    WCHAR* ws = CWStrTemp(js);
    webview->AddScriptToExecuteOnDocumentCreated(ws, nullptr);
}

void WebviewWnd::Navigate(Str url) {
    if (initFailed || len(url) == 0) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::Navigate, url);
        return;
    }
    WCHAR* ws = CWStrTemp(url);
    webview->Navigate(ws);
}

void WebviewWnd::GoBack() {
    if (webview) {
        webview->GoBack();
    }
}

void WebviewWnd::GoForward() {
    if (webview) {
        webview->GoForward();
    }
}

void WebviewWnd::SetZoomPercent(int zoom) {
    if (controller) {
        double factor = (double)zoom / 100.0;
        controller->put_ZoomFactor(factor);
    }
}

int WebviewWnd::GetZoomPercent() const {
    if (!controller) {
        return 100;
    }
    double factor = 1.0;
    controller->get_ZoomFactor(&factor);
    return (int)(factor * 100.0 + 0.5);
}

bool WebviewWnd::CanGoBack() const {
    if (!webview) {
        return false;
    }
    BOOL canGoBack = FALSE;
    webview->get_CanGoBack(&canGoBack);
    return canGoBack != FALSE;
}

bool WebviewWnd::CanGoForward() const {
    if (!webview) {
        return false;
    }
    BOOL canGoForward = FALSE;
    webview->get_CanGoForward(&canGoForward);
    return canGoForward != FALSE;
}

void WebviewWnd::Focus() {
    if (!hwnd) {
        return;
    }
    ::SetFocus(hwnd);
    if (controller) {
        controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

void WebviewWnd::ShowFindUI() {
    if (!webview || !gSharedEnvironment) {
        return;
    }
    Focus();
    // The find API (ICoreWebView2_28::get_Find) needs a recent enough WebView2
    // runtime; QueryInterface fails gracefully on older ones.
    ICoreWebView2_28* wv28 = nullptr;
    if (FAILED(webview->QueryInterface(IID_PPV_ARGS(&wv28))) || !wv28) {
        return;
    }
    ICoreWebView2Environment15* env15 = nullptr;
    ICoreWebView2Find* find = nullptr;
    ICoreWebView2FindOptions* opts = nullptr;
    if (SUCCEEDED(gSharedEnvironment->QueryInterface(IID_PPV_ARGS(&env15))) && env15) {
        env15->CreateFindOptions(&opts);
        env15->Release();
    }
    wv28->get_Find(&find);
    wv28->Release();
    if (find && opts) {
        // show the browser's own find bar (don't suppress it); empty term opens
        // the bar without a query, like pressing Ctrl+F in a browser
        opts->put_SuppressDefaultFindDialog(FALSE);
        opts->put_FindTerm(L"");
        auto* handler = new webview2_find_start_handler();
        find->Start(opts, handler);
        handler->Release();
    }
    if (opts) {
        opts->Release();
    }
    if (find) {
        find->Release();
    }
}

static BOOL CALLBACK CollectChildHwnds(HWND hwnd, LPARAM lp) {
    auto* hwnds = (Vec<HWND>*)lp;
    hwnds->Append(hwnd);
    return TRUE;
}

// Register `target` on `hwnd` (replacing any existing drop target). Records the
// window in `registered` on success so it can be revoked later.
static void RegisterDropOn(HWND hwnd, IDropTarget* target, Vec<HWND>& registered) {
    if (registered.Contains(hwnd)) {
        return;
    }
    RevokeDragDrop(hwnd);
    if (SUCCEEDED(RegisterDragDrop(hwnd, target))) {
        registered.Append(hwnd);
    }
}

// Forward file drops over the WebView2 to the parent window. The drop is
// actually received by the WebView2's Chrome_* child windows (which sit on top
// of the host hwnd), so register on the host hwnd and all of its descendants.
// Called again after each navigation because the child windows are created
// lazily and can be recreated.
void WebviewWnd::RegisterForwardingDropTarget() {
    if (allowExternalDrop || !hwnd) {
        return;
    }
    HWND parent = GetParent(hwnd);
    if (!parent) {
        return;
    }
    if (!dropTarget) {
        dropTarget = new ForwardingDropTarget(parent);
    }
    Vec<HWND> wnds;
    wnds.Append(hwnd);
    EnumChildWindows(hwnd, CollectChildHwnds, (LPARAM)&wnds);
    for (HWND h : wnds) {
        RegisterDropOn(h, dropTarget, dropTargetHwnds);
    }
}

void WebviewWnd::RevokeForwardingDropTarget() {
    for (HWND h : dropTargetHwnds) {
        RevokeDragDrop(h);
    }
    dropTargetHwnds.Reset();
    if (dropTarget) {
        dropTarget->Release();
        dropTarget = nullptr;
    }
}

static void ComHandlerCbHwnd(void* hwndVoid, ICoreWebView2Controller* ctrl) {
    HWND hwnd = (HWND)hwndVoid;
    auto* self = (WebviewWnd*)WndListFindByHwnd(hwnd);
    if (!self) {
        if (ctrl) {
            ctrl->Release();
        }
        return;
    }
    if (!ctrl) {
        self->FailInit();
        return;
    }
    self->OnControllerReady(ctrl);
}

static void OnBrowserMessageCbHwnd(void* hwndVoid, Str msg);

static void FailPendingWebviews() {
    Vec<WebviewWnd*> pending = gPendingWebviews;
    gPendingWebviews.Reset();
    for (WebviewWnd* wv : pending) {
        if (wv && !wv->initFailed) {
            wv->FailInit();
        }
    }
}

static void CreateControllerWithSharedEnvironment(WebviewWnd* self, WebViewMsgCb& cb) {
    if (!self || !gSharedEnvironment) {
        if (self) {
            self->FailInit();
        }
        return;
    }
    HWND hwnd = self->hwnd;
    auto fn = MkFunc1<void, ICoreWebView2Controller*>(ComHandlerCbHwnd, (void*)hwnd);
    auto* handler = new webview2_com_handler(hwnd, cb, fn);
    HRESULT hr = gSharedEnvironment->CreateCoreWebView2Controller(self->hwnd, handler);
    handler->Release();
    if (FAILED(hr)) {
        self->FailInit();
    }
}

static void OnSharedEnvironmentReady(HRESULT res, ICoreWebView2Environment* env) {
    if (FAILED(res) || !env) {
        gSharedEnvState = SharedWebViewEnvState::Failed;
        FailPendingWebviews();
        return;
    }

    env->AddRef();
    gSharedEnvironment = env;
    gSharedEnvState = SharedWebViewEnvState::Ready;

    Vec<WebviewWnd*> pending = gPendingWebviews;
    gPendingWebviews.Reset();
    for (WebviewWnd* wv : pending) {
        if (!wv || wv->initFailed) {
            continue;
        }
        HWND hwnd = wv->hwnd;
        auto fn = MkFunc1<void, Str>(OnBrowserMessageCbHwnd, (void*)hwnd);
        CreateControllerWithSharedEnvironment(wv, fn);
    }
}

static Microsoft::WRL::ComPtr<CoreWebView2EnvironmentOptions> CreateOfflineEnvironmentOptions() {
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(
        L"--disable-background-networking "
        L"--disable-component-update "
        L"--disable-component-extensions-with-background-pages "
        L"--disable-extensions "
        L"--disable-domain-reliability "
        L"--disable-renderer-backgrounding "
        L"--disable-backgrounding-occluded-windows "
        L"--disable-breakpad "
        L"--disable-sync "
        L"--disable-default-apps "
        L"--disable-features=AutofillServerCommunication,MediaRouter,OptimizationHints,Translate,"
        L"CertificateTransparencyComponentUpdater "
        L"--metrics-recording-only "
        L"--no-pings");
    return options;
}

bool WebviewWnd::Embed(WebViewMsgCb& cb) {
    if (initStarted) {
        return !initFailed;
    }
    if (!dataDir) {
        logf("WebviewWnd::Embed: dataDir is null, aborting\n");
        initFailed = true;
        return false;
    }
    initStarted = true;
    wstr::Free(userDataFolder);
    userDataFolder = wstr::Dup(ToWStrTemp(dataDir));

    if (gSharedEnvState == SharedWebViewEnvState::Ready && gSharedEnvironment) {
        CreateControllerWithSharedEnvironment(this, cb);
        return !initFailed;
    }

    if (gSharedEnvState == SharedWebViewEnvState::Failed) {
        initFailed = true;
        return false;
    }

    gPendingWebviews.Append(this);

    if (gSharedEnvState == SharedWebViewEnvState::NotStarted) {
        gSharedEnvState = SharedWebViewEnvState::Creating;
        wstr::Free(gSharedUserDataFolder);
        gSharedUserDataFolder = wstr::Dup(userDataFolder);
        auto options = CreateOfflineEnvironmentOptions();
        auto* envHandler = new webview2_env_handler(OnSharedEnvironmentReady);
        HRESULT hr =
            CreateCoreWebView2EnvironmentWithOptions(nullptr, gSharedUserDataFolder.s, options.Get(), envHandler);
        envHandler->Release();
        if (FAILED(hr)) {
            gSharedEnvState = SharedWebViewEnvState::Failed;
            FailPendingWebviews();
            return false;
        }
    } else if (gSharedEnvState == SharedWebViewEnvState::Creating) {
        if (!wstr::Eq(userDataFolder, gSharedUserDataFolder)) {
            logf("WebviewWnd::Embed: reusing shared WebView2 environment with first dataDir\n");
        }
    }

    return true;
}

void WebviewWnd::OnBrowserMessage(Str msg) {
    log(msg);
}

static void OnBrowserMessageCbHwnd(void* hwndVoid, Str msg) {
    HWND hwnd = (HWND)hwndVoid;
    auto* self = (WebviewWnd*)WndListFindByHwnd(hwnd);
    if (self) {
        self->OnBrowserMessage(msg);
    }
}

HWND WebviewWnd::Create(const CreateWebViewArgs& args) {
    ReportIf(!dataDir);
    CreateCustomArgs cargs;
    cargs.parent = args.parent;
    cargs.pos = args.pos;
    // Child-only style: default CreateCustom uses overlapped-window chrome
    // (caption, sysmenu, etc.) which flashes briefly before the controller embeds.
    cargs.style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    cargs.visible = false;
    isVisible = false;
    CreateCustom(cargs);
    if (!hwnd) {
        return nullptr;
    }

    auto fn = MkFunc1<void, Str>(OnBrowserMessageCbHwnd, (void*)hwnd);
    if (!Embed(fn)) {
        HwndDestroyWindowSafe(&hwnd);
        return nullptr;
    }
    UpdateWebviewSize();
    return hwnd;
}

LRESULT WebviewWnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_ENTERSIZEMOVE) {
        isInSizeMove = true;
        Eval("if (window.__setHostResizing) window.__setHostResizing(true);");
    } else if (msg == WM_EXITSIZEMOVE) {
        isInSizeMove = false;
        Eval("if (window.__setHostResizing) window.__setHostResizing(false);");
        UpdateWebviewSize();
    } else if (msg == WM_SIZE) {
        SetControllerVisible(wparam != SIZE_MINIMIZED);
        if (!isInSizeMove) {
            UpdateWebviewSize();
        }
    } else if (msg == WM_SHOWWINDOW) {
        SetControllerVisible(wparam != FALSE);
        UpdateWebviewSize();
    } else if (msg == WM_ACTIVATE) {
        if (wparam == WA_INACTIVE) {
            SetControllerVisible(false);
        } else {
            SetControllerVisible(true);
            UpdateWebviewSize();
        }
    }
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

WebviewWnd::~WebviewWnd() {
    RemovePendingWebview(this);
    FreePendingOps(pendingOps);
    RevokeForwardingDropTarget();
    if (webview) {
        webview->Release();
        webview = nullptr;
    }
    if (controller) {
        controller->Release();
        controller = nullptr;
    }
    str::Free(dataDir);
    wstr::Free(userDataFolder);
    wstr::Free(resourceUriPrefix);
}

#endif // _MSC_VER

#ifndef _MSC_VER
// stub implementations for mingw cross-compile / wine (no webview2)
WebviewWnd::WebviewWnd() = default;
WebviewWnd::~WebviewWnd() {
    str::Free(dataDir);
    wstr::Free(userDataFolder);
    wstr::Free(resourceUriPrefix);
}
HWND WebviewWnd::Create(const CreateWebViewArgs&) {
    return nullptr;
}
void WebviewWnd::Eval(Str) {}
void WebviewWnd::SetHtml(Str) {}
void WebviewWnd::Init(Str) {}
void WebviewWnd::Navigate(Str) {}
void WebviewWnd::RegisterForwardingDropTarget() {}
void WebviewWnd::RevokeForwardingDropTarget() {}
void WebviewWnd::GoBack() {}
void WebviewWnd::GoForward() {}
void WebviewWnd::SetZoomPercent(int) {}
int WebviewWnd::GetZoomPercent() const {
    return 100;
}
bool WebviewWnd::CanGoBack() const {
    return false;
}
bool WebviewWnd::CanGoForward() const {
    return false;
}
void WebviewWnd::Focus() {}
void WebviewWnd::ShowFindUI() {}
bool WebviewWnd::Embed(WebViewMsgCb&) {
    return false;
}
void WebviewWnd::OnControllerReady(ICoreWebView2Controller*) {}
void WebviewWnd::FailInit() {}
void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind, Str) {}
void WebviewWnd::FlushPendingOps() {}
void WebviewWnd::SetControllerVisible(bool) {}
void WebviewWnd::OnBrowserMessage(Str) {}
LRESULT WebviewWnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return WndProcDefault(hwnd, msg, wparam, lparam);
}
void WebviewWnd::UpdateWebviewSize() {}
#endif // !_MSC_VER