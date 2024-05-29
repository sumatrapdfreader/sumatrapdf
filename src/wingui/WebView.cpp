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

//#include "Theme.h"

#include "webview2.h"
#include "wingui/WebView.h"

#include "utils/Log.h"

Kind kindWebView = "webView";

TempStr GetWebView2VersionTemp() {
    WCHAR* ver = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
    if (FAILED(hr) || (ver == nullptr)) {
        return nullptr;
    }
    TempStr res = ToUtf8Temp(ver);
    return res;
}

class webview2_com_handler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
public ICoreWebView2WebMessageReceivedEventHandler,
public ICoreWebView2PermissionRequestedEventHandler {
    using webview2_com_handler_cb_t = std::function<void(ICoreWebView2Controller*)>;

public:
    webview2_com_handler(HWND hwnd, WebViewMsgCb msgCb, webview2_com_handler_cb_t cb)
    : m_window(hwnd), msgCb(msgCb), m_cb(cb) {
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release() {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* ppv) {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Environment* env) {
        env->CreateCoreWebView2Controller(m_window, this);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Controller* controller) {
        controller->AddRef();

        ICoreWebView2* webview;
        ::EventRegistrationToken token;
        controller->get_CoreWebView2(&webview);
        webview->add_WebMessageReceived(this, &token);
        webview->add_PermissionRequested(this, &token);

        m_cb(controller);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
        WCHAR* message = nullptr;
        args->TryGetWebMessageAsString(&message);
        if (!message) {
            return S_OK;
        }
        char* s = ToUtf8Temp(message);
        msgCb(s);
        sender->PostWebMessageAsString(message);
        CoTaskMemFree(message);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2PermissionRequestedEventArgs* args) {
        COREWEBVIEW2_PERMISSION_KIND kind;
        args->get_PermissionKind(&kind);
        if (kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ) {
            args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
        }
        return S_OK;
    }

private:
    HWND m_window;
    WebViewMsgCb msgCb;
    webview2_com_handler_cb_t m_cb;
};

Webview2Wnd::Webview2Wnd() {
    kind = kindWebView;
}

void Webview2Wnd::UpdateWebviewSize() {
    if (controller == nullptr) {
        return;
    }
    RECT bounds = ClientRECT(hwnd);
    controller->put_Bounds(bounds);
}

void Webview2Wnd::Eval(const char* js) {
    TempWStr ws = ToWStrTemp(js);
    webview->ExecuteScript(ws, nullptr);
}

void Webview2Wnd::SetHtml(const char* html) {
    #if 0
    std::string s = "data:text/html,";
    s += url_encode(html);
    WCHAR* html2 = ToWStrTemp(s.c_str());
    m_webview->Navigate(html2);
    #else
    WCHAR* html2 = ToWStrTemp(html);
    webview->NavigateToString(html2);
    #endif
}

void Webview2Wnd::Init(const char* js) {
    TempWStr ws = ToWStrTemp(js);
    webview->AddScriptToExecuteOnDocumentCreated(ws, nullptr);
}

void Webview2Wnd::Navigate(const char* url) {
    TempWStr ws = ToWStrTemp(url);
    webview->Navigate(ws);
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

bool Webview2Wnd::Embed(WebViewMsgCb cb) {
    // TODO: not sure if flag needs to be atomic i.e. is CreateCoreWebView2EnvironmentWithOptions()
    // called on a different thread?
    volatile LONG flag = 0;
    // InterlockedCompareExchange()
    WCHAR* userDataFolder = ToWStrTemp(dataDir);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder, nullptr, new webview2_com_handler(hwnd, cb, [&](ICoreWebView2Controller* ctrl) {
            controller = ctrl;
            controller->get_CoreWebView2(&webview);
            webview->AddRef();
            InterlockedAdd(&flag, 1);
    }));
    if (hr != S_OK) {
        return false;
    }
    MSG msg = {};
    while ((InterlockedAdd(&flag, 0) == 0) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // remove window frame and decorations
    auto style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_OVERLAPPEDWINDOW);
    SetWindowLong(hwnd, GWL_STYLE, style);

    ICoreWebView2Settings* settings = nullptr;
    hr = webview->get_Settings(&settings);
    if (hr == S_OK) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
    }

    Init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
    return true;
}

void Webview2Wnd::OnBrowserMessage(const char* msg) {
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
    log(msg);
}

HWND Webview2Wnd::Create(const CreateCustomArgs& args) {
    ReportIf(!dataDir);
    CreateCustom(args);
    if (!hwnd) {
        return nullptr;
    }

    auto cb = std::bind(&Webview2Wnd::OnBrowserMessage, this, std::placeholders::_1);
    Embed(cb);
    UpdateWebviewSize();
    return hwnd;
}

LRESULT Webview2Wnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_SIZE) {
        UpdateWebviewSize();
    }
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

Webview2Wnd::~Webview2Wnd() {
    str::Free(dataDir);
}
