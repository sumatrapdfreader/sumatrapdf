/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include <tlhelp32.h>
#include <io.h>

#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/FileUtil.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
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
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "AppUtil.h"
#include "Flags.h"
#include "Resource.h"
#include "Version.h"
#include "Installer.h"

#include "ifilter/PdfFilter.h"
#include "previewer/PdfPreview.h"

// if 1, adds checkbox to register as default PDF viewer
#define ENABLE_REGISTER_DEFAULT 0

struct InstallerGlobals {
#if ENABLE_REGISTER_DEFAULT
    bool registerAsDefault;
#endif
    bool autoUpdate;
};

static InstallerGlobals gInstallerGlobals = {
#if ENABLE_REGISTER_DEFAULT
    false, /* bool registerAsDefault */
#endif
    false, /* bool autoUpdate */
};

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
static CheckboxCtrl* gCheckboxRegisterPdfFilter = nullptr;
static CheckboxCtrl* gCheckboxRegisterPdfPreviewer = nullptr;
static ProgressCtrl* gProgressBar = nullptr;

static HANDLE hThread = nullptr;
static bool success = false;

int currProgress = 0;
static void ProgressStep() {
    if (gIsRaMicroBuild) {
        return;
    }
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

bool ExtractFiles(lzma::SimpleArchive* archive, const WCHAR* destDir) {
    lzma::FileInfo* fi;
    char* uncompressed;

    int nFiles = archive->filesCount;

    for (int i = 0; i < nFiles; i++) {
        fi = &archive->files[i];
        uncompressed = lzma::GetFileDataByIdx(archive, i, nullptr);

        if (!uncompressed) {
            NotifyFailed(
                _TR("The installer has been corrupted. Please download it again.\nSorry for the inconvenience!"));
            return false;
        }
        AutoFreeWstr fileName = strconv::Utf8ToWstr(fi->name);
        AutoFreeWstr filePath = path::Join(destDir, fileName);

        std::string_view d = {uncompressed, fi->uncompressedSize};
        bool ok = file::WriteFile(filePath, d);
        free(uncompressed);

        if (!ok) {
            WCHAR* msg = str::Format(_TR("Couldn't write %s to disk"), filePath.data);
            NotifyFailed(msg);
            free(msg);
            return false;
        }
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

bool CopySelfToDir(const WCHAR* destDir) {
    auto exePath = GetExePath();
    auto exeName = getExeName();
    auto* dstPath = path::Join(destDir, exeName);
    BOOL failIfExists = FALSE;
    BOOL ok = ::CopyFileW(exePath, dstPath, failIfExists);
    free(dstPath);
    return ok;
}

static void CopySettingsFile() {
    // up to 3.1.2 we stored settings in %APPDATA%
    // after that we use %LOCALAPPDATA%
    // copy the settings from old directory
    AutoFreeWstr srcDir = GetSpecialFolder(CSIDL_APPDATA, false);
    AutoFreeWstr dstDir = GetSpecialFolder(CSIDL_LOCAL_APPDATA, false);

    const WCHAR* appName = getAppName();
    WCHAR* prefsFileName = prefs::GetSettingsFileNameNoFree();
    AutoFreeWstr srcPath = path::Join(srcDir.data, appName, prefsFileName);
    AutoFreeWstr dstPath = path::Join(dstDir.data, appName, prefsFileName);

    // don't over-write
    BOOL failIfExists = true;
    // don't care if it fails or not
    ::CopyFileW(srcPath.data, dstPath.data, failIfExists);
}

static bool ExtractInstallerFiles() {
    if (!CreateInstallationDirectory()) {
        return false;
    }

    bool ok = CopySelfToDir(GetInstallDirNoFree());
    if (!ok) {
        return false;
    }
    ProgressStep();

    // on error, ExtractFiles() shows error message itself
    return ExtractFiles(&gArchive, gCli->installDir);
}

/* Caller needs to free() the result. */
static WCHAR* GetDefaultPdfViewer() {
    AutoFreeWstr buf = ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", PROG_ID);
    if (buf) {
        return buf.StealData();
    }
    return ReadRegStr(HKEY_CLASSES_ROOT, L".pdf", nullptr);
}

static bool IsPdfFilterInstalled() {
    const WCHAR* key = L".pdf\\PersistentHandler";
    AutoFreeWstr handler_iid = ReadRegStr(HKEY_CLASSES_ROOT, key, nullptr);
    if (!handler_iid) {
        return false;
    }
    return str::EqI(handler_iid, SZ_PDF_FILTER_HANDLER);
}

static bool IsPdfPreviewerInstalled() {
    const WCHAR* key = L".pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}";
    AutoFreeWstr handler_iid = ReadRegStr(HKEY_CLASSES_ROOT, key, nullptr);
    if (!handler_iid) {
        return false;
    }
    return str::EqI(handler_iid, SZ_PDF_PREVIEW_CLSID);
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(const WCHAR* dir) {
    AutoFreeWstr dirPattern = path::Join(dir, L"*");
    WIN32_FIND_DATA findData;

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

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

    AutoFreeWstr installedExePath = GetInstalledExePath();
    AutoFreeWstr installDate = GetInstallDate();
    AutoFreeWstr installDir = path::GetDir(installedExePath);
    AutoFreeWstr uninstallerPath = GetUninstallerPath();
    AutoFreeWstr uninstallCmdLine = str::Format(L"\"%s\" -uninstall", uninstallerPath.get());

    const WCHAR* appName = getAppName();
    AutoFreeWstr regPathUninst = getRegPathUninst(appName);
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
    ok &= WriteRegStr(hkey, regPathUninst, L"URLUpdateInfo", L"https://www.sumatrapdfreader.org/news.html");

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

// cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
static bool WriteExtendedFileExtensionInfo(HKEY hkey) {
    bool ok = true;

    AutoFreeWstr exePath(GetInstalledExePath());
    if (HKEY_LOCAL_MACHINE == hkey) {
        AutoFreeWstr key = str::Join(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", getExeName());
        ok &= WriteRegStr(hkey, key, nullptr, exePath);
    }
    AutoFreeWstr REG_CLASSES_APPS = getRegClassesApps(getAppName());

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    AutoFreeWstr iconPath(str::Join(exePath, L",1"));
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\DefaultIcon");
        ok &= WriteRegStr(hkey, key, nullptr, iconPath);
    }
    AutoFreeWstr cmdPath(str::Format(L"\"%s\" \"%%1\" %%*", exePath.get()));
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\Open\\Command");
        ok &= WriteRegStr(hkey, key, nullptr, cmdPath);
    }
    AutoFreeWstr printPath(str::Format(L"\"%s\" -print-to-default \"%%1\"", exePath.get()));
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\Print\\Command");
        ok &= WriteRegStr(hkey, key, nullptr, printPath);
    }
    AutoFreeWstr printToPath = str::Format(L"\"%s\" -print-to \"%%2\" \"%%1\"", exePath.get());
    {
        AutoFreeWstr key = str::Join(REG_CLASSES_APPS, L"\\Shell\\PrintTo\\Command");
        ok &= WriteRegStr(hkey, key, nullptr, printToPath);
    }

    // don't add REG_CLASSES_APPS L"\\SupportedTypes", as that prevents SumatraPDF.exe to
    // potentially appear in the Open With lists for other filetypes (such as single images)
    const WCHAR* exeName = getExeName();
    ok &= ListAsDefaultProgramPreWin10(exeName, getSupportedExts(), hkey);

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
}

void onRaMicroInstallerFinished();

static DWORD WINAPI InstallerThread(LPVOID data) {
    UNUSED(data);

    success = false;

    if (!ExtractInstallerFiles()) {
        goto Error;
    }

    CopySettingsFile();

    // all files have been extracted at this point
    if (gCli->justExtractFiles) {
        return 0;
    }

#if ENABLE_REGISTER_DEFAULT
    if (gInstallerGlobals.registerAsDefault) {
        AssociateExeWithPdfExtension();
    }
#endif

    if (!gIsRaMicroBuild) {
        if (gCli->withFilter) {
            InstallPdfFilter();
        } else if (IsPdfFilterInstalled()) {
            UninstallPdfFilter();
        }

        if (gCli->withPreview) {
            InstallPdfPreviewer();
        } else if (IsPdfPreviewerInstalled()) {
            UninstallPdfPreviewer();
        }

        UninstallBrowserPlugin();
    }

    CreateAppShortcuts();

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    success = true;

    if (!WriteUninstallerRegistryInfos()) {
        NotifyFailed(_TR("Failed to write the uninstallation information to the registry"));
    }

    if (!WriteExtendedFileExtensionInfos()) {
        NotifyFailed(_TR("Failed to write the extended file extension information to the registry"));
    }

    const WCHAR* appName = getAppName();
    const WCHAR* exeName = getExeName();
    if (!ListAsDefaultProgramWin10(appName, exeName, getSupportedExts())) {
        NotifyFailed(_TR("Failed to register as default program on win 10"));
    }

    ProgressStep();

Error:
    if (gIsRaMicroBuild) {
        onRaMicroInstallerFinished();
        return 0;
    }
    // TODO: roll back installation on failure (restore previous installation!)
    if (gHwndFrame) {
        if (!gCli->silent) {
            Sleep(500); // allow a glimpse of the completed progress bar before hiding it
            PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
        }
    }
    return 0;
}

static void InvalidateFrame() {
    ClientRect rc(gHwndFrame);
    RECT rcTmp = rc.ToRECT();
    InvalidateRect(gHwndFrame, &rcTmp, FALSE);
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
        KillProcess(exePath, true);
        str::Free(exePath);
    }

    if (!CheckInstallUninstallPossible()) {
        return;
    }

    WCHAR* userInstallDir = win::GetText(gTextboxInstDir->hwnd);
    if (!str::IsEmpty(userInstallDir))
        str::ReplacePtr(&gCli->installDir, userInstallDir);
    free(userInstallDir);

#if ENABLE_REGISTER_DEFAULT
    // note: this checkbox isn't created if we're already registered as default
    //       (in which case we're just going to re-register)
    gInstallerGlobals.registerAsDefault = gCheckboxRegisterDefault == nullptr || gCheckboxRegisterDefault->IsChecked();
#endif

    // note: this checkbox isn't created when running inside Wow64
    gCli->withFilter = gCheckboxRegisterPdfFilter != nullptr && gCheckboxRegisterPdfFilter->IsChecked();
    // note: this checkbox isn't created on Windows 2000 and XP
    gCli->withPreview = gCheckboxRegisterPdfPreviewer != nullptr && gCheckboxRegisterPdfPreviewer->IsChecked();

    // create a progress bar in place of the Options button
    int dx = DpiScale(gHwndFrame, INSTALLER_WIN_DX / 2);
    RectI rc(0, 0, dx, gButtonDy);
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
    delete gCheckboxRegisterPdfFilter;
    delete gCheckboxRegisterPdfPreviewer;
    delete gButtonOptions;

    gButtonInstUninst->SetIsEnabled(false);

    SetMsg(_TR("Installation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    hThread = CreateThread(nullptr, 0, InstallerThread, nullptr, 0, 0);
}

static void OnInstallationFinished() {
    delete gButtonInstUninst;
    delete gProgressBar;

    if (success) {
        CreateButtonRunSumatra(gHwndFrame);
        SetMsg(_TR("Thank you! SumatraPDF has been installed."), COLOR_MSG_OK);
    } else {
        CreateButtonExit(gHwndFrame);
        SetMsg(_TR("Installation failed!"), COLOR_MSG_FAILED);
    }
    gMsgError = gInstUninstGlobals.firstError;
    InvalidateFrame();

    CloseHandle(hThread);

    if (gInstallerGlobals.autoUpdate && success) {
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

static SIZE SetButtonTextAndResize(ButtonCtrl* b, const WCHAR* s) {
    b->SetText(s);
    SIZE size = b->GetIdealSize();
    UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(b->hwnd, nullptr, 0, 0, size.cx, size.cy, flags);
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
    EnableAndShow(gCheckboxRegisterPdfFilter, gShowOptions);
    EnableAndShow(gCheckboxRegisterPdfPreviewer, gShowOptions);

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

    ClientRect rc(gHwndFrame);
    RECT rcTmp = rc.ToRECT();
    InvalidateRect(gHwndFrame, &rcTmp, TRUE);

    gButtonOptions->SetFocus();
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT msg, LPARAM lParam, LPARAM lpData) {
    switch (msg) {
        case BFFM_INITIALIZED:
            if (!str::IsEmpty((WCHAR*)lpData)) {
                SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
            }
            break;

        // disable the OK button for non-filesystem and inaccessible folders (and shortcuts to folders)
        case BFFM_SELCHANGED: {
            WCHAR path[MAX_PATH];
            if (SHGetPathFromIDList((LPITEMIDLIST)lParam, path) && dir::Exists(path)) {
                SHFILEINFO sfi = {0};
                SHGetFileInfo((LPCWSTR)lParam, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_ATTRIBUTES);
                if (!(sfi.dwAttributes & SFGAO_LINK)) {
                    break;
                }
            }
            EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
        } break;
    }

    return 0;
}

static BOOL BrowseForFolder(HWND hwnd, const WCHAR* lpszInitialFolder, const WCHAR* lpszCaption, WCHAR* lpszBuf,
                            DWORD dwBufSize) {
    if (lpszBuf == nullptr || dwBufSize < MAX_PATH) {
        return FALSE;
    }

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
    AutoFreeWstr installDir = win::GetText(gTextboxInstDir->hwnd);

    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir)) {
        WCHAR* tmp = path::GetDir(installDir);
        installDir = tmp;
    }

    WCHAR path[MAX_PATH] = {};
    BOOL ok = BrowseForFolder(gHwndFrame, installDir, _TR("Select the folder where SumatraPDF should be installed:"),
                              path, dimof(path));
    if (!ok) {
        gButtonBrowseDir->SetFocus();
        return;
    }

    AutoFreeWstr installPath = str::Dup(path);
    // force paths that aren't entered manually to end in ...\SumatraPDF
    // to prevent unintended installations into e.g. %ProgramFiles% itself
    const WCHAR* appName = getAppName();
    AutoFreeWstr end = str::Join(L"\\", appName);
    if (!str::EndsWithI(path, end)) {
        installPath = path::Join(path, appName);
    }
    gTextboxInstDir->SetText(installPath);
    gTextboxInstDir->SetSelection(0, -1);
    gTextboxInstDir->SetFocus();
}

static bool InstallerOnWmCommand(WPARAM wParam) {
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
    gButtonInstUninst->onClicked = OnButtonInstall;

    SIZE btnSize;
    gButtonOptions = CreateDefaultButtonCtrl(hwnd, _TR("&Options"));
    gButtonOptions->onClicked = OnButtonOptions;

    btnSize = gButtonOptions->GetIdealSize();
    int x = WINDOW_MARGIN;
    int y = r.dy - btnSize.cy - WINDOW_MARGIN;
    UINT flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW;
    SetWindowPos(gButtonOptions->hwnd, nullptr, x, y, 0, 0, flags);

    gButtonDy = btnSize.cy;
    gBottomPartDy = gButtonDy + (WINDOW_MARGIN * 2);

    SizeI size = TextSizeInHwnd(hwnd, L"Foo");
    int staticDy = size.dy + DpiScale(hwnd, 6);

    y = r.dy - gBottomPartDy;
    int dx = r.dx - (WINDOW_MARGIN * 2) - DpiScale(hwnd, 2);

    x += DpiScale(hwnd, 2);

    // build options controls going from the bottom
    y -= (staticDy + WINDOW_MARGIN);

    AutoFreeWstr defaultViewer(GetDefaultPdfViewer());
    const WCHAR* appName = getAppName();
    BOOL hasOtherViewer = !str::EqI(defaultViewer, appName);
    BOOL isSumatraDefaultViewer = defaultViewer && !hasOtherViewer;

    // only show this checkbox if the CPU arch of DLL and OS match
    // (assuming that the installer has the same CPU arch as its content!)
    if (IsProcessAndOsArchSame()) {
        // for Windows XP, this means only basic thumbnail support
        const WCHAR* s = _TR("Let Windows show &previews of PDF documents");
        bool isChecked = gCli->withPreview || IsPdfPreviewerInstalled();
        gCheckboxRegisterPdfPreviewer = CreateCheckbox(hwnd, s, isChecked);
        rc = {x, y, x + dx, y + staticDy};
        gCheckboxRegisterPdfPreviewer->SetPos(&rc);
        y -= staticDy;

        isChecked = gCli->withFilter || IsPdfFilterInstalled();
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
    y -= (DpiScale(hwnd, 4) + WINDOW_MARGIN);

    const WCHAR* s = L"&...";
    SizeI btnSize2 = TextSizeInHwnd(hwnd, s);
    btnSize.cx += DpiScale(hwnd, 4);
    gButtonBrowseDir = CreateDefaultButtonCtrl(hwnd, s);
    gButtonBrowseDir->onClicked = OnButtonBrowse;
    btnSize = gButtonBrowseDir->GetIdealSize();
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

    gButtonInstUninst->SetFocus();

    if (gInstallerGlobals.autoUpdate) {
        // click the Install button
        PostMessage(hwnd, WM_COMMAND, IDOK, 0);
    }
}
//] ACCESSKEY_GROUP Installer

static void CreateMainWindow() {
    AutoFreeWstr title(str::Format(_TR("SumatraPDF %s Installer"), CURR_VERSION_STR));

    DWORD exStyle = 0;
    if (trans::IsCurrLangRtl()) {
        exStyle = WS_EX_LAYOUTRTL;
    }
    WCHAR* winCls = INSTALLER_FRAME_CLASS_NAME;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = DpiScale(INSTALLER_WIN_DX);
    int dy = DpiScale(INSTALLER_WIN_DY);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    HMODULE h = GetModuleHandleW(nullptr);
    gHwndFrame = CreateWindowExW(exStyle, winCls, title.Get(), dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
}

static WCHAR* GetInstallationDir() {
    const WCHAR* appName = getAppName();
    AutoFreeWstr regPath = getRegPathUninst(appName);
    AutoFreeWstr dir = ReadRegStr2(regPath, L"InstallLocation");
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
        WCHAR* res = path::Join(dataDir, appName);
        str::Free(dataDir);
        return res;
    }

    // fall back to C:\ as a last resort
    return str::Join(L"C:\\", appName);
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
            handled = InstallerOnWmCommand(wParam);
            if (!handled) {
                return DefWindowProc(hwnd, message, wParam, lParam);
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
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static bool RegisterWinClass() {
    WNDCLASSEX wcex{};

    FillWndClassEx(wcex, INSTALLER_FRAME_CLASS_NAME, WndProcFrame);
    auto h = GetModuleHandle(nullptr);
    auto resName = MAKEINTRESOURCEW(getAppIconID());
    wcex.hIcon = LoadIcon(h, resName);

    ATOM atom = RegisterClassEx(&wcex);
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
        if (dur > 10000 && gButtonInstUninst && gButtonInstUninst->IsEnabled()) {
            CheckInstallUninstallPossible(true);
            t = TimeGet();
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

int RunInstallerRaMicro();

int RunInstaller(Flags* cli) {
    RelaunchElevatedIfNotDebug();
    gCli = cli;

    if (gIsRaMicroBuild) {
        return RunInstallerRaMicro();
    }

    int ret = 0;

    if (!OpenEmbeddedFilesArchive()) {
        return 1;
    }

    gDefaultMsg = _TR("Thank you for choosing SumatraPDF!");

    if (!gCli->installDir) {
        gCli->installDir = GetInstallationDir();
    }

    if (gCli->silent) {
        // make sure not to uninstall the plugins during silent installation
        if (!gCli->withFilter) {
            gCli->withFilter = IsPdfFilterInstalled();
        }
        if (!gCli->withPreview) {
            gCli->withPreview = IsPdfPreviewerInstalled();
        }
        InstallerThread(nullptr);
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

Exit:
    free(gInstUninstGlobals.firstError);

    return ret;
}

/* ra-micro installer */

using std::placeholders::_1;

struct RaMicroInstallerWindow {
    HWND hwnd = nullptr;
    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;
    Gdiplus::Bitmap* bmpSplash = nullptr;

    // not owned by us but by mainLayout
    ButtonCtrl* btnInstall = nullptr;
    ButtonCtrl* btnExit = nullptr;
    StaticCtrl* finishedText = nullptr;

    bool finished = false;

    ~RaMicroInstallerWindow();

    void CloseHandler(WindowCloseArgs*);
    void SizeHandler(SizeArgs*);
    void Install();
    void InstallationFinished();
    void Exit();
    void MsgHandler(WndProcArgs*);
};

static RaMicroInstallerWindow* gRaMicroInstallerWindow = nullptr;

RaMicroInstallerWindow::~RaMicroInstallerWindow() {
    delete mainLayout;
    delete mainWindow;
    delete bmpSplash;
}

void RaMicroInstallerWindow::MsgHandler(WndProcArgs* args) {
    if (args->msg == WM_APP_INSTALLATION_FINISHED) {
        InstallationFinished();
        args->didHandle = true;
        return;
    }
}

void RaMicroInstallerWindow::Install() {
    if (finished) {
        if (success) {
            AutoFreeWstr exePath(GetInstalledExePath());
            RunNonElevated(exePath);
        }
        Exit();
        return;
    }
    hThread = CreateThread(nullptr, 0, InstallerThread, nullptr, 0, 0);
}

static Rect layoutAndSize(ILayout* layout, int dx, int dy) {
    if (dx == 0 || dy == 0) {
        return {};
    }
    Size windowSize{dx, dy};
    auto c = Tight(windowSize);
    auto size = layout->Layout(c);
    Point min{0, 0};
    Point max{size.Width, size.Height};
    Rect bounds{min, max};
    layout->SetBounds(bounds);
    return bounds;
}

void RaMicroInstallerWindow::InstallationFinished() {
    CloseHandle(hThread);
    hThread = nullptr;

    finished = true;
    if (success) {
        btnInstall->SetText("Run RA-Micro");
    } else {
        btnInstall->SetText("Exit");
        finishedText->SetText("Installation failed!");
    }
    finishedText->SetIsVisible(true);

    RECT rc = GetClientRect(hwnd);
    int dx = RectDx(rc);
    int dy = RectDy(rc);
    layoutAndSize(mainLayout, dx, dy);
}

void RaMicroInstallerWindow::Exit() {
    gRaMicroInstallerWindow->mainWindow->Close();
}

void RaMicroInstallerWindow::CloseHandler(WindowCloseArgs* args) {
    WindowBase* w = (WindowBase*)gRaMicroInstallerWindow->mainWindow;
    CrashIf(w != args->w);
    delete gRaMicroInstallerWindow;
    gRaMicroInstallerWindow = nullptr;
    PostQuitMessage(0);
}

void RaMicroInstallerWindow::SizeHandler(SizeArgs* args) {
    int dx = args->dx;
    int dy = args->dy;

    layoutAndSize(mainLayout, dx, dy);

    InvalidateRect(args->hwnd, nullptr, false);
    args->didHandle = true;
}

void onRaMicroInstallerFinished() {
    // called on a background thread
    PostMessage(gRaMicroInstallerWindow->hwnd, WM_APP_INSTALLATION_FINISHED, 0, 0);
}

static Gdiplus::Bitmap* LoadRaMicroSplash() {
    std::string_view d = LoadDataResource(IDD_RAMICRO_SPLASH);
    if (d.empty()) {
        return nullptr;
    }
    return BitmapFromData(d.data(), d.size());
}

static bool CreateRaMicroInstallerWindow() {
    HMODULE h = GetModuleHandleW(nullptr);
    LPCWSTR iconName = MAKEINTRESOURCEW(getAppIconID());
    HICON hIcon = LoadIconW(h, iconName);

    auto win = new RaMicroInstallerWindow();
    gRaMicroInstallerWindow = win;

    win->bmpSplash = LoadRaMicroSplash();
    CrashIf(!win->bmpSplash);

    auto w = new Window();
    w->msgFilter = std::bind(&RaMicroInstallerWindow::MsgHandler, win, _1);
    w->hIcon = hIcon;
    // w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->backgroundColor = MkRgb((u8)0xff, (u8)0xff, (u8)0xff);
    w->SetTitle("RA-MICRO Installer");
    int splashDx = (int)win->bmpSplash->GetWidth();
    int splashDy = (int)win->bmpSplash->GetHeight();
    int dx = splashDx + DpiScale(32 + 44); // image + padding
    int dy = splashDy + DpiScale(104);     // image + buttons
    w->initialSize = {dx, dy};
    SIZE winSize = {w->initialSize.Width, w->initialSize.Height};
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
        auto [l, b] = CreateButtonLayout(hwnd, "Install", [win]() { win->Install(); });
        buttons->addChild(l);
        win->btnInstall = b;
    }

    VBox* main = new VBox();
    main->alignMain = MainAxisAlign::SpaceAround;
    main->alignCross = CrossAxisAlign::CrossCenter;

    ImageCtrl* splashCtrl = new ImageCtrl(hwnd);
    splashCtrl->bmp = win->bmpSplash;
    ok = splashCtrl->Create();
    CrashIf(!ok);
    ILayout* splashLayout = NewImageLayout(splashCtrl);
    main->addChild(splashLayout);

    win->finishedText = new StaticCtrl(hwnd);
    win->finishedText->SetText("Installation finished!");
    // TODO: bigger font and maybe bold and different color
    // win->finishedText->SetFont();
    win->finishedText->Create();
    win->finishedText->SetIsVisible(false);
    ILayout* finishedTextLayout = NewStaticLayout(win->finishedText);

    main->addChild(finishedTextLayout);

    main->addChild(buttons);

    auto* padding = new Padding();
    padding->insets = DefaultInsets();
    padding->child = main;
    win->mainLayout = padding;

    w->onClose = std::bind(&RaMicroInstallerWindow::CloseHandler, win, _1);
    w->onSize = std::bind(&RaMicroInstallerWindow::SizeHandler, win, _1);
    w->SetIsVisible(true);
    return true;
}

int RunInstallerRaMicro() {
    int ret = 0;

    if (!OpenEmbeddedFilesArchive()) {
        return 1;
    }
    gDefaultMsg = _TR("Thank you for choosing RA-MICRO PDF!");

    if (!gCli->installDir) {
        gCli->installDir = GetInstallationDir();
    }

    if (gCli->silent) {
        InstallerThread(nullptr);
        ret = success ? 0 : 1;
        goto Exit;
    }

    bool ok = CreateRaMicroInstallerWindow();
    if (!ok) {
        goto Exit;
    }

#if 0
    if (!RegisterWinClass()) {
        goto Exit;
    }

    if (!InstanceInit()) {
        goto Exit;
    }

    BringWindowToTop(gHwndFrame);
#endif

    ret = RunApp();

Exit:
    free(gInstUninstGlobals.firstError);

    return ret;
}
