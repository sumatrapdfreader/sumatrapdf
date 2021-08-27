/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define CANVAS_CLASS_NAME L"SUMATRA_PDF_CANVAS"
#define FRAME_CLASS_NAME L"SUMATRA_PDF_FRAME"
#define SUMATRA_WINDOW_TITLE L"SumatraPDF"

#define WEBSITE_MAIN_URL L"https://www.sumatrapdfreader.org/"
#define WEBSITE_MANUAL_URL L"https://www.sumatrapdfreader.org/manual.html"
#define WEBSITE_TRANSLATIONS_URL L"https://www.sumatrapdfreader.org/docs/How-to-contribute-translation.html"

#ifndef CRASH_REPORT_URL
#define CRASH_REPORT_URL L"https://www.sumatrapdfreader.org/docs/Join-the-project-as-a-developer.html"
#endif

// scrolls half a page down/up (needed for Shift+Up/Down)
#define SB_HPAGEUP (WM_USER + 102)
#define SB_HPAGEDOWN (WM_USER + 103)

#define HIDE_CURSOR_TIMER_ID 3
#define HIDE_CURSOR_DELAY_IN_MS 3000

#define REPAINT_TIMER_ID 1
#define REPAINT_MESSAGE_DELAY_IN_MS 1000

#define AUTO_RELOAD_TIMER_ID 5
#define AUTO_RELOAD_DELAY_IN_MS 100

// permissions that can be revoked through sumatrapdfrestrict.ini or the -restrict command line flag
enum class Perm : uint {
    // enables Update checks, crash report submitting and hyperlinks
    InternetAccess = 1 << 0,
    // enables opening and saving documents and launching external viewers
    DiskAccess = 1 << 1,
    // enables persistence of preferences to disk (includes the Frequently Read page and Favorites)
    SavePreferences = 1 << 2,
    // enables setting as default viewer
    RegistryAccess = 1 << 3,
    // enables printing
    PrinterAccess = 1 << 4,
    // enables image/text selections and selection copying (if permitted by the document)
    CopySelection = 1 << 5,
    // enables fullscreen and presentation view modes
    FullscreenAccess = 1 << 6,
    // enables all of the above
    All = 0x0FFFFFF,
    // set if either sumatrapdfrestrict.ini or the -restrict command line flag is present
    RestrictedUse = 0x1000000,
};

inline constexpr Perm operator|(Perm lhs, Perm rhs) {
    using T = std::underlying_type_t<Perm>;
    return static_cast<Perm>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

inline constexpr Perm operator&(Perm lhs, Perm rhs) {
    using T = std::underlying_type_t<Perm>;
    return static_cast<Perm>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

inline constexpr Perm operator<<(Perm lhs, uint rhs) {
    using T = std::underlying_type_t<Perm>;
    return static_cast<Perm>(static_cast<T>(lhs) << static_cast<T>(rhs));
}

inline constexpr Perm operator~(Perm lhs) {
    using T = std::underlying_type_t<Perm>;
    T v = static_cast<T>(lhs);
    v = ~v;
    return static_cast<Perm>(v);
}

struct Controller;
class Favorites;
struct FileHistory;
struct WindowInfo;
struct NotificationWnd;
class RenderCache;
struct TabInfo;
struct LabelWithCloseWnd;
struct SessionData;
struct DropDownCtrl;
struct Flags;

// all defined in SumatraPDF.cpp
extern Flags* gCli;
extern bool gDebugShowLinks;
extern bool gShowFrameRate;

extern const WCHAR* gPluginURL;
extern Favorites gFavorites;
extern FileHistory gFileHistory;
extern WNDPROC DefWndProcCloseButton;
extern RenderCache gRenderCache;

extern bool gSuppressAltKey;
extern HBITMAP gBitmapReloadingCue;
extern HCURSOR gCursorDrag;
extern bool gCrashOnOpen;

#define gPluginMode (gPluginURL != nullptr)

void InitializePolicies(bool restrict);
void RestrictPolicies(Perm revokePermission);
bool HasPermission(Perm permission);
bool IsUIRightToLeft();
bool SumatraLaunchBrowser(const WCHAR* url);
bool OpenFileExternally(const WCHAR* path);
void AssociateExeWithPdfExtension();
void CloseCurrentTab(WindowInfo* win, bool quitIfLast = false);
bool CanCloseWindow(WindowInfo* win);
void CloseWindow(WindowInfo* win, bool quitIfLast, bool forceClose);
void SetSidebarVisibility(WindowInfo* win, bool tocVisible, bool showFavorites);
void RememberFavTreeExpansionState(WindowInfo* win);
void LayoutTreeContainer(LabelWithCloseWnd* l, HWND hwndTree);
void AdvanceFocus(WindowInfo* win);
void SetCurrentLanguageAndRefreshUI(const char* langCode);
void UpdateDocumentColors();
void UpdateFixedPageScrollbarsVisibility();
void UpdateTabFileDisplayStateForTab(TabInfo* tab);
bool FrameOnKeydown(WindowInfo* win, WPARAM key, LPARAM lp, bool inTextfield = false);
void ReloadDocument(WindowInfo* win, bool autoRefresh);
void OnMenuViewFullscreen(WindowInfo* win, bool presentation = false);
void RelayoutWindow(WindowInfo* win);

// note: background tabs are only searched if focusTab is true
WindowInfo* FindWindowInfoByFile(const WCHAR* file, bool focusTab);
WindowInfo* FindWindowInfoBySyncFile(const WCHAR* file, bool focusTab);

class EngineBase;

// LoadDocument carries a lot of state, this holds them in
// one place
struct LoadArgs {
    explicit LoadArgs(const WCHAR* fileName, WindowInfo* win) {
        this->fileName = fileName;
        this->win = win;
    }

    explicit LoadArgs(const char* fileName, WindowInfo* win) {
        this->win = win;
        fileNameToFree = strconv::Utf8ToWstr(fileName);
        this->fileName = fileNameToFree;
    }

    ~LoadArgs() {
        str::Free(fileNameToFree);
    }

    // we don't own those values
    EngineBase* engine{nullptr};
    const WCHAR* fileName{nullptr};
    WindowInfo* win{nullptr};

    const WCHAR* fileNameToFree{nullptr};

    bool showWin{true};
    bool forceReuse{false};
    // over-writes placeWindow and other flags and forces no changing
    // of window location after loading
    bool noPlaceWindow{false};

    // for internal use
    bool isNewWindow{false};
    bool placeWindow{true};
    // TODO: this is hacky. I save prefs too frequently. Need to go over
    // and rationalize all prefs::Save() calls
    bool noSavePrefs{false};
};

WindowInfo* LoadDocument(LoadArgs& args);
WindowInfo* CreateAndShowWindowInfo(SessionData* data = nullptr);

uint MbRtlReadingMaybe();
void MessageBoxWarning(HWND hwnd, const WCHAR* msg, const WCHAR* title = nullptr);
void UpdateCursorPositionHelper(WindowInfo* win, Point pos, NotificationWnd* wnd);
bool DocumentPathExists(const WCHAR* path);
void EnterFullScreen(WindowInfo* win, bool presentation = false);
void ExitFullScreen(WindowInfo* win);
void SetCurrentLang(const char* langCode);
void RebuildMenuBarForWindow(WindowInfo* win);
void DeleteWindowInfo(WindowInfo* win);
void SwitchToDisplayMode(WindowInfo* win, DisplayMode displayMode, bool keepContinuous = false);
void WindowInfoRerender(WindowInfo* win, bool includeNonClientArea = false);
LRESULT CALLBACK WndProcSumatraFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void ShutdownCleanup();
bool DocIsSupportedFileType(Kind);
