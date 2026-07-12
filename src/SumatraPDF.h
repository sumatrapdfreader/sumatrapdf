/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct AnnotCreateArgs;
enum class FileType : u8;

#include "OverlayScrollbar.h"

#define CANVAS_CLASS_NAME L"SUMATRA_PDF_CANVAS"
#define FRAME_CLASS_NAME L"SUMATRA_PDF_FRAME"

constexpr int kFrameResizeHitTest = 5;

extern bool gRedrawLog;

constexpr const char* kWebsiteURL = "https://www.sumatrapdfreader.org/";
constexpr const char* kManualURL = "https://www.sumatrapdfreader.org/manual";
constexpr const char* kContributeTranslationsURL = "https://www.sumatrapdfreader.org/docs/Contribute-translation";

#ifndef CRASH_REPORT_URL
#define CRASH_REPORT_URL "https://www.sumatrapdfreader.org/docs/Contribute-to-SumatraPDF"
#endif

// scrolls half a page down/up (needed for Shift+Up/Down)
#define SB_HALF_PAGEUP (WM_USER + 102)
#define SB_HALF_PAGEDOWN (WM_USER + 103)

constexpr int kHideCursorTimerID = 3;
constexpr int kHideCursorDelayInMs = 3000;

#define REPAINT_TIMER_ID 1
#define REPAINT_MESSAGE_DELAY_IN_MS 1000

#define AUTO_RELOAD_TIMER_ID 5

#define READ_ALOUD_HIGHLIGHT_TIMER_ID 8
#define READ_ALOUD_HIGHLIGHT_DELAY_IN_MS 80
// debounce: coalesce bursts of file-change notifications (a single save can
// fire several) into one reload. SetTimer() with the same id resets it, so the
// reload only happens once the file has been quiet for this long (#5690)
#define AUTO_RELOAD_DELAY_IN_MS 500

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

struct Favorites;
struct FileHistory;
struct MainWindow;
struct NotificationWnd;
struct RenderCache;
struct WindowTab;
struct LabelWithCloseWnd;
struct SessionData;
struct Flags;

// all defined in SumatraPDF.cpp
extern Flags* gCli;
extern bool gShowFrameRate;

extern Str gPluginURL;
extern bool gMyWindowWasEmbedded;
extern Favorites gFavorites;
extern WNDPROC DefWndProcCloseButton;
extern RenderCache* gRenderCache;

extern bool gSupressNextAltMenuTrigger;
extern HBITMAP gBitmapReloadingCue;
extern HCURSOR gCursorDrag;
extern bool gCrashOnOpen;
extern HWND gLastActiveFrameHwnd;

struct DocController;
extern DocController* gMostRecentlyOpenedDoc;

struct DocControllerCallback;
DocControllerCallback* CreateControllerCallbackHandler(MainWindow* win);

#define gPluginMode ((bool)gPluginURL)

bool NeedsWindowEmbeddingHacks();
bool SettingsUseTabs();
bool SettingsRestoreSession();
bool SettingsRememberOpenedFiles();

void InitializePolicies(bool restrict);
void RestrictPolicies(Perm revokePermission);
bool HasPermission(Perm permission);
bool CanAccessDisk();
bool AnnotationsAreDisabled();
bool IsUIRtl();
bool SumatraLaunchBrowser(Str url);
TempStr URLEncodeMayTruncateTemp(Str s);
void LaunchDocumentation(Str docURI);
bool MaybeLaunchDocumentation(Str url);
bool OpenFileExternally(Str path);
void CloseCurrentTab(MainWindow* win, bool quitIfLast);
void CloseTab(WindowTab* tab, bool quitIfLast);
// true if read aloud was paused and can be resumed in this tab
bool CanContinueReadAloud(WindowTab* tab);
// false if the user canceled (don't proceed with closing/replacing the doc)
bool MaybeSaveAnnotations(WindowTab* tab);
WindowTab* GetReadAloudSourceTab();

constexpr UINT CmdTtsVoiceDefault = 0x7100;
constexpr UINT CmdTtsVoiceFirst = 0x7101;
constexpr UINT CmdTtsVoiceLast = 0x71ff;
constexpr UINT CmdTtsMenuReadCurrentPage = 0x7200;
constexpr UINT CmdTtsMenuContinueReading = 0x7201;
constexpr UINT CmdTtsMenuReadSelection = 0x7202;
constexpr UINT CmdTtsMenuPauseReading = 0x7203;
constexpr UINT CmdTtsMenuReadFromCursor = 0x7204;
constexpr UINT CmdTtsMenuStopReading = 0x7205;
constexpr UINT CmdTtsSpeedFirst = 0x7300;
constexpr UINT CmdTtsSpeedLast = 0x730f;

TempStr ReadAloudSpeedLabelTemp(float speed);

void RebuildReadAloudMenu(MainWindow* win, HMENU menu, bool includeCursorItem = false, bool canReadFromCursor = false);
bool HandleReadAloudMenuCommand(MainWindow* win, int cmdId);
void SetReadAloudAppSubmenu(HMENU menu);
bool IsReadAloudAppSubmenu(HMENU menu);
void SetReadAloudContextSubmenu(HMENU menu);
bool IsReadAloudContextSubmenu(HMENU menu);
HMENU GetReadAloudContextSubmenu();
bool CanCloseWindow(MainWindow* win);
void CloseWindow(MainWindow* win, bool quitIfLast, bool forceClose);
void PostAppExit();
void SetSidebarVisibility(MainWindow* win, bool tocVisible, bool showFavorites, bool relayout = true);
void RememberFavTreeExpansionState(MainWindow* win);
void AdvanceFocus(MainWindow* win);
void SetCurrentLanguageAndRefreshUI(Str langCode);
void UpdateDocumentColors();
void UpdateFixedPageScrollbarsVisibility();

// scrollbar mode values: "windows\0smart\0overlay\0hidden\0"
constexpr int kScrollbarWindows = 0;
constexpr int kScrollbarSmart = 1;
constexpr int kScrollbarOverlay = 2;
constexpr int kScrollbarHidden = 3;
extern SeqStrings gScrollbarModeNames;
int ScrollbarModeFromPrefs();

bool ScrollbarsAreHidden();
bool ScrollbarsUseOverlay();
OverlayScrollbar::Mode ScrollbarsOverlayMode();

// toolbar mode values: "show\0hide\0overlay\0"
constexpr int kToolbarShow = 0;
constexpr int kToolbarHide = 1;
constexpr int kToolbarOverlay = 2;
extern SeqStrings gToolbarModeNames;
int ToolbarModeFromPrefs();
bool ToolbarModeIsOverlay();
bool ToolbarModeIsHidden();
void SetToolbarMode(int mode);

// toolbar position values: "top\0bottom\0"
constexpr int kToolbarTop = 0;
constexpr int kToolbarBottom = 1;
extern SeqStrings gToolbarPositionNames;
int ToolbarPositionFromPrefs();
bool ToolbarAtBottom();
void UpdateTabFileDisplayStateForTab(WindowTab* tab);
void ReloadDocument(MainWindow* win, bool autoRefresh);
void ToggleFullScreen(MainWindow* win, bool presentation = false);

// flags for ScheduleUiUpdate
// relayout unless nothing layout-affecting changed (RelayoutFrame compares
// against MainWindow::uiState.layout)
constexpr u32 kUiRelayout = 0x1;
// relayout even when the tracked layout state looks unchanged (something
// outside of it changed: fonts, tab width, menu bar height, ...)
constexpr u32 kUiForceRelayout = 0x2;
constexpr u32 kUiToolbarDirty = 0x4; // repaint the toolbar
constexpr u32 kUiTabsDirty = 0x8;    // repaint the tab bar
// this request doesn't need the toolbars re-fit (sidebar/splitter changes);
// ignored if another pending request wants them updated
constexpr u32 kUiNoToolbars = 0x10;
constexpr u32 kUiSidebarDirty = 0x20; // repaint toc/favorites boxes and their splitters

// Request an async, coalesced UI update: records what needs to happen and
// posts WM_UPDATE_UI once; any further requests before it's handled are
// folded into the same pass. Prefer this over direct relayout/RedrawWindow
// calls to avoid excessive repaints. sidebarDx >= 0 relayouts with a new
// sidebar width (splitter dragging).
void ScheduleUiUpdate(MainWindow* win, u32 flags = kUiRelayout, int sidebarDx = -1);
void DuplicateTabInNewWindow(WindowTab* tab);
void CopyFilePath(WindowTab*);

// note: background tabs are only searched if focusTab is true
// when limitWin is set, only that window's tabs are considered
MainWindow* FindMainWindowByFile(Str file, bool focusTab, MainWindow* limitWin = nullptr);
MainWindow* FindMainWindowBySyncFile(Str file, bool focusTab);
WindowTab* FindTabByFile(Str file, MainWindow* limitWin = nullptr);
void SelectTabInWindow(WindowTab*);

class EngineBase;
struct DocController;
struct FileArgs;
enum class NotifCorner : int; // full definition in Notifications.h

// LoadDocument carries a lot of state, this holds them in one place
struct LoadArgs {
    explicit LoadArgs(Str origPath, MainWindow* win);
    ~LoadArgs();

    Str FilePath() const;
    void SetFilePath(Str path);
    Str DisplayName() const;
    void SetDisplayName(Str name);
    LoadArgs* Clone();

    // we don't own those values
    EngineBase* engine = nullptr;
    MainWindow* win = nullptr;

    bool showWin = true;
    bool forceReuse = false;
    // over-writes placeWindow and other flags and forces no changing
    // of window location after loading
    bool noPlaceWindow = false;

    // for internal use
    bool isNewWindow = false;
    bool placeWindow = true;
    // TODO: this is hacky. I save prefs too frequently. Need to go over
    // and rationalize all SaveSettings() calls
    bool noSavePrefs = false;

    bool lazyLoad = false;
    bool async = false;
    bool activateExisting = false;
    // with activateExisting: only switch to an existing tab in args->win (UI
    // open paths). DDE and other global lookups leave this false.
    bool activateExistingInWindow = false;

    DocController* ctrl = nullptr;

    FileArgs* fileArgs = nullptr;

    TabState* tabState = nullptr;

    // if set, called on the UI thread when the load finishes,
    // with true if the document was loaded successfully
    Func1<bool> onFinished;
    // corner for the "Loading ..." notification; zero-init is TopLeft
    NotifCorner loadingNotifCorner{};

  private:
    Str fileName;
    Str displayName;
};

struct PasswordUI;

MainWindow* LoadDocument(LoadArgs* args);
MainWindow* LoadDocumentFinish(LoadArgs* args);
void StartLoadDocument(LoadArgs* args);
MainWindow* CreateAndShowMainWindow(SessionData* data = nullptr, bool showWin = true);
void ShowMainWindow(MainWindow* win, int windowState);
DocController* CreateControllerForEngineOrFile(EngineBase* engine, Str path, PasswordUI* pwdUI, MainWindow* win);

uint MbRtlReadingMaybe();
void MessageBoxWarning(HWND hwnd, Str msg, Str title = nullptr);
void UpdateCursorPositionHelper(MainWindow* win, Point pos, NotificationWnd* wnd);
void EnterFullScreen(MainWindow* win, bool presentation = false);
void ExitFullScreen(MainWindow* win);
void SetCurrentLang(Str langCode);
void RebuildMenuBarForWindow(MainWindow* win);
void DeleteMainWindow(MainWindow* win);

// snapshot of settings whose changes need explicit handling beyond a settings
// reload (used by the advanced settings dialog to apply changes on Save)
struct SettingsApplyState {
    bool useTabs = false;
    bool showMenubar = false;
    bool showMenubarWithTabs = false;
    bool disableAntiAlias = false;
    bool chmUseFixedPageUI = false;
    bool markdownUseFixedPageUI = false;
};
SettingsApplyState GetSettingsApplyState();
void ApplyChangedSettingsAndRelayout(const SettingsApplyState& before);

void SwitchToDisplayMode(MainWindow* win, DisplayMode displayMode, bool keepContinuous = false);
void MainWindowRerender(MainWindow* win, bool includeNonClientArea = false);
LRESULT CALLBACK WndProcSumatraFrame(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void ShutdownCleanup();

TempStr PageInfoOverlayResultTemp(Str pathTwoPages, Str pathOnePage, int* exitCodeOut = nullptr);
bool DocIsSupportedFileType(FileType);
TempStr GetLogFilePathTemp();
void ShowErrorLoadingNotification(MainWindow* win, Str path, bool noSavePrefs, bool showWin = true);
void SumatraOpenPathInDefaultFileManager(Str path);
void SmartZoom(MainWindow* win, float factor, Point* pt, bool smartZoom);
TempStr GetNotImportantDataDirTemp();
TempStr GetCrashInfoDirTemp();
TempStr GetBuildDirNameTemp();
