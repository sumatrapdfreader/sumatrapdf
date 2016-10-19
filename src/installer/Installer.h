/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define EXENAME             APP_NAME_STR L".exe"

#define INSTALLER_FRAME_CLASS_NAME    L"SUMATRA_PDF_INSTALLER_FRAME"

#define INSTALLER_WIN_DX    420
#define INSTALLER_WIN_DY    340

#define WIN_BG_COLOR RGB(0xff, 0xf2, 0) // yellow

#define WINDOW_MARGIN   dpiAdjust(8)

/* The window is divided in three parts:
- top part, where we display nice graphics
- middle part, where we either display messages or advanced options
- bottom part, with install/uninstall button
*/

// This is the height of the lower part
extern int gBottomPartDy;

extern int gButtonDy;

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
#define REG_PATH_UNINST     L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" APP_NAME_STR

#define REG_CLASSES_APP     L"Software\\Classes\\" APP_NAME_STR
#define REG_CLASSES_PDF     L"Software\\Classes\\.pdf"
#define REG_CLASSES_APPS    L"Software\\Classes\\Applications\\" EXENAME

#define REG_EXPLORER_PDF_EXT  L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.pdf"
#define PROG_ID               L"ProgId"
#define APPLICATION           L"Application"

#ifndef _WIN64
#define REG_PATH_PLUGIN     L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin"
#else
#define REG_PATH_PLUGIN     L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin_x64"
#endif

#define ID_BUTTON_EXIT                11

#define WM_APP_INSTALLATION_FINISHED        (WM_APP + 1)

struct GlobalData {
    bool    silent;
    bool    showUsageAndQuit;
    WCHAR * installDir;
#ifndef BUILD_UNINSTALLER
    bool    registerAsDefault;
    bool    installPdfFilter;
    bool    installPdfPreviewer;
    bool    keepBrowserPlugin;
    bool    justExtractFiles;
    bool    autoUpdate;
#endif

    WCHAR * firstError;
    HANDLE  hThread;
    bool    success;
};

struct PayloadInfo {
    const char *fileName;
    bool install;
};

extern GlobalData   gGlobalData;
extern PayloadInfo  gPayloadData[];
extern WCHAR *      gSupportedExts[];
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

void NotifyFailed(const WCHAR *msg);
void SetMsg(const WCHAR *msg, Gdiplus::Color color);
WCHAR *GetInstalledExePath();
WCHAR *GetInstalledBrowserPluginPath();
void OnCreateWindow(HWND hwnd);
void ShowUsage();
void CreateMainWindow();
const WCHAR *GetOwnPath();
bool OnWmCommand(WPARAM wParam);
bool CreateProcessHelper(const WCHAR *exe, const WCHAR *args=nullptr);
WCHAR *GetUninstallerPath();
int KillProcess(const WCHAR *processPath, bool waitUntilTerminated);
void UninstallBrowserPlugin();
void UninstallPdfFilter();
void UninstallPdfPreviewer();
void KillSumatra();
WCHAR *GetShortcutPath(bool allUsers);
void InvalidateFrame();
bool CheckInstallUninstallPossible(bool silent=false);
void CreateButtonExit(HWND hwndParent);
void OnButtonExit();
HWND CreateButton(HWND hwndParent, const WCHAR *s, int id, DWORD style, SIZE& sizeOut);
HWND CreateDefaultButton(HWND hwndParent, const WCHAR *s, int id);
SIZE SetButtonTextAndResize(HWND hwnd, const WCHAR * s);
SIZE GetIdealButtonSize(HWND hwnd);
int dpiAdjust(int value);
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
bool IsPdfFilterInstalled();
bool IsPdfPreviewerInstalled();
DWORD WINAPI InstallerThread(LPVOID data);

#endif
