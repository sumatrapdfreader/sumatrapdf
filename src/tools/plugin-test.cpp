/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: BSD */

// this is a minimal example for how to use SumatraPDF in plugin mode

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/FileUtil.h"

#define PLUGIN_TEST_NAMEA "SumatraPDF Plugin Test"
#define PLUGIN_TEST_NAME L"SumatraPDF Plugin Test"

struct PluginStartData {
    // path to SumatraPDF.exe
    const char* sumatraPath;
    // path to the (downloaded) document to display
    const char* filePath;
    // path/URL to display in the UI (to hide temporary paths)
    const char* fileOriginUrl;
};

// in order to host SumatraPDF as a plugin, create a (child) window and
// handle the following messages for it:
LRESULT CALLBACK PluginParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CREATE == msg) {
        // run SumatraPDF.exe with the -plugin command line argument
        PluginStartData* data = (PluginStartData*)((CREATESTRUCT*)lp)->lpCreateParams;
        auto path = data->filePath;
        TempStr cmdLine = str::FormatTemp("-plugin %d \"%s\"", hwnd, path);
        if (data->fileOriginUrl) {
            cmdLine = str::FormatTemp("-plugin \"%s\" %d \"%s\"", data->fileOriginUrl, hwnd, path);
        }
        ShellExecute(hwnd, L"open", ToWStrTemp(data->sumatraPath), ToWStrTemp(cmdLine), nullptr, SW_SHOW);
    } else if (WM_SIZE == msg) {
        // resize the SumatraPDF window
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        if (hChild) {
            Rect rcClient = ClientRect(hwnd);
            MoveWindow(hChild, rcClient.x, rcClient.y, rcClient.dx, rcClient.dy, FALSE);
        } else {
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
        }
    } else if (WM_COPYDATA == msg) {
        // handle a URL to open externally (or prevent it)
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lp;
        if (cds && 0x4C5255 /* URL */ == cds->dwData && (HWND)wp == hChild) {
            auto url(ToWStrTemp((const char*)cds->lpData));
            ShellExecute(hChild, L"open", url, nullptr, nullptr, SW_SHOW);
            return TRUE;
        }
    } else if (WM_PAINT == msg) {
        // paint an error message (only needed if SumatraPDF couldn't be run)
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hwnd, &ps);
        RECT rcClient = ToRECT(ClientRect(hwnd));
        HBRUSH brushBg = CreateSolidBrush(0xCCCCCC);
        FillRect(hDC, &rcClient, brushBg);
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

WCHAR* GetSumatraExePath() {
    // run SumatraPDF.exe either from plugin-test.exe's or the current directory
    TempStr path = GetPathInExeDirTemp("SumatraPDF.exe");
    if (!file::Exists(path)) {
        return str::Dup(L"SumatraPDF.exe");
    }
    return ToWStr(path);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    StrVec argList;
    ParseCmdLine(GetCommandLineW(), argList);

    if (argList.Size() == 1) {
        TempStr name = path::GetBaseNameTemp(argList.At(0));
        TempStr msg = str::FormatTemp("Syntax: %s [<SumatraPDF.exe>] [<URL>] <filename.ext>", name);
        MsgBox(nullptr, msg, PLUGIN_TEST_NAMEA, MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    if (argList.Size() == 2 || !str::EndsWithI(argList.At(1), ".exe")) {
        argList.InsertAt(1, ToUtf8Temp(GetSumatraExePath()));
    }
    if (argList.Size() == 3) {
        argList.InsertAt(2, nullptr);
    }

    WNDCLASS wc{};
    wc.lpfnWndProc = PluginParentWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = PLUGIN_TEST_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    PluginStartData data = {argList.At(1), argList.At(3), argList.At(2)};
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
