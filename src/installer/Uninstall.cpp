/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef BUILD_UNINSTALLER
#error "BUILD_UNINSTALLER must be defined!!!"
#endif

#include "Installer.h"

#define UNINSTALLER_WIN_DX  INSTALLER_WIN_DX
#define UNINSTALLER_WIN_DY  INSTALLER_WIN_DY

// Try harder getting temporary directory
// Caller needs to free() the result.
// Returns NULL if fails for any reason.
static WCHAR *GetValidTempDir()
{
    ScopedMem<WCHAR> d(path::GetTempPath());
    if (!d) {
        NotifyFailed(_TR("Couldn't obtain temporary directory"));
        return NULL;
    }
    bool ok = dir::Create(d);
    if (!ok) {
        LogLastError();
        NotifyFailed(_TR("Couldn't create temporary directory"));
        return NULL;
    }
    return d.StealData();
}

static WCHAR *GetTempUninstallerPath()
{
    ScopedMem<WCHAR> tempDir(GetValidTempDir());
    if (!tempDir)
        return NULL;
    // Using fixed (unlikely) name instead of GetTempFileName()
    // so that we don't litter temp dir with copies of ourselves
    return path::Join(tempDir, L"sum~inst.exe");
}

BOOL IsUninstallerNeeded()
{
    ScopedMem<WCHAR> exePath(GetInstalledExePath());
    return file::Exists(exePath);
}

static bool RemoveUninstallerRegistryInfo(HKEY hkey)
{
    bool ok1 = DeleteRegKey(hkey, REG_PATH_UNINST);
    // this key was added by installers up to version 1.8
    bool ok2 = DeleteRegKey(hkey, REG_PATH_SOFTWARE);
    return ok1 && ok2;
}

/* Undo what DoAssociateExeWithPdfExtension() in AppTools.cpp did */
static void UnregisterFromBeingDefaultViewer(HKEY hkey)
{
    ScopedMem<WCHAR> curr(ReadRegStr(hkey, REG_CLASSES_PDF, NULL));
    ScopedMem<WCHAR> prev(ReadRegStr(hkey, REG_CLASSES_APP, L"previous.pdf"));
    if (!curr || !str::Eq(curr, APP_NAME_STR)) {
        // not the default, do nothing
    } else if (prev) {
        WriteRegStr(hkey, REG_CLASSES_PDF, NULL, prev);
    } else {
        SHDeleteValue(hkey, REG_CLASSES_PDF, NULL);
    }

    // the following settings overrule HKEY_CLASSES_ROOT\.pdf
    ScopedMem<WCHAR> buf(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID));
    if (str::Eq(buf, APP_NAME_STR)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, PROG_ID);
        if (res != ERROR_SUCCESS)
            LogLastError(res);
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION));
    if (str::EqI(buf, EXENAME)) {
        LONG res = SHDeleteValue(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT, APPLICATION);
        if (res != ERROR_SUCCESS)
            LogLastError(res);
    }
    buf.Set(ReadRegStr(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", PROG_ID));
    if (str::Eq(buf, APP_NAME_STR))
        DeleteRegKey(HKEY_CURRENT_USER, REG_EXPLORER_PDF_EXT L"\\UserChoice", true);
}

static bool DeleteEmptyRegKey(HKEY root, const WCHAR *keyName)
{
    HKEY hkey;
    if (RegOpenKeyEx(root, keyName, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return true;

    DWORD subkeys, values;
    bool isEmpty = false;
    if (RegQueryInfoKey(hkey, NULL, NULL, NULL, &subkeys, NULL, NULL,
                        &values, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        isEmpty = 0 == subkeys && 0 == values;
    }
    RegCloseKey(hkey);

    if (isEmpty)
        DeleteRegKey(root, keyName);
    return isEmpty;
}

static void RemoveOwnRegistryKeys()
{
    HKEY keys[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    // remove all keys from both HKLM and HKCU (wherever they exist)
    for (int i = 0; i < dimof(keys); i++) {
        UnregisterFromBeingDefaultViewer(keys[i]);
        DeleteRegKey(keys[i], REG_CLASSES_APP);
        DeleteRegKey(keys[i], REG_CLASSES_APPS);
        SHDeleteValue(keys[i], REG_CLASSES_PDF L"\\OpenWithProgids", APP_NAME_STR);

        for (int j = 0; NULL != gSupportedExts[j]; j++) {
            ScopedMem<WCHAR> keyname(str::Join(L"Software\\Classes\\", gSupportedExts[j], L"\\OpenWithProgids"));
            SHDeleteValue(keys[i], keyname, APP_NAME_STR);
            DeleteEmptyRegKey(keys[i], keyname);

            keyname.Set(str::Join(L"Software\\Classes\\", gSupportedExts[j], L"\\OpenWithList\\" EXENAME));
            if (!DeleteRegKey(keys[i], keyname))
                continue;
            // remove empty keys that the installer might have created
            *(WCHAR *)str::FindCharLast(keyname, '\\') = '\0';
            if (!DeleteEmptyRegKey(keys[i], keyname))
                continue;
            *(WCHAR *)str::FindCharLast(keyname, '\\') = '\0';
            DeleteEmptyRegKey(keys[i], keyname);
        }
    }
}

static BOOL RemoveEmptyDirectory(const WCHAR *dir)
{
    WIN32_FIND_DATA findData;
    BOOL success = TRUE;

    ScopedMem<WCHAR> dirPattern(path::Join(dir, L"*"));
    HANDLE h = FindFirstFile(dirPattern, &findData);
    if (h != INVALID_HANDLE_VALUE)
    {
        do {
            ScopedMem<WCHAR> path(path::Join(dir, findData.cFileName));
            DWORD attrs = findData.dwFileAttributes;
            // filter out directories. Even though there shouldn't be any
            // subdirectories, it also filters out the standard "." and ".."
            if ((attrs & FILE_ATTRIBUTE_DIRECTORY) &&
                !str::Eq(findData.cFileName, L".") &&
                !str::Eq(findData.cFileName, L"..")) {
                success &= RemoveEmptyDirectory(path);
            }
        } while (FindNextFile(h, &findData) != 0);
        FindClose(h);
    }

    if (!RemoveDirectory(dir)) {
        DWORD lastError = GetLastError();
        if (ERROR_DIR_NOT_EMPTY != lastError && ERROR_FILE_NOT_FOUND != lastError) {
            LogLastError(lastError);
            success = FALSE;
        }
    }

    return success;
}

static BOOL RemoveInstalledFiles()
{
    BOOL success = TRUE;

    for (int i = 0; NULL != gPayloadData[i].fileName; i++) {
        ScopedMem<WCHAR> relPath(str::conv::FromUtf8(gPayloadData[i].fileName));
        ScopedMem<WCHAR> path(path::Join(gGlobalData.installDir, relPath));

        if (file::Exists(path))
            success &= DeleteFile(path);
    }

    RemoveEmptyDirectory(gGlobalData.installDir);
    return success;
}


// If this is uninstaller and we're running from installation directory,
// copy uninstaller to temp directory and execute from there, exiting
// ourselves. This is needed so that uninstaller can delete itself
// from installation directory and remove installation directory
// If returns TRUE, this is an installer and we sublaunched ourselves,
// so the caller needs to exit
bool ExecuteUninstallerFromTempDir()
{
    // only need to sublaunch if running from installation dir
    ScopedMem<WCHAR> ownDir(path::GetDir(GetOwnPath()));
    ScopedMem<WCHAR> tempPath(GetTempUninstallerPath());

    // no temp directory available?
    if (!tempPath)
        return false;

    // not running from the installation directory?
    // (likely a test uninstaller that shouldn't be removed anyway)
    if (!path::IsSame(ownDir, gGlobalData.installDir))
        return false;

    // already running from temp directory?
    if (path::IsSame(GetOwnPath(), tempPath))
        return false;

    if (!CopyFile(GetOwnPath(), tempPath, FALSE)) {
        NotifyFailed(_TR("Failed to copy uninstaller to temp directory"));
        return false;
    }

    ScopedMem<WCHAR> args(str::Format(L"/d \"%s\" %s", gGlobalData.installDir, gGlobalData.silent ? L"/s" : L""));
    bool ok = CreateProcessHelper(tempPath, args);

    // mark the uninstaller for removal at shutdown (note: works only for administrators)
    MoveFileEx(tempPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    return ok;
}

static bool RemoveShortcut(bool allUsers)
{
    ScopedMem<WCHAR> p(GetShortcutPath(allUsers));
    if (!p.Get())
        return false;
    bool ok = DeleteFile(p);
    if (!ok && (ERROR_FILE_NOT_FOUND != GetLastError())) {
        LogLastError();
        return false;
    }
    return true;
}

DWORD WINAPI UninstallerThread(LPVOID /* data */)
{
    // also kill the original uninstaller, if it's just spawned
    // a DELETE_ON_CLOSE copy from the temp directory
    WCHAR *exePath = GetUninstallerPath();
    if (!path::IsSame(exePath, GetOwnPath()))
        KillProcess(exePath, TRUE);
    free(exePath);

    if (!RemoveUninstallerRegistryInfo(HKEY_LOCAL_MACHINE) &&
        !RemoveUninstallerRegistryInfo(HKEY_CURRENT_USER)) {
        NotifyFailed(_TR("Failed to delete uninstaller registry keys"));
    }

    if (!RemoveShortcut(true) && !RemoveShortcut(false))
        NotifyFailed(_TR("Couldn't remove the shortcut"));

    UninstallBrowserPlugin();
    UninstallPdfFilter();
    UninstallPdfPreviewer();
    RemoveOwnRegistryKeys();

    if (!RemoveInstalledFiles())
        NotifyFailed(_TR("Couldn't remove installation directory"));

    // always succeed, even for partial uninstallations
    gGlobalData.success = true;

    if (!gGlobalData.silent)
        PostMessage(gHwndFrame, WM_APP_INSTALLATION_FINISHED, 0, 0);
    return 0;
}

static void OnButtonUninstall()
{
    KillSumatra();

    if (!CheckInstallUninstallPossible())
        return;

    // disable the button during uninstallation
    EnableWindow(gHwndButtonInstUninst, FALSE);
    SetMsg(_TR("Uninstallation in progress..."), COLOR_MSG_INSTALLATION);
    InvalidateFrame();

    gGlobalData.hThread = CreateThread(NULL, 0, UninstallerThread, NULL, 0, 0);
}

void OnUninstallationFinished()
{
    DestroyWindow(gHwndButtonInstUninst);
    gHwndButtonInstUninst = NULL;
    CreateButtonExit(gHwndFrame);
    SetMsg(_TR("SumatraPDF has been uninstalled."), gMsgError ? COLOR_MSG_FAILED : COLOR_MSG_OK);
    gMsgError = gGlobalData.firstError;
    InvalidateFrame();

    CloseHandle(gGlobalData.hThread);
}

bool OnWmCommand(WPARAM wParam)
{
    switch (LOWORD(wParam))
    {
        case IDOK:
            if (gHwndButtonInstUninst)
                OnButtonUninstall();
            else if (gHwndButtonExit)
                OnButtonExit();
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

void OnCreateWindow(HWND hwnd)
{
    // TODO: this button might be too narrow for some translations
    gHwndButtonInstUninst = CreateDefaultButton(hwnd, _TR("Uninstall SumatraPDF"), 150);
}

void CreateMainWindow()
{
    ScopedMem<WCHAR> title(str::Format(_TR("SumatraPDF %s Uninstaller"), CURR_VERSION_STR));
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
    MessageBox(NULL, L"uninstall.exe [/s][/d <path>]\n\
    \n\
    /s\tuninstalls " APP_NAME_STR L" silently (without user interaction).\n\
    /d\tchanges the directory from where " APP_NAME_STR L" will be uninstalled.",
    APP_NAME_STR L" Uninstaller Usage", MB_OK | MB_ICONINFORMATION);
}
