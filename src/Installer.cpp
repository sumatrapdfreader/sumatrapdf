/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
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
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"
#include "utils/Timer.h"
#include "utils/CmdLineParser.h"
#include "utils/ByteOrderDecoder.h"
#include "utils/LzmaSimpleArchive.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/EditCtrl.h"
#include "wingui/StaticCtrl.h"
#include "wingui/ProgressCtrl.h"

#include "Translations.h"
#include "Resource.h"
#include "Version.h"
#include "Installer.h"

#include "ifilter/PdfFilter.h"
#include "previewer/PdfPreview.h"

// if, 1 adds a way to specify custom installation directory
#define ENABLE_CUSTOM_DIR 0
// if 1, adds checkbox to register as default PDF viewer
#define ENABLE_REGISTER_DEFAULT 0

struct InstallerGlobals {
#if ENABLE_REGISTER_DEFAULT
    bool registerAsDefault;
#endif
    bool installPdfFilter;
    bool installPdfPreviewer;
    bool justExtractFiles;
    bool autoUpdate;
};

static InstallerGlobals gInstallerGlobals = {
#if ENABLE_REGISTER_DEFAULT
    false, /* bool registerAsDefault */
#endif
    false, /* bool installPdfFilter */
    false, /* bool installPdfPreviewer */
    false, /* bool extractFiles */
    false, /* bool autoUpdate */
};

static HBRUSH ghbrBackground = nullptr;

static ButtonCtrl* gButtonOptions = nullptr;
static ButtonCtrl* gButtonRunSumatra = nullptr;
static lzma::SimpleArchive gArchive = {};

#if ENABLE_CUSTOM_DIR
static StaticCtrl* gStaticInstDir = nullptr;
static EditCtrl* gTextboxInstDir = nullptr;
static ButtonCtrl* gButtonBrowseDir = nullptr;
#endif

#if ENABLE_REGISTER_DEFAULT
static CheckboxCtrl* gCheckboxRegisterDefault = nullptr;
#endif
static CheckboxCtrl* gCheckboxRegisterPdfFilter = nullptr;
static CheckboxCtrl* gCheckboxRegisterPdfPreviewer = nullptr;
static ProgressCtrl* gProgressBar = nullptr;

int currProgress = 0;
static inline void ProgressStep() {
    currProgress++;
    if (gProgressBar) {
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

static bool ExtractFiles(lzma::SimpleArchive* archive) {
    lzma::FileInfo* fi;
    char* uncompressed;

    for (int i = 0; i < archive->filesCount; i++) {
        fi = &archive->files[i];
        uncompressed = lzma::GetFileDataByIdx(archive, i, nullptr);
        if (!uncompressed) {
            NotifyFailed(
                _TR("The installer has been corrupted. Please download it again.\nSorry for the inconvenience!"));
            return false;
        }
        AutoFreeW filePath(str::conv::FromUtf8(fi->name));
        AutoFreeW extPath(path::Join(gInstUninstGlobals.installDir, filePath));
        bool ok = file::WriteFile(extPath, uncompressed, fi->uncompressedSize);
        free(uncompressed);
        if (!ok) {
            WCHAR* msg = str::Format(_TR("Couldn't write %s to disk"), filePath);
            NotifyFailed(msg);
            free(msg);
            return false;
        }
        ProgressStep();
    }

    return true;
}

static std::tuple<const char*, DWORD, HGLOBAL> LockDataResource(int id) {
    auto h = GetModuleHandle(nullptr);
    auto name = MAKEINTRESOURCEW(id);
    HRSRC resSrc = FindResourceW(h, name, RT_RCDATA);
    if (!resSrc) {
        return {nullptr, 0, 0};
    }
    HGLOBAL res = LoadResource(nullptr, resSrc);
    if (!res) {
        return {nullptr, 0, 0};
    }

    auto* data = (const char*)LockResource(res);
    DWORD dataSize = SizeofResource(nullptr, resSrc);
    return {data, dataSize, res};
}

static bool CreateInstallationDirectory() {
    bool ok = dir::CreateAll(gInstUninstGlobals.installDir);
    if (!ok) {
        LogLastError();
        NotifyFailed(_TR("Couldn't create the installation directory"));
    }
    return ok;
}

static bool CopySelfToInstallationDir() {
    auto exePath = GetExePath();
    auto* dstPath = path::Join(GetInstallDirNoFree(), L"SumatraPDF.exe");
    BOOL failIfExists = FALSE;
    BOOL ok = ::CopyFileW(exePath, dstPath, failIfExists);
    free(dstPath);
    return ok;
}

#define PREFS_FILE_NAME L"SumatraPDF-settings.txt"
#define APP_NAME L"SumatraPDF"

static void CopySettingsFile() {
    // up to 3.1.2 we stored settings in %APPDATA%
    // after that we use %LOCALAPPDATA%
    // copy the settings from old directory
    WCHAR* srcDir = GetSpecialFolder(CSIDL_APPDATA, false);
    WCHAR* dstDir = GetSpecialFolder(CSIDL_LOCAL_APPDATA, false);

    WCHAR* srcPath = path::Join(srcDir, APP_NAME, PREFS_FILE_NAME);
    WCHAR* dstPath = path::Join(dstDir, APP_NAME, PREFS_FILE_NAME);

    // don't over-write
    BOOL failIfExists = true;
    // don't care if it fails or not
    ::CopyFileW(srcPath, dstPath, failIfExists);
    free(dstPath);
    free(srcPath);
    free(dstDir);
    free(srcDir);
}

static bool ExtractInstallerFiles() {
    if (!CreateInstallationDirectory()) {
        return false;
    }

    bool ok = CopySelfToInstallationDir();
    if (!ok) {
        return false;
    }
    ProgressStep();

    // on error, ExtractFiles() shows error message itself
    return ExtractFiles(&gArchive);
}

/* Caller needs to free() the result. */
static WCHAR* GetDefaultPdfViewer() {
    AutoFreeW buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", PROG_ID));
    if (buf)
        return buf.StealData();
    return ReadRegStr(HKEY_CLASSES_ROOT, L".pdf", nullptr);
}

static bool IsPdfFilterInstalled() {
    const WCHAR* key = L".pdf\\PersistentHandler";
    AutoFreeW handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, key, nullptr));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_FILTER_HANDLER);
}

static bool IsPdfPreviewerInstalled() {
    const WCHAR* key = L".pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}";
    AutoFreeW handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, key, nullptr));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_PREVIEW_CLSID);
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(const WCHAR* dir) {
    AutoFreeW dirPattern(path::Join(dir, L"*"));
    WIN32_FIND_DATA findData;

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    DWORD totalSize = 0;
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            totalSize += findData.nFileSizeLow;
        } else if (!str::Eq(findData.cFileName, L".") && !str::Eq(findData.cFileName, L"..")) {
            AutoFreeW subdir(path::Join(dir, findData.cFileName));
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

    AutoFreeW installedExePath(GetInstalledExePath());
    AutoFreeW installDate(GetInstallDate());
    AutoFreeW installDir(path::GetDir(installedExePath));
    AutoFreeW uninstallCmdLine(str::Format(L"\"%s\" -uninstall", AutoFreeW(GetUninstallerPath())));

    // path to installed executable (or "$path,0" to force the first icon)
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"DisplayIcon", installedExePath);
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"DisplayName", APP_NAME_STR);
    // version format: "1.2"
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"DisplayVersion", CURR_VERSION_STR);
    // Windows XP doesn't allow to view the version number at a glance,
    // so include it in the DisplayName
    if (!IsVistaOrGreater())
        ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"DisplayName", APP_NAME_STR L" " CURR_VERSION_STR);
    DWORD size = GetDirSize(gInstUninstGlobals.installDir) / 1024;
    // size of installed directory after copying files
    ok &= WriteRegDWORD(hkey, REG_PATH_UNINST, L"EstimatedSize", size);
    // current date as YYYYMMDD
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"InstallDate", installDate);
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"InstallLocation", installDir);
    ok &= WriteRegDWORD(hkey, REG_PATH_UNINST, L"NoModify", 1);
    ok &= WriteRegDWORD(hkey, REG_PATH_UNINST, L"NoRepair", 1);
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"Publisher", TEXT(PUBLISHER_STR));
    // command line for uninstaller
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"UninstallString", uninstallCmdLine);
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"URLInfoAbout", L"https://www.sumatrapdfreader.org/");
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"URLUpdateInfo", L"https://www.sumatrapdfreader.org/news.html");

    return ok;
}

static bool WriteUninstallerRegistryInfos() {
    bool ok1 = WriteUninstallerRegistryInfo(HKEY_LOCAL_MACHINE);
    bool ok2 = WriteUninstallerRegistryInfo(HKEY_CURRENT_USER);
    return ok1 || ok2;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144154(v=vs.85).aspx
// http://www.tenforums.com/software-apps/23509-how-add-my-own-program-list-default-programs.html#post407794
static bool WriteWin10Registry(HKEY hkey) {
    bool ok = true;

    ok &= WriteRegStr(hkey, L"SOFTWARE\\RegisteredApplications", L"SumatraPDF", L"SOFTWARE\\SumatraPDF\\Capabilities");
    ok &= WriteRegStr(hkey, L"SOFTWARE\\SumatraPDF\\Capabilities", L"ApplicationDescription",
                      L"SumatraPDF is a PDF reader.");
    ok &= WriteRegStr(hkey, L"SOFTWARE\\SumatraPDF\\Capabilities", L"ApplicationName", L"SumatraPDF Reader");

    for (int i = 0; nullptr != gSupportedExts[i]; i++) {
        WCHAR* ext = gSupportedExts[i];
        ok &= WriteRegStr(hkey, L"SOFTWARE\\SumatraPDF\\Capabilities\\FileAssociations", ext, L"SumatraPDF.exe");
    }
    return ok;
}

static bool ListAsDefaultProgramWin10() {
    bool ok1 = WriteWin10Registry(HKEY_LOCAL_MACHINE);
    bool ok2 = WriteWin10Registry(HKEY_CURRENT_USER);
    return ok1 || ok2;
}

static bool ListAsDefaultProgramPreWin10(HKEY hkey) {
    // add the installed SumatraPDF.exe to the Open With lists of the supported file extensions
    // TODO: per http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx we shouldn't be
    // using OpenWithList but OpenWithProgIds. Also, it doesn't seem to work on my win7 32bit
    // (HKLM\Software\Classes\.mobi\OpenWithList\SumatraPDF.exe key is present but "Open With"
    // menu item doesn't even exist for .mobi files
    // It's not so easy, though, because if we just set it to SumatraPDF,
    // all gSupportedExts will be reported as "PDF Document" by Explorer, so this needs
    // to be more intelligent. We should probably mimic Windows Media Player scheme i.e.
    // set OpenWithProgIds to SumatraPDF.AssocFile.Mobi etc. and create apropriate
    // \SOFTWARE\Classes\CLSID\{GUID}\ProgID etc. entries
    // Also, if Sumatra is the only program handling those docs, our
    // PDF icon will be shown (we need icons and properly configure them)
    bool ok = true;
    for (int i = 0; nullptr != gSupportedExts[i]; i++) {
        WCHAR* ext = gSupportedExts[i];
        WCHAR* name = str::Join(L"Software\\Classes\\", ext, L"\\OpenWithList\\" EXENAME);
        ok &= CreateRegKey(hkey, name);
        free(name);
    }
    return ok;
}

// cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
static bool WriteExtendedFileExtensionInfo(HKEY hkey) {
    bool ok = true;

    AutoFreeW exePath(GetInstalledExePath());
    if (HKEY_LOCAL_MACHINE == hkey) {
        const WCHAR* key = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" EXENAME;
        ok &= WriteRegStr(hkey, key, nullptr, exePath);
    }

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    AutoFreeW iconPath(str::Join(exePath, L",1"));
    ok &= WriteRegStr(hkey, REG_CLASSES_APPS L"\\DefaultIcon", nullptr, iconPath);
    AutoFreeW cmdPath(str::Format(L"\"%s\" \"%%1\" %%*", exePath));
    ok &= WriteRegStr(hkey, REG_CLASSES_APPS L"\\Shell\\Open\\Command", nullptr, cmdPath);
    AutoFreeW printPath(str::Format(L"\"%s\" -print-to-default \"%%1\"", exePath));
    ok &= WriteRegStr(hkey, REG_CLASSES_APPS L"\\Shell\\Print\\Command", nullptr, printPath);
    AutoFreeW printToPath(str::Format(L"\"%s\" -print-to \"%%2\" \"%%1\"", exePath));
    ok &= WriteRegStr(hkey, REG_CLASSES_APPS L"\\Shell\\PrintTo\\Command", nullptr, printToPath);

    // don't add REG_CLASSES_APPS L"\\SupportedTypes", as that prevents SumatraPDF.exe to
    // potentially appear in the Open With lists for other filetypes (such as single images)
    ok &= ListAsDefaultProgramPreWin10(hkey);

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
    AutoFreeW exePath(GetInstalledExePath());
    RunNonElevated(exePath);
    OnButtonExit();
}

static void CreateButtonRunSumatra(HWND hwndParent) {
    gButtonRunSumatra = CreateDefaultButtonCtrl(hwndParent, _TR("Start SumatraPDF"));
    gButtonRunSumatra->OnClicked = OnButtonStartSumatra;
}

static bool CreateAppShortcut(int csidl) {
    AutoFreeW shortcutPath(GetShortcutPath(csidl));
    if (!shortcutPath.Get()) {
        return false;
    }
    AutoFreeW installedExePath(GetInstalledExePath());
    return CreateShortcut(shortcutPath, installedExePath);
}

static int shortcutDirs[] = {CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS, CSIDL_DESKTOP};

static void CreateAppShortcuts() {
    for (size_t i = 0; i < dimof(shortcutDirs); i++) {
        int csidl = shortcutDirs[i];
        CreateAppShortcut(csidl);
    }
}

static DWORD WINAPI InstallerThread(LPVOID data) {
    UNUSED(data);
    gInstUninstGlobals.success = false;

    if (!ExtractInstallerFiles()) {
        goto Error;
    }

    CopySettingsFile();

    // all files have been extracted at this point
    if (gInstallerGlobals.justExtractFiles) {
        return 0;
    }

#if ENABLE_REGISTER_DEFAULT
    if (gInstallerGlobals.registerAsDefault) {
        AssociateExeWithPdfExtension();
    }
#endif

    if (gInstallerGlobals.installPdfFilter) {
        InstallPdfFilter();
    } else if (IsPdfFilterInstalled()) {
        UninstallPdfFilter();
    }

    if (gInstallerGlobals.installPdfPreviewer) {
        InstallPdfPreviewer();
    } else if (IsPdfPreviewerInstalled()) {
        UninstallPdfPreviewer();
    }

    UninstallBrowserPlugin();

    CreateAppShortcuts();

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gInstUninstGlobals.success = true;

    if (!WriteUninstallerRegistryInfos()) {
        NotifyFailed(_TR("Failed to write the uninstallation information to the registry"));
    }

    if (!WriteExtendedFileExtensionInfos()) {
        NotifyFailed(_TR("Failed to write the extended file extension information to the registry"));
    }

    if (!ListAsDefaultProgramWin10()) {
        NotifyFailed(_TR("Failed to register as default program on win 10"));
    }

    ProgressStep();

Error:
    // TODO: roll back installation on failure (restore previous installation!)
    if (gHwndFrame && !gInstUninstGlobals.silent) {
        Sleep(500); // allow a glimpse of the completed progress bar before hiding it
        PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    }
    return 0;
}

static void OnButtonOptions();

static void OnButtonInstall() {
    if (gShowOptions) {
        OnButtonOptions();
    }

    {
        /* if the app is running, we have to kill it so that we can over-write the executable */
        WCHAR* exePath = GetInstalledExePath();
        KillProcess(exePath, true);
        str::Free(exePath);
    }

    if (!CheckInstallUninstallPossible()) {
        return;
    }

#if ENABLE_CUSTOM_DIR
    WCHAR* userInstallDir = win::GetText(gTextboxInstDir->hwnd);
    if (!str::IsEmpty(userInstallDir))
        str::ReplacePtr(&gInstUninstGlobals.installDir, userInstallDir);
    free(userInstallDir);
#endif

#if ENABLE_REGISTER_DEFAULT
    // note: this checkbox isn't created if we're already registered as default
    //       (in which case we're just going to re-register)
    gInstallerGlobals.registerAsDefault = gCheckboxRegisterDefault == nullptr || gCheckboxRegisterDefault->IsChecked();
#endif

    // note: this checkbox isn't created when running inside Wow64
    gInstallerGlobals.installPdfFilter =
        gCheckboxRegisterPdfFilter != nullptr && gCheckboxRegisterPdfFilter->IsChecked();
    // note: this checkbox isn't created on Windows 2000 and XP
    gInstallerGlobals.installPdfPreviewer =
        gCheckboxRegisterPdfPreviewer != nullptr && gCheckboxRegisterPdfPreviewer->IsChecked();

    // create a progress bar in place of the Options button
    RectI rc(0, 0, dpiAdjust(INSTALLER_WIN_DX / 2), gButtonDy);
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

#if ENABLE_CUSTOM_DIR
    // disable the install button and remove all the installation options
    delete gStaticInstDir;
    delete gTextboxInstDir;
    delete gButtonBrowseDir;
#endif

#if ENABLE_REGISTER_DEFAULT
    delete gCheckboxRegisterDefault;
#endif
    delete gCheckboxRegisterPdfFilter;
    delete gCheckboxRegisterPdfPreviewer;
    delete gButtonOptions;

    gButtonInstUninst->SetIsEnabled(false);

    SetMsg(_TR("Installation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    gInstUninstGlobals.hThread = CreateThread(nullptr, 0, InstallerThread, nullptr, 0, 0);
}

static void OnInstallationFinished() {
    delete gButtonInstUninst;
    delete gProgressBar;

    if (gInstUninstGlobals.success) {
        CreateButtonRunSumatra(gHwndFrame);
        SetMsg(_TR("Thank you! SumatraPDF has been installed."), COLOR_MSG_OK);
    } else {
        CreateButtonExit(gHwndFrame);
        SetMsg(_TR("Installation failed!"), COLOR_MSG_FAILED);
    }
    gMsgError = gInstUninstGlobals.firstError;
    InvalidateFrame();

    CloseHandle(gInstUninstGlobals.hThread);

    if (gInstallerGlobals.autoUpdate && gInstUninstGlobals.success) {
        // click the Start button
        PostMessage(gHwndFrame, WM_COMMAND, IDOK, 0);
    }
}

static void EnableAndShow(WindowBase* w, bool enable) {
    if (w) {
        win::SetVisibility(w->hwnd, enable);
        w->SetIsEnabled(enable);
    }
}

static void OnButtonOptions() {
    gShowOptions = !gShowOptions;

#if ENABLE_CUSTOM_DIR
    EnableAndShow(gStaticInstDir, gShowOptions);
    EnableAndShow(gTextboxInstDir, gShowOptions);
    EnableAndShow(gButtonBrowseDir, gShowOptions);
#endif

#if ENABLE_REGISTER_DEFAULT
    EnableAndShow(gCheckboxRegisterDefault, gShowOptions);
#endif
    EnableAndShow(gCheckboxRegisterPdfFilter, gShowOptions);
    EnableAndShow(gCheckboxRegisterPdfPreviewer, gShowOptions);

    //[ ACCESSKEY_GROUP Installer
    //[ ACCESSKEY_ALTERNATIVE // ideally, the same accesskey is used for both
    if (gShowOptions)
        SetButtonTextAndResize(gButtonOptions, _TR("Hide &Options"));
    //| ACCESSKEY_ALTERNATIVE
    else
        SetButtonTextAndResize(gButtonOptions, _TR("&Options"));
    //] ACCESSKEY_ALTERNATIVE
    //] ACCESSKEY_GROUP Installer

    ClientRect rc(gHwndFrame);
    RECT rcTmp = rc.ToRECT();
    InvalidateRect(gHwndFrame, &rcTmp, TRUE);

    gButtonOptions->SetFocus();
}

#if ENABLE_CUSTOM_DIR
static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT msg, LPARAM lParam, LPARAM lpData) {
    switch (msg) {
        case BFFM_INITIALIZED:
            if (!str::IsEmpty((WCHAR*)lpData))
                SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
            break;

        // disable the OK button for non-filesystem and inaccessible folders (and shortcuts to folders)
        case BFFM_SELCHANGED: {
            WCHAR path[MAX_PATH];
            if (SHGetPathFromIDList((LPITEMIDLIST)lParam, path) && dir::Exists(path)) {
                SHFILEINFO sfi = {0};
                SHGetFileInfo((LPCWSTR)lParam, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_ATTRIBUTES);
                if (!(sfi.dwAttributes & SFGAO_LINK))
                    break;
            }
            EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
        } break;
    }

    return 0;
}

static BOOL BrowseForFolder(HWND hwnd, const WCHAR* lpszInitialFolder, const WCHAR* lpszCaption, WCHAR* lpszBuf,
                            DWORD dwBufSize) {
    if (lpszBuf == nullptr || dwBufSize < MAX_PATH)
        return FALSE;

    BROWSEINFO bi = {0};
    bi.hwndOwner = hwnd;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = lpszCaption;
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM)lpszInitialFolder;

    BOOL ok = FALSE;
    LPITEMIDLIST pidlFolder = SHBrowseForFolder(&bi);
    if (pidlFolder) {
        ok = SHGetPathFromIDList(pidlFolder, lpszBuf);

        IMalloc* pMalloc = nullptr;
        if (SUCCEEDED(SHGetMalloc(&pMalloc)) && pMalloc) {
            pMalloc->Free(pidlFolder);
            pMalloc->Release();
        }
    }

    return ok;
}

static void OnButtonBrowse() {
    AutoFreeW installDir(win::GetText(gTextboxInstDir->hwnd));
    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir)) {
        installDir.Set(path::GetDir(installDir));
    }

    WCHAR path[MAX_PATH] = {};
    BOOL ok = BrowseForFolder(gHwndFrame, installDir, _TR("Select the folder where SumatraPDF should be installed:"),
                              path, dimof(path));
    if (!ok) {
        gButtonBrowseDir->SetFocus();
        return;
    }

    WCHAR* installPath = path;
    // force paths that aren't entered manually to end in ...\SumatraPDF
    // to prevent unintended installations into e.g. %ProgramFiles% itself
    if (!str::EndsWithI(path, L"\\" APP_NAME_STR)) {
        installPath = path::Join(path, APP_NAME_STR);
    }
    gTextboxInstDir->SetText(installPath);
    if (installPath != path) {
        free(installPath);
    }
    gTextboxInstDir->SetSelection(0, -1);
    gTextboxInstDir->SetFocus();
}
#endif

static bool OnWmCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
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
    RECT rc;
    ClientRect r(hwnd);

    gButtonInstUninst = CreateDefaultButtonCtrl(hwnd, _TR("Install SumatraPDF"));
    gButtonInstUninst->OnClicked = OnButtonInstall;

    SIZE btnSize;
    gButtonOptions = CreateDefaultButtonCtrl(hwnd, _TR("&Options"));
    gButtonOptions->OnClicked = OnButtonOptions;

    btnSize = gButtonOptions->GetIdealSize();
    int x = WINDOW_MARGIN;
    int y = r.dy - btnSize.cy - WINDOW_MARGIN;
    UINT flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW;
    SetWindowPos(gButtonOptions->hwnd, nullptr, x, y, 0, 0, flags);

    gButtonDy = btnSize.cy;
    gBottomPartDy = gButtonDy + (WINDOW_MARGIN * 2);

    SizeI size = TextSizeInHwnd(hwnd, L"Foo");
    int staticDy = size.dy + dpiAdjust(6);

    y = r.dy - gBottomPartDy;
    int dx = r.dx - (WINDOW_MARGIN * 2) - dpiAdjust(2);

    x += dpiAdjust(2);

    // build options controls going from the bottom
    y -= (staticDy + WINDOW_MARGIN);

    AutoFreeW defaultViewer(GetDefaultPdfViewer());
    BOOL hasOtherViewer = !str::EqI(defaultViewer, APP_NAME_STR);
    BOOL isSumatraDefaultViewer = defaultViewer && !hasOtherViewer;

    // only show this checkbox if the CPU arch of DLL and OS match
    // (assuming that the installer has the same CPU arch as its content!)
    if (IsProcessAndOsArchSame()) {
        // for Windows XP, this means only basic thumbnail support
        const WCHAR* s = _TR("Let Windows show &previews of PDF documents");
        bool isChecked = gInstallerGlobals.installPdfPreviewer || IsPdfPreviewerInstalled();
        gCheckboxRegisterPdfPreviewer = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        gCheckboxRegisterPdfPreviewer->SetPos(&rc);
        y -= staticDy;

        isChecked = gInstallerGlobals.installPdfFilter || IsPdfFilterInstalled();
        s = _TR("Let Windows Desktop Search &search PDF documents");
        gCheckboxRegisterPdfFilter = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        gCheckboxRegisterPdfFilter->SetPos(&rc);
        y -= staticDy;
    }

#if ENABLE_REGISTER_DEFAULT
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
    y -= (dpiAdjust(4) + WINDOW_MARGIN);

#if ENABLE_CUSTOM_DIR
    const WCHAR* s = L"&...";
    SizeI btnSize2 = TextSizeInHwnd(hwnd, s);
    btnSize.cx += dpiAdjust(4);
    gButtonBrowseDir = CreateDefaultButtonCtrl(hwnd, s);
    gButtonBrowseDir->OnClicked = OnButtonBrowse;
    btnSize = gButtonBrowseDir->GetIdealSize();
    x = r.dx - WINDOW_MARGIN - btnSize2.dx;
    SetWindowPos(gButtonBrowseDir->hwnd, nullptr, x, y, btnSize2.dx, staticDy,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    x = WINDOW_MARGIN;
    dx = r.dx - (2 * WINDOW_MARGIN) - btnSize2.dx - dpiAdjust(4);
    gTextboxInstDir = new EditCtrl(hwnd);
    gTextboxInstDir->dwStyle |= WS_BORDER;
    gTextboxInstDir->SetText(gInstUninstGlobals.installDir);
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
#endif

    gShowOptions = !gShowOptions;
    OnButtonOptions();

    gButtonInstUninst->SetFocus();

    if (gInstallerGlobals.autoUpdate) {
        // click the Install button
        PostMessage(hwnd, WM_COMMAND, IDOK, 0);
    }
}
//] ACCESSKEY_GROUP Installer

static void CreateMainWindow() {
    AutoFreeW title(str::Format(_TR("SumatraPDF %s Installer"), CURR_VERSION_STR));

    DWORD exStyle = 0;
    if (trans::IsCurrLangRtl()) {
        exStyle = WS_EX_LAYOUTRTL;
    }
    int dx = dpiAdjust(INSTALLER_WIN_DX);
    int dy = dpiAdjust(INSTALLER_WIN_DY);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    HMODULE h = GetModuleHandleW(nullptr);
    gHwndFrame = CreateWindowExW(exStyle, INSTALLER_FRAME_CLASS_NAME, title.Get(), dwStyle, CW_USEDEFAULT,
                                 CW_USEDEFAULT, dx, dy, nullptr, nullptr, h, nullptr);
}

static void ShowUsage() {
    // Note: translation services aren't initialized at this point, so English only
    MessageBox(nullptr, APP_NAME_STR L"-install.exe [/s][/d <path>][/register][/opt pdffilter,...][/x][/autoupdate]\n\
    \n\
    /s\tinstalls " APP_NAME_STR L" silently (without user interaction).\n\
    /d\tchanges the directory where " APP_NAME_STR L" will be installed.\n\
    /register\tregisters " APP_NAME_STR L" as the default PDF viewer.\n\
    /opt\tenables optional components (currently: pdffilter, pdfpreviewer, plugin).\n\
    /x\tjust extracts the files contained within the installer.\n\
    /autoupdate\tperforms an update with visible UI and minimal user interaction.", APP_NAME_STR L" Installer Usage", MB_OK);
}

using namespace Gdiplus;

static WCHAR* GetInstallationDir() {
    AutoFreeW dir(ReadRegStr2(HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, REG_PATH_UNINST, L"InstallLocation"));
    if (dir) {
        if (str::EndsWithI(dir, L".exe")) {
            dir.Set(path::GetDir(dir));
        }
        if (!str::IsEmpty(dir.Get()) && dir::Exists(dir))
            return dir.StealData();
    }

    // fall back to %APPLOCALDATA%\SumatraPDF
    WCHAR* dataDir = GetSpecialFolder(CSIDL_LOCAL_APPDATA, true);
    if (dataDir) {
        WCHAR* res = path::Join(dataDir, APP_NAME_STR);
        str::Free(dataDir);
        return res;
    }

    // fall back to C:\ as a last resort
    return str::Dup(L"C:\\" APP_NAME_STR);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    bool handled;
    switch (message) {
        case WM_CREATE:
            OnCreateWindow(hwnd);
            break;

        case WM_CTLCOLORSTATIC: {
            if (ghbrBackground == nullptr) {
                ghbrBackground = CreateSolidBrush(RGB(0xff, 0xf2, 0));
            }
            HDC hdc = (HDC)wParam;
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
            handled = OnWmCommand(wParam);
            if (!handled)
                return DefWindowProc(hwnd, message, wParam, lParam);
            break;

        case WM_APP_INSTALLATION_FINISHED:
            OnInstallationFinished();
            if (gButtonRunSumatra)
                gButtonRunSumatra->SetFocus();
            if (gButtonExit) {
                gButtonExit->SetFocus();
            }
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static bool RegisterWinClass() {
    WNDCLASSEX wcex{};

    FillWndClassEx(wcex, INSTALLER_FRAME_CLASS_NAME, WndProcFrame);
    auto h = GetModuleHandle(nullptr);
    auto resName = MAKEINTRESOURCEW(IDI_SUMATRAPDF);
    wcex.hIcon = LoadIcon(h, resName);

    ATOM atom = RegisterClassEx(&wcex);
    CrashIf(!atom);
    return atom != 0;
}

static BOOL InstanceInit() {
    InitInstallerUninstaller();

    CreateMainWindow();
    if (!gHwndFrame)
        return FALSE;

    SetDefaultMsg();

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
static int RunApp() {
    MSG msg;
    FrameTimeoutCalculator ftc(60);
    Timer t;
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
        if (t.GetTimeInMs() > 10000 && gButtonInstUninst && gButtonInstUninst->IsEnabled()) {
            CheckInstallUninstallPossible(true);
            t.Start();
        }
    }
}

static void ParseCommandLine(WCHAR* cmdLine) {
    WStrVec argList;
    ParseCmdLine(cmdLine, argList);

#define is_arg(param) str::EqI(arg + 1, TEXT(param))
#define is_arg_with_param(param) (is_arg(param) && i < argList.size() - 1)

    // skip the first arg (exe path)
    for (size_t i = 1; i < argList.size(); i++) {
        WCHAR* arg = argList.at(i);
        if ('-' != *arg && '/' != *arg)
            continue;

        if (is_arg("s"))
            gInstUninstGlobals.silent = true;
        else if (is_arg_with_param("d"))
            str::ReplacePtr(&gInstUninstGlobals.installDir, argList.at(++i));
#if 0
        else if (is_arg("register"))
            gInstallerGlobals.registerAsDefault = true;
#endif
        else if (is_arg_with_param("opt")) {
            WCHAR* opts = argList.at(++i);
            str::ToLowerInPlace(opts);
            str::TransChars(opts, L" ;", L",,");
            WStrVec optlist;
            optlist.Split(opts, L",", true);
            if (optlist.Contains(L"pdffilter"))
                gInstallerGlobals.installPdfFilter = true;
            if (optlist.Contains(L"pdfpreviewer"))
                gInstallerGlobals.installPdfPreviewer = true;
        } else if (is_arg("x")) {
            gInstallerGlobals.justExtractFiles = true;
            // silently extract files to the current directory (if /d isn't used)
            gInstUninstGlobals.silent = true;
            if (!gInstUninstGlobals.installDir)
                str::ReplacePtr(&gInstUninstGlobals.installDir, L".");
        } else if (is_arg("autoupdate")) {
            gInstallerGlobals.autoUpdate = true;
        } else if (is_arg("h") || is_arg("help") || is_arg("?")) {
            gInstUninstGlobals.showUsageAndQuit = true;
        }
    }
}

static void ShowNoEmbeddedFiles(const WCHAR* msg) {
    const WCHAR* caption = L"Error";
    MessageBoxW(nullptr, msg, caption, MB_OK);
}

static bool OpenEmbeddedFilesArchive() {
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
    return true;
}

int RunInstaller() {
    int ret = 0;

#if !defined(DEBUG)
    if (!IsRunningElevated()) {
        WCHAR* exePath = GetExePath();
        WCHAR* cmdline = GetCommandLineW(); // not owning the memory
        LaunchElevated(exePath, cmdline);
        str::Free(exePath);
        ::ExitProcess(0);
    }
#endif

    if (!OpenEmbeddedFilesArchive()) {
        return 1;
    }

    gDefaultMsg = _TR("Thank you for choosing SumatraPDF!");

    ParseCommandLine(GetCommandLine());
    if (gInstUninstGlobals.showUsageAndQuit) {
        ShowUsage();
        ret = 0;
        goto Exit;
    }

    if (!gInstUninstGlobals.installDir) {
        gInstUninstGlobals.installDir = GetInstallationDir();
    }

    if (gInstUninstGlobals.silent) {
        // make sure not to uninstall the plugins during silent installation
        if (!gInstallerGlobals.installPdfFilter)
            gInstallerGlobals.installPdfFilter = IsPdfFilterInstalled();
        if (!gInstallerGlobals.installPdfPreviewer)
            gInstallerGlobals.installPdfPreviewer = IsPdfPreviewerInstalled();
        InstallerThread(nullptr);
        ret = gInstUninstGlobals.success ? 0 : 1;
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

Exit:
    free(gInstUninstGlobals.installDir);
    free(gInstUninstGlobals.firstError);

    return ret;
}
