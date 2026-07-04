/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Plugin test: hosts SumatraPDF in plugin mode inside a child window.
// Activated with -test-plugin [<SumatraPDF.exe>] [<URL>] <filename.ext>
// Only available in debug builds.

#include "base/Base.h"
#include "base/Win.h"
#include "base/CmdLineArgsIter.h"

#define PLUGIN_TEST_NAME L"SumatraPDF Plugin Test"

struct PluginStartData {
    Str sumatraPath;
    Str filePath;
    Str fileOriginUrl;
};

constexpr UINT_PTR kPluginCheckTimerID = 1;
constexpr int kPluginTimeoutMs = 10000;

static bool gPluginTimedOut = false;

static LRESULT CALLBACK PluginParentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CREATE == msg) {
        PluginStartData* data = (PluginStartData*)((CREATESTRUCT*)lp)->lpCreateParams;
        auto path = data->filePath;
        TempStr cmdLine = fmt("-plugin %lld \"%s\"", (long long)(INT_PTR)hwnd, path);
        if (data->fileOriginUrl) {
            cmdLine = fmt("-plugin \"%s\" %lld \"%s\"", data->fileOriginUrl, (long long)(INT_PTR)hwnd, path);
        }
        TempStr fullCmd = fmt("\"%s\" %s", data->sumatraPath, cmdLine);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        CreateProcessW(nullptr, CWStrTemp(fullCmd), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
        if (pi.hProcess) {
            CloseHandle(pi.hProcess);
        }
        if (pi.hThread) {
            CloseHandle(pi.hThread);
        }
        // start a timer to detect if SumatraPDF fails to attach
        SetTimer(hwnd, kPluginCheckTimerID, kPluginTimeoutMs, nullptr);
    } else if (WM_TIMER == msg && wp == kPluginCheckTimerID) {
        KillTimer(hwnd, kPluginCheckTimerID);
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        if (!hChild) {
            gPluginTimedOut = true;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
    } else if (WM_SIZE == msg) {
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        if (hChild) {
            Rect rcClient = ClientRect(hwnd);
            MoveWindow(hChild, rcClient.x, rcClient.y, rcClient.dx, rcClient.dy, FALSE);
        }
    } else if (WM_COPYDATA == msg) {
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lp;
        if (cds && 0x4C5255 /* URL */ == cds->dwData && (HWND)wp == hChild) {
            int urlLen = (int)cds->cbData;
            char* urlData = (char*)cds->lpData;
            if (urlLen > 0 && urlData[urlLen - 1] == 0) {
                urlLen--;
            }
            Str urlZ(urlData, urlLen);
            WCHAR* url = CWStrTemp(urlZ);
            ShellExecute(hChild, L"open", url, nullptr, nullptr, SW_SHOW);
            return TRUE;
        }
    } else if (WM_PAINT == msg) {
        HWND hChild = FindWindowEx(hwnd, nullptr, nullptr, nullptr);
        if (!hChild) {
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
            WStr text = gPluginTimedOut ? WStr(L"Error: SumatraPDF didn't attach") : WStr(L"Waiting for SumatraPDF...");
            DrawText(hDC, text.s, -1, &rcClient, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            DeleteObject(SelectObject(hDC, hFont));
            DeleteObject(brushBg);
            EndPaint(hwnd, &ps);
        } else {
            // child is attached, kill timer if still running
            KillTimer(hwnd, kPluginCheckTimerID);
            return DefWindowProc(hwnd, msg, wp, lp);
        }
    } else if (WM_DESTROY == msg) {
        KillTimer(hwnd, kPluginCheckTimerID);
        PostQuitMessage(0);
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

// Parse args after -test-plugin: [<SumatraPDF.exe>] [<URL>] <filename.ext>
void TestPlugin(WStr cmdLine) {
    StrVec argList;
    ParseCmdLine(cmdLine, argList);

    // find the position of -test-plugin and take args after it
    int pluginIdx = -1;
    for (int i = 0; i < len(argList); i++) {
        if (str::EqI(argList[i], "-test-plugin")) {
            pluginIdx = i;
            break;
        }
    }

    StrVec args;
    if (pluginIdx >= 0) {
        for (int i = pluginIdx + 1; i < len(argList); i++) {
            args.Append(argList[i]);
        }
    }

    if (len(args) == 0) {
        MsgBox(nullptr, "Syntax: SumatraPDF.exe -test-plugin [<SumatraPDF.exe>] [<URL>] <filename.ext>",
               "SumatraPDF Plugin Test", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // if no exe path given or first arg doesn't end with .exe, use our own exe
    if (len(args) == 1 || !str::EndsWithI(args[0], ".exe")) {
        TempStr selfPath = GetSelfExePathTemp();
        args.InsertAt(0, selfPath);
    }
    // if no URL given (only exe + file), insert nullptr for URL
    if (len(args) == 2) {
        args.InsertAt(1, nullptr);
    }

    if (len(args) < 3) {
        MsgBox(nullptr, "Syntax: SumatraPDF.exe -test-plugin [<SumatraPDF.exe>] [<URL>] <filename.ext>",
               "SumatraPDF Plugin Test", MB_OK | MB_ICONINFORMATION);
        return;
    }

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASS wc{};
    wc.lpfnWndProc = PluginParentWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = PLUGIN_TEST_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    PluginStartData data = {args[0], args[2], args[1]};
    HWND hwnd = CreateWindowExW(0, PLUGIN_TEST_NAME, PLUGIN_TEST_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
                                CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, &data);
    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
