/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define INSTALLER_FRAME_CLASS_NAME L"SUMATRA_PDF_INSTALLER_FRAME"

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

struct InstUninstGlobals {
    WCHAR* firstError;
};

struct ButtonCtrl;

extern InstUninstGlobals gInstUninstGlobals;
extern Flags* gCli;
extern const WCHAR* gDefaultMsg;
extern HWND gHwndFrame;
const WCHAR** getSupportedExts();
extern ButtonCtrl* gButtonExit;
extern ButtonCtrl* gButtonInstUninst;
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

struct ButtonCtrl;

void InitInstallerUninstaller();
void OnPaintFrame(HWND hwnd);
void OnButtonExit();
void AnimStep();
void CreateButtonExit(HWND hwndParent);
ButtonCtrl* CreateDefaultButtonCtrl(HWND hwndParent, const WCHAR* s);
void InstallPdfFilter();
void InstallPdfPreviewer();
void UninstallPdfFilter();
void UninstallPdfPreviewer();
void UninstallBrowserPlugin();
bool CheckInstallUninstallPossible(bool silent = false);
WCHAR* GetInstallDirNoFree();
WCHAR* GetInstalledExePath();
WCHAR* GetUninstallerPath();
WCHAR* GetInstalledBrowserPluginPath();
WCHAR* GetBrowserPluginPath();
WCHAR* GetPdfFilterPath();
WCHAR* GetPdfPreviewerPath();
WCHAR* GetShortcutPath(int csidl);
int KillProcess(const WCHAR* processPath, bool waitUntilTerminated);
void NotifyFailed(const WCHAR* msg);
void SetMsg(const WCHAR* msg, Gdiplus::Color color);
void SetDefaultMsg();
const WCHAR* GetOwnPath();
