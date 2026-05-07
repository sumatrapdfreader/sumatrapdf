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

#include "webview2.h"
#include <wrl.h>
#include "WebView2EnvironmentOptions.h"
#include "wingui/WebView.h"

#include "prettysumatra/BridgeDispatcher.h"

#include "utils/Log.h"

Kind kindWebView = "webView";

namespace {
enum class SharedWebViewEnvState {
    NotStarted,
    Creating,
    Ready,
    Failed,
};

static SharedWebViewEnvState gSharedEnvState = SharedWebViewEnvState::NotStarted;
static ICoreWebView2Environment* gSharedEnvironment = nullptr;
static std::wstring gSharedUserDataFolder;
static std::vector<WebviewWnd*> gPendingWebviews;
} // namespace

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

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
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

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT /*errorCode*/, BOOL /*isSuccessful*/) {
        return S_OK;
    }

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
    op.text = text ? text : "";
    pendingOps.push_back(op);
}

void WebviewWnd::FlushPendingOps() {
    if (!webview || initFailed) {
        return;
    }
    std::vector<PendingWebViewOp> ops = pendingOps;
    pendingOps.clear();
    for (auto& op : ops) {
        switch (op.kind) {
            case PendingWebViewOp::Init:
                Init(op.text.c_str());
                break;
            case PendingWebViewOp::SetHtml:
                SetHtml(op.text.c_str());
                break;
            case PendingWebViewOp::Eval:
                Eval(op.text.c_str());
                break;
            case PendingWebViewOp::Navigate:
                Navigate(op.text.c_str());
                break;
        }
    }
}

void WebviewWnd::FailInit() {
    initFailed = true;
    pendingOps.clear();
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
    // Ensure the host window takes focus first, then move focus into the WebView2 content.
    ::SetFocus(hwnd);
    if (controller) {
        controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

/*
    Settings:
    put_IsWebMessageEnabled(BOOL isWebMessageEnabled)
    put_AreDefaultScriptDialogsEnabled(BOOL areDefaultScriptDialogsEnabled)
    put_IsStatusBarEnabled(BOOL isStatusBarEnabled)
    put_AreDevToolsEnabled(BOOL areDevToolsEnabled)
    put_AreDefaultContextMenusEnabled(BOOL enabled)
    put_AreHostObjectsAllowed(BOOL allowed)
    put_IsZoomControlEnabled(BOOL enabled)
    put_IsBuiltInErrorPageEnabled(BOOL enabled)
    */

static void ComHandlerCb(WebviewWnd* self, ICoreWebView2Controller* ctrl) {
    if (!ctrl) {
        self->FailInit();
        return;
    }
    self->OnControllerReady(ctrl);
}

static void OnBrowserMessageCb(WebviewWnd* self, const char* msg);

static void FailPendingWebviews() {
    std::vector<WebviewWnd*> pending = gPendingWebviews;
    gPendingWebviews.clear();
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

    std::vector<WebviewWnd*> pending = gPendingWebviews;
    gPendingWebviews.clear();
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
        L"--disable-features=AutofillServerCommunication,MediaRouter,OptimizationHints,Translate,CertificateTransparencyComponentUpdater "
        L"--metrics-recording-only "
        L"--no-pings");
    return options;
}

bool WebviewWnd::Embed(WebViewMsgCb& cb) {
    if (initStarted) {
        return true;
    }
    if (!dataDir) {
        logf("WebviewWnd::Embed: dataDir is null, aborting\n");
        initFailed = true;
        return false;
    }
    initStarted = true;
    userDataFolder = ToWStrTemp(dataDir);

    if (gSharedEnvState == SharedWebViewEnvState::Ready && gSharedEnvironment) {
        CreateControllerWithSharedEnvironment(this, cb);
        return !initFailed;
    }

    if (gSharedEnvState == SharedWebViewEnvState::Failed) {
        initFailed = true;
        return false;
    }

    gPendingWebviews.push_back(this);

    if (gSharedEnvState == SharedWebViewEnvState::NotStarted) {
        gSharedEnvState = SharedWebViewEnvState::Creating;
        gSharedUserDataFolder = userDataFolder;
        auto options = CreateOfflineEnvironmentOptions();
        auto* envHandler = new webview2_env_handler(OnSharedEnvironmentReady);
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, gSharedUserDataFolder.c_str(), options.Get(), envHandler);
        envHandler->Release();
        if (FAILED(hr)) {
            gSharedEnvState = SharedWebViewEnvState::Failed;
            FailPendingWebviews();
            return false;
        }
    } else if (gSharedEnvState == SharedWebViewEnvState::Creating) {
        if (gSharedUserDataFolder != userDataFolder) {
            logf("WebviewWnd::Embed: reusing shared WebView2 environment with first dataDir\n");
        }
    }

    return true;
}

void WebviewWnd::OnBrowserMessage(const char* msg) {
    /*
    auto seq = json_parse(msg, "id", 0);
    auto name = json_parse(msg, "method", 0);
    auto args = json_parse(msg, "params", 0);
    if (bindings.find(name) == bindings.end()) {
    return;
    }
    auto fn = bindings[name];
    (*fn->first)(seq, args, fn->second);
    */
    using prettysumatra::bridge::DispatchResult;
    DispatchResult res = prettysumatra::bridge::DispatchShellMessage(msg);
    if (res == DispatchResult::Disabled) {
        log(msg);
    }
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
    if (msg == WM_SIZE) {
        SetControllerVisible(wparam != SIZE_MINIMIZED);
        UpdateWebviewSize();
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
    if (webview) {
        webview->Release();
        webview = nullptr;
    }
    if (controller) {
        controller->Release();
        controller = nullptr;
    }
    str::Free(dataDir);
}
