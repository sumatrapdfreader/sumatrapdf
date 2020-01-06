/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: BSD */

// this is a minimal example for how to use SumatraPDF in plugin mode

#include "utils/BaseUtil.h"
#include "utils/CmdLineParser.h"
#include "utils/FileUtil.h"

#define PLUGIN_TEST_NAME L"SumatraPDF Plugin Test"

struct PluginStartData {
    // path to SumatraPDF.exe
    const WCHAR* sumatraPath;
    // path to the (downloaded) document to display
    const WCHAR* filePath;
    // path/URL to display in the UI (to hide temporary paths)
    const WCHAR* fileOriginUrl;
};

// in order to host SumatraPDF as a plugin, create a (child) window and
// handle the following messages for it:
LRESULT CALLBACK PluginParentWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (WM_CREATE == msg) {
        // run SumatraPDF.exe with the -plugin command line argument
        PluginStartData* data = (PluginStartData*)((CREATESTRUCT*)lParam)->lpCreateParams;
        AutoFreeWstr cmdLine(str::Format(L"-plugin %d \"%s\"", hwnd, data->filePath));
        if (data->fileOriginUrl) {
            cmdLine.Set(str::Format(L"-plugin \"%s\" %d \"%s\"", data->fileOriginUrl, hwnd, data->filePath));
        }
        ShellExecute(hwnd, L"open", data->sumatraPath, cmdLine, nullptr, SW_SHOW);
    } else if (WM_SIZE == msg) {
        // resize the SumatraPDF window
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        if (hChild) {
            ClientRect rcClient(hwnd);
            MoveWindow(hChild, rcClient.x, rcClient.y, rcClient.dx, rcClient.dy, FALSE);
        } else {
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
        }
    } else if (WM_COPYDATA == msg) {
        // handle a URL to open externally (or prevent it)
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lParam;
        if (cds && 0x4C5255 /* URL */ == cds->dwData && (HWND)wParam == hChild) {
            AutoFreeWstr url(strconv::Utf8ToWstr((const char*)cds->lpData));
            ShellExecute(hChild, L"open", url, nullptr, nullptr, SW_SHOW);
            return TRUE;
        }
    } else if (WM_PAINT == msg) {
        // paint an error message (only needed if SumatraPDF couldn't be run)
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hwnd, &ps);
        RECT rcClient = ClientRect(hwnd).ToRECT();
        HBRUSH brushBg = CreateSolidBrush(0xCCCCCC);
        FillRect(hDC, &rcClient, brushBg);
        LOGFONTW lf = {0};
        lf.lfHeight = -14;
        str::BufSet(lf.lfFaceName, dimof(lf.lfFaceName), L"MS Shell Dlg");
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

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

WCHAR* GetSumatraExePath() {
    // run SumatraPDF.exe either from plugin-test.exe's or the current directory
    AutoFreeWstr path(path::GetPathOfFileInAppDir(L"SumatraPDF.exe"));
    if (!file::Exists(path))
        return str::Dup(L"SumatraPDF.exe");
    return path.StealData();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLineA, int nCmdShow) {
    UNUSED(hPrevInstance);
    UNUSED(lpCmdLineA);
    WStrVec argList;
    ParseCmdLine(GetCommandLine(), argList);

    if (argList.size() == 1) {
        AutoFreeWstr msg(str::Format(L"Syntax: %s [<SumatraPDF.exe>] [<URL>] <filename.ext>",
                                     path::GetBaseNameNoFree(argList.at(0))));
        MessageBox(nullptr, msg, PLUGIN_TEST_NAME, MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    if (argList.size() == 2 || !str::EndsWithI(argList.at(1), L".exe")) {
        argList.InsertAt(1, GetSumatraExePath());
    }
    if (argList.size() == 3) {
        argList.InsertAt(2, nullptr);
    }

    WNDCLASS wc = {0};
    wc.lpfnWndProc = PluginParentWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = PLUGIN_TEST_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    PluginStartData data = {argList.at(1), argList.at(3), argList.at(2)};
    HWND hwnd = CreateWindow(PLUGIN_TEST_NAME, PLUGIN_TEST_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT,
                             0, nullptr, nullptr, hInstance, &data);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
