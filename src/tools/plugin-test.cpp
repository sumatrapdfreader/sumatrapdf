/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: BSD */

// this is a minimal example for how to use SumatraPDF in plugin mode

#include "base/Base.h"
#include "base/Win.h"
#include "base/CmdLineArgsIter.h"
#include "base/File.h"

// base expects these from the host app; provide no-ops for this tool
void log(Str s) {
    if (!s) {
        return;
    }
    fwrite(s.s, 1, (size_t)s.len, stderr);
}
void loga(Str s) {
    log(s);
}
void _uploadDebugReport(Str, Str, bool, bool) {}

#define PLUGIN_TEST_NAMEA "SumatraPDF Plugin Test"
#define PLUGIN_TEST_NAME L"SumatraPDF Plugin Test"

struct PluginStartData {
    // path to SumatraPDF.exe
    Str sumatraPath;
    // path to the (downloaded) document to display
    Str filePath;
    // path/URL to display in the UI (to hide temporary paths)
    Str fileOriginUrl;
};

// in order to host SumatraPDF as a plugin, create a (child) window and
// handle the following messages for it:
LRESULT CALLBACK PluginParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CREATE == msg) {
        // run SumatraPDF.exe with the -plugin command line argument
        PluginStartData* data = (PluginStartData*)((CREATESTRUCT*)lp)->lpCreateParams;
        auto path = data->filePath;
        TempStr cmdLine = fmt("-plugin %d \"%s\"", hwnd, path);
        if (data->fileOriginUrl) {
            cmdLine = fmt("-plugin \"%s\" %d \"%s\"", data->fileOriginUrl, hwnd, path);
        }
        ShellExecute(hwnd, L"open", CWStrTemp(data->sumatraPath), CWStrTemp(cmdLine), nullptr, SW_SHOW);
    } else if (WM_SIZE == msg) {
        // resize the SumatraPDF window
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        if (hChild) {
            Rect rcClient = HwndClientRect(hwnd);
            MoveWindow(hChild, rcClient.x, rcClient.y, rcClient.dx, rcClient.dy, FALSE);
        } else {
            HwndInvalidate(hwnd, true);
            UpdateWindow(hwnd);
        }
    } else if (WM_COPYDATA == msg) {
        // handle a URL to open externally (or prevent it)
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lp;
        if (cds && 0x4C5255 /* URL */ == cds->dwData && (HWND)wp == hChild) {
            WCHAR* url = CWStrTemp(Str((const char*)cds->lpData));
            ShellExecute(hChild, L"open", url, nullptr, nullptr, SW_SHOW);
            return TRUE;
        }
    } else if (WM_PAINT == msg) {
        // paint an error message (only needed if SumatraPDF couldn't be run)
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hwnd, &ps);
        RECT rcClient = ToRECT(HwndClientRect(hwnd));
        HBRUSH brushBg = CreateSolidBrush(0xCCCCCC);
        HdcFillRect(hDC, ToRect(rcClient), brushBg);
        LOGFONTW lf{};
        lf.lfHeight = -14;
        str::BufSet(lf.lfFaceName, dimof(lf.lfFaceName), "MS Shell Dlg");
        HFONT hFont = CreateFontIndirectW(&lf);
        hFont = (HFONT)SelectObject(hDC, hFont);
        SetTextColor(hDC, 0x000000);
        SetBkMode(hDC, TRANSPARENT);
        DrawText(hDC, L"Error: Couldn't run SumatraPDF!", -1, &rcClient,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DeleteObject(SelectObject(hDC, hFont));
        DeleteObject(brushBg);
        EndPaint(hwnd, &ps);
    } else if (WM_DESTROY == msg) {
        // clean-up happens automatically when the window is destroyed
        PostQuitMessage(0);
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

WStr GetSumatraExePath() {
    // run SumatraPDF.exe either from plugin-test.exe's or the current directory
    TempStr path = GetPathInExeDirTemp("SumatraPDF.exe");
    if (!file::Exists(path)) {
        return wstr::Dup(WStrL(L"SumatraPDF.exe"));
    }
    return ToWStr(path);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    StrVec argList;
    ParseCmdLine(GetCommandLineW(), argList);

    if (len(argList) == 1) {
        TempStr name = path::GetBaseNameTemp(argList[0]);
        TempStr msg = fmt("Syntax: %s [<SumatraPDF.exe>] [<URL>] <filename.ext>", name);
        MsgBox(nullptr, msg, PLUGIN_TEST_NAMEA, MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    if (len(argList) == 2 || !str::EndsWithI(argList[1], StrL(".exe"))) {
        argList.InsertAt(1, ToUtf8Temp(GetSumatraExePath()));
    }
    if (len(argList) == 3) {
        argList.InsertAt(2, nullptr);
    }

    WNDCLASS wc{};
    wc.lpfnWndProc = PluginParentWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = PLUGIN_TEST_NAME;
    wc.hCursor = GetCachedCursor(IDC_ARROW);
    RegisterClass(&wc);

    PluginStartData data = {argList[1], argList[3], argList[2]};
    HWND hwnd = CreateWindowExW(0, PLUGIN_TEST_NAME, PLUGIN_TEST_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
                                CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, &data);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
