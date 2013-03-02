/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifdef BUILD_UNINSTALLER
#error BUILD_UNINSTALLER must not be defined!!!
#endif

#include "Installer.h"
#include "ZipUtil.h"

#include "../ifilter/PdfFilter.h"
#include "../previewer/PdfPreview.h"

#define ID_CHECKBOX_MAKE_DEFAULT      14
#define ID_CHECKBOX_BROWSER_PLUGIN    15
#define ID_BUTTON_START_SUMATRA       16
#define ID_BUTTON_OPTIONS             17
#define ID_BUTTON_BROWSE              18
#define ID_CHECKBOX_PDF_FILTER        19
#define ID_CHECKBOX_PDF_PREVIEWER     20

static HWND             gHwndButtonOptions = NULL;
       HWND             gHwndButtonRunSumatra = NULL;
static HWND             gHwndStaticInstDir = NULL;
static HWND             gHwndTextboxInstDir = NULL;
static HWND             gHwndButtonBrowseDir = NULL;
static HWND             gHwndCheckboxRegisterDefault = NULL;
static HWND             gHwndCheckboxRegisterBrowserPlugin = NULL;
static HWND             gHwndCheckboxRegisterPdfFilter = NULL;
static HWND             gHwndCheckboxRegisterPdfPreviewer = NULL;
static HWND             gHwndProgressBar = NULL;

static int GetInstallationStepCount()
{
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
    for (int i = 0; NULL != gPayloadData[i].filepath; i++) {
        if (gPayloadData[i].install)
            count++;
    }
    return count;
}

static inline void ProgressStep()
{
    if (gHwndProgressBar)
        PostMessage(gHwndProgressBar, PBM_STEPIT, 0, 0);
}

bool IsValidInstaller()
{
    ZipFile archive(GetOwnPath());
    return archive.GetFileCount() > 0;
}

static bool InstallCopyFiles()
{
    // extract all payload files one by one (transacted, if possible)
    ZipFile archive(GetOwnPath());
    FileTransaction trans;

    for (int i = 0; gPayloadData[i].filepath; i++) {
        // skip files that are only uninstalled
        if (!gPayloadData[i].install)
            continue;
        ScopedMem<WCHAR> filepathT(str::conv::FromUtf8(gPayloadData[i].filepath));

        size_t size;
        ScopedMem<char> data(archive.GetFileData(filepathT, &size));
        if (!data) {
            NotifyFailed(_TR("Some files to be installed are damaged or missing"));
            return false;
        }

        ScopedMem<WCHAR> extpath(path::Join(gGlobalData.installDir, path::GetBaseName(filepathT)));
        bool ok = trans.WriteAll(extpath, data, size);
        if (!ok) {
            ScopedMem<WCHAR> msg(str::Format(_TR("Couldn't write %s to disk"), filepathT));
            NotifyFailed(msg);
            return false;
        }

        // set modification time to original value
        FILETIME ftModified = archive.GetFileTime(filepathT);
        trans.SetModificationTime(extpath, ftModified);

        ProgressStep();
    }

    return trans.Commit();
}

/* Caller needs to free() the result. */
static WCHAR *GetDefaultPdfViewer()
{
    ScopedMem<WCHAR> buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", PROG_ID));
    if (buf)
        return buf.StealData();
    return ReadRegStr(HKEY_CLASSES_ROOT, L".pdf", NULL);
}

bool IsBrowserPluginInstalled()
{
    ScopedMem<WCHAR> buf(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_PLUGIN, PLUGIN_PATH));
    if (!buf)
        buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_PLUGIN, PLUGIN_PATH));
    return file::Exists(buf);
}

bool IsPdfFilterInstalled()
{
    ScopedMem<WCHAR> handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, L".pdf\\PersistentHandler", NULL));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_FILTER_HANDLER);
}

bool IsPdfPreviewerInstalled()
{
    ScopedMem<WCHAR> handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, L".pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}", NULL));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_PREVIEW_CLSID);
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(const WCHAR *dir)
{
    ScopedMem<WCHAR> dirPattern(path::Join(dir, L"*"));
    WIN32_FIND_DATA findData;

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    DWORD totalSize = 0;
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            totalSize += findData.nFileSizeLow;
        }
        else if (!str::Eq(findData.cFileName, L".") && !str::Eq(findData.cFileName, L"..")) {
            ScopedMem<WCHAR> subdir(path::Join(dir, findData.cFileName));
            totalSize += GetDirSize(subdir);
        }
    } while (FindNextFile(h, &findData) != 0);
    FindClose(h);

    return totalSize;
}

// caller needs to free() the result
static WCHAR *GetInstallDate()
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    return str::Format(L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
}

static bool WriteUninstallerRegistryInfo(HKEY hkey)
{
    bool success = true;

    ScopedMem<WCHAR> uninstallerPath(GetUninstallerPath());
    ScopedMem<WCHAR> installedExePath(GetInstalledExePath());
    ScopedMem<WCHAR> installDate(GetInstallDate());
    ScopedMem<WCHAR> installDir(path::GetDir(installedExePath));

    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_ICON, installedExePath);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_NAME, APP_NAME_STR);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_VERSION, CURR_VERSION_STR);
    // Windows XP doesn't allow to view the version number at a glance,
    // so include it in the DisplayName
    if (!IsVistaOrGreater())
        success &= WriteRegStr(hkey, REG_PATH_UNINST, DISPLAY_NAME, APP_NAME_STR L" " CURR_VERSION_STR);
    DWORD size = GetDirSize(gGlobalData.installDir) / 1024;
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, ESTIMATED_SIZE, size);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, INSTALL_DATE, installDate);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, INSTALL_LOCATION, installDir);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, NO_MODIFY, 1);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, NO_REPAIR, 1);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, PUBLISHER, TEXT(PUBLISHER_STR));
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, UNINSTALL_STRING, uninstallerPath);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, URL_INFO_ABOUT, L"http://blog.kowalczyk.info/software/sumatrapdf/");
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, URL_UPDATE_INFO, L"http://blog.kowalczyk.info/software/sumatrapdf/news.html");

    return success;
}

// cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
static bool WriteExtendedFileExtensionInfo(HKEY hkey)
{
    bool success = true;

    ScopedMem<WCHAR> exePath(GetInstalledExePath());
    if (HKEY_LOCAL_MACHINE == hkey)
        success &= WriteRegStr(hkey, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" EXENAME, NULL, exePath);

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    ScopedMem<WCHAR> iconPath(str::Join(exePath, L",1"));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS L"\\DefaultIcon", NULL, iconPath);
    ScopedMem<WCHAR> cmdPath(str::Format(L"\"%s\" \"%%1\" %%*", exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS L"\\Shell\\Open\\Command", NULL, cmdPath);
    ScopedMem<WCHAR> printPath(str::Format(L"\"%s\" -print-to-default \"%%1\"", exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS L"\\Shell\\Print\\Command", NULL, printPath);
    ScopedMem<WCHAR> printToPath(str::Format(L"\"%s\" -print-to \"%%2\" \"%%1\"", exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS L"\\Shell\\PrintTo\\Command", NULL, printToPath);
    // don't add REG_CLASSES_APPS L"\\SupportedTypes", as that prevents SumatraPDF.exe to
    // potentially appear in the Open With lists for other filetypes (such as single images)

    // add the installed SumatraPDF.exe to the Open With lists of the supported file extensions
    for (int i = 0; NULL != gSupportedExts[i]; i++) {
        ScopedMem<WCHAR> keyname(str::Join(L"Software\\Classes\\", gSupportedExts[i], L"\\OpenWithList\\" EXENAME));
        success &= CreateRegKey(hkey, keyname);
        // TODO: stop removing this after version 1.8 (was wrongly created for version 1.6)
        keyname.Set(str::Join(L"Software\\Classes\\", gSupportedExts[i], L"\\OpenWithList\\" APP_NAME_STR));
        DeleteRegKey(hkey, keyname);
    }

    // in case these values don't exist yet (we won't delete these at uninstallation)
    success &= WriteRegStr(hkey, REG_CLASSES_PDF, L"Content Type", L"application/pdf");
    success &= WriteRegStr(hkey, L"Software\\Classes\\MIME\\Database\\Content Type\\application/pdf", L"Extension", L".pdf");

    return success;
}

static bool CreateInstallationDirectory()
{
    bool ok = dir::CreateAll(gGlobalData.installDir);
    if (!ok) {
        LogLastError();
        NotifyFailed(_TR("Couldn't create the installation directory"));
    }
    return ok;
}

static void CreateButtonRunSumatra(HWND hwndParent)
{
    // TODO: this button might be too narrow for some translations
    gHwndButtonRunSumatra = CreateDefaultButton(hwndParent, _TR("Start SumatraPDF"), 140, ID_BUTTON_START_SUMATRA);
}

static bool CreateAppShortcut(bool allUsers)
{
    ScopedMem<WCHAR> shortcutPath(GetShortcutPath(allUsers));
    if (!shortcutPath.Get())
        return false;
    ScopedMem<WCHAR> installedExePath(GetInstalledExePath());
    return CreateShortcut(shortcutPath, installedExePath);
}

DWORD WINAPI InstallerThread(LPVOID data)
{
    gGlobalData.success = false;

    if (!CreateInstallationDirectory())
        goto Error;
    ProgressStep();

    if (!InstallCopyFiles())
        goto Error;

    if (gGlobalData.registerAsDefault) {
        // need to sublaunch SumatraPDF.exe instead of replicating the code
        // because registration uses translated strings
        ScopedMem<WCHAR> installedExePath(GetInstalledExePath());
        CreateProcessHelper(installedExePath, L"-register-for-pdf");
    }

    if (gGlobalData.installBrowserPlugin)
        InstallBrowserPlugin();
    else if (IsBrowserPluginInstalled())
        UninstallBrowserPlugin();

    if (gGlobalData.installPdfFilter)
        InstallPdfFilter();
    else if (IsPdfFilterInstalled())
        UninstallPdfFilter();

    if (gGlobalData.installPdfPreviewer)
        InstallPdfPreviewer();
    else if (IsPdfPreviewerInstalled())
        UninstallPdfPreviewer();

    if (!CreateAppShortcut(true) && !CreateAppShortcut(false)) {
        NotifyFailed(_TR("Failed to create a shortcut"));
        goto Error;
    }

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gGlobalData.success = true;

    if (!WriteUninstallerRegistryInfo(HKEY_LOCAL_MACHINE) &&
        !WriteUninstallerRegistryInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_TR("Failed to write the uninstallation information to the registry"));
    }
    if (!WriteExtendedFileExtensionInfo(HKEY_LOCAL_MACHINE) &&
        !WriteExtendedFileExtensionInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_TR("Failed to write the extended file extension information to the registry"));
    }
    ProgressStep();

Error:
    // TODO: roll back installation on failure (restore previous installation!)
    if (gHwndFrame && !gGlobalData.silent) {
        Sleep(500); // allow a glimpse of the completed progress bar before hiding it
        PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    }
    return 0;
}

static void OnButtonOptions();

static bool IsCheckboxChecked(HWND hwnd)
{
    return (Button_GetState(hwnd) & BST_CHECKED) == BST_CHECKED;
}

static void OnButtonInstall()
{
    CrashAlwaysIf(gForceCrash);

    if (gShowOptions)
        OnButtonOptions();

    KillSumatra();

    if (!CheckInstallUninstallPossible())
        return;

    WCHAR *userInstallDir = win::GetText(gHwndTextboxInstDir);
    if (!str::IsEmpty(userInstallDir))
        str::ReplacePtr(&gGlobalData.installDir, userInstallDir);
    free(userInstallDir);

    // note: this checkbox isn't created if we're already registered as default
    //       (in which case we're just going to re-register)
    gGlobalData.registerAsDefault = gHwndCheckboxRegisterDefault == NULL ||
                                    IsCheckboxChecked(gHwndCheckboxRegisterDefault);

    gGlobalData.installBrowserPlugin = IsCheckboxChecked(gHwndCheckboxRegisterBrowserPlugin);
    // note: this checkbox isn't created when running inside Wow64
    gGlobalData.installPdfFilter = gHwndCheckboxRegisterPdfFilter != NULL &&
                                   IsCheckboxChecked(gHwndCheckboxRegisterPdfFilter);
    // note: this checkbox isn't created on Windows 2000 and XP
    gGlobalData.installPdfPreviewer = gHwndCheckboxRegisterPdfPreviewer != NULL &&
                                      IsCheckboxChecked(gHwndCheckboxRegisterPdfPreviewer);

    // create a progress bar in place of the Options button
    RectI rc(0, 0, dpiAdjust(INSTALLER_WIN_DX / 2), PUSH_BUTTON_DY);
    rc = MapRectToWindow(rc, gHwndButtonOptions, gHwndFrame);
    gHwndProgressBar = CreateWindow(PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
                                    rc.x, rc.y, rc.dx, rc.dy,
                                    gHwndFrame, 0, ghinst, NULL);
    SendMessage(gHwndProgressBar, PBM_SETRANGE32, 0, GetInstallationStepCount());
    SendMessage(gHwndProgressBar, PBM_SETSTEP, 1, 0);

    // disable the install button and remove all the installation options
    SafeDestroyWindow(&gHwndStaticInstDir);
    SafeDestroyWindow(&gHwndTextboxInstDir);
    SafeDestroyWindow(&gHwndButtonBrowseDir);
    SafeDestroyWindow(&gHwndCheckboxRegisterDefault);
    SafeDestroyWindow(&gHwndCheckboxRegisterBrowserPlugin);
    SafeDestroyWindow(&gHwndCheckboxRegisterPdfFilter);
    SafeDestroyWindow(&gHwndCheckboxRegisterPdfPreviewer);
    SafeDestroyWindow(&gHwndButtonOptions);

    EnableWindow(gHwndButtonInstUninst, FALSE);

    SetMsg(_TR("Installation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    gGlobalData.hThread = CreateThread(NULL, 0, InstallerThread, NULL, 0, 0);
}

void OnInstallationFinished()
{
    DestroyWindow(gHwndButtonInstUninst);
    gHwndButtonInstUninst = NULL;
    DestroyWindow(gHwndProgressBar);
    gHwndProgressBar = NULL;

    if (gGlobalData.success) {
        CreateButtonRunSumatra(gHwndFrame);
        SetMsg(_TR("Thank you! SumatraPDF has been installed."), COLOR_MSG_OK);
    } else {
        CreateButtonExit(gHwndFrame);
        SetMsg(_TR("Installation failed!"), COLOR_MSG_FAILED);
    }
    gMsgError = gGlobalData.firstError;
    InvalidateFrame();

    CloseHandle(gGlobalData.hThread);
}

static void OnButtonStartSumatra()
{
    ScopedMem<WCHAR> exePath(GetInstalledExePath());
    RunNonElevated(exePath);
    OnButtonExit();
}

inline void EnableAndShow(HWND hwnd, bool enable)
{
    ShowWindow(hwnd, enable ? SW_SHOW : SW_HIDE);
    EnableWindow(hwnd, enable);
}

static void OnButtonOptions()
{
    gShowOptions = !gShowOptions;

    EnableAndShow(gHwndStaticInstDir, gShowOptions);
    EnableAndShow(gHwndTextboxInstDir, gShowOptions);
    EnableAndShow(gHwndButtonBrowseDir, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterDefault, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterBrowserPlugin, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterPdfFilter, gShowOptions);
    EnableAndShow(gHwndCheckboxRegisterPdfPreviewer, gShowOptions);

    win::SetText(gHwndButtonOptions, gShowOptions ? _TR("Hide &Options") : _TR("&Options"));

    ClientRect rc(gHwndFrame);
    rc.dy -= BOTTOM_PART_DY;
    InvalidateRect(gHwndFrame, &rc.ToRECT(), FALSE);

    SetFocus(gHwndButtonOptions);

    CreateButtonRunSumatra(gHwndFrame);
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT msg, LPARAM lParam, LPARAM lpData)
{
    switch (msg) {
    case BFFM_INITIALIZED:
        if (!str::IsEmpty((WCHAR *)lpData))
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
        break;

    // disable the OK button for non-filesystem and inaccessible folders (and shortcuts to folders)
    case BFFM_SELCHANGED:
        {
            WCHAR path[MAX_PATH];
            if (SHGetPathFromIDList((LPITEMIDLIST)lParam, path) && dir::Exists(path)) {
                SHFILEINFO sfi;
                SHGetFileInfo((LPCWSTR)lParam, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_ATTRIBUTES);
                if (!(sfi.dwAttributes & SFGAO_LINK))
                    break;
            }
            EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
        }
        break;
    }

    return 0;
}

static BOOL BrowseForFolder(HWND hwnd, const WCHAR *lpszInitialFolder, const WCHAR *lpszCaption, WCHAR *lpszBuf, DWORD dwBufSize)
{
    if (lpszBuf == NULL || dwBufSize < MAX_PATH)
        return FALSE;

    BROWSEINFO bi = { 0 };
    bi.hwndOwner = hwnd;
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = lpszCaption;
    bi.lpfn      = BrowseCallbackProc;
    bi.lParam    = (LPARAM)lpszInitialFolder;

    BOOL success = FALSE;
    LPITEMIDLIST pidlFolder = SHBrowseForFolder(&bi);
    if (pidlFolder) {
        success = SHGetPathFromIDList(pidlFolder, lpszBuf);

        IMalloc *pMalloc = NULL;
        if (SUCCEEDED(SHGetMalloc(&pMalloc)) && pMalloc) {
            pMalloc->Free(pidlFolder);
            pMalloc->Release();
        }
    }

    return success;
}

static void OnButtonBrowse()
{
    ScopedMem<WCHAR> installDir(win::GetText(gHwndTextboxInstDir));
    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir))
        installDir.Set(path::GetDir(installDir));

    WCHAR path[MAX_PATH];
    if (BrowseForFolder(gHwndFrame, installDir, _TR("Select the folder where SumatraPDF should be installed:"), path, dimof(path))) {
        WCHAR *installPath = path;
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
    else
        SetFocus(gHwndButtonBrowseDir);
}

bool OnWmCommand(WPARAM wParam)
{
    switch (LOWORD(wParam))
    {
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

// TODO: since we have a variable UI, for better layout (anchored to the bottom,
// not the top), we should layout controls starting at the bottom and go up
void OnCreateWindow(HWND hwnd)
{
    // TODO: this button might be too narrow for some translations
    gHwndButtonInstUninst = CreateDefaultButton(hwnd, _TR("Install SumatraPDF"), 140);

    RectI rc(WINDOW_MARGIN, 0, dpiAdjust(96), PUSH_BUTTON_DY);
    ClientRect r(hwnd);
    rc.y = r.dy - rc.dy - WINDOW_MARGIN;

    gHwndButtonOptions = CreateWindow(WC_BUTTON, _TR("&Options"),
                        BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        rc.x, rc.y, rc.dx, rc.dy, hwnd,
                        (HMENU)ID_BUTTON_OPTIONS, ghinst, NULL);
    SetWindowFont(gHwndButtonOptions, gFontDefault, TRUE);

    int staticDy = dpiAdjust(20);
    rc.y = TITLE_PART_DY + WINDOW_MARGIN;
    gHwndStaticInstDir = CreateWindow(WC_STATIC, _TR("Install SumatraPDF in &folder:"),
                                      WS_CHILD,
                                      rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
                                      hwnd, NULL, ghinst, NULL);
    SetWindowFont(gHwndStaticInstDir, gFontDefault, TRUE);
    rc.y += staticDy;

    gHwndTextboxInstDir = CreateWindow(WC_EDIT, gGlobalData.installDir,
                                       WS_CHILD | WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
                                       rc.x, rc.y, r.dx - 3 * rc.x - staticDy, staticDy,
                                       hwnd, NULL, ghinst, NULL);
    SetWindowFont(gHwndTextboxInstDir, gFontDefault, TRUE);
    gHwndButtonBrowseDir = CreateWindow(WC_BUTTON, L"&...",
                                        BS_PUSHBUTTON | WS_CHILD | WS_TABSTOP,
                                        r.dx - rc.x - staticDy, rc.y, staticDy, staticDy,
                                        hwnd, (HMENU)ID_BUTTON_BROWSE, ghinst, NULL);
    SetWindowFont(gHwndButtonBrowseDir, gFontDefault, TRUE);
    rc.y += 2 * staticDy;

    ScopedMem<WCHAR> defaultViewer(GetDefaultPdfViewer());
    BOOL hasOtherViewer = !str::EqI(defaultViewer, APP_NAME_STR);
    BOOL isSumatraDefaultViewer = defaultViewer && !hasOtherViewer;

    // only show the checbox if Sumatra is not already a default viewer.
    // the alternative (disabling the checkbox) is more confusing
    if (!isSumatraDefaultViewer) {
        gHwndCheckboxRegisterDefault = CreateWindow(
            WC_BUTTON, _TR("Use SumatraPDF as the &default PDF reader"),
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
            hwnd, (HMENU)ID_CHECKBOX_MAKE_DEFAULT, ghinst, NULL);
        SetWindowFont(gHwndCheckboxRegisterDefault, gFontDefault, TRUE);
        // only check the "Use as default" checkbox when no other PDF viewer
        // is currently selected (not going to intrude)
        Button_SetCheck(gHwndCheckboxRegisterDefault, !hasOtherViewer || gGlobalData.registerAsDefault);
        rc.y += staticDy;
    }

    gHwndCheckboxRegisterBrowserPlugin = CreateWindow(
        WC_BUTTON, _TR("Install PDF &browser plugin for Firefox, Chrome and Opera"),
        WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
        rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
        hwnd, (HMENU)ID_CHECKBOX_BROWSER_PLUGIN, ghinst, NULL);
    SetWindowFont(gHwndCheckboxRegisterBrowserPlugin, gFontDefault, TRUE);
    Button_SetCheck(gHwndCheckboxRegisterBrowserPlugin, gGlobalData.installBrowserPlugin || IsBrowserPluginInstalled());
    rc.y += staticDy;

    // only show this checkbox if the CPU arch of DLL and OS match
    // (assuming that the installer has the same CPU arch as its content!)
#ifndef _WIN64
    if (!IsRunningInWow64())
#endif
    {
        gHwndCheckboxRegisterPdfFilter = CreateWindow(
            WC_BUTTON, _TR("Let Windows Desktop Search &search PDF documents"),
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
            hwnd, (HMENU)ID_CHECKBOX_PDF_FILTER, ghinst, NULL);
        SetWindowFont(gHwndCheckboxRegisterPdfFilter, gFontDefault, TRUE);
        Button_SetCheck(gHwndCheckboxRegisterPdfFilter, gGlobalData.installPdfFilter || IsPdfFilterInstalled());
        rc.y += staticDy;
    }

    // for Windows XP, this means only basic thumbnail support
    gHwndCheckboxRegisterPdfPreviewer = CreateWindow(
        WC_BUTTON, _TR("Let Windows show &previews of PDF documents"),
        WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
        rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
        hwnd, (HMENU)ID_CHECKBOX_PDF_PREVIEWER, ghinst, NULL);
    SetWindowFont(gHwndCheckboxRegisterPdfPreviewer, gFontDefault, TRUE);
    Button_SetCheck(gHwndCheckboxRegisterPdfPreviewer, gGlobalData.installPdfPreviewer || IsPdfPreviewerInstalled());
    rc.y += staticDy;

    gShowOptions = !gShowOptions;
    OnButtonOptions();

    SetFocus(gHwndButtonInstUninst);
}

void CreateMainWindow()
{
    ScopedMem<WCHAR> title(str::Format(_TR("SumatraPDF %s Installer"), CURR_VERSION_STR));

    gHwndFrame = CreateWindow(
        INSTALLER_FRAME_CLASS_NAME, title.Get(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        dpiAdjust(INSTALLER_WIN_DX), dpiAdjust(INSTALLER_WIN_DY),
        NULL, NULL,
        ghinst, NULL);
}

void ShowUsage()
{
    // Note: translation services aren't initialized at this point, so English only
    MessageBox(NULL, APP_NAME_STR L"-install.exe [/s][/d <path>][/register][/opt plugin,...]\n\
    \n\
    /s\tinstalls " APP_NAME_STR L" silently (without user interaction).\n\
    /d\tchanges the directory where " APP_NAME_STR L" will be installed.\n\
    /register\tregisters " APP_NAME_STR L" as the default PDF viewer.\n\
    /opt\tenables optional components (currently: plugin, pdffilter, pdfpreviewer).", APP_NAME_STR L" Installer Usage", MB_OK | MB_ICONINFORMATION);
}
