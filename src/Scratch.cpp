/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// this is for adding temporary code for testing

// TODO: remove this
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/GuessFileType.h"
#include "utils/Timer.h"

#include "DisplayMode.h"
#include "Controller.h"
#include "PalmDbReader.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "Scratch.h"

#include "utils/Log.h"

#include "../../ext/unrar/dll.hpp"

/*
extern "C" {
#include <unarr.h>
}
*/

// META-INF/container.xml
static const char* metaInfContainerXML = R"(<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
   <rootfiles>
      <rootfile full-path="content.opf" media-type="application/oebps-package+xml"/>
      
   </rootfiles>
</container>)";

// mimetype

static const char* mimeType = R"(application/epub+zip)";

// content.opf
static const char* contentOpf = R"(<?xml version='1.0' encoding='utf-8'?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="uuid_id" version="2.0">
  <metadata xmlns:calibre="http://calibre.kovidgoyal.net/2009/metadata" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:opf="http://www.idpf.org/2007/opf" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    <dc:title>input</dc:title>
  </metadata>
  <manifest>
    <item href="input.htm" id="html" media-type="application/xhtml+xml"/>
    <item href="toc.ncx" id="ncx" media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx">
    <itemref idref="html"/>
  </spine>
</package>
)";

// toc.ncx
static const char* tocNcx = R"--(<?xml version='1.0' encoding='utf-8'?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1" xml:lang="en-US">
  <head>
    <meta content="d3bfbfa8-a0b5-4e1c-9297-297fb130bcbc" name="dtb:uid"/>
    <meta content="2" name="dtb:depth"/>
    <meta content="0" name="dtb:totalPageCount"/>
    <meta content="0" name="dtb:maxPageNumber"/>
  </head>
  <docTitle>
    <text>input</text>
  </docTitle>
  <navMap>
    <navPoint id="urvn6G6FwfJA4zq7INOgGaD" playOrder="1">
      <navLabel>
        <text>Start</text>
      </navLabel>
      <content src="input.htm"/>
    </navPoint>
  </navMap>
</ncx>
)--";

Vec<FileData*> MobiToEpub2(const WCHAR* path) {
    Vec<FileData*> res;
    MobiDoc* doc = MobiDoc::CreateFromFile(path);
    if (!doc) {
        return res;
    }
    auto d = doc->GetHtmlData();
    {
        auto e = new FileData();
        e->name = str::Dup("input.htm");
        e->data = d.Clone();
        res.Append(e);
        logf("name: '%s', size: %d, %d images\n", e->name, (int)e->data.size(), doc->imagesCount);
    }
    {
        auto e = new FileData();
        e->name = str::Dup("META-INF\\container.xml");
        e->data = ByteSlice(metaInfContainerXML).Clone();
        res.Append(e);
    }

    {
        auto e = new FileData();
        e->name = str::Dup("mimetype");
        e->data = ByteSlice(mimeType).Clone();
        res.Append(e);
    }

    {
        auto e = new FileData();
        e->name = str::Dup("content.opf");
        e->data = ByteSlice(contentOpf).Clone();
        res.Append(e);
    }

    {
        auto e = new FileData();
        e->name = str::Dup("toc.ncx");
        e->data = ByteSlice(tocNcx).Clone();
        res.Append(e);
    }

    for (size_t i = 1; i <= doc->imagesCount; i++) {
        auto imageData = doc->GetImage(i);
        if (!imageData) {
            logf("image %d is missing\n", (int)i);
            continue;
        }
        const WCHAR* ext = GfxFileExtFromData(*imageData);
        char* extA = ToUtf8Temp(ext).Get();
        logf("image %d, size: %d, ext: %s\n", (int)i, (int)imageData->size(), extA);
        auto e = new FileData();
        e->name = str::Format("image-%d%s", (int)i, extA);
        e->data = imageData->Clone();
        e->imageNo = (int)i;
        res.Append(e);
    }
    return res;
}

Vec<FileData*> MobiToEpub(const WCHAR* path) {
    auto files = MobiToEpub2(path);
    const WCHAR* dstDir = LR"(C:\Users\kjk\Downloads\mobiToEpub)";
    bool failed = false;
    for (auto& f : files) {
        if (failed) {
            break;
        }
        WCHAR* name = ToWstrTemp(f->name);
        AutoFreeWstr dstPath = path::Join(dstDir, name);
        bool ok = dir::CreateForFile(dstPath);
        if (!ok) {
            logf("Failed to create directory for file '%s'\n", ToUtf8Temp(dstPath).Get());
            failed = true;
            continue;
        }
        ok = file::WriteFile(dstPath.Get(), f->data);
        if (!ok) {
            logf("Failed to write '%s'\n", ToUtf8Temp(dstPath).Get());
            failed = true;
        } else {
            logf("Wrote '%s'\n", ToUtf8Temp(dstPath).Get());
        }
    }
    return files;
}

constexpr const WCHAR* rarFilePath =
    LR"__(x:\comics\!new4\Bride of Graphic Novels, Hardcovers and Trade Paperbacks\ABSOLUTE WATCHMEN (2005) (DC) (Minutemen-TheKid).cbr)__";

void LoadFile() {
    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("LoadFile() took %.2f ms\n", dur);
    };
    auto d = file::ReadFile(rarFilePath);
    free(d.data());
}

// return 1 on success. Other values for msg that we don't handle: UCM_CHANGEVOLUME, UCM_NEEDPASSWORD
static int CALLBACK unrarCallback2(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed) {
    if (UCM_PROCESSDATA != msg || !userData) {
        return -1;
    }
    str::Slice* buf = (str::Slice*)userData;
    size_t bytesGot = (size_t)bytesProcessed;
    if (bytesGot > buf->Left()) {
        return -1;
    }
    memcpy(buf->curr, (char*)rarBuffer, bytesGot);
    buf->curr += bytesGot;
    return 1;
}

void LoadRar() {
    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("LoadRar() took %.2f ms\n", dur);
    };

    str::Slice uncompressedBuf;

    RAROpenArchiveDataEx arcData = {nullptr};
    arcData.ArcNameW = (WCHAR*)rarFilePath;
    arcData.OpenMode = RAR_OM_EXTRACT;
    arcData.Callback = unrarCallback2;
    arcData.UserData = (LPARAM)&uncompressedBuf;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        return;
    }
    size_t fileId = 0;
    while (true) {
        RARHeaderDataEx rarHeader = {0};
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res) {
            break;
        }

        str::TransCharsInPlace(rarHeader.FileNameW, L"\\", L"/");
        auto name = ToUtf8Temp(rarHeader.FileNameW);

        size_t fileSizeUncompressed = (size_t)rarHeader.UnpSize;
        char* data = AllocArray<char>(fileSizeUncompressed + 3);
        if (!data) {
            return;
        }
        uncompressedBuf.Set(data, fileSizeUncompressed);
        RARProcessFile(hArc, RAR_EXTRACT, nullptr, nullptr);
    }

    RARCloseArchive(hArc);
}
#define WIN32_LEAN_AND_MEAN
#include <Shlwapi.h>
#include <codecvt>
#include <stdlib.h>
#include <windows.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Shlwapi.lib")

// EdgeHTML headers and libs
/*
#include <objbase.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.UI.Interop.h>
#pragma comment(lib, "windowsapp")
*/

// Edge/Chromium headers and libs
#include "WebView2.h"
#include "WebView2EnvironmentOptions.h"
#include "webview2.h"
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#define WEBVIEW_HINT_NONE 0  // Width and height are default size
#define WEBVIEW_HINT_MIN 1   // Width and height are minimum bounds
#define WEBVIEW_HINT_MAX 2   // Width and height are maximum bounds
#define WEBVIEW_HINT_FIXED 3 // Window size can not be changed by a user

using msg_cb_t = std::function<void(const std::string)>;
using dispatch_fn_t = std::function<void()>;

// Convert ASCII hex digit to a nibble (four bits, 0 - 15).
//
// Use unsigned to avoid signed overflow UB.
unsigned char hex2nibble(unsigned char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    } else if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return 0;
}

// Convert ASCII hex string (two characters) to byte.
//
// E.g., "0B" => 0x0B, "af" => 0xAF.
char hex2char(const char* p) {
    return hex2nibble(p[0]) * 16 + hex2nibble(p[1]);
}

std::string url_encode(const std::string s) {
    std::string encoded;
    for (unsigned int i = 0; i < s.length(); i++) {
        auto c = s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded = encoded + c;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02x", c);
            encoded = encoded + hex;
        }
    }
    return encoded;
}

std::string url_decode(const std::string st) {
    std::string decoded;
    const char* s = st.c_str();
    size_t length = strlen(s);
    for (unsigned int i = 0; i < length; i++) {
        if (s[i] == '%') {
            decoded.push_back(hex2char(s + i + 1));
            i = i + 2;
        } else if (s[i] == '+') {
            decoded.push_back(' ');
        } else {
            decoded.push_back(s[i]);
        }
    }
    return decoded;
}

std::string html_from_uri(const std::string s) {
    if (s.substr(0, 15) == "data:text/html,") {
        return url_decode(s.substr(15));
    }
    return "";
}

// Common interface for EdgeHTML and Edge/Chromium
class browser {
  public:
    virtual ~browser() = default;
    virtual bool embed(HWND, bool, msg_cb_t) = 0;
    virtual void navigate(const std::string url) = 0;
    virtual void eval(const std::string js) = 0;
    virtual void init(const std::string js) = 0;
    virtual void resize(HWND) = 0;
};

class edge_chromium : public browser {
  public:
    bool embed(HWND wnd, bool debug, msg_cb_t cb) override {
        CoInitializeEx(nullptr, 0);
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
        flag.test_and_set();

        char currentExePath[MAX_PATH];
        GetModuleFileNameA(NULL, currentExePath, MAX_PATH);
        char* currentExeName = PathFindFileNameA(currentExePath);

        // TODO: use our convert
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> wideCharConverter;
        std::wstring userDataFolder = wideCharConverter.from_bytes(std::getenv("APPDATA"));
        std::wstring currentExeNameW = wideCharConverter.from_bytes(currentExeName);

        HRESULT res = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, (userDataFolder + L"/" + currentExeNameW).c_str(), nullptr,
            new webview2_com_handler(wnd, cb, [&](ICoreWebView2Controller* controller) {
                m_controller = controller;
                m_controller->get_CoreWebView2(&m_webview);
                m_webview->AddRef();
                flag.clear();
            }));
        if (res != S_OK) {
            CoUninitialize();
            return false;
        }
        MSG msg = {};
        while (flag.test_and_set() && GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
        return true;
    }

    void resize(HWND wnd) override {
        if (m_controller == nullptr) {
            return;
        }
        RECT bounds;
        GetClientRect(wnd, &bounds);
        m_controller->put_Bounds(bounds);
    }

    void navigate(const std::string url) override {
        auto wurl = to_lpwstr(url);
        m_webview->Navigate(wurl);
        delete[] wurl;
    }

    void init(const std::string js) override {
        LPCWSTR wjs = to_lpwstr(js);
        m_webview->AddScriptToExecuteOnDocumentCreated(wjs, nullptr);
        delete[] wjs;
    }

    void eval(const std::string js) override {
        LPCWSTR wjs = to_lpwstr(js);
        m_webview->ExecuteScript(wjs, nullptr);
        delete[] wjs;
    }

  private:
    LPWSTR to_lpwstr(const std::string s) {
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        wchar_t* ws = new wchar_t[n];
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws, n);
        return ws;
    }

    ICoreWebView2* m_webview = nullptr;
    ICoreWebView2Controller* m_controller = nullptr;

    class webview2_com_handler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                                 public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                                 public ICoreWebView2WebMessageReceivedEventHandler,
                                 public ICoreWebView2PermissionRequestedEventHandler {
        using webview2_com_handler_cb_t = std::function<void(ICoreWebView2Controller*)>;

      public:
        webview2_com_handler(HWND hwnd, msg_cb_t msgCb, webview2_com_handler_cb_t cb)
            : m_window(hwnd), m_msgCb(msgCb), m_cb(cb) {
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
            LPWSTR message;
            args->TryGetWebMessageAsString(&message);

            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> wideCharConverter;
            m_msgCb(wideCharConverter.to_bytes(message));
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
        msg_cb_t m_msgCb;
        webview2_com_handler_cb_t m_cb;
    };
};

/*
HWND createBrowserTopLevelWindow() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    HICON icon = (HICON)LoadImage(hInstance, IDI_APPLICATION, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hInstance = hInstance;
    wc.lpszClassName = "webview";
    wc.hIcon = icon;
    wc.hIconSm = icon;
    wc.lpfnWndProc = (WNDPROC)(+[](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> int {
        auto w = (win32_edge_engine*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        switch (msg) {
            case WM_SIZE:
                w->m_browser->resize(hwnd);
                break;
            case WM_CLOSE:
                DestroyWindow(hwnd);
                break;
            case WM_DESTROY:
                w->terminate();
                break;
            case WM_GETMINMAXINFO: {
                auto lpmmi = (LPMINMAXINFO)lp;
                if (w == nullptr) {
                    return 0;
                }
                if (w->m_maxsz.x > 0 && w->m_maxsz.y > 0) {
                    lpmmi->ptMaxSize = w->m_maxsz;
                    lpmmi->ptMaxTrackSize = w->m_maxsz;
                }
                if (w->m_minsz.x > 0 && w->m_minsz.y > 0) {
                    lpmmi->ptMinTrackSize = w->m_minsz;
                }
            } break;
            default:
                return DefWindowProc(hwnd, msg, wp, lp);
        }
        return 0;
    });
    RegisterClassEx(&wc);
    m_window = CreateWindowW("webview", "", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr,
                             nullptr, GetModuleHandle(nullptr), nullptr);
    SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);
}
*/

class win32_edge_engine {
  public:
    win32_edge_engine(bool debug, HWND window) {
        m_window = window;

        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
        ShowWindow(m_window, SW_SHOW);
        UpdateWindow(m_window);
        SetFocus(m_window);

        auto cb = std::bind(&win32_edge_engine::on_message, this, std::placeholders::_1);

        if (!m_browser->embed(m_window, debug, cb)) {
            // TODO: need to fail
        }

        m_browser->resize(m_window);
    }

    void run() {
        MSG msg;
        BOOL res;
        while ((res = GetMessage(&msg, nullptr, 0, 0)) != -1) {
            if (msg.hwnd) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                continue;
            }
            if (msg.message == WM_APP) {
                auto f = (dispatch_fn_t*)(msg.lParam);
                (*f)();
                delete f;
            } else if (msg.message == WM_QUIT) {
                return;
            }
        }
    }
    void* window() {
        return (void*)m_window;
    }
    void terminate() {
        PostQuitMessage(0);
    }
    void dispatch(dispatch_fn_t f) {
        PostThreadMessage(m_main_thread, WM_APP, 0, (LPARAM) new dispatch_fn_t(f));
    }

    void set_title(const std::string title) {
        SetWindowTextA(m_window, title.c_str());
    }

    void set_size(int width, int height, int hints) {
        auto style = GetWindowLong(m_window, GWL_STYLE);
        if (hints == WEBVIEW_HINT_FIXED) {
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        } else {
            style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
        }
        SetWindowLong(m_window, GWL_STYLE, style);

        if (hints == WEBVIEW_HINT_MAX) {
            m_maxsz.x = width;
            m_maxsz.y = height;
        } else if (hints == WEBVIEW_HINT_MIN) {
            m_minsz.x = width;
            m_minsz.y = height;
        } else {
            RECT r;
            r.left = r.top = 0;
            r.right = width;
            r.bottom = height;
            AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, 0);
            SetWindowPos(m_window, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_FRAMECHANGED);
            m_browser->resize(m_window);
        }
    }

    void navigate(const std::string url) {
        m_browser->navigate(url);
    }
    void eval(const std::string js) {
        m_browser->eval(js);
    }
    void init(const std::string js) {
        m_browser->init(js);
    }

  private:
    virtual void on_message(const std::string msg) = 0;

    HWND m_window;
    POINT m_minsz = POINT{0, 0};
    POINT m_maxsz = POINT{0, 0};
    DWORD m_main_thread = GetCurrentThreadId();
    browser* m_browser = new edge_chromium();
};

using browser_engine = win32_edge_engine;

class webview : public browser_engine {
  public:
    webview(bool debug = false, HWND wnd = nullptr) : browser_engine(debug, wnd) {
    }

    void navigate(const std::string url) {
        if (url == "") {
            browser_engine::navigate("data:text/html," + url_encode("<html><body>Hello</body></html>"));
            return;
        }
        std::string html = html_from_uri(url);
        if (html != "") {
            browser_engine::navigate("data:text/html," + url_encode(html));
        } else {
            browser_engine::navigate(url);
        }
    }

    using binding_t = std::function<void(std::string, std::string, void*)>;
    using binding_ctx_t = std::pair<binding_t*, void*>;

    using sync_binding_t = std::function<std::string(std::string)>;
    using sync_binding_ctx_t = std::pair<webview*, sync_binding_t>;

    void bind(const std::string name, sync_binding_t fn) {
        bind(
            name,
            [](std::string seq, std::string req, void* arg) {
                auto pair = static_cast<sync_binding_ctx_t*>(arg);
                pair->first->resolve(seq, 0, pair->second(req));
            },
            new sync_binding_ctx_t(this, fn));
    }

    void bind(const std::string name, binding_t f, void* arg) {
        auto js = "(function() { var name = '" + name + "';" + R"(
      var RPC = window._rpc = (window._rpc || {nextSeq: 1});
      window[name] = function() {
        var seq = RPC.nextSeq++;
        var promise = new Promise(function(resolve, reject) {
          RPC[seq] = {
            resolve: resolve,
            reject: reject,
          };
        });
        window.external.invoke(JSON.stringify({
          id: seq,
          method: name,
          params: Array.prototype.slice.call(arguments),
        }));
        return promise;
      }
    })())";
        init(js);
        // bindings[name] = new binding_ctx_t(new binding_t(f), arg);
    }

    void resolve(const std::string seq, int status, const std::string result) {
        dispatch([=]() {
            if (status == 0) {
                eval("window._rpc[" + seq + "].resolve(" + result + "); window._rpc[" + seq + "] = undefined");
            } else {
                eval("window._rpc[" + seq + "].reject(" + result + "); window._rpc[" + seq + "] = undefined");
            }
        });
    }

  private:
    void on_message(const std::string) {
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
    }
    // std::map<std::string, binding_ctx_t*> bindings;
};

/*
void SetupWebView(HWND hwnd) {
    // Step 3 - Create a single WebView within the parent window
    // Locate the browser and set up the environment for WebView
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([hWnd](HRESULT result,
                                                                                    ICoreWebView2Environment* env)
                                                                                    -> HRESULT {
            // Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window
    hWnd env->CreateCoreWebView2Controller( hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [hWnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (controller != nullptr) {
                                    webviewController = controller;
                                    webviewController->get_CoreWebView2(&webviewWindow);
                                }

                                // Add a few settings for the webview
                                // The demo step is redundant since the values are the default
                                // settings
                                ICoreWebView2Settings* Settings;
                                webviewWindow->get_Settings(&Settings);
                                Settings->put_IsScriptEnabled(TRUE);
                                Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                Settings->put_IsWebMessageEnabled(TRUE);

                                // Resize WebView to fit the bounds of the parent window
                                RECT bounds;
                                GetClientRect(hWnd, &bounds);
                                webviewController->put_Bounds(bounds);

                                // Schedule an async task to navigate to Bing
                                webviewWindow->Navigate(L"https://www.bing.com/");

                                // Step 4 - Navigation events

                                // Step 5 - Scripting

                                // Step 6 - Communication between host and web content

                                return S_OK;
                            })
                            .Get());
            return S_OK;
        }).Get());
}
*/
