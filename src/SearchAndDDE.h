/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define kSumatraDdeServer L"SUMATRA"
#define kSumatraDdeTopic L"control"

struct Gfx;

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
#define kCopyDataDdeW 0x44646557     // 'DdeW'
#define kCopyDataOpen 0x4F70656E     // 'Open'
#define kCopyDataOpenMany 0x4F704D6E // 'OpMn'

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

#define HIDE_FWDSRCHMARK_TIMER_ID 4
#define HIDE_FWDSRCHMARK_DELAY_IN_MS 400
#define HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS 100
#define HIDE_FWDSRCHMARK_STEPS 5

// find-as-you-type debounce timer (lives on hwndFrame); see SearchAndDDE.cpp
#define kFindDebounceTimerId 0x100

bool NeedsFindUI(MainWindow* win);
void ClearSearchResult(MainWindow* win);
bool OnInverseSearch(MainWindow* win, int x, int y);
void ShowForwardSearchResult(MainWindow* win, Str fileName, int line, int col, int ret, int page, Vec<Rect>& rects);
void PaintForwardSearchMark(MainWindow* win, Gfx* gfx);
void PaintAllFindMatches(MainWindow* win, Gfx* gfx);
void InvalidateFindMatchPaintCache();

void FindPrev(MainWindow* win);
void FindNext(MainWindow* win);
void FindFirst(MainWindow* win);
void FindToggleMatchCase(MainWindow* win);
void FindToggleMatchWholeWord(MainWindow* win);
void OnFindBarTextChanged(MainWindow* win);
bool ParseFindPageRange(Str s, int nPages, Vec<bool>& allowedOut);
void FindDebounceTimerFired(MainWindow* win);
bool FindFlushPendingSearch(MainWindow* win);
void GoToFindMatch(MainWindow* win, int startPage, int startGlyph, int endPage, int endGlyph);
void ClearFindMatches(MainWindow* win);
void InvalidateFindForDocumentChange(MainWindow* win);
void FindSelection(MainWindow* win, TextSearch::Direction direction);
void BrowserFindResultReceived(MainWindow* win, int gen, int current, int total);
void BrowserFindAllResultReceived(MainWindow* win, Str payload);
bool AbortFinding(MainWindow* win, bool hideMessage);
void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, bool showProgress);
void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, Str text, bool wasModified, bool showProgress);
extern bool gIsStartup;
extern StrVec gDdeOpenOnStartup;
