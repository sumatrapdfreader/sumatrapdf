/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

// #include "Theme.h"

#ifdef _MSC_VER
#include <wrl.h>
#include "webview2.h"
#include "WebView2EnvironmentOptions.h"
#endif
#include "wingui/WebView.h"

#include "utils/Log.h"

Kind kindWebView = "webView";

#ifndef _MSC_VER
TempStr GetWebView2VersionTemp() {
    return nullptr;
}
bool HasWebView() {
    return false;
}
#else
TempStr GetWebView2VersionTemp() {
    WCHAR* ver = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
    if (FAILED(hr) || (ver == nullptr)) {
        return nullptr;
    }
    TempStr res = ToUtf8Temp(ver);
    CoTaskMemFree((void*)ver);
    return res;
}

bool HasWebView() {
    WCHAR* ver = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
    if (FAILED(hr) || str::IsEmpty(ver)) {
        logf("WebView2 is not available\n");
        return false;
    }
    return true;
}
#endif // _MSC_VER

#ifdef _MSC_VER

namespace {

enum class SharedWebViewEnvState {
    NotStarted,
    Creating,
    Ready,
    Failed,
};

SharedWebViewEnvState gSharedEnvState = SharedWebViewEnvState::NotStarted;
ICoreWebView2Environment* gSharedEnvironment = nullptr;
WCHAR* gSharedUserDataFolder = nullptr;
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
        char* s = ToUtf8Temp(message);
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
    explicit webview2_accel_handler(HWND hwnd) : m_hwnd(hwnd) {}
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

        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool isAppAccel = false;
        if (ctrl && (vk == 'O' || vk == 'P' || vk == 'S' || vk == 'F' || vk == 'G' || vk == 'C' || vk == 'V' ||
                     vk == 'W' || vk == 'N' || vk == 'K')) {
            isAppAccel = true;
        }
        if (vk == VK_F3) {
            isAppAccel = true;
        }

        if (isAppAccel) {
            args->put_Handled(TRUE);
            HWND root = GetAncestor(m_hwnd, GA_ROOT);
            if (root && ::IsWindow(root)) {
                PostMessageW(root, WM_KEYDOWN, (WPARAM)vk, 0);
                PostMessageW(root, WM_KEYUP, (WPARAM)vk, 0);
            }
            return S_OK;
        }
        return S_OK;
    }

  private:
    ULONG m_refCount = 1;
    HWND m_hwnd = nullptr;
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

WebviewWnd::WebviewWnd() {
    kind = kindWebView;
}

void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind kind, const char* text) {
    if (initFailed) {
        return;
    }
    PendingWebViewOp op;
    op.kind = kind;
    op.text = str::Dup(text ? text : "");
    pendingOps.Append(op);
}

void WebviewWnd::FlushPendingOps() {
    if (!webview || initFailed) {
        return;
    }
    Vec<PendingWebViewOp> ops = pendingOps;
    FreePendingOps(pendingOps);
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

    controller->put_IsVisible(isVisible ? TRUE : FALSE);

    ICoreWebView2Controller2* controller2 = nullptr;
    HRESULT bgHr = controller->QueryInterface(IID_PPV_ARGS(&controller2));
    if (SUCCEEDED(bgHr) && controller2) {
        COREWEBVIEW2_COLOR bg = {0, 0, 0, 0};
        controller2->put_DefaultBackgroundColor(bg);
        controller2->Release();
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
        webview2_accel_handler* accelHandler = new webview2_accel_handler(hwnd);
        ::EventRegistrationToken token = {};
        if (SUCCEEDED(controller->add_AcceleratorKeyPressed(accelHandler, &token))) {
            accelHandler->Release();
        } else {
            accelHandler->Release();
        }
    }

    Init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
    initStarted = true;
    FlushPendingOps();
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

void WebviewWnd::Eval(const char* js) {
    if (initFailed || str::IsEmpty(js)) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::Eval, js);
        return;
    }
    TempWStr ws = ToWStrTemp(js);
    webview->ExecuteScript(ws, nullptr);
}

void WebviewWnd::SetHtml(const char* html) {
    if (initFailed || str::IsEmpty(html)) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::SetHtml, html);
        return;
    }
    WCHAR* html2 = ToWStrTemp(html);
    webview->NavigateToString(html2);
}

void WebviewWnd::Init(const char* js) {
    if (initFailed || str::IsEmpty(js)) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::Init, js);
        return;
    }
    TempWStr ws = ToWStrTemp(js);
    webview->AddScriptToExecuteOnDocumentCreated(ws, nullptr);
}

void WebviewWnd::Navigate(const char* url) {
    if (initFailed || str::IsEmpty(url)) {
        return;
    }
    if (!webview) {
        QueuePendingOp(PendingWebViewOp::Navigate, url);
        return;
    }
    TempWStr ws = ToWStrTemp(url);
    webview->Navigate(ws);
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

static void ComHandlerCb(WebviewWnd* self, ICoreWebView2Controller* ctrl) {
    if (!ctrl) {
        self->FailInit();
        return;
    }
    self->OnControllerReady(ctrl);
}

static void OnBrowserMessageCb(WebviewWnd* self, const char* msg);

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
    auto fn = MkFunc1(ComHandlerCb, self);
    auto* handler = new webview2_com_handler(self->hwnd, cb, fn);
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
        auto fn = MkFunc1(OnBrowserMessageCb, wv);
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
    str::Free(userDataFolder);
    userDataFolder = str::Dup(ToWStrTemp(dataDir));

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
        str::Free(gSharedUserDataFolder);
        gSharedUserDataFolder = str::Dup(userDataFolder);
        auto options = CreateOfflineEnvironmentOptions();
        auto* envHandler = new webview2_env_handler(OnSharedEnvironmentReady);
        HRESULT hr =
            CreateCoreWebView2EnvironmentWithOptions(nullptr, gSharedUserDataFolder, options.Get(), envHandler);
        envHandler->Release();
        if (FAILED(hr)) {
            gSharedEnvState = SharedWebViewEnvState::Failed;
            FailPendingWebviews();
            return false;
        }
    } else if (gSharedEnvState == SharedWebViewEnvState::Creating) {
        if (!str::Eq(userDataFolder, gSharedUserDataFolder)) {
            logf("WebviewWnd::Embed: reusing shared WebView2 environment with first dataDir\n");
        }
    }

    return true;
}

void WebviewWnd::OnBrowserMessage(const char* msg) {
    log(msg);
}

static void OnBrowserMessageCb(WebviewWnd* self, const char* msg) {
    self->OnBrowserMessage(msg);
}

HWND WebviewWnd::Create(const CreateWebViewArgs& args) {
    ReportIf(!dataDir);
    CreateCustomArgs cargs;
    cargs.parent = args.parent;
    cargs.pos = args.pos;
    CreateCustom(cargs);
    if (!hwnd) {
        return nullptr;
    }

    auto fn = MkFunc1(OnBrowserMessageCb, this);
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
    if (webview) {
        webview->Release();
        webview = nullptr;
    }
    if (controller) {
        controller->Release();
        controller = nullptr;
    }
    str::Free(dataDir);
    str::Free(userDataFolder);
}

#endif // _MSC_VER

#ifndef _MSC_VER
// stub implementations for mingw cross-compile / wine (no webview2)
WebviewWnd::WebviewWnd() = default;
WebviewWnd::~WebviewWnd() {
    str::Free(dataDir);
    str::Free(userDataFolder);
}
HWND WebviewWnd::Create(const CreateWebViewArgs&) {
    return nullptr;
}
void WebviewWnd::Eval(const char*) {}
void WebviewWnd::SetHtml(const char*) {}
void WebviewWnd::Init(const char*) {}
void WebviewWnd::Navigate(const char*) {}
void WebviewWnd::Focus() {}
bool WebviewWnd::Embed(WebViewMsgCb&) {
    return false;
}
void WebviewWnd::OnControllerReady(ICoreWebView2Controller*) {}
void WebviewWnd::FailInit() {}
void WebviewWnd::QueuePendingOp(PendingWebViewOp::Kind, const char*) {}
void WebviewWnd::FlushPendingOps() {}
void WebviewWnd::SetControllerVisible(bool) {}
void WebviewWnd::OnBrowserMessage(const char*) {}
LRESULT WebviewWnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return WndProcDefault(hwnd, msg, wparam, lparam);
}
void WebviewWnd::UpdateWebviewSize() {}
#endif // !_MSC_VER