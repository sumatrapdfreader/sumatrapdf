/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

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
#include "wingui/WebView.h"

#include "AppTools.h"

#include "SimpleBrowserWindow.h"

SimpleBrowserWindow::~SimpleBrowserWindow() {
    delete webView;
}

LRESULT SimpleBrowserWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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

HWND SimpleBrowserWindow::Create(const SimpleBrowserCreateArgs& args) {
    HWND hwnd = nullptr;
    {
        CreateCustomArgs cargs;
        cargs.pos = args.pos;
        if (cargs.pos.IsZero()) {
            cargs.pos = {CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT};
        }
        cargs.title = args.title;
        if (!cargs.title) {
            cargs.title = "Browser Window";
        }
        // TODO: if set, navigate to url doesn't work
        // args.visible = false;
        hwnd = CreateCustom(cargs);
        ReportIf(!hwnd);
    }
    {
        Rect rc = ClientRect(hwnd);
        webView = new WebviewWnd();
        const char* dataDir = args.dataDir;
        if (!dataDir) {
            dataDir = GetPathInAppDataDirTemp("webViewData");
        }
        webView->dataDir = str::Dup(dataDir);
        CreateWebViewArgs cargs;
        cargs.parent = hwnd;
        cargs.pos = {10, 10, rc.dx - 20, rc.dy - 20};
        hwnd = webView->Create(cargs);
        if (!hwnd) {
            // ReportIfQuick(!hwnd);
            return nullptr;
        }
        webView->SetIsVisible(true);
    }
    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    webView->Navigate(args.url);
    SetIsVisible(true);
    return hwnd;
}

SimpleBrowserWindow* SimpleBrowserWindowCreate(const SimpleBrowserCreateArgs& args) {
    if (!HasWebView()) {
        return nullptr;
    }
    auto res = new SimpleBrowserWindow();
    auto hwnd = res->Create(args);
    ReportIfQuick(!hwnd);
    if (!hwnd) {
        delete res;
        return nullptr;
    }
    return res;
}
