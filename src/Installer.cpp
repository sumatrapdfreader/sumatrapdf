/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/FileUtil.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/WinUtil.h"
#include "utils/Timer.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/GdiPlusUtil.h"
#include "utils/ByteOrderDecoder.h"
#include "utils/LzmaSimpleArchive.h"
#include "utils/ThreadUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "resource.h"
#include "AppSettings.h"
#include "Settings.h"
#include "Flags.h"
#include "Version.h"
#include "Annotation.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"
#include "Installer.h"
#include "SumatraConfig.h"
#include "Translations.h"

#include "utils/Log.h"

constexpr int kInstallerWinMargin = 8;

struct InstallerWnd;

static InstallerWnd* gWnd = nullptr;
static lzma::SimpleArchive gArchive{};
static bool gInstallStarted = false; // a bit of a hack
static bool gInstallFailed = false;

static PreviousInstallationInfo gPrevInstall;
Flags gCliNew;

struct InstallerWnd {
    HWND hwnd = nullptr;

    HBRUSH hbrBackground = nullptr;
    Button* btnOptions = nullptr;
    Button* btnRunSumatra = nullptr;
    Static* staticInstDir = nullptr;
    Edit* editInstallationDir = nullptr;
    Button* btnBrowseDir = nullptr;
    Checkbox* checkboxForAllUsers = nullptr;
    Checkbox* checkboxRegisterSearchFilter = nullptr;
    Checkbox* checkboxRegisterPreview = nullptr;
    int currProgress = 0;
    Progress* progressBar = nullptr;
    Button* btnExit = nullptr;
    Button* btnInstall = nullptr;

    bool showOptions = false;
    HANDLE hThread = nullptr;
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
    if (gWnd->progressBar) {
        // possibly dangerous as is called on a thread
        gWnd->progressBar->SetCurrent(gWnd->currProgress);
    }
}

static Checkbox* CreateCheckbox(HWND hwndParent, const char* s, bool isChecked) {
    Checkbox::CreateArgs args;
    args.parent = hwndParent;
    args.text = s;
    args.initialState = isChecked ? Checkbox::State::Checked : Checkbox::State::Unchecked;

    Checkbox* w = new Checkbox();
    w->Create(args);
    return w;
}

constexpr const char* kLogFileName = "sumatra-install-log.txt";
// caller has to free()
char* GetInstallerLogPath() {
    TempStr dir = GetTempDirTemp();
    if (!dir) {
        return str::Dup(kLogFileName);
    }
    return path::Join(dir, kLogFileName);
}

static bool ExtractInstallerFiles(lzma::SimpleArchive* archive, const char* destDir) {
    logf("ExtractFiles(): dir '%s'\n", destDir);
    lzma::FileInfo* fi;
    u8* uncompressed;

    int nFiles = archive->filesCount;

    for (int i = 0; i < nFiles; i++) {
        fi = &archive->files[i];
        uncompressed = lzma::GetFileDataByIdx(archive, i, nullptr);

        if (!uncompressed) {
            NotifyFailed(
                _TRA("The installer has been corrupted. Please download it again.\nSorry for the inconvenience!"));
            return false;
        }
        TempStr filePath = path::JoinTemp(destDir, fi->name);

        ByteSlice d = {uncompressed, fi->uncompressedSize};
        bool ok = file::WriteFile(filePath, d);
        free(uncompressed);

        if (!ok) {
            TempStr msg = str::FormatTemp(_TRA("Couldn't write %s to disk"), filePath);
            NotifyFailed(msg);
            return false;
        }
        logf("  extracted '%s'\n", filePath);
        ProgressStep();
    }

    return true;
}

static bool CopySelfToDir(const char* destDir) {
    logf("CopySelfToDir(%s)\n", destDir);
    TempStr exePath = GetSelfExePathTemp();
    TempStr dstPath = path::JoinTemp(destDir, kExeName);
    bool failIfExists = false;
    bool ok = file::Copy(dstPath, exePath, failIfExists);
    // strip zone identifier (if exists) to avoid windows
    // complaining when launching the file
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1782
    file::DeleteZoneIdentifier(dstPath);
    if (!ok) {
        logf("  failed to copy '%s' to dir '%s'\n", exePath, destDir);
        return false;
    }
    logf("  copied '%s' to dir '%s'\n", exePath, destDir);
    return true;
}

static void CopySettingsFile() {
    log("CopySettingsFile()\n");
    // up to 3.1.2 we stored settings in %APPDATA%
    // after that we use %LOCALAPPDATA%
    // copy the settings from old directory

    // seen a crash when running elevated
    TempStr srcDir = GetSpecialFolderTemp(CSIDL_APPDATA, false);
    if (str::IsEmpty(srcDir)) {
        return;
    }
    TempStr dstDir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (str::IsEmpty(dstDir)) {
        return;
    }

    TempStr prefsFileName = GetSettingsFileNameTemp();
    TempStr srcPath = path::JoinTemp(srcDir, kAppName, prefsFileName);
    TempStr dstPath = path::JoinTemp(dstDir, kAppName, prefsFileName);

    // don't over-write
    bool failIfExists = true;
    // don't care if it fails or not
    file::Copy(dstPath, srcPath, failIfExists);
    logf("  copied '%s' to '%s'\n", srcPath, dstPath);
}

static bool CreateAppShortcut(int csidl, const char* installedExePath) {
    char* shortcutPath = GetShortcutPathTemp(csidl);
    if (!shortcutPath) {
        log("CreateAppShortcut() failed\n");
        return false;
    }
    logf("CreateAppShortcut(csidl=%d), path=%s\n", csidl, shortcutPath);
    return CreateShortcut(shortcutPath, installedExePath);
}

// https://docs.microsoft.com/en-us/windows/win32/shell/csidl
// CSIDL_COMMON_DESKTOPDIRECTORY - files and folders on desktop for all users. C:\Documents and Settings\All
// Users\Desktop
// CSIDL_COMMON_STARTMENU - Start menu for all users, C:\Documents and Settings\All Users\Start Menu
// CSIDL_DESKTOP - virtual folder, desktop for current user
// CSIDL_STARTMENU - Start menu for current user. Settings\username\Start Menu
static int shortcutDirs[] = {CSIDL_COMMON_DESKTOPDIRECTORY, CSIDL_COMMON_STARTMENU, CSIDL_DESKTOP, CSIDL_STARTMENU};

static void CreateAppShortcuts(bool forAllUsers, const char* installedExePath) {
    logf("CreateAppShortcuts(forAllUsers=%d)\n", (int)forAllUsers);
    size_t start = forAllUsers ? 0 : 2;
    size_t end = forAllUsers ? 2 : dimof(shortcutDirs);
    for (size_t i = start; i < end; i++) {
        int csidl = shortcutDirs[i];
        CreateAppShortcut(csidl, installedExePath);
    }
}

static void RemoveShortcutFile(int csidl) {
    char* path = GetShortcutPathTemp(csidl);
    if (!path || !file::Exists(path)) {
        return;
    }
    file::Delete(path);
    logf("RemoveShortcutFile: deleted '%s'\n", path);
}

// those are shortcuts created by versions before 3.4
static int shortcutDirsPre34[] = {CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS, CSIDL_DESKTOP};

void RemoveAppShortcuts() {
    for (int csidl : shortcutDirs) {
        RemoveShortcutFile(csidl);
    }
    for (int csidl : shortcutDirsPre34) {
        RemoveShortcutFile(csidl);
    }
}

static void InstallerThread(Flags* cli) {
    bool ok;

    gInstallFailed = true;

    TempStr installedExePath = path::JoinTemp(cli->installDir, kExeName);
    auto allUsers = cli->allUsers;
    logf("InstallerThread: cli->allUsers: %d, cli->withFilter: %d, cli->withPreview: %d, installerExePath: '%s'\n",
         (int)cli->allUsers, (int)cli->withFilter, (int)cli->withPreview, installedExePath);
    HKEY key = cli->allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    if (!ExtractInstallerFiles(cli->installDir)) {
        log("ExtractInstallerFiles() failed\n");
        goto Exit;
    }

    // for cleaner upgrades, remove registry entries and shortcuts from previous installations
    // doing it unconditionally, because deleting non-existing things doesn't hurt
    UninstallBrowserPlugin();
    UninstallPreviewDll();
    UninstallSearchFilter();
    if (gPrevInstall.allUsers) {
        RemoveInstallRegistryKeys(HKEY_LOCAL_MACHINE);
        RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    }
    RemoveInstallRegistryKeys(HKEY_CURRENT_USER);
    RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER);
    RemoveAppShortcuts();

    CopySettingsFile();

    // mark them as uninstalled
    gPrevInstall.searchFilterInstalled = false;
    gPrevInstall.previewInstalled = false;

    if (cli->withFilter) {
        RegisterSearchFilter(allUsers);
    }

    if (cli->withPreview) {
        RegisterPreviewer(allUsers);
    }

    CreateAppShortcuts(allUsers, installedExePath);

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gInstallFailed = false;

    ok = WriteUninstallerRegistryInfo(key, allUsers, cli->installDir);
    if (!ok) {
        NotifyFailed(_TRA("Failed to write the uninstallation information to the registry"));
    }

    ok = WriteExtendedFileExtensionInfo(key, installedExePath);
    if (!ok) {
        NotifyFailed(_TRA("Failed to write the extended file extension information to the registry"));
    }

    ProgressStep();
    log("Installer thread finished\n");
Exit:
    if (gWnd && gWnd->hwnd) {
        if (!gCli->silent) {
            Sleep(500); // allow a glimpse of the completed progress bar before hiding it
            PostMessageW(gWnd->hwnd, WM_APP_INSTALLATION_FINISHED, 0, 0);
        }
    }
}

static void RestartElevatedForAllUsers(Flags* cli) {
    char* exePath = GetSelfExePathTemp();
    const char* cmdLine = "-run-install-now";
    bool allUsersChecked = gWnd && gWnd->checkboxForAllUsers && gWnd->checkboxForAllUsers->IsChecked();
    bool allUsers = cli->allUsers || allUsersChecked;
    logf("RestartElevatedForAllUsers: cli->allUsers: %d, allUsersChecked: %d, allUsers: %d\n", (int)cli->allUsers,
         (int)allUsersChecked, (int)allUsers);
    if (allUsers) {
        cmdLine = str::JoinTemp(cmdLine, " -all-users");
    }
    if (cli->withFilter) {
        cmdLine = str::JoinTemp(cmdLine, " -with-filter");
    }
    if (cli->withPreview) {
        cmdLine = str::JoinTemp(cmdLine, " -with-preview");
    }
    if (cli->silent) {
        cmdLine = str::JoinTemp(cmdLine, " -silent");
    }
    if (cli->fastInstall) {
        cmdLine = str::JoinTemp(cmdLine, " -fast-install");
    }
    if (cli->log) {
        cmdLine = str::JoinTemp(cmdLine, " -log");
    }
    char* dir = cli->installDir;
    cmdLine = str::JoinTemp(cmdLine, " -install-dir \"", dir);
    cmdLine = str::JoinTemp(cmdLine, "\"");
    logf("LaunchElevated('%s', '%s')\n", exePath, cmdLine);
    bool ok = LaunchElevated(exePath, cmdLine);
    if (!ok) {
        logf("LaunchElevated('%s', '%s') failed!\n", exePath, cmdLine);
        LogLastError();
    } else {
        logf("LaunchElevated() ok!\n");
    }
}

// in pre-release the window is wider to accommodate bigger version number
// TODO: instead of changing size of the window, change how we draw version number
int GetInstallerWinDx() {
    if (gIsPreReleaseBuild) {
        return 492;
    }
    return 420;
}

static void StartInstallation(InstallerWnd* wnd) {
    gInstallStarted = true;

    // create a progress bar in place of the Options button
    int dx = DpiScale(wnd->hwnd, GetInstallerWinDx() / 2);
    Rect rc(0, 0, dx, gButtonDy);
    rc = MapRectToWindow(rc, wnd->btnOptions->hwnd, wnd->hwnd);

    int nInstallationSteps = gArchive.filesCount;
    nInstallationSteps++; // for copying files to installation dir
    nInstallationSteps++; // for writing registry entries
    nInstallationSteps++; // to show progress at the beginning

    Progress::CreateArgs args;
    args.initialMax = nInstallationSteps;
    args.parent = wnd->hwnd;
    wnd->progressBar = new Progress();
    wnd->progressBar->Create(args);
    RECT prc = {rc.x, rc.y, rc.x + rc.dx, rc.y + rc.dy};
    wnd->progressBar->SetBounds(prc);
    // first one to show progress quickly
    ProgressStep();

    // disable the install button and remove all the installation options
    DeleteWnd(&wnd->staticInstDir);
    DeleteWnd(&wnd->editInstallationDir);
    DeleteWnd(&wnd->btnBrowseDir);
    DeleteWnd(&wnd->checkboxForAllUsers);
    DeleteWnd(&wnd->checkboxRegisterSearchFilter);
    DeleteWnd(&wnd->checkboxRegisterPreview);
    DeleteWnd(&wnd->btnOptions);

    SetMsg(_TRA("Installation in progress..."), COLOR_MSG_INSTALLATION);
    HwndRepaintNow(wnd->hwnd);

    auto fn = MkFunc0(InstallerThread, &gCliNew);
    wnd->hThread = StartThread(fn, "InstallerThread");
}

static void OnButtonOptions(InstallerWnd* wnd);

static TempStr GetInstalledExePathTemp(Flags* cli) {
    TempStr dir = cli->installDir;
    return path::JoinTemp(dir, kExeName);
}

static void OnButtonInstall(InstallerWnd* wnd) {
    // gInstallStarted is set in StartInstallation because we might not proceed here
    if (gInstallStarted) {
        // I've seen crashes where somehow "Install" button was pressed twice
        logf("OnButtonInstall: called but gInstallStarted is %d\n", (int)gInstallStarted);
        // ReportIfQuick(gInstallStarted);
        return;
    }

    Flags* cli = &gCliNew;
    if (wnd->showOptions) {
        // hide and disable "Options" button during installation
        OnButtonOptions(wnd);
    }
    wnd->btnInstall->SetIsEnabled(false);

    // TODO: if needs elevation, this might not have enough prermissions
    {
        /* if the app is running, we have to kill it so that we can over-write the executable */
        char* exePath = GetInstalledExePathTemp(cli);
        KillProcessesWithModule(exePath, true);
    }

    logf("OnButtonInstall: before CheckInstallUninstallPossible()\n");
    if (!CheckInstallUninstallPossible(wnd->hwnd)) {
        wnd->btnInstall->SetIsEnabled(true);
        return;
    }

    logf("OnButtonInstall: after CheckInstallUninstallPossible()\n");
    logf("OnButtonInstall: wnd: 0x%p\n", wnd);
    logf("OnButtonInstall: wnd->editInstallationDir: 0x%p\n", wnd->editInstallationDir);

    char* userInstallDir = HwndGetTextTemp(wnd->editInstallationDir->hwnd);
    if (!str::IsEmpty(userInstallDir)) {
        str::ReplaceWithCopy(&cli->installDir, userInstallDir);
    }

    cli->allUsers = wnd->checkboxForAllUsers->IsChecked();
    // note: this checkbox isn't created when running inside Wow64
    cli->withFilter = wnd->checkboxRegisterSearchFilter && wnd->checkboxRegisterSearchFilter->IsChecked();
    // note: this checkbox isn't created on Windows 2000 and XP
    cli->withPreview = wnd->checkboxRegisterPreview && wnd->checkboxRegisterPreview->IsChecked();

    bool needsElevation = cli->allUsers || gPrevInstall.allUsers;
    if (needsElevation && !IsProcessRunningElevated()) {
        RestartElevatedForAllUsers(cli);
        ::ExitProcess(0);
    }
    StartInstallation(wnd);
}

static void OnButtonExit() {
    if (gWnd) {
        SendMessageW(gWnd->hwnd, WM_CLOSE, 0, 0);
    } else {
        log("OnButtonExit: gWnd is null\n");
    }
}

static void StartSumatra() {
    TempStr exePath = GetInstalledExePathTemp(&gCliNew);
    RunNonElevated(exePath);
}

static void OnButtonStartSumatra() {
    StartSumatra();
    OnButtonExit();
}

static void OnInstallationFinished(Flags* cli) {
    logf("OnInstallationFinished: cli->fastInstall: %d\n", (int)cli->fastInstall);

    if (gWnd->btnRunSumatra) {
        HwndSetFocus(gWnd->btnRunSumatra->hwnd);
    }
    if (gWnd->btnExit) {
        HwndSetFocus(gWnd->btnExit->hwnd);
    }
    SetForegroundWindow(gWnd->hwnd);

    DeleteWnd(&gWnd->btnInstall);
    DeleteWnd(&gWnd->progressBar);

    if (gInstallFailed) {
        gWnd->btnExit = CreateDefaultButton(gWnd->hwnd, _TRA("Close"));
        gWnd->btnExit->onClick = MkFunc0Void(OnButtonExit);
        SetMsg(_TRA("Installation failed!"), COLOR_MSG_FAILED);
    } else {
        gWnd->btnRunSumatra = CreateDefaultButton(gWnd->hwnd, _TRA("Start SumatraPDF"));
        gWnd->btnRunSumatra->onClick = MkFunc0Void(OnButtonStartSumatra);
        SetMsg(_TRA("Thank you! SumatraPDF has been installed."), COLOR_MSG_OK);
    }
    gMsgError = gFirstError;
    HwndRepaintNow(gWnd->hwnd);

    CloseHandle(gWnd->hThread);

    if (cli->fastInstall && !gInstallFailed) {
        StartSumatra();
        ::ExitProcess(0);
    }
}

static void ShowAndEnable(Wnd* w, bool enable) {
    if (w) {
        HwndSetVisibility(w->hwnd, enable);
        w->SetIsEnabled(enable);
    }
}

static Size SetButtonTextAndResize(Button* b, const char* s) {
    b->SetText(s);
    Size size = b->GetIdealSize();
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(b->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}

static TempStr GetDefaultInstallationDirTemp(bool forAllUsers, bool ignorePrev) {
    logf("GetDefaultInstallationDir(forAllUsers=%d, ignorePrev=%d)\n", (int)forAllUsers, (int)ignorePrev);

    char* dir;
    char* dirPrevInstall = gPrevInstall.installationDir;

    if (dirPrevInstall && !ignorePrev) {
        logf("  using %s from previous install\n", dirPrevInstall);
        return (TempStr)dirPrevInstall;
    }

    if (forAllUsers) {
        TempStr dirAll = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES, false);
        dir = path::JoinTemp(dirAll, kAppName);
        logf("  using '%s' from GetSpecialFolderTemp(CSIDL_PROGRAM_FILES)\n", dir);
        return dir;
    }

    // %APPLOCALDATA%\SumatraPDF
    TempStr dirUser = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    dir = path::JoinTemp(dirUser, kAppName);
    logf("  using '%s' from GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA)\n", dir);
    return dir;
}

static void SetInstallButtonElevationState() {
    bool forAllUsers = gWnd->checkboxForAllUsers->IsChecked();
    bool mustElevate = forAllUsers || gPrevInstall.allUsers;
    Button_SetElevationRequiredState(gWnd->btnInstall->hwnd, mustElevate);
}

static void ForAllUsersStateChanged() {
    Flags* cli = &gCliNew;
    bool forAllUsers = gWnd->checkboxForAllUsers->IsChecked();
    bool mustElevate = forAllUsers || gPrevInstall.allUsers;
    logf("ForAllUsersStateChanged() to %d\n", (int)forAllUsers);
    SetInstallButtonElevationState();
    cli->allUsers = forAllUsers;
    auto dir = GetDefaultInstallationDirTemp(cli->allUsers, true);
    str::ReplacePtr(&cli->installDir, str::Dup(dir));
    gWnd->editInstallationDir->SetText(cli->installDir);
    logf("ForAllUsersStateChanged: cli->allUsers: %d, cli->installDir: '%s', forAllUsers: %d\n", (int)cli->allUsers,
         cli->installDir),
        (int)forAllUsers;
}

static void UpdateUIForOptionsState(InstallerWnd* wnd) {
    bool showOpts = wnd->showOptions;

    ShowAndEnable(wnd->staticInstDir, showOpts);
    ShowAndEnable(wnd->editInstallationDir, showOpts);
    ShowAndEnable(wnd->btnBrowseDir, showOpts);

    ShowAndEnable(wnd->checkboxForAllUsers, showOpts);
    ShowAndEnable(wnd->checkboxRegisterSearchFilter, showOpts);
    ShowAndEnable(wnd->checkboxRegisterPreview, showOpts);

    auto btnOptions = wnd->btnOptions;
    //[ ACCESSKEY_GROUP Installer
    //[ ACCESSKEY_ALTERNATIVE // ideally, the same access key is used for both
    auto s = _TRA("&Options");
    if (showOpts) {
        //| ACCESSKEY_ALTERNATIVE
        s = _TRA("Hide &Options");
    }
    SetButtonTextAndResize(btnOptions, s);
    //] ACCESSKEY_ALTERNATIVE
    //] ACCESSKEY_GROUP Installer

    HwndRepaintNow(wnd->hwnd);
    HwndSetFocus(btnOptions->hwnd);
}

static void OnButtonOptions(InstallerWnd* wnd) {
    // toggle options ui
    wnd->showOptions = !wnd->showOptions;
    UpdateUIForOptionsState(wnd);
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT msg, LPARAM lp, LPARAM lpData) {
    switch (msg) {
        case BFFM_INITIALIZED:
            if (!str::IsEmpty((WCHAR*)lpData)) {
                SendMessageW(hwnd, BFFM_SETSELECTION, TRUE, lpData);
            }
            break;

        // disable the OK button for non-filesystem and inaccessible folders (and shortcuts to folders)
        case BFFM_SELCHANGED: {
            WCHAR path[MAX_PATH];
            if (SHGetPathFromIDList((LPITEMIDLIST)lp, path) && dir::Exists(path)) {
                SHFILEINFO sfi{};
                SHGetFileInfo((LPCWSTR)lp, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_ATTRIBUTES);
                if (!(sfi.dwAttributes & SFGAO_LINK)) {
                    break;
                }
            }
            EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
        } break;
    }

    return 0;
}

static TempStr BrowseForFolderTemp(HWND hwnd, const char* initialFolderA, const char* caption) {
    WCHAR* initialFolder = ToWStrTemp(initialFolderA);
    BROWSEINFO bi{};
    bi.hwndOwner = hwnd;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = ToWStrTemp(caption);
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM)initialFolder;

    LPITEMIDLIST pidlFolder = SHBrowseForFolder(&bi);
    if (!pidlFolder) {
        return {};
    }
    WCHAR buf[MAX_PATH];
    BOOL ok = SHGetPathFromIDListW(pidlFolder, buf);
    if (!ok) {
        return {};
    }
    IMalloc* pMalloc = nullptr;
    HRESULT hr = SHGetMalloc(&pMalloc);
    if (SUCCEEDED(hr) && pMalloc) {
        pMalloc->Free(pidlFolder);
        pMalloc->Release();
    }
    return ToUtf8Temp(buf);
}

static void OnButtonBrowse(InstallerWnd* wnd) {
    auto editDir = wnd->editInstallationDir;
    char* installDir = HwndGetTextTemp(editDir->hwnd);

    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir)) {
        installDir = path::GetDirTemp(installDir);
    }

    auto caption = _TRA("Select the folder where SumatraPDF should be installed:");
    char* installPath = BrowseForFolderTemp(wnd->hwnd, installDir, caption);
    if (!installPath) {
        HwndSetFocus(wnd->btnBrowseDir->hwnd);
        return;
    }

    // force paths that aren't entered manually to end in ...\SumatraPDF
    // to prevent unintended installations into e.g. %ProgramFiles% itself
    char* end = str::JoinTemp("\\", kAppName);
    if (!str::EndsWithI(installPath, end)) {
        installPath = path::JoinTemp(installPath, kAppName);
    }
    editDir->SetText(installPath);
    editDir->SetSelection(0, -1);
    HwndSetFocus(editDir->hwnd);
}

static bool InstallerOnWmCommand(WPARAM wp) {
    switch (LOWORD(wp)) {
        case IDCANCEL:
            OnButtonExit();
            break;

        default:
            return false;
    }
    return true;
}

static void SetTabOrder(HWND* hwnds, int nHwnds) {
    for (int i = 0; i < nHwnds; i++) {
        HWND hNew = hwnds[i];
        HWND hOld = hwnds[(i + 1) % nHwnds];
        SetWindowPos(hOld, hNew, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

//[ ACCESSKEY_GROUP Installer
static void CreateInstallerWindowControls(InstallerWnd* wnd, Flags* cli) {
    logf(
        "CreateInstallerWindowControls: cli->allUsers: %d, cli->withPreview: %d, cli->withFilter: %d, install dir: "
        "'%s'\n",
        (int)cli->allUsers, (int)cli->withPreview, (int)cli->withFilter, cli->installDir);
    // intelligently show options if user chose non-defaults
    // via cmd-line
    bool showOptions = false;

    HWND hwnd = wnd->hwnd;
    int margin = DpiScale(hwnd, kInstallerWinMargin);

    wnd->btnInstall = CreateDefaultButton(hwnd, _TRA("Install SumatraPDF"));
    auto b = wnd->btnInstall;
    b->onClick = MkFunc0(OnButtonInstall, wnd);
    {
        // button position: bottom-right
        HWND parent = ::GetParent(b->hwnd);
        Rect r = ClientRect(parent);
        Size size = b->GetIdealSize();
        int x = r.dx - size.dx - margin;
        int y = r.dy - size.dy - margin;
        b->SetBounds({x, y, size.dx, size.dy});
    }

    Rect r = ClientRect(hwnd);
    wnd->btnOptions = CreateDefaultButton(hwnd, _TRA("&Options"));
    b = wnd->btnOptions;
    b->onClick = MkFunc0(OnButtonOptions, wnd);
    int x = margin;
    int y;
    {
        auto size = b->GetIdealSize();
        y = r.dy - size.dy - margin;
        b->SetBounds({x, y, size.dx, size.dy});
        gButtonDy = size.dy;
        gBottomPartDy = gButtonDy + (margin * 2);
    }

    Size size = HwndMeasureText(hwnd, "Foo");
    int staticDy = size.dy + DpiScale(hwnd, 6);

    y = r.dy - gBottomPartDy;
    int dx = r.dx - (margin * 2) - DpiScale(hwnd, 2);

    x += DpiScale(hwnd, 2);

    // build options controls going from the bottom
    y -= (staticDy + margin);

    RECT rc;
    int checkDy;
    // only show this checkbox if the CPU arch of DLL and OS match
    // (assuming that the installer has the same CPU arch as its content!)
    if (IsProcessAndOsArchSame()) {
        // for Windows XP, this means only basic thumbnail support
        const char* s = _TRA("Let Windows show &previews of PDF documents");
        bool isChecked = cli->withPreview || IsPreviewInstalled();
        if (isChecked) {
            showOptions = true;
        }
        wnd->checkboxRegisterPreview = CreateCheckbox(hwnd, s, isChecked);
        checkDy = wnd->checkboxRegisterPreview->GetIdealSize().dy;

        rc = {x, y, x + dx, y + checkDy};
        wnd->checkboxRegisterPreview->SetPos(&rc);
        y -= checkDy;

        isChecked = cli->withFilter || IsSearchFilterInstalled();
        if (isChecked) {
            showOptions = true;
        }
        s = _TRA("Let Windows Desktop Search &search PDF documents");
        wnd->checkboxRegisterSearchFilter = CreateCheckbox(hwnd, s, isChecked);
        checkDy = wnd->checkboxRegisterSearchFilter->GetIdealSize().dy;
        rc = {x, y, x + dx, y + checkDy};
        wnd->checkboxRegisterSearchFilter->SetPos(&rc);
        y -= checkDy;
    }

    {
        const char* s = _TRA("Install for all users");
        bool isChecked = cli->allUsers;
        if (isChecked) {
            showOptions = true;
        }
        wnd->checkboxForAllUsers = CreateCheckbox(hwnd, s, isChecked);
        wnd->checkboxForAllUsers->onStateChanged = MkFunc0Void(ForAllUsersStateChanged);

        checkDy = wnd->checkboxRegisterPreview->GetIdealSize().dy;
        rc = {x, y, x + dx, y + checkDy};
        wnd->checkboxForAllUsers->SetPos(&rc);
        y -= checkDy;
    }

    // a bit more space between text box and checkboxes
    y -= (DpiScale(hwnd, 4) + margin);

    wnd->btnBrowseDir = CreateDefaultButton(hwnd, "&...");
    wnd->btnBrowseDir->onClick = MkFunc0(OnButtonBrowse, wnd);

    Size btnSize2 = wnd->btnBrowseDir->GetIdealSize();

    Edit::CreateArgs eargs;
    eargs.parent = hwnd;
    eargs.withBorder = true;
    wnd->editInstallationDir = new Edit();
    HWND ehwnd = wnd->editInstallationDir->Create(eargs);
    wnd->editInstallationDir->SetText(cli->installDir);

    int editDy = wnd->editInstallationDir->GetIdealSize().dy;

    int btnDx = editDy; // btnDx == btnDy
    x = r.dx - margin - btnDx;
    wnd->btnBrowseDir->SetBounds({x, y, btnDx, btnDx});

    x = margin;
    dx = r.dx - (2 * margin) - btnDx - DpiScale(hwnd, 4);

    rc = {x, y, x + dx, y + editDy};
    wnd->editInstallationDir->SetBounds(rc);

    y -= editDy;

    const char* s2 = _TRA("Install SumatraPDF in &folder:");
    rc = {x, y, x + r.dx, y + staticDy};

    Static::CreateArgs args;
    args.parent = hwnd;
    args.text = s2;
    wnd->staticInstDir = new Static();
    wnd->staticInstDir->Create(args);
    wnd->staticInstDir->SetBounds(rc);

    wnd->showOptions = showOptions;
    UpdateUIForOptionsState(wnd);

    HWND hwnds[8] = {};
    int nHwnds = 0;
    hwnds[nHwnds++] = wnd->btnInstall->hwnd;
    hwnds[nHwnds++] = wnd->editInstallationDir->hwnd;
    hwnds[nHwnds++] = wnd->btnBrowseDir->hwnd;
    hwnds[nHwnds++] = wnd->checkboxForAllUsers->hwnd;
    if (wnd->checkboxRegisterSearchFilter) {
        hwnds[nHwnds++] = wnd->checkboxRegisterSearchFilter->hwnd;
    }
    if (wnd->checkboxRegisterPreview) {
        hwnds[nHwnds++] = wnd->checkboxRegisterPreview->hwnd;
    }
    hwnds[nHwnds++] = wnd->btnOptions->hwnd;
    SetTabOrder(hwnds, nHwnds);

    SetInstallButtonElevationState();
    HwndSetFocus(wnd->btnInstall->hwnd);
}
//] ACCESSKEY_GROUP Installer

static LRESULT CALLBACK WndProcInstallerFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool handled;

    LRESULT res = 0;
    res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    switch (msg) {
        case WM_CTLCOLORSTATIC: {
            if (gWnd->hbrBackground == nullptr) {
                gWnd->hbrBackground = CreateSolidBrush(RGB(0xff, 0xf2, 0));
            }
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)gWnd->hbrBackground;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT: {
            OnPaintFrame(hwnd, gWnd->showOptions);
            break;
        }

        case WM_COMMAND: {
            handled = InstallerOnWmCommand(wp);
            if (!handled) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            break;
        }

        case WM_APP_START_INSTALLATION: {
            StartInstallation(gWnd);
            SetForegroundWindow(hwnd);
            break;
        }

        case WM_APP_INSTALLATION_FINISHED: {
            OnInstallationFinished(&gCliNew);
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }

    return 0;
}

#define kInstallerWindowClassName L"SUMATRA_PDF_INSTALLER_FRAME"

static bool CreateInstallerWnd(Flags* cli) {
    gWnd = new InstallerWnd();
    {
        WNDCLASSEX wcex{};

        FillWndClassEx(wcex, kInstallerWindowClassName, WndProcInstallerFrame);
        auto h = GetModuleHandleW(nullptr);
        WCHAR* resName = MAKEINTRESOURCEW(GetAppIconID());
        wcex.hIcon = LoadIconW(h, resName);

        ATOM atom = RegisterClassExW(&wcex);
        ReportIf(!atom);
        if (atom == 0) {
            logf("CreateInstallerWindow: RegisterClassExW() failed\n");
            return false;
        }
    }

    TempStr title = str::FormatTemp(_TRA("SumatraPDF %s Installer"), CURR_VERSION_STRA);
    DWORD exStyle = 0;
    if (trans::IsCurrLangRtl()) {
        exStyle = WS_EX_LAYOUTRTL;
    }
    const WCHAR* winCls = kInstallerWindowClassName;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = GetInstallerWinDx();
    int dy = kInstallerWinDy;
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    HMODULE h = GetModuleHandleW(nullptr);
    TempWStr titleW = ToWStrTemp(title);
    HWND hwnd = CreateWindowExW(exStyle, winCls, titleW, dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
    if (!hwnd) {
        return false;
    }
    gWnd->hwnd = hwnd;
    DpiScale(hwnd, dx, dy);
    HwndResizeClientSize(hwnd, dx, dy);
    CreateInstallerWindowControls(gWnd, cli);
    return true;
}

static bool CreateInstallerWindow(Flags* cli) {
    gDefaultMsg = _TRA("Thank you for choosing SumatraPDF!");
    if (!CreateInstallerWnd(cli)) {
        return false;
    }
    auto autoStartInstall = cli->runInstallNow || cli->fastInstall;
    // TODO: gHwndFrame is shared between installer and uninstaller windows
    gHwndFrame = gWnd->hwnd;
    if (autoStartInstall) {
        PostMessageW(gWnd->hwnd, WM_APP_START_INSTALLATION, 0, 0);
    }

    SetDefaultMsg();

    CenterDialog(gWnd->hwnd);
    ShowWindow(gWnd->hwnd, SW_SHOW);

    return true;
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
            if (!IsDialogMessage(gWnd->hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        // check if there are processes that need to be closed but
        // not more frequently than once per ten seconds and
        // only before (un)installation starts.
        auto dur = TimeSinceInMs(t);
        if (!gInstallStarted && dur > 10000) {
            CheckInstallUninstallPossible(gWnd->hwnd, true);
            t = TimeGet();
        }
    }
}

static void ShowNoEmbeddedFiles(const char* msg) {
    if (gCli->silent) {
        log(msg);
        return;
    }
    MsgBox(nullptr, msg, "Error", MB_OK);
}

static LoadedDataResource gLoadedArchive;

static bool OpenEmbeddedFilesArchive() {
    if (gArchive.filesCount > 0) {
        log("OpenEmbeddedFilesArchive: already opened\n");
        return true;
    }
    bool ok = LockDataResource(IDR_DLL_PAK, &gLoadedArchive);
    if (!ok) {
        ShowNoEmbeddedFiles("No embedded files");
        return false;
    }

    auto data = gLoadedArchive.data;
    auto size = gLoadedArchive.dataSize;
    ok = lzma::ParseSimpleArchive(data, (size_t)size, &gArchive);
    if (!ok) {
        ShowNoEmbeddedFiles("Embedded lzsa archive is corrupted");
        return false;
    }
    log("OpenEmbeddedFilesArchive: opened archive\n");
    return true;
}

u32 GetLibmupdfDllSize() {
    bool ok = OpenEmbeddedFilesArchive();
    if (!ok) {
        return 0;
    }
    auto archive = &gArchive;
    int nFiles = archive->filesCount;
    lzma::FileInfo* fi;
    for (int i = 0; i < nFiles; i++) {
        fi = &archive->files[i];
        if (!str::EqI(fi->name, "libmupdf.dll")) {
            continue;
        }
        return (u32)fi->uncompressedSize;
    }
    return 0;
}

bool ExtractInstallerFiles(char* dir) {
    logf("ExtractInstallerFiles() to '%s'\n", dir);
    bool ok = dir::CreateAll(dir);
    if (!ok) {
        log("  dir::CreateAll() failed\n");
        LogLastError();
        NotifyFailed(_TRA("Couldn't create the installation directory"));
        return false;
    }

    ok = CopySelfToDir(dir);
    if (!ok) {
        return false;
    }
    ProgressStep();

    ok = OpenEmbeddedFilesArchive();
    if (!ok) {
        return false;
    }
    // on error, ExtractFiles() shows error message itself
    return ExtractInstallerFiles(&gArchive, dir);
}

static bool MismatchedOSDialog(HWND hwndParent) {
    logf("Mismatch of the OS and executable arch\n");

    constexpr int kBtnIdContinue = 100;
    constexpr int kBtnIdDownload = 101;
    TASKDIALOGCONFIG dialogConfig{};
    TASKDIALOG_BUTTON buttons[2];

    buttons[0].nButtonID = kBtnIdDownload;
    const char* s = _TRA("Download 64-bit version");
    buttons[0].pszButtonText = ToWStrTemp(s);
    buttons[1].nButtonID = kBtnIdContinue;
    s = _TRA("&Continue installing 32-bit version");
    buttons[1].pszButtonText = ToWStrTemp(s);

    DWORD flags = TDF_SIZE_TO_CONTENT | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    s = _TRA("Installing 32-bit SumatraPDF on 64-bit OS");
    dialogConfig.pszWindowTitle = ToWStrTemp(s);
    // dialogConfig.pszMainInstruction = mainInstr;
    s = _TRA("You're installing 32-bit SumatraPDF on 64-bit OS.\nWould you like to download\n64-bit version?");
    dialogConfig.pszContent = ToWStrTemp(s);
    dialogConfig.nDefaultButton = kBtnIdContinue;
    dialogConfig.dwFlags = flags;
    dialogConfig.cxWidth = 0;
    dialogConfig.pfCallback = nullptr;
    dialogConfig.dwCommonButtons = 0;
    dialogConfig.cButtons = dimof(buttons);
    dialogConfig.pButtons = &buttons[0];
    dialogConfig.pszMainIcon = TD_INFORMATION_ICON;
    dialogConfig.hwndParent = hwndParent;

    int buttonPressedId = 0;

    auto hr = TaskDialogIndirect(&dialogConfig, &buttonPressedId, nullptr, nullptr);
    ReportIf(hr == E_INVALIDARG);
    if (buttonPressedId == kBtnIdDownload) {
        const char* url = "https://www.sumatrapdfreader.org/download-free-pdf-viewer";
        if (gIsPreReleaseBuild) {
            url = "https://www.sumatrapdfreader.org/prerelease";
        }
        LaunchBrowser(url);
        return true;
    }
    return false;
}

int RunInstaller() {
    trans::SetCurrentLangByCode(trans::DetectUserLang());

    const char* installerLogPath = nullptr;

    gCliNew.log = gCli->log;
    gCliNew.allUsers = gCli->allUsers;
    gCliNew.withFilter = gCli->withFilter;
    gCliNew.withPreview = gCli->withPreview;
    gCliNew.silent = gCli->silent;
    gCliNew.runInstallNow = gCli->runInstallNow;
    gCliNew.fastInstall = gCli->fastInstall;
    if (gCli->log) {
        installerLogPath = GetInstallerLogPath();
        bool removeLog = !gCli->runInstallNow;
        StartLogToFile(installerLogPath, removeLog);
    }
    logf("------------- Starting SumatraPDF installation\n");
    if (!IsProcessAndOsArchSame()) {
        logfa("quitting because !IsProcessAndOsArchSame()\n");
        MismatchedOSDialog(nullptr);
        RunNonElevated(installerLogPath);
        return 1;
    }
    if (!OpenEmbeddedFilesArchive()) {
        return 1;
    }

    GetPreviousInstallInfo(&gPrevInstall);
    // with -run-install all values should be explicitly set
    // otherwise we inherit values from previous install
    if (HasPreviousInstall() && !gCli->runInstallNow) {
        logf("!gCli->runInstallNew so inheriting prev install state\n");
        if (!gCliNew.allUsers) {
            gCliNew.allUsers = gPrevInstall.allUsers;
        }
        // if not set explicitly, default to state from previous installation
        if (!gCliNew.withFilter) {
            gCliNew.withFilter = gPrevInstall.searchFilterInstalled;
        }
        if (!gCliNew.withPreview) {
            gCliNew.withPreview = gPrevInstall.previewInstalled;
        }
    }

    gCliNew.installDir = str::Dup(gCli->installDir);
    if (!gCliNew.installDir) {
        auto dir = GetDefaultInstallationDirTemp(gCliNew.allUsers, false);
        gCliNew.installDir = str::Dup(dir);
    }
    char* cmdLine = ToUtf8Temp(GetCommandLineW());
    logf("Running'%s', cmdLine: '%s', installing into dir '%s'\n", GetSelfExePathTemp(), cmdLine, gCliNew.installDir);

    int ret = 0;

    // restart as admin if necessary. in non-silent mode it happens after clicking
    // Install button
    bool requiresSilentElevation = gCli->silent || gCli->fastInstall;
    bool isElevated = IsProcessRunningElevated();
    if (requiresSilentElevation && !isElevated) {
        bool needsElevation = gCliNew.allUsers || gPrevInstall.allUsers;
        if (needsElevation) {
            logf(
                "Restarting as elevated: gCli->silent: %d, gCli->fastInstall: %d, isElevated: %d, gCli->allUsers: %d, "
                "prevInstall.needsElevation: %d\n",
                (int)gCli->silent, (int)gCli->fastInstall, (int)isElevated, (int)gCli->allUsers,
                (int)gPrevInstall.allUsers);
            RestartElevatedForAllUsers(&gCliNew);
            ::ExitProcess(0);
        }
    }

    logf(
        "RunInstaller: gCliNew.silent: %d, gCliNew.allUsers: %d, gCliNew.runInstallNow: %d, gCliNew.withFilter: "
        "%d, "
        "gCliNew.withPreview: %d, gCliNew.fastInstall: %d\n",
        (int)gCliNew.silent, (int)gCliNew.allUsers, (int)gCliNew.runInstallNow, (int)gCliNew.withFilter,
        (int)gCliNew.withPreview, (int)gCliNew.fastInstall);

    // TODO: either tighten condition for doing it or remove
    // with prev install we might need to elevate first
    bool earlyUninstall = false;
    if (earlyUninstall) {
        // unregister search filter and previewer to reduce
        // possibility of blocking the installation because the dlls are loaded
        if (gPrevInstall.searchFilterInstalled) {
            UninstallSearchFilter();
            log("After UninstallSearchFilter\n");
        }
        if (gPrevInstall.previewInstalled) {
            UninstallPreviewDll();
            log("After UninstallPreviewDll\n");
        }
    }

    if (gCli->silent) {
        gInstallStarted = true;
        InstallerThread(&gCliNew);
        ret = gInstallFailed ? 1 : 0;
    } else {
        log("Before CreateInstallerWindow()\n");
        if (!CreateInstallerWindow(&gCliNew)) {
            log("CreateInstallerWindow() failed\n");
            goto Exit;
        }
        log("Before SetForegroundWindow()\n");
        SetForegroundWindow(gWnd->hwnd);
        log("Before RunApp()\n");
        ret = RunApp();
        logfa("RunApp() returned %d\n", ret);
    }

    if (earlyUninstall) {
        // re-register if we un-registered but installation was cancelled
        if (gPrevInstall.searchFilterInstalled) {
            log("re-registering search filter\n");
            RegisterSearchFilter(gCli->allUsers);
        }
        if (gPrevInstall.previewInstalled) {
            log("re-registering previewer\n");
            RegisterPreviewer(gCli->allUsers);
        }
    }
    log("Installer finished\n");
Exit:
    if (installerLogPath) {
        RunNonElevated(installerLogPath);
    } else if (!gCli->silent && (ret != 0)) {
        // if installation failed, automatically show the log
        installerLogPath = GetInstallerLogPath();
        bool ok = WriteCurrentLogToFile(installerLogPath);
        if (ok) {
            LaunchFileIfExists(installerLogPath);
        }
    }
#if 0 // technically a leak but there's no point
    str::Free(installerLogPath);
    str::Free(gFirstError);
#endif
    return ret;
}
