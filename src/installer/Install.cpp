/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifdef BUILD_UNINSTALLER
#error BUILD_UNINSTALLER must not be defined!!!
#endif

#include "Installer.h"
#include "ZipUtil.h"
#include <WinSafer.h>

#include "../ifilter/PdfFilter.h"
#include "../previewer/PdfPreview.h"

#include "DebugLog.h"

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
        ScopedMem<TCHAR> filepathT(str::conv::FromUtf8(gPayloadData[i].filepath));

        size_t size;
        ScopedMem<char> data(archive.GetFileData(filepathT, &size));
        if (!data) {
            NotifyFailed(_T("Some files to be installed are damaged or missing"));
            return false;
        }

        ScopedMem<TCHAR> extpath(path::Join(gGlobalData.installDir, path::GetBaseName(filepathT)));
        bool ok = trans.WriteAll(extpath, data, size);
        if (!ok) {
            ScopedMem<TCHAR> msg(str::Format(_T("Couldn't write %s to disk"), filepathT));
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
static TCHAR *GetDefaultPdfViewer()
{
    ScopedMem<TCHAR> buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT _T("\\UserChoice"), PROG_ID));
    if (buf)
        return buf.StealData();
    return ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf"), NULL);
}

bool IsBrowserPluginInstalled()
{
    ScopedMem<TCHAR> buf(ReadRegStr(HKEY_LOCAL_MACHINE, REG_PATH_PLUGIN, PLUGIN_PATH));
    if (!buf)
        buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_PATH_PLUGIN, PLUGIN_PATH));
    return file::Exists(buf);
}

bool IsPdfFilterInstalled()
{
    ScopedMem<TCHAR> handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf\\PersistentHandler"), NULL));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_FILTER_HANDLER);
}

bool IsPdfPreviewerInstalled()
{
    ScopedMem<TCHAR> handler_iid(ReadRegStr(HKEY_CLASSES_ROOT, _T(".pdf\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}"), NULL));
    if (!handler_iid)
        return false;
    return str::EqI(handler_iid, SZ_PDF_PREVIEW_CLSID);
}

// Note: doesn't handle (total) sizes above 4GB
static DWORD GetDirSize(TCHAR *dir)
{
    ScopedMem<TCHAR> dirPattern(path::Join(dir, _T("*")));
    WIN32_FIND_DATA findData;

    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    DWORD totalSize = 0;
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            totalSize += findData.nFileSizeLow;
        }
        else if (!str::Eq(findData.cFileName, _T(".")) && !str::Eq(findData.cFileName, _T(".."))) {
            ScopedMem<TCHAR> subdir(path::Join(dir, findData.cFileName));
            totalSize += GetDirSize(subdir);
        }
    } while (FindNextFile(h, &findData) != 0);
    FindClose(h);

    return totalSize;
}

// caller needs to free() the result
static TCHAR *GetInstallDate()
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    return str::Format(_T("%04d%02d%02d"), st.wYear, st.wMonth, st.wDay);
}

static bool WriteUninstallerRegistryInfo(HKEY hkey)
{
    bool success = true;

    ScopedMem<TCHAR> uninstallerPath(GetUninstallerPath());
    ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
    ScopedMem<TCHAR> installDate(GetInstallDate());
    ScopedMem<TCHAR> installDir(path::GetDir(installedExePath));

    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_ICON, installedExePath);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_NAME, TAPP);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, DISPLAY_VERSION, CURR_VERSION_STR);
    // Windows XP doesn't allow to view the version number at a glance,
    // so include it in the DisplayName
    if (!WindowsVerVistaOrGreater())
        success &= WriteRegStr(hkey, REG_PATH_UNINST, DISPLAY_NAME, TAPP _T(" ") CURR_VERSION_STR);
    DWORD size = GetDirSize(gGlobalData.installDir) / 1024;
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, ESTIMATED_SIZE, size);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, INSTALL_DATE, installDate);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, INSTALL_LOCATION, installDir);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, NO_MODIFY, 1);
    success &= WriteRegDWORD(hkey, REG_PATH_UNINST, NO_REPAIR, 1);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, PUBLISHER, _T(PUBLISHER_STR));
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, UNINSTALL_STRING, uninstallerPath);
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, URL_INFO_ABOUT, _T("http://blog.kowalczyk.info/software/sumatrapdf/"));
    success &= WriteRegStr(hkey,   REG_PATH_UNINST, URL_UPDATE_INFO, _T("http://blog.kowalczyk.info/software/sumatrapdf/news.html"));

    return success;
}

// cf. http://msdn.microsoft.com/en-us/library/cc144148(v=vs.85).aspx
static bool WriteExtendedFileExtensionInfo(HKEY hkey)
{
    bool success = true;

    ScopedMem<TCHAR> exePath(GetInstalledExePath());
    if (HKEY_LOCAL_MACHINE == hkey)
        success &= WriteRegStr(hkey, _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\") EXENAME, NULL, exePath);

    // mirroring some of what DoAssociateExeWithPdfExtension() does (cf. AppTools.cpp)
    ScopedMem<TCHAR> iconPath(str::Join(exePath, _T(",1")));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS _T("\\DefaultIcon"), NULL, iconPath);
    ScopedMem<TCHAR> cmdPath(str::Format(_T("\"%s\" \"%%1\""), exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS _T("\\Shell\\Open\\Command"), NULL, cmdPath);
    ScopedMem<TCHAR> printPath(str::Format(_T("\"%s\" -print-to-default \"%%1\""), exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS _T("\\Shell\\Print\\Command"), NULL, printPath);
    ScopedMem<TCHAR> printToPath(str::Format(_T("\"%s\" -print-to \"%%2\" \"%%1\""), exePath));
    success &= WriteRegStr(hkey, REG_CLASSES_APPS _T("\\Shell\\PrintTo\\Command"), NULL, printToPath);
    // don't add REG_CLASSES_APPS _T("\\SupportedTypes"), as that prevents SumatraPDF.exe to
    // potentially appear in the Open With lists for other filetypes (such as single images)

    // add the installed SumatraPDF.exe to the Open With lists of the supported file extensions
    for (int i = 0; NULL != gSupportedExts[i]; i++) {
        ScopedMem<TCHAR> keyname(str::Join(_T("Software\\Classes\\"), gSupportedExts[i], _T("\\OpenWithList\\") EXENAME));
        success &= CreateRegKey(hkey, keyname);
        // TODO: stop removing this after version 1.8 (was wrongly created for version 1.6)
        keyname.Set(str::Join(_T("Software\\Classes\\"), gSupportedExts[i], _T("\\OpenWithList\\") TAPP));
        DeleteRegKey(hkey, keyname);
    }

    // in case these values don't exist yet (we won't delete these at uninstallation)
    success &= WriteRegStr(hkey, REG_CLASSES_PDF, _T("Content Type"), _T("application/pdf"));
    success &= WriteRegStr(hkey, _T("Software\\Classes\\MIME\\Database\\Content Type\\application/pdf"), _T("Extension"), _T(".pdf"));

    return success;
}

static bool CreateInstallationDirectory()
{
    bool ok = dir::CreateAll(gGlobalData.installDir);
    if (!ok) {
        LogLastError();
        NotifyFailed(_T("Couldn't create the installation directory"));
    }
    return ok;
}

static void CreateButtonRunSumatra(HWND hwndParent)
{
    gHwndButtonRunSumatra = CreateDefaultButton(hwndParent, _T("Start ") TAPP, 120, ID_BUTTON_START_SUMATRA);
}

static bool CreateAppShortcut(bool allUsers)
{
    ScopedMem<TCHAR> shortcutPath(GetShortcutPath(allUsers));
    if (!shortcutPath.Get())
        return false;
    ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
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
        ScopedMem<TCHAR> installedExePath(GetInstalledExePath());
        CreateProcessHelper(installedExePath, _T("-register-for-pdf"));
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
        NotifyFailed(_T("Failed to create a shortcut"));
        goto Error;
    }

    // consider installation a success from here on
    // (still warn, if we've failed to create the uninstaller, though)
    gGlobalData.success = true;

    if (!WriteUninstallerRegistryInfo(HKEY_LOCAL_MACHINE) &&
        !WriteUninstallerRegistryInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_T("Failed to write the uninstallation information to the registry"));
    }
    if (!WriteExtendedFileExtensionInfo(HKEY_LOCAL_MACHINE) &&
        !WriteExtendedFileExtensionInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_T("Failed to write the extended file extension information to the registry"));
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

    TCHAR *userInstallDir = win::GetText(gHwndTextboxInstDir);
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
    DestroyWindow(gHwndStaticInstDir);
    gHwndStaticInstDir = NULL;
    DestroyWindow(gHwndTextboxInstDir);
    gHwndTextboxInstDir = NULL;
    DestroyWindow(gHwndButtonBrowseDir);
    gHwndButtonBrowseDir = NULL;
    DestroyWindow(gHwndCheckboxRegisterDefault);
    gHwndCheckboxRegisterDefault = NULL;
    DestroyWindow(gHwndCheckboxRegisterBrowserPlugin);
    gHwndCheckboxRegisterBrowserPlugin = NULL;
    DestroyWindow(gHwndCheckboxRegisterPdfFilter);
    gHwndCheckboxRegisterPdfFilter = NULL;
    DestroyWindow(gHwndCheckboxRegisterPdfPreviewer);
    gHwndCheckboxRegisterPdfPreviewer = NULL;
    DestroyWindow(gHwndButtonOptions);
    gHwndButtonOptions = NULL;

    EnableWindow(gHwndButtonInstUninst, FALSE);

    SetMsg(_T("Installation in progress..."), COLOR_MSG_INSTALLATION);
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
        SetMsg(_T("Thank you! ") TAPP _T(" has been installed."), COLOR_MSG_OK);
    } else {
        CreateButtonExit(gHwndFrame);
        SetMsg(_T("Installation failed!"), COLOR_MSG_FAILED);
    }
    gMsgError = gGlobalData.firstError;
    InvalidateFrame();

    CloseHandle(gGlobalData.hThread);
}

#if 0
typedef BOOL WINAPI SaferCreateLevelProc(DWORD dwScopeId, DWORD dwLevelId, DWORD OpenFlags, SAFER_LEVEL_HANDLE *pLevelHandle, LPVOID lpReserved);
typedef BOOL WINAPI SaferComputeTokenFromLevelProc(SAFER_LEVEL_HANDLE LevelHandle, HANDLE InAccessToken, PHANDLE OutAccessToken, DWORD dwFlags, LPVOID lpReserved);
typedef BOOL WINAPI SaferCloseLevelProc(SAFER_LEVEL_HANDLE hLevelHandle);

static HANDLE CreateProcessAtLevel(const TCHAR *exe, const TCHAR *args=NULL, DWORD level=SAFER_LEVELID_NORMALUSER)
{
    HMODULE h = SafeLoadLibrary(_T("Advapi32.dll"));
    if (!h)
        return NULL;
#define ImportProc(func) func ## Proc *_ ## func = (func ## Proc *)GetProcAddress(h, #func)
    ImportProc(SaferCreateLevel);
    ImportProc(SaferComputeTokenFromLevel);
    ImportProc(SaferCloseLevel);
#undef ImportProc
    if (!_SaferCreateLevel || !_SaferComputeTokenFromLevel || !_SaferCloseLevel)
        return NULL;

    SAFER_LEVEL_HANDLE slh;
    if (!_SaferCreateLevel(SAFER_SCOPEID_USER, level, 0, &slh, NULL))
        return NULL;

    ScopedMem<TCHAR> cmd(str::Format(_T("\"%s\" %s"), exe, args ? args : _T("")));
    PROCESS_INFORMATION pi;
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);

    HANDLE token;
    if (!_SaferComputeTokenFromLevel(slh, NULL, &token, 0, NULL))
        goto Error;
    if (!CreateProcessAsUser(token, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        goto Error;

    CloseHandle(pi.hThread);
    _SaferCloseLevel(slh);
    return pi.hProcess;

Error:
    LogLastError();
    _SaferCloseLevel(slh);
    return NULL;
}
#endif

// Run a given *.exe as a non-elevated (non-admin) process.
// based on http://stackoverflow.com/questions/3298611/run-my-program-asuser
// TODO: move to WinUtil.cpp
bool RunAsUser(WCHAR *cmd)
{
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    HANDLE hProcessToken = 0;
    HANDLE hShellProcess = 0;
    HANDLE hShellProcessToken = 0;
    HANDLE hPrimaryToken = 0;
    DWORD retLength, pid;
    TOKEN_PRIVILEGES tkp = { 0 };
    bool ret = false;

    // Enable SeIncreaseQuotaPrivilege in this process (won't work if current process is not elevated)
    BOOL ok = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hProcessToken);
    if (!ok) {
        plogf("RunAsUser(): OpenProcessToken() failed");
        goto Error;
    }

    tkp.PrivilegeCount = 1;
    ok = LookupPrivilegeValue(NULL, SE_INCREASE_QUOTA_NAME, &tkp.Privileges[0].Luid);
    if (!ok) {
        plogf("RunAsUser(): LookupPrivilegeValue() failed");
        goto Error;
    }
   
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    ok = AdjustTokenPrivileges(hProcessToken, FALSE, &tkp, 0, NULL, &retLength);
    if (!ok || (ERROR_SUCCESS != GetLastError())) {
        plogf("RunAsUser(): AdjustTokenPrivileges() failed");
        goto Error;
    }

    // Get an HWND representing the desktop shell.
    // Note: this will fail if the shell is not running (crashed or terminated),
    // or the default shell has been replaced with a custom shell. This also won't
    // return what you probably want if Explorer has been terminated and
    // restarted elevated.b
    HWND hwnd = GetShellWindow();
    if (NULL == hwnd) {
        plogf("RunAsUser(): GetShellWindow() failed");
        goto Error;
    }

    // Get the PID of the desktop shell process.
    GetWindowThreadProcessId(hwnd, &pid);
    if (0 == pid) {
        plogf("RunAsUser(): GetWindowThreadProcessId() failed");
        goto Error;
    }

    // Open the desktop shell process in order to query it (get its token)
    hShellProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (0 == hShellProcess) {
        plogf("RunAsUser(): OpenProcess() failed");
        goto Error;
    }

    // Get the process token of the desktop shell.
    ok = OpenProcessToken(hShellProcess, TOKEN_DUPLICATE, &hShellProcessToken);
    if (!ok) {
        plogf("RunAsUser(): OpenProcessToken() failed");
        goto Error;
    }

    // Duplicate the shell's process token to get a primary token.
    // Based on experimentation, this is the minimal set of rights required 
    // for CreateProcessWithTokenW (contrary to current documentation).
    //DWORD tokenRights = TOKEN_QUERY || TOKEN_ASSIGN_PRIMARY || TOKEN_DUPLICATE || TOKEN_ADJUST_DEFAULT || TOKEN_ADJUST_SESSIONID || TOKEN_IMPERSONATE;
    // TODO: tokenRights could probably be trimmed but the one above is not enough
    DWORD tokenRights = TOKEN_ALL_ACCESS;
    ok = DuplicateTokenEx(hShellProcessToken, tokenRights, NULL, SecurityImpersonation, TokenPrimary, &hPrimaryToken);
    if (!ok) {
        plogf("RunAsUser(): DuplicateTokenEx() failed");
        goto Error;
    }

    si.cb = sizeof(si);
    si.wShowWindow = SW_SHOWNORMAL;
    si.dwFlags = STARTF_USESHOWWINDOW;

    ok = CreateProcessWithTokenW(hPrimaryToken, 0, NULL, cmd, 0, NULL, NULL, &si, &pi);
    if (!ok) {
        plogf("RunAsUser(): CreateProcessWithTokenW() failed");
        goto Error;
    }

    ret = true;
Exit:
    CloseHandle(hProcessToken);
    CloseHandle(pi.hProcess);
    CloseHandle(hShellProcessToken);
    CloseHandle(hPrimaryToken);
    CloseHandle(hShellProcess);
    return ret;
Error:
    LogLastError();
    goto Exit;
}

static void OnButtonStartSumatra()
{
    ScopedMem<TCHAR> exePath(GetInstalledExePath());
#if 0
    // try to create the process as a normal user
    ScopedHandle h(CreateProcessAtLevel(exePath));
    // create the process as is (mainly for Windows 2000 compatibility)
    if (!h)
        CreateProcessHelper(exePath);
#else
    RunAsUser(exePath);
#endif
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

    win::SetText(gHwndButtonOptions, gShowOptions ? _T("Hide &Options") : _T("&Options"));

    ClientRect rc(gHwndFrame);
    rc.dy -= BOTTOM_PART_DY;
    InvalidateRect(gHwndFrame, &rc.ToRECT(), FALSE);

    SetFocus(gHwndButtonOptions);
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT msg, LPARAM lParam, LPARAM lpData)
{
    switch (msg) {
    case BFFM_INITIALIZED:
        if (!str::IsEmpty((TCHAR *)lpData))
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
        break;

    // disable the OK button for non-filesystem and inaccessible folders (and shortcuts to folders)
    case BFFM_SELCHANGED:
        {
            TCHAR path[MAX_PATH];
            if (SHGetPathFromIDList((LPITEMIDLIST)lParam, path) && dir::Exists(path)) {
                SHFILEINFO sfi;
                SHGetFileInfo((LPCTSTR)lParam, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_ATTRIBUTES);
                if (!(sfi.dwAttributes & SFGAO_LINK))
                    break;
            }
            EnableWindow(GetDlgItem(hwnd, IDOK), FALSE);
        }
        break;
    }

    return 0;
}

static BOOL BrowseForFolder(HWND hwnd, LPCTSTR lpszInitialFolder, LPCTSTR lpszCaption, LPTSTR lpszBuf, DWORD dwBufSize)
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
    ScopedMem<TCHAR> installDir(win::GetText(gHwndTextboxInstDir));
    // strip a trailing "\SumatraPDF" if that directory doesn't exist (yet)
    if (!dir::Exists(installDir))
        installDir.Set(path::GetDir(installDir));

    TCHAR path[MAX_PATH];
    if (BrowseForFolder(gHwndFrame, installDir, _T("Select the folder into which ") TAPP _T(" should be installed:"), path, dimof(path))) {
        TCHAR *installPath = path;
        // force paths that aren't entered manually to end in ...\SumatraPDF
        // to prevent unintended installations into e.g. %ProgramFiles% itself
        if (!str::EndsWithI(path, _T("\\") TAPP))
            installPath = path::Join(path, TAPP);
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
    gHwndButtonInstUninst = CreateDefaultButton(hwnd, _T("Install ") TAPP, 140);

    RectI rc(WINDOW_MARGIN, 0, dpiAdjust(96), PUSH_BUTTON_DY);
    ClientRect r(hwnd);
    rc.y = r.dy - rc.dy - WINDOW_MARGIN;

    gHwndButtonOptions = CreateWindow(WC_BUTTON, _T("&Options"),
                        BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        rc.x, rc.y, rc.dx, rc.dy, hwnd,
                        (HMENU)ID_BUTTON_OPTIONS, ghinst, NULL);
    SetWindowFont(gHwndButtonOptions, gFontDefault, TRUE);

    int staticDy = dpiAdjust(20);
    rc.y = TITLE_PART_DY + WINDOW_MARGIN;
    gHwndStaticInstDir = CreateWindow(WC_STATIC, _T("Install ") TAPP _T(" into the following &folder:"),
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
    gHwndButtonBrowseDir = CreateWindow(WC_BUTTON, _T("&..."),
                                        BS_PUSHBUTTON | WS_CHILD | WS_TABSTOP,
                                        r.dx - rc.x - staticDy, rc.y, staticDy, staticDy,
                                        hwnd, (HMENU)ID_BUTTON_BROWSE, ghinst, NULL);
    SetWindowFont(gHwndButtonBrowseDir, gFontDefault, TRUE);
    rc.y += 2 * staticDy;

    ScopedMem<TCHAR> defaultViewer(GetDefaultPdfViewer());
    BOOL hasOtherViewer = !str::EqI(defaultViewer, TAPP);
    BOOL isSumatraDefaultViewer = defaultViewer && !hasOtherViewer;

    // only show the checbox if Sumatra is not already a default viewer.
    // the alternative (disabling the checkbox) is more confusing
    if (!isSumatraDefaultViewer) {
        gHwndCheckboxRegisterDefault = CreateWindow(
            WC_BUTTON, _T("Use ") TAPP _T(" as the &default PDF reader"),
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
        WC_BUTTON, _T("Install PDF &browser plugin for Firefox, Chrome and Opera"),
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
            WC_BUTTON, _T("Let Windows Desktop Search &search PDF documents"),
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            rc.x, rc.y, r.dx - 2 * rc.x, staticDy,
            hwnd, (HMENU)ID_CHECKBOX_PDF_FILTER, ghinst, NULL);
        SetWindowFont(gHwndCheckboxRegisterPdfFilter, gFontDefault, TRUE);
        Button_SetCheck(gHwndCheckboxRegisterPdfFilter, gGlobalData.installPdfFilter || IsPdfFilterInstalled());
        rc.y += staticDy;
    }

    // for Windows XP, this means only basic thumbnail support
    gHwndCheckboxRegisterPdfPreviewer = CreateWindow(
        WC_BUTTON, _T("Let Windows show &previews of PDF documents"),
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
    gHwndFrame = CreateWindow(
        INSTALLER_FRAME_CLASS_NAME, TAPP _T(" ") CURR_VERSION_STR _T(" Installer"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        dpiAdjust(INSTALLER_WIN_DX), dpiAdjust(INSTALLER_WIN_DY),
        NULL, NULL,
        ghinst, NULL);
}

void ShowUsage()
{
    MessageBox(NULL, TAPP _T("-install.exe [/s][/d <path>][/register][/opt plugin,...]\n\
    \n\
    /s\tinstalls ") TAPP _T(" silently (without user interaction).\n\
    /d\tchanges the directory where ") TAPP _T(" will be installed.\n\
    /register\tregisters ") TAPP _T(" as the default PDF viewer.\n\
    /opt\tenables optional components (currently: plugin, pdffilter, pdfpreviewer)."), TAPP _T(" Installer Usage"), MB_OK | MB_ICONINFORMATION);
}
