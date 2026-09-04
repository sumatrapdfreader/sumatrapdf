/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/CmdLineArgs.h"
#include "base/Win.h"
#include "base/GuessFileType.h"
#include "base/UITask.h"

#include <exdisp.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Flags.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Toolbar.h"
#include "AppSettings.h"
#include "ExplorerQuickLook.h"
#include "SumatraLog.h"

constexpr const WCHAR* kQuickLookAgentClass = L"SUMATRA_PDF_QUICKLOOK_AGENT";
constexpr const WCHAR* kQuickLookAgentMutexName = L"SumatraPDF-QuickLookAgent";
#define kQuickLookRunValue StrL("SumatraPDF-QuickLook")
#define kQuickLookRunKey StrL("Software\\Microsoft\\Windows\\CurrentVersion\\Run")
constexpr UINT kMsgQuickLookSpace = WM_APP + 40;

static HWND gQuickLookAgentHwnd = nullptr;
static HHOOK gQuickLookHook = nullptr;
static HANDLE gQuickLookAgentMutex = nullptr;

MainWindow* FindExplorerQuickLookWindow() {
    for (MainWindow* win : gWindows) {
        if (win->isQuickLook && IsMainWindowValidAndNotClosing(win)) {
            return win;
        }
    }
    return nullptr;
}

void ApplyExplorerQuickLookChrome(MainWindow* win) {
    if (!win || !win->hwndFrame) {
        return;
    }
    HWND hwnd = win->hwndFrame;
    HWND fg = GetForegroundWindow();
    Rect work = GetWorkAreaRect(HwndWindowRect(fg ? fg : hwnd), fg ? fg : hwnd);
    int dx = work.dx * 85 / 100;
    int dy = work.dy * 85 / 100;
    if (dx < 400) {
        dx = work.dx;
    }
    if (dy < 300) {
        dy = work.dy;
    }
    Rect r{work.x + (work.dx - dx) / 2, work.y + (work.dy - dy) / 2, dx, dy};
    HwndMoveWindow(hwnd, &r);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ShowOrHideToolbar(win);
    SetSidebarVisibility(win, false, false);
    win->tabsVisible = false;
    ScheduleUiUpdate(win);
}

static bool PathIsSupportedPreview(Str path) {
    if (len(path) == 0 || !file::Exists(path)) {
        return false;
    }
    FileType kind = GuessFileTypeFromFile(path);
    return IsSupportedFileType(kind, true);
}

void ShowExplorerQuickLook(Str path) {
    if (len(path) == 0) {
        return;
    }
    TempStr norm = path::NormalizeTemp(path);
    if (!PathIsSupportedPreview(norm)) {
        return;
    }

    MainWindow* existing = FindExplorerQuickLookWindow();
    if (existing) {
        WindowTab* tab = existing->CurrentTab();
        if (tab && tab->filePath && path::IsSame(tab->filePath, norm)) {
            CloseWindow(existing, true, false);
            return;
        }
        LoadArgs args(norm, existing);
        args.forceReuse = true;
        args.noSavePrefs = true;
        LoadDocument(&args);
        SetForegroundWindow(existing->hwndFrame);
        return;
    }

    MainWindow* win = CreateAndShowMainWindow(nullptr, false);
    if (!win) {
        return;
    }
    win->isQuickLook = true;
    LoadArgs args(norm, win);
    args.showWin = false;
    args.noSavePrefs = true;
    LoadDocument(&args);
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    ApplyExplorerQuickLookChrome(win);
    ShowWindow(win->hwndFrame, SW_SHOW);
    SetForegroundWindow(win->hwndFrame);
}

bool SendExplorerQuickLookToExisting(HWND hwnd, Str path) {
    if (!hwnd || len(path) == 0 || !IsWindow(hwnd)) {
        return false;
    }
    TempStr pathZ = str::DupTemp(path);
    COPYDATASTRUCT cds{};
    cds.dwData = kCopyDataQuickLook;
    cds.cbData = (DWORD)pathZ.len + 1;
    cds.lpData = (void*)pathZ.s;
    LRESULT res = SendMessageW(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    return res != FALSE;
}

struct QuickLookCopyDataAsync {
    Str path;
};

static void QuickLookCopyDataAsyncRun(QuickLookCopyDataAsync* d) {
    ShowExplorerQuickLook(d->path);
    str::Free(d->path);
    delete d;
}

bool HandleExplorerQuickLookCopyData(COPYDATASTRUCT* cds) {
    if (!cds || cds->dwData != kCopyDataQuickLook || !cds->lpData || cds->cbData < 1) {
        return false;
    }
    Str pathZ = Str((char*)cds->lpData, (int)cds->cbData);
    if (strnlen_s(pathZ.s, (size_t)cds->cbData) >= cds->cbData) {
        return false;
    }
    auto* d = new QuickLookCopyDataAsync;
    d->path = str::Dup(pathZ);
    uitask::Post(MkFunc0<QuickLookCopyDataAsync>(QuickLookCopyDataAsyncRun, d), "QuickLookCopyData");
    return true;
}

static HWND FindExistingFrameHwnd() {
    return FindWindowW(kFrameClassName, nullptr);
}

static void LaunchQuickLookProcess(Str path) {
    TempStr exe = GetSelfExePathTemp();
    TempStr cmd = fmt("%s -quicklook %s", QuoteCmdLineArgTemp(exe), QuoteCmdLineArgTemp(path));
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, CWStrTemp(cmd), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        logf("LaunchQuickLookProcess: CreateProcess failed for '%s'\n", path);
    }
}

static void RequestQuickLook(Str path) {
    HWND frame = FindExistingFrameHwnd();
    if (frame && SendExplorerQuickLookToExisting(frame, path)) {
        return;
    }
    LaunchQuickLookProcess(path);
}

static bool ClassEq(HWND hwnd, const WCHAR* name) {
    WCHAR cls[96]{};
    if (!hwnd || !GetClassNameW(hwnd, cls, dimof(cls))) {
        return false;
    }
    return wstr::EqI(WStr(cls), WStr(name));
}

static bool FocusLooksLikeTyping(HWND focus) {
    if (!focus) {
        return false;
    }
    WCHAR cls[96]{};
    if (!GetClassNameW(focus, cls, dimof(cls))) {
        return false;
    }
    TempStr utf = ToUtf8Temp(WStr(cls));
    return str::ContainsI(utf, StrL("Edit")) || str::ContainsI(utf, StrL("Search")) ||
           str::EqI(utf, StrL("ComboBox")) || str::EqI(utf, StrL("ComboBoxEx32"));
}

static bool IsExplorerOrDesktopHwnd(HWND hwnd) {
    return ClassEq(hwnd, L"CabinetWClass") || ClassEq(hwnd, L"ExploreWClass") || ClassEq(hwnd, L"Progman") ||
           ClassEq(hwnd, L"WorkerW");
}

static HWND ExplorerTopLevel() {
    HWND fg = GetForegroundWindow();
    HWND h = fg;
    while (h) {
        if (IsExplorerOrDesktopHwnd(h)) {
            return h;
        }
        h = GetParent(h);
    }
    return nullptr;
}

static bool ExplorerForegroundAllowsPreview() {
    HWND top = ExplorerTopLevel();
    if (!top) {
        return false;
    }
    MainWindow* ours = FindMainWindowByHwnd(GetForegroundWindow());
    if (ours) {
        return false;
    }
    GUITHREADINFO gi{};
    gi.cbSize = sizeof(gi);
    DWORD tid = GetWindowThreadProcessId(top, nullptr);
    if (GetGUIThreadInfo(tid, &gi)) {
        if (gi.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE)) {
            return false;
        }
        if (FocusLooksLikeTyping(gi.hwndFocus)) {
            return false;
        }
    }
    return true;
}

static TempStr PathFromFolderViewTemp(IFolderView* fv) {
    if (!fv) {
        return {};
    }
    int idx = -1;
    HRESULT hr = fv->GetFocusedItem(&idx);
    if (FAILED(hr) || idx < 0) {
        return {};
    }
    ScopedComPtr<IPersistFolder2> persist;
    hr = fv->GetFolder(IID_PPV_ARGS(&persist));
    if (FAILED(hr) || !persist) {
        return {};
    }
    PIDLIST_ABSOLUTE folderPidl = nullptr;
    hr = persist->GetCurFolder(&folderPidl);
    if (FAILED(hr) || !folderPidl) {
        return {};
    }
    LPITEMIDLIST relPidl = nullptr;
    hr = fv->Item(idx, &relPidl);
    TempStr out;
    if (SUCCEEDED(hr) && relPidl) {
        PIDLIST_ABSOLUTE full = ILCombine(folderPidl, relPidl);
        if (full) {
            WCHAR pathW[MAX_PATH]{};
            if (SHGetPathFromIDListW(full, pathW)) {
                out = ToUtf8Temp(WStr(pathW));
            }
            CoTaskMemFree(full);
        }
        CoTaskMemFree(relPidl);
    }
    CoTaskMemFree(folderPidl);
    return out;
}

static TempStr SelectedPathFromBrowserTemp(IWebBrowserApp* app) {
    ScopedComQIPtr<IServiceProvider> sp(app);
    if (!sp) {
        return {};
    }
    ScopedComPtr<IShellBrowser> browser;
    HRESULT hr = sp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
    if (FAILED(hr) || !browser) {
        return {};
    }
    ScopedComPtr<IShellView> view;
    hr = browser->QueryActiveShellView(&view);
    if (FAILED(hr) || !view) {
        return {};
    }
    ScopedComQIPtr<IFolderView> fv(view);
    return PathFromFolderViewTemp(fv);
}

static TempStr GetExplorerSelectedPathTemp() {
    HWND top = ExplorerTopLevel();
    if (!top) {
        return {};
    }
    bool wantDesktop = ClassEq(top, L"Progman") || ClassEq(top, L"WorkerW");
    ScopedComPtr<IShellWindows> windows;
    if (!windows.Create(CLSID_ShellWindows)) {
        return {};
    }
    long count = 0;
    windows->get_Count(&count);
    for (long i = 0; i < count; i++) {
        VARIANT idx;
        VariantInit(&idx);
        idx.vt = VT_I4;
        idx.lVal = i;
        ScopedComPtr<IDispatch> disp;
        HRESULT hr = windows->Item(idx, &disp);
        if (FAILED(hr) || !disp) {
            continue;
        }
        ScopedComQIPtr<IWebBrowserApp> app(disp);
        if (!app) {
            continue;
        }
        SHANDLE_PTR hwndVal = 0;
        app->get_HWND(&hwndVal);
        HWND hwnd = (HWND)hwndVal;
        bool match = hwnd == top;
        if (!match && wantDesktop && ClassEq(hwnd, L"Progman")) {
            match = true;
        }
        if (!match) {
            continue;
        }
        TempStr path = SelectedPathFromBrowserTemp(app);
        if (path) {
            return path;
        }
    }
    return {};
}

static void OnQuickLookSpace() {
    if (!ExplorerForegroundAllowsPreview()) {
        return;
    }
    TempStr path = GetExplorerSelectedPathTemp();
    if (!PathIsSupportedPreview(path)) {
        return;
    }
    RequestQuickLook(path);
}

static bool ShouldStealSpace() {
    if (!ExplorerForegroundAllowsPreview()) {
        return false;
    }
    TempStr path = GetExplorerSelectedPathTemp();
    return PathIsSupportedPreview(path);
}

static LRESULT CALLBACK QuickLookKeyboardProc(int nCode, WPARAM wp, LPARAM lp) {
    if (nCode == HC_ACTION && (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN)) {
        auto* ks = (KBDLLHOOKSTRUCT*)lp;
        if (ks->vkCode == VK_SPACE && !(ks->flags & LLKHF_INJECTED)) {
            if (!(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000) &&
                !(GetKeyState(VK_SHIFT) & 0x8000)) {
                HWND fg = GetForegroundWindow();
                MainWindow* ours = FindMainWindowByHwnd(fg);
                if (ours && ours->isQuickLook) {
                    // Space is handled by the preview window itself
                } else if (gQuickLookAgentHwnd && ShouldStealSpace()) {
                    PostMessageW(gQuickLookAgentHwnd, kMsgQuickLookSpace, 0, 0);
                    return 1;
                }
            }
        }
    }
    return CallNextHookEx(gQuickLookHook, nCode, wp, lp);
}

static LRESULT CALLBACK QuickLookAgentWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == kMsgQuickLookSpace) {
        OnQuickLookSpace();
        return 0;
    }
    if (msg == WM_QUERYENDSESSION) {
        // we never block logging off / shutting down
        return TRUE;
    }
    if (msg == WM_ENDSESSION) {
        if (wp) {
            DestroyWindow(hwnd);
        }
        return 0;
    }
    if (msg == WM_DESTROY) {
        if (gQuickLookHook) {
            UnhookWindowsHookEx(gQuickLookHook);
            gQuickLookHook = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static TempStr QuickLookRunCommandTemp() {
    TempStr exe = GetSelfExePathTemp();
    return fmt("%s -quicklook-agent", QuoteCmdLineArgTemp(exe));
}

void ExplorerQuickLookRemoveRunKey() {
    LoggedDeleteRegValue(HKEY_CURRENT_USER, kQuickLookRunKey, kQuickLookRunValue);
}

static void WriteQuickLookRunKey() {
    LoggedWriteRegStr(HKEY_CURRENT_USER, kQuickLookRunKey, kQuickLookRunValue, QuickLookRunCommandTemp());
}

static HWND FindQuickLookAgentHwnd() {
    HWND hwnd = FindWindowW(kQuickLookAgentClass, nullptr);
    if (hwnd) {
        return hwnd;
    }
    // the agent used to be a message-only window, which FindWindowW never sees
    // (it only enumerates top-level windows). Look for those too so a stray
    // agent left over from an older build can still be found and stopped
    return FindWindowExW(HWND_MESSAGE, nullptr, kQuickLookAgentClass, nullptr);
}

static void StopAgentsUnder(HWND parent) {
    HWND hwnd = nullptr;
    for (;;) {
        hwnd = FindWindowExW(parent, hwnd, kQuickLookAgentClass, nullptr);
        if (!hwnd) {
            return;
        }
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
}

// stop all of them: before we used a top-level window, EnsureQuickLookAgentProcess()
// couldn't tell an agent was already running and started a new one on every run
static void StopQuickLookAgentProcess() {
    StopAgentsUnder(nullptr);
    StopAgentsUnder(HWND_MESSAGE);
}

static void EnsureQuickLookAgentProcess() {
    if (FindQuickLookAgentHwnd()) {
        return;
    }
    TempStr cmd = QuickLookRunCommandTemp();
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, CWStrTemp(cmd), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        logf("EnsureQuickLookAgentProcess: CreateProcess failed\n");
    }
}

void ExplorerQuickLookApplyFromSettings() {
    if (gForTesting || gPluginMode) {
        return;
    }
    if (gCli && (gCli->install || gCli->uninstall || gCli->quickLookAgent || gCli->forTesting)) {
        return;
    }
    bool on = gSettings && gSettings->explorerQuickLook;
    if (on) {
        WriteQuickLookRunKey();
        EnsureQuickLookAgentProcess();
    } else {
        ExplorerQuickLookRemoveRunKey();
        StopQuickLookAgentProcess();
    }
}

bool RunExplorerQuickLookAgentLoop() {
    // belt and braces on top of the FindQuickLookAgentHwnd() check in
    // EnsureQuickLookAgentProcess(): that one races when 2 instances start at
    // the same time, and a duplicate agent means a duplicate low-level
    // keyboard hook (and a process nobody can find or stop)
    gQuickLookAgentMutex = CreateMutexW(nullptr, TRUE, kQuickLookAgentMutexName);
    if (gQuickLookAgentMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        SafeCloseHandle(&gQuickLookAgentMutex);
        return true;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = QuickLookAgentWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kQuickLookAgentClass;
    if (!RegisterClassExW(&wc)) {
        HWND already = FindQuickLookAgentHwnd();
        if (already) {
            SafeCloseHandle(&gQuickLookAgentMutex);
            return true;
        }
        logf("RunExplorerQuickLookAgentLoop: RegisterClassEx failed\n");
        SafeCloseHandle(&gQuickLookAgentMutex);
        return false;
    }
    // a hidden top-level window, not a message-only window: message-only
    // windows are invisible to FindWindowW() and don't get WM_QUERYENDSESSION /
    // WM_ENDSESSION, so the agent could neither be found nor asked to quit
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    gQuickLookAgentHwnd = CreateWindowExW(exStyle, kQuickLookAgentClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                          wc.hInstance, nullptr);
    if (!gQuickLookAgentHwnd) {
        logf("RunExplorerQuickLookAgentLoop: CreateWindowEx failed\n");
        SafeCloseHandle(&gQuickLookAgentMutex);
        return false;
    }
    gQuickLookHook = SetWindowsHookExW(WH_KEYBOARD_LL, QuickLookKeyboardProc, wc.hInstance, 0);
    if (!gQuickLookHook) {
        logf("RunExplorerQuickLookAgentLoop: SetWindowsHookEx failed\n");
        DestroyWindow(gQuickLookAgentHwnd);
        gQuickLookAgentHwnd = nullptr;
        SafeCloseHandle(&gQuickLookAgentMutex);
        return false;
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    gQuickLookAgentHwnd = nullptr;
    SafeCloseHandle(&gQuickLookAgentMutex);
    return true;
}
