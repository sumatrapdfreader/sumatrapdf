/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SumatraPDF.h declares the whole application, most of it in terms
// of MainWindow / WindowTab / HWND. The declarations below are copied
// verbatim from orig, in orig's order; what the gpui shell replaced is gone.

constexpr const char* kWebsiteURL = "https://www.sumatrapdfreader.org/";
constexpr const char* kContributeTranslationsURL = "https://www.sumatrapdfreader.org/docs/Contribute-translation";

// in plugin mode the "file path" is the URL the document came from. ng: the
// plugin host is step 17, so this is always empty for now
extern Str gPluginURL;
#define gPluginMode (len(gPluginURL) > 0)

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

constexpr Perm operator|(Perm lhs, Perm rhs) {
    using T = std::underlying_type_t<Perm>;
    return static_cast<Perm>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

constexpr Perm operator&(Perm lhs, Perm rhs) {
    using T = std::underlying_type_t<Perm>;
    return static_cast<Perm>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

constexpr Perm operator<<(Perm lhs, uint rhs) {
    using T = std::underlying_type_t<Perm>;
    return static_cast<Perm>(static_cast<T>(lhs) << static_cast<T>(rhs));
}

constexpr Perm operator~(Perm lhs) {
    using T = std::underlying_type_t<Perm>;
    T v = static_cast<T>(lhs);
    v = ~v;
    return static_cast<Perm>(v);
}

struct RenderCache;
extern RenderCache* gRenderCache;

struct MainWindow;
struct WindowTab;
struct DocController;
struct DocControllerCallback;

DocControllerCallback* CreateControllerCallbackHandler(MainWindow* win);
void DeleteControllerAsync(DocController* ctrl);
void UpdateTabFileDisplayStateForTab(WindowTab* tab);
void RememberDefaultWindowPosition(MainWindow* win);

// orig's sidebar layout limits (SumatraPDF.cpp)
constexpr int kSidebarMinDx = 150;
constexpr int kTocMinDy = 100;
constexpr int kSplitterDx = 5;
// orig's kSplitterDy: the splitter between the two sidebar panels
constexpr int kSplitterDy = 4;
// leave at least this much canvas for the document when the sidebar is open
constexpr int kMinDocCanvasDx = 200;

// ng: orig also takes a SidebarResizeFrame telling it whether to grow the
// window by the sidebar's width; gpui cannot move a window (see "gpui gaps")
void SetSidebarVisibility(MainWindow* win, bool tocVisible, bool showFavorites);

void UpdateWindowTitle(MainWindow* win);
void RebuildMenuBar(MainWindow* win);
void ExecuteCmd(MainWindow* win, int cmdId);
// set by the last ExecuteCmd() that reached its default branch
extern bool gLastCmdFellThrough;
// ms since the process started (the -dbg-control performance snapshot)
double AppElapsedMs();
// a command the context menu sends with the canvas point it was opened on
// (orig puts that point in the WM_COMMAND's LPARAM)
void ExecuteCmdAtPoint(MainWindow* win, int cmdId, Point pt);
struct RenderedBitmap;
void CopyTextToClipboard(MainWindow* win, Str s);
// takes ownership of bmp
void CopyRenderedBitmapToClipboard(MainWindow* win, RenderedBitmap* bmp);
// orig's SaveDataToFile: asks where, then writes (the prompt does not block)
void SaveDataToFile(MainWindow* win, Str fileName, Str data);
void DeleteFileFromDiskAndHistory(Str path);
void DeleteFileFromHomePage(MainWindow* win, Str path);
bool OpenDocumentFromMemory(MainWindow* win, Str data, Str nameHint);
// orig runs an annotation command a second time to commit a placement mode;
// the placement code passes the extra state as arguments here instead of in a
// WM_COMMAND's WPARAM
void ExecuteAnnotCreateCmd(MainWindow* win, int cmdId, bool isPlacementCommit, Point pt);
// the whole page has to be rendered again (annotations changed)
void MainWindowRerender(MainWindow* win);
void RerenderTabPage(WindowTab*, int pageNo);

struct AnnotCreateArgs;
struct CustomCommand;
void SetAnnotCreateArgs(AnnotCreateArgs&, CustomCommand*);
struct Annotation;
void DeleteSelectedAnnotation(MainWindow* win);
void SaveEmbeddedFileAs(MainWindow* win, Annotation* annot, Str fileName);
// everything changed while an operation is open is one undo step
void BeginPdfEditOperation(MainWindow* win, const char* name);
void EndPdfEditOperation(MainWindow* win);
bool SaveAnnotationsToExistingFile(WindowTab* tab);
void SaveAnnotationsToMaybeNewPdfFile(WindowTab* tab, const Func1<bool>& onDone = {});
// ng: a gpui dialog cannot block, so orig's `bool MaybeSaveAnnotations(tab)`
// is asynchronous: `onDone(true)` means the caller may proceed
void MaybeSaveAnnotations(WindowTab* tab, const Func1<bool>& onDone);
// true when there is nothing to ask about, so the caller can go on right away
bool AnnotationsNeedSavePrompt(WindowTab* tab);

// ng: orig passes a LoadArgs; the only flag the port needs so far is orig's
// args.noSavePrefs, which a session restore sets so the load does not write
// the settings file it is reading
enum class LoadPrefs {
    Save,
    DontSave,
};
// orig's args.forceReuse: replace the document in the current tab instead of
// adding one (a reload, or next / prev file in folder)
enum class LoadReuse {
    NewTab,
    CurrentTab,
};
MainWindow* LoadDocument(MainWindow* win, Str path, LoadPrefs prefs = LoadPrefs::Save,
                         LoadReuse reuse = LoadReuse::NewTab);
void OpenDroppedFiles(MainWindow* win, const Str* paths, int n);
void SelectTabInWindow(WindowTab* tab);
MainWindow* FindMainWindowByFile(Str file, bool focusTab, MainWindow* limitWin = nullptr);
MainWindow* FindMainWindowBySyncFile(Str path, bool focusTab);
struct SessionData;
MainWindow* CreateAndShowMainWindow(SessionData* data = nullptr);
enum class PlaceWindowWhen {
    Now,
    Later,
};
void PlaceMainWindow(MainWindow* win, SessionData* data, PlaceWindowWhen when);
void PlaceMainWindowLater(MainWindow* win, Rect pos, bool maximize);
void DuplicateTabInNewWindow(WindowTab* tab);
void ReloadDocument(MainWindow* win, bool autoRefresh);
bool ForwardBrowserMsg(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp);
// the shell's tick: the delayed reload the file watcher asked for
void AutoReloadTick(MainWindow* win, int elapsedMs);
// next / prev openable file in the current document's folder (no wrap)
void OpenNextPrevFileInFolder(MainWindow* win, bool forward, Str pathToDelete = {});
// the page-info tip (CmdTogglePageInfo): put it up or refresh it when the
// user asked for it and a document is loaded
void ShowPageInfoIfWanted(MainWindow* win);
#if OS_WIN
void ShowDefaultAppNotification(MainWindow* win, const StrVec& missing);
// orig's window picker. hwndRestore gets focus back when the picker closes.
void ShowScreenshotPicker(HWND hwndRestore);
#endif
bool DismissNotificationsOnEsc(MainWindow* win);
// the cursor-position tip (CmdToggleCursorPosition), refreshed on mouse move
struct NotificationWnd;
void UpdateCursorPositionHelper(MainWindow* win, Point pos, NotificationWnd* wnd);
// what the next CmdToggleCursorPosition switches to, for the command palette
Str NextCursorPositionUnitName(MainWindow* win);
void OnDocumentVerticalScrollIntent(MainWindow* win, bool down);
void DismissNextFileScrollHint(MainWindow* win);
// the folder listing the picker and next/prev share; null while it is built
StrVec* GetNextPrevFilesReady(Str path);
void CloseCurrentTab(MainWindow* win, bool quitIfLast);
void CloseTab(WindowTab* tab, bool quitIfLast);
void RequestCloseWindow(MainWindow* win, bool quitIfLast);
void CloseWindow(MainWindow* win, bool quitIfLast, bool forceClose);
void CloseWindowIfCan(MainWindow* win, bool quitIfLast);
void OnMenuExit();
void CopyFilePath(WindowTab* tab);
void ShowFileInFolder(MainWindow* win, Str path);
enum class DisplayMode;
void SwitchToDisplayMode(MainWindow* win, DisplayMode displayMode, bool keepContinuous = false);

// if pt is given (a point on the canvas) that point stays put across the zoom
void SmartZoom(MainWindow* win, float factor, Point* pt, bool smartZoom);

enum class FileType : u8;
// true for the ebook formats EbookDoc handles (epub, fb2, mobi, palmdoc)

bool SettingsUseTabs();
bool SettingsRememberOpenedFiles();

void CrashHandlerSetSettings(Str settings);

void InitializePolicies(bool restrict);
bool HasPermission(Perm permission);
bool CanAccessDisk();
bool OpenFileExternally(Str path);
bool SumatraLaunchBrowser(Str url);
bool AnnotationsAreDisabled();
bool IsUIRtl();

void SetCurrentLanguageAndRefreshUI(Str langCode);
void SetCurrentLang(Str langCode);
void UpdateDocumentColors();
void MaybeRedrawHomePage();
// removes a file from the Frequently Read list on the home page (orig has it
// in Menu.cpp, with the home page's context menu)
void ForgetFileFromFrequentlyRead(MainWindow* win, Str filePath);
void UpdateFixedPageScrollbarsVisibility();

// the settings that need explicit handling beyond a reload; snapshot before a
// change, hand back to ApplyChangedSettingsAndRelayout after it (orig's)
struct SettingsApplyState {
    bool useTabs = false;
    bool showMenubar = false;
    bool showMenubarWithTabs = false;
    bool disableAntiAlias = false;
    bool chmUseFixedPageUI = false;
    bool markdownUseFixedPageUI = false;
    // owned copy of the ebook layout inputs; ApplyChangedSettingsAndRelayout frees it
    Str ebookLayout;
};
SettingsApplyState GetSettingsApplyState();
void ApplyChangedSettingsAndRelayout(const SettingsApplyState& before);
// opens a page of the online documentation ("LaTeX-integration", ...)
constexpr const char* kManualURL = "https://www.sumatrapdfreader.org/manual";
void LaunchDocumentation(Str page);

TempStr GetSumatraDataDirTemp();

// scrollbar mode values: "windows\0smart\0overlay\0hidden\0"
constexpr int kScrollbarWindows = 0;
constexpr int kScrollbarSmart = 1;
constexpr int kScrollbarOverlay = 2;
constexpr int kScrollbarHidden = 3;
extern SeqStrings gScrollbarModeNames;
int ScrollbarModeFromPrefs();

bool ScrollbarsAreHidden();
bool ScrollbarsUseOverlay();

// toolbar mode values: "show\0hide\0overlay\0" (Toolbar and Fullscreen.Toolbar)
constexpr int kToolbarShow = 0;
constexpr int kToolbarHide = 1;
constexpr int kToolbarOverlay = 2;
extern SeqStrings gToolbarModeNames;

int ToolbarModeFromPrefs();
bool ToolbarModeIsOverlay();
bool ToolbarModeIsHidden();
void SetToolbarMode(int mode);
int FullscreenToolbarModeFromPrefs();
void SetFullscreenToolbarMode(int mode);

// fullscreen and presentation mode (step 19a)
struct Flags;
void EnterFullScreen(MainWindow* win, bool presentation = false);
void ExitFullScreen(MainWindow* win);
void ToggleFullScreen(MainWindow* win, bool presentation = false);
void AdvanceFocus(MainWindow* win, bool isShift);
bool FrameOnKeydown(MainWindow* win, int key, bool isCtrl, bool isShift, bool isAlt);
bool FrameOnChar(MainWindow* win, u32 key, bool isShift);
bool FrameOnSysChar(MainWindow* win, int key);
void EnterFullScreenFromFlags(const Flags& flags, MainWindow* win);
void SwitchToFullScreen(MainWindow* win, bool presentation);

// toolbar position values: "top\0bottom\0"
constexpr int kToolbarTop = 0;
constexpr int kToolbarBottom = 1;
extern SeqStrings gToolbarPositionNames;
int ToolbarPositionFromPrefs();
bool ToolbarAtBottom();
