/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/*
The installer is good enough for production but it doesn't mean it couldn't be improved:
 * some more fanciful animations e.g.:
 * letters could drop down and back up when cursor is over it
 * messages could scroll-in
 * some background thing could be going on, e.g. a spinning 3d cube
 * show fireworks on successful installation/uninstallation
*/

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include <tlhelp32.h>
#include <io.h>
#include "utils/FileUtil.h"
#include "Translations.h"
#include "Resource.h"
#include "utils/Timer.h"
#include "Version.h"
#include "utils/WinUtil.h"
#include "utils/CmdLineParser.h"
#include "CrashHandler.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/Log.h"
#include "utils/RegistryPaths.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/EditCtrl.h"

#include "SumatraConfig.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "Installer.h"

#define UNINSTALLER_WIN_DX INSTALLER_WIN_DX
#define UNINSTALLER_WIN_DY INSTALLER_WIN_DY

static HBRUSH ghbrBackground = nullptr;
static HANDLE hThread = nullptr;
static bool success = false;

static bool RemoveUninstallerRegistryInfo(HKEY hkey) {
    AutoFreeWstr REG_PATH_UNINST = getRegPathUninst(getAppName());
    bool ok1 = DeleteRegKey(hkey, REG_PATH_UNINST);
    // legacy, this key was added by installers up to version 1.8
    const WCHAR* appName = getAppName();
    AutoFreeWstr key = str::Join(L"Software\\", appName);
    bool ok2 = DeleteRegKey(hkey, key);
    return ok1 && ok2;
}

static bool RemoveUninstallerRegistryInfo() {
    bool ok1 = RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    bool ok2 = RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);
    return ok1 || ok2;
}

/* Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did */
static void UnregisterFromBeingDefaultViewer(HKEY hkey) {
    AutoFreeWstr curr = ReadRegStr(hkey, REG_CLASSES_PDF, nullptr);
    AutoFreeWstr REG_CLASSES_APP = getRegClassesApp(getAppName());
    AutoFreeWstr prev = ReadRegStr(hkey, REG_CLASSES_APP, L"previous.pdf");
    const WCHAR* appName = getAppName();
    if (!curr || !str::Eq(curr, appName)) {
        // not the default, do nothing
    } else if (prev) {
        WriteRegStr(hkey, REG_CLASSES_PDF, nullptr, prev);
    } else {
#pragma warning(push)
#pragma warning(disable : 6387) // silence /analyze: '_Param_(3)' could be '0':  this does not adhere to the
                                // specification for the function 'SHDeleteValueW'
        SHDeleteValue(hkey, REG_CLASSES_PDF, nullptr);
#pragma warning(pop)
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    AutoFreeWstr buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID));
    if (str::Eq(buf, appName)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID);
        if (res != ERROR_SUCCESS) {
            LogLastError(res);
        }
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION));
    const WCHAR* exeName = getExeName();
    if (str::EqI(buf, exeName)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION);
        if (res != ERROR_SUCCESS) {
            LogLastError(res);
        }
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", PROG_ID));
    if (str::Eq(buf, appName)) {
        DeleteRegKey(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", true);
    }
}

static bool DeleteEmptyRegKey(HKEY root, const WCHAR* keyName) {
    HKEY hkey;
    if (RegOpenKeyEx(root, keyName, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return true;

    DWORD subkeys, values;
    bool isEmpty = false;
    if (RegQueryInfoKey(hkey, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, &values, nullptr, nullptr, nullptr,
                        nullptr) == ERROR_SUCCESS) {
        isEmpty = 0 == subkeys && 0 == values;
    }
    RegCloseKey(hkey);

    if (isEmpty)
        DeleteRegKey(root, keyName);
    return isEmpty;
}

static void RemoveOwnRegistryKeys(HKEY hkey) {
    UnregisterFromBeingDefaultViewer(hkey);
    const WCHAR* appName = getAppName();
    const WCHAR* exeName = getExeName();
    AutoFreeWstr REG_CLASSES_APP = getRegClassesApp(appName);
    DeleteRegKey(hkey, REG_CLASSES_APP);
    AutoFreeWstr REG_CLASSES_APPS = getRegClassesApps(appName);
    DeleteRegKey(hkey, REG_CLASSES_APPS);
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_PDF, L"\\OpenWithProgids");
        SHDeleteValueW(hkey, key, appName);
    }

    AutoFreeWstr openWithVal = str::Join(L"\\OpenWithList\\", exeName);
    for (int j = 0; nullptr != gSupportedExts[j]; j++) {
        AutoFreeWstr keyname(str::Join(L"Software\\Classes\\", gSupportedExts[j], L"\\OpenWithProgids"));
        SHDeleteValueW(hkey, keyname, appName);
        DeleteEmptyRegKey(hkey, keyname);

        keyname.Set(str::Join(L"Software\\Classes\\", gSupportedExts[j], openWithVal));
        if (!DeleteRegKey(hkey, keyname)) {
            continue;
        }
        // remove empty keys that the installer might have created
        *(WCHAR*)str::FindCharLast(keyname, '\\') = '\0';
        if (!DeleteEmptyRegKey(hkey, keyname)) {
            continue;
        }
        *(WCHAR*)str::FindCharLast(keyname, '\\') = '\0';
        DeleteEmptyRegKey(hkey, keyname);
    }

    // delete keys written in ListAsDefaultProgramWin10()
    SHDeleteValue(hkey, L"SOFTWARE\\RegisteredApplications", L"SumatraPDF");
    DeleteRegKey(hkey, L"SOFTWARE\\SumatraPDF\\Capabilities");
}

static void RemoveOwnRegistryKeys() {
    RemoveOwnRegistryKeys(HKEY_LOCAL_MACHINE);
    RemoveOwnRegistryKeys(HKEY_CURRENT_USER);
}

static bool RemoveEmptyDirectory(const WCHAR* dir) {
    WIN32_FIND_DATA findData;
    bool ok = TRUE;

    AutoFreeWstr dirPattern(path::Join(dir, L"*"));
    HANDLE h = FindFirstFileW(dirPattern, &findData);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            AutoFreeWstr path(path::Join(dir, findData.cFileName));
            DWORD attrs = findData.dwFileAttributes;
            // filter out directories. Even though there shouldn't be any
            // subdirectories, it also filters out the standard "." and ".."
            if ((attrs & FILE_ATTRIBUTE_DIRECTORY) && !str::Eq(findData.cFileName, L".") &&
                !str::Eq(findData.cFileName, L"..")) {
                ok &= RemoveEmptyDirectory(path);
            }
        } while (FindNextFileW(h, &findData) != 0);
        FindClose(h);
    }

    if (!::RemoveDirectoryW(dir)) {
        DWORD lastError = GetLastError();
        if (ERROR_DIR_NOT_EMPTY != lastError && ERROR_FILE_NOT_FOUND != lastError) {
            LogLastError(lastError);
            ok = false;
        }
    }

    return ok;
}

// The following list is used to verify that all the required files have been
// installed (install flag set) and to know what files are to be removed at
// uninstallation (all listed files that actually exist).
// When a file is no longer shipped, just disable the install flag so that the
// file is still correctly removed when SumatraPDF is eventually uninstalled.
const char* gInstalledFiles[] = {
    "libmupdf.dll",
    "PdfFilter.dll",
    "PdfPreview.dll",
    // files no longer shipped, to be deleted
    "SumatraPDF.exe",
    "sumatrapdfprefs.dat",
    "DroidSansFallback.ttf",
    "npPdfViewer.dll",
    "uninstall.exe",
    "UnRar.dll",
    "UnRar64.dll",
};

// TODO: maybe just delete the directory
static void RemoveInstalledFiles() {
    const WCHAR* dir = GetInstallDirNoFree();
    size_t n = dimof(gInstalledFiles);
    for (size_t i = 0; i < n; i++) {
        const char* s = gInstalledFiles[i];
        AutoFreeWstr relPath = strconv::Utf8ToWstr(s);
        AutoFreeWstr path = path::Join(dir, relPath);
        DeleteFile(path);
    }

    RemoveEmptyDirectory(dir);
}

static int shortcutDirs[] = {CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS, CSIDL_DESKTOP};

static void RemoveShortcuts() {
    for (size_t i = 0; i < dimof(shortcutDirs); i++) {
        int csidl = shortcutDirs[i];
        WCHAR* path = GetShortcutPath(csidl);
        if (!path) {
            continue;
        }
        DeleteFile(path);
        free(path);
    }
}

static DWORD WINAPI UninstallerThread(LPVOID data) {
    UNUSED(data);
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    WCHAR* exePath = GetUninstallerPath();
    const WCHAR* ownPath = GetOwnPath();
    if (!path::IsSame(exePath, ownPath)) {
        KillProcess(exePath, TRUE);
    }
    free(exePath);

    if (!RemoveUninstallerRegistryInfo()) {
        NotifyFailed(_TR("Failed to delete uninstaller registry keys"));
    }

    RemoveShortcuts();

    UninstallBrowserPlugin();
    UninstallPdfFilter();
    UninstallPdfPreviewer();
    RemoveOwnRegistryKeys();

    RemoveInstalledFiles();
    // NotifyFailed(_TR("Couldn't remove installation directory"));

    // always succeed, even for partial uninstallations
    success = true;

    if (!gCli->silent) {
        PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    }
    return 0;
}

static void InvalidateFrame() {
    ClientRect rc(gHwndFrame);
    RECT rcTmp = rc.ToRECT();
    InvalidateRect(gHwndFrame, &rcTmp, FALSE);
}

static void OnButtonUninstall() {
    if (!CheckInstallUninstallPossible()) {
        return;
    }

    // disable the button during uninstallation
    gButtonInstUninst->SetIsEnabled(false);
    SetMsg(_TR("Uninstallation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    hThread = CreateThread(nullptr, 0, UninstallerThread, nullptr, 0, 0);
}

void OnUninstallationFinished() {
    delete gButtonInstUninst;
    gButtonInstUninst = nullptr;
    CreateButtonExit(gHwndFrame);
    SetMsg(_TR("SumatraPDF has been uninstalled."), gMsgError ? COLOR_MSG_FAILED : COLOR_MSG_OK);
    gMsgError = gInstUninstGlobals.firstError;
    InvalidateFrame();

    CloseHandle(hThread);
}

static bool UninstallerOnWmCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case IDCANCEL:
            OnButtonExit();
            break;

        default:
            return false;
    }
    return true;
}

static void OnCreateWindow(HWND hwnd) {
    gButtonInstUninst = CreateDefaultButtonCtrl(hwnd, _TR("Uninstall SumatraPDF"));
    gButtonInstUninst->onClicked = OnButtonUninstall;
}

static void CreateMainWindow() {
    AutoFreeWstr title(str::Format(_TR("SumatraPDF %s Uninstaller"), CURR_VERSION_STR));
    WCHAR* winCls = INSTALLER_FRAME_CLASS_NAME;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = DpiScale(INSTALLER_WIN_DX);
    int dy = DpiScale(INSTALLER_WIN_DY);
    HMODULE h = GetModuleHandleW(nullptr);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    gHwndFrame = CreateWindowW(winCls, title.Get(), dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
}

static void ShowUsage() {
    // Note: translation services aren't initialized at this point, so English only
    const WCHAR* appName = getAppName();
    AutoFreeWstr caption = str::Join(appName, L" Uninstaller Usage");
    AutoFreeWstr msg = str::Format(
        L"uninstall.exe [/s][/d <path>]\n\
    \n\
    /s\tuninstalls %s silently (without user interaction).\n\
    /d\tchanges the directory from where %s will be uninstalled.",
        appName, appName);
    MessageBoxW(nullptr, msg, caption, MB_OK | MB_ICONINFORMATION);
}

using namespace Gdiplus;

static WCHAR* GetInstallationDir() {
    AutoFreeWstr REG_PATH_UNINST = getRegPathUninst(getAppName());
    AutoFreeWstr dir = ReadRegStr2(REG_PATH_UNINST, L"InstallLocation");
    if (dir) {
        if (str::EndsWithI(dir, L".exe")) {
            dir.Set(path::GetDir(dir));
        }
        if (!str::IsEmpty(dir.Get()) && dir::Exists(dir))
            return dir.StealData();
    }

    // fall back to the uninstaller's path
    return path::GetDir(GetOwnPath());
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    bool handled;
    switch (message) {
        case WM_CREATE:
            OnCreateWindow(hwnd);
            break;

        case WM_CTLCOLORSTATIC: {
            if (ghbrBackground == nullptr) {
                ghbrBackground = CreateSolidBrush(RGB(0xff, 0xf2, 0));
            }
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)ghbrBackground;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT:
            OnPaintFrame(hwnd);
            break;

        case WM_COMMAND:
            handled = UninstallerOnWmCommand(wParam);
            if (!handled)
                return DefWindowProc(hwnd, message, wParam, lParam);
            break;

        case WM_APP_INSTALLATION_FINISHED:
            OnUninstallationFinished();
            if (gButtonExit) {
                gButtonExit->SetFocus();
            }
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static bool RegisterWinClass() {
    WNDCLASSEX wcex{};

    FillWndClassEx(wcex, INSTALLER_FRAME_CLASS_NAME, WndProcFrame);
    auto h = GetModuleHandle(nullptr);
    auto iconName = MAKEINTRESOURCEW(getAppIconID());
    wcex.hIcon = LoadIcon(h, iconName);

    ATOM atom = RegisterClassEx(&wcex);
    CrashIf(!atom);
    return atom != 0;
}

static BOOL InstanceInit() {
    InitInstallerUninstaller();

    CreateMainWindow();
    if (!gHwndFrame)
        return FALSE;

    SetDefaultMsg();

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
static int RunApp() {
    MSG msg;
    FrameTimeoutCalculator ftc(60);
    auto t = TimeGet();
    for (;;) {
        const DWORD timeout = ftc.GetTimeoutInMilliseconds();
        DWORD res = WAIT_TIMEOUT;
        if (timeout > 0) {
            res = MsgWaitForMultipleObjects(0, 0, TRUE, timeout, QS_ALLINPUT);
        }
        if (res == WAIT_TIMEOUT) {
            AnimStep();
            ftc.Step();
        }

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return (int)msg.wParam;
            }
            if (!IsDialogMessage(gHwndFrame, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        // check if there are processes that need to be closed but
        // not more frequently than once per ten seconds and
        // only before (un)installation starts.
        auto dur = TimeSinceInMs(t);
        if (dur > 10000 && gButtonInstUninst && gButtonInstUninst->IsEnabled()) {
            CheckInstallUninstallPossible(true);
            t = TimeGet();
        }
    }
}

int RunUninstaller(CommandLineInfo* cli) {
    int ret = 1;

    gCli = cli;

    if (!gIsDebugBuild) {
        if (!IsRunningElevated()) {
            WCHAR* exePath = GetExePath();
            WCHAR* cmdline = GetCommandLineW(); // not owning the memory
            LaunchElevated(exePath, cmdline);
            str::Free(exePath);
            ::ExitProcess(0);
        }
    }

    gDefaultMsg = _TR("Are you sure you want to uninstall SumatraPDF?");
    gCli->installDir = GetInstallationDir();

    AutoFreeWstr exePath(GetInstalledExePath());
    auto installerExists = file::Exists(exePath);

    if (gCli->showHelp) {
        ShowUsage();
        ret = 0;
        goto Exit;
    }

    // installerExists = true;
    if (!installerExists) {
        const WCHAR* caption = _TR("Uninstallation failed");
        const WCHAR* msg = _TR("SumatraPDF installation not found.");
        MessageBox(nullptr, msg, caption, MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    if (gCli->silent) {
        UninstallerThread(nullptr);
        ret = success ? 0 : 1;
        goto Exit;
    }

    if (!RegisterWinClass()) {
        goto Exit;
    }

    if (!InstanceInit()) {
        goto Exit;
    }

    BringWindowToTop(gHwndFrame);
    ret = RunApp();

Exit:
    free(gInstUninstGlobals.firstError);

    return ret;
}
