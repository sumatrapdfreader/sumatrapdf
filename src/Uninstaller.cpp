/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
#include "resource.h"
#include "utils/Timer.h"
#include "Version.h"
#include "utils/WinUtil.h"
#include "utils/CmdLineParser.h"
#include "CrashHandler.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/Log.h"
#include "utils/RegistryPaths.h"
#include "utils/GdiPlusUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/EditCtrl.h"
#include "wingui/ImageCtrl.h"
#include "wingui/StaticCtrl.h"

#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "Installer.h"
#include "AppUtil.h"

#define UNINSTALLER_WIN_DX INSTALLER_WIN_DX
#define UNINSTALLER_WIN_DY INSTALLER_WIN_DY

static HBRUSH ghbrBackground = nullptr;
static HANDLE hThread = nullptr;
static bool success = false;
static ButtonCtrl* gButtonExit = nullptr;
static ButtonCtrl* gButtonUninstaller = nullptr;
static bool gWasSearchFilterInstalled = false;
static bool gWasPreviewInstaller = false;

static void OnButtonExit() {
    SendMessageW(gHwndFrame, WM_CLOSE, 0, 0);
}

static void CreateButtonExit(HWND hwndParent) {
    gButtonExit = CreateDefaultButtonCtrl(hwndParent, _TR("Close"));
    gButtonExit->onClicked = OnButtonExit;
}

static bool RemoveUninstallerRegistryInfo(HKEY hkey) {
    AutoFreeWstr REG_PATH_UNINST = GetRegPathUninst(GetAppName());
    bool ok1 = DeleteRegKey(hkey, REG_PATH_UNINST);
    // legacy, this key was added by installers up to version 1.8
    const WCHAR* appName = GetAppName();
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
    AutoFreeWstr REG_CLASSES_APP = GetRegClassesApp(GetAppName());
    AutoFreeWstr prev = ReadRegStr(hkey, REG_CLASSES_APP, L"previous.pdf");
    const WCHAR* appName = GetAppName();
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
    AutoFreeWstr buf = ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID);
    if (str::Eq(buf, appName)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID);
        if (res != ERROR_SUCCESS) {
            LogLastError(res);
        }
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION));
    const WCHAR* exeName = GetExeName();
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

    if (isEmpty) {
        DeleteRegKey(root, keyName);
    }
    return isEmpty;
}

static void RemoveOwnRegistryKeys(HKEY hkey) {
    UnregisterFromBeingDefaultViewer(hkey);
    const WCHAR* appName = GetAppName();
    const WCHAR* exeName = GetExeName();
    AutoFreeWstr regClassApp = GetRegClassesApp(appName);
    DeleteRegKey(hkey, regClassApp);
    AutoFreeWstr regClassApps = GetRegClassesApps(appName);
    DeleteRegKey(hkey, regClassApps);
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_PDF, L"\\OpenWithProgids");
        SHDeleteValueW(hkey, key, appName);
    }

    if (HKEY_LOCAL_MACHINE == hkey) {
        AutoFreeWstr key = str::Join(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", exeName);
        DeleteRegKey(hkey, key);
    }

    const WCHAR** supportedExts = GetSupportedExts();
    AutoFreeWstr openWithVal = str::Join(L"\\OpenWithList\\", exeName);
    for (int j = 0; nullptr != supportedExts[j]; j++) {
        AutoFreeWstr keyname(str::Join(L"Software\\Classes\\", supportedExts[j], L"\\OpenWithProgids"));
        SHDeleteValueW(hkey, keyname, appName);
        DeleteEmptyRegKey(hkey, keyname);

        keyname.Set(str::Join(L"Software\\Classes\\", supportedExts[j], openWithVal));
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
    SHDeleteValue(hkey, L"SOFTWARE\\RegisteredApplications", appName);
    AutoFreeWstr keyName = str::Format(L"SOFTWARE\\%s\\Capabilities", appName);
    DeleteRegKey(hkey, keyName);
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
        AutoFreeWstr relPath = strconv::Utf8ToWstr(s);
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

void onRaMicroUninstallerFinished();

static DWORD WINAPI UninstallerThread([[maybe_unused]] LPVOID data) {
    log("UninstallerThread started\n");
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    AutoFreeWstr exePath = GetInstalledExePath();
    AutoFreeWstr ownPath = GetExePath();
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

    if (gIsRaMicroBuild) {
        onRaMicroUninstallerFinished();
        return 0;
    }

    log("UninstallerThread finished\n");
    if (!gCli->silent) {
        PostMessageW(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    }
    return 0;
}

static void InvalidateFrame() {
    Rect rc = ClientRect(gHwndFrame);
    RECT rcTmp = ToRECT(rc);
    InvalidateRect(gHwndFrame, &rcTmp, FALSE);
}

static void OnButtonUninstall() {
    if (!CheckInstallUninstallPossible()) {
        return;
    }

    // disable the button during uninstallation
    gButtonUninstaller->SetIsEnabled(false);
    SetMsg(_TR("Uninstallation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    hThread = CreateThread(nullptr, 0, UninstallerThread, nullptr, 0, 0);
}

void OnUninstallationFinished() {
    delete gButtonUninstaller;
    gButtonUninstaller = nullptr;
    CreateButtonExit(gHwndFrame);
    SetMsg(_TR("SumatraPDF has been uninstalled."), gMsgError ? COLOR_MSG_FAILED : COLOR_MSG_OK);
    gMsgError = firstError;
    InvalidateFrame();

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
    gButtonUninstaller = CreateDefaultButtonCtrl(hwnd, _TR("Uninstall SumatraPDF"));
    gButtonUninstaller->onClicked = OnButtonUninstall;
}

static void CreateMainWindow() {
    AutoFreeWstr title = str::Format(_TR("SumatraPDF %s Uninstaller"), CURR_VERSION_STR);
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = DpiScale(INSTALLER_WIN_DX);
    int dy = DpiScale(INSTALLER_WIN_DY);
    HMODULE h = GetModuleHandleW(nullptr);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    auto winCls = INSTALLER_FRAME_CLASS_NAME;
    gHwndFrame = CreateWindowW(winCls, title.Get(), dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
}

static void ShowUsage() {
    // Note: translation services aren't initialized at this point, so English only
    const WCHAR* appName = GetAppName();
    AutoFreeWstr caption = str::Join(appName, L" Uninstaller Usage");
    AutoFreeWstr msg = str::Format(
        L"uninstall.exe [/s][/d <path>]\n\
    \n\
    /s\tuninstalls %s silently (without user interaction).\n\
    /d\tchanges the directory from where %s will be uninstalled.",
        appName, appName);
    MessageBoxW(nullptr, msg, caption, MB_OK | MB_ICONINFORMATION);
}

#if 0
static WCHAR* GetInstallationDir() {
    WCHAR* dir = GetExistingInstallationDir();
    if (dir) {
        return dir;
    }
    // fall back to the uninstaller's path
    AutoFreeWstr exePath = GetExePath();
    return path::GetDir(exePath);
}
#endif

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool handled;

    LRESULT res = 0;
    if (HandleRegisteredMessages(hwnd, msg, wp, lp, res)) {
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
            OnPaintFrame(hwnd);
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

    FillWndClassEx(wcex, INSTALLER_FRAME_CLASS_NAME, WndProcFrame);
    auto h = GetModuleHandle(nullptr);
    WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
    wcex.hIcon = LoadIconW(h, iconName);

    ATOM atom = RegisterClassExW(&wcex);
    CrashIf(!atom);
    return atom != 0;
}

static BOOL InstanceInit() {
    InitInstallerUninstaller();

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
        if (dur > 10000 && gButtonUninstaller && gButtonUninstaller->IsEnabled()) {
            CheckInstallUninstallPossible(true);
            t = TimeGet();
        }
    }
}

static char* PickUnInstallerLogPath() {
    AutoFreeWstr dir = GetSpecialFolder(CSIDL_LOCAL_APPDATA, true);
    if (!dir) {
        return nullptr;
    }
    AutoFreeStr dira = strconv::WstrToUtf8(dir);
    return path::JoinUtf(dira, "sumatra-uninstall-log.txt", nullptr);
}

static void StartUnInstallerLogging() {
    char* dir = PickUnInstallerLogPath();
    if (!dir) {
        return;
    }
    StartLogToFile(dir);
    free(dir);
}

static WCHAR* GetUninstallerPathInTemp() {
    WCHAR tempDir[MAX_PATH + 14] = {0};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    CrashAlwaysIf(res == 0 || res >= dimof(tempDir));
    return path::Join(tempDir, L"Sumatra-Uninstaller.exe");
}

// to be able to delete installation directory we must copy
// ourselves to temp directory and re-launch
static void RelaunchElevatedFromTempDirectory(Flags* cli) {
    if (gIsDebugBuild) {
        // for easier debugging, debug build doesn't need
        // to be copied / re-launched
        return;
    }

    AutoFreeWstr installerTempPath = GetUninstallerPathInTemp();
    AutoFreeWstr ownPath = GetExePath();
    if (str::EqI(installerTempPath, ownPath)) {
        if (IsProcessRunningElevated()) {
            log("Already running elevated and from temp dir\n");
            return;
        }
    }

    logf(L"must copy installer '%s' to '%s'\n", ownPath.Get(), installerTempPath.Get());
    BOOL ok = CopyFileW(ownPath, installerTempPath, FALSE);
    if (!ok) {
        logf("failed to copy installer\n");
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
    logf(L"Re-launching '%s' with args '%s' as elevated\n", installerTempPath.Get(), cmdLine.Get());
    LaunchElevated(installerTempPath, cmdLine.Get());
    ::ExitProcess(0);
}

static WCHAR* GetSelfDeleteBatchPathInTemp() {
    WCHAR tempDir[MAX_PATH + 14] = {0};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    CrashAlwaysIf(res == 0 || res >= dimof(tempDir));
    return path::Join(tempDir, L"sumatra-self-del.bat");
}

// a hack to allow deleting our own executable
// we create a bash script that deletes us
static void InitSelfDelete() {
    AutoFreeWstr exePath = GetExePath();
    AutoFreeStr exePathA = strconv::WstrToUtf8(exePath);
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
    AutoFreeStr scriptPathA = strconv::WstrToUtf8(scriptPath.AsView());
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

int RunUninstallerRaMicro();

int RunUninstaller(Flags* cli) {
    logToDebugger = true;
    gCli = cli;
    if (gCli->log) {
        StartUnInstallerLogging();
    }
    // TODO: remove dependency on this in the uninstaller
    gCli->installDir = GetExistingInstallationDir();
    WCHAR* cmdLine = GetCommandLineW();
    WCHAR* exePath = GetExePath();
    logf(L"Starting uninstaller '%s' with args '%s' for '%s'\n", exePath, cmdLine, gCli->installDir);

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

    RelaunchElevatedFromTempDirectory(cli);

    if (gIsRaMicroBuild) {
        return RunUninstallerRaMicro();
    }

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
Exit:
    free(firstError);
    return ret;
}

/* ra-micro uninstaller */

using std::placeholders::_1;

struct RaMicroUninstallerWindow {
    HWND hwnd = nullptr;
    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;
    Gdiplus::Bitmap* bmpSplash = nullptr;

    // not owned by us but by mainLayout
    ButtonCtrl* btnInstall = nullptr;
    ButtonCtrl* btnExit = nullptr;
    StaticCtrl* finishedText = nullptr;

    bool finished = false;

    ~RaMicroUninstallerWindow();

    void CloseHandler(WindowCloseEvent*);
    void SizeHandler(SizeEvent*);
    void Uninstall();
    void InstallationFinished();
    void Exit();
    void MsgHandler(WndEvent*);
};

static RaMicroUninstallerWindow* gRaMicroUninstallerWindow = nullptr;

RaMicroUninstallerWindow::~RaMicroUninstallerWindow() {
    delete mainLayout;
    delete mainWindow;
    delete bmpSplash;
}

void RaMicroUninstallerWindow::MsgHandler(WndEvent* ev) {
    if (ev->msg == WM_APP_INSTALLATION_FINISHED) {
        InstallationFinished();
        ev->didHandle = true;
        return;
    }
}

void RaMicroUninstallerWindow::Uninstall() {
    if (finished) {
        Exit();
        return;
    }
    hThread = CreateThread(nullptr, 0, UninstallerThread, nullptr, 0, 0);
}

static Size layoutAndSize(ILayout* layout, int dx, int dy) {
    if (dx == 0 || dy == 0) {
        return {};
    }
    return LayoutToSize(layout, {dx, dy});
}

void RaMicroUninstallerWindow::InstallationFinished() {
    CloseHandle(hThread);
    hThread = nullptr;

    finished = true;
    btnInstall->SetText("Exit");
    if (!success) {
        finishedText->SetText("Uninstallation failed!");
    }
    finishedText->SetIsVisible(true);

    RECT rc = GetClientRect(hwnd);
    int dx = RectDx(rc);
    int dy = RectDy(rc);
    layoutAndSize(mainLayout, dx, dy);
}

void RaMicroUninstallerWindow::Exit() {
    gRaMicroUninstallerWindow->mainWindow->Close();
}

void RaMicroUninstallerWindow::CloseHandler(WindowCloseEvent* ev) {
    WindowBase* w = (WindowBase*)gRaMicroUninstallerWindow->mainWindow;
    CrashIf(w != ev->w);
    delete gRaMicroUninstallerWindow;
    gRaMicroUninstallerWindow = nullptr;
    PostQuitMessage(0);
}

void onRaMicroUninstallerFinished() {
    // called on a background thread
    PostMessageW(gRaMicroUninstallerWindow->hwnd, WM_APP_INSTALLATION_FINISHED, 0, 0);
}

void RaMicroUninstallerWindow::SizeHandler(SizeEvent* ev) {
    int dx = ev->dx;
    int dy = ev->dy;

    layoutAndSize(mainLayout, dx, dy);

    InvalidateRect(ev->hwnd, nullptr, false);
    ev->didHandle = true;
}

void onRaMicroUnistallerFinished() {
    // called on a background thread
    PostMessageW(gRaMicroUninstallerWindow->hwnd, WM_APP_INSTALLATION_FINISHED, 0, 0);
}

static Gdiplus::Bitmap* LoadRaMicroSplash() {
    auto d = LoadDataResource(IDD_RAMICRO_SPLASH);
    if (d.empty()) {
        return nullptr;
    }
    return BitmapFromData(d);
}

static bool CreateRaMicroUninstallerWindow() {
    HMODULE h = GetModuleHandleW(nullptr);
    WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
    HICON hIcon = LoadIconW(h, iconName);

    auto win = new RaMicroUninstallerWindow();
    gRaMicroUninstallerWindow = win;

    win->bmpSplash = LoadRaMicroSplash();
    CrashIf(!win->bmpSplash);

    auto w = new Window();
    w->msgFilter = std::bind(&RaMicroUninstallerWindow::MsgHandler, win, _1);
    w->hIcon = hIcon;
    // w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->backgroundColor = MkRgb((u8)0xff, (u8)0xff, (u8)0xff);
    w->SetTitle("RA-MICRO Uninstaller");
    int splashDx = (int)win->bmpSplash->GetWidth();
    int splashDy = (int)win->bmpSplash->GetHeight();
    int dx = splashDx + DpiScale(32 + 44); // image + padding
    int dy = splashDy + DpiScale(104);     // image + buttons
    w->initialSize = {dx, dy};
    SIZE winSize = {w->initialSize.dx, w->initialSize.dy};
    w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);
    win->hwnd = w->hwnd;

    win->mainWindow = w;

    HWND hwnd = win->hwnd;
    CrashIf(!hwnd);

    // create layout
    // TODO: image should be centered, the buttons should be on the edges
    // Probably need to implement a Center layout
    HBox* buttons = new HBox();
    buttons->alignMain = MainAxisAlign::SpaceBetween;
    buttons->alignCross = CrossAxisAlign::CrossEnd;

    /*
    {
        auto [l, b] = CreateButtonLayout(hwnd, "Exit", [win]() { win->Exit(); });
        buttons->addChild(l);
        win->btnExit = b;
    }
    */

    {
        auto b = CreateButton(hwnd, "Uninstall RA-Micro", [win]() { win->Uninstall(); });
        buttons->AddChild(b);
        win->btnInstall = b;
    }

    VBox* main = new VBox();
    main->alignMain = MainAxisAlign::SpaceAround;
    main->alignCross = CrossAxisAlign::CrossCenter;

    ImageCtrl* splashCtrl = new ImageCtrl(hwnd);
    splashCtrl->bmp = win->bmpSplash;
    ok = splashCtrl->Create();
    CrashIf(!ok);
    main->AddChild(splashCtrl);

    win->finishedText = new StaticCtrl(hwnd);
    win->finishedText->SetText("RA-MICRO was uninstalled!");
    // TODO: bigger font and maybe bold and different color
    // win->finishedText->SetFont();
    win->finishedText->Create();
    win->finishedText->SetIsVisible(false);

    main->AddChild(win->finishedText);

    main->AddChild(buttons);

    auto padding = new Padding(main, DpiScaledInsets(hwnd, 8));
    win->mainLayout = padding;

    w->onClose = std::bind(&RaMicroUninstallerWindow::CloseHandler, win, _1);
    w->onSize = std::bind(&RaMicroUninstallerWindow::SizeHandler, win, _1);
    w->SetIsVisible(true);
    return true;
}

int RunUninstallerRaMicro() {
    int ret{1};
    bool ok{false};
    const WCHAR* msgFmt = _TR("Are you sure you want to uninstall %s?");
    const WCHAR* appName = GetAppName();
    gDefaultMsg = str::Format(msgFmt, appName);

    AutoFreeWstr exePath(GetInstalledExePath());
    auto installerExists = file::Exists(exePath);

    if (gCli->showHelp) {
        ShowUsage();
        ret = 0;
        goto Exit;
    }

    if (!installerExists) {
        const WCHAR* caption = _TR("Uninstallation failed");
        msgFmt = _TR("%s installation not found.");
        const WCHAR* msg = str::Format(msgFmt, appName);
        MessageBox(nullptr, msg, caption, MB_ICONEXCLAMATION | MB_OK);
        str::Free(msg);
        goto Exit;
    }

    if (gCli->silent) {
        UninstallerThread(nullptr);
        ret = success ? 0 : 1;
        goto Exit;
    }

    ok = CreateRaMicroUninstallerWindow();
    if (!ok) {
        goto Exit;
    }

    ret = RunApp();

Exit:
    free(firstError);

    return ret;
}
