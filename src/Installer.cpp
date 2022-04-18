/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <tlhelp32.h>
#include <io.h>

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
#include "utils/RegistryPaths.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"

#include "wingui/wingui2.h"

#include "Translations.h"

#include "AppPrefs.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "SumatraPDF.h"
#include "Version.h"
#include "Installer.h"

#include "ifilter/PdfFilterClsid.h"
#include "previewer/PdfPreview.h"

#include "utils/Log.h"

#define kInstallerWinMargin DpiScale(8)

using namespace wg;

static bool gAutoUpdate = false;
static HBRUSH ghbrBackground = nullptr;

static Button* gButtonOptions = nullptr;
static Button* gButtonRunSumatra = nullptr;
static lzma::SimpleArchive gArchive{};

static Static* gStaticInstDir = nullptr;
static Edit* gEditInstallationDir = nullptr;
static Button* gButtonBrowseDir = nullptr;

static Checkbox* gCheckboxForAllUsers = nullptr;
static Checkbox* gCheckboxRegisterSearchFilter = nullptr;
static Checkbox* gCheckboxRegisterPreviewer = nullptr;
static Progress* gProgressBar = nullptr;
static Button* gButtonExit = nullptr;
static Button* gButtonInstall = nullptr;

static HANDLE hThread = nullptr;
static bool success = false;
static bool gWasSearchFilterInstalled = false;
static bool gWasPreviewInstaller = false;
static bool gShowOptions = false;

int currProgress = 0;
static void ProgressStep() {
    currProgress++;
    if (gProgressBar) {
        // possibly dangerous as is called on a thread
        gProgressBar->SetCurrent(currProgress);
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

static void OnButtonExit() {
    SendMessageW(gHwndFrame, WM_CLOSE, 0, 0);
}

static void CreateButtonExit(HWND hwndParent) {
    gButtonExit = CreateDefaultButton(hwndParent, _TR("Close"));
    gButtonExit->onClicked = OnButtonExit;
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
    auto exeName = GetExeNameTemp();
    auto dstPath = path::Join(destDir, exeName);
    bool failIfExists = false;
    bool ok = file::Copy(dstPath, exePath, failIfExists);
    // strip zone identifier (if exists) to avoid windows
    // complaining when launching the file
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1782
    auto dstPathA = ToUtf8Temp(dstPath);
    file::DeleteZoneIdentifier(dstPathA);
    str::Free(dstPath);
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

    const WCHAR* appName = GetAppNameTemp();
    const WCHAR* prefsFileName = prefs::GetSettingsFileNameTemp();
    AutoFreeWstr srcPath = path::Join(srcDir.Get(), appName, prefsFileName);
    AutoFreeWstr dstPath = path::Join(dstDir.Get(), appName, prefsFileName);

    // don't over-write
    bool failIfExists = true;
    // don't care if it fails or not
    file::Copy(dstPath.Get(), srcPath.Get(), failIfExists);
    logf(L"  copied '%s' to '%s'\n", srcPath.Get(), dstPath.Get());
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(const WCHAR* dir) {
    logf(L"GetDirSize(%s)\n", dir);
    AutoFreeWstr dirPattern = path::Join(dir, L"*");
    WIN32_FIND_DATA findData;

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD totalSize = 0;
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            totalSize += findData.nFileSizeLow;
        } else if (!str::Eq(findData.cFileName, L".") && !str::Eq(findData.cFileName, L"..")) {
            AutoFreeWstr subdir = path::Join(dir, findData.cFileName);
            totalSize += GetDirSize(subdir);
        }
    } while (FindNextFile(h, &findData) != 0);
    FindClose(h);

    return totalSize;
}

// caller needs to str::Free() the result
static WCHAR* GetInstallDate() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    return str::Format(L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
}

static bool WriteUninstallerRegistryInfo(HKEY hkey) {
    logf("WriteUninstallerRegistryInfo(%s)\n", RegKeyNameTemp(hkey));
    bool ok = true;

    AutoFreeWstr installedExePath = GetInstallationFilePath(GetExeNameTemp());
    AutoFreeWstr installDate = GetInstallDate();
    WCHAR* installDir = GetInstallDirTemp();
    WCHAR* uninstallerPath = installedExePath; // same as
    AutoFreeWstr uninstallCmdLine = str::Format(L"\"%s\" -uninstall", uninstallerPath);

    const WCHAR* appName = GetAppNameTemp();
    AutoFreeWstr regPathUninst = GetRegPathUninst(appName);
    // path to installed executable (or "$path,0" to force the first icon)
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"DisplayIcon", installedExePath);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"DisplayName", appName);
    // version format: "1.2"
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"DisplayVersion", CURR_VERSION_STR);
    // Windows XP doesn't allow to view the version number at a glance,
    // so include it in the DisplayName
    if (!IsWindowsVistaOrGreater()) {
        auto key = str::JoinTemp(appName, L" ", CURR_VERSION_STR);
        ok &= LoggedWriteRegStr(hkey, regPathUninst, L"DisplayName", key);
    }
    DWORD size = GetDirSize(gCli->installDir) / 1024;
    // size of installed directory after copying files
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, L"EstimatedSize", size);
    // current date as YYYYMMDD
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"InstallDate", installDate);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"InstallLocation", installDir);
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, L"NoModify", 1);
    ok &= LoggedWriteRegDWORD(hkey, regPathUninst, L"NoRepair", 1);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"Publisher", TEXT(PUBLISHER_STR));
    // command line for uninstaller
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"UninstallString", uninstallCmdLine);
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"URLInfoAbout", L"https://www.sumatrapdfreader.org/");
    ok &= LoggedWriteRegStr(hkey, regPathUninst, L"URLUpdateInfo",
                            L"https://www.sumatrapdfreader.org/docs/Version-history.html");

    return ok;
}

static bool WriteUninstallerRegistryInfos() {
    // we only want to write one of those
    if (gCli->allUsers) {
        bool ok = WriteUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
        if (ok) {
            return true;
        }
    }
    return WriteUninstallerRegistryInfo(HKEY_CURRENT_USER);
}

// http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
static bool WriteExtendedFileExtensionInfo(HKEY hkey) {
    logf("WriteExtendedFileExtensionInfo('%s')\n", RegKeyNameTemp(hkey));
    bool ok = true;

    const WCHAR* exeName = GetExeNameTemp();
    AutoFreeWstr exePath = GetInstalledExePath();
    if (HKEY_LOCAL_MACHINE == hkey) {
        AutoFreeWstr key = str::Join(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", exeName);
        ok &= LoggedWriteRegStr(hkey, key, nullptr, exePath);
    }
    AutoFreeWstr REG_CLASSES_APPS = GetRegClassesApps(GetAppNameTemp());

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    AutoFreeWstr iconPath = str::Join(exePath, L",1");
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\DefaultIcon");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, iconPath);
    }
    AutoFreeWstr cmdPath = str::Format(L"\"%s\" \"%%1\" %%*", exePath.Get());
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\Open\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, cmdPath);
    }
    AutoFreeWstr printPath = str::Format(L"\"%s\" -print-to-default \"%%1\"", exePath.Get());
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\Print\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, printPath);
    }
    AutoFreeWstr printToPath = str::Format(L"\"%s\" -print-to \"%%2\" \"%%1\"", exePath.Get());
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\PrintTo\\Command");
        ok &= LoggedWriteRegStr(hkey, key, nullptr, printToPath);
    }

    // don't add REG_CLASSES_APPS L"\\SupportedTypes", as that prevents SumatraPDF.exe to
    // potentially appear in the Open With lists for other filetypes (such as single images)
    ok &= ListAsDefaultProgramPreWin10(exeName, GetSupportedExts(), hkey);

    // in case these values don't exist yet (we won't delete these at uninstallation)
    ok &= LoggedWriteRegStr(hkey, kRegClassesPdf, L"Content Type", L"application/pdf");
    const WCHAR* key = L"Software\\Classes\\MIME\\Database\\Content Type\\application/pdf";
    ok &= LoggedWriteRegStr(hkey, key, L"Extension", L".pdf");

    return ok;
}

static bool WriteExtendedFileExtensionInfos() {
    bool ok1 = true;
    if (gCli->allUsers) {
        ok1 = WriteExtendedFileExtensionInfo(HKEY_LOCAL_MACHINE);
    }
    bool ok2 = WriteExtendedFileExtensionInfo(HKEY_CURRENT_USER);
    return ok1 || ok2;
}

static void OnButtonStartSumatra() {
    AutoFreeWstr exePath(GetInstalledExePath());
    RunNonElevated(exePath);
    OnButtonExit();
}

static void CreateButtonRunSumatra(HWND hwndParent) {
    gButtonRunSumatra = CreateDefaultButton(hwndParent, _TR("Start SumatraPDF"));
    gButtonRunSumatra->onClicked = OnButtonStartSumatra;
}

static bool CreateAppShortcut(int csidl) {
    AutoFreeWstr shortcutPath = GetShortcutPath(csidl);
    if (!shortcutPath.Get()) {
        log("CreateAppShortcut() failed\n");
        return false;
    }
    logf(L"CreateAppShortcut(csidl=%d), path=%s\n", csidl, shortcutPath.Get());
    AutoFreeWstr installedExePath = GetInstalledExePath();
    return CreateShortcut(shortcutPath, installedExePath);
}

// https://docs.microsoft.com/en-us/windows/win32/shell/csidl
// CSIDL_COMMON_DESKTOPDIRECTORY - files and folders on desktop for all users. C:\Documents and Settings\All
// Users\Desktop CSIDL_COMMON_STARTMENU - Start menu for all users, C:\Documents and Settings\All Users\Start Menu
// CSIDL_DESKTOP - virutal folder, desktop for current user
// CSIDL_STARTMENU - Start menu for current user. Settings\username\Start Menu
static int shortcutDirs[] = {CSIDL_COMMON_DESKTOPDIRECTORY, CSIDL_COMMON_STARTMENU, CSIDL_DESKTOP, CSIDL_STARTMENU};

static void CreateAppShortcuts(bool forAllUsers) {
    logf("CreateAppShortcuts(forAllUsers=%d)\n", (int)forAllUsers);
    size_t start = forAllUsers ? 0 : 2;
    for (size_t i = start; i < dimof(shortcutDirs); i++) {
        int csidl = shortcutDirs[i];
        CreateAppShortcut(csidl);
    }
}

static DWORD WINAPI InstallerThread(__unused LPVOID data) {
    success = false;
    const WCHAR* appName{nullptr};
    const WCHAR* exeName{nullptr};

    if (!ExtractInstallerFiles()) {
        log("ExtractInstallerFiles() failed\n");
        goto Error;
    }

    CopySettingsFile();

    // mark them as uninstalled
    gWasSearchFilterInstalled = false;
    gWasPreviewInstaller = false;

    if (gCli->withFilter) {
        RegisterSearchFilter(false);
    }

    if (gCli->withPreview) {
        RegisterPreviewer(false);
    }

    UninstallBrowserPlugin();

    CreateAppShortcuts(gCli->allUsers);

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    success = true;

    if (!WriteUninstallerRegistryInfos()) {
        log("WriteUninstallerRegistryInfos() failed\n");
        NotifyFailed(_TR("Failed to write the uninstallation information to the registry"));
    }

    if (!WriteExtendedFileExtensionInfos()) {
        log("WriteExtendedFileExtensionInfos() failed\n");
        NotifyFailed(_TR("Failed to write the extended file extension information to the registry"));
    }

    appName = GetAppNameTemp();
    exeName = GetExeNameTemp();
    if (!ListAsDefaultProgramWin10(appName, exeName, GetSupportedExts())) {
        log("Failed to register as default program on win 10\n");
        NotifyFailed(_TR("Failed to register as default program on win 10"));
    }

    ProgressStep();
    log("Installer thread finished\n");
Error:
    // TODO: roll back installation on failure (restore previous installation!)
    if (gHwndFrame) {
        if (!gCli->silent) {
            Sleep(500); // allow a glimpse of the completed progress bar before hiding it
            PostMessageW(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
        }
    }
    return 0;
}

static void RestartElevatedForAllUsers() {
    auto exePath = GetExePathTemp();
    WCHAR* cmdLine = (WCHAR*)L"-run-install-now -all-users";
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

static void StartInstallation() {
    // create a progress bar in place of the Options button
    int dx = DpiScale(gHwndFrame, kInstallerWinDx / 2);
    Rect rc(0, 0, dx, gButtonDy);
    rc = MapRectToWindow(rc, gButtonOptions->hwnd, gHwndFrame);

    int nInstallationSteps = gArchive.filesCount;
    nInstallationSteps++; // for copying self
    nInstallationSteps++; // for writing registry entries
    nInstallationSteps++; // to show progress at the beginning

    ProgressCreateArgs args;
    args.initialMax = nInstallationSteps;
    args.parent = gHwndFrame;
    gProgressBar = new Progress();
    gProgressBar->Create(args);
    RECT prc = {rc.x, rc.y, rc.x + rc.dx, rc.y + rc.dy};
    gProgressBar->SetBounds(prc);
    // first one to show progress quickly
    ProgressStep();

    // disable the install button and remove all the installation options
    delete gStaticInstDir;
    delete gEditInstallationDir;
    delete gButtonBrowseDir;

    delete gCheckboxForAllUsers;
    delete gCheckboxRegisterSearchFilter;
    delete gCheckboxRegisterPreviewer;
    delete gButtonOptions;

    gButtonInstall->SetIsEnabled(false);

    SetMsg(_TR("Installation in progress..."), COLOR_MSG_INSTALLATION);
    HwndInvalidate(gHwndFrame);

    hThread = CreateThread(nullptr, 0, InstallerThread, nullptr, 0, nullptr);
}

static void OnButtonOptions();

static void OnButtonInstall() {
    if (gShowOptions) {
        // hide and disable "Options" button during installation
        OnButtonOptions();
    }

    {
        /* if the app is running, we have to kill it so that we can over-write the executable */
        WCHAR* exePath = GetInstalledExePath();
        KillProcessesWithModule(exePath, true);
        str::Free(exePath);
    }

    if (!CheckInstallUninstallPossible()) {
        return;
    }

    WCHAR* userInstallDir = win::GetTextTemp(gEditInstallationDir->hwnd).Get();
    if (!str::IsEmpty(userInstallDir)) {
        str::ReplaceWithCopy(&gCli->installDir, userInstallDir);
    }

    // note: this checkbox isn't created when running inside Wow64
    gCli->withFilter = gCheckboxRegisterSearchFilter && gCheckboxRegisterSearchFilter->IsChecked();
    // note: this checkbox isn't created on Windows 2000 and XP
    gCli->withPreview = gCheckboxRegisterPreviewer && gCheckboxRegisterPreviewer->IsChecked();

    gCli->allUsers = gCheckboxForAllUsers && gCheckboxForAllUsers->IsChecked();

    if (gCli->allUsers && !IsProcessRunningElevated()) {
        RestartElevatedForAllUsers();
        ::ExitProcess(0);
    }
    StartInstallation();
}

static void OnInstallationFinished() {
    delete gButtonInstall;
    delete gProgressBar;

    if (success) {
        CreateButtonRunSumatra(gHwndFrame);
        SetMsg(_TR("Thank you! SumatraPDF has been installed."), COLOR_MSG_OK);
    } else {
        CreateButtonExit(gHwndFrame);
        SetMsg(_TR("Installation failed!"), COLOR_MSG_FAILED);
    }
    gMsgError = firstError;
    HwndInvalidate(gHwndFrame);

    CloseHandle(hThread);

    if (gAutoUpdate && success) {
        // click the Start button
        PostMessageW(gHwndFrame, WM_COMMAND, IDOK, 0);
    }
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
    gShowOptions = !gShowOptions;

    EnableAndShow(gStaticInstDir, gShowOptions);
    EnableAndShow(gEditInstallationDir, gShowOptions);
    EnableAndShow(gButtonBrowseDir, gShowOptions);

    EnableAndShow(gCheckboxForAllUsers, gShowOptions);
    EnableAndShow(gCheckboxRegisterSearchFilter, gShowOptions);
    EnableAndShow(gCheckboxRegisterPreviewer, gShowOptions);

    //[ ACCESSKEY_GROUP Installer
    //[ ACCESSKEY_ALTERNATIVE // ideally, the same accesskey is used for both
    if (gShowOptions) {
        SetButtonTextAndResize(gButtonOptions, _TR("Hide &Options"));
    } else {
        //| ACCESSKEY_ALTERNATIVE
        SetButtonTextAndResize(gButtonOptions, _TR("&Options"));
    }
    //] ACCESSKEY_ALTERNATIVE
    //] ACCESSKEY_GROUP Installer

    Rect rc = ClientRect(gHwndFrame);
    RECT rcTmp = ToRECT(rc);
    InvalidateRect(gHwndFrame, &rcTmp, TRUE);

    gButtonOptions->SetFocus();
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

static bool BrowseForFolder(HWND hwnd, const WCHAR* initialFolder, const WCHAR* caption, WCHAR* buf, DWORD cchBuf) {
    if (buf == nullptr || cchBuf < MAX_PATH) {
        return false;
    }

    BROWSEINFO bi = {nullptr};
    bi.hwndOwner = hwnd;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = caption;
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM)initialFolder;

    LPITEMIDLIST pidlFolder = SHBrowseForFolder(&bi);
    if (!pidlFolder) {
        return false;
    }
    BOOL ok = SHGetPathFromIDList(pidlFolder, buf);
    if (!ok) {
        return false;
    }
    IMalloc* pMalloc = nullptr;
    HRESULT hr = SHGetMalloc(&pMalloc);
    if (SUCCEEDED(hr) && pMalloc) {
        pMalloc->Free(pidlFolder);
        pMalloc->Release();
    }
    return true;
}

static void OnButtonBrowse() {
    WCHAR* installDir = win::GetTextTemp(gEditInstallationDir->hwnd).Get();

    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir)) {
        WCHAR* tmp = path::GetDir(installDir);
        installDir = str::DupTemp(tmp);
        str::Free(tmp);
    }

    WCHAR path[MAX_PATH]{};
    bool ok = BrowseForFolder(gHwndFrame, installDir, _TR("Select the folder where SumatraPDF should be installed:"),
                              path, dimof(path));
    if (!ok) {
        gButtonBrowseDir->SetFocus();
        return;
    }

    AutoFreeWstr installPath = str::Dup(path);
    // force paths that aren't entered manually to end in ...\SumatraPDF
    // to prevent unintended installations into e.g. %ProgramFiles% itself
    const WCHAR* appName = GetAppNameTemp();
    AutoFreeWstr end = str::Join(L"\\", appName);
    if (!str::EndsWithI(path, end)) {
        installPath = path::Join(path, appName);
    }
    gEditInstallationDir->SetText(installPath);
    gEditInstallationDir->SetSelection(0, -1);
    gEditInstallationDir->SetFocus();
}

// bottom-right
static void PositionInstallButton(Button* b) {
    HWND parent = ::GetParent(b->hwnd);
    Rect r = ClientRect(parent);
    Size size = b->GetIdealSize();
    int x = r.dx - size.dx - kInstallerWinMargin;
    int y = r.dy - size.dy - kInstallerWinMargin;
    b->SetBounds({x, y, size.dx, size.dy});
}

// caller needs to str::Free()
static WCHAR* GetInstallationDir(bool forAllUsers) {
    logf(L"GetInstallationDir(forAllUsers=%d)\n", (int)forAllUsers);
    const WCHAR* appName = GetAppNameTemp();
    AutoFreeWstr regPath = GetRegPathUninst(appName);
    AutoFreeWstr dir = LoggedReadRegStr2(regPath, L"InstallLocation");
    if (dir) {
        if (str::EndsWithI(dir, L".exe")) {
            dir.Set(path::GetDir(dir));
        }
        if (!str::IsEmpty(dir.Get()) && dir::Exists(dir)) {
            logf(L"  got '%s' from InstallLocation registry\n", dir.Get());
            return dir.StealData();
        }
    }

    if (forAllUsers) {
        WCHAR* dataDir = GetSpecialFolderTemp(CSIDL_PROGRAM_FILES, true).Get();
        if (dataDir) {
            WCHAR* res = path::Join(dataDir, appName);
            logf(L"  got '%s' by GetSpecialFolderTemp(CSIDL_PROGRAM_FILES)\n", res);
            return res;
        }
    }

    // fall back to %APPLOCALDATA%\SumatraPDF
    WCHAR* dataDir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true).Get();
    if (dataDir) {
        WCHAR* res = path::Join(dataDir, appName);
        logf(L"  got '%s' by GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA)\n", res);
        return res;
    }

    // fall back to C:\ as a last resort
    auto res = str::Join(L"C:\\", appName);
    logf(L"  got %s as last resort\n", res);
    return res;
}

void ForAllUsersStateChanged() {
    bool checked = gCheckboxForAllUsers->IsChecked();
    logf("ForAllUsersStateChanged() to %d\n", (int)checked);
    Button_SetElevationRequiredState(gButtonInstall->hwnd, checked);
    PositionInstallButton(gButtonInstall);
    gCli->allUsers = checked;
    str::Free(gCli->installDir);
    gCli->installDir = GetInstallationDir(gCli->allUsers);
    gEditInstallationDir->SetText(gCli->installDir);
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
static void OnCreateWindow(HWND hwnd) {
    gButtonInstall = CreateDefaultButton(hwnd, _TR("Install SumatraPDF"));
    gButtonInstall->onClicked = OnButtonInstall;
    PositionInstallButton(gButtonInstall);

    Rect r = ClientRect(hwnd);
    gButtonOptions = CreateDefaultButton(hwnd, _TR("&Options"));
    gButtonOptions->onClicked = OnButtonOptions;
    auto btnSize = gButtonOptions->GetIdealSize();
    int x = kInstallerWinMargin;
    int y = r.dy - btnSize.dy - kInstallerWinMargin;
    uint flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW;
    SetWindowPos(gButtonOptions->hwnd, nullptr, x, y, 0, 0, flags);

    gButtonDy = btnSize.dy;
    gBottomPartDy = gButtonDy + (kInstallerWinMargin * 2);

    Size size = TextSizeInHwnd(hwnd, L"Foo");
    int staticDy = size.dy + DpiScale(hwnd, 6);

    y = r.dy - gBottomPartDy;
    int dx = r.dx - (kInstallerWinMargin * 2) - DpiScale(hwnd, 2);

    x += DpiScale(hwnd, 2);

    // build options controls going from the bottom
    y -= (staticDy + kInstallerWinMargin);

    RECT rc;
    // only show this checkbox if the CPU arch of DLL and OS match
    // (assuming that the installer has the same CPU arch as its content!)
    if (IsProcessAndOsArchSame()) {
        // for Windows XP, this means only basic thumbnail support
        const WCHAR* s = _TR("Let Windows show &previews of PDF documents");
        bool isChecked = gCli->withPreview || IsPreviewerInstalled();
        gCheckboxRegisterPreviewer = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        gCheckboxRegisterPreviewer->SetPos(&rc);
        y -= staticDy;

        isChecked = gCli->withFilter || IsSearchFilterInstalled();
        s = _TR("Let Windows Desktop Search &search PDF documents");
        gCheckboxRegisterSearchFilter = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        gCheckboxRegisterSearchFilter->SetPos(&rc);
        y -= staticDy;
    }

    {
        const WCHAR* s = _TR("Install for all users");
        bool isChecked = gCli->allUsers;
        gCheckboxForAllUsers = CreateCheckbox(hwnd, s, isChecked);
        gCheckboxForAllUsers->onCheckStateChanged = ForAllUsersStateChanged;
        rc = {x, y, x + dx, y + staticDy};
        gCheckboxForAllUsers->SetPos(&rc);
        y -= staticDy;
    }

    // a bit more space between text box and checkboxes
    y -= (DpiScale(hwnd, 4) + kInstallerWinMargin);

    const WCHAR* s = L"&...";
    Size btnSize2 = TextSizeInHwnd(hwnd, s);
    btnSize2.dx += DpiScale(hwnd, 4);
    gButtonBrowseDir = CreateDefaultButton(hwnd, s);
    gButtonBrowseDir->onClicked = OnButtonBrowse;
    // btnSize = gButtonBrowseDir->GetIdealSize();
    x = r.dx - kInstallerWinMargin - btnSize2.dx;
    SetWindowPos(gButtonBrowseDir->hwnd, nullptr, x, y, btnSize2.dx, staticDy,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    x = kInstallerWinMargin;
    dx = r.dx - (2 * kInstallerWinMargin) - btnSize2.dx - DpiScale(hwnd, 4);

    EditCreateArgs eargs;
    eargs.parent = hwnd;
    eargs.withBorder = true;
    gEditInstallationDir = new Edit();
    HWND ehwnd = gEditInstallationDir->Create(eargs);
    CrashIf(!ehwnd);

    gEditInstallationDir->SetText(gCli->installDir);
    rc = {x, y, x + dx, y + staticDy};
    gEditInstallationDir->SetBounds(rc);

    y -= staticDy;

    const char* s2 = _TRA("Install SumatraPDF in &folder:");
    rc = {x, y, x + r.dx, y + staticDy};

    StaticCreateArgs args;
    args.parent = hwnd;
    args.text = s2;
    gStaticInstDir = new Static();
    gStaticInstDir->Create(args);
    gStaticInstDir->SetBounds(rc);

    gShowOptions = !gShowOptions;
    OnButtonOptions();

    gButtonInstall->SetFocus();

    if (gAutoUpdate) {
        // click the Install button
        PostMessageW(hwnd, WM_COMMAND, IDOK, 0);
    }
}
//] ACCESSKEY_GROUP Installer

static void CreateMainWindow() {
    AutoFreeWstr title(str::Format(_TR("SumatraPDF %s Installer"), CURR_VERSION_STR));

    DWORD exStyle = 0;
    if (trans::IsCurrLangRtl()) {
        exStyle = WS_EX_LAYOUTRTL;
    }
    const WCHAR* winCls = kInstallerWindowClassName;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = DpiScale(kInstallerWinDx);
    int dy = DpiScale(kInstallerWinDy);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    HMODULE h = GetModuleHandleW(nullptr);
    gHwndFrame = CreateWindowExW(exStyle, winCls, title.Get(), dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
}

static LRESULT CALLBACK WndProcInstallerFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool handled;

    LRESULT res = 0;
    res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    switch (msg) {
        case WM_CREATE:
            OnCreateWindow(hwnd);
            if (gCli->runInstallNow) {
                PostMessageW(gHwndFrame, WM_APP_START_INSTALLATION, 0, 0);
            }
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
            OnPaintFrame(hwnd, gShowOptions);
            break;

        case WM_COMMAND:
            handled = InstallerOnWmCommand(wp);
            if (!handled) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
            break;

        case WM_APP_START_INSTALLATION:
            StartInstallation();
            break;

        case WM_APP_INSTALLATION_FINISHED:
            OnInstallationFinished();
            if (gButtonRunSumatra) {
                gButtonRunSumatra->SetFocus();
            }
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

    FillWndClassEx(wcex, kInstallerWindowClassName, WndProcInstallerFrame);
    auto h = GetModuleHandleW(nullptr);
    WCHAR* resName = MAKEINTRESOURCEW(GetAppIconID());
    wcex.hIcon = LoadIconW(h, resName);

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
        if (dur > 10000 && gButtonInstall && gButtonInstall->IsEnabled()) {
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
            StartLogToFile(installerLogPath, false);
        }
    }
    logf("------------- Starting SumatraPDF installation\n");
    if (!gCli->installDir) {
        gCli->installDir = GetInstallationDir(gCli->allUsers);
    }
    logf(L"Running'%s' installing into dir '%s'\n", GetExePathTemp().Get(), gCli->installDir);

    if (!gCli->silent && MaybeMismatchedOSDialog(nullptr)) {
        return 0;
    }

    gWasSearchFilterInstalled = IsSearchFilterInstalled();
    if (gWasSearchFilterInstalled) {
        log("Search filter is installed\n");
    }
    gWasPreviewInstaller = IsPreviewerInstalled();
    if (gWasPreviewInstaller) {
        log("Previewer is installed\n");
    }

    int ret = 0;

    if (!OpenEmbeddedFilesArchive()) {
        return 1;
    }

    gDefaultMsg = _TR("Thank you for choosing SumatraPDF!");

    if (!gCli->runInstallNow) {
        // use settings from previous installation
        if (!gCli->withFilter) {
            gCli->withFilter = gWasSearchFilterInstalled;
            log("setting gCli->withFilter because search filter installed\n");
        }
        if (!gCli->withPreview) {
            gCli->withPreview = gWasPreviewInstaller;
            log("setting gCli->withPreview because previewer installed\n");
        }
    }

    // unregister search filter and previewer to reduce
    // possibility of blocking the installation because the dlls are loaded
    if (gWasSearchFilterInstalled) {
        UnRegisterSearchFilter(true);
    }
    if (gWasPreviewInstaller) {
        UnRegisterPreviewer(true);
    }

    if (gCli->silent) {
        if (gCli->allUsers && !IsProcessRunningElevated()) {
            log("allUsers but not elevated: re-starting as elevated\n");
            RestartElevatedForAllUsers();
            ::ExitProcess(0);
        }
        InstallerThread(nullptr);
        ret = success ? 0 : 1;
    } else {
        if (!RegisterWinClass()) {
            log("RegisterWinClass() failed\n");
            goto Exit;
        }

        if (!InstanceInit()) {
            log("InstanceInit() failed\n");
            goto Exit;
        }

        BringWindowToTop(gHwndFrame);

        ret = RunApp();
    }

    // re-register if we un-registered but installation was cancelled
    if (gWasSearchFilterInstalled) {
        log("re-registering search filter\n");
        RegisterSearchFilter(true);
    }
    if (gWasPreviewInstaller) {
        log("re-registering previewer\n");
        RegisterPreviewer(true);
    }
    log("Installer finished\n");
    ShowLogFile(installerLogPath);
Exit:
    free(firstError);

    return ret;
}
