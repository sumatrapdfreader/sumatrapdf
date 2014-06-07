/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: BSD */

// this is a minimal example for how to use SumatraPDF in plugin mode

#include "BaseUtil.h"
#include "CmdLineParser.h"
#include "FileUtil.h"

#define PLUGIN_TEST_NAME L"SumatraPDF Plugin Test"

struct PluginStartData {
    // path to SumatraPDF.exe
    const WCHAR *sumatraPath;
    // path to the (downloaded) document to display
    const WCHAR *filePath;
    // path/URL to display in the UI (to hide temporary paths)
    const WCHAR *fileOriginUrl;
};

// in order to host SumatraPDF as a plugin, create a (child) window and
// handle the following messages for it:
LRESULT CALLBACK PluginParentWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (WM_CREATE == msg) {
        // run SumatraPDF.exe with the -plugin command line argument
        PluginStartData *data = (PluginStartData *)((CREATESTRUCT *)lParam)->lpCreateParams;
        ScopedMem<WCHAR> cmdLine(str::Format(L"-plugin %d \"%s\"", hwnd, data->filePath));
        if (data->fileOriginUrl) {
            cmdLine.Set(str::Format(L"-plugin \"%s\" %d \"%s\"", data->fileOriginUrl, hwnd, data->filePath));
        }
        ShellExecute(hwnd, L"open", data->sumatraPath, cmdLine, NULL, SW_SHOW);
    }
    else if (WM_SIZE == msg) {
        // resize the SumatraPDF window
        HWND hChild = FindWindowEx(hwnd, NULL, NULL, NULL);
        if (hChild) {
            ClientRect rcClient(hwnd);
            MoveWindow(hChild, rcClient.x, rcClient.y, rcClient.dx, rcClient.dy, FALSE);
        }
        else {
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
        }
    }
    else if (WM_COPYDATA == msg) {
        // handle a URL to open externally (or prevent it)
        HWND hChild = FindWindowEx(hwnd, NULL, NULL, NULL);
        COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lParam;
        if (cds && 0x4C5255 /* URL */ == cds->dwData && (HWND)wParam == hChild) {
            ScopedMem<WCHAR> url(str::conv::FromUtf8((const char *)cds->lpData));
            ShellExecute(hChild, L"open", url, NULL, NULL, SW_SHOW);
            return TRUE;
        }
    }
    else if (WM_PAINT == msg) {
        // paint an error message (only needed if SumatraPDF couldn't be run)
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hwnd, &ps);
        RECT rcClient = ClientRect(hwnd).ToRECT();
        HBRUSH brushBg = CreateSolidBrush(0xCCCCCC);
        FillRect(hDC, &rcClient, brushBg);
        LOGFONT lf = { 0 };
        lf.lfHeight = -14;
        str::BufSet(lf.lfFaceName, dimof(lf.lfFaceName), L"MS Shell Dlg");
        HFONT hFont = CreateFontIndirect(&lf);
        hFont = (HFONT)SelectObject(hDC, hFont);
        SetTextColor(hDC, 0x000000);
        SetBkMode(hDC, TRANSPARENT);
        DrawText(hDC, L"Error: Couldn't run SumatraPDF!", -1, &rcClient, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DeleteObject(SelectObject(hDC, hFont));
        DeleteObject(brushBg);
        EndPaint(hwnd, &ps);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

WCHAR *GetSumatraExePath()
{
    WCHAR buf[MAX_PATH];
    buf[0] = 0;
    GetModuleFileName(NULL, buf, dimof(buf));
    buf[max(path::GetBaseName(buf) - buf - 1, 0)] = 0;
    ScopedMem<WCHAR> path(path::Join(buf, L"SumatraPDF.exe"));
    // run SumatraPDF.exe either from plugin-test.exe's or the current directory
    if (!file::Exists(path))
        return str::Dup(L"SumatraPDF.exe");
    return path.StealData();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLineA, int nCmdShow)
{
    WStrVec argList;
    ParseCmdLine(GetCommandLine(), argList);

    if (argList.Count() == 1) {
        ScopedMem<WCHAR> msg(str::Format(L"Syntax: %s [<SumatraPDF.exe>] [<URL>] <filename.ext>", path::GetBaseName(argList.At(0))));
        MessageBox(NULL, msg, PLUGIN_TEST_NAME, MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    if (argList.Count() == 2 || !str::EndsWithI(argList.At(1), L".exe")) {
        argList.InsertAt(1, GetSumatraExePath());
    }
    if (argList.Count() == 3) {
        argList.InsertAt(2, NULL);
    }

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = PluginParentWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = PLUGIN_TEST_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    PluginStartData data = { argList.At(1), argList.At(3), argList.At(2) };
    HWND hwnd = CreateWindow(PLUGIN_TEST_NAME, PLUGIN_TEST_NAME,
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
                             NULL, NULL, hInstance,
                             &data);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}
