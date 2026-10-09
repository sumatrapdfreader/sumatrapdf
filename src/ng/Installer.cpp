/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's installer window is win32 (a yellow frame it paints with GDI+,
// real Button / Edit / Checkbox controls over it). Here it is a gpui window of
// the same size with the same rows, texts and behaviour. Gone with the
// features they installed (step 17a): the lzsa payload, the "Let Windows show
// previews" / "Let Windows Desktop Search search" checkboxes and everything
// that unregistered PdfFilter.dll / PdfPreview.dll before an upgrade - a
// static exe installs itself by copying.

#include "gui/GpuiBridge.h"

#include "base/File.h"
#include "base/Win.h"
#include "base/Timer.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Flags.h"
#include "Version.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "gui/DialogWidgets.h"
#include "Installer.h"
#include "UpdateCheck.h"

#include "SumatraLog.h"

#if OS_WIN

constexpr int kInstallerWinMargin = 8;

struct InstallerWnd;

static InstallerWnd* gWnd = nullptr;
static bool gInstallStarted = false; // a bit of a hack
static bool gInstallFailed = false;
static volatile LONG gInstallFinished = 0;

static PreviousInstallationInfo gPrevInstall;
static Flags gCliNew;

// the three steps a static-exe install takes: copy the exe, write the registry
// entries, done
constexpr int kInstallationSteps = 3;

struct InstallerWnd {
    gp::App* app = nullptr;
    gp::Window* win = nullptr;
    gp::InputState* editInstallationDir = nullptr;
    bool forAllUsers = false;
    // orig's checkboxDesktopShortcut
    bool desktopShortcut = true;
    bool showOptions = false;
    bool installing = false;
    bool finished = false;
    int currProgress = 0;
    ThreadHandle hThread = nullptr;
    // tests/installer-desktop-shortcut.ts reads these. The window has no
    // Button children, so the probe hwnd carries the same rows.
    gp::Bounds optionsBounds;
    gp::Bounds allUsersBounds;
    gp::Bounds desktopBounds;
};

static bool HasPreviousInstall() {
    bool hasPrev = (gPrevInstall.typ != PreviousInstallationType::None);
    logf("HasPreviousInstall(): hasPrev: %d\n", hasPrev);
    return hasPrev;
}

static void ProgressStep() {
    if (!gWnd) {
        // when extracting with -x we don't create window
        return;
    }
    gWnd->currProgress++;
}

constexpr const char* kLogFileName = "sumatra-install-log.txt";

// caller has to free()
Str GetInstallerLogPath() {
    TempStr dir = GetTempDirTemp();
    if (len(dir) == 0) {
        return str::Dup(Str(kLogFileName));
    }
    return path::Join(dir, Str(kLogFileName));
}

static void ClearReadOnly(Str path) {
    DWORD attrs = file::GetAttributes(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
        logf("  clearing READONLY on '%s'\n", path);
        file::SetAttributes(path, attrs & ~FILE_ATTRIBUTE_READONLY);
    }
}

static bool IsDiskFullError(DWORD err) {
    return err == ERROR_DISK_FULL || err == ERROR_HANDLE_DISK_FULL;
}

// Copy the running installer to installDir\SumatraPDF.exe. Retries and uses
// temp+rename: a single CopyFileW often fails with ACCESS_DENIED (AV /
// Controlled Folder Access) or a sharing race after TerminateProcess of the
// previous instance.
static bool CopySelfToDir(Str destDir) {
    logf("CopySelfToDir(%s)\n", destDir);
    TempStr exePath = GetSelfExePathTemp();
    TempStr dstPath = path::JoinTemp(destDir, Str(kExeName));
    TempStr tmpPath = str::JoinTemp(dstPath, StrL(".tmp"));
    DWORD lastErr = 0;

    auto tryDirectCopy = [&]() -> bool {
        ClearReadOnly(dstPath);
        BOOL ok = CopyFileW(CWStrTemp(exePath), CWStrTemp(dstPath), FALSE);
        if (!ok) {
            lastErr = GetLastError();
            logf("  CopyFileW('%s' -> '%s') failed lastError=%u\n", exePath, dstPath, lastErr);
            LogLastError(lastErr);
            return false;
        }
        return true;
    };

    auto tryTempCopyRename = [&]() -> bool {
        ClearReadOnly(tmpPath);
        file::Delete(tmpPath);
        BOOL ok = CopyFileW(CWStrTemp(exePath), CWStrTemp(tmpPath), FALSE);
        if (!ok) {
            lastErr = GetLastError();
            logf("  CopyFileW('%s' -> '%s') failed lastError=%u\n", exePath, tmpPath, lastErr);
            LogLastError(lastErr);
            return false;
        }
        ClearReadOnly(dstPath);
        if (!MoveFileExW(CWStrTemp(tmpPath), CWStrTemp(dstPath), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            lastErr = GetLastError();
            logf("  MoveFileExW('%s' -> '%s') failed lastError=%u\n", tmpPath, dstPath, lastErr);
            LogLastError(lastErr);
            file::Delete(tmpPath);
            return false;
        }
        return true;
    };

    for (int attempt = 1; attempt <= 4; attempt++) {
        logf("  attempt %d/4\n", attempt);
        if (tryDirectCopy() || tryTempCopyRename()) {
            // strip zone identifier (if exists) to avoid windows
            // complaining when launching the file
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/1782
            file::DeleteZoneIdentifier(dstPath);
            logf("  copied '%s' to '%s'\n", exePath, dstPath);
            return true;
        }
        int killed = KillProcessesWithModule(dstPath, true);
        logf("  KillProcessesWithModule('%s') killed=%d\n", dstPath, killed);
        if (file::Exists(dstPath)) {
            ClearReadOnly(dstPath);
            bool delOk = file::Delete(dstPath);
            logf("  Delete('%s') => %d\n", dstPath, (int)delOk);
            if (!delOk) {
                LogLastError();
            }
        }
        file::Delete(tmpPath);
        if (attempt < 4) {
            DWORD sleepMs = 300u * (DWORD)attempt;
            logf("  sleep %u ms before retry\n", sleepMs);
            Sleep(sleepMs);
        }
    }

    logf("  failed to copy '%s' to '%s' lastError=%u\n", exePath, dstPath, lastErr);
    if (lastErr == ERROR_ACCESS_DENIED) {
        NotifyFailed(
            Tr("Couldn't copy SumatraPDF.exe to the installation directory (access denied). "
               "Temporarily disable antivirus or Controlled Folder Access for this folder, "
               "run the installer as administrator, or choose a different install folder. "
               "See https://www.sumatrapdfreader.org/docs/Installation"));
    } else if (lastErr == ERROR_SHARING_VIOLATION || lastErr == ERROR_LOCK_VIOLATION) {
        NotifyFailed(
            Tr("Couldn't copy SumatraPDF.exe to the installation directory (file in use). "
               "Close all SumatraPDF windows and Explorer PDF previews, then try again. "
               "See https://www.sumatrapdfreader.org/docs/Installation"));
    } else if (IsDiskFullError(lastErr)) {
        NotifyFailed(
            Tr("Not enough free disk space to copy SumatraPDF.exe to the installation directory.\n\n"
               "Free up space on this drive and try again."));
    } else {
        NotifyFailed(Tr("Couldn't copy SumatraPDF.exe to the installation directory"));
    }
    return false;
}

// ng: orig unpacks the lzsa payload here; a static exe only has itself
bool ExtractInstallerFiles(Str dir) {
    logf("ExtractInstallerFiles() to '%s'\n", dir);
    bool ok = dir::CreateAll(dir);
    if (!ok) {
        log(StrL("  dir::CreateAll() failed\n"));
        LogLastError();
        NotifyFailed(Tr("Couldn't create the installation directory"));
        return false;
    }
    TempStr selfDir = GetSelfExeDirTemp();
    if (path::IsSame(selfDir, dir)) {
        log(StrL("ExtractInstallerFiles: dest is this exe's directory, not copying SumatraPDF.exe\n"));
        ProgressStep();
        return true;
    }
    if (!CopySelfToDir(dir)) {
        // NotifyFailed already called inside CopySelfToDir with a specific reason.
        return false;
    }
    ProgressStep();
    return true;
}

static void CopySettingsFile() {
    log(StrL("CopySettingsFile()\n"));
    // Settings moved from %APPDATA% to %LOCALAPPDATA% in 3.2; copy from the old location on upgrade.

    // seen a crash when running elevated
    TempStr srcDir = GetSpecialFolderTemp(CSIDL_APPDATA, false);
    if (len(srcDir) == 0) {
        return;
    }
    TempStr dstDir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (len(dstDir) == 0) {
        return;
    }

    TempStr prefsFileName = GetSettingsFileNameTemp();
    TempStr srcPath = path::JoinTemp(srcDir, StrL(kAppName), prefsFileName);
    TempStr dstPath = path::JoinTemp(dstDir, StrL(kAppName), prefsFileName);

    // don't over-write
    bool failIfExists = true;
    // don't care if it fails or not
    file::Copy(dstPath, srcPath, failIfExists);
    logf("  copied '%s' to '%s'\n", srcPath, dstPath);
}

static bool CreateAppShortcut(int csidl, Str installedExePath) {
    TempStr shortcutPath = GetShortcutPathTemp(csidl);
    if (len(shortcutPath) == 0) {
        log(StrL("CreateAppShortcut() failed\n"));
        return false;
    }
    logf("CreateAppShortcut(csidl=%d), path=%s\n", csidl, shortcutPath);
    return CreateShortcut(shortcutPath, installedExePath);
}

// https://docs.microsoft.com/en-us/windows/win32/shell/csidl
// CSIDL_COMMON_DESKTOPDIRECTORY - files and folders on desktop for all users
// CSIDL_COMMON_PROGRAMS - Programs item in Start menu for all users
// CSIDL_DESKTOP - virtual folder, desktop for current user
// CSIDL_PROGRAMS - Programs item in Start menu for current user
static int shortcutDirs[] = {CSIDL_COMMON_DESKTOPDIRECTORY, CSIDL_COMMON_PROGRAMS, CSIDL_DESKTOP, CSIDL_PROGRAMS};

static void CreateAppShortcuts(bool forAllUsers, bool withDesktop, Str installedExePath) {
    logf("CreateAppShortcuts(forAllUsers=%d, withDesktop=%d)\n", (int)forAllUsers, (int)withDesktop);
    size_t start = forAllUsers ? 0 : 2;
    size_t end = forAllUsers ? 2 : dimof(shortcutDirs);
    for (size_t i = start; i < end; i++) {
        int csidl = shortcutDirs[i];
        bool isDesktop = csidl == CSIDL_COMMON_DESKTOPDIRECTORY || csidl == CSIDL_DESKTOP;
        if (isDesktop && !withDesktop) {
            continue;
        }
        CreateAppShortcut(csidl, installedExePath);
    }
}

static void RemoveShortcutFile(int csidl) {
    TempStr path = GetShortcutPathTemp(csidl);
    if (len(path) == 0 || !file::Exists(path)) {
        return;
    }
    file::Delete(path);
    logf("RemoveShortcutFile: deleted '%s'\n", path);
}

// those are shortcuts created by versions before 3.4
static int shortcutDirsPre34[] = {CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS, CSIDL_DESKTOP};

// those are shortcuts created by versions 3.4 through 3.6
static int shortcutDirs34To36[] = {CSIDL_COMMON_DESKTOPDIRECTORY, CSIDL_COMMON_STARTMENU, CSIDL_DESKTOP,
                                   CSIDL_STARTMENU};

void RemoveAppShortcuts() {
    for (int csidl : shortcutDirs) {
        RemoveShortcutFile(csidl);
    }
    for (int csidl : shortcutDirsPre34) {
        RemoveShortcutFile(csidl);
    }
    for (int csidl : shortcutDirs34To36) {
        RemoveShortcutFile(csidl);
    }
}

static Str GetEnvRegKey(bool allUsers) {
    if (allUsers) {
        return StrL(R"(SYSTEM\CurrentControlSet\Control\Session Manager\Environment)");
    }
    return StrL("Environment");
}

static void AddInstallDirToPath(bool allUsers, Str installDir) {
    HKEY root = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    Str keyName = GetEnvRegKey(allUsers);
    TempStr currPath = ReadRegStrTemp(root, keyName, StrL("Path"));
    // check if installDir is already in PATH (case-insensitive)
    if (currPath && IsDirInPath(currPath, installDir)) {
        logf("AddInstallDirToPath: '%s' already in PATH\n", installDir);
        return;
    }
    str::Builder newPath;
    if (len(currPath) > 0) {
        newPath.Append(currPath);
        if (newPath.LastChar() != ';') {
            newPath.Append(StrL(";"));
        }
    }
    newPath.Append(installDir);

    if (!WriteRegExpandSz(root, keyName, StrL("Path"), ToStr(newPath))) {
        return;
    }
    logf("AddInstallDirToPath: added '%s' to PATH\n", installDir);
    // notify other processes that environment has changed
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, nullptr);
}

static void InstallerThread(Flags* cli) {
    bool ok;

    gInstallFailed = true;

    TempStr installedExePath = path::JoinTemp(cli->installDir, Str(kExeName));
    auto allUsers = cli->allUsers;
    logf("InstallerThread: cli->allUsers: %d, installerExePath: '%s'\n", (int)cli->allUsers, installedExePath);
    HKEY key = cli->allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    if (!ExtractInstallerFiles(cli->installDir)) {
        log(StrL("ExtractInstallerFiles() failed\n"));
        goto Exit;
    }

    // for cleaner upgrades, remove registry entries and shortcuts from previous installations
    // doing it unconditionally, because deleting non-existing things doesn't hurt
    if (gPrevInstall.allUsers) {
        RemoveInstallRegistryKeys(HKEY_LOCAL_MACHINE);
        RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    }
    RemoveInstallRegistryKeys(HKEY_CURRENT_USER);
    RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);
    RemoveAppShortcuts();

    CopySettingsFile();

    CreateAppShortcuts(allUsers, !cli->noDesktopShortcut, installedExePath);

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gInstallFailed = false;

    ok = WriteUninstallerRegistryInfo(key, allUsers, cli->installDir);
    if (!ok) {
        NotifyFailed(Tr("Failed to write the uninstallation information to the registry"));
    }

    // remembered for the next upgrade (GetPreviousInstallInfo)
    LoggedWriteRegDWORD(key, GetRegPathUninstTemp(StrL(kAppName)), StrL(kRegDesktopShortcut),
                        cli->noDesktopShortcut ? 0 : 1);

    ok = WriteExtendedFileExtensionInfo(key, installedExePath);
    if (!ok) {
        NotifyFailed(Tr("Failed to write the extended file extension information to the registry"));
    }

    AddInstallDirToPath(allUsers, cli->installDir);

    ProgressStep();
    log(StrL("Installer thread finished\n"));
Exit:
    ProgressStep();
    InterlockedExchange(&gInstallFinished, 1);
}

static void RestartElevatedForAllUsers(Flags* cli) {
    TempStr exePath = GetSelfExePathTemp();
    TempStr cmdLine = StrL("-run-install-now");
    bool allUsersChecked = gWnd && gWnd->forAllUsers;
    bool allUsers = cli->allUsers || allUsersChecked;
    logf("RestartElevatedForAllUsers: cli->allUsers: %d, allUsersChecked: %d, allUsers: %d\n", (int)cli->allUsers,
         (int)allUsersChecked, (int)allUsers);
    if (allUsers) {
        cmdLine = str::JoinTemp(cmdLine, StrL(" -all-users"));
    }
    if (cli->noDesktopShortcut) {
        cmdLine = str::JoinTemp(cmdLine, StrL(" -no-desktop-shortcut"));
    }
    if (cli->silent) {
        cmdLine = str::JoinTemp(cmdLine, StrL(" -silent"));
    }
    if (cli->fastInstall) {
        cmdLine = str::JoinTemp(cmdLine, StrL(" -fast-install"));
    }
    if (cli->log) {
        cmdLine = str::JoinTemp(cmdLine, StrL(" -log"));
    }
    Str dir = cli->installDir;
    cmdLine = str::JoinTemp(cmdLine, StrL(" -install-dir \""), dir);
    cmdLine = str::JoinTemp(cmdLine, StrL("\""));
    logf("LaunchElevated('%s', '%s')\n", exePath, cmdLine);
    bool ok = LaunchElevated(exePath, cmdLine);
    if (!ok) {
        logf("LaunchElevated('%s', '%s') failed!\n", exePath, cmdLine);
        LogLastError();
    } else {
        logf("LaunchElevated() ok!\n");
        NoteTempInstallerRelaunch();
    }
}

// in pre-release the window is wider to accommodate bigger version number
int GetInstallerWinDx() {
    if (gIsPreReleaseBuild) {
        return 492;
    }
    return 420;
}

static TempStr GetDefaultInstallationDirTemp(bool forAllUsers, bool ignorePrev) {
    logf("GetDefaultInstallationDir(forAllUsers=%d, ignorePrev=%d)\n", (int)forAllUsers, (int)ignorePrev);

    Str dirPrevInstall = gPrevInstall.installationDir;

    if (dirPrevInstall && !ignorePrev) {
        logf("  using %s from previous install\n", dirPrevInstall);
        return dirPrevInstall;
    }

    if (forAllUsers) {
        TempStr dirAll = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES, false);
        TempStr dir = path::JoinTemp(dirAll, StrL(kAppName));
        logf("  using '%s' from GetSpecialFolderTemp(CSIDL_PROGRAM_FILES)\n", dir);
        return dir;
    }

    // %APPLOCALDATA%\SumatraPDF
    TempStr dirUser = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    TempStr dir = path::JoinTemp(dirUser, StrL(kAppName));
    logf("  using '%s' from GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA)\n", dir);
    return dir;
}

static TempStr GetInstalledExePathTemp(Flags* cli) {
    TempStr dir = cli->installDir;
    return path::JoinTemp(dir, Str(kExeName));
}

static Str InstallDirFromUiTemp() {
    if (!gWnd || !gWnd->editInstallationDir) {
        return gCliNew.installDir;
    }
    Str s = FromGpui(gp::InputValue(gWnd->editInstallationDir));
    if (len(s) == 0) {
        return gCliNew.installDir;
    }
    return str::DupTemp(s);
}

static void StartInstallation(InstallerWnd* wnd) {
    gInstallStarted = true;
    wnd->installing = true;
    wnd->currProgress = 0;
    // first one to show progress quickly
    ProgressStep();
    wnd->showOptions = false;

    SetMsg(Tr("Installation in progress..."), kColorMsgInstallation);

    auto fn = MkFunc0(InstallerThread, &gCliNew);
    wnd->hThread = StartThread(fn, StrL("InstallerThread"));
}

static void OnButtonInstall(InstallerWnd* wnd) {
    // gInstallStarted is set in StartInstallation because we might not proceed here
    if (gInstallStarted) {
        // I've seen crashes where somehow "Install" button was pressed twice
        logf("OnButtonInstall: called but gInstallStarted is %d\n", (int)gInstallStarted);
        return;
    }

    Flags* cli = &gCliNew;

    {
        /* if the app is running, we have to kill it so that we can over-write the executable */
        TempStr exePath = GetInstalledExePathTemp(cli);
        KillProcessesWithModule(exePath, true);
    }

    logf("OnButtonInstall: before CheckInstallUninstallPossible()\n");
    if (!CheckInstallUninstallPossible()) {
        return;
    }

    TempStr userInstallDir = InstallDirFromUiTemp();
    if (len(userInstallDir) > 0) {
        str::ReplaceWithCopy(&cli->installDir, userInstallDir);
    }

    cli->allUsers = wnd->forAllUsers;
    cli->noDesktopShortcut = !wnd->desktopShortcut;

    // Program Files always needs machine-style install + elevation
    if (IsPathUnderProgramFiles(cli->installDir) && !cli->allUsers) {
        logf("OnButtonInstall: install dir under Program Files; forcing allUsers\n");
        cli->allUsers = true;
    }

    bool needsElevation = InstallNeedsElevation(cli->installDir, cli->allUsers || gPrevInstall.allUsers);
    logf("OnButtonInstall: needsElevation=%d elevated=%d allUsers=%d dir='%s'\n", (int)needsElevation,
         (int)IsProcessRunningElevated(), (int)cli->allUsers, cli->installDir);
    if (needsElevation && !IsProcessRunningElevated()) {
        RestartElevatedForAllUsers(cli);
        ScheduleDeleteTempInstaller();
        ::ExitProcess(0);
    }
    StartInstallation(wnd);
}

static void StartSumatra() {
    TempStr exePath = GetInstalledExePathTemp(&gCliNew);
    RunNonElevated(exePath);
}

static void ForAllUsersStateChanged(InstallerWnd* wnd) {
    Flags* cli = &gCliNew;
    bool forAllUsers = wnd->forAllUsers;
    logf("ForAllUsersStateChanged() to %d\n", (int)forAllUsers);
    cli->allUsers = forAllUsers;
    auto dir = GetDefaultInstallationDirTemp(cli->allUsers, true);
    str::ReplaceWithCopy(&cli->installDir, dir);
    if (wnd->editInstallationDir) {
        gp::InputSetValue(wnd->editInstallationDir, ToGpui(cli->installDir));
    }
    logf("ForAllUsersStateChanged: cli->allUsers: %d, cli->installDir: '%s'\n", (int)cli->allUsers, cli->installDir);
}

static void OnButtonBrowse(InstallerWnd* wnd) {
    TempStr installDir = str::DupTemp(InstallDirFromUiTemp());

    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir)) {
        installDir = path::GetDirTemp(installDir);
    }

    gp::PathPrompt opts;
    opts.files = false;
    opts.directories = true;
    opts.title = ToGpui(Tr("Select the folder where SumatraPDF should be installed:"));
    TempStr installPath = str::DupTemp(FromGpui(gp::PromptForPathTemp(wnd->win, opts)));
    if (len(installPath) == 0) {
        return;
    }

    // force paths that aren't entered manually to end in ...\SumatraPDF
    // to prevent unintended installations into e.g. %ProgramFiles% itself
    TempStr end = str::JoinTemp(StrL("\\"), StrL(kAppName));
    if (!str::EndsWithI(installPath, end)) {
        installPath = path::JoinTemp(installPath, StrL(kAppName));
    }
    if (wnd->editInstallationDir) {
        gp::InputSetValue(wnd->editInstallationDir, ToGpui(installPath));
    }
}

static void OnInstallationFinished(InstallerWnd* wnd, Flags* cli) {
    logf("OnInstallationFinished: cli->fastInstall: %d gInstallFailed: %d\n", (int)cli->fastInstall,
         (int)gInstallFailed);

    SafeCloseThreadHandle(&wnd->hThread);
    wnd->installing = false;
    wnd->finished = true;

    if (gInstallFailed) {
        gMsgError = gFirstError;
        SetMsg(Tr("Installation could not be completed."), kColorMsgFailed);
        return;
    }

    SetMsg(Tr("Thank you! SumatraPDF has been installed."), kColorMsgOk);
    gMsgError = gFirstError;

    if (cli->fastInstall) {
        StartSumatra();
        ScheduleDeleteTempInstaller();
        ::ExitProcess(0);
    }
}

// --- the gpui window -------------------------------------------------------

// orig draws the logo in Impact 40pt with per-letter rotation and a shadow.
struct InstallerView {
    static gp::El* Render(InstallerView* self, gp::Ctx* cx);
    static void OnTick(InstallerView* self, gp::Ctx* cx, const gp::TickEvent*);
    static void OnInstall(InstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnOptions(InstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnBrowse(InstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnAllUsers(InstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnDesktopShortcut(InstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnKeyDown(InstallerView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnExit(InstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnStartSumatra(InstallerView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<InstallerView> gInstallerView;

// one letter of the logo: the shadow behind and the letter itself, rotated
static gp::El* BuildLetter(gp::Ctx* cx, const LetterInfo& li, float fontSize) {
    char buf[2] = {li.c == ' ' ? 'X' : li.c, 0};
    gp::Str s = gp::StrDup(cx->a, gp::Str{buf, 1});
    gp::El* d = gp::Div(cx->a);
    if (li.c == ' ') {
        // keeps the layout stable while the reveal animation runs
        d->Opacity(0);
    }
    d->Child(gp::TextEl(cx->a, s)
                 ->Absolute()
                 ->Left(-3)
                 ->Top(4)
                 ->Font(fontSize)
                 ->FontFamily(GStrL("Impact"))
                 ->Weight(gp::FontWeight::Black)
                 ->Fg(ToGpui(li.colShadow)));
    d->Child(gp::TextEl(cx->a, s)
                 ->Font(fontSize)
                 ->FontFamily(GStrL("Impact"))
                 ->Weight(gp::FontWeight::Black)
                 ->Fg(ToGpui(li.col)));
    d->MarginT(li.dyOff);
    return d->Rotate(li.rotation / 360.f);
}

gp::El* BuildInstallerLogo(gp::Ctx* cx) {
    float fontSize = 40.f * 96.f / 72.f;
    gp::El* row = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->JustifyCenter()->ItemsStart();
    for (const LetterInfo& li : gLetters) {
        row->Child(BuildLetter(cx, li, fontSize));
    }
    TempStr ver = fmt("v%s", StrL(CURR_VERSION_STRA));
    row->Child(
        gp::Div(cx->a)->Absolute()->Right(8)->Top(2)->Rotate(0.125f)->Child(gp::TextEl(cx->a, GpuiDup(cx->a, ver))
                                                                                ->Font(16.f * 96.f / 72.f)
                                                                                ->Weight(gp::FontWeight::Black)
                                                                                ->Fg(ToGpui(kColWhite))));
    return row;
}

// the message under the logo, plus the error line orig draws below it
gp::El* BuildInstallerMessage(gp::Ctx* cx) {
    float maxW = (float)GetInstallerWinDx() - 2 * (float)kInstallerWinMargin;
    auto line = [&](Str s, Color col) {
        return gp::TextEl(cx->a, GpuiDup(cx->a, s))
            ->Font(16.f * 96.f / 72.f)
            ->Weight(gp::FontWeight::Bold)
            ->Fg(ToGpui(col))
            ->Wrap()
            ->W(maxW)
            ->TextCenter();
    };
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->ItemsCenter()->Gap(5);
    if (len(gMsg) > 0) {
        col->Child(line(gMsg, gMsgColor));
    }
    if (len(gMsgError) > 0) {
        col->Child(line(gMsgError, kColorMsgFailed));
    }
    return col;
}

// WM_APP + 44. tests/installer-desktop-shortcut.ts posts this at the frame
// to open the option rows. gpui uses WM_APP + 71 for its own notify.
constexpr UINT kInstallerToggleOptions = WM_APP + 44;
constexpr const WCHAR* kInstallerProbeClass = L"SUMATRA_INSTALLER_PROBE";

static HWND gInstallerHwnd = nullptr;
static HWND gProbe = nullptr;

static void AppendDipRect(str::Builder& out, gp::Bounds b) {
    float scale = 1.f;
    if (gWnd && gWnd->win) {
        gp::WinSize ws = gp::WindowSize(gWnd->win);
        if (ws.dipW > 0 && ws.pxW > 0) {
            scale = ws.dipW / (float)ws.pxW;
        }
    }
    POINT origin{0, 0};
    if (gInstallerHwnd) {
        ClientToScreen(gInstallerHwnd, &origin);
    }
    int x = origin.x + (int)(b.x / scale + 0.5f);
    int y = origin.y + (int)(b.y / scale + 0.5f);
    int dx = (int)(b.w / scale + 0.5f);
    int dy = (int)(b.h / scale + 0.5f);
    out.Append(fmt("%d,%d,%d,%d", x, y, dx, dy));
}

static TempStr StripAccelTemp(Str s) {
    str::Builder out;
    for (int i = 0; i < len(s); i++) {
        if (s.s[i] != '&') {
            out.AppendChar(s.s[i]);
        }
    }
    return ToStrTemp(out);
}

static void AppendCheck(str::Builder& out, bool checked, gp::Bounds b, Str label) {
    out.Append(checked ? StrL("|1|") : StrL("|0|"));
    AppendDipRect(out, b);
    out.Append(StrL("|"));
    out.Append(StripAccelTemp(label));
}

// One line the test reads with WM_GETTEXT:
// opt|x,y,dx,dy|N|checked|x,y,dx,dy|label|...
static void InstallerProbeRefresh() {
    if (!gProbe || !gWnd) {
        return;
    }
    str::Builder out;
    out.Append(StrL("opt|"));
    AppendDipRect(out, gWnd->optionsBounds);
    if (!gWnd->showOptions) {
        out.Append(StrL("|0"));
    } else {
        out.Append(StrL("|2"));
        AppendCheck(out, gWnd->forAllUsers, gWnd->allUsersBounds, Tr("Install for all users"));
        AppendCheck(out, gWnd->desktopShortcut, gWnd->desktopBounds, Tr("Install &desktop shortcut"));
    }
    SetWindowTextW(gProbe, ToWStrTemp(ToStrTemp(out)).s);
}

static LRESULT CALLBACK InstallerSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == kInstallerToggleOptions && gWnd) {
        gWnd->showOptions = !gWnd->showOptions;
        gp::AppInvalidate(gWnd->win);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static BOOL CALLBACK FindInstallerHwnd(HWND hwnd, LPARAM lp) {
    WCHAR cls[64]{};
    GetClassNameW(hwnd, cls, dimof(cls));
    if (!wstr::EqI(WStr(cls), WStrL(L"GpuiSystemMonitor"))) {
        return TRUE;
    }
    *(HWND*)lp = hwnd;
    return FALSE;
}

static void InstallerAttachProbe() {
    HWND hwnd = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), FindInstallerHwnd, (LPARAM)&hwnd);
    if (!hwnd) {
        return;
    }
    gInstallerHwnd = hwnd;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kInstallerProbeClass;
    RegisterClassExW(&wc);
    gProbe = CreateWindowExW(0, kInstallerProbeClass, L"", WS_CHILD, 0, 0, 0, 0, hwnd, nullptr, wc.hInstance, nullptr);
    SetWindowSubclass(hwnd, InstallerSubclass, 1, 0);
}

void InstallerView::OnTick(InstallerView*, gp::Ctx* cx, const gp::TickEvent*) {
    bool needsRedraw = IsRevealingLettersAnimRunning();
    AnimStep();
    if (gWnd && gWnd->installing && InterlockedCompareExchange(&gInstallFinished, 0, 1) == 1) {
        OnInstallationFinished(gWnd, &gCliNew);
        needsRedraw = true;
    }
    if (gWnd && gWnd->installing) {
        needsRedraw = true;
    }
    if (needsRedraw) {
        gp::Notify(cx);
    }
    InstallerProbeRefresh();
}

void InstallerView::OnInstall(InstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    OnButtonInstall(gWnd);
    gp::Notify(cx);
}

void InstallerView::OnOptions(InstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gWnd->showOptions = !gWnd->showOptions;
    gp::Notify(cx);
}

void InstallerView::OnBrowse(InstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    OnButtonBrowse(gWnd);
    gp::Notify(cx);
}

void InstallerView::OnAllUsers(InstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gWnd->forAllUsers = !gWnd->forAllUsers;
    ForAllUsersStateChanged(gWnd);
    gp::Notify(cx);
}

void InstallerView::OnDesktopShortcut(InstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gWnd->desktopShortcut = !gWnd->desktopShortcut;
    gp::Notify(cx);
}

// orig's window is a dialog (IsDialogMessage): Alt + letter, or the letter
// alone outside the edit, is a control's access key
void InstallerView::OnKeyDown(InstallerView*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    if (ev->alt) {
        DlgAccelOnAlt();
        gp::Notify(cx);
    }
    bool editFocused = cx->win && cx->win->input && cx->win->input->focused;
    if (!ev->shift && DlgAccelOnKey(cx, (int)ev->vk, ev->alt, ev->ctrl, editFocused)) {
        const_cast<gp::KeyEvent*>(ev)->propagate = false;
        gp::Notify(cx);
    }
}

void InstallerView::OnExit(InstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gp::AppQuit(cx->win);
}

void InstallerView::OnStartSumatra(InstallerView*, gp::Ctx* cx, const gp::ClickEvent*) {
    StartSumatra();
    gp::AppQuit(cx->win);
}

//[ ACCESSKEY_GROUP Installer
gp::El* InstallerView::Render(InstallerView*, gp::Ctx* cx) {
    InstallerWnd* wnd = gWnd;
    float margin = (float)kInstallerWinMargin;
    wnd->optionsBounds = {};
    wnd->allUsersBounds = {};
    wnd->desktopBounds = {};
    DlgAccelBeginFrame(cx->win);

    gp::El* root = gp::Div(cx->a)
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(gp::kFill)
                       ->Bg(ToGpui(kInstallerWinBgColor))
                       ->OnKeyDown(gp::Listen(cx, &InstallerView::OnKeyDown));
    root->Child(gp::Div(cx->a)->W(gp::kFill)->PadT(18)->Child(BuildInstallerLogo(cx)));
    root->Child(gp::Div(cx->a)->Flex1()->W(gp::kFill)->FlexCol()->JustifyCenter()->Pad(margin)->Child(
        BuildInstallerMessage(cx)));

    if (wnd->showOptions && !wnd->installing && !wnd->finished) {
        gp::El* opts = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Gap(2)->PadL(margin)->PadR(margin);
        opts->Child(DlgAccelText(cx, DlgAccelInput(cx, Tr("Install SumatraPDF in &folder:"), wnd->editInstallationDir))
                        ->Font(13)
                        ->Fg(ToGpui(kColBlack)));
        gp::El* dirRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(6);
        dirRow->Child(
            gp::Div(cx->a)->Flex1()->MinW(0)->Child(gpc::Input::New(cx, GStrL("inst-dir"), wnd->editInstallationDir)
                                                        ->WithSize(gp::UiSize::Small)
                                                        ->W(gp::kFill)
                                                        ->IntoEl()));
        dirRow->Child(DlgAccelEl(cx, gpc::Button::New(cx, GStrL("inst-browse"))->WithSize(gp::UiSize::Small),
                                 StrL("&..."), gp::ListenTo(gInstallerView, &InstallerView::OnBrowse)));
        opts->Child(dirRow);
        opts->Child(gp::Div(cx->a)->H(margin));
        opts->Child(gpc::Checkbox::New(cx, GStrL("inst-allusers"))
                        ->Label(ToGpui(Tr("Install for all users")))
                        ->Checked(wnd->forAllUsers)
                        ->OnClick(gp::ListenTo(gInstallerView, &InstallerView::OnAllUsers))
                        ->IntoEl()
                        ->BoundsOut(&wnd->allUsersBounds));
        opts->Child(DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("inst-desktop"))->Checked(wnd->desktopShortcut),
                               Tr("Install &desktop shortcut"),
                               gp::ListenTo(gInstallerView, &InstallerView::OnDesktopShortcut))
                        ->BoundsOut(&wnd->desktopBounds));
        root->Child(opts);
    }

    gp::El* bottom = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8)->Pad(margin);
    if (wnd->installing) {
        float pct = 100.f * (float)wnd->currProgress / (float)kInstallationSteps;
        bottom->Child(gpc::Progress::New(cx)->Value(pct)->W((float)GetInstallerWinDx() / 2)->IntoEl());
    } else if (!wnd->finished) {
        //[ ACCESSKEY_ALTERNATIVE // ideally, the same access key is used for both
        Str optsLabel = wnd->showOptions ? Tr("Hide &Options") : Tr("&Options");
        //] ACCESSKEY_ALTERNATIVE
        bottom->Child(DlgAccelEl(cx, gpc::Button::New(cx, GStrL("inst-options"))->WithSize(gp::UiSize::Small),
                                 optsLabel, gp::ListenTo(gInstallerView, &InstallerView::OnOptions))
                          ->BoundsOut(&wnd->optionsBounds));
    }
    bottom->Child(gp::Div(cx->a)->Flex1());
    if (wnd->finished) {
        if (!gInstallFailed && !gCliNew.fastInstall) {
            bottom->Child(gpc::Button::New(cx, GStrL("inst-start"))
                              ->Label(ToGpui(Tr("Start SumatraPDF")))
                              ->Primary()
                              ->WithSize(gp::UiSize::Small)
                              ->OnClick(gp::ListenTo(gInstallerView, &InstallerView::OnStartSumatra))
                              ->IntoEl());
        }
        bottom->Child(gpc::Button::New(cx, GStrL("inst-close"))
                          ->Label(ToGpui(Tr("Close")))
                          ->WithSize(gp::UiSize::Small)
                          ->OnClick(gp::ListenTo(gInstallerView, &InstallerView::OnExit))
                          ->IntoEl());
    } else if (!wnd->installing && !gCliNew.fastInstall) {
        bottom->Child(gpc::Button::New(cx, GStrL("inst-install"))
                          ->Label(ToGpui(Tr("Install SumatraPDF")))
                          ->Primary()
                          ->WithSize(gp::UiSize::Small)
                          ->OnClick(gp::ListenTo(gInstallerView, &InstallerView::OnInstall))
                          ->IntoEl());
    }
    root->Child(bottom);
    return root;
}
//] ACCESSKEY_GROUP Installer

static bool CreateInstallerWindow(Flags* cli) {
    gDefaultMsg = Tr("Thank you for choosing SumatraPDF!");

    gWnd = new InstallerWnd();
    gWnd->forAllUsers = cli->allUsers;
    gWnd->desktopShortcut = !cli->noDesktopShortcut;
    // show options if user chose non-defaults via cmd-line
    // or if previous install had them enabled
    gWnd->showOptions = cli->allUsers || cli->noDesktopShortcut;

    gp::App* app = gp::AppNew();
    gpc::Init(app);
    gWnd->app = app;
    gInstallerView = gp::EntityNew<InstallerView>(app);

    gWnd->editInstallationDir = new gp::InputState();
    gWnd->editInstallationDir->focus = gp::FocusHandleNew(app);
    gp::InputSetValue(gWnd->editInstallationDir, ToGpui(cli->installDir));

    TempStr title = fmt(Tr("SumatraPDF %s Installer").s, StrL(CURR_VERSION_STRA));
    int dx = GetInstallerWinDx();
    int dy = kInstallerWinDy;
    gWnd->win = gp::WindowOpenView(app, ToGpui(title), dx, dy, gInstallerView.id, gp::WinOpts{});
    if (!gWnd->win) {
        return false;
    }
    gp::WindowSetInterval(gWnd->win, 33, gp::ListenTo(gInstallerView, &InstallerView::OnTick));
    InstallerAttachProbe();
    SetDefaultMsg();
    RevealingLettersAnimStart();

    auto autoStartInstall = cli->runInstallNow || cli->fastInstall;
    if (autoStartInstall) {
        StartInstallation(gWnd);
    }
    return true;
}

int RunInstaller(Flags* cli) {
    gCli = cli;
    gLogRegistryCalls = true;
    trans::SetCurrentLangByCode(trans::DetectUserLang());

    Str installerLogPath;

    gCliNew.log = gCli->log;
    gCliNew.allUsers = gCli->allUsers;
    gCliNew.noDesktopShortcut = gCli->noDesktopShortcut;
    gCliNew.silent = gCli->silent;
    gCliNew.runInstallNow = gCli->runInstallNow;
    gCliNew.fastInstall = gCli->fastInstall;
    if (gCli->log) {
        installerLogPath = GetInstallerLogPath();
        bool removeLog = !gCli->runInstallNow;
        StartLogToFile(installerLogPath, removeLog);
    }
    logf("------------- Starting SumatraPDF installation\n");

    GetPreviousInstallInfo(&gPrevInstall);
    // with -run-install all values should be explicitly set
    // otherwise we inherit values from previous install
    if (HasPreviousInstall() && !gCli->runInstallNow) {
        logf("!gCli->runInstallNew so inheriting prev install state\n");
        if (!gCliNew.allUsers) {
            gCliNew.allUsers = gPrevInstall.allUsers;
        }
        // if not set explicitly, default to state from previous installation
        if (!gCliNew.noDesktopShortcut) {
            gCliNew.noDesktopShortcut = !gPrevInstall.desktopShortcut;
        }
    }

    gCliNew.installDir = str::Dup(gCli->installDir);
    if (len(gCliNew.installDir) == 0) {
        auto dir = GetDefaultInstallationDirTemp(gCliNew.allUsers, false);
        gCliNew.installDir = str::Dup(dir);
    }
    // Program Files installs must be all-users (and will elevate below)
    if (IsPathUnderProgramFiles(gCliNew.installDir) && !gCliNew.allUsers) {
        logf("RunInstaller: install dir under Program Files; forcing allUsers\n");
        gCliNew.allUsers = true;
    }
    logf("RunInstaller: '%s', installing into dir '%s'\n", GetSelfExePathTemp(), gCliNew.installDir);

    int ret = 0;

    if (gCli->justExtractFiles) {
        bool ok = ExtractInstallerFiles(gCliNew.installDir);
        log(StrL("Installer finished (-x)\n"));
        return ok ? 0 : 1;
    }

    // restart as admin if necessary. in non-silent mode it happens after clicking
    // Install button
    bool requiresSilentElevation = gCli->silent || gCli->fastInstall || gCli->runInstallNow;
    bool isElevated = IsProcessRunningElevated();
    logf("RunInstaller: requiresSilentElevation: %d, isElevated: %d\n", (int)requiresSilentElevation, (int)isElevated);
    if (requiresSilentElevation && !isElevated) {
        bool needsElevation = InstallNeedsElevation(gCliNew.installDir, gCliNew.allUsers || gPrevInstall.allUsers);
        logf("RunInstaller: needsElevation: %d (allUsers=%d prevAllUsers=%d underPF=%d)\n", (int)needsElevation,
             (int)gCliNew.allUsers, (int)gPrevInstall.allUsers, (int)IsPathUnderProgramFiles(gCliNew.installDir));
        if (needsElevation) {
            logf("Restarting as elevated: gCli->silent: %d, gCli->fastInstall: %d, gCli->allUsers: %d\n",
                 (int)gCli->silent, (int)gCli->fastInstall, (int)gCli->allUsers);
            RestartElevatedForAllUsers(&gCliNew);
            ScheduleDeleteTempInstaller();
            ::ExitProcess(0);
        }
    }

    logf("RunInstaller: gCliNew.silent: %d, gCliNew.allUsers: %d, gCliNew.runInstallNow: %d, gCliNew.fastInstall: %d\n",
         (int)gCliNew.silent, (int)gCliNew.allUsers, (int)gCliNew.runInstallNow, (int)gCliNew.fastInstall);

    if (gCli->silent) {
        gInstallStarted = true;
        InstallerThread(&gCliNew);
        ret = gInstallFailed ? 1 : 0;
    } else {
        log(StrL("Before CreateInstallerWindow()\n"));
        if (!CreateInstallerWindow(&gCliNew)) {
            log(StrL("CreateInstallerWindow() failed\n"));
            return 1;
        }
        ret = gp::AppRun(gWnd->app);
        logf("AppRun() returned %d\n", ret);
    }

    log(StrL("Installer finished\n"));
    if (installerLogPath && gInstallStarted) {
        RunNonElevated(installerLogPath);
    } else if (!gCli->silent && (ret != 0 || gInstallFailed)) {
        // if installation failed, save log to file and show it
        installerLogPath = GetInstallerLogPath();
        bool ok = WriteCurrentLogToFile(installerLogPath);
        if (ok) {
            LaunchFileIfExists(installerLogPath);
        }
    }
    return ret;
}

#else

int RunInstaller(Flags*) {
    log(StrL("-install is not supported on this platform\n"));
    return 1;
}

#endif
