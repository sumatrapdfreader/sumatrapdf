/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define CANVAS_CLASS_NAME L"SUMATRA_PDF_CANVAS"
#define FRAME_CLASS_NAME L"SUMATRA_PDF_FRAME"
#define SUMATRA_WINDOW_TITLE L"SumatraPDF"

#define WEBSITE_MAIN_URL L"https://www.sumatrapdfreader.org/"
#define WEBSITE_MANUAL_URL L"https://www.sumatrapdfreader.org/manual.html"
#define WEBSITE_TRANSLATIONS_URL L"https://www.notion.so/How-to-contribute-translation-da023536e9774982b09937c8ed8cf6c1"

#ifndef CRASH_REPORT_URL
#define CRASH_REPORT_URL L"https://www.notion.so/Join-the-project-as-a-developer-be6ef085e89f49038c2b671c0743b690"
#endif

// scrolls half a page down/up (needed for Shift+Up/Down)
#define SB_HPAGEUP (WM_USER + 1)
#define SB_HPAGEDOWN (WM_USER + 2)

#define HIDE_CURSOR_TIMER_ID 3
#define HIDE_CURSOR_DELAY_IN_MS 3000

#define REPAINT_TIMER_ID 1
#define REPAINT_MESSAGE_DELAY_IN_MS 1000

#define AUTO_RELOAD_TIMER_ID 5
#define AUTO_RELOAD_DELAY_IN_MS 100

#define EBOOK_LAYOUT_TIMER_ID 7

// permissions that can be revoked through sumatrapdfrestrict.ini or the -restrict command line flag
enum {
    // enables Update checks, crash report submitting and hyperlinks
    Perm_InternetAccess = 1 << 0,
    // enables opening and saving documents and launching external viewers
    Perm_DiskAccess = 1 << 1,
    // enables persistence of preferences to disk (includes the Frequently Read page and Favorites)
    Perm_SavePreferences = 1 << 2,
    // enables setting as default viewer
    Perm_RegistryAccess = 1 << 3,
    // enables printing
    Perm_PrinterAccess = 1 << 4,
    // enables image/text selections and selection copying (if permitted by the document)
    Perm_CopySelection = 1 << 5,
    // enables fullscreen and presentation view modes
    Perm_FullscreenAccess = 1 << 6,
    // enables all of the above
    Perm_All = 0x0FFFFFF,
    // set if either sumatrapdfrestrict.ini or the -restrict command line flag is present
    Perm_RestrictedUse = 0x1000000,
};

enum MenuToolbarFlags {
    MF_NO_TRANSLATE = 1 << 0,
    MF_PLUGIN_MODE_ONLY = 1 << 1,
    MF_NOT_FOR_CHM = 1 << 2,
    MF_NOT_FOR_EBOOK_UI = 1 << 3,
    MF_CBX_ONLY = 1 << 4,
#define PERM_FLAG_OFFSET 5
    MF_REQ_INET_ACCESS = Perm_InternetAccess << PERM_FLAG_OFFSET,
    MF_REQ_DISK_ACCESS = Perm_DiskAccess << PERM_FLAG_OFFSET,
    MF_REQ_PREF_ACCESS = Perm_SavePreferences << PERM_FLAG_OFFSET,
    MF_REQ_PRINTER_ACCESS = Perm_PrinterAccess << PERM_FLAG_OFFSET,
    MF_REQ_ALLOW_COPY = Perm_CopySelection << PERM_FLAG_OFFSET,
    MF_REQ_FULLSCREEN = Perm_FullscreenAccess << PERM_FLAG_OFFSET,
};

/* styling for About/Properties windows */

#define LEFT_TXT_FONT L"Arial"
#define LEFT_TXT_FONT_SIZE 12
#define RIGHT_TXT_FONT L"Arial Black"
#define RIGHT_TXT_FONT_SIZE 12

class Controller;
class Favorites;
class FileHistory;
class WindowInfo;
class NotificationWnd;
class RenderCache;
class TabInfo;
struct LabelWithCloseWnd;
struct SessionData;
struct DropDownCtrl;

// all defined in SumatraPDF.cpp
extern bool gDebugShowLinks;
extern bool gShowFrameRate;

extern const WCHAR* gPluginURL;
extern std::vector<WindowInfo*> gWindows;
extern Favorites gFavorites;
extern FileHistory gFileHistory;
extern WNDPROC DefWndProcCloseButton;
extern RenderCache gRenderCache;
extern bool gShowFrameRate;
extern bool gSuppressAltKey;
extern HBITMAP gBitmapReloadingCue;
extern HCURSOR gCursorDrag;
extern bool gCrashOnOpen;

#define gPluginMode (gPluginURL != nullptr)

void InitializePolicies(bool restrict);
void RestrictPolicies(int revokePermission);
bool HasPermission(int permission);
bool IsUIRightToLeft();
bool LaunchBrowser(const WCHAR* url);
bool OpenFileExternally(const WCHAR* path);
void AssociateExeWithPdfExtension();
void CloseTab(WindowInfo* win, bool quitIfLast = false);
bool MayCloseWindow(WindowInfo* win);
void CloseWindow(WindowInfo* win, bool quitIfLast, bool forceClose = false);
void SetSidebarVisibility(WindowInfo* win, bool tocVisible, bool favVisible);
void RememberFavTreeExpansionState(WindowInfo* win);
void LayoutTreeContainer(LabelWithCloseWnd* l, DropDownCtrl*, HWND hwndTree);
void AdvanceFocus(WindowInfo* win);
bool WindowInfoStillValid(WindowInfo* win);
void SetCurrentLanguageAndRefreshUI(const char* langCode);
void UpdateDocumentColors();
void UpdateTabFileDisplayStateForWin(WindowInfo* win, TabInfo* td);
bool FrameOnKeydown(WindowInfo* win, WPARAM key, LPARAM lparam, bool inTextfield = false);
void ReloadDocument(WindowInfo* win, bool autorefresh = false);
void OnMenuViewFullscreen(WindowInfo* win, bool presentation = false);
void RelayoutWindow(WindowInfo* win);

WindowInfo* FindWindowInfoByHwnd(HWND hwnd);
// note: background tabs are only searched if focusTab is true
WindowInfo* FindWindowInfoByFile(const WCHAR* file, bool focusTab);
WindowInfo* FindWindowInfoBySyncFile(const WCHAR* file, bool focusTab);
WindowInfo* FindWindowInfoByTab(TabInfo* tab);
WindowInfo* FindWindowInfoByController(Controller* ctrl);

// LoadDocument carries a lot of state, this holds them in
// one place
struct LoadArgs {
    explicit LoadArgs(const WCHAR* fileName, WindowInfo* win) {
        this->fileName = fileName;
        this->win = win;
    }

    const WCHAR* fileName = nullptr;
    WindowInfo* win = nullptr;
    bool showWin = true;
    bool forceReuse = false;
    // over-writes placeWindow and other flags and forces no changing
    // of window location after loading
    bool noPlaceWindow = false;

    // for internal use
    bool isNewWindow = false;
    bool placeWindow = true;
};

WindowInfo* LoadDocument(LoadArgs& args);
WindowInfo* CreateAndShowWindowInfo(SessionData* data = nullptr);

UINT MbRtlReadingMaybe();
void MessageBoxWarning(HWND hwnd, const WCHAR* msg, const WCHAR* title = nullptr);
void UpdateCursorPositionHelper(WindowInfo* win, PointI pos, NotificationWnd* wnd);
bool DocumentPathExists(const WCHAR* path);
void EnterFullScreen(WindowInfo* win, bool presentation = false);
void ExitFullScreen(WindowInfo* win);
void SetCurrentLang(const char* langCode);
void GetFixedPageUiColors(COLORREF& text, COLORREF& bg);
void GetEbookUiColors(COLORREF& text, COLORREF& bg);
void RebuildMenuBarForWindow(WindowInfo* win);
void UpdateCheckAsync(WindowInfo* win, bool autoCheck);
void DeleteWindowInfo(WindowInfo* win);

LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
