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
#include "utils/ZipUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "PalmDbReader.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "AppTools.h"
#include "Scratch.h"

#include "utils/Log.h"

// ----------------

void TestUngzip() {
    const char* pathGz = R"(C:\Users\kjk\Downloads\AUTOSAR_TPS_SoftwareComponentTemplate.synctex.gz)";
    const char* path = R"(C:\Users\kjk\Downloads\AUTOSAR_TPS_SoftwareComponentTemplate.synctex)";
    ByteSlice uncomprFile = file::ReadFile(path);
    CrashIf(uncomprFile.empty());
    ByteSlice compr = file::ReadFile(pathGz);
    CrashIf(compr.empty());
    ByteSlice uncompr = Ungzip(compr);
    bool same = IsEqual(uncomprFile, uncompr);
    CrashIf(!same);
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
    if (msg == WM_SIZE && webView) {
        Rect rc = ClientRect(hwnd);
        rc.x += 10;
        rc.y += 10;
        rc.dx -= 20;
        rc.dy -= 20;
        webView->SetBounds(rc);
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
        args.title = "test browser";
        // TODO: if set, navigate to url doesn't work
        // args.visible = false;
        HWND hwnd = w->CreateCustom(args);
        CrashIf(!hwnd);
    }

    {
        Rect rc = ClientRect(w->hwnd);
        w->webView = new Webview2Wnd();
        w->webView->dataDir = str::Dup(AppGenDataFilenameTemp("webViewData"));
        CreateCustomArgs args;
        args.parent = w->hwnd;
        dx = rc.dx;
        dy = rc.dy;
        args.pos = {10, 10, dx - 20, dy - 20};
        HWND hwnd = w->webView->Create(args);
        CrashIf(!hwnd);
        w->webView->SetIsVisible(true);
    }

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->webView->Navigate("https://blog.kowalczyk.info/");
    w->SetIsVisible(true);
    RunMessageLoop(nullptr, w->hwnd);
    delete w;
}
