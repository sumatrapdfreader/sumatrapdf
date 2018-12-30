/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/*
The installer is good enough for production but it doesn't mean it couldn't be improved:
 * some more fanciful animations e.g.:
 * letters could drop down and back up when cursor is over it
 * messages could scroll-in
 * some background thing could be going on, e.g. a spinning 3d cube
 * show fireworks on successful installation/uninstallation
*/

// define to allow testing crash handling via -crash cmd-line option
#define ENABLE_CRASH_TESTING

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
#include "utils/DebugLog.h"

#define UNINSTALLER_WIN_DX INSTALLER_WIN_DX
#define UNINSTALLER_WIN_DY INSTALLER_WIN_DY

// Try harder getting temporary directory
// Caller needs to free() the result.
// Returns nullptr if fails for any reason.
static WCHAR* GetValidTempDir() {
    AutoFreeW d(path::GetTempPath());
    if (!d) {
        NotifyFailed(_TR("Couldn't obtain temporary directory"));
        return nullptr;
    }
    bool ok = dir::Create(d);
    if (!ok) {
        LogLastError();
        NotifyFailed(_TR("Couldn't create temporary directory"));
        return nullptr;
    }
    return d.StealData();
}

static WCHAR* GetTempUninstallerPath() {
    AutoFreeW tempDir(GetValidTempDir());
    if (!tempDir)
        return nullptr;
    // Using fixed (unlikely) name instead of GetTempFileName()
    // so that we don't litter temp dir with copies of ourselves
    return path::Join(tempDir, L"sum~inst.exe");
}

static BOOL IsUninstallerNeeded() {
    AutoFreeW exePath(GetInstalledExePath());
    return file::Exists(exePath);
}

static bool RemoveUninstallerRegistryInfo(HKEY hkey) {
    bool ok1 = DeleteRegKey(hkey, REG_PATH_UNINST);
    // legacy, this key was added by installers up to version 1.8
    bool ok2 = DeleteRegKey(hkey, L"Software\\" APP_NAME_STR);
    return ok1 && ok2;
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

static void RemoveOwnRegistryKeys() {
    HKEY keys[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
    // remove all keys from both HKLM and HKCU (wherever they exist)
    for (int i = 0; i < dimof(keys); i++) {
        UnregisterFromBeingDefaultViewer(keys[i]);
        DeleteRegKey(keys[i], REG_CLASSES_APP);
        DeleteRegKey(keys[i], REG_CLASSES_APPS);
        SHDeleteValue(keys[i], REG_CLASSES_PDF L"\\OpenWithProgids", APP_NAME_STR);

        for (int j = 0; nullptr != gSupportedExts[j]; j++) {
            AutoFreeW keyname(str::Join(L"Software\\Classes\\", gSupportedExts[j], L"\\OpenWithProgids"));
            SHDeleteValue(keys[i], keyname, APP_NAME_STR);
            DeleteEmptyRegKey(keys[i], keyname);

            keyname.Set(str::Join(L"Software\\Classes\\", gSupportedExts[j], L"\\OpenWithList\\" EXENAME));
            if (!DeleteRegKey(keys[i], keyname))
                continue;
            // remove empty keys that the installer might have created
            *(WCHAR*)str::FindCharLast(keyname, '\\') = '\0';
            if (!DeleteEmptyRegKey(keys[i], keyname))
                continue;
            *(WCHAR*)str::FindCharLast(keyname, '\\') = '\0';
            DeleteEmptyRegKey(keys[i], keyname);
        }
    }

    // delete keys written in ListAsDefaultProgramWin10()
    HKEY hkey = HKEY_LOCAL_MACHINE;
    SHDeleteValue(hkey, L"SOFTWARE\\RegisteredApplications", L"SumatraPDF");
    DeleteRegKey(hkey, L"SOFTWARE\\SumatraPDF\\Capabilities");
}

static BOOL RemoveEmptyDirectory(const WCHAR* dir) {
    WIN32_FIND_DATA findData;
    BOOL success = TRUE;

    AutoFreeW dirPattern(path::Join(dir, L"*"));
    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            AutoFreeW path(path::Join(dir, findData.cFileName));
            DWORD attrs = findData.dwFileAttributes;
            // filter out directories. Even though there shouldn't be any
            // subdirectories, it also filters out the standard "." and ".."
            if ((attrs & FILE_ATTRIBUTE_DIRECTORY) && !str::Eq(findData.cFileName, L".") &&
                !str::Eq(findData.cFileName, L"..")) {
                success &= RemoveEmptyDirectory(path);
            }
        } while (FindNextFile(h, &findData) != 0);
        FindClose(h);
    }

    if (!RemoveDirectory(dir)) {
        DWORD lastError = GetLastError();
        if (ERROR_DIR_NOT_EMPTY != lastError && ERROR_FILE_NOT_FOUND != lastError) {
            LogLastError(lastError);
            success = FALSE;
        }
    }

    return success;
}

static BOOL RemoveInstalledFiles() {
    BOOL success = TRUE;

    for (int i = 0; nullptr != gPayloadData[i].fileName; i++) {
        AutoFreeW relPath(str::conv::FromUtf8(gPayloadData[i].fileName));
        AutoFreeW path(path::Join(gInstUninstGlobals.installDir, relPath));

        if (file::Exists(path))
            success &= DeleteFile(path);
    }

    RemoveEmptyDirectory(gInstUninstGlobals.installDir);
    return success;
}

static const WCHAR* GetOwnPath() {
    static WCHAR exePath[MAX_PATH];
    exePath[0] = '\0';
    GetModuleFileName(nullptr, exePath, dimof(exePath));
    exePath[dimof(exePath) - 1] = '\0';
    return exePath;
}

// If this is uninstaller and we're running from installation directory,
// copy uninstaller to temp directory and execute from there, exiting
// ourselves. This is needed so that uninstaller can delete itself
// from installation directory and remove installation directory
// If returns TRUE, this is an installer and we sublaunched ourselves,
// so the caller needs to exit
static bool ExecuteUninstallerFromTempDir() {
    // only need to sublaunch if running from installation dir
    AutoFreeW ownDir(path::GetDir(GetOwnPath()));
    AutoFreeW tempPath(GetTempUninstallerPath());

    // no temp directory available?
    if (!tempPath)
        return false;

    // not running from the installation directory?
    // (likely a test uninstaller that shouldn't be removed anyway)
    if (!path::IsSame(ownDir, gInstUninstGlobals.installDir))
        return false;

    // already running from temp directory?
    if (path::IsSame(GetOwnPath(), tempPath))
        return false;

    if (!CopyFile(GetOwnPath(), tempPath, FALSE)) {
        NotifyFailed(_TR("Failed to copy uninstaller to temp directory"));
        return false;
    }

    AutoFreeW args(
        str::Format(L"/d \"%s\" %s", gInstUninstGlobals.installDir, gInstUninstGlobals.silent ? L"/s" : L""));
    bool ok = CreateProcessHelper(tempPath, args);

    // mark the uninstaller for removal at shutdown (note: works only for administrators)
    MoveFileEx(tempPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    return ok;
}

static bool RemoveShortcut(bool allUsers) {
    AutoFreeW p(GetShortcutPath(allUsers));
    if (!p.Get())
        return false;
    bool ok = DeleteFile(p);
    if (!ok && (ERROR_FILE_NOT_FOUND != GetLastError())) {
        LogLastError();
        return false;
    }
    return true;
}

static DWORD WINAPI UninstallerThread(LPVOID data) {
    UNUSED(data);
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    WCHAR* exePath = GetUninstallerPath();
    if (!path::IsSame(exePath, GetOwnPath()))
        KillProcess(exePath, TRUE);
    free(exePath);

    if (!RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE) && !RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_TR("Failed to delete uninstaller registry keys"));
    }

    if (!RemoveShortcut(true) && !RemoveShortcut(false))
        NotifyFailed(_TR("Couldn't remove the shortcut"));

    UninstallBrowserPlugin();
    UninstallPdfFilter();
    UninstallPdfPreviewer();
    RemoveOwnRegistryKeys();

    if (!RemoveInstalledFiles())
        NotifyFailed(_TR("Couldn't remove installation directory"));

    // always succeed, even for partial uninstallations
    gInstUninstGlobals.success = true;

    if (!gInstUninstGlobals.silent)
        PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    return 0;
}

static void OnButtonUninstall() {
    KillSumatra();

    if (!CheckInstallUninstallPossible())
        return;

    // disable the button during uninstallation
    EnableWindow(gHwndButtonInstUninst, FALSE);
    SetMsg(_TR("Uninstallation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    gInstUninstGlobals.hThread = CreateThread(nullptr, 0, UninstallerThread, nullptr, 0, 0);
}

void OnUninstallationFinished() {
    DestroyWindow(gHwndButtonInstUninst);
    gHwndButtonInstUninst = nullptr;
    CreateButtonExit(gHwndFrame);
    SetMsg(_TR("SumatraPDF has been uninstalled."), gMsgError ? COLOR_MSG_FAILED : COLOR_MSG_OK);
    gMsgError = gInstUninstGlobals.firstError;
    InvalidateFrame();

    CloseHandle(gInstUninstGlobals.hThread);
}

static bool OnWmCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case IDOK:
            if (gHwndButtonInstUninst)
                OnButtonUninstall();
            else if (gHwndButtonExit)
                OnButtonExit();
            break;

        case ID_BUTTON_EXIT:
        case IDCANCEL:
            OnButtonExit();
            break;

        default:
            return false;
    }
    return true;
}

static void OnCreateWindow(HWND hwnd) {
    gHwndButtonInstUninst = CreateDefaultButton(hwnd, _TR("Uninstall SumatraPDF"), IDOK);
}

static void CreateMainWindow() {
    AutoFreeW title(str::Format(_TR("SumatraPDF %s Uninstaller"), CURR_VERSION_STR));
    gHwndFrame = CreateWindow(INSTALLER_FRAME_CLASS_NAME, title.Get(), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              CW_USEDEFAULT, CW_USEDEFAULT, dpiAdjust(INSTALLER_WIN_DX), dpiAdjust(INSTALLER_WIN_DY),
                              nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
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
    AutoFreeW dir(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_UNINST, L"InstallLocation"));
    if (!dir)
        dir.Set(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_UNINST, L"InstallLocation"));
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

// TODO: must pass msg to CheckInstallUninstallPossible() instead
void SetDefaultMsg() {
    SetMsg(_TR("Are you sure you want to uninstall SumatraPDF?"), COLOR_MSG_WELCOME);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    bool handled;
    switch (message) {
        case WM_CREATE:
            if (!IsUninstallerNeeded()) {
                MessageBox(nullptr, _TR("SumatraPDF installation not found."), _TR("Uninstallation failed"),
                           MB_ICONEXCLAMATION | MB_OK);
                PostQuitMessage(0);
                return -1;
            }
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
            if (gHwndButtonExit)
                SetFocus(gHwndButtonExit);
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static bool RegisterWinClass() {
    WNDCLASSEX wcex;

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
        if (t.GetTimeInMs() > 10000 && gHwndButtonInstUninst && IsWindowEnabled(gHwndButtonInstUninst)) {
            CheckInstallUninstallPossible(true);
            t.Start();
        }
    }
}

static void ParseCommandLine(WCHAR* cmdLine) {
    WStrVec argList;
    ParseCmdLine(cmdLine, argList);

#define is_arg(param) str::EqI(arg + 1, TEXT(param))
#define is_arg_with_param(param) (is_arg(param) && i < argList.size() - 1)

    // skip the first arg (exe path)
    for (size_t i = 1; i < argList.size(); i++) {
        WCHAR* arg = argList.at(i);
        if ('-' != *arg && '/' != *arg)
            continue;

        if (is_arg("s"))
            gInstUninstGlobals.silent = true;
        else if (is_arg_with_param("d"))
            str::ReplacePtr(&gInstUninstGlobals.installDir, argList.at(++i));
        else if (is_arg("h") || is_arg("help") || is_arg("?"))
            gInstUninstGlobals.showUsageAndQuit = true;
#ifdef ENABLE_CRASH_TESTING
        else if (is_arg("crash")) {
            // will induce crash when 'Install' button is pressed
            // for testing crash handling
            gForceCrash = true;
        }
#endif
    }
}

#define CRASH_DUMP_FILE_NAME L"suminstaller.dmp"

// no-op but must be defined for CrashHandler.cpp
void ShowCrashHandlerMessage() {}
void GetStressTestInfo(str::Str<char>* s) {
    UNUSED(s);
}

void GetProgramInfo(str::Str<char>& s) {
    s.AppendFmt("Ver: %s", CURR_VERSION_STRA);
#ifdef SVN_PRE_RELEASE_VER
    s.AppendFmt(" pre-release");
#endif
    if (IsProcess64()) {
        s.Append(" 64-bit");
    }
#ifdef DEBUG
    if (!str::Find(s.Get(), " (dbg)"))
        s.Append(" (dbg)");
#endif
    s.Append("\r\n");
#if defined(GIT_COMMIT_ID)
    const char* gitSha1 = QM(GIT_COMMIT_ID);
    s.AppendFmt("Git: %s (https://github.com/sumatrapdfreader/sumatrapdf/tree/%s)\r\n", gitSha1, gitSha1);
#endif
}

bool CrashHandlerCanUseNet() {
    return true;
}

int APIENTRY WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /* lpCmdLine*/, int nCmdShow) {
    UNUSED(nCmdShow);
    int ret = 1;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    // Change current directory to prevent dll hijacking.
    // LoadLibrary first loads from current directory which could be
    // browser's download directory, which is an easy target
    // for attackers to put their own fake dlls).
    // For this to work we also have to /delayload all libraries otherwise
    // they will be loaded even before WinMain executes.
    auto currDir = GetSystem32Dir();
    SetCurrentDirectoryW(currDir);
    free(currDir);

    InitDynCalls();
    NoDllHijacking();

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    ParseCommandLine(GetCommandLine());
    if (gInstUninstGlobals.showUsageAndQuit) {
        ShowUsage();
        ret = 0;
        goto Exit;
    }
    if (!gInstUninstGlobals.installDir)
        gInstUninstGlobals.installDir = GetInstallationDir();

    if (ExecuteUninstallerFromTempDir())
        return 0;

    if (gInstUninstGlobals.silent) {
        UninstallerThread(nullptr);
        ret = gInstUninstGlobals.success ? 0 : 1;
        goto Exit;
    }

    if (!RegisterWinClass())
        goto Exit;

    if (!InstanceInit())
        goto Exit;

    ret = RunApp();

Exit:
    trans::Destroy();
    free(gInstUninstGlobals.installDir);
    free(gInstUninstGlobals.firstError);

    return ret;
}
