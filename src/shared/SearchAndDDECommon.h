/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SearchAndDDECommon.cpp and each app's SearchAndDDE.cpp ---

extern StrVec gFindHistory;
struct FindMatchPaintPageRect {
    int pageNo = 0;
    Rect rect;
};
DocController* BrowserFindCtrl(MainWindow* win);
void MarkSearchStart(MainWindow* win);
int BrowserFindGlobalMatchIdx(MainWindow* win, int pageNo, int pageCur);
Str FindTermWithoutWordStartSpace(Str text);
void BrowserFindGotoMatch(MainWindow* win, DocController* md, int pageNo, int idxInPage);
void StartIncrementalFind(MainWindow* win);
u64 MatchKey(int page, int offset);
int CmpFindMatchByPos(const FindMatch* a, const FindMatch* b);
int CmpMatchKey(const u64* a, const u64* b);
int MatchIndexInCache(MainWindow* win, u64 key);
// stop scanning after this many matches: with a common word the full count
// isn't useful, only slow. The status then shows "n / 999+".
constexpr int kMaxFindCount = 999;
TempStr BuildSnippet(EngineBase* engine, const FindMatch& m);
void FreeMatchSnippets(Vec<FindMatch>* matches);
extern int gFindCountCurPage;
void SetFindCountProgressStatus(MainWindow* win, int nFound, int pageNo);
Vec<FindMatch>* CloneMatchesRange(Vec<FindMatch>* matches, int from, int to);
bool FindMatchTouchesVisiblePages(const FindMatch& fm, int firstPage, int lastPage);
void GetVisiblePageRange(DisplayModel* dm, int& firstOut, int& lastOut);
void AppendMatchPageRects(EngineBase* engine, const FindMatch& fm, Vec<FindMatchPaintPageRect>& out);
void AppendPageRectsToScreen(DisplayModel* dm, const Rect& clipRc, const FindMatchPaintPageRect* pageRects, int nRects,
                             Vec<Rect>& out);
void AppendTextSelScreenRects(DisplayModel* dm, const Rect& clipRc, Vec<TextSel>* sel, Vec<Rect>& out);
bool LinkDestHighlightRect(DisplayModel* dm, int pageNo, RectF dest, Rect* out);
Str HandleSearchCmd(HWND hwnd, Str cmd, bool* ack);
Str HandleGotoPageWordCmd(HWND hwnd, Str cmd, bool* ack);
Str HandleGotoCmd(HWND hwnd, Str cmd, bool* ack);
Str HandlePageCmd(HWND hwnd, Str cmd, bool* ack);
Str HandleSetViewCmd(HWND hwnd, Str cmd, bool* ack);
Str HandleFullScreenCmd(HWND hwnd, Str cmd, bool* ack);
Str HandleNewWindowCmd(Str cmd, bool* ack);
Str HandleGetOpenFilesCmd(Str cmd, bool* ack, str::Builder& res);

// implemented by each app
struct FindEndTaskData;
MainWindow* FindDdeTargetWindow(HWND hwnd, Str pdfFile, bool focusTab);
void ShowSearchResult(MainWindow* win, Vec<TextSel>* result, bool goToPage);
void BrowserFindStartSearch(MainWindow* win, DocController* md);
void BrowserFindUpdateStatus(MainWindow* win, DocController* md, int pageCur, int pageTotal);
void ShowMatchCount(MainWindow* win);
void UpdateMatchCount(MainWindow* win, Str text);
bool HasFindText(MainWindow* win);
void FindEndTask(FindEndTaskData* d);

void HighlightRestoredFindTerm(MainWindow* win);

// implemented by each app
void CancelPendingFind(MainWindow* win);
void AbortCount(MainWindow* win);
bool JoinFindThread(MainWindow* win, bool hideMessage);

// references a [firstPos, firstPos + len) slice of gFindMatchPaintCache.positions
struct FindMatchPaintRects {
    u64 key = 0;
    int firstPos = 0;
    int len = 0;
};

struct FindMatchPaintCache {
    int firstPage = 0;
    int lastPage = 0;
    int countEpoch = 0;
    // all page rects for all entries, laid out contiguously; each entry
    // references its rects as a [firstPos, firstPos + len) slice. Both entries
    // and positions are plain POD so they can live in a Vec by value; entries
    // hold indices (not pointers), so they stay valid as positions reallocates.
    Vec<FindMatchPaintPageRect> positions;
    Vec<FindMatchPaintRects> entries;
};

extern FindMatchPaintCache gFindMatchPaintCache;
void RebuildFindMatchPaintCache(MainWindow* win, DisplayModel* dm, int firstPage, int lastPage);

// implemented by each app
bool HandleExecuteCmds(HWND hwnd, Str cmd);

struct OpenManyCopyDataAsync {
    StrVec paths;
    HWND hwnd;
    u32 newWindow;
};

// Payload for async Open command carried in kCopyDataOpen WM_COPYDATA
struct OpenCopyDataAsync {
    Str path; // heap-allocated, freed by OpenCopyDataAsyncRun
    u32 newWindow;
};

// streaming partial results to the floating results list while the scan runs:
// first batch after kFindResultsFirstBatch matches, then a batch only when
// both kFindResultsBatch new matches accumulated and kFindResultsBatchMs
// passed since the last one (avoids flooding the UI thread for common words)
constexpr int kFindResultsFirstBatch = 16;

// Chrome-style orange for the non-active find matches. The active (current)
// match uses the user-customizable FixedPageUI.SelectionColor instead, so it
// stands out with the color the user finds most noticeable (issue #5740).
constexpr Color kFindOtherMatchColor = MkRgb(0xff, 0x96, 0x32);

// implemented by each app
bool ApplyFindPageRange(MainWindow* win);
void StartFindCount(MainWindow* win, Str text, bool matchCase, bool matchWholeWord);

Str HandleSyncCmd(Str cmd, bool* ack);

// implemented by each app
struct UpdateFindStatusData;
void UpdateFindStatus(UpdateFindStatusData* d);
void FindSetToolbarBusy(MainWindow* win, bool busy);
void FindWindowDocChanged(MainWindow* win);
void FindResultsInstalled(MainWindow* win, bool gotSnippets);
void FindCountShown(MainWindow* win);
void FindStatusChanged(MainWindow* win);
void HideFwdSearchMarkAfter(MainWindow* win, int delayMs);
MainWindow* WindowFromHwnd(HWND hwnd);
MainWindow* LastActiveWindow();
Str HandleAppRequestCmd(Str cmd, bool* ack, str::Builder& res);
void DdeExecuteBegin();

struct UpdateFindStatusData {
    MainWindow* win;
    int current;
    int total;
    bool showProgress;
    // page the incremental find is on. Written on the UI thread.
    int* firstPage = nullptr;
};
struct CountThreadData;
struct FindThreadData;
void CountThread(CountThreadData* d);
void FindThread(FindThreadData* ftd);

Str HandleGetFileStateCmd(Str cmd, bool* ack, str::Builder& res);
Str HandleCmdCommand(HWND hwnd, Str cmd, bool* ack);
Str HandleOpenCmd(Str cmd, bool* ack);
void FindDebounceStop(MainWindow* win);
void BrowserFindStatusShown(MainWindow* win);
TempStr FindPagesEditTextTemp(MainWindow* win);

// what find marks are painted on: GDI+ in orig, gpui in ng
#if defined(SUMATRA_NG)
using PaintRef = gpui::PaintCtx*;
#else
using PaintRef = Gfx*;
#endif
Rect FindPaintClipRc(MainWindow* win, DisplayModel* dm);

void PaintCurrentFindMatch(MainWindow* win, DisplayModel* dm, TextSearch* ts, PaintRef ctx);
