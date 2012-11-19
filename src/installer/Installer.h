/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Installer_h
#define Installer_h

#include <Tlhelp32.h>
#include <objidl.h>
#include <io.h>

#include "FileTransactions.h"
#include "FileUtil.h"

#include "Resource.h"
#include "Timer.h"
#include "Version.h"
#include "WinUtil.h"

#define TAPP                L"SumatraPDF"
#define EXENAME             TAPP L".exe"

#define INSTALLER_FRAME_CLASS_NAME    L"SUMATRA_PDF_INSTALLER_FRAME"

#define INSTALLER_WIN_DX    420
#define INSTALLER_WIN_DY    340

#define PUSH_BUTTON_DY  dpiAdjust(22)

#define WINDOW_MARGIN   8
// The window is divided in three parts:
// * top part, where we display nice graphics
// * middle part, where we either display messages or advanced options
// * bottom part, with install/uninstall button
// This is the height of the top part
#define TITLE_PART_DY  dpiAdjust(110)
// This is the height of the lower part
#define BOTTOM_PART_DY (PUSH_BUTTON_DY + 2 * WINDOW_MARGIN)

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
#define REG_PATH_UNINST     L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" TAPP
// Legacy key, only read during an update and removed at uninstallation
#define REG_PATH_SOFTWARE   L"Software\\" TAPP

#define REG_CLASSES_APP     L"Software\\Classes\\" TAPP
#define REG_CLASSES_PDF     L"Software\\Classes\\.pdf"
#define REG_CLASSES_APPS    L"Software\\Classes\\Applications\\" EXENAME

#define REG_EXPLORER_PDF_EXT  L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"
#define PROG_ID               L"ProgId"
#define APPLICATION           L"Application"

#define REG_PATH_PLUGIN     L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin"
#define PLUGIN_PATH         L"Path"

// Keys we'll set in REG_PATH_UNINST path

// REG_SZ, a path to installed executable (or "$path,0" to force the first icon)
#define DISPLAY_ICON _T("DisplayIcon")
// REG_SZ, e.g "SumatraPDF" (TAPP)
#define DISPLAY_NAME _T("DisplayName")
// REG_SZ, e.g. "1.2" (CURR_VERSION_STR)
#define DISPLAY_VERSION _T("DisplayVersion")
// REG_DWORD, get size of installed directory after copying files
#define ESTIMATED_SIZE _T("EstimatedSize")
// REG_SZ, the current date as YYYYMMDD
#define INSTALL_DATE _T("InstallDate")
// REG_DWORD, set to 1
#define NO_MODIFY _T("NoModify")
// REG_DWORD, set to 1
#define NO_REPAIR _T("NoRepair")
// REG_SZ, e.g. "Krzysztof Kowalczyk" (PUBLISHER_STR)
#define PUBLISHER _T("Publisher")
// REG_SZ, path to uninstaller exe
#define UNINSTALL_STRING _T("UninstallString")
// REG_SZ, e.g. "http://blog.kowalczyk.info/software/sumatrapdf/"
#define URL_INFO_ABOUT _T("URLInfoAbout")
// REG_SZ, e.g. "http://blog.kowalczyk.info/software/sumatrapdf/news.html"
#define URL_UPDATE_INFO _T("URLUpdateInfo")
// REG_SZ, same as INSTALL_DIR below
#define INSTALL_LOCATION _T("InstallLocation")

// Installation directory (set in HKLM REG_PATH_SOFTWARE
// for compatibility with the old NSIS installer)
#define INSTALL_DIR _T("Install_Dir")

#define ID_BUTTON_EXIT                11

#define WM_APP_INSTALLATION_FINISHED        (WM_APP + 1)

struct GlobalData {
    bool    silent;
    bool    showUsageAndQuit;
    WCHAR * installDir;
#ifndef BUILD_UNINSTALLER
    bool    registerAsDefault;
    bool    installBrowserPlugin;
    bool    installPdfFilter;
    bool    installPdfPreviewer;
#endif

    WCHAR * firstError;
    HANDLE  hThread;
    bool    success;
};

struct PayloadInfo {
    char *filepath;
    bool install;
};

extern GlobalData   gGlobalData;
extern PayloadInfo  gPayloadData[];
extern WCHAR *      gSupportedExts[];
extern HINSTANCE    ghinst;
extern HWND         gHwndFrame;
extern HWND         gHwndButtonExit;
extern HWND         gHwndButtonInstUninst;
extern HFONT        gFontDefault;
extern WCHAR *      gMsgError;
extern bool         gShowOptions;
extern bool         gForceCrash;
extern bool         gReproBug;

extern Gdiplus::Color COLOR_MSG_WELCOME;
extern Gdiplus::Color COLOR_MSG_OK;
extern Gdiplus::Color COLOR_MSG_INSTALLATION;
extern Gdiplus::Color COLOR_MSG_FAILED;

void NotifyFailed(WCHAR *msg);
WCHAR *GetInstalledExePath();
void OnCreateWindow(HWND hwnd);
void ShowUsage();
void CreateMainWindow();
WCHAR *GetOwnPath();
bool OnWmCommand(WPARAM wParam);
bool CreateProcessHelper(const WCHAR *exe, const WCHAR *args=NULL);
WCHAR *GetUninstallerPath();
int KillProcess(WCHAR *processPath, BOOL waitUntilTerminated);
void UninstallBrowserPlugin();
void UninstallPdfFilter();
void UninstallPdfPreviewer();
void KillSumatra();
WCHAR *GetShortcutPath(bool allUsers);
void SetMsg(WCHAR *msg, Gdiplus::Color color);
void InvalidateFrame();
bool CheckInstallUninstallPossible(bool silent=false);
void CreateButtonExit(HWND hwndParent);
void OnButtonExit();
HWND CreateDefaultButton(HWND hwndParent, const WCHAR *label, int width, int id=IDOK);
int dpiAdjust(int value);
void InstallBrowserPlugin();
void InstallPdfFilter();
void InstallPdfPreviewer();

#ifdef BUILD_UNINSTALLER

bool ExecuteUninstallerFromTempDir();
BOOL IsUninstallerNeeded();
void OnUninstallationFinished();
DWORD WINAPI UninstallerThread(LPVOID data);

#else

extern HWND gHwndButtonRunSumatra;
bool IsValidInstaller();
void OnInstallationFinished();
bool IsBrowserPluginInstalled();
bool IsPdfFilterInstalled();
bool IsPdfPreviewerInstalled();
DWORD WINAPI InstallerThread(LPVOID data);

#endif

#endif
