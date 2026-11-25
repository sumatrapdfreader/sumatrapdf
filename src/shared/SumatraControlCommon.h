/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SumatraControlCommon.cpp and each app's SumatraControl.cpp ---

TempStr PageBoxesResultTemp(int pageNo, int* exitCodeOut);
void AppendLayoutRect(str::Builder& out, Str name, bool visible, Rect rect);
TempStr SelectionVarsResultTemp(Str pattern, int* exitCodeOut);
TempStr DocumentSignaturesResultTemp(int* exitCodeOut);
TempStr DocumentFontListResultTemp(int* exitCodeOut);
TempStr DocumentPropertiesResultTemp(int* exitCodeOut);
enum class ControlArgType : u16 {
    End = 0,
    Int32 = 1,
    Bytes = 2,
    String = 3,
    List = 4,
};
struct ControlArg {
    ControlArgType type = ControlArgType::End;
    i32 intVal = 0;
    u8* bytes = nullptr;
    u32 bytesLen = 0;
    Str str;
    Vec<ControlArg*>* list = nullptr;
};
void DeleteControlArg(ControlArg* arg);
void AppendU16(str::Builder& s, u16 v);
void AppendU32(str::Builder& s, u32 v);
void AppendArgEnd(str::Builder& s);
void AppendArgInt(str::Builder& s, i32 v);
void AppendArgString(str::Builder& s, Str str);

enum class RenderIdleState : u8 {
    NotReady = 0,
    Busy = 1,
    Idle = 2,
};
// a manual-reset event, from the portable primitives
struct DoneEvent {
    Mutex mutex;
    ConditionVariable cond;
    bool isSet = false;

    void Set() {
        mutex.Lock();
        isSet = true;
        mutex.Unlock();
        cond.WakeAll();
    }
    void Reset() {
        mutex.Lock();
        isSet = false;
        mutex.Unlock();
    }
    void Wait() {
        mutex.Lock();
        while (!isSet) {
            cond.Wait(&mutex);
        }
        mutex.Unlock();
    }
};

struct ControlRequest {
    u16 cmd = 0;
    u16 reqId = 0;
    Vec<ControlArg*> args;
    str::Builder results;
    DoneEvent done;
    RenderIdleState idleState = RenderIdleState::NotReady;
    char idleInfo[320]{};
};
void DeleteControlRequest(ControlRequest* req);
Str StringArg(ControlRequest* req, size_t idx);
bool IntArg(ControlRequest* req, size_t idx, i32& valOut);
void AppendError(ControlRequest* req, Str msg);
void AppendTestResult(ControlRequest* req, int exitCode, Str result);

struct PacketReader {
    const u8* data = nullptr;
    size_t size = 0;
    size_t pos = 0;

    bool ReadU16(u16& v) {
        if (pos + 2 > size) {
            return false;
        }
        v = (u16)(data[pos] | (data[pos + 1] << 8));
        pos += 2;
        return true;
    }

    bool ReadU32(u32& v) {
        if (pos + 4 > size) {
            return false;
        }
        v = (u32)data[pos] | ((u32)data[pos + 1] << 8) | ((u32)data[pos + 2] << 16) | ((u32)data[pos + 3] << 24);
        pos += 4;
        return true;
    }

    bool ReadBytes(u8* dst, size_t n) {
        if (pos + n > size) {
            return false;
        }
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    }
};
bool ParseArgList(PacketReader& r, Vec<ControlArg*>* args, bool explicitCount, u16 count = 0);
void ControlPollSleep(u64 startMs);

// implemented by each app
bool ParseArg(PacketReader& r, ControlArg** argOut);
void SnapshotRenderIdle(ControlRequest* req);
void SnapshotSessionRestore(ControlRequest* req);
void AddFavoriteSilent(MainWindow* win, int pageNo);
bool ControlFavoritesMenu(MainWindow* win, Str action, TempStr* idsOut);
void ControlTogglePresentation(MainWindow* win);
void ControlToggleFullScreen(MainWindow* win);

#if OS_WIN
using ControlConn = HANDLE;
bool ReadExact(HANDLE h, void* data, DWORD n);
bool WriteExact(HANDLE h, Str data);
struct ControlThreadArg {
    Str pipeName;
};
void SumatraControlThread(ControlThreadArg* arg);
// implemented by each app
bool ProcessControlConnection(HANDLE h);
#else
// a connected unix domain socket
using ControlConn = int;
bool ReadExact(int fd, void* data, u32 n);
bool WriteExact(int fd, Str data);
#endif

// Wire IDs used by the debug control clients. Keep existing numeric assignments.
enum class ControlCmd : u16 {
    Ping = 1,
    Quit = 2,
    TestSynctex = 10,
    TestSearch = 11,
    TestDest = 12,
    TestNamedDest = 13,
    TestChm = 14,
    TestSelectionTranslate = 15,
    TestTripleClickLineSelect = 16,
    TestContextMenuSelection = 17,
    TestGoToFindMatch = 18,
    TestInverseSearch = 22,
    TestImageResizeArrowKey = 23,
    TestFindResultPageColumnClip = 24,
    TestScrollToLink = 26,
    TestPageInfoOverlay = 28,
    TestGetToc = 29,
    TestPageLinks = 30,
    TestWindowStateDuringLoad = 31,
    TestTocNavigate = 32,
    TestMarkdownTocNavigate = 33,
    TestFavoriteNav = 34,
    TestToolbarButtons = 35,
    TestKeyboardLinkFollow = 36,
    TestFindResultsOrder = 37,
    TestClickClearsSelection = 38,
    TestRectSelectionDrag = 39,
    TestSelectTextKeyboard = 40,
    TestAIChat = 41,
    TestAIChatReplay = 42,
    TestMarkdownFollowLink = 43,
    TestHomeListRows = 44,
    TestPageComments = 45,
    TestAdvSettingsRows = 46,
    TestDestZoomNav = 47,
    TestAnnotEditorLayout = 48,
    TestDisplayMode = 49,
    TestSidebarLayout = 50,
    TestCadEnhanceColors = 51,
    TestFindPageRange = 52,
    TestDocumentFontList = 53,
    WaitRenderIdle = 54,
    SetNotificationsEnabled = 55,
    TestHomeSelection = 56,
    TestImageRenderEdges = 57,
    TestInsertImage = 58,
    TestRenderPageColors = 59,
    TestListSigningCerts = 60,
    TestSignDocument = 61,
    TestGetPolicies = 62,
    TestPageBoxes = 63,
    TestDocumentSignatures = 64,
    TestCommandPalette = 65,
    TestFindHistory = 66,
    TestImageResizeEdges = 67,
    TestLinkDestHighlight = 68,
    TestConvertToImages = 69,
    TestLayout = 70,
    TestDpi = 71,
    TestSelectionVars = 72,
    TestSelectionToolbar = 73,
    TestMarkupAnnots = 74,
    TestCmykImageSave = 75,
    TestFindWindowContents = 77,
    TestFindUiState = 78,
    TestRenderViewPrint = 79,
    TestReadAloudPlaybackBar = 80,
    TestRotatedTextMouseDrag = 81,
    TestChapterInfo = 82,
    TestGoToLocation = 83,
    TestTocSidebarNav = 84,
    TestSelectionSurvivesRenumber = 85,
    TestConvertToPdf = 86,
    TestInvokeCommand = 87,
    TestCurrentTab = 88,
    TestCommandVisibility = 89,
    TestExtractPages = 90,
    TestAnnotFilter = 91,
    TestCanvasFlags = 92,
    CrashMe = 93,
    TestDocumentProperties = 94,
    TestHiddenTabGoToPage = 95,
    TestSaveSelectionAsImage = 96,
    TestReadingAutoScroll = 97,
    TestReadingBar = 98,
    TestSeedTextSelection = 99,
    TestTtsEngineCrash = 100,
    StopPerfLog = 102,
    WaitSessionRestored = 103,
    TestNavFiles = 104,
    TestSaveFileAs = 105,
    TestImageOrientation = 106,
    TestTtsPumpOnSpeak = 107,
    TestRenderSelections = 108,
    TestToggleFormButton = 109,
    ResolveUnsavedChanges = 110,
    TestRefHover = 111,
    TestPageInfo = 112,
    TestSidebarThumbnails = 113,
    TestFrameNcStrips = 114,
    TestMergePdf = 115,
    TestWheelWhileClosing = 116,
    TestMainMenu = 117,
    // Send GPUI input and return the shell state.
    TestInput = 121,
    TestUiState = 122,
    // Inspect tool windows or send input to one of them.
    TestToolWindow = 125,
    // the home page's context menu of the selected entry (else the first), in
    // ng's TestMainMenu row format
    TestContextMenuAt = 127,
    // ng: DDE execute grammar ([GotoPageWord], [Open], ...). Orig receives
    // these as WM_COPYDATA; macOS has no such message.
    TestDdeExecute = 129,
    TestQuitProcess = 130,
    TestRenameFile = 131,
    // ng: open a file the way a drop or a Finder open does
    TestOpenFiles = 132,
    TestChapterLayoutHold = 133,
    // the path the next Save As takes, without its dialog
    TestSaveFileName = 134,
    // places the signature being placed at a canvas point (pixels), as a click does
    TestPlaceSignature = 135,
    // Run PDF Script's drawn buttons: examples, pick <idx>, add, remove, run
    TestPdfScript = 136,
    // visible virtual controls (kind, name, text, client rect), or the one under a point
    TestVirtTree = 137,
    // [brightness, contrast] applied to the current file as the Brightness /
    // Contrast dialog's OK does; returns the current values
    TestPageAdjust = 138,
    // urls the current markdown document's browser view was served
    TestMarkdownServed = 139,
    // the current document: from a temporary folder?, its notification, its path
    TestTemporaryFile = 140,
    // [x, y] frame client point the auto-hide sidebar takes as the cursor; x < 0 clears
    TestSidebarCursor = 141,
    // [destPath, pageSizeIdx, fontSize]: Convert EPUB to PDF without its dialog
    TestConvertEpubToPdf = 142,
    // [path, pageNo]: image element rects of a page
    TestPageImages = 143,
    // the first window's page and scroll position
    TestScrollState = 144,
    TestDrawRectSelection = 145,
    TestEutlUpdate = 146,
    TestFetchRevocation = 147,
    // [action]: the state of the signature bar, panel, marks and dialog
    TestSignatureStatus = 148,
    // the first window's tab widths, one "dx=<n> text=<title>" line per tab
    TestTabWidths = 149,
    // [url, dir]: Open from URL as the dialog's Download starts it
    TestOpenFromUrl = 150,
    // [srcWin, tabIdx, dstWin]: move a tab as a drag does (-1 = new window);
    // tabIdx < 0 only lists every window's tabs
    TestMigrateTab = 151,
    // [action]: "toggle <row>" or "show", then the Layers view's rows
    TestLayers = 152,
    // [word]: drag-select it from edge to edge, then the selected text
    TestSelectWordEdges = 153,
    // the word count notification's text
    TestWordCount = 154,
    // the page info tip's text
    TestPageInfoText = 155,
    // the page info bar: where it is, the canvas, its text
    TestPageInfoBar = 156,
    // the Show Selected Text notification's text
    TestSelectedText = 157,
    // the bookmarks tree's visible rows and their heights
    TestTocRows = 158,
    // [action, pageNo, x, y, ...]: drives the Measure tool
    TestMeasurement = 159,
    // [action, pageNo, imageIdx, path]: replace / edit an image embedded in the PDF
    TestImageReplace = 160,
    TestPerfStats = 161,
    TestOverlayState = 162,
    // [timeoutMs]: blocks until the home page painted after startup
    WaitHomePainted = 163,
    // [idx]: select tab idx; idx < 0 only reports the current tab and count
    TestSelectTab = 164,
    // [definition]: does "CmdToggleX on" parse, and the command's on/off state
    TestToggleCmd = 165,
    // page 1 pixels a Clone() of the current document renders differently
    TestCloneRender = 166,
    // does the document have load errors, does the Errors notification show
    TestDocErrors = 167,
    // [pageNo, idx, value]: pick value in the idx-th combo / list box
    TestSetFormChoice = 168,
    // [action, a, b, c]: "drop" from to groupPos, "move" dir, "select" idx, or
    // "" for the state; answers with the strip groups and tabs (StripGroupsStateTemp)
    TestStripGroups = 169,
    // ng only (macOS): [path, mode]: a PNG of the first window as drawn ("all": with its other windows)
    TestCaptureWindow = 170,
    // [ms]: every document open waits this long first, like a sleeping drive
    TestLoadDelay = 171,
    TestWindows = 172,
    // [path, value]: sets a setting the way the palette's "= settings" rows do
    TestSetSetting = 173,
    // [action, word]: touch selection probe (TouchSelResultTemp)
    TestTouchSel = 174,
    // ["status"] or ["hooks", stallMs, delayMs, pathMatch, relistMs]: the
    // content hashes of history files (ContentHashStatusTemp), or the test hooks
    // of the hashing and of re-listing SumatraPDF Web files
    TestContentHash = 175,
    // [paths, a line each]: the files the next multi-file Open takes, without its dialog
    TestOpenFileNames = 176,
    // the next "Unsaved changes" prompt fails as if TaskDialogIndirect had
    TestFailSavePrompt = 177,
    // [action]: "closepaint" closes the open tab groups dialog with a paint before its end
    TestTabGroups = 178,
    // the AI chat session combo of the first window (AIChatSessionsTestTemp)
    TestAIChatSessions = 179,
};

enum class ControlFrameAction : u8 {
    RunOnUi = 0,
    WaitRenderIdle = 1,
    WaitSessionRestored = 2,
};

struct ControlServeResult {
    bool hadRequest = false;
    bool wrote = false;
    bool isQuit = false;
};

ControlFrameAction ControlFrameActionFor(u16 cmd);
bool ControlCmdEndsListener(u16 cmd);
using ControlExecFn = void (*)(ControlRequest*);
ControlServeResult ServeControlRequest(ControlConn h, ControlExecFn execOnUi);

struct LayoutProbeState {
    MainWindow* win = nullptr;
    int count = 0;
    bool active = false;
};

extern LayoutProbeState gLayoutProbe;
void LayoutProbeNote(MainWindow* win);

TempStr FavoriteNavResultTemp(Str action, int pageNo, int* exitCodeOut);
TempStr ScrollStateResultTemp(int* exitCodeOut);
TempStr ToggleCmdResultTemp(Str definition, int* exitCodeOut);
TempStr CloneRenderDiffTemp(int* exitCodeOut);
TempStr DocErrorsResultTemp(int* exitCodeOut);
TempStr DisplayModeResultTemp(Str action, int* exitCodeOut);
TempStr WordCountResultTemp(int* exitCodeOut);
TempStr PageInfoTextResultTemp(int* exitCodeOut);
TempStr PageInfoBarResultTemp(int* exitCodeOut);
TempStr SelectedTextResultTemp(int* exitCodeOut);
// implemented by each app
TempStr TocRowsResultTemp(int* exitCodeOut);
