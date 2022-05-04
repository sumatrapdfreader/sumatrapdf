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
#include "wingui/wingui2.h"

#include "Translations.h"

#include "AppPrefs.h"
#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "Version.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "Installer.h"
#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"

#include "utils/Log.h"

constexpr int kInstallerWinMargin = 8;

using namespace wg;

struct InstallerWnd;

static InstallerWnd* gWnd = nullptr;

static lzma::SimpleArchive gArchive{};

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
    Checkbox* checkboxRegisterPreviewer = nullptr;
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
        args.text = ToUtf8Temp(s).Get();
    }
    args.initialState = isChecked ? CheckState::Checked : CheckState::Unchecked;

    Checkbox* w = new Checkbox();
    w->Create(args);
    return w;
}

char* GetInstallerLogPath() {
    TempWstr dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true);
    if (!dir.Get()) {
        return nullptr;
    }
    WCHAR* path = path::Join(dir, L"sumatra-install-log.txt", nullptr);
    auto res = strconv::WstrToUtf8(path);
    str::Free(path);
    return res;
}

bool ExtractFiles(lzma::SimpleArchive* archive, const WCHAR* destDir) {
    logf(L"ExtractFiles(): dir '%s'\n", destDir);
    lzma::FileInfo* fi;
    u8* uncompressed;

    int nFiles = archive->filesCount;

    for (int i = 0; i < nFiles; i++) {
        fi = &archive->files[i];
        uncompressed = lzma::GetFileDataByIdx(archive, i, nullptr);

        if (!uncompressed) {
            NotifyFailed(
                _TR("The installer has been corrupted. Please download it again.\nSorry for the inconvenience!"));
            return false;
        }
        auto fileName = ToWstrTemp(fi->name);
        AutoFreeWstr filePath = path::Join(destDir, fileName);

        ByteSlice d = {uncompressed, fi->uncompressedSize};
        bool ok = file::WriteFile(filePath, d);
        free(uncompressed);

        if (!ok) {
            WCHAR* msg = str::Format(_TR("Couldn't write %s to disk"), filePath.data);
            NotifyFailed(msg);
            str::Free(msg);
            return false;
        }
        logf(L"  extracted '%s'\n", fileName.Get());
        ProgressStep();
    }

    return true;
}

static bool CreateInstallationDirectory() {
    log("CreateInstallationDirectory()\n");
    bool ok = dir::CreateAll(gCli->installDir);
    if (!ok) {
        LogLastError();
        NotifyFailed(_TR("Couldn't create the installation directory"));
    }
    return ok;
}

static bool CopySelfToDir(const WCHAR* destDir) {
    logf(L"CopySelfToDir(%s)\n", destDir);
    auto exePath = GetExePathTemp();
    WCHAR* dstPath = path::JoinTemp(destDir, kExeName);
    bool failIfExists = false;
    bool ok = file::Copy(dstPath, exePath, failIfExists);
    // strip zone identifier (if exists) to avoid windows
    // complaining when launching the file
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1782
    auto dstPathA = ToUtf8Temp(dstPath);
    file::DeleteZoneIdentifier(dstPathA);
    if (!ok) {
        logf(L"  failed to copy '%s' to dir '%s'\n", exePath.Get(), destDir);
        return false;
    }
    logf(L"  copied '%s' to dir '%s'\n", exePath.Get(), destDir);
    return true;
}

static void CopySettingsFile() {
    log("CopySettingsFile()\n");
    // up to 3.1.2 we stored settings in %APPDATA%
    // after that we use %LOCALAPPDATA%
    // copy the settings from old directory

    // seen a crash when running elevated
    TempWstr srcDir = GetSpecialFolderTemp(CSIDL_APPDATA, false);
    if (srcDir.empty()) {
        return;
    }
    TempWstr dstDir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, false);
    if (dstDir.empty()) {
        return;
    }

    const WCHAR* prefsFileName = prefs::GetSettingsFileNameTemp();
    AutoFreeWstr srcPath = path::Join(srcDir.Get(), kAppName, prefsFileName);
    AutoFreeWstr dstPath = path::Join(dstDir.Get(), kAppName, prefsFileName);

    // don't over-write
    bool failIfExists = true;
    // don't care if it fails or not
    file::Copy(dstPath.Get(), srcPath.Get(), failIfExists);
    logf(L"  copied '%s' to '%s'\n", srcPath.Get(), dstPath.Get());
}

static bool CreateAppShortcut(int csidl) {
    WCHAR* shortcutPath = GetShortcutPathTemp(csidl);
    if (!shortcutPath) {
        log("CreateAppShortcut() failed\n");
        return false;
    }
    logf(L"CreateAppShortcut(csidl=%d), path=%s\n", csidl, shortcutPath);
    WCHAR* installedExePath = GetInstalledExePathTemp();
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
    WCHAR* path = GetShortcutPathTemp(csidl);
    if (!path || !file::Exists(path)) {
        return;
    }
    DeleteFileW(path);
    logf("RemoveShorcuts: deleted '%s'\n", path);
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

static DWORD WINAPI InstallerThread(__unused LPVOID data) {
    gWnd->failed = true;
    bool ok;

    HKEY key = gCli->allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    if (!ExtractInstallerFiles()) {
        log("ExtractInstallerFiles() failed\n");
        goto Error;
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
        RegisterSearchFilter(gCli->allUsers);
    }

    if (gCli->withPreview) {
        RegisterPreviewer(gCli->allUsers);
    }

    CreateAppShortcuts(gCli->allUsers);

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gWnd->failed = false;

    ok = WriteUninstallerRegistryInfo(key);
    if (!ok) {
        NotifyFailed(_TR("Failed to write the uninstallation information to the registry"));
    }

    ok = WriteExtendedFileExtensionInfo(key);
    if (!ok) {
        NotifyFailed(_TR("Failed to write the extended file extension information to the registry"));
    }

    ProgressStep();
    log("Installer thread finished\n");
Error:
    // TODO: roll back installation on failure (restore previous installation!)
    if (gWnd->hwnd) {
        if (!gCli->silent) {
            Sleep(500); // allow a glimpse of the completed progress bar before hiding it
            PostMessageW(gWnd->hwnd, WM_APP_INSTALLATION_FINISHED, 0, 0);
        }
    }
    return 0;
}

static void RestartElevatedForAllUsers() {
    auto exePath = GetExePathTemp();
    WCHAR* cmdLine = (WCHAR*)L"-run-install-now";
    if (gWnd->checkboxForAllUsers->IsChecked()) {
        cmdLine = str::JoinTemp(cmdLine, L" -all-users");
    }
    if (gCli->withFilter) {
        cmdLine = str::JoinTemp(cmdLine, L" -with-filter");
    }
    if (gCli->withPreview) {
        cmdLine = str::JoinTemp(cmdLine, L" -with-preview");
    }
    if (gCli->silent) {
        cmdLine = str::JoinTemp(cmdLine, L" -silent");
    }
    if (gCli->log) {
        cmdLine = str::JoinTemp(cmdLine, L" -log");
    }
    cmdLine = str::JoinTemp(cmdLine, L" -install-dir \"", gCli->installDir);
    cmdLine = str::JoinTemp(cmdLine, L"\"");
    logf(L"Re-launching '%s' as elevated, args\n%s\n", exePath.Get(), cmdLine);
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

static void DeleteWnd(Static** wnd) {
    delete *wnd;
    *wnd = nullptr;
}

static void DeleteWnd(Button** wnd) {
    delete *wnd;
    *wnd = nullptr;
}

static void DeleteWnd(Edit** wnd) {
    delete *wnd;
    *wnd = nullptr;
}

static void DeleteWnd(Checkbox** wnd) {
    delete *wnd;
    *wnd = nullptr;
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
    DeleteWnd(&wnd->checkboxRegisterPreviewer);
    DeleteWnd(&wnd->btnOptions);

    wnd->btnInstall->SetIsEnabled(false);

    SetMsg(_TR("Installation in progress..."), COLOR_MSG_INSTALLATION);
    HwndInvalidate(wnd->hwnd);

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
        WCHAR* exePath = GetInstalledExePathTemp();
        KillProcessesWithModule(exePath, true);
    }

    if (!CheckInstallUninstallPossible()) {
        return;
    }

    WCHAR* userInstallDir = win::GetTextTemp(gWnd->editInstallationDir->hwnd).Get();
    if (!str::IsEmpty(userInstallDir)) {
        str::ReplaceWithCopy(&gCli->installDir, userInstallDir);
    }

    // note: this checkbox isn't created when running inside Wow64
    gCli->withFilter = gWnd->checkboxRegisterSearchFilter->IsChecked();
    // note: this checkbox isn't created on Windows 2000 and XP
    gCli->withPreview = gWnd->checkboxRegisterPreviewer->IsChecked();
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
    WCHAR* exePath = GetInstalledExePathTemp();
    RunNonElevated(exePath);
    OnButtonExit();
}

static void OnInstallationFinished() {
    delete gWnd->btnInstall;
    delete gWnd->progressBar;

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
        win::SetVisibility(w->hwnd, enable);
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

static void OnButtonOptions() {
    gWnd->showOptions = !gWnd->showOptions;
    bool showOpts = gWnd->showOptions;

    EnableAndShow(gWnd->staticInstDir, showOpts);
    EnableAndShow(gWnd->editInstallationDir, showOpts);
    EnableAndShow(gWnd->btnBrowseDir, showOpts);

    EnableAndShow(gWnd->checkboxForAllUsers, showOpts);
    EnableAndShow(gWnd->checkboxRegisterSearchFilter, showOpts);
    EnableAndShow(gWnd->checkboxRegisterPreviewer, showOpts);

    auto btnOptions = gWnd->btnOptions;
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

    HwndInvalidate(gWnd->hwnd);
    btnOptions->SetFocus();
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

static TempWstr BrowseForFolderTemp(HWND hwnd, const WCHAR* initialFolder, const WCHAR* caption) {
    BROWSEINFO bi = {};
    bi.hwndOwner = hwnd;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = caption;
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
    return str::DupTemp(buf);
}

static void OnButtonBrowse() {
    auto editDir = gWnd->editInstallationDir;
    WCHAR* installDir = win::GetTextTemp(editDir->hwnd).Get();

    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir)) {
        installDir = path::GetDirTemp(installDir);
    }

    auto caption = _TR("Select the folder where SumatraPDF should be installed:");
    WCHAR* installPath = BrowseForFolderTemp(gWnd->hwnd, installDir, caption);
    if (!installPath) {
        gWnd->btnBrowseDir->SetFocus();
        return;
    }

    // force paths that aren't entered manually to end in ...\SumatraPDF
    // to prevent unintended installations into e.g. %ProgramFiles% itself
    WCHAR* end = str::JoinTemp(L"\\", kAppName);
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
static WCHAR* GetDefaultInstallationDir(bool forAllUsers) {
    logf(L"GetDefaultInstallationDir(forAllUsers=%d)\n", (int)forAllUsers);

    auto dir = gWnd->prevInstall.installationDir;
    if (dir) {
        // Note: prev dir might be incompatible with forAllUsers setting
        // e.g. installing for current user in an admin-only dir
        // but not sure what should I do in such case
        logf(L"  using %s from previous install\n", dir);
        return str::Dup(dir);
    }

    if (forAllUsers) {
        dir = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES, true).Get();
        if (dir) {
            dir = path::Join(dir, kAppName);
            logf(L"  using '%s' by GetSpecialFolderTemp(CSIDL_PROGRAM_FILES)\n", dir);
            return dir;
        }
    }

    // %APPLOCALDATA%\SumatraPDF
    dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true).Get();
    if (dir) {
        dir = path::Join(dir, kAppName);
        logf(L"  using '%s' by GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA)\n", dir);
        return dir;
    }

    // fall back to C:\SumatraPDF as a last resort
    dir = str::Join(L"C:\\", kAppName);
    logf(L"  using %s as last resort\n", dir);
    return dir;
}

void ForAllUsersStateChanged() {
    bool checked = gWnd->checkboxForAllUsers->IsChecked();
    logf("ForAllUsersStateChanged() to %d\n", (int)checked);
    Button_SetElevationRequiredState(gWnd->btnInstall->hwnd, checked);
    PositionInstallButton(gWnd->btnInstall);
    gCli->allUsers = checked;
    str::Free(gCli->installDir);
    gCli->installDir = GetDefaultInstallationDir(gCli->allUsers);
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

//[ ACCESSKEY_GROUP Installer
static void CreateInstallerWindowControls(HWND hwnd) {
    gWnd->btnInstall = CreateDefaultButton(hwnd, _TR("Install SumatraPDF"));
    gWnd->btnInstall->onClicked = OnButtonInstall;
    PositionInstallButton(gWnd->btnInstall);

    Rect r = ClientRect(hwnd);
    gWnd->btnOptions = CreateDefaultButton(hwnd, _TR("&Options"));
    gWnd->btnOptions->onClicked = OnButtonOptions;
    auto btnSize = gWnd->btnOptions->GetIdealSize();
    int margin = DpiScale(hwnd, kInstallerWinMargin);
    int x = margin;
    int y = r.dy - btnSize.dy - margin;
    uint flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW;
    SetWindowPos(gWnd->btnOptions->hwnd, nullptr, x, y, 0, 0, flags);

    gButtonDy = btnSize.dy;
    gBottomPartDy = gButtonDy + (margin * 2);

    Size size = TextSizeInHwnd(hwnd, L"Foo");
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
        bool isChecked = gCli->withPreview || IsPreviewerInstalled();
        gWnd->checkboxRegisterPreviewer = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        gWnd->checkboxRegisterPreviewer->SetPos(&rc);
        y -= staticDy;

        isChecked = gCli->withFilter || IsSearchFilterInstalled();
        s = _TR("Let Windows Desktop Search &search PDF documents");
        gWnd->checkboxRegisterSearchFilter = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        gWnd->checkboxRegisterSearchFilter->SetPos(&rc);
        y -= staticDy;
    }

    {
        const WCHAR* s = _TR("Install for all users");
        bool isChecked = gCli->allUsers;
        gWnd->checkboxForAllUsers = CreateCheckbox(hwnd, s, isChecked);
        gWnd->checkboxForAllUsers->onCheckStateChanged = ForAllUsersStateChanged;
        rc = {x, y, x + dx, y + staticDy};
        gWnd->checkboxForAllUsers->SetPos(&rc);
        y -= staticDy;
    }

    // a bit more space between text box and checkboxes
    y -= (DpiScale(hwnd, 4) + margin);

    const WCHAR* s = L"&...";
    Size btnSize2 = TextSizeInHwnd(hwnd, s);
    btnSize2.dx += DpiScale(hwnd, 4);
    gWnd->btnBrowseDir = CreateDefaultButton(hwnd, s);
    gWnd->btnBrowseDir->onClicked = OnButtonBrowse;
    // btnSize = btnBrowseDir->GetIdealSize();
    x = r.dx - margin - btnSize2.dx;
    SetWindowPos(gWnd->btnBrowseDir->hwnd, nullptr, x, y, btnSize2.dx, staticDy,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    x = margin;
    dx = r.dx - (2 * margin) - btnSize2.dx - DpiScale(hwnd, 4);

    EditCreateArgs eargs;
    eargs.parent = hwnd;
    eargs.withBorder = true;
    gWnd->editInstallationDir = new Edit();
    HWND ehwnd = gWnd->editInstallationDir->Create(eargs);
    CrashIf(!ehwnd);

    gWnd->editInstallationDir->SetText(gCli->installDir);
    rc = {x, y, x + dx, y + staticDy};
    gWnd->editInstallationDir->SetBounds(rc);

    y -= staticDy;

    const char* s2 = _TRA("Install SumatraPDF in &folder:");
    rc = {x, y, x + r.dx, y + staticDy};

    StaticCreateArgs args;
    args.parent = hwnd;
    args.text = s2;
    gWnd->staticInstDir = new Static();
    gWnd->staticInstDir->Create(args);
    gWnd->staticInstDir->SetBounds(rc);

    gWnd->showOptions = !gWnd->showOptions;
    OnButtonOptions();

    gWnd->btnInstall->SetFocus();
}
//] ACCESSKEY_GROUP Installer

static HWND CreateInstallerHwnd() {
    AutoFreeWstr title(str::Format(_TR("SumatraPDF %s Installer"), CURR_VERSION_STR));

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
    HWND hwnd = CreateWindowExW(exStyle, winCls, title.Get(), dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
    DpiScale(hwnd, dx, dy);
    HwndResizeClientSize(hwnd, dx, dy);
    CreateInstallerWindowControls(hwnd);
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

        case WM_PAINT:
            OnPaintFrame(hwnd, gWnd->showOptions);
            break;

        case WM_COMMAND:
            handled = InstallerOnWmCommand(wp);
            if (!handled) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            break;

        case WM_APP_START_INSTALLATION:
            StartInstallation(gWnd);
            break;

        case WM_APP_INSTALLATION_FINISHED:
            OnInstallationFinished();
            if (gWnd->btnRunSumatra) {
                gWnd->btnRunSumatra->SetFocus();
            }
            if (gWnd->btnExit) {
                gWnd->btnExit->SetFocus();
            }
            break;

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
        if (dur > 10000 && gWnd->btnInstall && gWnd->btnInstall->IsEnabled()) {
            CheckInstallUninstallPossible(true);
            t = TimeGet();
        }
    }
}

static void ShowNoEmbeddedFiles(const WCHAR* msg) {
    if (gCli->silent) {
        log(msg);
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
    auto [data, size, res] = LockDataResource(1);
    if (data == nullptr) {
        ShowNoEmbeddedFiles(L"No embbedded files");
        return false;
    }

    bool ok = lzma::ParseSimpleArchive(data, size, &gArchive);
    if (!ok) {
        ShowNoEmbeddedFiles(L"Embedded lzsa archive is corrupted");
        return false;
    }
    log("OpenEmbeddedFilesArchive: opened archive\n");
    return true;
}

bool ExtractInstallerFiles() {
    log("ExtractInstallerFiles()\n");
    if (!CreateInstallationDirectory()) {
        log("  CreateInstallationDirectory() failed\n");
        return false;
    }

    bool ok = CopySelfToDir(GetInstallDirTemp());
    if (!ok) {
        return false;
    }
    ProgressStep();

    ok = OpenEmbeddedFilesArchive();
    if (!ok) {
        return false;
    }
    // on error, ExtractFiles() shows error message itself
    return ExtractFiles(&gArchive, gCli->installDir);
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
        LaunchBrowser("https://www.sumatrapdfreader.org/download-free-pdf-viewer.html");
        return true;
    }
    return false;
}

int RunInstaller() {
    trans::SetCurrentLangByCode(trans::DetectUserLang());

    const char* installerLogPath = nullptr;
    if (gCli->log) {
        installerLogPath = GetInstallerLogPath();
        if (installerLogPath) {
            StartLogToFile(installerLogPath, true);
        }
    }
    logf("------------- Starting SumatraPDF installation\n");

    gWnd = new InstallerWnd();
    GetPreviousInstallInfo(&gWnd->prevInstall);

    if (!gCli->installDir) {
        gCli->installDir = GetDefaultInstallationDir(gCli->allUsers);
    }
    logf(L"Running'%s' installing into dir '%s'\n", GetExePathTemp().Get(), gCli->installDir);

    if (!gCli->silent && MaybeMismatchedOSDialog(nullptr)) {
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
    logf("RunInstaller: gCli->runInstallNow = %d, gCli->withFilter = %d, gCli->withPreview = %d\n",
         (int)gCli->runInstallNow, (int)gCli->withFilter, (int)gCli->withPreview);

    // unregister search filter and previewer to reduce
    // possibility of blocking the installation because the dlls are loaded
    UninstallSearchFilter();
    UninstallPreviewDll();

    if (gCli->silent) {
        if (gCli->allUsers && !IsProcessRunningElevated()) {
            log("allUsers but not elevated: re-starting as elevated\n");
            RestartElevatedForAllUsers();
            ::ExitProcess(0);
        }
        InstallerThread(nullptr);
        ret = gWnd->failed ? 1 : 0;
    } else {
        if (!CreateInstallerWindow()) {
            log("CreateInstallerWindow() failed\n");
            goto Exit;
        }

        BringWindowToTop(gWnd->hwnd);

        ret = RunApp();
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
    LaunchFileIfExists(installerLogPath);
Exit:
    free(gFirstError);

    return ret;
}
