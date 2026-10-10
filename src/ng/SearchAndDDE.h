/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SearchAndDDE.h declares the search driver and the DDE server.
// The search half is step 8b, DDE is step 17a (Windows only) and the
// forward-search mark plus inverse search are step 18.

namespace gpui {
struct PaintCtx;
}

bool NeedsFindUI(MainWindow* win);
void ClearSearchResult(MainWindow* win);
void PaintAllFindMatches(MainWindow* win, gpui::PaintCtx* ctx);
void InvalidateFindMatchPaintCache();

void FindPrev(MainWindow* win);
void FindNext(MainWindow* win);
void FindFirst(MainWindow* win);
void FindToggleMatchCase(MainWindow* win);
void FindToggleMatchWholeWord(MainWindow* win);
void OnFindBarTextChanged(MainWindow* win);
bool ParseFindPageRange(Str s, int nPages, Vec<bool>& allowedOut);
// ng: orig arms a WM_TIMER for the find-as-you-type debounce; the shell's tick
// calls this instead
void FindDebounceTick(MainWindow* win, int elapsedMs);
bool FindFlushPendingSearch(MainWindow* win);
bool FindTermDiffersFromLast(MainWindow* win);
void GoToFindMatch(MainWindow* win, int startPage, int startGlyph, int endPage, int endGlyph);
void ClearFindMatches(MainWindow* win);
void InvalidateFindForDocumentChange(MainWindow* win);
void FindSelection(MainWindow* win, TextSearch::Direction direction);
void BrowserFindResultReceived(MainWindow* win, int gen, int current, int total);
void BrowserFindAllResultReceived(MainWindow* win, Str payload);
bool AbortFinding(MainWindow* win, bool hideMessage);
void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, bool showProgress);
void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, Str text, bool wasModified, bool showProgress);
void StartSearchFromCommandLine(MainWindow* win, Str text);
void StartPendingSearch(MainWindow* win);
TempStr CurrentFindTermTemp(MainWindow* win);
void EnsureFindSnippets(MainWindow* win);

// how long the forward-search mark stays before it starts fading, and the
// fade itself (orig's WM_TIMER constants)
constexpr int kHideFwdSearchMarkDelayInMs = 400;
// an internal-link destination mark is held longer: it has to survive the page
// jump that follows it
constexpr int kHideLinkDestMarkDelayInMs = 2000;
constexpr int kHideFwdSearchMarkDecayIntervalInMs = 100;
constexpr int kHideFwdSearchMarkSteps = 5;

void PaintForwardSearchMark(MainWindow* win, gpui::PaintCtx* ctx);
// ng: orig arms kHideFwdSearchMarkTimerID; the shell's tick fades the mark
void ForwardSearchMarkTick(MainWindow* win, int elapsedMs);
void ShowForwardSearchResult(MainWindow* win, Str fileName, int line, int col, int ret, int page, Vec<Rect>& rects);
void ShowLinkDestHighlight(MainWindow* win, int pageNo, RectF dest);
TempStr LinkDestHighlightResultTemp(int* exitCodeOut);
// double-click on the canvas: ask the .synctex / .pdfsync file where this came
// from and open it in the configured editor. true if it handled the click.
bool OnInverseSearch(MainWindow* win, int x, int y);

void RememberFindQuery(Str);
TempStr FindHistoryResultTemp(int* exitCodeOut);
// the remembered find terms, newest first
const StrVec& FindHistory();
// what the find state is, for the scripted tests
TempStr FindStateResultTemp(MainWindow* win);

bool ExecuteDdeCmds(Str cmd);

extern bool gIsStartup;
extern StrVec gDdeOpenOnStartup;

#if OS_WIN
// ng: orig's DDE half of this file, Windows only. The commands, their grammar
// and the WM_COPYDATA fast paths are orig's.
constexpr const WCHAR* kSumatraDdeServer = L"SUMATRA";
constexpr const WCHAR* kSumatraDdeTopic = L"control";

// WM_COPYDATA magic numbers (in COPYDATASTRUCT::dwData):
// - kCopyDataDdeW   : payload is a null-terminated UTF-16 DDE command string
//                    ("[Open(\"...\",...)]..."). Handled synchronously via
//                    the full DDE grammar in HandleExecuteCmds.
// - kCopyDataOpen   : payload is a SumatraOpenCopyData struct followed by the
//                    UTF-8 null-terminated path. Handled asynchronously so
//                    the sending instance (launched by Explorer for
//                    reuseInstance) can exit immediately without waiting for
//                    the receiver to finish loading the file.
// - kCopyDataOpenMany: payload is a SumatraOpenManyCopyData struct followed by
//                     UTF-8 null-terminated paths.
constexpr int kCopyDataDdeW = 0x44646557;     // 'DdeW'
constexpr int kCopyDataOpen = 0x4F70656E;     // 'Open'
constexpr int kCopyDataOpenMany = 0x4F704D6E; // 'OpMn'

struct SumatraOpenCopyData {
    u32 newWindow; // 0: reuse existing, non-zero: force new window
    // followed by UTF-8 path, null-terminated
};

struct SumatraOpenManyCopyData {
    u32 newWindow;
    u32 pathCount;
    // followed by pathCount UTF-8 paths, each null-terminated
};

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT OnDDExecute(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT OnDDERequest(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT OnDDETerminate(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT OnCopyData(HWND hwnd, WPARAM wp, LPARAM lp);

// loads what arrived while gIsStartup was set and clears the queue
void LoadDdeOpenOnStartup(MainWindow* win);
#endif
