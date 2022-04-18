/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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
#include "utils/FileUtil.h"
#include "utils/Timer.h"

#include "utils/WinUtil.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/Log.h"
#include "utils/RegistryPaths.h"
#include "utils/GdiPlusUtil.h"

#include <tlhelp32.h>
#include <io.h>
#include "Translations.h"
#include "Version.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/wingui2.h"

#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "SumatraPDF.h"
#include "Installer.h"
#include "AppUtil.h"

using namespace wg;

static HBRUSH ghbrBackground = nullptr;
static HANDLE hThread = nullptr;
static bool success = false;
static Button* gButtonExit = nullptr;
static Button* gButtonUninstaller = nullptr;
static bool gWasSearchFilterInstalled = false;
static bool gWasPreviewInstaller = false;
static char* gUninstallerLogPath = nullptr;

static void OnButtonExit() {
    SendMessageW(gHwndFrame, WM_CLOSE, 0, 0);
}

static void CreateButtonExit(HWND hwndParent) {
    gButtonExit = CreateDefaultButton(hwndParent, _TR("Close"));
    gButtonExit->onClicked = OnButtonExit;
}

static bool RemoveUninstallerRegistryInfo(HKEY hkey) {
    logf("RemoveUninstallerRegistryInfo(%s)\n", RegKeyNameTemp(hkey));
    AutoFreeWstr regPathUninst = GetRegPathUninst(GetAppNameTemp());
    bool ok1 = LoggedDeleteRegKey(hkey, regPathUninst);
    // legacy, this key was added by installers up to version 1.8
    const WCHAR* appName = GetAppNameTemp();
    AutoFreeWstr key = str::Join(L"Software\\", appName);
    bool ok2 = LoggedDeleteRegKey(hkey, key);
    return ok1 && ok2;
}

static bool RemoveUninstallerRegistryInfo() {
    bool ok1 = RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    bool ok2 = RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);
    return ok1 || ok2;
}

// TODO: this method no longer works
#if 0
/* Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did */
static void UnregisterFromBeingDefaultViewer(HKEY hkey) {
    logf("UnregisterFromBeingDefaultViewer()\n");
    AutoFreeWstr curr = LoggedReadRegStr(hkey, kRegClassesPdf, nullptr);
    AutoFreeWstr regClassesApp = GetRegClassesApp(GetAppNameTemp());
    AutoFreeWstr prev = LoggedReadRegStr(hkey, regClassesApp, L"previous.pdf");
    const WCHAR* appName = GetAppNameTemp();
    if (!curr || !str::Eq(curr, appName)) {
        // not the default, do nothing
    } else if (prev) {
        LoggedWriteRegStr(hkey, kRegClassesPdf, nullptr, prev);
    } else {
#pragma warning(push)
#pragma warning(disable : 6387) // silence /analyze: '_Param_(3)' could be '0':  this does not adhere to the
                                // specification for the function 'SHDeleteValueW'
        SHDeleteValueW(hkey, kRegClassesPdf, nullptr);
#pragma warning(pop)
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    AutoFreeWstr buf = LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt, kRegProgId);
    if (str::Eq(buf, appName)) {
        LONG res = SHDeleteValueW(HKEY_CURRENT_USER, kRegExplorerPdfExt, kRegProgId);
        if (res != ERROR_SUCCESS) {
            LogLastError(res);
        }
    }
    const WCHAR* kRegApplication = L"Application";
    buf.Set(LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt, kRegApplication));
    const WCHAR* exeName = GetExeNameTemp();
    if (str::EqI(buf, exeName)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, kRegExplorerPdfExt, kRegApplication);
        if (res != ERROR_SUCCESS) {
            LogLastError(res);
        }
    }
    buf.Set(LoggedReadRegStr(HKEY_CURRENT_USER, kRegExplorerPdfExt L"\\UserChoice", kRegProgId));
    if (str::Eq(buf, appName)) {
        LoggedDeleteRegKey(HKEY_CURRENT_USER, kRegExplorerPdfExt L"\\UserChoice", true);
    }
}
#endif

// delete registry key but only if it's empty
static bool DeleteEmptyRegKey(HKEY root, const WCHAR* keyName) {
    HKEY hkey;
    LSTATUS status = RegOpenKeyExW(root, keyName, 0, KEY_READ, &hkey);
    if (status != ERROR_SUCCESS) {
        return true;
    }

    DWORD subkeys, values;
    bool isEmpty = false;
    status = RegQueryInfoKeyW(hkey, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, &values, nullptr, nullptr,
                              nullptr, nullptr);
    if (status == ERROR_SUCCESS) {
        isEmpty = 0 == subkeys && 0 == values;
    }
    RegCloseKey(hkey);
    if (!isEmpty) {
        return isEmpty;
    }

    LoggedDeleteRegKey(root, keyName);
    return isEmpty;
}

static void RemoveOwnRegistryKeys(HKEY hkey) {
    if (!hkey) {
        return;
    }
    logf("RemoveOwnRegistryKeys(%s)\n", RegKeyNameTemp(hkey));
    //UnregisterFromBeingDefaultViewer(hkey);
    const WCHAR* appName = GetAppNameTemp();
    const WCHAR* exeName = GetExeNameTemp();
    AutoFreeWstr regClassApp = GetRegClassesApp(appName);
    LoggedDeleteRegKey(hkey, regClassApp);
    AutoFreeWstr regClassApps = GetRegClassesApps(appName);
    LoggedDeleteRegKey(hkey, regClassApps);
    {
        AutoFreeWstr key = str::Join(kRegClassesPdf, L"\\OpenWithProgids");
        SHDeleteValueW(hkey, key, appName);
    }

    if (HKEY_LOCAL_MACHINE == hkey) {
        AutoFreeWstr key = str::Join(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", exeName);
        LoggedDeleteRegKey(hkey, key);
    }

    const WCHAR** supportedExts = GetSupportedExts();
    AutoFreeWstr openWithVal = str::Join(L"\\OpenWithList\\", exeName);
    for (int j = 0; nullptr != supportedExts[j]; j++) {
        AutoFreeWstr keyname(str::Join(L"Software\\Classes\\", supportedExts[j], L"\\OpenWithProgids"));
        SHDeleteValueW(hkey, keyname, appName);
        DeleteEmptyRegKey(hkey, keyname);

        keyname.Set(str::Join(L"Software\\Classes\\", supportedExts[j], openWithVal));
        if (!LoggedDeleteRegKey(hkey, keyname)) {
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
    SHDeleteValue(hkey, L"SOFTWARE\\RegisteredApplications", appName);
    AutoFreeWstr keyName = str::Format(L"SOFTWARE\\%s\\Capabilities", appName);
    LoggedDeleteRegKey(hkey, keyName);
}

static void RemoveOwnRegistryKeys() {
    RemoveOwnRegistryKeys(HKEY_LOCAL_MACHINE);
    RemoveOwnRegistryKeys(HKEY_CURRENT_USER);
    log("RemoveOwnRegistryKeys()\n");
}

#if 0
// The following list is used to verify that all the required files have been
// installed (install flag set) and to know what files are to be removed at
// uninstallation (all listed files that actually exist).
// When a file is no longer shipped, just disable the install flag so that the
// file is still correctly removed when SumatraPDF is eventually uninstalled.
// clang-format off
const char* gInstalledFiles[] = {
    "libmupdf.dll",
    "PdfFilter.dll",
    "PdfPreview.dll",
    // those probably won't delete because in use
    "SumatraPDF.exe",
    "RA-MICRO PDF Viewer.exe",
    // files no longer shipped, to be deleted
    "DroidSansFallback.ttf",
    "npPdfViewer.dll",
    "uninstall.exe",
    "UnRar.dll",
    "UnRar64.dll",
    // other files we might generate
    "sumatrapdfprefs.dat",
    "SumatraPDF-settings.txt",
};
// clang-format on
#endif

static void RemoveInstalledFiles() {
    // can't use GetExistingInstallationDir() anymore because we
    // delete registry entries
    WCHAR* dir = gCli->installDir;
    if (!dir) {
        log("RemoveInstalledFiles(): dir is empty\n");
    }
#if 0
    size_t n = dimof(gInstalledFiles);
    for (size_t i = 0; i < n; i++) {
        const char* s = gInstalledFiles[i];
        auto relPath = ToWstrTemp(s);
        AutoFreeWstr path = path::Join(dir, relPath);
        BOOL ok = DeleteFileW(path);
        if (ok) {
            logf(L"RemoveInstalledFiles(): removed '%s'\n", path.Get());
        }
    }
#endif
    bool ok = dir::RemoveAll(dir);
    logf(L"RemoveInstalledFiles(): removed dir '%s', ok = %d\n", dir, (int)ok);
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
    logf("removed shortcuts\n");
}

static DWORD WINAPI UninstallerThread(__unused LPVOID data) {
    log("UninstallerThread started\n");
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    AutoFreeWstr exePath = GetInstalledExePath();
    auto ownPath = GetExePathTemp();
    if (!path::IsSame(exePath, ownPath)) {
        KillProcessesWithModule(exePath, true);
    }

    if (!RemoveUninstallerRegistryInfo()) {
        log("RemoveUninstallerRegistryInfo failed\n");
        NotifyFailed(_TR("Failed to delete uninstaller registry keys"));
    }

    // mark them as uninstalled
    gWasSearchFilterInstalled = false;
    gWasPreviewInstaller = false;

    RemoveShortcuts();

    UninstallBrowserPlugin();
    RemoveOwnRegistryKeys();

    RemoveInstalledFiles();
    // NotifyFailed(_TR("Couldn't remove installation directory"));

    // always succeed, even for partial uninstallations
    success = true;

    log("UninstallerThread finished\n");
    if (!gCli->silent) {
        PostMessageW(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    }
    return 0;
}

static void OnButtonUninstall() {
    if (!CheckInstallUninstallPossible()) {
        return;
    }

    // disable the button during uninstallation
    gButtonUninstaller->SetIsEnabled(false);
    SetMsg(_TR("Uninstallation in progress..."), COLOR_MSG_INSTALLATION);
    HwndInvalidate(gHwndFrame);

    hThread = CreateThread(nullptr, 0, UninstallerThread, nullptr, 0, nullptr);
}

void OnUninstallationFinished() {
    delete gButtonUninstaller;
    gButtonUninstaller = nullptr;
    CreateButtonExit(gHwndFrame);
    SetMsg(_TR("SumatraPDF has been uninstalled."), gMsgError ? COLOR_MSG_FAILED : COLOR_MSG_OK);
    gMsgError = firstError;
    HwndInvalidate(gHwndFrame);

    CloseHandle(hThread);
}

static bool UninstallerOnWmCommand(WPARAM wp) {
    switch (LOWORD(wp)) {
        case IDCANCEL:
            OnButtonExit();
            break;

        default:
            return false;
    }
    return true;
}

static void OnCreateWindow(HWND hwnd) {
    gButtonUninstaller = CreateDefaultButton(hwnd, _TR("Uninstall SumatraPDF"));
    gButtonUninstaller->onClicked = OnButtonUninstall;
}

static void CreateMainWindow() {
    AutoFreeWstr title = str::Format(_TR("SumatraPDF %s Uninstaller"), CURR_VERSION_STR);
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = DpiScale(kInstallerWinDx);
    int dy = DpiScale(kInstallerWinDy);
    HMODULE h = GetModuleHandleW(nullptr);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    auto winCls = kInstallerWindowClassName;
    gHwndFrame = CreateWindowW(winCls, title.Get(), dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
}

static void ShowUsage() {
    // Note: translation services aren't initialized at this point, so English only
    const WCHAR* appName = GetAppNameTemp();
    AutoFreeWstr caption = str::Join(appName, L" Uninstaller Usage");
    AutoFreeWstr msg = str::Format(
        L"uninstall.exe [/s][/d <path>]\n\
    \n\
    /s\tuninstalls %s silently (without user interaction).\n\
    /d\tchanges the directory from where %s will be uninstalled.",
        appName, appName);
    MessageBoxW(nullptr, msg, caption, MB_OK | MB_ICONINFORMATION);
}

static LRESULT CALLBACK WndProcUninstallerFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool handled;

    LRESULT res = 0;
    res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res != 0) {
        return res;
    }

    switch (msg) {
        case WM_CREATE:
            OnCreateWindow(hwnd);
            break;

        case WM_CTLCOLORSTATIC: {
            if (ghbrBackground == nullptr) {
                ghbrBackground = CreateSolidBrush(RGB(0xff, 0xf2, 0));
            }
            HDC hdc = (HDC)wp;
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
            OnPaintFrame(hwnd, false);
            break;

        case WM_COMMAND:
            handled = UninstallerOnWmCommand(wp);
            if (!handled) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            break;

        case WM_APP_INSTALLATION_FINISHED:
            OnUninstallationFinished();
            if (gButtonExit) {
                gButtonExit->SetFocus();
            }
            break;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }

    return 0;
}

static bool RegisterWinClass() {
    WNDCLASSEX wcex{};

    FillWndClassEx(wcex, kInstallerWindowClassName, WndProcUninstallerFrame);
    auto h = GetModuleHandle(nullptr);
    WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
    wcex.hIcon = LoadIconW(h, iconName);

    ATOM atom = RegisterClassExW(&wcex);
    CrashIf(!atom);
    return atom != 0;
}

static BOOL InstanceInit() {
    CreateMainWindow();
    if (!gHwndFrame) {
        return FALSE;
    }

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
            res = MsgWaitForMultipleObjects(0, nullptr, TRUE, timeout, QS_ALLINPUT);
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
        if (dur > 10000 && gButtonUninstaller && gButtonUninstaller->IsEnabled()) {
            CheckInstallUninstallPossible(true);
            t = TimeGet();
        }
    }
}

static WCHAR* GetUninstallerPathInTemp() {
    WCHAR tempDir[MAX_PATH + 14]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    CrashAlwaysIf(res == 0 || res >= dimof(tempDir));
    return path::Join(tempDir, L"Sumatra-Uninstaller.exe");
}

// to be able to delete installation directory we must copy
// ourselves to temp directory and re-launch
static void RelaunchElevatedFromTempDirectory(Flags* cli) {
    log("RelaunchElevatedFromTempDirectory()\n");
    if (gIsDebugBuild) {
        // for easier debugging, debug build doesn't need
        // to be copied / re-launched
        return;
    }

    AutoFreeWstr installerTempPath = GetUninstallerPathInTemp();
    auto ownPath = GetExePathTemp();
    if (str::EqI(installerTempPath, ownPath)) {
        if (IsProcessRunningElevated()) {
            log("  already running elevated and from temp dir\n");
            return;
        }
    }

    logf(L"  copying installer '%s' to '%s'\n", ownPath.Get(), installerTempPath.Get());
    bool ok = file::Copy(installerTempPath, ownPath, false);
    if (!ok) {
        logf("  failed to copy installer\n");
        return;
    }

    // TODO: should extract cmd-line from GetCommandLineW() by skipping the first
    // item, which is path to the executable

    str::WStr cmdLine = L"-uninstall";
    if (cli->silent) {
        cmdLine.Append(L" -silent");
    }
    if (cli->log) {
        cmdLine.Append(L" -log");
    }
    logf(L"  re-launching '%s' with args '%s' as elevated\n", installerTempPath.Get(), cmdLine.Get());
    LaunchElevated(installerTempPath, cmdLine.Get());
    ::ExitProcess(0);
}

static WCHAR* GetSelfDeleteBatchPathInTemp() {
    WCHAR tempDir[MAX_PATH + 14]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    CrashAlwaysIf(res == 0 || res >= dimof(tempDir));
    return path::Join(tempDir, L"sumatra-self-del.bat");
}

// a hack to allow deleting our own executable
// we create a bash script that deletes us
static void InitSelfDelete() {
    log("InitSelfDelete()\n");
    auto exePath = GetExePathTemp();
    auto exePathA = ToUtf8Temp(exePath.AsView());
    str::Str script;
    // wait 2 seconds to give our process time to exit
    // alternatively use ping,
    // https://stackoverflow.com/questions/1672338/how-to-sleep-for-five-seconds-in-a-batch-file-cmd
    script.Append("timeout /t 2 /nobreak >nul\r\n");
    // delete our executable
    script.AppendFmt("del \"%s\"\r\n", exePathA.Get());
    // del itself
    // https://stackoverflow.com/questions/2888976/how-to-make-bat-file-delete-it-self-after-completion
    script.Append("(goto) 2>nul & del \"%~f0\"\r\n");

    AutoFreeWstr scriptPath = GetSelfDeleteBatchPathInTemp();
    auto scriptPathA = ToUtf8Temp(scriptPath.AsView());
    bool ok = file::WriteFile(scriptPathA, script.AsSpan());
    if (!ok) {
        logf("Failed to write '%s'\n", scriptPathA.Get());
        return;
    }
    logf("Created self-delete batch script '%s'\n", scriptPathA.Get());
    AutoFreeWstr cmdLine = str::Format(L"cmd.exe /C \"%s\"", scriptPath.Get());
    DWORD flags = CREATE_NO_WINDOW;
    LaunchProcess(cmdLine, nullptr, flags);
}

int RunUninstaller() {
    const char* uninstallerLogPath = nullptr;
    trans::SetCurrentLangByCode(trans::DetectUserLang());

    if (gCli->log) {
        // same as installer
        uninstallerLogPath = GetInstallerLogPath();
        if (uninstallerLogPath) {
            StartLogToFile(uninstallerLogPath, false);
        }
        logf("------------- Starting SumatraPDF uninstallation\n");
    }

    // TODO: remove dependency on this in the uninstaller
    gCli->installDir = GetExistingInstallationDir();
    WCHAR* cmdLine = GetCommandLineW();
    WCHAR* exePath = GetExePathTemp();
    logf(L"Running uninstaller '%s' with args '%s' for '%s'\n", exePath, cmdLine, gCli->installDir);

    int ret = 1;
    auto installerExists = file::Exists(exePath);
    if (!installerExists) {
        log("Uninstaller executable doesn't exist\n");
        const WCHAR* caption = _TR("Uninstallation failed");
        const WCHAR* msg = _TR("SumatraPDF installation not found.");
        MessageBox(nullptr, msg, caption, MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    if (gCli->showHelp) {
        ShowUsage();
        ret = 0;
        goto Exit;
    }

    RelaunchElevatedFromTempDirectory(gCli);

    gWasSearchFilterInstalled = IsSearchFilterInstalled();
    if (gWasSearchFilterInstalled) {
        log("Search filter is installed\n");
    }
    gWasPreviewInstaller = IsPreviewerInstalled();
    if (gWasPreviewInstaller) {
        log("Previewer is installed\n");
    }

    gDefaultMsg = _TR("Are you sure you want to uninstall SumatraPDF?");

    // unregister search filter and previewer to reduce
    // possibility of blocking
    if (gWasSearchFilterInstalled) {
        UnRegisterSearchFilter(true);
    }
    if (gWasPreviewInstaller) {
        UnRegisterPreviewer(true);
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

    // re-register if we un-registered but uninstallation was cancelled
    if (gWasSearchFilterInstalled) {
        RegisterSearchFilter(true);
    }
    if (gWasPreviewInstaller) {
        RegisterPreviewer(true);
    }
    InitSelfDelete();
    ShowLogFile(uninstallerLogPath);

Exit:
    free(firstError);
    return ret;
}
