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

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "AppSettings.h"
#include "Settings.h"
#include "Flags.h"
#include "Version.h"
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
    bool failed = false;
    HANDLE hThread = nullptr;

    PreviousInstallationInfo prevInstall;
};

static void ProgressStep() {
    if (!gWnd) {
        // when extracing with -x we don't create window
        return;
    }
    gWnd->currProgress++;
    if (gWnd->progressBar) {
        // possibly dangerous as is called on a thread
        gWnd->progressBar->SetCurrent(gWnd->currProgress);
    }
}

static Checkbox* CreateCheckbox(HWND hwndParent, const WCHAR* s, bool isChecked) {
    CheckboxCreateArgs args;
    args.parent = hwndParent;
    if (s) {
        args.text = ToUtf8Temp(s);
    }
    args.initialState = isChecked ? CheckState::Checked : CheckState::Unchecked;

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

static bool ExtractFiles(lzma::SimpleArchive* archive, const char* destDir) {
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
    char* exePath = GetExePathTemp();
    char* dstPath = path::JoinTemp(destDir, kExeName);
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
    char* srcDir = GetSpecialFolderTemp(CSIDL_APPDATA, false);
    if (str::IsEmpty(srcDir)) {
        return;
    }
    char* dstDir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (str::IsEmpty(dstDir)) {
        return;
    }

    const char* prefsFileName = GetSettingsFileNameTemp();
    char* srcPath = path::JoinTemp(srcDir, kAppName, prefsFileName);
    char* dstPath = path::JoinTemp(dstDir, kAppName, prefsFileName);

    // don't over-write
    bool failIfExists = true;
    // don't care if it fails or not
    file::Copy(dstPath, srcPath, failIfExists);
    logf("  copied '%s' to '%s'\n", srcPath, dstPath);
}

static bool CreateAppShortcut(int csidl) {
    char* shortcutPath = GetShortcutPathTemp(csidl);
    if (!shortcutPath) {
        log("CreateAppShortcut() failed\n");
        return false;
    }
    logf("CreateAppShortcut(csidl=%d), path=%s\n", csidl, shortcutPath);
    char* installedExePath = GetInstalledExePathTemp();
    return CreateShortcut(shortcutPath, installedExePath);
}

// https://docs.microsoft.com/en-us/windows/win32/shell/csidl
// CSIDL_COMMON_DESKTOPDIRECTORY - files and folders on desktop for all users. C:\Documents and Settings\All
// Users\Desktop
// CSIDL_COMMON_STARTMENU - Start menu for all users, C:\Documents and Settings\All Users\Start Menu
// CSIDL_DESKTOP - virutal folder, desktop for current user
// CSIDL_STARTMENU - Start menu for current user. Settings\username\Start Menu
static int shortcutDirs[] = {CSIDL_COMMON_DESKTOPDIRECTORY, CSIDL_COMMON_STARTMENU, CSIDL_DESKTOP, CSIDL_STARTMENU};

static void CreateAppShortcuts(bool forAllUsers) {
    logf("CreateAppShortcuts(forAllUsers=%d)\n", (int)forAllUsers);
    size_t start = forAllUsers ? 0 : 2;
    size_t end = forAllUsers ? 2 : dimof(shortcutDirs);
    for (size_t i = start; i < end; i++) {
        int csidl = shortcutDirs[i];
        CreateAppShortcut(csidl);
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

static DWORD WINAPI InstallerThread(void*) {
    gWnd->failed = true;
    bool ok;

    bool allUsers = gCli->allUsers;
    HKEY key = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    if (!ExtractInstallerFiles(gCli->installDir)) {
        log("ExtractInstallerFiles() failed\n");
        goto Exit;
    }

    // for cleaner upgrades, remove registry entries and shortcuts from previous installations
    // doing it unconditionally, because deleting non-existing things doesn't hurt
    UninstallBrowserPlugin();
    UninstallPreviewDll();
    UninstallSearchFilter();
    RemoveInstallRegistryKeys(HKEY_LOCAL_MACHINE);
    RemoveInstallRegistryKeys(HKEY_CURRENT_USER);
    RemoveAppShortcuts();

    CopySettingsFile();

    // mark them as uninstalled
    gWnd->prevInstall.searchFilterInstalled = false;
    gWnd->prevInstall.previewInstalled = false;

    if (gCli->withFilter) {
        RegisterSearchFilter(allUsers);
    }

    if (gCli->withPreview) {
        RegisterPreviewer(allUsers);
    }

    CreateAppShortcuts(allUsers);

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gWnd->failed = false;

    ok = WriteUninstallerRegistryInfo(key, allUsers);
    if (!ok) {
        NotifyFailed(_TRA("Failed to write the uninstallation information to the registry"));
    }

    ok = WriteExtendedFileExtensionInfo(key);
    if (!ok) {
        NotifyFailed(_TRA("Failed to write the extended file extension information to the registry"));
    }

    ProgressStep();
    log("Installer thread finished\n");
Exit:
    if (gWnd->hwnd) {
        if (!gCli->silent) {
            Sleep(500); // allow a glimpse of the completed progress bar before hiding it
            PostMessageW(gWnd->hwnd, WM_APP_INSTALLATION_FINISHED, 0, 0);
        }
    }
    return 0;
}

static void RestartElevatedForAllUsers() {
    char* exePath = GetExePathTemp();
    const char* cmdLine = "-run-install-now";
    if (gWnd->checkboxForAllUsers->IsChecked()) {
        cmdLine = str::JoinTemp(cmdLine, " -all-users");
    }
    if (gCli->withFilter) {
        cmdLine = str::JoinTemp(cmdLine, " -with-filter");
    }
    if (gCli->withPreview) {
        cmdLine = str::JoinTemp(cmdLine, " -with-preview");
    }
    if (gCli->silent) {
        cmdLine = str::JoinTemp(cmdLine, " -silent");
    }
    if (gCli->log) {
        cmdLine = str::JoinTemp(cmdLine, " -log");
    }
    char* dir = gCli->installDir;
    cmdLine = str::JoinTemp(cmdLine, " -install-dir \"", dir);
    cmdLine = str::JoinTemp(cmdLine, "\"");
    logf("Re-launching '%s' as elevated, args\n%s\n", exePath, cmdLine);
    LaunchElevated(exePath, cmdLine);
}

// in pre-relase the window is wider to acommodate bigger version number
// TODO: instead of changing size of the window, change how we draw version number
int GetInstallerWinDx() {
    if (gIsPreReleaseBuild) {
        return 492;
    }
    return 420;
}

static void StartInstallation(InstallerWnd* wnd) {
    // create a progress bar in place of the Options button
    int dx = DpiScale(wnd->hwnd, GetInstallerWinDx() / 2);
    Rect rc(0, 0, dx, gButtonDy);
    rc = MapRectToWindow(rc, wnd->btnOptions->hwnd, wnd->hwnd);

    int nInstallationSteps = gArchive.filesCount;
    nInstallationSteps++; // for copying files to installation dir
    nInstallationSteps++; // for writing registry entries
    nInstallationSteps++; // to show progress at the beginning

    ProgressCreateArgs args;
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

    wnd->btnInstall->SetIsEnabled(false);

    SetMsg(_TR("Installation in progress..."), COLOR_MSG_INSTALLATION);
    HwndInvalidate(wnd->hwnd);

    gInstallStarted = true;
    wnd->hThread = CreateThread(nullptr, 0, InstallerThread, nullptr, 0, nullptr);
}

static void OnButtonOptions();

static void OnButtonInstall() {
    if (gWnd->showOptions) {
        // hide and disable "Options" button during installation
        OnButtonOptions();
    }

    {
        /* if the app is running, we have to kill it so that we can over-write the executable */
        char* exePath = GetInstalledExePathTemp();
        KillProcessesWithModule(exePath, true);
    }

    if (!CheckInstallUninstallPossible()) {
        return;
    }

    char* userInstallDir = HwndGetTextTemp(gWnd->editInstallationDir->hwnd);
    if (!str::IsEmpty(userInstallDir)) {
        str::ReplaceWithCopy(&gCli->installDir, userInstallDir);
    }

    // note: this checkbox isn't created when running inside Wow64
    gCli->withFilter = gWnd->checkboxRegisterSearchFilter && gWnd->checkboxRegisterSearchFilter->IsChecked();
    // note: this checkbox isn't created on Windows 2000 and XP
    gCli->withPreview = gWnd->checkboxRegisterPreview && gWnd->checkboxRegisterPreview->IsChecked();
    gCli->allUsers = gWnd->checkboxForAllUsers->IsChecked();

    bool needsElevation = gCli->allUsers;
    needsElevation |= (gWnd->prevInstall.typ == PreviousInstallationType::Both);
    needsElevation |= (gWnd->prevInstall.typ == PreviousInstallationType::Machine);
    if (needsElevation && !IsProcessRunningElevated()) {
        RestartElevatedForAllUsers();
        ::ExitProcess(0);
    }
    StartInstallation(gWnd);
}

static void OnButtonExit() {
    SendMessageW(gWnd->hwnd, WM_CLOSE, 0, 0);
}

static void OnButtonStartSumatra() {
    char* exePath = GetInstalledExePathTemp();
    RunNonElevated(exePath);
    OnButtonExit();
}

static void OnInstallationFinished() {
    DeleteWnd(&gWnd->btnInstall);
    DeleteWnd(&gWnd->progressBar);

    if (gWnd->failed) {
        gWnd->btnExit = CreateDefaultButton(gWnd->hwnd, _TR("Close"));
        gWnd->btnExit->onClicked = OnButtonExit;
        SetMsg(_TR("Installation failed!"), COLOR_MSG_FAILED);
    } else {
        gWnd->btnRunSumatra = CreateDefaultButton(gWnd->hwnd, _TR("Start SumatraPDF"));
        gWnd->btnRunSumatra->onClicked = OnButtonStartSumatra;
        SetMsg(_TR("Thank you! SumatraPDF has been installed."), COLOR_MSG_OK);
    }
    gMsgError = gFirstError;
    HwndInvalidate(gWnd->hwnd);

    CloseHandle(gWnd->hThread);

#if 0 // TODO: not sure
    if (gCli->runInstallNow && !gWnd->failed) {
        // click the Start button
        PostMessageW(gWnd->hwnd, WM_COMMAND, IDOK, 0);
    }
#endif
}

static void EnableAndShow(Wnd* w, bool enable) {
    if (w) {
        HwndSetVisibility(w->hwnd, enable);
        w->SetIsEnabled(enable);
    }
}

static Size SetButtonTextAndResize(Button* b, const WCHAR* s) {
    b->SetText(s);
    Size size = b->GetIdealSize();
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(b->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}

static void UpdateUIForOptionsState(InstallerWnd* wnd) {
    bool showOpts = wnd->showOptions;

    EnableAndShow(wnd->staticInstDir, showOpts);
    EnableAndShow(wnd->editInstallationDir, showOpts);
    EnableAndShow(wnd->btnBrowseDir, showOpts);

    EnableAndShow(wnd->checkboxForAllUsers, showOpts);
    EnableAndShow(wnd->checkboxRegisterSearchFilter, showOpts);
    EnableAndShow(wnd->checkboxRegisterPreview, showOpts);

    auto btnOptions = wnd->btnOptions;
    //[ ACCESSKEY_GROUP Installer
    //[ ACCESSKEY_ALTERNATIVE // ideally, the same accesskey is used for both
    if (showOpts) {
        SetButtonTextAndResize(btnOptions, _TR("Hide &Options"));
    } else {
        //| ACCESSKEY_ALTERNATIVE
        SetButtonTextAndResize(btnOptions, _TR("&Options"));
    }
    //] ACCESSKEY_ALTERNATIVE
    //] ACCESSKEY_GROUP Installer

    HwndInvalidate(wnd->hwnd);
    btnOptions->SetFocus();
}
static void OnButtonOptions() {
    // toggle options ui
    gWnd->showOptions = !gWnd->showOptions;
    UpdateUIForOptionsState(gWnd);
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
                SHFILEINFO sfi = {nullptr};
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
    BROWSEINFO bi = {0};
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

static void OnButtonBrowse() {
    auto editDir = gWnd->editInstallationDir;
    char* installDir = HwndGetTextTemp(editDir->hwnd);

    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir)) {
        installDir = path::GetDirTemp(installDir);
    }

    auto caption = _TRA("Select the folder where SumatraPDF should be installed:");
    char* installPath = BrowseForFolderTemp(gWnd->hwnd, installDir, caption);
    if (!installPath) {
        gWnd->btnBrowseDir->SetFocus();
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
    editDir->SetFocus();
}

// bottom-right
static void PositionInstallButton(Button* b) {
    HWND parent = ::GetParent(b->hwnd);
    Rect r = ClientRect(parent);
    Size size = b->GetIdealSize();
    int margin = DpiScale(parent, kInstallerWinMargin);
    int x = r.dx - size.dx - margin;
    int y = r.dy - size.dy - margin;
    b->SetBounds({x, y, size.dx, size.dy});
}

// caller needs to str::Free()
static char* GetDefaultInstallationDir(bool forAllUsers, bool ignorePrev) {
    logf("GetDefaultInstallationDir(forAllUsers=%d, ignorePrev=%d)\n", (int)forAllUsers, (int)ignorePrev);

    char* dir;
    char* dirPrevInstall = gWnd->prevInstall.installationDir;
    char* dirAll = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES, false);
    char* dirUser = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);

    if (dirPrevInstall && !ignorePrev) {
        logf("  using %s from previous install\n", dirPrevInstall);
        return str::Dup(dirPrevInstall);
    }

    if (forAllUsers) {
        dir = path::Join(dirAll, kAppName);
        logf("  using '%s' from GetSpecialFolderTemp(CSIDL_PROGRAM_FILES)\n", dir);
        return dir;
    }

    // %APPLOCALDATA%\SumatraPDF
    dir = path::Join(dirUser, kAppName);
    logf("  using '%s' from GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA)\n", dir);
    return dir;
}

void ForAllUsersStateChanged() {
    bool checked = gWnd->checkboxForAllUsers->IsChecked();
    logf("ForAllUsersStateChanged() to %d\n", (int)checked);
    Button_SetElevationRequiredState(gWnd->btnInstall->hwnd, checked);
    gCli->allUsers = checked;
    str::Free(gCli->installDir);
    gCli->installDir = GetDefaultInstallationDir(gCli->allUsers, true);
    gWnd->editInstallationDir->SetText(gCli->installDir);
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
static void CreateInstallerWindowControls(InstallerWnd* wnd) {
    // intelligently show options if user chose non-defaults
    // via cmd-line
    bool showOptions = false;

    HWND hwnd = wnd->hwnd;
    wnd->btnInstall = CreateDefaultButton(hwnd, _TR("Install SumatraPDF"));
    wnd->btnInstall->onClicked = OnButtonInstall;
    PositionInstallButton(wnd->btnInstall);

    Rect r = ClientRect(hwnd);
    wnd->btnOptions = CreateDefaultButton(hwnd, _TR("&Options"));
    wnd->btnOptions->onClicked = OnButtonOptions;
    auto btnSize = wnd->btnOptions->GetIdealSize();
    int margin = DpiScale(hwnd, kInstallerWinMargin);
    int x = margin;
    int y = r.dy - btnSize.dy - margin;
    uint flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW;
    SetWindowPos(wnd->btnOptions->hwnd, nullptr, x, y, 0, 0, flags);

    gButtonDy = btnSize.dy;
    gBottomPartDy = gButtonDy + (margin * 2);

    Size size = HwndMeasureText(hwnd, "Foo");
    int staticDy = size.dy + DpiScale(hwnd, 6);

    y = r.dy - gBottomPartDy;
    int dx = r.dx - (margin * 2) - DpiScale(hwnd, 2);

    x += DpiScale(hwnd, 2);

    // build options controls going from the bottom
    y -= (staticDy + margin);

    RECT rc;
    // only show this checkbox if the CPU arch of DLL and OS match
    // (assuming that the installer has the same CPU arch as its content!)
    if (IsProcessAndOsArchSame()) {
        // for Windows XP, this means only basic thumbnail support
        const WCHAR* s = _TR("Let Windows show &previews of PDF documents");
        bool isChecked = gCli->withPreview || IsPreviewInstalled();
        if (isChecked) {
            showOptions = true;
        }
        wnd->checkboxRegisterPreview = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        wnd->checkboxRegisterPreview->SetPos(&rc);
        y -= staticDy;

        isChecked = gCli->withFilter || IsSearchFilterInstalled();
        if (isChecked) {
            showOptions = true;
        }
        s = _TR("Let Windows Desktop Search &search PDF documents");
        wnd->checkboxRegisterSearchFilter = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        wnd->checkboxRegisterSearchFilter->SetPos(&rc);
        y -= staticDy;
    }

    {
        const WCHAR* s = _TR("Install for all users");
        bool isChecked = gCli->allUsers;
        if (isChecked) {
            showOptions = true;
        }
        wnd->checkboxForAllUsers = CreateCheckbox(hwnd, s, isChecked);
        wnd->checkboxForAllUsers->onCheckStateChanged = ForAllUsersStateChanged;
        rc = {x, y, x + dx, y + staticDy};
        wnd->checkboxForAllUsers->SetPos(&rc);
        y -= staticDy;
    }

    // a bit more space between text box and checkboxes
    y -= (DpiScale(hwnd, 4) + margin);

    const char* s = "&...";
    Size btnSize2 = HwndMeasureText(hwnd, s);
    btnSize2.dx += DpiScale(hwnd, 4);
    wnd->btnBrowseDir = CreateDefaultButton(hwnd, s);
    wnd->btnBrowseDir->onClicked = OnButtonBrowse;
    // btnSize = btnBrowseDir->GetIdealSize();
    x = r.dx - margin - btnSize2.dx;
    SetWindowPos(wnd->btnBrowseDir->hwnd, nullptr, x, y, btnSize2.dx, staticDy,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    x = margin;
    dx = r.dx - (2 * margin) - btnSize2.dx - DpiScale(hwnd, 4);

    EditCreateArgs eargs;
    eargs.parent = hwnd;
    eargs.withBorder = true;
    wnd->editInstallationDir = new Edit();
    HWND ehwnd = wnd->editInstallationDir->Create(eargs);
    CrashIf(!ehwnd);

    wnd->editInstallationDir->SetText(gCli->installDir);
    rc = {x, y, x + dx, y + staticDy};
    wnd->editInstallationDir->SetBounds(rc);

    y -= staticDy;

    const char* s2 = _TRA("Install SumatraPDF in &folder:");
    rc = {x, y, x + r.dx, y + staticDy};

    StaticCreateArgs args;
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

    wnd->btnInstall->SetFocus();
}
//] ACCESSKEY_GROUP Installer

#define kInstallerWindowClassName L"SUMATRA_PDF_INSTALLER_FRAME"

static HWND CreateInstallerHwnd() {
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
    gWnd->hwnd = hwnd;
    DpiScale(hwnd, dx, dy);
    HwndResizeClientSize(hwnd, dx, dy);
    CreateInstallerWindowControls(gWnd);
    if (gCli->runInstallNow) {
        PostMessageW(hwnd, WM_APP_START_INSTALLATION, 0, 0);
    }
    return hwnd;
}

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
            OnInstallationFinished();
            if (gWnd->btnRunSumatra) {
                gWnd->btnRunSumatra->SetFocus();
            }
            if (gWnd->btnExit) {
                gWnd->btnExit->SetFocus();
            }
            SetForegroundWindow(hwnd);
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }

    return 0;
}

static bool CreateInstallerWindow() {
    {
        WNDCLASSEX wcex{};

        FillWndClassEx(wcex, kInstallerWindowClassName, WndProcInstallerFrame);
        auto h = GetModuleHandleW(nullptr);
        WCHAR* resName = MAKEINTRESOURCEW(GetAppIconID());
        wcex.hIcon = LoadIconW(h, resName);

        ATOM atom = RegisterClassExW(&wcex);
        CrashIf(!atom);
        if (atom == 0) {
            logf("CreateInstallerWindow: RegisterClassExW() failed\n");
            return false;
        }
    }

    // TODO: gHwndFrame is shared between installer and uninstaller windows
    gHwndFrame = CreateInstallerHwnd();
    if (!gHwndFrame) {
        return false;
    }
    gWnd->hwnd = gHwndFrame;

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
            CheckInstallUninstallPossible(true);
            t = TimeGet();
        }
    }
}

static void ShowNoEmbeddedFiles(const WCHAR* msg) {
    if (gCli->silent) {
        log(ToUtf8Temp(msg));
        return;
    }
    const WCHAR* caption = L"Error";
    MessageBoxW(nullptr, msg, caption, MB_OK);
}

static bool OpenEmbeddedFilesArchive() {
    if (gArchive.filesCount > 0) {
        log("OpenEmbeddedFilesArchive: already opened\n");
        return true;
    }
    ByteSlice r = LockDataResource(1);
    if (r.empty()) {
        ShowNoEmbeddedFiles(L"No embbedded files");
        return false;
    }

    bool ok = lzma::ParseSimpleArchive(r.data(), r.size(), &gArchive);
    if (!ok) {
        ShowNoEmbeddedFiles(L"Embedded lzsa archive is corrupted");
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
    return ExtractFiles(&gArchive, dir);
}

// returns true if should exit the installer
bool MaybeMismatchedOSDialog(HWND hwndParent) {
    if (IsProcessAndOsArchSame()) {
        return false;
    }
    logf("Mismatch of the OS and executable arch\n");

    constexpr int kBtnIdContinue = 100;
    constexpr int kBtnIdDownload = 101;
    TASKDIALOGCONFIG dialogConfig{};
    TASKDIALOG_BUTTON buttons[2];

    buttons[0].nButtonID = kBtnIdDownload;
    buttons[0].pszButtonText = _TR("Download 64-bit version");
    buttons[1].nButtonID = kBtnIdContinue;
    buttons[1].pszButtonText = _TR("&Continue installing 32-bit version");

    DWORD flags = TDF_SIZE_TO_CONTENT | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = _TR("Installing 32-bit SumatraPDF on 64-bit OS");
    // dialogConfig.pszMainInstruction = mainInstr;
    dialogConfig.pszContent =
        _TR("You're installing 32-bit SumatraPDF on 64-bit OS.\nWould you like to download\n64-bit version?");
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
    CrashIf(hr == E_INVALIDARG);
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
    if (gCli->log) {
        installerLogPath = GetInstallerLogPath();
        bool removeLog = !gCli->runInstallNow;
        StartLogToFile(installerLogPath, removeLog);
    }
    logf("------------- Starting SumatraPDF installation\n");

    gWnd = new InstallerWnd();
    GetPreviousInstallInfo(&gWnd->prevInstall);

    if (!gCli->installDir) {
        gCli->installDir = GetDefaultInstallationDir(gCli->allUsers, false);
    }
    char* cmdLine = ToUtf8Temp(GetCommandLineW());
    logf("Running'%s', cmdLine: '%s', installing into dir '%s'\n", GetExePathTemp(), cmdLine, gCli->installDir);

    if (!gCli->silent && MaybeMismatchedOSDialog(nullptr)) {
        logfa("quitting because !gCli->silent && MaybeMismatchedOSDialog()\n");
        return 0;
    }

    int ret = 0;

    if (!OpenEmbeddedFilesArchive()) {
        return 1;
    }

    gDefaultMsg = _TR("Thank you for choosing SumatraPDF!");

    if (!gCli->runInstallNow) {
        // if not set explicitly, default to state from previous installation
        if (!gCli->withFilter) {
            gCli->withFilter = gWnd->prevInstall.searchFilterInstalled;
        }
        if (!gCli->withPreview) {
            gCli->withPreview = gWnd->prevInstall.previewInstalled;
        }
    }
    logf(
        "RunInstaller: gClii->silent: %d, gCli->allUsers: %d, gCli->runInstallNow = %d, gCli->withFilter = %d, "
        "gCli->withPreview = %d\n",
        (int)gCli->silent, (int)gCli->allUsers, (int)gCli->runInstallNow, (int)gCli->withFilter,
        (int)gCli->withPreview);

    // unregister search filter and previewer to reduce
    // possibility of blocking the installation because the dlls are loaded
    UninstallSearchFilter();
    log("After UninstallSearchFilter\n");
    UninstallPreviewDll();
    log("After UninstallPreviewDll\n");

    if (gCli->silent) {
        if (gCli->allUsers && !IsProcessRunningElevated()) {
            log("allUsers but not elevated: re-starting as elevated\n");
            RestartElevatedForAllUsers();
            ::ExitProcess(0);
        }
        gInstallStarted = true;
        logfa("gCli->silent, before runinng InstallerThread()\n");
        InstallerThread(nullptr);
        ret = gWnd->failed ? 1 : 0;
    } else {
        log("Before CreateInstallerWindow()\n");
        if (!CreateInstallerWindow()) {
            log("CreateInstallerWindow() failed\n");
            goto Exit;
        }
        log("Before SetForegroundWindow()\n");
        SetForegroundWindow(gWnd->hwnd);
        log("Before RunApp()\n");
        ret = RunApp();
        logfa("RunApp() returned %d\n", ret);
    }

    // re-register if we un-registered but installation was cancelled
    if (gWnd->prevInstall.searchFilterInstalled) {
        log("re-registering search filter\n");
        RegisterSearchFilter(gCli->allUsers);
    }
    if (gWnd->prevInstall.previewInstalled) {
        log("re-registering previewer\n");
        RegisterPreviewer(gCli->allUsers);
    }
    log("Installer finished\n");
Exit:
    if (installerLogPath) {
        LaunchFileIfExists(installerLogPath);
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
