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
void UpdatePageInfoHelper(MainWindow* win, NotificationWnd* wnd, int pageNo);

// PageInfoLocation
enum class PageInfoBarMode {
    Notification,
    Bottom,
    BottomOverlay,
};
PageInfoBarMode GetPageInfoBarMode();
bool PageInfoBarShown(MainWindow* win);
void PageInfoBarTextTemp(MainWindow* win, TempStr* leftOut, TempStr* rightOut);
// implemented by each app: the bar was shown, hidden or its text changed
void PageInfoBarChanged(MainWindow* win);
// implemented by each app: "canvas=x,y,dx,dy bar=x,y,dx,dy" (or bar=none), for tests
TempStr PageInfoBarGeometryTemp(MainWindow* win);
// ReadingProgressBarDy
Color ProgressBarDoneColor();
Color ProgressBarRestColor();
bool ProgressBarRects(MainWindow* win, Size canvas, int dpi, int bottomInset, Rect* doneOut, Rect* restOut);
// implemented by each app: the bar in canvas coordinates; false when there is none
bool ProgressBarCanvasRects(MainWindow* win, Rect* doneOut, Rect* restOut);
bool ShouldUseBrowserView(FileType kind);
struct CustomCommand;
bool RunDocColorsFollowThemeCmd(CustomCommand* cmd);
void ToggleCaption(CustomCommand* cmd);
void ToggleSidebarAutoHide(CustomCommand* cmd);
TextWindowOption DocErrorsWindowOption();
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
struct PendingNextPrevNav {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    bool forward = true;
    Str path;         // owned
    Str pathToDelete; // owned
    ~PendingNextPrevNav() {
        str::Free(path);
        str::Free(pathToDelete);
    }
};
extern PendingNextPrevNav* gPendingNextPrevNav;
void ClearPendingNextPrevNav();
void DeferNextPrevNav(MainWindow* win, WindowTab* tab, bool forward, Str pathToDelete);
void ReplayPendingNextPrevNav();
bool IsAtDocumentBottom(MainWindow* win);
bool GoOnToNextFile(MainWindow* win, int cmdId);
void OnNextFileHintClosed(NotificationClosedEvent* ev);
void ToggleMangaMode(MainWindow* win);
void ToggleFacingStepOne(MainWindow* win);
void ToggleMirror(MainWindow* win);
void CountWordsInDoc(MainWindow* win);
TempStr WordCountNotifMsgTemp(MainWindow* win);
void ShowSelectedText(MainWindow* win);
TempStr SelectedTextNotifMsgTemp(MainWindow* win);
void ToggleAutoRotateWidePages();
void OnMenuZoom(MainWindow* win, int menuId);
void TogglePresentationMode(MainWindow* win);
int wrapIdx(int idx, int max);
TempStr GetISO639LangCodeFromLangTemp(Str lang);
Str EbookLayoutSnapshot();
void ReloadEbookLayoutDocs();
void ReloadWebDocLayouts(Str beforeMarkdown, Str beforeHtml);
TempStr ZoomArgTemp(DocController* ctrl);
void ManualOnJsNotify(void*, Str method, Str paramsJson);
TempStr DocURIToLocalManualUrlTemp(Str docURI);
void TocItemToText(str::Builder& s, TocItem* item, int level);
bool ToggleCmdIsNoOp(MainWindow* win, CustomCommand* cmd, int cmdId);
Str CurrentImageTabPathTemp(MainWindow* win);
void PrintCurrentFileDeferred(MainWindow* win);
void PrintSelectionDeferred(MainWindow* win);
void ReplaceColor(ParsedColor& col, Str maybeColor);

bool WindowHasDocumentLoading(MainWindow* win);
void TogglePageInfoHelper(MainWindow* win);
void RenameFileInHistory(Str oldPath, Str newPath);
extern MeasurementUnit cursorPosUnit;
extern Kind kNotifNextFileHint;
void ToggleContinuousView(MainWindow* win);
void ShowZoomNotification(MainWindow* win, float zoomLevel);
void ShowViewModeNotification(MainWindow* win, int cmdId);
void ZoomToSelection(MainWindow* win);
void TrimPagesToSelection(MainWindow* win);
void ShowWholePages(MainWindow* win);
bool CanSaveTrimmedPages(MainWindow* win);
TempStr TrimmedPagesPathTemp(Str srcPath);
bool SaveTrimmedPages(MainWindow* win, Str dstPath);
void ToggleReadColumns(MainWindow* win);
bool IsManualDocHtmlPage(Str path);
Str ManualInjectThemeCss(Str html);
TempStr DocURIToWebUrlTemp(Str docURI);
bool SetPointToVisiblePage(DisplayModel* dm, Point& pt, int& pageNo);

// implemented by each app
void ShowPageInfoIfWanted(MainWindow* win);
bool AppToggleCmdState(MainWindow* win, int cmdId, bool& on);
bool IsDocErrorsNotifShown(MainWindow* win);
void ToggleCursorPositionInDoc(MainWindow* win);

extern Str gNextPrevDir;
extern StrVec gNextPrevDirCache;
void StealStrVec(StrVec& dst, StrVec& src);
extern bool gNextPrevDirReady;
extern bool gNextPrevDirScanning;
void ScheduleReloadTab(WindowTab* tab);
void EnsureNextPrevDirScan(Str filePath);
StrVec* GetNextPrevFilesReady(Str path);

// implemented by each app
void ReloadTab(WindowTab* tab);
void StartNextPrevDirScan(Str dir);
void RemoveFailedFiles(StrVec& files);

TempStr PeekNextFileInFolderTemp(MainWindow* win, int* outN = nullptr, int* outM = nullptr);

struct CreateThumbnailFromFileData {
    Str filePath;
    // when set, the open document's engine, ref'd for us: render from it
    // instead of loading the file again
    EngineBase* engine = nullptr;
    Pixmap* bmp = nullptr;
    // see LoadDocumentAsyncData: the thumbnail is rendered off the UI thread,
    // so the per-document ebook settings have to come along as a copy (#4600)
    FileEBookUI* fileEBookUI = nullptr;
    // the regenerate batch it came from, 0 for a single thumbnail
    int batchGen = 0;
    ~CreateThumbnailFromFileData() {
        str::Free(filePath);
        SafeEngineRelease(&engine);
        FreePixmap(bmp);
        DeleteFileEBookUI(fileEBookUI);
    }
};

TempStr PeekNextFileInFolderTemp(MainWindow* win, int* outN, int* outM);
bool AreThumbnailThreadsActive();

bool TextSelectExtendFromVk(int key, TextSelectUnit& unit, int& delta);

void NotifyUrlSelectionTruncated(WindowTab* tab);
void ReopenLastClosedFile(MainWindow* win);
void RememberClosedWindow(MainWindow* win);

bool SendOpenFilesToExistingInstance(HWND targetHwnd, StrVec& paths, u32 newWindow);

// implemented by each app
void UndoRedoUnavailable(MainWindow* win, bool redo);
void ApplyWindowPosFlag(const Flags& i);
void ToolbarModeApplied(MainWindow* win);

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
bool ShouldUsePageAspect(Str path);
void OnTemporaryFileNotifClosed(NotificationClosedEvent* ev);

constexpr const char* kSumatraWindowTitle = "SumatraPDF";

// Cap how many extension links we put in the bar
constexpr int kMaxDefaultAppLinks = 8;

extern Kind kNotifPersistentWarning;
extern Kind kNotifDocErrors;
// Kind used so only one default-app bar is shown at a time
extern Kind kNotifDefaultApp;
extern Kind kNotifThumbnailProgress;
int SidebarNativeDx(MainWindow* win);
void ToggleLineWeights();

int SidebarExtraDx(MainWindow* win);
