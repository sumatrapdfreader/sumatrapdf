/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/JsonParser.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/GuiColors.h"

// #include "Theme.h"

#ifdef _MSC_VER
#include "WebView2EnvironmentOptions.h"
#endif
#include "gui/win/WebView.h"

static Kind kindWebView = "webView";

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

static bool IsWebViewAvailable() {
    WCHAR* ver = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
    if (FAILED(hr) || len(ver) == 0) {
        logf("WebView2 is not available\n");
        return false;
    }
    return true;
}

bool HasWebView() {
    // the runtime's availability doesn't change while we're running
    static bool hasWebView = IsWebViewAvailable();
    return hasWebView;
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
    AtomicInt refCount = 1;
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
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
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

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* dataObj, DWORD /*grfKeyState*/, POINTL /*pt*/,
                                        DWORD* pdwEffect) override {
        *pdwEffect = HasFiles(dataObj) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD* pdwEffect) override {
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* dataObj, DWORD /*grfKeyState*/, POINTL /*pt*/,
                                   DWORD* pdwEffect) override {
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

// defined below, after the shared-environment state they operate on
static void ScheduleEnvCreateRetry();
static void ResetSharedEnvironment();

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
bool gWebViewShuttingDown = false;

// Creating the environment fails with HRESULT_FROM_WIN32(ERROR_INVALID_STATE) when
// another process is already using the same user data folder with different
// EnvironmentOptions -- e.g. a second SumatraPDF from before/after an upgrade, since
// we pass our own AdditionalBrowserArguments. It's transient (the other instance
// exits), so retry a few times before giving up.
constexpr int kMaxEnvCreateAttempts = 10;
constexpr DWORD kEnvCreateRetryDelayMs = 300;
// after giving up, let a later Embed() start over rather than leaving every webview
// in the process dead until restart
constexpr DWORD kEnvFailedCooldownMs = 10 * 1000;

int gEnvCreateAttempts = 0;
DWORD gEnvFailedAtMs = 0;
UINT_PTR gEnvRetryTimerId = 0;
HWND gEnvRetryTimerHwnd = nullptr;

constexpr UINT_PTR kEnvRetryTimerId = 0x5eb1;

void FreePendingOps(Vec<PendingWebViewOp>& ops) {
    for (PendingWebViewOp& op : ops) {
        str::Free(op.text);
    }
    VecReset(ops);
}

void RemovePendingWebview(WebviewWnd* wv) {
    int i = VecFind(gPendingWebviews, wv);
    if (i >= 0) {
        VecRemoveAt(gPendingWebviews, i);
    }
    // the retry timer lives on a pending webview's hwnd, so move it if this was
    // the one hosting it -- otherwise the timer dies with the window and the
    // shared environment stays stuck in Creating forever
    if (wv && wv->hwnd && wv->hwnd == gEnvRetryTimerHwnd) {
        ScheduleEnvCreateRetry();
    }
}

bool ShouldWebviewBeVisible(HWND hwnd) {
    if (!hwnd) {
        return false;
    }
    HWND parent = GetParent(hwnd);
    if (parent && !HwndIsVisible(parent)) {
        return false;
    }
    HWND root = GetAncestor(hwnd, GA_ROOT);
    if (root && IsIconic(root)) {
        return false;
    }
    return true;
}

} // namespace

// The JS half of the native-call bridge. window.__sumatra__.call(name, ...args)
// returns a promise that Resolve() settles. Params are JSON.stringify'd into a
// *string* field so the native side gets them as one opaque value -- our JSON
// parser is a push parser over primitives and can't hand back a subtree.
static const char* kJsBridgeScript = R"JS((function() {
  'use strict';
  if (window.__sumatra__) { return; }
  function genId() {
    var b = new Uint8Array(16);
    (window.crypto || window.msCrypto).getRandomValues(b);
    return Array.prototype.map.call(b, function(n) {
      var s = n.toString(16);
      return (s.length === 1 ? '0' : '') + s;
    }).join('');
  }
  var pending = {};
  var S = {};
  S.post = function(m) { window.chrome.webview.postMessage(m); };
  S.call = function(method) {
    var id = genId();
    var params = Array.prototype.slice.call(arguments, 1);
    var p = new Promise(function(resolve, reject) { pending[id] = {resolve: resolve, reject: reject}; });
    S.post(JSON.stringify({__sumatraCall: 1, id: id, method: method, params: JSON.stringify(params)}));
    return p;
  };
  S.onReply = function(id, status, result) {
    var pr = pending[id];
    if (!pr) { return; }
    delete pending[id];
    var v;
    if (result !== undefined && result !== null && result !== '') {
      try { v = JSON.parse(result); } catch (e) { pr.reject(new Error('bad JSON from host')); return; }
    }
    if (status === 0) { pr.resolve(v); } else { pr.reject(v); }
  };
  S.onBind = function(name) {
    if (window[name]) { return; }
    window[name] = function() {
      return S.call.apply(S, [name].concat(Array.prototype.slice.call(arguments)));
    };
  };
  S.onUnbind = function(name) { delete window[name]; };
  S.notify = function(method) {
    var params = Array.prototype.slice.call(arguments, 1);
    S.post(JSON.stringify({__sumatraNotify: 1, method: method, params: JSON.stringify(params)}));
  };
  window.__sumatra__ = S;
})())JS";

class webview2_com_handler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                             public ICoreWebView2WebMessageReceivedEventHandler,
                             public ICoreWebView2PermissionRequestedEventHandler {
    using webview2_com_handler_cb_t = Func1<ICoreWebView2Controller*>;

  public:
    webview2_com_handler(HWND hwnd, WebViewMsgCb& msgCb, webview2_com_handler_cb_t cb, bool allowClipboardRead)
        : m_window(hwnd), msgCb(msgCb), m_cb(cb), allowClipboardRead(allowClipboardRead) {}
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
        // NOLINTNEXTLINE(bugprone-branch-clone): each branch casts to a different base, not a clone
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
        if (kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ && allowClipboardRead) {
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
    bool allowClipboardRead;
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

        if (m_wnd && m_wnd->forwardAppAccelerators) {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            // WindowBase closeOnEsc / closeOnCtrlW: WebView2 eats keys before
            // PreTranslate, so honor the flags here when the host window set them
            HWND root = GetAncestor(m_hwnd, GA_ROOT);
            WindowBase* wb = root ? WindowBaseFromHwnd(root) : nullptr;
            if (wb) {
                if (wb->closeOnEsc && vk == VK_ESCAPE) {
                    args->put_Handled(TRUE);
                    wb->Close();
                    return S_OK;
                }
                if (wb->closeOnCtrlW && vk == 'W' && ctrl && !alt) {
                    args->put_Handled(TRUE);
                    wb->Close();
                    return S_OK;
                }
                if (wb->closeOnF1 && vk == VK_F1 && !ctrl && !shift && !alt) {
                    args->put_Handled(TRUE);
                    wb->Close();
                    return S_OK;
                }
            }
            if (!m_wnd->events.resolveAccelCmd) {
                return S_OK;
            }
            int cmd = m_wnd->events.resolveAccelCmd(m_wnd->events.ctx, (u16)vk, ctrl, shift, alt);
            if (cmd != 0) {
                args->put_Handled(TRUE);
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

static TempWStr UriPathFromPrefix(WStr uri, WStr prefix, bool keepQueryAndFragment = false);

static TempStr UrlForWebViewEvent(WStr uri, WStr prefix) {
    if (len(uri) == 0) {
        return {};
    }
    if (prefix && wstr::StartsWith(uri, prefix)) {
        // keep ?query and #fragment: navigation handlers (markdown heading
        // dests, CHM) need the hash; resource fetches strip it separately
        TempWStr pathW = UriPathFromPrefix(uri, prefix, true);
        if (len(pathW) == 0) {
            return {};
        }
        TempStr path = ToUtf8Temp(pathW);
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
        if (len(url) == 0) {
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

// Used when routeDownloadsToOsBrowser is set: cancel the in-webview download
// and open external http(s) URLs in the OS default browser. Virtual-host
// content (markdown/CHM) is not opened externally.
static bool ShouldOpenWebViewDownloadInOsBrowser(WStr uri, WStr resourceUriPrefix) {
    if (len(uri) == 0) {
        return false;
    }
    if (resourceUriPrefix && wstr::StartsWith(uri, resourceUriPrefix)) {
        return false;
    }
    TempStr url = ToUtf8Temp(uri);
    return str::StartsWithI(url, StrL("http://")) || str::StartsWithI(url, StrL("https://"));
}

class webview2_download_starting_handler : public ICoreWebView2DownloadStartingEventHandler {
  public:
    explicit webview2_download_starting_handler(WebviewWnd* wnd) : m_wnd(wnd) {}
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
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2DownloadStartingEventHandler)) {
            *ppv = static_cast<ICoreWebView2DownloadStartingEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2DownloadStartingEventArgs* args) {
        if (!args) {
            return S_OK;
        }
        // hide default download dialog and abort in-webview download
        args->put_Cancel(TRUE);
        args->put_Handled(TRUE);

        ICoreWebView2DownloadOperation* op = nullptr;
        if (FAILED(args->get_DownloadOperation(&op)) || !op) {
            return S_OK;
        }
        WCHAR* uri = nullptr;
        HRESULT hr = op->get_Uri(&uri);
        op->Release();
        if (FAILED(hr) || !uri) {
            return S_OK;
        }
        WStr uriW = WStr(uri);
        WStr prefix = m_wnd ? m_wnd->resourceUriPrefix : WStr{};
        if (ShouldOpenWebViewDownloadInOsBrowser(uriW, prefix)) {
            TempStr url = ToUtf8Temp(uriW);
            logf("WebView2: routing download to OS browser: '%s'\n", url);
            LaunchBrowser(url);
        } else {
            logf("WebView2: cancelled download (not opened externally)\n");
        }
        CoTaskMemFree(uri);
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
        contentType = StrL("text/html");
    }
    TempWStr contentTypeW = ToWStrTemp(contentType);
    return str::JoinTemp(WStrL(L"Content-Type: "), contentTypeW);
}

static TempWStr UriPathFromPrefix(WStr uri, WStr prefix, bool keepQueryAndFragment) {
    if (len(uri) == 0 || len(prefix) == 0 || !wstr::StartsWith(uri, prefix)) {
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
    if (!keepQueryAndFragment) {
        int q = wstr::IndexOfChar(path, L'?');
        if (q >= 0) {
            path = WStr(path.s, q);
        }
        int h = wstr::IndexOfChar(path, L'#');
        if (h >= 0) {
            path = WStr(path.s, h);
        }
    }
    return str::DupTemp(path);
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
        if (!args || !m_wnd || !m_wnd->resourceProvider.getResource || len(m_wnd->resourceUriPrefix) == 0) {
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
        if (len(pathW) == 0) {
            return S_OK;
        }

        TempStr path = ToUtf8Temp(pathW);
        WebViewResourceResult res;
        if (!m_wnd->resourceProvider.getResource(m_wnd->resourceProvider.ctx, path, &res)) {
            CreateWebResourceResponseFromData(args, {}, StrL("text/plain"), 404);
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

// Receives the id WebView2 assigns to a script passed to
// AddScriptToExecuteOnDocumentCreated. The id is the only handle to that script
// and is needed by RemoveScriptToExecuteOnDocumentCreated, so without capturing
// it here init scripts could never be removed or replaced. Looks the window up
// by hwnd because the completion is asynchronous and the window may be gone.
class webview2_add_script_handler : public ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler {
  public:
    webview2_add_script_handler(HWND hwnd, int token) : m_hwnd(hwnd), m_token(token) {}

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
        if (riid == IID_IUnknown ||
            riid == __uuidof(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, LPCWSTR id) {
        if (FAILED(res) || !id) {
            return S_OK;
        }
        auto* wnd = (WebviewWnd*)WindowBaseFromHwnd(m_hwnd);
        if (wnd) {
            wnd->OnInitScriptAdded(m_token, id);
        }
        return S_OK;
    }

  private:
    ULONG m_refCount = 1;
    HWND m_hwnd = nullptr;
    int m_token = 0;
};

// A WebView2 process died. Without this the control just goes blank forever:
// a dead renderer leaves nothing painting, and a dead browser process takes the
// whole environment with it.
class webview2_process_failed_handler : public ICoreWebView2ProcessFailedEventHandler {
  public:
    explicit webview2_process_failed_handler(HWND hwnd) : m_hwnd(hwnd) {}

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
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2ProcessFailedEventHandler)) {
            *ppv = static_cast<ICoreWebView2ProcessFailedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* /*sender*/, ICoreWebView2ProcessFailedEventArgs* args) {
        if (!args) {
            return S_OK;
        }
        COREWEBVIEW2_PROCESS_FAILED_KIND kind;
        if (FAILED(args->get_ProcessFailedKind(&kind))) {
            return S_OK;
        }
        auto* wnd = (WebviewWnd*)WindowBaseFromHwnd(m_hwnd);
        if (!wnd) {
            return S_OK;
        }
        WebViewProcessFailure f = WebViewProcessFailure::Other;
        if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED) {
            f = WebViewProcessFailure::BrowserExited;
        } else if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED) {
            f = WebViewProcessFailure::RenderExited;
        } else if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE) {
            f = WebViewProcessFailure::RenderUnresponsive;
        }
        wnd->OnProcessFailed(f);
        return S_OK;
    }

  private:
    ULONG m_refCount = 1;
    HWND m_hwnd = nullptr;
};

WebviewWnd::WebviewWnd() {
    kind = kindWebView;
}

void WebviewWnd::OnProcessFailed(WebViewProcessFailure kind) {
    logf("WebviewWnd::OnProcessFailed: kind=%d\n", (int)kind);
    if (events.processFailed && events.processFailed(events.ctx, kind)) {
        return;
    }
    switch (kind) {
        case WebViewProcessFailure::RenderExited:
        case WebViewProcessFailure::RenderUnresponsive:
            // the browser process is still alive, so a reload rebuilds the renderer
            if (webview) {
                webview->Reload();
            }
            break;
        case WebViewProcessFailure::BrowserExited:
            // everything hanging off this environment is dead, including the
            // shared one other webviews use; drop it so the next Embed() builds
            // a fresh environment instead of handing out a dead one
            ResetSharedEnvironment();
            FailInit();
            break;
        case WebViewProcessFailure::Other:
            // GPU/utility process: WebView2 recovers on its own
            break;
    }
}

void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind kind, Str text, int token) {
    if (initFailed) {
        return;
    }
    PendingWebViewOp op;
    op.kind = kind;
    op.text = str::Dup(text ? text : StrL(""));
    op.token = token;
    VecAppend(pendingOps, op);
}

void WebviewWnd::FlushPendingOps() {
    if (!webview || initFailed) {
        return;
    }
    // Vec copies PendingWebViewOp by value (shallow copy of text pointers), so only
    // free op.text once -- in the loop below.
    Vec<PendingWebViewOp> ops = pendingOps;
    VecReset(pendingOps);
    for (PendingWebViewOp& op : ops) {
        switch (op.kind) {
            case PendingWebViewOp::Init:
                // keep the token the caller already holds so RemoveInitScript works
                AddInitScriptWithToken(op.text, op.token);
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
    // see the destructor: releasing the controller doesn't shut the browser
    // instance down, Close() does
    if (controller) {
        controller->Close();
    }
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
    desiredVisible = visible;
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

// WebView2 can keep a hidden or old-size composition surface after showing a
// tab or after its parent changes from a normal window to fullscreen. Briefly
// detaching that surface makes it rebuild without reloading the document.
void WebviewWnd::RefreshControllerSurface() {
    if (!controller || !desiredVisible) {
        return;
    }
    controller->put_IsVisible(FALSE);
    controller->put_IsVisible(TRUE);
    isVisible = true;
    UpdateWebviewSize();
}

void WebviewWnd::OnControllerReady(ICoreWebView2Controller* controller) {
    if (!controller) {
        FailInit();
        return;
    }
    this->controller = controller;

    auto style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_OVERLAPPEDWINDOW);
    SetWindowLong(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    isVisible = false;
    controller->put_IsVisible(FALSE);

    ICoreWebView2Controller2* controller2 = nullptr;
    HRESULT bgHr = controller->QueryInterface(IID_PPV_ARGS(&controller2));
    if (SUCCEEDED(bgHr) && controller2) {
        COREWEBVIEW2_COLOR bg = {};
        if (!ColorSkipsPaint(defaultBackgroundColor)) {
            bg.A = 255;
            bg.R = GetRed(defaultBackgroundColor);
            bg.G = GetGreen(defaultBackgroundColor);
            bg.B = GetBlue(defaultBackgroundColor);
        }
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
    lastBounds = HwndClientRect(hwnd);
    hasLastBounds = true;
    RECT bounds = ToRECT(lastBounds);
    controller->put_Bounds(bounds);
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
        controller->add_AcceleratorKeyPressed(accelHandler, &token);
        accelHandler->Release();
    }

    if (resourceProvider.getResource && resourceUriPrefix) {
        TempWStr filter = str::JoinTemp(resourceUriPrefix, WStrL(L"*"));
        ICoreWebView2_22* wv22 = nullptr;
        if (SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&wv22))) && wv22) {
            wv22->AddWebResourceRequestedFilterWithRequestSourceKinds(
                filter.s, COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL, COREWEBVIEW2_WEB_RESOURCE_REQUEST_SOURCE_KINDS_ALL);
            wv22->Release();
        } else {
            webview->AddWebResourceRequestedFilter(filter.s, COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        }
        auto* resourceHandler = new webview2_resource_handler(this);
        ::EventRegistrationToken resourceToken = {};
        webview->add_WebResourceRequested(resourceHandler, &resourceToken);
        resourceHandler->Release();
    }

    if (events.navigationStarting || events.navigationCompleted || events.historyChanged) {
        ::EventRegistrationToken token = {};
        if (events.navigationStarting) {
            auto* handler = new webview2_navigation_starting_handler(this);
            webview->add_NavigationStarting(handler, &token);
            handler->Release();
            auto* newWindowHandler = new webview2_new_window_handler(this);
            webview->add_NewWindowRequested(newWindowHandler, &token);
            newWindowHandler->Release();
        }
        if (events.navigationCompleted) {
            auto* handler = new webview2_navigation_completed_handler(this);
            webview->add_NavigationCompleted(handler, &token);
            handler->Release();
        }
        if (events.historyChanged) {
            auto* handler = new webview2_history_changed_handler(this);
            webview->add_HistoryChanged(handler, &token);
            handler->Release();
        }
    }

    // optional: open external downloads in the OS browser (issue #5920)
    if (routeDownloadsToOsBrowser) {
        ICoreWebView2_4* wv4 = nullptr;
        if (SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&wv4))) && wv4) {
            auto* handler = new webview2_download_starting_handler(this);
            ::EventRegistrationToken token = {};
            if (FAILED(wv4->add_DownloadStarting(handler, &token))) {
                logf("WebviewWnd: add_DownloadStarting failed\n");
            }
            handler->Release();
            wv4->Release();
        }
    }

    {
        auto* failedHandler = new webview2_process_failed_handler(hwnd);
        ::EventRegistrationToken token = {};
        if (FAILED(webview->add_ProcessFailed(failedHandler, &token))) {
            logf("WebviewWnd: add_ProcessFailed failed\n");
        }
        failedHandler->Release();
    }

    Init(StrL("window.external={invoke:s=>window.chrome.webview.postMessage(s)}"));
    Init(Str(kJsBridgeScript));
    // re-register any names bound before the controller was ready
    RebuildBindScript();
    initStarted = true;
    FlushPendingOps();

    // honor desiredVisible (SetControllerVisible) so BrowserDocView can create
    // hidden during a tab probe without the async ready callback showing it
    bool wantVisible = desiredVisible && ShouldWebviewBeVisible(hwnd);
    if (wantVisible) {
        ::ShowWindow(hwnd, SW_SHOW);
    } else {
        ::ShowWindow(hwnd, SW_HIDE);
    }
    bool want = desiredVisible;
    isVisible = !want; // force SetControllerVisible to apply
    SetControllerVisible(want);
}

void WebviewWnd::UpdateWebviewSize() {
    if (controller == nullptr) {
        return;
    }
    Rect bounds = HwndClientRect(hwnd);
    controller->NotifyParentWindowPositionChanged();
    if (hasLastBounds && bounds == lastBounds) {
        return;
    }
    lastBounds = bounds;
    hasLastBounds = true;
    RECT r = ToRECT(bounds);
    controller->put_Bounds(r);
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
    AddInitScript(js);
}

// Registers a script to run on every document, returning a token that
// RemoveInitScript() takes. The token is ours and valid immediately; the
// WebView2 id it maps to arrives later via OnInitScriptAdded().
int WebviewWnd::AddInitScript(Str js) {
    if (initFailed || len(js) == 0) {
        return 0;
    }
    int token = nextInitScriptToken++;
    AddInitScriptWithToken(js, token);
    return token;
}

void WebviewWnd::AddInitScriptWithToken(Str js, int token) {
    if (initFailed || len(js) == 0 || token == 0) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::Init, js, token);
        return;
    }
    WebViewInitScript script;
    script.token = token;
    VecAppend(initScripts, script);

    WCHAR* ws = CWStrTemp(js);
    auto* handler = new webview2_add_script_handler(hwnd, token);
    HRESULT hr = webview->AddScriptToExecuteOnDocumentCreated(ws, handler);
    handler->Release();
    if (FAILED(hr)) {
        int idx = FindInitScript(token);
        if (idx >= 0) {
            VecRemoveAt(initScripts, idx);
        }
    }
}

int WebviewWnd::FindInitScript(int token) const {
    int n = len(initScripts);
    for (int i = 0; i < n; i++) {
        if (initScripts[i].token == token) {
            return i;
        }
    }
    return -1;
}

void WebviewWnd::OnInitScriptAdded(int token, const WCHAR* id) {
    int idx = FindInitScript(token);
    if (idx < 0) {
        return;
    }
    WebViewInitScript& script = initScripts[idx];
    // RemoveInitScript() ran before the id arrived; drop it now that we can
    if (script.removePending) {
        if (webview) {
            webview->RemoveScriptToExecuteOnDocumentCreated(id);
        }
        VecRemoveAt(initScripts, idx);
        return;
    }
    wstr::Free(script.id);
    script.id = wstr::Dup(WStr(id));
}

void WebviewWnd::RemoveInitScript(int token) {
    if (token == 0) {
        return;
    }
    // still queued: it was never handed to WebView2, so just drop it
    int n = len(pendingOps);
    for (int i = 0; i < n; i++) {
        PendingWebViewOp& op = pendingOps[i];
        if (op.kind == PendingWebViewOp::Init && op.token == token) {
            str::Free(op.text);
            VecRemoveAt(pendingOps, i);
            return;
        }
    }
    int idx = FindInitScript(token);
    if (idx < 0) {
        return;
    }
    WebViewInitScript& script = initScripts[idx];
    if (len(script.id) == 0) {
        // the id hasn't arrived yet; OnInitScriptAdded will remove it
        script.removePending = true;
        return;
    }
    if (webview) {
        webview->RemoveScriptToExecuteOnDocumentCreated(script.id.s);
    }
    wstr::Free(script.id);
    VecRemoveAt(initScripts, idx);
}

void WebviewWnd::RemoveAllInitScripts() {
    for (WebViewInitScript& script : initScripts) {
        if (script.id) {
            if (webview) {
                webview->RemoveScriptToExecuteOnDocumentCreated(script.id.s);
            }
            wstr::Free(script.id);
        } else {
            script.removePending = true;
        }
    }
    // scripts whose id is still pending must stay in the list so
    // OnInitScriptAdded() can remove them once it arrives
    for (int i = len(initScripts) - 1; i >= 0; i--) {
        if (!initScripts[i].removePending) {
            VecRemoveAt(initScripts, i);
        }
    }
}

// markers the bridge puts first in its messages; JSON.stringify preserves
// insertion order, so a prefix test is enough to tell these from raw messages
static const char* kJsCallPrefix = "{\"__sumatraCall\":1";
static const char* kJsNotifyPrefix = "{\"__sumatraNotify\":1";

static bool IsJsCallMessage(Str msg) {
    return str::StartsWith(msg, Str(kJsCallPrefix));
}

static bool IsJsNotifyMessage(Str msg) {
    return str::StartsWith(msg, Str(kJsNotifyPrefix));
}

namespace {
// json::Parse hands value as a temp-arena copy; path is valid only during the callback.
struct JsCallState {
    TempStr id;
    TempStr method;
    TempStr params;
};

static void JsCallOnValue(JsCallState* st, json::Value* v) {
    if (v->type != json::Type::String) {
        return;
    }
    if (json::PathMatch(v->path, StrL("/id"))) {
        st->id = v->value;
    } else if (json::PathMatch(v->path, StrL("/method"))) {
        st->method = v->value;
    } else if (json::PathMatch(v->path, StrL("/params"))) {
        st->params = v->value;
    }
}
} // namespace

void WebviewWnd::OnJsCall(Str msg) {
    JsCallState st;
    if (!json::Parse(msg, MkFunc1(JsCallOnValue, &st))) {
        logf("WebviewWnd::OnJsCall: failed to parse '%s'\n", msg);
        return;
    }
    if (len(st.id) == 0 || len(st.method) == 0) {
        return;
    }
    if (!events.jsCall) {
        // nothing can answer, so reject instead of leaving the promise pending
        Resolve(st.id, 1, StrL("\"no handler\""));
        return;
    }
    Str params = st.params ? Str(st.params) : StrL("[]");
    events.jsCall(events.ctx, st.id, st.method, params);
}

void WebviewWnd::OnJsNotify(Str msg) {
    if (!events.jsNotify) {
        return;
    }
    JsCallState st;
    if (!json::Parse(msg, MkFunc1(JsCallOnValue, &st))) {
        logf("WebviewWnd::OnJsNotify: failed to parse '%s'\n", msg);
        return;
    }
    if (len(st.method) == 0) {
        return;
    }
    Str params = st.params ? Str(st.params) : StrL("[]");
    events.jsNotify(events.ctx, st.method, params);
}

// status 0 resolves the JS promise, non-0 rejects it. resultJson must be
// valid JSON (or empty for undefined)
void WebviewWnd::Resolve(Str id, int status, Str resultJson) {
    if (len(id) == 0) {
        return;
    }
    TempStr js = fmt("if (window.__sumatra__) window.__sumatra__.onReply(\"%s\", %d, \"%s\");", json::EscapeStrTemp(id),
                     status, json::EscapeStrTemp(resultJson));
    Eval(js);
}

// Rebuilds the init script that re-binds every name on each new document, so
// bindings survive navigation.
void WebviewWnd::RebuildBindScript() {
    if (bindScriptToken) {
        RemoveInitScript(bindScriptToken);
        bindScriptToken = 0;
    }
    if (len(boundNames) == 0) {
        return;
    }
    str::Builder js;
    js.Append(StrL("(function(){var m=["));
    bool first = true;
    for (Str& name : boundNames) {
        if (!first) {
            js.Append(StrL(","));
        }
        first = false;
        js.Append(fmt("\"%s\"", json::EscapeStrTemp(name)));
    }
    js.Append(StrL("];m.forEach(function(n){window.__sumatra__.onBind(n)})})()"));
    bindScriptToken = AddInitScript(ToStr(js));
}

// makes window.<name>(...) available to page JS, returning a promise that
// WebViewEvents::jsCall resolves via Resolve()
void WebviewWnd::Bind(Str name) {
    if (len(name) == 0) {
        return;
    }
    for (Str& n : boundNames) {
        if (str::Eq(n, name)) {
            return;
        }
    }
    VecAppend(boundNames, str::Dup(name));
    RebuildBindScript();
    // also expose it in the document that's already loaded
    Eval(fmt("if (window.__sumatra__) window.__sumatra__.onBind(\"%s\");", json::EscapeStrTemp(name)));
}

void WebviewWnd::Unbind(Str name) {
    int n = len(boundNames);
    for (int i = 0; i < n; i++) {
        if (str::Eq(boundNames[i], name)) {
            str::Free(boundNames[i]);
            VecRemoveAt(boundNames, i);
            RebuildBindScript();
            Eval(fmt("if (window.__sumatra__) window.__sumatra__.onUnbind(\"%s\");", json::EscapeStrTemp(name)));
            return;
        }
    }
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
    return (int)lround(factor * 100.0);
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

// open the WebView2 (Chromium) find-on-page bar, like a browser's Ctrl+F
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
    VecAppend(*hwnds, hwnd);
    return TRUE;
}

// Register `target` on `hwnd` (replacing any existing drop target). Records the
// window in `registered` on success so it can be revoked later.
static void RegisterDropOn(HWND hwnd, IDropTarget* target, Vec<HWND>& registered) {
    if (VecContains(registered, hwnd)) {
        return;
    }
    RevokeDragDrop(hwnd);
    if (SUCCEEDED(RegisterDragDrop(hwnd, target))) {
        VecAppend(registered, hwnd);
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
    VecAppend(wnds, hwnd);
    EnumChildWindows(hwnd, CollectChildHwnds, (LPARAM)&wnds);
    for (HWND h : wnds) {
        RegisterDropOn(h, dropTarget, dropTargetHwnds);
    }
}

void WebviewWnd::RevokeForwardingDropTarget() {
    for (HWND h : dropTargetHwnds) {
        RevokeDragDrop(h);
    }
    VecReset(dropTargetHwnds);
    if (dropTarget) {
        dropTarget->Release();
        dropTarget = nullptr;
    }
}

static void ComHandlerCbHwnd(void* hwndVoid, ICoreWebView2Controller* ctrl) {
    HWND hwnd = (HWND)hwndVoid;
    auto* self = (WebviewWnd*)WindowBaseFromHwnd(hwnd);
    if (!self) {
        if (ctrl) {
            // The window went away while the webview was still being created
            // (closing a .md/.html document right after opening it). Releasing
            // only drops our reference, leaving a live browser instance bound
            // to a destroyed HWND, which hung the app on exit. Close() is what
            // shuts it down.
            ctrl->Close();
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
    VecReset(gPendingWebviews);
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
    auto* handler = new webview2_com_handler(hwnd, cb, fn, self->allowClipboardRead);
    HRESULT hr = gSharedEnvironment->CreateCoreWebView2Controller(self->hwnd, handler);
    handler->Release();
    if (FAILED(hr)) {
        self->FailInit();
    }
}

static bool StartSharedEnvironmentCreation();

// Give up on creating the shared environment. Records when, so that
// EnvCreationAllowed() lets a later Embed() try again instead of leaving every
// webview in the process dead until restart.
static void SharedEnvironmentGiveUp() {
    gSharedEnvState = SharedWebViewEnvState::Failed;
    gEnvFailedAtMs = GetTickCount();
    FailPendingWebviews();
}

// Drops the shared environment. Used when the browser process dies: every
// controller created from it is dead too, so the next Embed() must build a new
// one rather than hand out a stale pointer.
static void ResetSharedEnvironment() {
    if (gSharedEnvironment) {
        gSharedEnvironment->Release();
        gSharedEnvironment = nullptr;
    }
    gSharedEnvState = SharedWebViewEnvState::NotStarted;
    gEnvCreateAttempts = 0;
    gEnvFailedAtMs = 0;
}

static void CancelEnvRetryTimer() {
    if (gEnvRetryTimerId && gEnvRetryTimerHwnd && ::IsWindow(gEnvRetryTimerHwnd)) {
        KillTimer(gEnvRetryTimerHwnd, gEnvRetryTimerId);
    }
    gEnvRetryTimerId = 0;
    gEnvRetryTimerHwnd = nullptr;
}

void WebViewShutdown() {
    gWebViewShuttingDown = true;
    CancelEnvRetryTimer();
    FailPendingWebviews();
    ResetSharedEnvironment();
    wstr::Free(gSharedUserDataFolder);
}

// Retry on a timer rather than Sleep()ing: this runs on the UI thread.
static void ScheduleEnvCreateRetry() {
    CancelEnvRetryTimer();
    HWND hwnd = nullptr;
    for (WebviewWnd* wv : gPendingWebviews) {
        if (wv && wv->hwnd && ::IsWindow(wv->hwnd)) {
            hwnd = wv->hwnd;
            break;
        }
    }
    if (!hwnd) {
        // nothing is waiting anymore; let the next Embed() start over with a
        // fresh attempt budget rather than leaving the state stuck in Creating
        gSharedEnvState = SharedWebViewEnvState::NotStarted;
        gEnvCreateAttempts = 0;
        return;
    }
    gEnvRetryTimerHwnd = hwnd;
    gEnvRetryTimerId = SetTimer(hwnd, kEnvRetryTimerId, kEnvCreateRetryDelayMs, nullptr);
    if (!gEnvRetryTimerId) {
        SharedEnvironmentGiveUp();
    }
}

static void OnEnvCreateRetryTimer() {
    CancelEnvRetryTimer();
    if (gSharedEnvState != SharedWebViewEnvState::Creating) {
        return;
    }
    if (!StartSharedEnvironmentCreation()) {
        SharedEnvironmentGiveUp();
    }
}

static void OnSharedEnvironmentReady(HRESULT res, ICoreWebView2Environment* env) {
    if (gWebViewShuttingDown) {
        return;
    }
    if (FAILED(res) || !env) {
        logf("WebView2: creating environment failed with 0x%x (attempt %d/%d)\n", (int)res, gEnvCreateAttempts,
             kMaxEnvCreateAttempts);
        if (gEnvCreateAttempts < kMaxEnvCreateAttempts) {
            ScheduleEnvCreateRetry();
        } else {
            SharedEnvironmentGiveUp();
        }
        return;
    }

    env->AddRef();
    gSharedEnvironment = env;
    gSharedEnvState = SharedWebViewEnvState::Ready;
    gEnvCreateAttempts = 0;

    Vec<WebviewWnd*> pending = gPendingWebviews;
    VecReset(gPendingWebviews);
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

static bool StartSharedEnvironmentCreation() {
    gEnvCreateAttempts++;
    auto options = CreateOfflineEnvironmentOptions();
    auto* envHandler = new webview2_env_handler(OnSharedEnvironmentReady);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, gSharedUserDataFolder.s, options.Get(), envHandler);
    envHandler->Release();
    if (SUCCEEDED(hr)) {
        return true;
    }
    logf("WebView2: CreateCoreWebView2EnvironmentWithOptions failed with 0x%x (attempt %d/%d)\n", (int)hr,
         gEnvCreateAttempts, kMaxEnvCreateAttempts);
    if (gEnvCreateAttempts < kMaxEnvCreateAttempts) {
        ScheduleEnvCreateRetry();
        return true;
    }
    return false;
}

// After giving up we stay Failed for a cooldown, then allow a fresh attempt: the
// usual cause (another instance holding the user data folder) goes away on its own.
static bool EnvCreationAllowedAgain() {
    if (gSharedEnvState != SharedWebViewEnvState::Failed) {
        return false;
    }
    DWORD elapsed = GetTickCount() - gEnvFailedAtMs;
    return elapsed >= kEnvFailedCooldownMs;
}

bool WebviewWnd::Embed(WebViewMsgCb& cb) {
    if (gWebViewShuttingDown) {
        return false;
    }
    if (initStarted) {
        return !initFailed;
    }
    if (len(dataDir) == 0) {
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
        if (!EnvCreationAllowedAgain()) {
            initFailed = true;
            return false;
        }
        logf("WebView2: retrying environment creation after an earlier failure\n");
        gSharedEnvState = SharedWebViewEnvState::NotStarted;
        gEnvCreateAttempts = 0;
    }

    VecAppend(gPendingWebviews, this);

    if (gSharedEnvState == SharedWebViewEnvState::NotStarted) {
        gSharedEnvState = SharedWebViewEnvState::Creating;
        wstr::Free(gSharedUserDataFolder);
        gSharedUserDataFolder = wstr::Dup(userDataFolder);
        if (!StartSharedEnvironmentCreation()) {
            SharedEnvironmentGiveUp();
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
    auto* self = (WebviewWnd*)WindowBaseFromHwnd(hwnd);
    if (self) {
        // structured calls from the JS bridge don't go to OnBrowserMessage, so
        // subclasses keep seeing only their own raw messages
        if (IsJsCallMessage(msg)) {
            self->OnJsCall(msg);
            return;
        }
        if (IsJsNotifyMessage(msg)) {
            self->OnJsNotify(msg);
            return;
        }
        self->OnBrowserMessage(msg);
    }
}

HWND WebviewWnd::Create(const CreateWebViewArgs& args) {
    ReportIf(len(dataDir) == 0);
    onTimer = MkMethod1<WebviewWnd, WindowBase::TimerEvent*, &WebviewWnd::OnTimer>(this);
    onSize = MkMethod1<WebviewWnd, WindowBase::SizeEvent*, &WebviewWnd::OnSize>(this);
    onActivate = MkMethod1<WebviewWnd, WindowBase::ActivateEvent*, &WebviewWnd::OnActivate>(this);
    onShowWindow = MkMethod1<WebviewWnd, WindowBase::ShowWindowEvent*, &WebviewWnd::OnShowWindow>(this);
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
    // Keep the host fallback and WebView2's composition background in sync.
    // Transparent general-purpose webviews use the chrome background; document
    // webviews supply their page color so resize and tab transitions cannot
    // reveal stale pixels or the gray canvas behind the page.
    Color fallbackBg = ColorSkipsPaint(defaultBackgroundColor) ? gColsWin[kColWinBg] : defaultBackgroundColor;
    SetColors(kColorNoChange, fallbackBg);

    auto fn = MkFunc1<void, Str>(OnBrowserMessageCbHwnd, (void*)hwnd);
    if (!Embed(fn)) {
        HwndDestroyWindowSafe(&hwnd);
        return nullptr;
    }
    UpdateWebviewSize();
    return hwnd;
}

void WebviewWnd::OnTimer(WindowBase::TimerEvent* ev) {
    if (ev->timerId == kEnvRetryTimerId) {
        OnEnvCreateRetryTimer();
    }
}

void WebviewWnd::OnSize(WindowBase::SizeEvent* ev) {
    if (ev->msg == WM_ENTERSIZEMOVE) {
        isInSizeMove = true;
        Eval(StrL("if (window.__setHostResizing) window.__setHostResizing(true);"));
        return;
    }
    if (ev->msg == WM_EXITSIZEMOVE) {
        isInSizeMove = false;
        Eval(StrL("if (window.__setHostResizing) window.__setHostResizing(false);"));
        UpdateWebviewSize();
        return;
    }
    if (ev->msg == WM_SIZE) {
        SetControllerVisible(ev->type != SIZE_MINIMIZED);
        if (!isInSizeMove) {
            UpdateWebviewSize();
        }
    }
}

void WebviewWnd::OnActivate(WindowBase::ActivateEvent* ev) {
    if (ev->state == WA_INACTIVE) {
        SetControllerVisible(false);
    } else {
        SetControllerVisible(true);
        UpdateWebviewSize();
    }
}

void WebviewWnd::OnShowWindow(WindowBase::ShowWindowEvent* ev) {
    // RelayoutFrame sends WM_SETREDRAW to the frame, which hides the parent
    // and delivers SW_PARENTCLOSING / SW_PARENTOPENING to children. Hiding
    // the WebView2 controller then flashes the canvas (last PDF/CBR paint)
    // on every resize. Tab show/hide uses ShowWindow on this HWND (status 0).
    if (ev->status == SW_PARENTCLOSING || ev->status == SW_PARENTOPENING) {
        HWND root = GetAncestor(hwnd, GA_ROOT);
        if (root && IsIconic(root)) {
            SetControllerVisible(false);
            return;
        }
        if (ev->status == SW_PARENTOPENING && desiredVisible) {
            SetControllerVisible(true);
            UpdateWebviewSize();
        }
        return;
    }
    SetControllerVisible(ev->show);
    UpdateWebviewSize();
}

WebviewWnd::~WebviewWnd() {
    RemovePendingWebview(this);
    FreePendingOps(pendingOps);
    for (WebViewInitScript& script : initScripts) {
        wstr::Free(script.id);
    }
    VecReset(initScripts);
    for (Str& name : boundNames) {
        str::Free(name);
    }
    VecReset(boundNames);
    RevokeForwardingDropTarget();
    // Close() shuts down the browser instance behind this webview and is not
    // optional: releasing the controller only drops our reference, so without
    // it the WebView2 runtime keeps running against the window we're about to
    // destroy. Closing a document (or the app) while the webview was still
    // coming up then hung the process on exit or exited it with an error.
    if (controller) {
        controller->Close();
    }
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
int WebviewWnd::AddInitScript(Str) {
    return 0;
}
void WebviewWnd::AddInitScriptWithToken(Str, int) {}
int WebviewWnd::FindInitScript(int) const {
    return -1;
}
void WebviewWnd::RemoveInitScript(int) {}
void WebviewWnd::RemoveAllInitScripts() {}
void WebviewWnd::OnInitScriptAdded(int, const WCHAR*) {}
// makes window.<name>(...) available to page JS, returning a promise that
// WebViewEvents::jsCall resolves via Resolve()
void WebviewWnd::Bind(Str) {}
void WebviewWnd::Unbind(Str) {}
// status 0 resolves the JS promise, non-0 rejects it. resultJson must be
// valid JSON (or empty for undefined)
void WebviewWnd::Resolve(Str, int, Str) {}
void WebviewWnd::OnJsCall(Str) {}
void WebviewWnd::OnJsNotify(Str) {}
void WebviewWnd::RebuildBindScript() {}
void WebviewWnd::OnProcessFailed(WebViewProcessFailure) {}
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
// open the WebView2 (Chromium) find-on-page bar, like a browser's Ctrl+F
void WebviewWnd::ShowFindUI() {}
bool WebviewWnd::Embed(WebViewMsgCb&) {
    return false;
}
void WebViewShutdown() {}
void WebviewWnd::OnControllerReady(ICoreWebView2Controller*) {}
void WebviewWnd::FailInit() {}
void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind, Str, int) {}
void WebviewWnd::FlushPendingOps() {}
void WebviewWnd::SetControllerVisible(bool) {}
void WebviewWnd::RefreshControllerSurface() {}
void WebviewWnd::OnBrowserMessage(Str) {}
void WebviewWnd::OnTimer(WindowBase::TimerEvent*) {}
void WebviewWnd::OnSize(WindowBase::SizeEvent*) {}
void WebviewWnd::OnActivate(WindowBase::ActivateEvent*) {}
void WebviewWnd::OnShowWindow(WindowBase::ShowWindowEvent*) {}
void WebviewWnd::UpdateWebviewSize() {}
#endif // !_MSC_VER
