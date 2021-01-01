/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define INSTALLER_FRAME_CLASS_NAME L"SUMATRA_PDF_INSTALLER_FRAME"

#define BROWSER_PLUGIN_NAME L"npPdfViewer.dll"
#define SEARCH_FILTER_DLL_NAME L"PdfFilter.dll"
#define PREVIEW_DLL_NAME L"PdfPreview.dll"

#define INSTALLER_WIN_DX 420
#define INSTALLER_WIN_DY 340

#define WIN_BG_COLOR RGB(0xff, 0xf2, 0) // yellow

// TODO: should scale
#define WINDOW_MARGIN DpiScale(8)

/* The window is divided in three parts:
- top part, where we display nice graphics
- middle part, where we either display messages or advanced options
- bottom part, with install/uninstall button
*/

// This is the height of the lower part
extern int gBottomPartDy;

extern int gButtonDy;

#define WM_APP_INSTALLATION_FINISHED (WM_APP + 1)

struct ButtonCtrl;

extern WCHAR* firstError;
extern Flags* gCli;
extern const WCHAR* gDefaultMsg;
extern HWND gHwndFrame;
extern HFONT gFontDefault;
extern WCHAR* gMsgError;
extern bool gShowOptions;
extern bool gReproBug;

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

ButtonCtrl* CreateDefaultButtonCtrl(HWND hwndParent, const WCHAR* s);
void InitInstallerUninstaller();
void OnPaintFrame(HWND hwnd);
void AnimStep();
void NotifyFailed(const WCHAR* msg);
void SetMsg(const WCHAR* msg, Gdiplus::Color color);
void SetDefaultMsg();

int KillProcessesWithModule(const WCHAR* modulePath, bool waitUntilTerminated);

const WCHAR** GetSupportedExts();

WCHAR* GetShortcutPath(int csidl);

WCHAR* GetExistingInstallationDir();
WCHAR* GetInstallDirNoFree();
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
