/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/wingui2.h"
#include "wingui/Window.h"

#include "Settings.h"
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
        RARHeaderDataEx rarHeader{};
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

// ----------------

#define WIN32_LEAN_AND_MEAN
#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <windows.h>
#include <atomic>

#include "webview2.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "windowsapp")
#pragma comment(lib, "shell32.lib")

using msg_cb_t = std::function<void(const char*)>;
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

using namespace wg;

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
        WCHAR* message = nullptr;
        args->TryGetWebMessageAsString(&message);
        if (!message) {
            return S_OK;
        }
        char* s = ToUtf8Temp(message);
        m_msgCb(s);
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

struct Webview2Wnd : Wnd {
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void UpdateWebviewSize() {
        if (m_controller == nullptr) {
            return;
        }
        RECT bounds;
        GetClientRect(hwnd, &bounds);
        m_controller->put_Bounds(bounds);
    }

    void Eval(const char* js) {
        WCHAR* ws = ToWstrTemp(js);
        m_webview->ExecuteScript(ws, nullptr);
    }

    void SetHtml(const char* html) {
        std::string s = "data:text/html,";
        s += url_encode(html);
        WCHAR* html2 = ToWstrTemp(s.c_str());
        m_webview->Navigate(html2);
    }

    void Init(const char* js) {
        WCHAR* ws = ToWstrTemp(js);
        m_webview->AddScriptToExecuteOnDocumentCreated(ws, nullptr);
    }

    void Navigate(const char* url) {
        WCHAR* ws = ToWstrTemp(url);
        m_webview->Navigate(ws);
    }

    bool Embed(msg_cb_t cb) {
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
        flag.test_and_set();

        wchar_t currentExePath[MAX_PATH];
        GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
        wchar_t* currentExeName = PathFindFileNameW(currentExePath);

        wchar_t dataPath[MAX_PATH];
        if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, dataPath))) {
            return false;
        }
        wchar_t userDataFolder[MAX_PATH];
        PathCombineW(userDataFolder, dataPath, currentExeName);

        HRESULT res = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, userDataFolder, nullptr,
            new webview2_com_handler(hwnd, cb, [&](ICoreWebView2Controller* controller) {
                m_controller = controller;
                m_controller->get_CoreWebView2(&m_webview);
                m_webview->AddRef();
                flag.clear();
            }));
        if (res != S_OK) {
            return false;
        }
        MSG msg = {};
        while (flag.test_and_set() && GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

#if 0
        {
            auto style = GetWindowLong(hwnd, GWL_STYLE);
            style &= ~(WS_OVERLAPPEDWINDOW);
            SetWindowLong(hwnd, GWL_STYLE, style);
        }
#endif

#if 0
        {
            auto style = GetWindowLong(hwnd, GWL_EXSTYLE);
            style &= ~(WS_EX_OVERLAPPEDWINDOW);
            SetWindowLong(hwnd, GWL_EXSTYLE, style);
        }
#endif
        Init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
        return true;
    }

    virtual void OnBrowserMessage(const char* msg);

    DWORD m_main_thread = GetCurrentThreadId();
    ICoreWebView2* m_webview = nullptr;
    ICoreWebView2Controller* m_controller = nullptr;

    HWND Create(const CreateCustomArgs&);
};

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

struct BrowserTestWnd : Wnd {
    Webview2Wnd* webView = nullptr;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    ~BrowserTestWnd() {
        delete webView;
    }
};

LRESULT BrowserTestWnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_CLOSE) {
        OnClose();
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

void TestBrowser() {
    int dx = 480;
    int dy = 640;
    auto w = new BrowserTestWnd();
    {
        CreateCustomArgs args;
        args.pos = {CW_USEDEFAULT, CW_USEDEFAULT, dx, dy};
        args.title = L"test browser";
        args.visible = false;
        HWND hwnd = w->CreateCustom(args);
        CrashIf(!hwnd);
    }

    {
        //Rect rc = ClientRect(w->hwnd);
        w->webView = new Webview2Wnd();
        CreateCustomArgs args;
        args.parent = w->hwnd;
        //dx = rc.dx;
        //dy = rc.dy;
        args.pos = {10, 10, dx - 20, dy - 20};
        HWND hwnd = w->webView->Create(args);
        CrashIf(!hwnd);
        w->webView->SetIsVisible(true);
    }

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->webView->Navigate("https://blog.kowalczyk.info/");
    w->SetIsVisible(true);
    auto res = RunMessageLoop(nullptr, w->hwnd);
    delete w;
}
