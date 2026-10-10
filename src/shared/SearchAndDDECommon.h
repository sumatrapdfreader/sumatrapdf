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
