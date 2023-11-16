/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/Timer.h"

#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/GdiPlusUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "SumatraConfig.h"
#include "Flags.h"
#include "SumatraPDF.h"
#include "Installer.h"
#include "AppTools.h"
#include "Translations.h"
#include "Version.h"

#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"

#include "utils/Log.h"

static HBRUSH ghbrBackground = nullptr;
static HANDLE hThread = nullptr;
static bool success = false;
static Button* gButtonExit = nullptr;
static Button* gButtonUninstaller = nullptr;
static bool gWasSearchFilterInstalled = false;
static bool gWasPreviewInstaller = false;
static char* gUninstallerLogPath = nullptr;

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
    char* dir = gCli->installDir;
    if (!dir) {
        log("RemoveInstalledFiles(): dir is empty\n");
    }
#if 0
    size_t n = dimof(gInstalledFiles);
    for (size_t i = 0; i < n; i++) {
        const char* s = gInstalledFiles[i];
        auto relPath = ToWStrTemp(s);
        AutoFreeWstr path = path::Join(dir, relPath);
        BOOL ok = file::Delete(path);
        if (ok) {
            logf(L"RemoveInstalledFiles(): removed '%s'\n", path.Get());
        }
    }
#endif
    bool ok = dir::RemoveAll(dir);
    logf("RemoveInstalledFiles(): removed dir '%s', ok = %d\n", dir, (int)ok);
}

static DWORD WINAPI UninstallerThread(void*) {
    log("UninstallerThread started\n");
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    TempStr exePath = GetInstalledExePathTemp();
    TempStr ownPath = GetExePathTemp();
    if (!path::IsSame(exePath, ownPath)) {
        KillProcessesWithModule(exePath, true);
    }

    // TODO: reconsider what is failure
    bool ok = RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    ok |= RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);

    if (!ok) {
        log("RemoveUninstallerRegistryInfo failed\n");
        NotifyFailed(_TRA("Failed to delete uninstaller registry keys"));
    }

    // mark them as uninstalled
    gWasSearchFilterInstalled = false;
    gWasPreviewInstaller = false;

    UninstallBrowserPlugin();
    RemoveInstallRegistryKeys(HKEY_LOCAL_MACHINE);
    RemoveInstallRegistryKeys(HKEY_CURRENT_USER);
    RemoveAppShortcuts();

    RemoveInstalledFiles();

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

static void OnButtonExit() {
    SendMessageW(gHwndFrame, WM_CLOSE, 0, 0);
}

void OnUninstallationFinished() {
    delete gButtonUninstaller;
    gButtonUninstaller = nullptr;
    gButtonExit = CreateDefaultButton(gHwndFrame, _TR("Close"));
    gButtonExit->onClicked = OnButtonExit;
    SetMsg(_TR("SumatraPDF has been uninstalled."), gMsgError ? COLOR_MSG_FAILED : COLOR_MSG_OK);
    gMsgError = gFirstError;
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

#define kInstallerWindowClassName L"SUMATRA_PDF_INSTALLER_FRAME"

static void CreateUninstallerWindow() {
    TempStr title = str::FormatTemp(_TRA("SumatraPDF %s Uninstaller"), CURR_VERSION_STRA);
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = GetInstallerWinDx();
    int dy = kInstallerWinDy;
    HMODULE h = GetModuleHandleW(nullptr);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    auto winCls = kInstallerWindowClassName;
    gHwndFrame = CreateWindowW(winCls, ToWStrTemp(title), dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);

    DpiScale(gHwndFrame, dx, dy);
    HwndResizeClientSize(gHwndFrame, dx, dy);

    gButtonUninstaller = CreateDefaultButton(gHwndFrame, _TR("Uninstall SumatraPDF"));
    gButtonUninstaller->onClicked = OnButtonUninstall;
}

static void ShowUsage() {
    // Note: translation services aren't initialized at this point, so English only
    TempStr caption = str::JoinTemp(kAppName, " Uninstaller Usage");
    TempStr msg = str::FormatTemp(
        "uninstall.exe [/s][/d <path>]\n\
    \n\
    /s\tuninstalls %s silently (without user interaction).\n\
    /d\tchanges the directory from where %s will be uninstalled.",
        kAppName, kAppName);
    MessageBoxA(nullptr, msg, caption, MB_OK | MB_ICONINFORMATION);
}

static LRESULT CALLBACK WndProcUninstallerFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool handled;

    LRESULT res = 0;
    res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res != 0) {
        return res;
    }

    switch (msg) {
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

        case WM_COMMAND: {
            handled = UninstallerOnWmCommand(wp);
            if (!handled) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            break;
        }

        case WM_APP_INSTALLATION_FINISHED: {
            OnUninstallationFinished();
            if (gButtonExit) {
                gButtonExit->SetFocus();
            }
            SetForegroundWindow(hwnd);
            break;
        }

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

static bool InstanceInit() {
    CreateUninstallerWindow();
    if (!gHwndFrame) {
        return FALSE;
    }

    SetDefaultMsg();

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);
    SetForegroundWindow(gHwndFrame);

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

static char* GetUninstallerPathInTemp() {
    WCHAR tempDir[MAX_PATH + 14]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    CrashAlwaysIf(res == 0 || res >= dimof(tempDir));
    char* dirA = ToUtf8Temp(tempDir);
    return path::Join(dirA, "Sumatra-Uninstaller.exe");
}

// to be able to delete installation directory we must copy
// ourselves to temp directory and re-launch
static void RelaunchMaybeElevatedFromTempDirectory(Flags* cli) {
    log("RelaunchMaybeElevatedFromTempDirectory()\n");
    if (gIsDebugBuild) {
        // for easier debugging, debug build doesn't need
        // to be copied / re-launched
        return;
    }

    char* installerTempPath = GetUninstallerPathInTemp();
    char* ownPath = GetExePathTemp();
    if (str::EqI(installerTempPath, ownPath)) {
        if (!gCli->allUsers) {
            log("  already running from temp dir\n");
            return;
        }
        if (IsProcessRunningElevated()) {
            log("  already running elevated and from temp dir\n");
            return;
        }
    }

    logf("  copying installer '%s' to '%s'\n", ownPath, installerTempPath);
    bool ok = file::Copy(installerTempPath, ownPath, false);
    if (!ok) {
        logf("  failed to copy installer\n");
        return;
    }

    // TODO: should extract cmd-line from GetCommandLineW() by skipping the first
    // item, which is path to the executable
    str::Str cmdLine = "-uninstall";
    if (cli->silent) {
        cmdLine.Append(" -silent");
    }
    if (cli->log) {
        cmdLine.Append(" -log");
    }
    if (cli->allUsers) {
        cmdLine.Append(" -all-users");
    }
    logf("  re-launching '%s' with args '%s' as elevated\n", installerTempPath, cmdLine.Get());
    if (cli->allUsers) {
        LaunchElevated(installerTempPath, cmdLine.Get());
    } else {
        LaunchProcess(installerTempPath, cmdLine.Get());
    }
    ::ExitProcess(0);
}

static char* GetSelfDeleteBatchPathInTemp() {
    WCHAR tempDir[MAX_PATH + 14]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    CrashAlwaysIf(res == 0 || res >= dimof(tempDir));
    char* tempDirA = ToUtf8Temp(tempDir);
    return path::JoinTemp(tempDirA, "sumatra-self-del.bat");
}

// a hack to allow deleting our own executable
// we create a bash script that deletes us
static void InitSelfDelete() {
    log("InitSelfDelete()\n");
    TempStr exePath = GetExePathTemp();
    str::Str script;
    // wait 2 seconds to give our process time to exit
    // alternatively use ping,
    // https://stackoverflow.com/questions/1672338/how-to-sleep-for-five-seconds-in-a-batch-file-cmd
    script.Append("timeout /t 2 /nobreak >nul\r\n");
    // delete our executable
    script.AppendFmt("del \"%s\"\r\n", exePath);
    // del itself
    // https://stackoverflow.com/questions/2888976/how-to-make-bat-file-delete-it-self-after-completion
    script.Append("(goto) 2>nul & del \"%~f0\"\r\n");

    char* scriptPath = GetSelfDeleteBatchPathInTemp();
    bool ok = file::WriteFile(scriptPath, script.AsByteSlice());
    if (!ok) {
        logf("Failed to write '%s'\n", scriptPath);
        return;
    }
    logf("Created self-delete batch script '%s'\n", scriptPath);
    TempStr cmdLine = str::FormatTemp("cmd.exe /C \"%s\"", scriptPath);
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
    char* instDir = gCli->installDir;
    TempStr cmdLine = ToUtf8Temp(GetCommandLineW());
    TempStr exePath = GetExePathTemp();
    logf("Running uninstaller '%s' with args '%s' for '%s'\n", exePath, cmdLine, instDir);

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

    RelaunchMaybeElevatedFromTempDirectory(gCli);

    gWasSearchFilterInstalled = IsSearchFilterInstalled();
    if (gWasSearchFilterInstalled) {
        log("Search filter is installed\n");
    }
    gWasPreviewInstaller = IsPreviewInstalled();
    if (gWasPreviewInstaller) {
        log("Previewer is installed\n");
    }

    gDefaultMsg = _TR("Are you sure you want to uninstall SumatraPDF?");

    // unregister search filter and previewer to reduce
    // possibility of blocking
    if (gWasSearchFilterInstalled) {
        UnRegisterSearchFilter();
    }
    if (gWasPreviewInstaller) {
        UnRegisterPreviewer();
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
    ret = RunApp();

    // re-register if we un-registered but uninstallation was cancelled
    if (gWasSearchFilterInstalled) {
        RegisterSearchFilter(gCli->allUsers);
    }
    if (gWasPreviewInstaller) {
        RegisterPreviewer(gCli->allUsers);
    }
    InitSelfDelete();
    LaunchFileIfExists(uninstallerLogPath);

Exit:
#if 0 // technically a leak but there's no point
    free(gFirstError);
#endif
    return ret;
}
