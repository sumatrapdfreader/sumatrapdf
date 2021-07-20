/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <tlhelp32.h>
#include <io.h>

#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/FileUtil.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/LogDbg.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"
#include "utils/Timer.h"
#include "utils/CmdLineParser.h"
#include "utils/GdiPlusUtil.h"
#include "utils/ByteOrderDecoder.h"
#include "utils/LzmaSimpleArchive.h"
#include "utils/RegistryPaths.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/EditCtrl.h"
#include "wingui/StaticCtrl.h"
#include "wingui/ProgressCtrl.h"
#include "wingui/ImageCtrl.h"

#include "Translations.h"

#include "AppPrefs.h"
#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "AppUtil.h"
#include "Flags.h"
#include "SumatraPDF.h"
#include "resource.h"
#include "Version.h"
#include "Installer.h"

#include "ifilter/PdfFilterClsid.h"
#include "previewer/PdfPreview.h"

// if 1, adds checkbox to register as default PDF viewer
#define ENABLE_REGISTER_DEFAULT 0

#if ENABLE_REGISTER_DEFAULT
static bool gRegisterAsDefault = false;
#endif

static bool gAutoUpdate = false;
static HBRUSH ghbrBackground = nullptr;

static ButtonCtrl* gButtonOptions = nullptr;
static ButtonCtrl* gButtonRunSumatra = nullptr;
static lzma::SimpleArchive gArchive = {};

static StaticCtrl* gStaticInstDir = nullptr;
static EditCtrl* gTextboxInstDir = nullptr;
static ButtonCtrl* gButtonBrowseDir = nullptr;

#if ENABLE_REGISTER_DEFAULT
static CheckboxCtrl* gCheckboxRegisterDefault = nullptr;
#endif
static CheckboxCtrl* gCheckboxRegisterSearchFilter = nullptr;
static CheckboxCtrl* gCheckboxRegisterPreviewer = nullptr;
static ProgressCtrl* gProgressBar = nullptr;
static ButtonCtrl* gButtonExit = nullptr;
static ButtonCtrl* gButtonInstaller = nullptr;

static HANDLE hThread = nullptr;
static bool success = false;
static bool gWasSearchFilterInstalled = false;
static bool gWasPreviewInstaller = false;

int currProgress = 0;
static void ProgressStep() {
    currProgress++;
    if (gProgressBar) {
        // possibly dangerous as is called on a thread
        gProgressBar->SetCurrent(currProgress);
    }
}

static CheckboxCtrl* CreateCheckbox(HWND hwndParent, const WCHAR* s, bool isChecked) {
    CheckboxCtrl* w = new CheckboxCtrl(hwndParent);
    w->SetText(s);
    w->Create();
    w->SetIsChecked(isChecked);
    return w;
}

static void OnButtonExit() {
    SendMessageW(gHwndFrame, WM_CLOSE, 0, 0);
}

static void CreateButtonExit(HWND hwndParent) {
    gButtonExit = CreateDefaultButtonCtrl(hwndParent, _TR("Close"));
    gButtonExit->onClicked = OnButtonExit;
}

bool ExtractFiles(lzma::SimpleArchive* archive, const WCHAR* destDir) {
    logf(L"ExtractFiles: extracting to dir '%s'\n", destDir);
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

        std::span<u8> d = {uncompressed, fi->uncompressedSize};
        bool ok = file::WriteFile(filePath, d);
        free(uncompressed);

        if (!ok) {
            WCHAR* msg = str::Format(_TR("Couldn't write %s to disk"), filePath.data);
            NotifyFailed(msg);
            str::Free(msg);
            return false;
        }
        logf(L"Extracted '%s'\n", fileName.Get());
        ProgressStep();
    }

    return true;
}

static bool CreateInstallationDirectory() {
    bool ok = dir::CreateAll(gCli->installDir);
    if (!ok) {
        LogLastError();
        NotifyFailed(_TR("Couldn't create the installation directory"));
    }
    return ok;
}

static bool CopySelfToDir(const WCHAR* destDir) {
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
        logf(L"CopySelfToDir: failed to cpoy '%s' to dir '%s'\n", exePath.Get(), destDir);
        return false;
    }
    logf(L"CopySelfToDir: copied '%s' to dir '%s'\n", exePath.Get(), destDir);
    return true;
}

static void CopySettingsFile() {
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
    logf(L"CopySettingsFile: copied '%s' to '%s'\n", srcPath.Get(), dstPath.Get());
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(const WCHAR* dir) {
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

// caller needs to free() the result
static WCHAR* GetInstallDate() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    return str::Format(L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
}

static bool WriteUninstallerRegistryInfo(HKEY hkey) {
    bool ok = true;

    AutoFreeWstr installedExePath = GetInstallationFilePath(GetExeNameTemp());
    AutoFreeWstr installDate = GetInstallDate();
    WCHAR* installDir = GetInstallDirTemp();
    WCHAR* uninstallerPath = installedExePath; // same as
    AutoFreeWstr uninstallCmdLine = str::Format(L"\"%s\" -uninstall", uninstallerPath);

    const WCHAR* appName = GetAppNameTemp();
    AutoFreeWstr regPathUninst = GetRegPathUninst(appName);
    // path to installed executable (or "$path,0" to force the first icon)
    ok &= WriteRegStr(hkey, regPathUninst, L"DisplayIcon", installedExePath);
    ok &= WriteRegStr(hkey, regPathUninst, L"DisplayName", appName);
    // version format: "1.2"
    ok &= WriteRegStr(hkey, regPathUninst, L"DisplayVersion", CURR_VERSION_STR);
    // Windows XP doesn't allow to view the version number at a glance,
    // so include it in the DisplayName
    if (!IsVistaOrGreater()) {
        AutoFreeWstr key = str::Join(appName, L" ", CURR_VERSION_STR);
        ok &= WriteRegStr(hkey, regPathUninst, L"DisplayName", key);
    }
    DWORD size = GetDirSize(gCli->installDir) / 1024;
    // size of installed directory after copying files
    ok &= WriteRegDWORD(hkey, regPathUninst, L"EstimatedSize", size);
    // current date as YYYYMMDD
    ok &= WriteRegStr(hkey, regPathUninst, L"InstallDate", installDate);
    ok &= WriteRegStr(hkey, regPathUninst, L"InstallLocation", installDir);
    ok &= WriteRegDWORD(hkey, regPathUninst, L"NoModify", 1);
    ok &= WriteRegDWORD(hkey, regPathUninst, L"NoRepair", 1);
    ok &= WriteRegStr(hkey, regPathUninst, L"Publisher", TEXT(PUBLISHER_STR));
    // command line for uninstaller
    ok &= WriteRegStr(hkey, regPathUninst, L"UninstallString", uninstallCmdLine);
    ok &= WriteRegStr(hkey, regPathUninst, L"URLInfoAbout", L"https://www.sumatrapdfreader.org/");
    ok &= WriteRegStr(hkey, regPathUninst, L"URLUpdateInfo",
                      L"https://www.sumatrapdfreader.org/docs/Version-history.html");

    return ok;
}

static bool WriteUninstallerRegistryInfos() {
    // we only want to write one of those
    bool ok = WriteUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    if (ok) {
        return true;
    }
    return WriteUninstallerRegistryInfo(HKEY_CURRENT_USER);
}

// http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
static bool WriteExtendedFileExtensionInfo(HKEY hkey) {
    bool ok = true;

    const WCHAR* exeName = GetExeNameTemp();
    AutoFreeWstr exePath = GetInstalledExePath();
    if (HKEY_LOCAL_MACHINE == hkey) {
        AutoFreeWstr key = str::Join(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", exeName);
        ok &= WriteRegStr(hkey, key, nullptr, exePath);
    }
    AutoFreeWstr REG_CLASSES_APPS = GetRegClassesApps(GetAppNameTemp());

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    AutoFreeWstr iconPath = str::Join(exePath, L",1");
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\DefaultIcon");
        ok &= WriteRegStr(hkey, key, nullptr, iconPath);
    }
    AutoFreeWstr cmdPath = str::Format(L"\"%s\" \"%%1\" %%*", exePath.Get());
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\Open\\Command");
        ok &= WriteRegStr(hkey, key, nullptr, cmdPath);
    }
    AutoFreeWstr printPath = str::Format(L"\"%s\" -print-to-default \"%%1\"", exePath.Get());
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\Print\\Command");
        ok &= WriteRegStr(hkey, key, nullptr, printPath);
    }
    AutoFreeWstr printToPath = str::Format(L"\"%s\" -print-to \"%%2\" \"%%1\"", exePath.Get());
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\PrintTo\\Command");
        ok &= WriteRegStr(hkey, key, nullptr, printToPath);
    }

    // don't add REG_CLASSES_APPS L"\\SupportedTypes", as that prevents SumatraPDF.exe to
    // potentially appear in the Open With lists for other filetypes (such as single images)
    ok &= ListAsDefaultProgramPreWin10(exeName, GetSupportedExts(), hkey);

    // in case these values don't exist yet (we won't delete these at uninstallation)
    ok &= WriteRegStr(hkey, REG_CLASSES_PDF, L"Content Type", L"application/pdf");
    const WCHAR* key = L"Software\\Classes\\MIME\\Database\\Content Type\\application/pdf";
    ok &= WriteRegStr(hkey, key, L"Extension", L".pdf");

    return ok;
}

static bool WriteExtendedFileExtensionInfos() {
    bool ok1 = WriteExtendedFileExtensionInfo(HKEY_LOCAL_MACHINE);
    bool ok2 = WriteExtendedFileExtensionInfo(HKEY_CURRENT_USER);
    return ok1 || ok2;
}

static void OnButtonStartSumatra() {
    AutoFreeWstr exePath(GetInstalledExePath());
    RunNonElevated(exePath);
    OnButtonExit();
}

static void CreateButtonRunSumatra(HWND hwndParent) {
    gButtonRunSumatra = CreateDefaultButtonCtrl(hwndParent, _TR("Start SumatraPDF"));
    gButtonRunSumatra->onClicked = OnButtonStartSumatra;
}

static bool CreateAppShortcut(int csidl) {
    AutoFreeWstr shortcutPath = GetShortcutPath(csidl);
    if (!shortcutPath.Get()) {
        return false;
    }
    AutoFreeWstr installedExePath = GetInstalledExePath();
    return CreateShortcut(shortcutPath, installedExePath);
}

static int shortcutDirs[] = {CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS, CSIDL_DESKTOP};

static void CreateAppShortcuts() {
    for (size_t i = 0; i < dimof(shortcutDirs); i++) {
        int csidl = shortcutDirs[i];
        CreateAppShortcut(csidl);
    }
    log("did create app shortcuts\n");
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

#if ENABLE_REGISTER_DEFAULT
    if (gInstallerGlobals.registerAsDefault) {
        AssociateExeWithPdfExtension();
    }
#endif

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

    CreateAppShortcuts();

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

static void InvalidateFrame() {
    RECT rc;
    GetClientRect(gHwndFrame, &rc);
    InvalidateRect(gHwndFrame, &rc, FALSE);
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

    WCHAR* userInstallDir = win::GetTextTemp(gTextboxInstDir->hwnd).Get();
    if (!str::IsEmpty(userInstallDir)) {
        str::ReplaceWithCopy(&gCli->installDir, userInstallDir);
    }

#if ENABLE_REGISTER_DEFAULT
    // note: this checkbox isn't created if we're already registered as default
    //       (in which case we're just going to re-register)
    gInstallerGlobals.registerAsDefault = gCheckboxRegisterDefault == nullptr || gCheckboxRegisterDefault->IsChecked();
#endif

    // note: this checkbox isn't created when running inside Wow64
    gCli->withFilter = gCheckboxRegisterSearchFilter != nullptr && gCheckboxRegisterSearchFilter->IsChecked();
    // note: this checkbox isn't created on Windows 2000 and XP
    gCli->withPreview = gCheckboxRegisterPreviewer != nullptr && gCheckboxRegisterPreviewer->IsChecked();

    // create a progress bar in place of the Options button
    int dx = DpiScale(gHwndFrame, INSTALLER_WIN_DX / 2);
    Rect rc(0, 0, dx, gButtonDy);
    rc = MapRectToWindow(rc, gButtonOptions->hwnd, gHwndFrame);

    int nInstallationSteps = gArchive.filesCount;
    nInstallationSteps++; // for copying self
    nInstallationSteps++; // for writing registry entries
    nInstallationSteps++; // to show progress at the beginning

    gProgressBar = new ProgressCtrl(gHwndFrame, nInstallationSteps);
    gProgressBar->Create();
    RECT prc = {rc.x, rc.y, rc.x + rc.dx, rc.y + rc.dy};
    gProgressBar->SetBounds(prc);
    // first one to show progress quickly
    ProgressStep();

    // disable the install button and remove all the installation options
    delete gStaticInstDir;
    delete gTextboxInstDir;
    delete gButtonBrowseDir;

#if ENABLE_REGISTER_DEFAULT
    delete gCheckboxRegisterDefault;
#endif
    delete gCheckboxRegisterSearchFilter;
    delete gCheckboxRegisterPreviewer;
    delete gButtonOptions;

    gButtonInstaller->SetIsEnabled(false);

    SetMsg(_TR("Installation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    hThread = CreateThread(nullptr, 0, InstallerThread, nullptr, 0, 0);
}

static void OnInstallationFinished() {
    delete gButtonInstaller;
    delete gProgressBar;

    if (success) {
        CreateButtonRunSumatra(gHwndFrame);
        SetMsg(_TR("Thank you! SumatraPDF has been installed."), COLOR_MSG_OK);
    } else {
        CreateButtonExit(gHwndFrame);
        SetMsg(_TR("Installation failed!"), COLOR_MSG_FAILED);
    }
    gMsgError = firstError;
    InvalidateFrame();

    CloseHandle(hThread);

    if (gAutoUpdate && success) {
        // click the Start button
        PostMessageW(gHwndFrame, WM_COMMAND, IDOK, 0);
    }
}

static void EnableAndShow(WindowBase* w, bool enable) {
    if (w) {
        win::SetVisibility(w->hwnd, enable);
        w->SetIsEnabled(enable);
    }
}

static Size SetButtonTextAndResize(ButtonCtrl* b, const WCHAR* s) {
    b->SetText(s);
    Size size = b->GetIdealSize();
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(b->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}

static void OnButtonOptions() {
    gShowOptions = !gShowOptions;

    EnableAndShow(gStaticInstDir, gShowOptions);
    EnableAndShow(gTextboxInstDir, gShowOptions);
    EnableAndShow(gButtonBrowseDir, gShowOptions);

#if ENABLE_REGISTER_DEFAULT
    EnableAndShow(gCheckboxRegisterDefault, gShowOptions);
#endif
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
                SHFILEINFO sfi = {0};
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

    BROWSEINFO bi = {0};
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
    WCHAR* installDir = win::GetTextTemp(gTextboxInstDir->hwnd).Get();

    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir)) {
        WCHAR* tmp = path::GetDir(installDir);
        installDir = str::DupTemp(tmp);
        str::Free(tmp);
    }

    WCHAR path[MAX_PATH] = {};
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
    gTextboxInstDir->SetText(installPath);
    gTextboxInstDir->SetSelection(0, -1);
    gTextboxInstDir->SetFocus();
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

#if ENABLE_REGISTER_DEFAULT
/* Caller needs to free() the result. */
static WCHAR* GetDefaultPdfViewer() {
    AutoFreeWstr buf = ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", PROG_ID);
    if (buf) {
        return buf.StealData();
    }
    return ReadRegStr(HKEY_CLASSES_ROOT, L".pdf", nullptr);
}
#endif

//[ ACCESSKEY_GROUP Installer
static void OnCreateWindow(HWND hwnd) {
    RECT rc;
    Rect r = ClientRect(hwnd);

    gButtonInstaller = CreateDefaultButtonCtrl(hwnd, _TR("Install SumatraPDF"));
    gButtonInstaller->onClicked = OnButtonInstall;

    Size btnSize;
    gButtonOptions = CreateDefaultButtonCtrl(hwnd, _TR("&Options"));
    gButtonOptions->onClicked = OnButtonOptions;

    btnSize = gButtonOptions->GetIdealSize();
    int x = WINDOW_MARGIN;
    int y = r.dy - btnSize.dy - WINDOW_MARGIN;
    uint flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW;
    SetWindowPos(gButtonOptions->hwnd, nullptr, x, y, 0, 0, flags);

    gButtonDy = btnSize.dy;
    gBottomPartDy = gButtonDy + (WINDOW_MARGIN * 2);

    Size size = TextSizeInHwnd(hwnd, L"Foo");
    int staticDy = size.dy + DpiScale(hwnd, 6);

    y = r.dy - gBottomPartDy;
    int dx = r.dx - (WINDOW_MARGIN * 2) - DpiScale(hwnd, 2);

    x += DpiScale(hwnd, 2);

    // build options controls going from the bottom
    y -= (staticDy + WINDOW_MARGIN);

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

#if ENABLE_REGISTER_DEFAULT
    AutoFreeWstr defaultViewer(GetDefaultPdfViewer());
    const WCHAR* appName = GetAppNameTemp();
    BOOL hasOtherViewer = !str::EqI(defaultViewer, appName);

    BOOL isSumatraDefaultViewer = defaultViewer && !hasOtherViewer;
    // only show the checbox if Sumatra is not already a default viewer.
    // the alternative (disabling the checkbox) is more confusing
    if (!isSumatraDefaultViewer) {
        // only check the "Use as default" checkbox when no other PDF viewer
        // is currently selected (not going to intrude)
        bool isChecked = !hasOtherViewer || gInstallerGlobals.registerAsDefault;
        gCheckboxRegisterDefault = CreateCheckbox(hwnd, _TR("Use SumatraPDF as the &default PDF reader"), isChecked);
        rc = {x, y, x + dx, y + staticDy};
        gCheckboxRegisterDefault->SetPos(&rc);
        y -= staticDy;
    }
#endif
    // a bit more space between text box and checkboxes
    y -= (DpiScale(hwnd, 4) + WINDOW_MARGIN);

    const WCHAR* s = L"&...";
    Size btnSize2 = TextSizeInHwnd(hwnd, s);
    btnSize2.dx += DpiScale(hwnd, 4);
    gButtonBrowseDir = CreateDefaultButtonCtrl(hwnd, s);
    gButtonBrowseDir->onClicked = OnButtonBrowse;
    // btnSize = gButtonBrowseDir->GetIdealSize();
    x = r.dx - WINDOW_MARGIN - btnSize2.dx;
    SetWindowPos(gButtonBrowseDir->hwnd, nullptr, x, y, btnSize2.dx, staticDy,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    x = WINDOW_MARGIN;
    dx = r.dx - (2 * WINDOW_MARGIN) - btnSize2.dx - DpiScale(hwnd, 4);
    gTextboxInstDir = new EditCtrl(hwnd);
    gTextboxInstDir->dwStyle |= WS_BORDER;
    gTextboxInstDir->SetText(gCli->installDir);
    gTextboxInstDir->Create();
    rc = {x, y, x + dx, y + staticDy};
    gTextboxInstDir->SetBounds(rc);

    y -= staticDy;

    s = _TR("Install SumatraPDF in &folder:");
    rc = {x, y, x + r.dx, y + staticDy};
    gStaticInstDir = new StaticCtrl(hwnd);
    gStaticInstDir->SetText(s);
    gStaticInstDir->Create();
    gStaticInstDir->SetBounds(rc);

    gShowOptions = !gShowOptions;
    OnButtonOptions();

    gButtonInstaller->SetFocus();

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
    const WCHAR* winCls = INSTALLER_FRAME_CLASS_NAME;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = DpiScale(INSTALLER_WIN_DX);
    int dy = DpiScale(INSTALLER_WIN_DY);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    HMODULE h = GetModuleHandleW(nullptr);
    gHwndFrame = CreateWindowExW(exStyle, winCls, title.Get(), dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
}

static WCHAR* GetInstallationDir() {
    const WCHAR* appName = GetAppNameTemp();
    AutoFreeWstr regPath = GetRegPathUninst(appName);
    AutoFreeWstr dir = ReadRegStr2(regPath, L"InstallLocation");
    if (dir) {
        if (str::EndsWithI(dir, L".exe")) {
            dir.Set(path::GetDir(dir));
        }
        if (!str::IsEmpty(dir.Get()) && dir::Exists(dir)) {
            logf(L"GetInstallationDir: got '%s' from InstallLocation registry\n", dir.Get());
            return dir.StealData();
        }
    }

    // fall back to %APPLOCALDATA%\SumatraPDF
    WCHAR* dataDir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true).Get();
    if (dataDir) {
        WCHAR* res = path::Join(dataDir, appName);
        return res;
    }

    // fall back to C:\ as a last resort
    return str::Join(L"C:\\", appName);
}

static LRESULT CALLBACK WndProcInstallerFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
            handled = InstallerOnWmCommand(wp);
            if (!handled) {
                return DefWindowProc(hwnd, msg, wp, lp);
            }
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

    FillWndClassEx(wcex, INSTALLER_FRAME_CLASS_NAME, WndProcInstallerFrame);
    auto h = GetModuleHandleW(nullptr);
    WCHAR* resName = MAKEINTRESOURCEW(GetAppIconID());
    wcex.hIcon = LoadIconW(h, resName);

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
        if (dur > 10000 && gButtonInstaller && gButtonInstaller->IsEnabled()) {
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
    if (!CreateInstallationDirectory()) {
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

static char* PickInstallerLogPath() {
    TempWstr dir = GetSpecialFolderTemp(CSIDL_LOCAL_APPDATA, true);
    if (!dir.Get()) {
        return nullptr;
    }
    auto dirA = ToUtf8Temp(dir.AsView());
    return path::Join(dirA, "sumatra-install-log.txt", nullptr);
}

static void StartInstallerLogging() {
    char* dir = PickInstallerLogPath();
    if (!dir) {
        return;
    }
    StartLogToFile(dir);
    free(dir);
}

void RelaunchElevatedIfNotDebug() {
    if (gIsDebugBuild) {
        // for easier debugging, we don't require
        // elevation in debug build
        return;
    }
    if (IsProcessRunningElevated()) {
        log("Already running elevated\n");
        return;
    }
    auto exePath = GetExePathTemp();
    WCHAR* cmdLine = GetCommandLineW(); // not owning the memory
    logf(L"Re-launching '%s' with args '%s' as elevated\n", exePath.Get(), cmdLine);
    LaunchElevated(exePath, cmdLine);
    ::ExitProcess(0);
}

int RunInstaller() {
    if (gCli->log) {
        StartInstallerLogging();
    }
    if (!gCli->installDir) {
        gCli->installDir = GetInstallationDir();
    }
    logf(L"RunInstaller: '%s' installing into dir '%s'\n", GetExePathTemp().Get(), gCli->installDir);

    RelaunchElevatedIfNotDebug();

    if (!gCli->silent && !IsProcessAndOsArchSame()) {
        logf("Mismatch of the OS and executable arch\n");
        const char* msg =
            "You're installing 32-bit SumatraPDF on 64-bit OS. Are you sure?\nPress 'Ok' to proceed.\nPress 'Cancel' "
            "to download 64-bit version.";
        uint flags = MB_ICONERROR | MB_OK | MB_OKCANCEL;
        flags |= MB_SETFOREGROUND | MB_TOPMOST;
        int res = MessageBoxA(nullptr, msg, "SumatraPDF Installer", flags);
        if (IDCANCEL == res) {
            LaunchBrowser(L"https://www.sumatrapdfreader.org/download-free-pdf-viewer.html");
            return 0;
        }
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

    if (!gCli->withFilter) {
        gCli->withFilter = IsSearchFilterInstalled();
        log("setting gCli->withFilter because search filter installed\n");
    }
    if (!gCli->withPreview) {
        gCli->withPreview = IsPreviewerInstalled();
        log("setting gCli->withPreview because previewer installed\n");
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
        log("Silent installation\n");
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
Exit:
    free(firstError);

    return ret;
}
