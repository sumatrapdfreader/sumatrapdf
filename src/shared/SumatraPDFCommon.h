/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SumatraPDFCommon.cpp and each app's SumatraPDF.cpp ---

#define kDefaultFilePerceivedTypes "audio,video,webpage"
#define kDefaultLinkProtocols "http,https,mailto,file"

// Light separator between page-info values: space + U+00B7 MIDDLE DOT + space.
#define kPageInfoSep " \xC2\xB7 "

constexpr const char* kManualDefaultDocURI = "/SumatraPDF-documentation";
constexpr const char* kManualVirtualHost = "https://sumatrapdf.manual/";

FileEBookUI* GetFileEBookUI(Str filePath);
extern StrVec gAllowedLinkProtocols;
extern StrVec gAllowedFileTypes;
void SetLoadThreadFileEBookUI(FileEBookUI* v);
void RestrictPolicies(Perm revokePermission);
extern bool gForceRtl;
bool ShouldSaveThumbnail(FileState* ds);
TempStr BuildZoomString(float zoomLevel);
void UpdatePageInfoHelper(DocController* ctrl, NotificationWnd* wnd, int pageNo);
bool ShouldUseBrowserView(FileType kind);
struct CustomCommand;
void ToggleCaption(CustomCommand* cmd);
bool showTocByDefault(Str path, EngineBase* engine);
bool IsEbookFileType(FileType ft);
DisplayMode DisplayModeForNewDocument(Str path, EngineBase* engine);
float ZoomForNewDocument(Str path, EngineBase* engine, float fallback);
void AutoReloadResetFileState(WindowTab* tab);
enum class MeasurementUnit {
    pt,
    mm,
    in
};
TempStr FormatCursorPositionTemp(EngineBase* engine, PointF pt, MeasurementUnit unit);
// Identity of the selected annotation for restore after save/reload (Annotation*
// pointers die with the engine).
struct SavedAnnotSel {
    bool valid = false;
    int pageNo = -1;
    AnnotationType type = AnnotationType::Unknown;
    RectF bounds;
};
SavedAnnotSel CaptureSelectedAnnotation(WindowTab* tab);
Annotation* FindMatchingAnnotation(WindowTab* tab, const SavedAnnotSel& key);
EngineBase* GetLiveTabEngine(WindowTab* tab, MainWindow** winOut);
bool TabStillInWindow(MainWindow* win, WindowTab* tab);
bool AppendFileFilterForDoc(DocController* ctrl, str::Builder& fileFilter);
bool FilePickerIsSumatraPDF();
bool IsOpenableNextPrevFile(Str path);
void CollectHistoryFilesInDir(Str dir, StrVec& out);
void InsertSortedNatural(StrVec* v, Str s);
struct PendingNextPrevNav {
    MainWindow* win = nullptr;
    bool forward = true;
    Str pathToDelete; // owned
    ~PendingNextPrevNav() { str::Free(pathToDelete); }
};
extern PendingNextPrevNav* gPendingNextPrevNav;
void ClearPendingNextPrevNav();
bool IsAtDocumentBottom(MainWindow* win);
void OnNextFileHintClosed(NotificationClosedEvent* ev);
void ToggleMangaMode(MainWindow* win);
void ToggleAutoRotateWidePages();
void OnMenuZoom(MainWindow* win, int menuId);
void TogglePresentationMode(MainWindow* win);
int wrapIdx(int idx, int max);
TempStr GetISO639LangCodeFromLangTemp(Str lang);
Str EbookLayoutSnapshot();
void ReloadEbookLayoutDocs();
TempStr ZoomArgTemp(DocController* ctrl);
Str HelpThemePref();
void ManualOnJsNotify(void*, Str method, Str paramsJson);
TempStr DocURIToLocalManualUrlTemp(Str docURI);
void TocItemToText(str::Builder& s, TocItem* item, int level);
bool ShouldToggle(CustomCommand* cmd, bool curState);
Str CurrentImageTabPathTemp(MainWindow* win);
void PrintCurrentFileDeferred(MainWindow* win);
void PrintSelectionDeferred(MainWindow* win);
void ReplaceColor(ParsedColor& col, Str maybeColor);

bool WindowHasDocumentLoading(MainWindow* win);
TempStr FindCoverImageTemp(Str docPath);
void TogglePageInfoHelper(MainWindow* win);
void RenameFileInHistory(Str oldPath, Str newPath);
extern MeasurementUnit cursorPosUnit;
extern Kind kNotifNextFileHint;
void ToggleContinuousView(MainWindow* win);
void ShowZoomNotification(MainWindow* win, float zoomLevel);
void ShowViewModeNotification(MainWindow* win, int cmdId);
void ZoomToSelection(MainWindow* win);
bool IsManualDocHtmlPage(Str path);
Str ManualInjectThemeCss(Str html);
TempStr DocURIToWebUrlTemp(Str docURI);
bool SetPointToVisiblePage(DisplayModel* dm, Point& pt, int& pageNo);

// implemented by each app
void ShowPageInfoIfWanted(MainWindow* win);
void ToggleCursorPositionInDoc(MainWindow* win);

extern Str gNextPrevDir;
extern StrVec gNextPrevDirCache;
extern bool gNextPrevDirReady;
extern bool gNextPrevDirScanning;
void ScheduleReloadTab(WindowTab* tab);
bool AutoReloadFileStillChanging(WindowTab* tab);
void EnsureNextPrevDirScan(Str filePath);
StrVec* GetNextPrevFilesReady(Str path);

// implemented by each app
void ReloadTab(WindowTab* tab);
void StartNextPrevDirScan(Str dir);
void RemoveFailedFiles(StrVec& files);

TempStr PeekNextFileInFolderTemp(MainWindow* win, int* outN = nullptr, int* outM = nullptr);

struct CreateThumbnailFromFileData {
    Str filePath;
    // when set, render from this clone of the open document instead of loading
    // the file again (owned)
    EngineBase* engine = nullptr;
    Pixmap* bmp = nullptr;
    // see LoadDocumentAsyncData: the thumbnail is rendered off the UI thread,
    // so the per-document ebook settings have to come along as a copy (#4600)
    FileEBookUI* fileEBookUI = nullptr;
    ~CreateThumbnailFromFileData() {
        str::Free(filePath);
        SafeEngineRelease(&engine);
        FreePixmap(bmp);
        DeleteFileEBookUI(fileEBookUI);
    }
};

TempStr PeekNextFileInFolderTemp(MainWindow* win, int* outN, int* outM);

bool TextSelectExtendFromVk(int key, TextSelectUnit& unit, int& delta);

void NotifyUrlSelectionTruncated(WindowTab* tab);
void ReopenLastClosedFile(MainWindow* win);

bool SendOpenFileToExistingInstance(HWND targetHwnd, Str fullPath, u32 newWindow);
bool SendOpenFilesToExistingInstance(HWND targetHwnd, StrVec& paths, u32 newWindow);

// implemented by each app
void UndoRedoUnavailable(MainWindow* win, bool redo);
void ApplyWindowPosFlag(const Flags& i);
void ToolbarModeApplied(MainWindow* win);
void CreateThumbnailFromFileAsync(FileState* ds, EngineBase* engine);

void CreateThumbnailForFile(MainWindow* win, FileState* ds);
void DeleteCurrentFileAndOpenNext(MainWindow* win);
void OnMenuViewShowHideToolbar(MainWindow* win);
void DeleteCachedFiles(MainWindow* win);
void UndoRedoInTab(WindowTab* tab, bool redo);
void UpdateSettings(const Flags& i);
void CancelDrag(MainWindow* win);

Annotation* MakeAnnotationsFromSelection(WindowTab* tab, AnnotCreateArgs* args);
void RemoveDeletedFilesFromHistory(MainWindow* win);
void ApplyRedactionsInTab(WindowTab* tab);

void SendMyselfDDE(Str cmdA, HWND targetHwnd);
bool IsSimpleOpenCase(const Flags& i, bool isFirstWin);
void OpenUsingDDE(HWND targetHwnd, Str path, Flags& i, bool isFirstWin);
bool SetupPluginMode(Flags& i);

void OnFrameKeyB(MainWindow* win, bool isShift);
EngineBase* CreateEngineForThumbnail(Str path);
void CreateThumbnailFromFileFinish(CreateThumbnailFromFileData* d);

constexpr const char* kSumatraWindowTitle = "SumatraPDF";

// Cap how many extension links we put in the bar
constexpr int kMaxDefaultAppLinks = 8;

static Kind kNotifPersistentWarning = "persistentWarning";
static Kind kNotifDocErrors = "docErrors";
// Kind used so only one default-app bar is shown at a time
static Kind kNotifDefaultApp = "defaultApp";
static Kind kNotifThumbnailProgress = "thumbnailProgress";
int SidebarNativeDx(MainWindow* win);

int SidebarExtraDx(MainWindow* win);
