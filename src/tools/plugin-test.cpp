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

void _uploadDebugReportIfFunc(bool, const char*) {
    // no-op implementation to satisfy SubmitBugReport()
}

// in order to host SumatraPDF as a plugin, create a (child) window and
// handle the following messages for it:
LRESULT CALLBACK PluginParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CREATE == msg) {
        // run SumatraPDF.exe with the -plugin command line argument
        PluginStartData* data = (PluginStartData*)((CREATESTRUCT*)lp)->lpCreateParams;
        AutoFreeStr cmdLine(str::Format("-plugin %d \"%s\"", hwnd, data->filePath));
        if (data->fileOriginUrl) {
            cmdLine.Set(str::Format("-plugin \"%s\" %d \"%s\"", data->fileOriginUrl, hwnd, data->filePath));
        }
        ShellExecute(hwnd, L"open", ToWstrTemp(data->sumatraPath), ToWstrTemp(cmdLine), nullptr, SW_SHOW);
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
            auto url(ToWstrTemp((const char*)cds->lpData));
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
    char* path = path::GetPathOfFileInAppDir("SumatraPDF.exe");
    if (!file::Exists(path))
        return str::Dup(L"SumatraPDF.exe");
    return ToWstr(path);
}

static void ParseCmdLine(const WCHAR* cmdLine, StrVec& args) {
    int nArgs = 0;
    WCHAR** argsArr = CommandLineToArgvW(cmdLine, &nArgs);
    for (int i = 0; i < nArgs; i++) {
        char* arg = ToUtf8Temp(argsArr[i]);
        args.Append(arg);
    }
    LocalFree(argsArr);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    StrVec argList;
    ParseCmdLine(GetCommandLine(), argList);

    if (argList.size() == 1) {
        AutoFreeStr msg(
            str::Format("Syntax: %s [<SumatraPDF.exe>] [<URL>] <filename.ext>", path::GetBaseNameTemp(argList.at(0))));
        MessageBoxA(nullptr, msg.Get(), PLUGIN_TEST_NAMEA, MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    if (argList.size() == 2 || !str::EndsWithI(argList.at(1), ".exe")) {
        argList.InsertAt(1, ToUtf8Temp(GetSumatraExePath()));
    }
    if (argList.size() == 3) {
        argList.InsertAt(2, nullptr);
    }

    WNDCLASS wc{};
    wc.lpfnWndProc = PluginParentWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = PLUGIN_TEST_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    PluginStartData data = {argList.at(1), argList.at(3), argList.at(2)};
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
