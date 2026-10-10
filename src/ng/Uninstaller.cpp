/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's uninstaller window is win32 over the same painted frame as the
// installer's; here it is a gpui window with the same logo, message and single
// button. The previewer / search filter un-registration is gone with those
// features (step 17a).

#include "gui/GpuiBridge.h"

#include "base/File.h"
#include "base/Win.h"
#include "base/Timer.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "SumatraConfig.h"
#include "Flags.h"
#include "Version.h"
#include "AppTools.h"
#include "Translations.h"
#include "Installer.h"

#include "SumatraLog.h"

#if OS_WIN

// the installer's logo / message elements, shared as orig shares OnPaintFrame
gp::El* BuildInstallerLogo(gp::Ctx* cx);
gp::El* BuildInstallerMessage(gp::Ctx* cx);

struct UninstallerWnd {
    gp::App* app = nullptr;
    gp::Window* win = nullptr;
    bool uninstalling = false;
    bool finished = false;
    ThreadHandle hThread = nullptr;
};

static UninstallerWnd* gWnd = nullptr;
static bool gSuccess = false;
static volatile LONG gUninstallFinished = 0;
static Str gUninstallerLogPath;

static Str GetEnvRegKey(bool allUsers) {
    if (allUsers) {
        return StrL(R"(SYSTEM\CurrentControlSet\Control\Session Manager\Environment)");
    }
    return StrL("Environment");
}

static void RemoveInstallDirFromPath(bool allUsers, Str installDir) {
    HKEY root = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    Str keyName = GetEnvRegKey(allUsers);
    TempStr currPath = ReadRegStrTemp(root, keyName, StrL("Path"));
    if (len(currPath) == 0) {
        return;
    }
    if (!IsDirInPath(currPath, installDir)) {
        logf("RemoveInstallDirFromPath: '%s' not found in PATH\n", installDir);
        return;
    }

    str::Builder newPath;
    StrVec parts;
    Split(&parts, currPath, StrL(";"));
    for (Str entry : parts) {
        // skip empty entries and the one matching installDir (case-insensitive)
        if (len(entry) == 0 || str::EqI(entry, installDir)) {
            continue;
        }
        if (len(newPath) > 0) {
            newPath.Append(StrL(";"));
        }
        newPath.Append(entry);
    }

    if (!WriteRegExpandSz(root, keyName, StrL("Path"), ToStr(newPath))) {
        return;
    }
    logf("RemoveInstallDirFromPath: removed '%s' from PATH\n", installDir);
    // notify other processes that environment has changed
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, nullptr);
}

static void RemoveInstalledFiles() {
    // can't use GetExistingInstallationDir() anymore because we
    // delete registry entries
    Str dir = gCli->installDir;
    if (len(dir) == 0) {
        log(StrL("RemoveInstalledFiles(): dir is empty\n"));
        return;
    }
    bool ok = dir::RemoveAll(dir);
    logf("RemoveInstalledFiles(): removed dir '%s', ok = %d\n", dir, (int)ok);
}

static TempStr GetInstalledExePathTemp() {
    TempStr dir = gCli->installDir;
    return path::JoinTemp(dir, Str(kExeName));
}

static void UninstallerThread() {
    log(StrL("UninstallerThread started\n"));
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    TempStr exePath = GetInstalledExePathTemp();
    TempStr ownPath = GetSelfExePathTemp();
    if (!path::IsSame(exePath, ownPath)) {
        KillProcessesWithModule(exePath, true);
    }

    // TODO: reconsider what is failure
    bool ok = RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    ok |= RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);

    if (!ok) {
        log(StrL("RemoveUninstallerRegistryInfo failed\n"));
        NotifyFailed(Tr("Failed to delete uninstaller registry keys"));
    }

    RemoveInstallRegistryKeys(HKEY_LOCAL_MACHINE);
    RemoveInstallRegistryKeys(HKEY_CURRENT_USER);
    RemoveAppShortcuts();

    RemoveInstallDirFromPath(gCli->allUsers, gCli->installDir);
    RemoveInstalledFiles();
    LoggedDeleteRegValue(HKEY_CURRENT_USER, StrL("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                         StrL("SumatraPDF-QuickLook"));

    // always succeed, even for partial uninstallations
    gSuccess = true;

    log(StrL("UninstallerThread finished\n"));
    InterlockedExchange(&gUninstallFinished, 1);
}

static void OnButtonUninstall(UninstallerWnd* wnd) {
    if (!CheckInstallUninstallPossible()) {
        return;
    }

    // disable the button during uninstallation
    wnd->uninstalling = true;
    SetMsg(Tr("Uninstallation in progress..."), kColorMsgInstallation);

    auto fn = MkFunc0Void(UninstallerThread);
    wnd->hThread = StartThread(fn, StrL("UninstallerThread"));
}

static void OnUninstallationFinished(UninstallerWnd* wnd) {
    wnd->uninstalling = false;
    wnd->finished = true;
    SetMsg(Tr("SumatraPDF has been uninstalled."), gMsgError ? kColorMsgFailed : kColorMsgOk);
    gMsgError = gFirstError;
    SafeCloseThreadHandle(&wnd->hThread);
}

struct UninstallerView {
    static gp::El* Render(UninstallerView* self, gp::Ctx* cx);
    static void OnTick(UninstallerView* self, gp::Ctx* cx, const gp::TickEvent*);
    static void OnUninstall(UninstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnExit(UninstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<UninstallerView> gUninstallerView;

void UninstallerView::OnTick(UninstallerView*, gp::Ctx* cx, const gp::TickEvent*) {
    bool needsRedraw = IsRevealingLettersAnimRunning();
    AnimStep();
    if (gWnd && gWnd->uninstalling && InterlockedCompareExchange(&gUninstallFinished, 0, 1) == 1) {
        OnUninstallationFinished(gWnd);
        needsRedraw = true;
    }
    if (needsRedraw) {
        gp::Notify(cx);
    }
}

void UninstallerView::OnUninstall(UninstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    OnButtonUninstall(gWnd);
    gp::Notify(cx);
}

void UninstallerView::OnExit(UninstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gp::AppQuit(cx->win);
}

gp::El* UninstallerView::Render(UninstallerView*, gp::Ctx* cx) {
    UninstallerWnd* wnd = gWnd;
    float margin = 8;

    gp::El* root = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->H(gp::kFill)->Bg(ToGpui(kInstallerWinBgColor));
    root->Child(gp::Div(cx->a)->W(gp::kFill)->PadT(18)->Child(BuildInstallerLogo(cx)));
    root->Child(gp::Div(cx->a)->Flex1()->W(gp::kFill)->FlexCol()->JustifyCenter()->Pad(margin)->Child(
        BuildInstallerMessage(cx)));

    gp::El* bottom = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8)->Pad(margin);
    bottom->Child(gp::Div(cx->a)->Flex1());
    if (wnd->finished) {
        bottom->Child(gpc::Button::New(cx, GStrL("uninst-close"))
                          ->Label(ToGpui(Tr("Close")))
                          ->Primary()
                          ->WithSize(gp::UiSize::Small)
                          ->OnClick(gp::ListenTo(gUninstallerView, &UninstallerView::OnExit))
                          ->IntoEl());
    } else {
        bottom->Child(gpc::Button::New(cx, GStrL("uninst-run"))
                          ->Label(ToGpui(Tr("Uninstall SumatraPDF")))
                          ->Primary()
                          ->Disabled(wnd->uninstalling)
                          ->WithSize(gp::UiSize::Small)
                          ->OnClick(gp::ListenTo(gUninstallerView, &UninstallerView::OnUninstall))
                          ->IntoEl());
    }
    root->Child(bottom);
    return root;
}

static bool CreateUninstallerWindow() {
    gWnd = new UninstallerWnd();
    gp::App* app = gp::AppNew();
    gpc::Init(app);
    gWnd->app = app;
    gUninstallerView = gp::EntityNew<UninstallerView>(app);

    TempStr title = fmt(Tr("SumatraPDF %s Uninstaller").s, currentVersion);
    int dx = GetInstallerWinDx();
    int dy = kInstallerWinDy;
    gWnd->win = gp::WindowOpenView(app, ToGpui(title), dx, dy, gUninstallerView.id, gp::WinOpts{});
    if (!gWnd->win) {
        return false;
    }
    gp::WindowSetInterval(gWnd->win, 33, gp::ListenTo(gUninstallerView, &UninstallerView::OnTick));
    SetDefaultMsg();
    RevealingLettersAnimStart();
    return true;
}

static void ShowUsage() {
    // Note: translation services aren't initialized at this point, so English only
    TempStr caption = str::JoinTemp(StrL(kAppName), StrL(" Uninstaller Usage"));
    TempStr msg = fmt(R"(SumatraPDF.exe -uninstall [-s][-d <path>]

-s	uninstalls %s silently (without user interaction).
-d	changes the directory from where %s will be uninstalled.)",
                      StrL(kAppName), StrL(kAppName));
    logf("%s\n%s\n", caption, msg);
}

static TempStr GetUninstallerPathInTemp() {
    WCHAR tempDir[MAX_PATH + 14]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    ReportIf(res == 0 || res >= dimof(tempDir));
    TempStr dirA = ToUtf8Temp(tempDir);
    return path::JoinTemp(dirA, StrL("Sumatra-Uninstaller.exe"));
}

// %SystemRoot%\Temp, used instead of the per-user temp directory for the
// elevated copy. A file an elevated process creates there is owned by
// Administrators, so a non-elevated process running as the same user can't
// swap it for something else before we launch it.
static TempStr GetUninstallerPathInSystemTemp() {
    WCHAR winDir[MAX_PATH]{};
    UINT n = GetWindowsDirectoryW(winDir, dimof(winDir));
    if (n == 0 || n >= dimof(winDir)) {
        return {};
    }
    TempStr dir = path::JoinTemp(ToUtf8Temp(winDir), StrL("Temp"));
    return path::JoinTemp(dir, StrL("Sumatra-Uninstaller.exe"));
}

// to be able to delete installation directory we must copy
// ourselves to temp directory and re-launch
static void RelaunchMaybeElevatedFromTempDirectory(Flags* cli) {
    log(StrL("RelaunchMaybeElevatedFromTempDirectory()\n"));
    if (gIsDebugBuild) {
        // for easier debugging, debug build doesn't need
        // to be copied / re-launched
        return;
    }

    TempStr ownPath = GetSelfExePathTemp();
    str::Builder cmdLine;
    cmdLine.Append(StrL("-uninstall"));
    if (cli->silent) {
        cmdLine.Append(StrL(" -silent"));
    }
    if (cli->log) {
        cmdLine.Append(StrL(" -log"));
    }
    if (cli->allUsers) {
        cmdLine.Append(StrL(" -all-users"));
    }
    Str cl = ToStr(cmdLine);

    if (cli->allUsers) {
        if (!IsProcessRunningElevated()) {
            // Elevate the installed executable directly. Copying it to a
            // user-writable temporary path before elevation would let another
            // process replace it between the copy and ShellExecute.
            logf("LaunchElevated('%s', '%s')\n", ownPath, cl);
            bool okElev = LaunchElevated(ownPath, cl);
            if (!okElev) {
                logf("LaunchElevated() failed to launch '%s' '%s'\n", ownPath, cl);
                LogLastError();
            } else {
                logf("LaunchElevated() launched '%s' '%s' ok!\n", ownPath, cl);
            }
            ::ExitProcess(0);
        }

        // Elevated, but still running out of the directory we're about to
        // delete. Windows keeps a running image's file open, so dir::RemoveAll()
        // fails on our own exe and leaves the whole installation directory
        // behind (issue #5904). Re-launch from somewhere else first.
        //
        // Not the per-user temp directory: a copy there can be swapped for
        // something else before it runs, which is the elevation hole that
        // launching the installed exe directly closed. Use %SystemRoot%\Temp,
        // where a non-elevated process can create a file but cannot list the
        // directory or delete what's in it.
        TempStr sysTempPath = GetUninstallerPathInSystemTemp();
        if (str::IsEmptyOrWhiteSpace(sysTempPath) || str::EqI(sysTempPath, ownPath)) {
            log(StrL("  already running from the system temp dir (or couldn't find it)\n"));
            return;
        }
        file::Delete(sysTempPath);
        logf("  copying uninstaller '%s' to '%s'\n", ownPath, sysTempPath);
        if (!file::Copy(sysTempPath, ownPath, true)) {
            // best effort: uninstalling from the install dir still removes the
            // registry entries and most files, it just can't remove the folder
            logf("  failed to copy uninstaller to '%s', uninstalling in place\n", sysTempPath);
            return;
        }
        // deny writers for as long as the path is a launch target
        HANDLE hLock = CreateFileW(CWStrTemp(sysTempPath), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        logf("LaunchProcessWithCmdLine('%s' '%s')\n", sysTempPath, cl);
        HANDLE hElev = LaunchProcessWithCmdLine(sysTempPath, cl);
        if (hLock != INVALID_HANDLE_VALUE) {
            CloseHandle(hLock);
        }
        if (!hElev) {
            logf("LaunchProcessWithCmdLine() failed to launch '%s' '%s'\n", sysTempPath, cl);
            LogLastError();
            // the copy never ran, so keep going here rather than doing nothing
            return;
        }
        logf("LaunchProcessWithCmdLine() launched '%s' '%s' ok!\n", sysTempPath, cl);
        ::ExitProcess(0);
    }

    TempStr installerTempPath = GetUninstallerPathInTemp();
    if (str::EqI(installerTempPath, ownPath)) {
        log(StrL("  already running from temp dir\n"));
        return;
    }
    logf("  copying installer '%s' to '%s'\n", ownPath, installerTempPath);
    bool ok = file::Copy(installerTempPath, ownPath, false);
    if (!ok) {
        logf("  failed to copy installer\n");
        return;
    }
    logf("LaunchProcessWithCmdLine('%s' '%s')\n", installerTempPath, cl);
    HANDLE h = LaunchProcessWithCmdLine(installerTempPath, cl);
    if (!h) {
        logf("LaunchProcessWithCmdLine() failed to launch '%s' '%s'\n", installerTempPath, cl);
        LogLastError();
    } else {
        logf("LaunchProcessWithCmdLine() launched '%s' '%s' ok!\n", installerTempPath, cl);
    }
    ::ExitProcess(0);
}

static TempStr GetSystem32PathTemp(Str exeName) {
    WCHAR sysDir[MAX_PATH]{};
    UINT n = GetSystemDirectoryW(sysDir, dimof(sysDir));
    if (n == 0 || n >= dimof(sysDir)) {
        return {};
    }
    return path::JoinTemp(ToUtf8Temp(sysDir), exeName);
}

// A process can't delete its own executable: Windows keeps the image file open
// for as long as it runs. Put the delete on cmd.exe's command line - no
// intermediate file for anyone to tamper with, cmd.exe comes from System32 by
// absolute path so PATH can't redirect it, and the file goes away seconds
// after we exit rather than at the next boot.
static void InitSelfDelete() {
    log(StrL("InitSelfDelete()\n"));
    TempStr exePath = GetSelfExePathTemp();
    TempStr cmdExe = GetSystem32PathTemp(StrL("cmd.exe"));
    if (str::IsEmptyOrWhiteSpace(cmdExe)) {
        log(StrL("InitSelfDelete(): couldn't find cmd.exe\n"));
        return;
    }
    // ping, not timeout: timeout.exe exits immediately with "Input redirection
    // is not supported" whenever stdin isn't a console, which is exactly what a
    // child of a windowless process gets - the del then ran while we were still
    // running and failed. 3 pings to loopback is ~2s, enough for us to exit.
    TempStr cmdLine = fmt("\"%s\" /C ping -n 3 127.0.0.1 >nul & del \"%s\"", cmdExe, exePath);
    logf("InitSelfDelete(): '%s'\n", cmdLine);
    HANDLE h = LaunchProcessInDir(cmdLine, {}, CREATE_NO_WINDOW);
    if (!h) {
        logf("InitSelfDelete(): failed to launch, scheduling delete for next reboot\n");
        LogLastError();
        MoveFileExW(CWStrTemp(exePath), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        return;
    }
    CloseHandle(h);
}

int RunUninstaller(Flags* cli) {
    gCli = cli;
    gLogRegistryCalls = true;
    trans::SetCurrentLangByCode(trans::DetectUserLang());

    if (gCli->log) {
        // same as installer
        gUninstallerLogPath = GetInstallerLogPath();
        if (gUninstallerLogPath) {
            StartLogToFile(gUninstallerLogPath, false);
        }
        logf("------------- Starting SumatraPDF uninstallation\n");
    }

    // TODO: remove dependency on this in the uninstaller
    // dup from the perm arena: flag strings are never individually freed
    if (len(gCli->installDir) == 0) {
        gCli->installDir = str::Dup(GetPermArena(), GetExistingInstallationDirTemp());
    }
    Str instDir = gCli->installDir;
    TempStr exePath = GetSelfExePathTemp();
    logf("Running uninstaller '%s' for '%s'\n", exePath, instDir);

    int ret = 1;
    if (!file::Exists(exePath)) {
        log(StrL("Uninstaller executable doesn't exist\n"));
        // ng: orig shows a MessageBox; the port's MsgBox needs a MainWindow,
        // which the uninstaller has none of
        logf("%s: %s\n", Tr("Uninstallation failed"), Tr("SumatraPDF installation not found."));
        return ret;
    }

    if (gCli->showHelp) {
        ShowUsage();
        return 0;
    }

    RelaunchMaybeElevatedFromTempDirectory(gCli);

    gDefaultMsg = Tr("Are you sure you want to uninstall SumatraPDF?");

    if (gCli->silent) {
        UninstallerThread();
        return gSuccess ? 0 : 1;
    }

    if (!CreateUninstallerWindow()) {
        return ret;
    }
    ret = gp::AppRun(gWnd->app);

    InitSelfDelete();
    LaunchFileIfExists(gUninstallerLogPath);
    return ret;
}

#else

int RunUninstaller(Flags*) {
    log(StrL("-uninstall is not supported on this platform\n"));
    return 1;
}

#endif
