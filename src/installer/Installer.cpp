/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/*
The installer is good enough for production but it doesn't mean it couldn't be improved:
 * some more fanciful animations e.g.:
 * letters could drop down and back up when cursor is over it
 * messages could scroll-in
 * some background thing could be going on, e.g. a spinning 3d cube
 * show fireworks on successful installation/uninstallation
*/

// define to allow testing crash handling via -crash cmd-line option
#define ENABLE_CRASH_TESTING

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include <tlhelp32.h>
#include <io.h>
#include "utils/FileUtil.h"
#include "Translations.h"
#include "Resource.h"
#include "utils/Timer.h"
#include "Version.h"
#include "utils/WinUtil.h"
#include "Installer.h"
#include "utils/CmdLineParser.h"
#include "CrashHandler.h"
#include "utils/Dpi.h"
#include "utils/FrameTimeoutCalculator.h"
#include "utils/DebugLog.h"

#include "utils/ByteOrderDecoder.h"
#include "utils/LzmaSimpleArchive.h"

#include "../ifilter/PdfFilter.h"
#include "../previewer/PdfPreview.h"

struct InstallerGlobals {
    bool registerAsDefault;
    bool installPdfFilter;
    bool installPdfPreviewer;
    bool keepBrowserPlugin;
    bool justExtractFiles;
    bool autoUpdate;
};

#define ID_CHECKBOX_MAKE_DEFAULT 14
#define ID_CHECKBOX_BROWSER_PLUGIN 15
#define ID_BUTTON_START_SUMATRA 16
#define ID_BUTTON_OPTIONS 17
#define ID_BUTTON_BROWSE 18
#define ID_CHECKBOX_PDF_FILTER 19
#define ID_CHECKBOX_PDF_PREVIEWER 20

static InstallerGlobals gInstallerGlobals = {
    false, /* bool registerAsDefault */
    false, /* bool installPdfFilter */
    false, /* bool installPdfPreviewer */
    true,  /* bool keepBrowserPlugin */
    false, /* bool extractFiles */
    false, /* bool autoUpdate */
};

static HWND gHwndButtonOptions = nullptr;
static HWND gHwndButtonRunSumatra = nullptr;
static HWND gHwndStaticInstDir = nullptr;
static HWND gHwndTextboxInstDir = nullptr;
static HWND gHwndButtonBrowseDir = nullptr;
static HWND gHwndCheckboxRegisterDefault = nullptr;
static HWND gHwndCheckboxRegisterPdfFilter = nullptr;
static HWND gHwndCheckboxRegisterPdfPreviewer = nullptr;
static HWND gHwndCheckboxKeepBrowserPlugin = nullptr;
static HWND gHwndProgressBar = nullptr;

static int GetInstallationStepCount() {
    /* Installation steps
     * - Create directory
     * - One per file to be copied (count extracted from gPayloadData)
     * - Optional registration (default viewer, browser plugin),
     *   Shortcut and Registry keys
     *
     * Most time is taken by file extraction/copying, so we just add
     * one step before - so that we start with some initial progress
     * - and one step afterwards.
     */
    int count = 2;
    for (int i = 0; nullptr != gPayloadData[i].fileName; i++) {
        if (gPayloadData[i].install)
            count++;
    }
    return count;
}

static inline void ProgressStep() {
    if (gHwndProgressBar)
        PostMessage(gHwndProgressBar, PBM_STEPIT, 0, 0);
}

static bool ExtractFiles(lzma::SimpleArchive* archive) {
    lzma::FileInfo* fi;
    char* uncompressed;

    for (int i = 0; gPayloadData[i].fileName; i++) {
        if (!gPayloadData[i].install)
            continue;
        int idx = lzma::GetIdxFromName(archive, gPayloadData[i].fileName);
        if (-1 == idx) {
            NotifyFailed(_TR("Some files to be installed are damaged or missing"));
            return false;
        }

        fi = &archive->files[idx];
        uncompressed = lzma::GetFileDataByIdx(archive, idx, nullptr);
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
            AutoFreeW msg(str::Format(_TR("Couldn't write %s to disk"), filePath));
            NotifyFailed(msg);
            return false;
        }
        file::SetModificationTime(extPath, fi->ftModified);

        ProgressStep();
    }

    return true;
}

// TODO: also check if valid lzma::ParseSimpleArchive()
// TODO: use it early in installer to show an error message
#if 0
static bool IsValidInstaller()
{
  HRSRC resSrc = FindResource(GetModuleHandle(nullptr), MAKEINTRESOURCE(1), RT_RCDATA);
  return resSrc != nullptr;
}
#endif

static bool InstallCopyFiles() {
    bool ok;
    HGLOBAL res = 0;
    defer {
        if (res != 0)
            UnlockResource(res);
    };

    HRSRC resSrc = FindResource(GetModuleHandle(nullptr), MAKEINTRESOURCE(1), RT_RCDATA);
    if (!resSrc) {
        goto Corrupted;
    }
    res = LoadResource(nullptr, resSrc);
    if (!res) {
        goto Corrupted;
    }

    const char* data = (const char*)LockResource(res);
    DWORD dataSize = SizeofResource(nullptr, resSrc);

    lzma::SimpleArchive archive;
    ok = lzma::ParseSimpleArchive(data, dataSize, &archive);
    if (!ok) {
        goto Corrupted;
    }

    // on error, ExtractFiles() shows error message itself
    ok = ExtractFiles(&archive);
    return ok;
Corrupted:
    NotifyFailed(_TR("The installer has been corrupted. Please download it again.\nSorry for the inconvenience!"));
    return false;
}

/* Caller needs to free() the result. */
static WCHAR* GetDefaultPdfViewer() {
    AutoFreeW buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", PROG_ID));
    if (buf)
        return buf.StealData();
    return ReadRegStr(HKEY_CLASSES_ROOT, L".pdf", nullptr);
}

static bool IsBrowserPluginInstalled() {
    AutoFreeW dllPath(GetInstalledBrowserPluginPath());
    return file::Exists(dllPath);
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
    AutoFreeW uninstallCmdLine(str::Format(L"\"%s\"", AutoFreeW(GetUninstallerPath())));

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
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"URLInfoAbout", L"http://www.sumatrapdfreader.org/");
    ok &= WriteRegStr(hkey, REG_PATH_UNINST, L"URLUpdateInfo", L"http://www.sumatrapdfreader.org/news.html");

    return ok;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/cc144154(v=vs.85).aspx
// http://www.tenforums.com/software-apps/23509-how-add-my-own-program-list-default-programs.html#post407794
static bool ListAsDefaultProgramWin10() {
    HKEY hkey = HKEY_LOCAL_MACHINE;
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
        AutoFreeW keyname(str::Join(L"Software\\Classes\\", gSupportedExts[i], L"\\OpenWithList\\" EXENAME));
        ok &= CreateRegKey(hkey, keyname);
    }
    return ok;
}

// cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
static bool WriteExtendedFileExtensionInfo(HKEY hkey) {
    bool ok = true;

    AutoFreeW exePath(GetInstalledExePath());
    if (HKEY_LOCAL_MACHINE == hkey)
        ok &= WriteRegStr(hkey, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" EXENAME, nullptr, exePath);

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
    ok &= WriteRegStr(hkey, L"Software\\Classes\\MIME\\Database\\Content Type\\application/pdf", L"Extension", L".pdf");

    return ok;
}

static bool CreateInstallationDirectory() {
    bool ok = dir::CreateAll(gInstUninstGlobals.installDir);
    if (!ok) {
        LogLastError();
        NotifyFailed(_TR("Couldn't create the installation directory"));
    }
    return ok;
}

static void CreateButtonRunSumatra(HWND hwndParent) {
    gHwndButtonRunSumatra = CreateDefaultButton(hwndParent, _TR("Start SumatraPDF"), ID_BUTTON_START_SUMATRA);
}

static bool CreateAppShortcut(bool allUsers) {
    AutoFreeW shortcutPath(GetShortcutPath(allUsers));
    if (!shortcutPath.Get())
        return false;
    AutoFreeW installedExePath(GetInstalledExePath());
    return CreateShortcut(shortcutPath, installedExePath);
}

static DWORD WINAPI InstallerThread(LPVOID data) {
    UNUSED(data);
    gInstUninstGlobals.success = false;

    if (!CreateInstallationDirectory())
        goto Error;
    ProgressStep();

    if (!InstallCopyFiles())
        goto Error;
    // all files have been extracted at this point
    if (gInstallerGlobals.justExtractFiles)
        return 0;

    if (gInstallerGlobals.registerAsDefault) {
        // need to sublaunch SumatraPDF.exe instead of replicating the code
        // because registration uses translated strings
        AutoFreeW installedExePath(GetInstalledExePath());
        CreateProcessHelper(installedExePath, L"-register-for-pdf");
    }

    if (gInstallerGlobals.installPdfFilter)
        InstallPdfFilter();
    else if (IsPdfFilterInstalled())
        UninstallPdfFilter();

    if (gInstallerGlobals.installPdfPreviewer)
        InstallPdfPreviewer();
    else if (IsPdfPreviewerInstalled())
        UninstallPdfPreviewer();

    if (!gInstallerGlobals.keepBrowserPlugin)
        UninstallBrowserPlugin();

    if (!CreateAppShortcut(true) && !CreateAppShortcut(false)) {
        NotifyFailed(_TR("Failed to create a shortcut"));
        goto Error;
    }

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gInstUninstGlobals.success = true;

    if (!WriteUninstallerRegistryInfo(HKEY_LOCAL_MACHINE) && !WriteUninstallerRegistryInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_TR("Failed to write the uninstallation information to the registry"));
    }
    if (!WriteExtendedFileExtensionInfo(HKEY_LOCAL_MACHINE) && !WriteExtendedFileExtensionInfo(HKEY_CURRENT_USER)) {
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

static bool IsCheckboxChecked(HWND hwnd) {
    return (Button_GetState(hwnd) & BST_CHECKED) == BST_CHECKED;
}

static void OnButtonInstall() {
    CrashAlwaysIf(gForceCrash);

    if (gShowOptions)
        OnButtonOptions();

    KillSumatra();

    if (!CheckInstallUninstallPossible())
        return;

    WCHAR* userInstallDir = win::GetText(gHwndTextboxInstDir);
    if (!str::IsEmpty(userInstallDir))
        str::ReplacePtr(&gInstUninstGlobals.installDir, userInstallDir);
    free(userInstallDir);

    // note: this checkbox isn't created if we're already registered as default
    //       (in which case we're just going to re-register)
    gInstallerGlobals.registerAsDefault =
        gHwndCheckboxRegisterDefault == nullptr || IsCheckboxChecked(gHwndCheckboxRegisterDefault);

    // note: this checkbox isn't created when running inside Wow64
    gInstallerGlobals.installPdfFilter =
        gHwndCheckboxRegisterPdfFilter != nullptr && IsCheckboxChecked(gHwndCheckboxRegisterPdfFilter);
    // note: this checkbox isn't created on Windows 2000 and XP
    gInstallerGlobals.installPdfPreviewer =
        gHwndCheckboxRegisterPdfPreviewer != nullptr && IsCheckboxChecked(gHwndCheckboxRegisterPdfPreviewer);
    // note: this checkbox isn't created if the browser plugin hasn't been installed before
    gInstallerGlobals.keepBrowserPlugin =
        gHwndCheckboxKeepBrowserPlugin != nullptr && IsCheckboxChecked(gHwndCheckboxKeepBrowserPlugin);

    // create a progress bar in place of the Options button
    RectI rc(0, 0, dpiAdjust(INSTALLER_WIN_DX / 2), gButtonDy);
    rc = MapRectToWindow(rc, gHwndButtonOptions, gHwndFrame);
    gHwndProgressBar = CreateWindow(PROGRESS_CLASS, nullptr, WS_CHILD | WS_VISIBLE, rc.x, rc.y, rc.dx, rc.dy,
                                    gHwndFrame, 0, GetModuleHandle(nullptr), nullptr);
    SendMessage(gHwndProgressBar, PBM_SETRANGE32, 0, GetInstallationStepCount());
    SendMessage(gHwndProgressBar, PBM_SETSTEP, 1, 0);

    // disable the install button and remove all the installation options
    SafeDestroyWindow(&gHwndStaticInstDir);
    SafeDestroyWindow(&gHwndTextboxInstDir);
    SafeDestroyWindow(&gHwndButtonBrowseDir);
    SafeDestroyWindow(&gHwndCheckboxRegisterDefault);
    SafeDestroyWindow(&gHwndCheckboxRegisterPdfFilter);
    SafeDestroyWindow(&gHwndCheckboxRegisterPdfPreviewer);
    SafeDestroyWindow(&gHwndCheckboxKeepBrowserPlugin);
    SafeDestroyWindow(&gHwndButtonOptions);

    EnableWindow(gHwndButtonInstUninst, FALSE);

    SetMsg(_TR("Installation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    gInstUninstGlobals.hThread = CreateThread(nullptr, 0, InstallerThread, nullptr, 0, 0);
}

static void OnInstallationFinished() {
    SafeDestroyWindow(&gHwndButtonInstUninst);
    SafeDestroyWindow(&gHwndProgressBar);

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

static void OnButtonStartSumatra() {
    AutoFreeW exePath(GetInstalledExePath());
    RunNonElevated(exePath);
    OnButtonExit();
}

static void EnableAndShow(HWND hwnd, bool enable) {
    if (!hwnd)
        return;
    win::SetVisibility(hwnd, enable);
    EnableWindow(hwnd, enable);
}

static void OnButtonOptions() {
    gShowOptions = !gShowOptions;

    EnableAndShow(gHwndStaticInstDir, gShowOptions);
    EnableAndShow(gHwndTextboxInstDir, gShowOptions);
    EnableAndShow(gHwndButtonBrowseDir, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterDefault, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterPdfFilter, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterPdfPreviewer, gShowOptions);
    EnableAndShow(gHwndCheckboxKeepBrowserPlugin, gShowOptions);

    //[ ACCESSKEY_GROUP Installer
    //[ ACCESSKEY_ALTERNATIVE // ideally, the same accesskey is used for both
    if (gShowOptions)
        SetButtonTextAndResize(gHwndButtonOptions, _TR("Hide &Options"));
    //| ACCESSKEY_ALTERNATIVE
    else
        SetButtonTextAndResize(gHwndButtonOptions, _TR("&Options"));
    //] ACCESSKEY_ALTERNATIVE
    //] ACCESSKEY_GROUP Installer

    ClientRect rc(gHwndFrame);
    RECT rcTmp = rc.ToRECT();
    InvalidateRect(gHwndFrame, &rcTmp, TRUE);

    SetFocus(gHwndButtonOptions);
}

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
    AutoFreeW installDir(win::GetText(gHwndTextboxInstDir));
    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir))
        installDir.Set(path::GetDir(installDir));

    WCHAR path[MAX_PATH];
    BOOL ok = BrowseForFolder(gHwndFrame, installDir, _TR("Select the folder where SumatraPDF should be installed:"),
                              path, dimof(path));
    if (!ok) {
        SetFocus(gHwndButtonBrowseDir);
        return;
    }

    WCHAR* installPath = path;
    // force paths that aren't entered manually to end in ...\SumatraPDF
    // to prevent unintended installations into e.g. %ProgramFiles% itself
    if (!str::EndsWithI(path, L"\\" APP_NAME_STR))
        installPath = path::Join(path, APP_NAME_STR);
    win::SetText(gHwndTextboxInstDir, installPath);
    Edit_SetSel(gHwndTextboxInstDir, 0, -1);
    SetFocus(gHwndTextboxInstDir);
    if (installPath != path)
        free(installPath);
}

static bool OnWmCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case IDOK:
            if (gHwndButtonInstUninst)
                OnButtonInstall();
            else if (gHwndButtonRunSumatra)
                OnButtonStartSumatra();
            else if (gHwndButtonExit)
                OnButtonExit();
            break;

        case ID_BUTTON_START_SUMATRA:
            OnButtonStartSumatra();
            break;

        case ID_BUTTON_OPTIONS:
            OnButtonOptions();
            break;

        case ID_BUTTON_BROWSE:
            OnButtonBrowse();
            break;

        case ID_BUTTON_EXIT:
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
    ClientRect r(hwnd);
    gHwndButtonInstUninst = CreateDefaultButton(hwnd, _TR("Install SumatraPDF"), IDOK);

    SIZE btnSize;
    gHwndButtonOptions = CreateButton(hwnd, _TR("&Options"), ID_BUTTON_OPTIONS, BS_PUSHBUTTON, btnSize);
    int x = WINDOW_MARGIN;
    int y = r.dy - btnSize.cy - WINDOW_MARGIN;
    SetWindowPos(gHwndButtonOptions, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    gButtonDy = btnSize.cy;
    gBottomPartDy = gButtonDy + (WINDOW_MARGIN * 2);

    SizeI size = TextSizeInHwnd(hwnd, L"Foo");
    int staticDy = size.dy + dpiAdjust(4);

    y = r.dy - gBottomPartDy;
    int dx = r.dx - (WINDOW_MARGIN * 2) - dpiAdjust(2);

    x += dpiAdjust(2);

    // build options controls going from the bottom
    y -= (staticDy + WINDOW_MARGIN);

    AutoFreeW defaultViewer(GetDefaultPdfViewer());
    BOOL hasOtherViewer = !str::EqI(defaultViewer, APP_NAME_STR);
    BOOL isSumatraDefaultViewer = defaultViewer && !hasOtherViewer;

    // only show this checkbox if the browser plugin has been installed before
    if (IsBrowserPluginInstalled()) {
        gHwndCheckboxKeepBrowserPlugin =
            CreateWindowExW(0, WC_BUTTON, _TR("Keep the PDF &browser plugin installed (no longer supported)"),
                            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP, x, y, dx, staticDy, hwnd,
                            (HMENU)ID_CHECKBOX_BROWSER_PLUGIN, GetModuleHandle(nullptr), nullptr);
        SetWindowFont(gHwndCheckboxKeepBrowserPlugin, gFontDefault, TRUE);
        Button_SetCheck(gHwndCheckboxKeepBrowserPlugin, gInstallerGlobals.keepBrowserPlugin);
        y -= staticDy;
    }

    // only show this checkbox if the CPU arch of DLL and OS match
    // (assuming that the installer has the same CPU arch as its content!)
    if (IsProcessAndOsArchSame()) {
        // for Windows XP, this means only basic thumbnail support
        gHwndCheckboxRegisterPdfPreviewer = CreateWindowExW(
            0, WC_BUTTON, _TR("Let Windows show &previews of PDF documents"), WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            x, y, dx, staticDy, hwnd, (HMENU)ID_CHECKBOX_PDF_PREVIEWER, GetModuleHandle(nullptr), nullptr);
        SetWindowFont(gHwndCheckboxRegisterPdfPreviewer, gFontDefault, TRUE);
        Button_SetCheck(gHwndCheckboxRegisterPdfPreviewer,
                        gInstallerGlobals.installPdfPreviewer || IsPdfPreviewerInstalled());
        y -= staticDy;

        gHwndCheckboxRegisterPdfFilter =
            CreateWindowEx(0, WC_BUTTON, _TR("Let Windows Desktop Search &search PDF documents"),
                           WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP, x, y, dx, staticDy, hwnd,
                           (HMENU)ID_CHECKBOX_PDF_FILTER, GetModuleHandle(nullptr), nullptr);
        SetWindowFont(gHwndCheckboxRegisterPdfFilter, gFontDefault, TRUE);
        Button_SetCheck(gHwndCheckboxRegisterPdfFilter, gInstallerGlobals.installPdfFilter || IsPdfFilterInstalled());
        y -= staticDy;
    }

    // only show the checbox if Sumatra is not already a default viewer.
    // the alternative (disabling the checkbox) is more confusing
    if (!isSumatraDefaultViewer) {
        gHwndCheckboxRegisterDefault = CreateWindowExW(
            0, WC_BUTTON, _TR("Use SumatraPDF as the &default PDF reader"), WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP, x,
            y, dx, staticDy, hwnd, (HMENU)ID_CHECKBOX_MAKE_DEFAULT, GetModuleHandle(nullptr), nullptr);
        SetWindowFont(gHwndCheckboxRegisterDefault, gFontDefault, TRUE);
        // only check the "Use as default" checkbox when no other PDF viewer
        // is currently selected (not going to intrude)
        Button_SetCheck(gHwndCheckboxRegisterDefault, !hasOtherViewer || gInstallerGlobals.registerAsDefault);
        y -= staticDy;
    }
    // a bit more space between text box and checkboxes
    y -= (dpiAdjust(4) + WINDOW_MARGIN);

    const WCHAR* s = L"&...";
    SizeI btnSize2 = TextSizeInHwnd(hwnd, s);
    btnSize.cx += dpiAdjust(4);
    gHwndButtonBrowseDir = CreateButton(hwnd, s, ID_BUTTON_BROWSE, BS_PUSHBUTTON, btnSize);
    x = r.dx - WINDOW_MARGIN - btnSize2.dx;
    SetWindowPos(gHwndButtonBrowseDir, nullptr, x, y, btnSize2.dx, staticDy,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    x = WINDOW_MARGIN;
    dx = r.dx - (2 * WINDOW_MARGIN) - btnSize2.dx - dpiAdjust(4);
    gHwndTextboxInstDir = CreateWindowExW(0, WC_EDIT, gInstUninstGlobals.installDir,
                                          WS_CHILD | WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, x, y, dx,
                                          staticDy, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    SetWindowFont(gHwndTextboxInstDir, gFontDefault, TRUE);

    y -= staticDy;

    gHwndStaticInstDir = CreateWindowExW(0, WC_STATIC, _TR("Install SumatraPDF in &folder:"), WS_CHILD, x, y, r.dx,
                                         staticDy, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    SetWindowFont(gHwndStaticInstDir, gFontDefault, TRUE);

    gShowOptions = !gShowOptions;
    OnButtonOptions();

    SetFocus(gHwndButtonInstUninst);

    if (gInstallerGlobals.autoUpdate) {
        // click the Install button
        PostMessage(hwnd, WM_COMMAND, IDOK, 0);
    }
}
//] ACCESSKEY_GROUP Installer

static void CreateMainWindow() {
    AutoFreeW title(str::Format(_TR("SumatraPDF %s Installer"), CURR_VERSION_STR));

    gHwndFrame = CreateWindowEx(trans::IsCurrLangRtl() ? WS_EX_LAYOUTRTL : 0, INSTALLER_FRAME_CLASS_NAME, title.Get(),
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                                dpiAdjust(INSTALLER_WIN_DX), dpiAdjust(INSTALLER_WIN_DY), nullptr, nullptr,
                                GetModuleHandle(nullptr), nullptr);
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
    AutoFreeW dir(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_UNINST, L"InstallLocation"));
    if (!dir)
        dir.Set(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_UNINST, L"InstallLocation"));
    if (dir) {
        if (str::EndsWithI(dir, L".exe")) {
            dir.Set(path::GetDir(dir));
        }
        if (!str::IsEmpty(dir.Get()) && dir::Exists(dir))
            return dir.StealData();
    }

    // fall back to %ProgramFiles%
    dir.Set(GetSpecialFolder(CSIDL_PROGRAM_FILES));
    if (dir)
        return path::Join(dir, APP_NAME_STR);
    // fall back to C:\ as a last resort
    return str::Dup(L"C:\\" APP_NAME_STR);
}

// TODO: must pass msg to CheckInstallUninstallPossible() instead
void SetDefaultMsg() {
    SetMsg(_TR("Thank you for choosing SumatraPDF!"), COLOR_MSG_WELCOME);
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
            if (gHwndButtonRunSumatra)
                SetFocus(gHwndButtonRunSumatra);
            if (gHwndButtonExit)
                SetFocus(gHwndButtonExit);
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static bool RegisterWinClass() {
    WNDCLASSEX wcex;

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
        if (t.GetTimeInMs() > 10000 && gHwndButtonInstUninst && IsWindowEnabled(gHwndButtonInstUninst)) {
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
        else if (is_arg("register"))
            gInstallerGlobals.registerAsDefault = true;
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
            // uninstall the deprecated browser plugin if it's not
            // explicitly listed (only applies if the /opt flag is used)
            if (!optlist.Contains(L"plugin"))
                gInstallerGlobals.keepBrowserPlugin = false;
        } else if (is_arg("x")) {
            gInstallerGlobals.justExtractFiles = true;
            // silently extract files to the current directory (if /d isn't used)
            gInstUninstGlobals.silent = true;
            if (!gInstUninstGlobals.installDir)
                str::ReplacePtr(&gInstUninstGlobals.installDir, L".");
        } else if (is_arg("autoupdate")) {
            gInstallerGlobals.autoUpdate = true;
        } else if (is_arg("h") || is_arg("help") || is_arg("?"))
            gInstUninstGlobals.showUsageAndQuit = true;
#ifdef ENABLE_CRASH_TESTING
        else if (is_arg("crash")) {
            // will induce crash when 'Install' button is pressed
            // for testing crash handling
            gForceCrash = true;
        }
#endif
    }
}

#define CRASH_DUMP_FILE_NAME L"suminstaller.dmp"

// no-op but must be defined for CrashHandler.cpp
void ShowCrashHandlerMessage() {}
void GetStressTestInfo(str::Str<char>* s) {
    UNUSED(s);
}

void GetProgramInfo(str::Str<char>& s) {
    s.AppendFmt("Ver: %s", CURR_VERSION_STRA);
#ifdef SVN_PRE_RELEASE_VER
    s.AppendFmt(" pre-release");
#endif
    if (IsProcess64()) {
        s.Append(" 64-bit");
    }
#ifdef DEBUG
    if (!str::Find(s.Get(), " (dbg)"))
        s.Append(" (dbg)");
#endif
    s.Append("\r\n");
#if defined(GIT_COMMIT_ID)
    const char* gitSha1 = QM(GIT_COMMIT_ID);
    s.AppendFmt("Git: %s (https://github.com/sumatrapdfreader/sumatrapdf/tree/%s)\r\n", gitSha1, gitSha1);
#endif
}

bool CrashHandlerCanUseNet() {
    return true;
}

static void InstallInstallerCrashHandler() {
    // save symbols directly into %TEMP% (so that the installer doesn't
    // unnecessarily leave an empty directory behind if it doesn't have to)
    AutoFreeW tempDir(path::GetTempPath());
    if (!tempDir || !dir::Exists(tempDir))
        tempDir.Set(GetSpecialFolder(CSIDL_LOCAL_APPDATA, true));
    {
        if (!tempDir || !dir::Exists(tempDir))
            return;
    }
    AutoFreeW crashDumpPath(path::Join(tempDir, CRASH_DUMP_FILE_NAME));
    InstallCrashHandler(crashDumpPath, tempDir);
}

int APIENTRY WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /* lpCmdLine*/, int nCmdShow) {
    UNUSED(nCmdShow);
    int ret = 1;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    // Change current directory to prevent dll hijacking.
    // LoadLibrary first loads from current directory which could be
    // browser's download directory, which is an easy target
    // for attackers to put their own fake dlls).
    // For this to work we also have to /delayload all libraries otherwise
    // they will be loaded even before WinMain executes.
    auto currDir = GetSystem32Dir();
    SetCurrentDirectoryW(currDir);
    free(currDir);

    InitDynCalls();
    NoDllHijacking();

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    // TOOD: remove as there'll be just one app
    InstallInstallerCrashHandler();

    ParseCommandLine(GetCommandLine());
    if (gInstUninstGlobals.showUsageAndQuit) {
        ShowUsage();
        ret = 0;
        goto Exit;
    }
    if (!gInstUninstGlobals.installDir)
        gInstUninstGlobals.installDir = GetInstallationDir();

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

    if (!RegisterWinClass())
        goto Exit;

    if (!InstanceInit())
        goto Exit;

    ret = RunApp();

Exit:
    trans::Destroy();
    free(gInstUninstGlobals.installDir);
    free(gInstUninstGlobals.firstError);

    return ret;
}
