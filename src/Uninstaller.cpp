/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
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
#include "Installer.h"
#include "utils/CmdLineParser.h"
#include "CrashHandler.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/EditCtrl.h"

#include "utils/DebugLog.h"

#define UNINSTALLER_WIN_DX INSTALLER_WIN_DX
#define UNINSTALLER_WIN_DY INSTALLER_WIN_DY

static HBRUSH ghbrBackground = nullptr;

static bool RemoveUninstallerRegistryInfo(HKEY hkey) {
    bool ok1 = DeleteRegKey(hkey, REG_PATH_UNINST);
    // legacy, this key was added by installers up to version 1.8
    bool ok2 = DeleteRegKey(hkey, L"Software\\" APP_NAME_STR);
    return ok1 && ok2;
}

static bool RemoveUninstallerRegistryInfo() {
    bool ok1 = RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    bool ok2 = RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);
    return ok1 || ok2;
}

/* Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did */
static void UnregisterFromBeingDefaultViewer(HKEY hkey) {
    AutoFreeW curr(ReadRegStr(hkey, REG_CLASSES_PDF, nullptr));
    AutoFreeW prev(ReadRegStr(hkey, REG_CLASSES_APP, L"previous.pdf"));
    if (!curr || !str::Eq(curr, APP_NAME_STR)) {
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
    AutoFreeW buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID));
    if (str::Eq(buf, APP_NAME_STR)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID);
        if (res != ERROR_SUCCESS)
            LogLastError(res);
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION));
    if (str::EqI(buf, EXENAME)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION);
        if (res != ERROR_SUCCESS)
            LogLastError(res);
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", PROG_ID));
    if (str::Eq(buf, APP_NAME_STR))
        DeleteRegKey(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", true);
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
    DeleteRegKey(hkey, REG_CLASSES_APP);
    DeleteRegKey(hkey, REG_CLASSES_APPS);
    SHDeleteValue(hkey, REG_CLASSES_PDF L"\\OpenWithProgids", APP_NAME_STR);

    for (int j = 0; nullptr != gSupportedExts[j]; j++) {
        AutoFreeW keyname(str::Join(L"Software\\Classes\\", gSupportedExts[j], L"\\OpenWithProgids"));
        SHDeleteValue(hkey, keyname, APP_NAME_STR);
        DeleteEmptyRegKey(hkey, keyname);

        keyname.Set(str::Join(L"Software\\Classes\\", gSupportedExts[j], L"\\OpenWithList\\" EXENAME));
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

    AutoFreeW dirPattern(path::Join(dir, L"*"));
    HANDLE h = FindFirstFileW(dirPattern, &findData);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            AutoFreeW path(path::Join(dir, findData.cFileName));
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
        AutoFreeW relPath(str::conv::FromUtf8(s));
        AutoFreeW path(path::Join(dir, relPath));
        DeleteFile(path);
    }

    RemoveEmptyDirectory(dir);
}

static bool RemoveShortcut(bool allUsers) {
    WCHAR* p = GetShortcutPath(allUsers);
    if (!p) {
        return false;
    }
    bool ok = DeleteFile(p);
    free(p);
    if (!ok && (ERROR_FILE_NOT_FOUND != GetLastError())) {
        LogLastError();
        return false;
    }
    return true;
}

static bool RemoveShortcut() {
    bool ok1 = RemoveShortcut(true);
    bool ok2 = RemoveShortcut(false);
    return ok1 || ok2;
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

    if (!RemoveShortcut()) {
        NotifyFailed(_TR("Couldn't remove the shortcut"));
    }

    UninstallBrowserPlugin();
    UninstallPdfFilter();
    UninstallPdfPreviewer();
    RemoveOwnRegistryKeys();

    RemoveInstalledFiles();
    // NotifyFailed(_TR("Couldn't remove installation directory"));

    // always succeed, even for partial uninstallations
    gInstUninstGlobals.success = true;

    if (!gInstUninstGlobals.silent) {
        PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    }
    return 0;
}

static void OnButtonUninstall() {
    if (!CheckInstallUninstallPossible()) {
        return;
    }

    // disable the button during uninstallation
    gButtonInstUninst->SetIsEnabled(false);
    SetMsg(_TR("Uninstallation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    gInstUninstGlobals.hThread = CreateThread(nullptr, 0, UninstallerThread, nullptr, 0, 0);
}

void OnUninstallationFinished() {
    delete gButtonInstUninst;
    gButtonInstUninst = nullptr;
    CreateButtonExit(gHwndFrame);
    SetMsg(_TR("SumatraPDF has been uninstalled."), gMsgError ? COLOR_MSG_FAILED : COLOR_MSG_OK);
    gMsgError = gInstUninstGlobals.firstError;
    InvalidateFrame();

    CloseHandle(gInstUninstGlobals.hThread);
}

static bool OnWmCommand(WPARAM wParam) {
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
    gButtonInstUninst->OnClicked = OnButtonUninstall;
}

static void CreateMainWindow() {
    AutoFreeW title(str::Format(_TR("SumatraPDF %s Uninstaller"), CURR_VERSION_STR));
    int dx = dpiAdjust(INSTALLER_WIN_DX);
    int dy = dpiAdjust(INSTALLER_WIN_DY);
    HMODULE h = GetModuleHandleW(nullptr);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    gHwndFrame = CreateWindowW(INSTALLER_FRAME_CLASS_NAME, title.Get(), dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, dx, dy,
                               nullptr, nullptr, h, nullptr);
}

static void ShowUsage() {
    // Note: translation services aren't initialized at this point, so English only
    MessageBox(nullptr, L"uninstall.exe [/s][/d <path>]\n\
    \n\
    /s\tuninstalls " APP_NAME_STR L" silently (without user interaction).\n\
    /d\tchanges the directory from where " APP_NAME_STR L" will be uninstalled.",
    APP_NAME_STR L" Uninstaller Usage", MB_OK | MB_ICONINFORMATION);
}

using namespace Gdiplus;

static WCHAR* GetInstallationDir() {
    AutoFreeW dir(ReadRegStr2(HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, REG_PATH_UNINST, L"InstallLocation"));
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
            handled = OnWmCommand(wParam);
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
    auto resName = MAKEINTRESOURCEW(IDI_SUMATRAPDF);
    wcex.hIcon = LoadIcon(h, resName);

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
    Timer t;
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
        if (t.GetTimeInMs() > 10000 && gButtonInstUninst && gButtonInstUninst->IsEnabled()) {
            CheckInstallUninstallPossible(true);
            t.Start();
        }
    }
}

int RunUninstaller(bool silent) {
    int ret = 1;

    gInstUninstGlobals.silent = silent;
    gDefaultMsg = _TR("Are you sure you want to uninstall SumatraPDF?");
    gInstUninstGlobals.installDir = GetInstallationDir();

    AutoFreeW exePath(GetInstalledExePath());
    auto installerExists = file::Exists(exePath);

    if (gInstUninstGlobals.showUsageAndQuit) {
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

    if (gInstUninstGlobals.silent) {
        UninstallerThread(nullptr);
        ret = gInstUninstGlobals.success ? 0 : 1;
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
    free(gInstUninstGlobals.installDir);
    free(gInstUninstGlobals.firstError);

    return ret;
}
