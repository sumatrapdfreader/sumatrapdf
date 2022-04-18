/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define kInstallerWindowClassName L"SUMATRA_PDF_INSTALLER_FRAME"

#define SEARCH_FILTER_DLL_NAME L"PdfFilter.dll"
#define PREVIEW_DLL_NAME L"PdfPreview.dll"

#define kInstallerWinDx 420
#define kInstallerWinDy 340

/* The window is divided in three parts:
- top part, where we display nice graphics
- middle part, where we either display messages or advanced options
- bottom part, with install/uninstall button
*/

// This is the height of the lower part
extern int gBottomPartDy;

extern int gButtonDy;

#define WM_APP_INSTALLATION_FINISHED (WM_APP + 1)
#define WM_APP_START_INSTALLATION (WM_APP + 2)

extern WCHAR* firstError;
extern const WCHAR* gDefaultMsg;
extern HWND gHwndFrame;
extern WCHAR* gMsgError;

extern Gdiplus::Color COLOR_MSG_WELCOME;
extern Gdiplus::Color COLOR_MSG_OK;
extern Gdiplus::Color COLOR_MSG_INSTALLATION;
extern Gdiplus::Color COLOR_MSG_FAILED;
extern Gdiplus::Color gCol1;
extern Gdiplus::Color gCol1Shadow;
extern Gdiplus::Color gCol2;
extern Gdiplus::Color gCol2Shadow;
extern Gdiplus::Color gCol3;
extern Gdiplus::Color gCol3Shadow;
extern Gdiplus::Color gCol4;
extern Gdiplus::Color gCol4Shadow;
extern Gdiplus::Color gCol5;
extern Gdiplus::Color gCol5Shadow;

void OnPaintFrame(HWND hwnd, bool skipoMessage);
void AnimStep();
void NotifyFailed(const WCHAR* msg);
void SetMsg(const WCHAR* msg, Gdiplus::Color color);
void SetDefaultMsg();

int KillProcessesWithModule(const WCHAR* modulePath, bool waitUntilTerminated);

const WCHAR** GetSupportedExts();

WCHAR* GetShortcutPath(int csidl);

bool ExtractInstallerFiles();
WCHAR* GetExistingInstallationDir();
WCHAR* GetInstallDirTemp();
WCHAR* GetInstalledExePath();

WCHAR* GetInstallationFilePath(const WCHAR* name);
WCHAR* GetExistingInstallationFilePath(const WCHAR* name);

void RegisterPreviewer(bool silent);
void UnRegisterPreviewer(bool silent);
bool IsPreviewerInstalled();

void RegisterSearchFilter(bool silent);
void UnRegisterSearchFilter(bool silent);
bool IsSearchFilterInstalled();

void UninstallBrowserPlugin();

bool CheckInstallUninstallPossible(bool silent = false);
char* GetInstallerLogPath();
void ShowLogFile(const char* logPath);
