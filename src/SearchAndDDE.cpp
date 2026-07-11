/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "PdfSync.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "AppTools.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "Toolbar.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Translations.h"
#include "Version.h"

bool gIsStartup = false;
StrVec gDdeOpenOnStartup;

// TODO: expose as a setting; default true for testing
bool gShowAllMatches = true;

// Chrome-style orange for the non-active find matches. The active (current)
// match uses the user-customizable FixedPageUI.SelectionColor instead, so it
// stands out with the color the user finds most noticeable (issue #5740).
constexpr COLORREF kFindOtherMatchColor = RGB(0xff, 0x96, 0x32);

struct FindMatchPaintPageRect {
    int pageNo = 0;
    Rect rect{};
};

// references a [firstPos, firstPos + len) slice of gFindMatchPaintCache.positions
struct FindMatchPaintRects {
    u64 key = 0;
    int firstPos = 0;
    int len = 0;
};

static struct {
    int firstPage = 0;
    int lastPage = 0;
    LONG countEpoch = 0;
    // all page rects for all entries, laid out contiguously; each entry
    // references its rects as a [firstPos, firstPos + len) slice. Both entries
    // and positions are plain POD so they can live in a Vec by value; entries
    // hold indices (not pointers), so they stay valid as positions reallocates.
    Vec<FindMatchPaintPageRect> positions;
    Vec<FindMatchPaintRects> entries;
} gFindMatchPaintCache;

static void FreeFindMatchPaintCacheEntries() {
    gFindMatchPaintCache.entries.Reset();
    gFindMatchPaintCache.positions.Reset();
}

void InvalidateFindMatchPaintCache() {
    FreeFindMatchPaintCacheEntries();
    gFindMatchPaintCache.firstPage = 0;
    gFindMatchPaintCache.lastPage = 0;
    gFindMatchPaintCache.countEpoch = 0;
}

Kind kNotifFindProgress = "findProgress";

// the controller if the current document is rendered in a webview that
// supports our in-page find (chm / markdown with a WebView2 backend: native
// find bar + highlighting driven from JS injected into the webview)
static DocController* BrowserFindCtrl(MainWindow* win) {
    DocController* ctrl = win->ctrl;
    if (ctrl && ctrl->CanFindInPage()) {
        return ctrl;
    }
    return nullptr;
}

// start a new find in the browser-hosted (chm / markdown) webview for the
// find bar's text: highlight
// the current page and sweep all pages for the match list. Results arrive
// asynchronously via BrowserFindResultReceived() / BrowserFindAllResultReceived()
static void BrowserFindStartSearch(MainWindow* win, DocController* md) {
    TempStr term = HwndGetTextTemp(win->hwndFindEdit);
    str::ReplaceWithCopy(&win->browserFindTerm, term);
    ClearFindMatches(win); // also resets browserFindPageCurrent / browserFindCurrent / browserFindTotal
    win->browserFindGen++;
    md->FindStart(term, win->findMatchCase, win->findMatchWholeWord, win->browserFindGen);
    md->FindAllPages(term, win->findMatchCase, win->findMatchWholeWord, win->browserFindGen);
}

// index into win->findMatches of the in-page match pageCur (1-based) on
// pageNo, or -1. findMatches is in (page, in-page index) order
static int BrowserFindGlobalMatchIdx(MainWindow* win, int pageNo, int pageCur) {
    if (pageCur <= 0) {
        return -1;
    }
    int n = len(win->findMatches);
    for (int i = 0; i < n; i++) {
        const FindMatch& fm = win->findMatches[i];
        if (fm.startPage == pageNo && fm.startGlyph == pageCur - 1) {
            return i;
        }
    }
    return -1;
}

// update the find bar's "n / m" status (and the results list selection) from
// the current in-page match and the all-pages sweep
static void BrowserFindUpdateStatus(MainWindow* win, DocController* md, int pageCur, int pageTotal) {
    if (win->browserFindTotal < 0) {
        // the all-pages sweep hasn't finished: show per-page numbers for now
        TempStr s = fmt("%d / %d", pageCur, pageTotal);
        FindBarSetStatus(win, s);
        return;
    }
    win->browserFindCurrent = BrowserFindGlobalMatchIdx(win, md->CurrentPageNo(), pageCur);
    TempStr s = fmt("%d / %d", win->browserFindCurrent + 1, win->browserFindTotal);
    FindBarSetStatus(win, s);
    FindWindowRefreshResults(win); // mirror the current match in the results list
}

void BrowserFindResultReceived(MainWindow* win, int gen, int current, int total) {
    if (gen != win->browserFindGen || !IsFindUIVisible(win)) {
        // result of a superseded search or the find UI was closed
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (!md) {
        return;
    }
    win->browserFindPageCurrent = current;
    BrowserFindUpdateStatus(win, md, current, total);
}

// payload: "<gen> <total> <records>", records separated by \x1e (record sep),
// each "<page>\x1f<idx>\x1f<snippet>" (\x1f: unit sep). Built by searchAll()
// in kFindInPageJs (BrowserDocView.cpp)
void BrowserFindAllResultReceived(MainWindow* win, Str payload) {
    int gen = 0;
    int total = 0;
    Str rest = str::Parse(payload, "%d %d ", &gen, &total);
    if (str::IsNull(rest) || gen != win->browserFindGen || !IsFindUIVisible(win)) {
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (!md) {
        return;
    }
    int pageCur = win->browserFindPageCurrent; // survives the ClearFindMatches below
    ClearFindMatches(win);
    win->browserFindPageCurrent = pageCur;
    while (rest.len > 0) {
        int recLen = rest.len;
        for (int i = 0; i < rest.len; i++) {
            if (rest.s[i] == '\x1e') {
                recLen = i;
                break;
            }
        }
        Str rec = Str(rest.s, recLen);
        rest = (recLen < rest.len) ? Str(rest.s + recLen + 1, rest.len - recLen - 1) : Str();
        int page = 0;
        int idx = 0;
        Str snippet = str::Parse(rec, "%d\x1f%d\x1f", &page, &idx);
        if (str::IsNull(snippet)) {
            continue;
        }
        FindMatch fm;
        fm.startPage = page;
        fm.startGlyph = idx;
        fm.endPage = page;
        fm.endGlyph = idx;
        fm.snippet = str::Dup(snippet);
        win->findMatches.Append(fm);
    }
    win->browserFindTotal = total;
    win->findCountHasSnippets = true;
    BrowserFindUpdateStatus(win, md, win->browserFindPageCurrent, total); // also refreshes the results list
}

// jump to the idxInPage-th match on pageNo: directly if that page is showing,
// otherwise navigate there and re-run the in-page find once it has loaded
static void BrowserFindGotoMatch(MainWindow* win, DocController* md, int pageNo, int idxInPage) {
    if (pageNo == md->CurrentPageNo()) {
        md->FindGoto(idxInPage);
        return;
    }
    md->GoToPageWithFind(pageNo, win->browserFindTerm, win->findMatchCase, win->findMatchWholeWord, idxInPage,
                         win->browserFindGen);
}

// advance to the next/previous match, across page boundaries (wraps around)
static void BrowserFindNextPrev(MainWindow* win, DocController* md, bool forward) {
    // typing still pending: run the search first instead of advancing
    // through the previous term's matches
    if (FindFlushPendingSearch(win)) {
        return;
    }
    int n = len(win->findMatches);
    if (win->browserFindTotal < 0 || n == 0) {
        BrowserFindStartSearch(win, md);
        return;
    }
    int j = win->browserFindCurrent;
    if (j < 0) {
        j = forward ? 0 : n - 1;
    } else {
        j = forward ? (j + 1) % n : (j + n - 1) % n;
    }
    win->browserFindCurrent = j;
    const FindMatch& fm = win->findMatches[j];
    BrowserFindGotoMatch(win, md, fm.startPage, fm.startGlyph);
}

// don't show the Search UI for document types that don't
// support extracting text and/or navigating to a specific
// text selection; default to showing it, since most users
// will never use a format that does not support search
bool NeedsFindUI(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return true;
    }
    if (BrowserFindCtrl(win)) {
        return true;
    }
    if (!win->AsFixed()) {
        return false;
    }
    if (win->AsFixed()->GetEngine()->IsImageCollection()) {
        return false;
    }
    return true;
}

void FindFirst(MainWindow* win) {
    if (BrowserFindCtrl(win)) {
        // chm / markdown in a webview: our own find bar drives the search
        // inside the webview
        ShowFindBar(win);
        if (win->hwndFindEdit) {
            HwndSetFocus(win->hwndFindEdit);
            Edit_SetSel(win->hwndFindEdit, 0, -1);
        }
        return;
    }
    // IE backend: fall back to the browser's own find dialog
    if (win->AsChm()) {
        win->AsChm()->FindInCurrentPage();
    } else if (win->AsMarkdown()) {
        win->AsMarkdown()->FindInCurrentPage();
        return;
    }

    if (!win->AsFixed() || !NeedsFindUI(win)) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    bool hadFindFocus = HwndIsFocused(win->hwndFindEdit);

    // show the floating Chrome-style find bar (creates it lazily if needed)
    ShowFindBar(win);

    // If focus was in the document (not find bar), copy selected text
    // to find edit only if it's different from current text. Setting the text
    // triggers find-as-you-type via the bar's onTextChanged handler.
    if (!hadFindFocus && dm->textSelection->result.len > 0) {
        Str sel = dm->textSelection->ExtractText(" ");
        TempStr selection = str::DupTemp(sel);
        str::Free(sel);
        selection.len -= str::NormalizeWSInPlace(selection);
        if (len(selection) > 0) {
            TempStr current = HwndGetTextTemp(win->hwndFindEdit);
            if (!str::EqI(selection, current)) {
                AbortFinding(win, false);
                dm->textSearch->SetLastResult(dm->textSelection);
                HwndSetText(win->hwndFindEdit, selection);
            }
        }
    }

    if (win->hwndFindEdit) {
        HwndSetFocus(win->hwndFindEdit);
        Edit_SetSel(win->hwndFindEdit, 0, -1);
    }
}

// debounce delays (ms) for find-as-you-type. Short terms (1-2 chars) match a
// lot of text and the search is expensive, so wait longer before starting them
// (issue #4626). Enter bypasses the wait (see FindFlushPendingSearch).
constexpr UINT kFindDebounceDelayMs = 500;
constexpr UINT kFindDebounceShortDelayMs = 1000;

// run the actual incremental search; assumes there is non-empty find text
static void StartIncrementalFind(MainWindow* win) {
    DocController* md = BrowserFindCtrl(win);
    if (md) {
        BrowserFindStartSearch(win, md);
        return;
    }
    // the full-document count (n/m + results list) is kicked from FindEndTask,
    // after this find thread exits, so the two never touch the engine's text
    // extraction concurrently (mupdf isn't safe for that)
    FindTextOnThread(win, TextSearch::Direction::Forward, false);
}

// find-as-you-type: called when the find bar's edit text changes. Instead of
// searching on every keystroke, (re)arm a debounce timer; the search starts a
// short while after the user stops typing (issue #4626).
void OnFindBarTextChanged(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    TempStr s = HwndGetTextTemp(win->hwndFindEdit);
    if (len(s) == 0) {
        AbortFinding(win, true); // also cancels a pending debounce timer
        DocController* md = BrowserFindCtrl(win);
        if (md) {
            md->FindClear(); // remove the highlights in the webview
        }
        ClearSearchResult(win);
        FindBarSetStatus(win, "");
        ClearFindMatches(win);
        FindWindowRefreshResults(win); // empty the results list
        return;
    }
    size_t nChars = HwndGetTextLen(win->hwndFindEdit);
    UINT delay = (nChars <= 2) ? kFindDebounceShortDelayMs : kFindDebounceDelayMs;
    // SetTimer with the same id replaces the previous timer, so each keystroke
    // restarts the countdown
    SetTimer(win->hwndFrame, kFindDebounceTimerId, delay, nullptr);
    win->findDebouncePending = true;
}

void FindDebounceTimerFired(MainWindow* win) {
    KillTimer(win->hwndFrame, kFindDebounceTimerId);
    if (!win->findDebouncePending) {
        return;
    }
    win->findDebouncePending = false;
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (win->hwndFindEdit && HwndGetTextLen(win->hwndFindEdit) > 0) {
        StartIncrementalFind(win);
    }
}

static bool HasFindText(MainWindow* win) {
    return win->hwndFindEdit && HwndGetTextLen(win->hwndFindEdit) > 0;
}

bool FindFlushPendingSearch(MainWindow* win) {
    if (!win->findDebouncePending) {
        return false;
    }
    win->findDebouncePending = false;
    KillTimer(win->hwndFrame, kFindDebounceTimerId);
    if (HasFindText(win)) {
        StartIncrementalFind(win);
    }
    return true;
}

void FindNext(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (!HasFindText(win)) {
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (md) {
        BrowserFindNextPrev(win, md, true);
        return;
    }
    FindTextOnThread(win, TextSearch::Direction::Forward, true);
}

void FindPrev(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (!HasFindText(win)) {
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (md) {
        BrowserFindNextPrev(win, md, false);
        return;
    }
    FindTextOnThread(win, TextSearch::Direction::Backward, true);
}

void FindToggleMatchCase(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (!md && !win->AsFixed()) {
        return;
    }
    win->findMatchCase = !win->findMatchCase;
    if (win->AsFixed()) {
        win->AsFixed()->textSearch->SetMatchCase(win->findMatchCase);
    }
    FindBarSetMatchCaseChecked(win, win->findMatchCase);
    if (win->hwndFindEdit) {
        Edit_SetModify(win->hwndFindEdit, TRUE);
    }
    // re-run the search with the new match-case setting
    if (HasFindText(win)) {
        if (md) {
            BrowserFindStartSearch(win, md);
        } else {
            FindTextOnThread(win, TextSearch::Direction::Forward, true);
        }
    }
}

void FindToggleMatchWholeWord(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (!md && !win->AsFixed()) {
        return;
    }
    win->findMatchWholeWord = !win->findMatchWholeWord;
    if (win->AsFixed()) {
        win->AsFixed()->textSearch->SetMatchWholeWord(win->findMatchWholeWord);
    }
    FindBarSetMatchWholeWordChecked(win, win->findMatchWholeWord);
    if (win->hwndFindEdit) {
        Edit_SetModify(win->hwndFindEdit, TRUE);
    }
    // re-run the search with the new whole-word setting
    if (HasFindText(win)) {
        if (md) {
            BrowserFindStartSearch(win, md);
        } else {
            FindTextOnThread(win, TextSearch::Direction::Forward, true);
        }
    }
}

void FindSelection(MainWindow* win, TextSearch::Direction direction) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win) || !win->AsFixed()) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!win->CurrentTab()->selectionOnPage || 0 == dm->textSelection->result.len) {
        return;
    }

    Str sel = dm->textSelection->ExtractText(" ");
    TempStr selection = str::DupTemp(sel);
    str::Free(sel);
    selection.len -= str::NormalizeWSInPlace(selection);
    if (len(selection) == 0) {
        return;
    }

    HwndSetText(win->hwndFindEdit, selection);
    AbortFinding(win, false); // cancel "find as you type"
    Edit_SetModify(win->hwndFindEdit, FALSE);
    dm->textSearch->SetLastResult(dm->textSelection);

    FindTextOnThread(win, direction, true);
}

static void ShowSearchResult(MainWindow* win, TextSel* result, bool addNavPt) {
    ReportIf(0 == result->len || !result->pages || !result->rects);
    if (0 == result->len || !result->pages || !result->rects) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (addNavPt || !dm->PageShown(result->pages[0]) ||
        (dm->GetZoomVirtual() == kZoomFitPage || dm->GetZoomVirtual() == kZoomFitContent)) {
        win->ctrl->GoToPage(result->pages[0], addNavPt);
    }

    // Find never changes the text selection: all matches (including the active
    // one) are highlighted independently by PaintAllFindMatches, so the user's
    // selection highlight is separate and survives searching (issue #5737).
    dm->ShowResultRectToScreen(result);
    InvalidateFindMatchPaintCache();
    ScheduleRepaint(win, 0);
}

void ClearSearchResult(MainWindow* win) {
    // clear only the find-match highlights, never the user's text selection:
    // find and selection highlights are tracked independently (issue #5737)
    ClearFindMatches(win); // also invalidates the find-match paint cache
    ScheduleRepaint(win, 0);
}

struct UpdateFindStatusData {
    MainWindow* win;
    int current;
    int total;
    bool showProgress;
};

static void UpdateFindStatus(UpdateFindStatusData* d) {
    AutoDelete delData(d);

    auto win = d->win;
    if (!IsMainWindowValid(win) || win->findCancelled) {
        return;
    }
    if (!d->showProgress) {
        // find-as-you-type: don't let the incremental find scan the whole
        // document. The n/m counter and the floating results list are built by
        // the count thread (which does its own full scan), so bail out early and
        // leave the heavy lifting to it.
        win->findCancelled = true;
    }
    // explicit Find Next/Prev (showProgress): keep going to completion. There's
    // no progress notification now -- the n/m counter is the only feedback.
}

struct FindThreadData {
    MainWindow* win = nullptr;
    TextSearch::Direction direction = TextSearch::Direction::Forward;
    bool wasModified = false;
    bool showProgress = false;
    Str text;
    ThreadHandle thread = nullptr;

    FindThreadData(MainWindow* win, TextSearch::Direction direction, Str text, bool wasModified) {
        this->win = win;
        this->direction = direction;
        this->text = str::Dup(text);
        this->wasModified = wasModified;
    }
    ~FindThreadData() {
        str::Free(text);
        CloseHandle(thread);
    }

    void ShowUI(bool showProgress) {
        // no "Searching n of m..." notification anymore: the find UI's own n/m
        // counter (and the floating window's results list) is the feedback. We
        // still remember showProgress to decide whether the incremental find may
        // bail early (see UpdateFindStatus).
        this->showProgress = showProgress;

        SetToolbarButtonEnableState(win, CmdFindPrev, false);
        SetToolbarButtonEnableState(win, CmdFindNext, false);
        SetToolbarButtonEnableState(win, CmdFindToggleMatchCase, false);
        SetToolbarButtonEnableState(win, CmdFindToggleMatchWholeWord, false);
    }

    void HideUI(bool success, bool loopedAround) const {
        SetToolbarButtonEnableState(win, CmdFindPrev, true);
        SetToolbarButtonEnableState(win, CmdFindNext, true);
        SetToolbarButtonEnableState(win, CmdFindToggleMatchCase, true);
        SetToolbarButtonEnableState(win, CmdFindToggleMatchWholeWord, true);

        if (!success && !loopedAround) {
            // i.e. canceled
            FindBarSetStatus(win, "");
        } else if (!success && loopedAround) {
            // keep it compact and consistent with the "n / m" counter
            FindBarSetStatus(win, "0 / 0");
        }
        // else: a match was found; the "n / m" counter (set by UpdateMatchCount
        // after this) is the only feedback - no beep on wrap-around
    }

    bool WasCanceled() {
        bool winValid = IsMainWindowValid(win);
        auto res = !winValid || win->findCancelled;
        if (res) {
            logf("FindThreadData: WasCanceled() returns true, isMainWindowValid: %d, win->findCancelled: %d\n",
                 (int)winValid, (int)win->findCancelled);
        }
        return res;
    }

    void UpdateProgress(int current, int total) {
        auto data = new UpdateFindStatusData;
        data->win = this->win;
        data->current = current;
        data->total = total;
        data->showProgress = this->showProgress;
        auto fn = MkFunc0<UpdateFindStatusData>(UpdateFindStatus, data);
        uitask::Post(fn, nullptr);
    }
};

struct FindEndTaskData {
    MainWindow* win = nullptr;
    FindThreadData* ftd = nullptr;
    TextSel* textSel = nullptr;
    bool wasModifiedCanceled = false;
    bool loopedAround = false;
    FindEndTaskData() = default;
    ~FindEndTaskData() {
        delete ftd;
        ftd = nullptr;
    }
};

// ---- find bar "n / m" match counter ----------------------------------------
//
// We show the position of the current match among all matches in the document.
// Counting all matches requires a full-document scan, so it runs on a background
// thread and the per-match positions are cached: prev/next (which don't change
// the term) recompute the index instantly from the cache, and a new scan only
// runs when the search term or match-case option changes.

static u64 MatchKey(int page, int offset) {
    return ((u64)(u32)page << 32) | (u32)offset;
}

// 1-based index of `key` within the positions cache, or 0 if not found.
// positions are in scan order (starting at the page current when the scan
// started, wrapping around), so this is a linear lookup (n <= kMaxFindCount)
static int MatchIndexInCache(MainWindow* win, u64 key) {
    Vec<u64>& pos = win->findCountPositions;
    int n = len(pos);
    for (int i = 0; i < n; i++) {
        if (pos[i] == key) {
            return i + 1;
        }
    }
    return 0;
}

// update the find bar with "n / m" from the (valid) cache and the current match
static void ShowMatchCount(MainWindow* win) {
    if (!win->findCountValid) {
        return; // count not ready yet; leave whatever status is showing
    }
    int total = len(win->findCountPositions);
    int n = 0;
    DisplayModel* dm = win->AsFixed();
    if (dm && dm->textSearch) {
        u64 key = MatchKey(dm->textSearch->startPage, dm->textSearch->startGlyph);
        n = MatchIndexInCache(win, key);
    }
    TempStr s = fmt("%d / %d%s", n, total, Str(win->findCountCapped ? "+" : ""));
    FindBarSetStatus(win, s);
}

// cap on how many per-match snippets we build for the floating results list
// (matches beyond this still count toward "n / m", just aren't listed)
constexpr int kMaxFindResults = 5000;

// stop scanning after this many matches: with a common word the full count
// isn't useful, only slow. The status then shows "n / 999+".
constexpr int kMaxFindCount = 999;

void ClearFindMatches(MainWindow* win) {
    int n = len(win->findMatches);
    for (int i = 0; i < n; i++) {
        str::Free(win->findMatches[i].snippet);
    }
    win->findMatches.Reset();
    win->findCountHasSnippets = false;
    InvalidateFindMatchPaintCache();
    // for markdown, findMatches came from the webview's all-pages sweep; reset
    // the state tied to it (but not browserFindGen, which is monotonic so stale
    // async results keep getting dropped)
    win->browserFindPageCurrent = 0;
    win->browserFindCurrent = -1;
    win->browserFindTotal = -1;
}

// build a one-line "...context match context..." snippet (UTF-8) around a match
static TempStr BuildSnippet(EngineBase* engine, const FindMatch& m) {
    int textLen = 0;
    Str pageText = engine->GetTextForPage(m.startPage, &textLen);
    if (!pageText) {
        return {};
    }
    int mStart = limitValue(m.startGlyph, 0, textLen);
    int mEnd = (m.endPage == m.startPage) ? m.endGlyph : textLen;
    mEnd = limitValue(mEnd, mStart, textLen);
    const int kCtx = 40;
    int from = std::max(0, mStart - kCtx);
    int to = std::min(textLen, mEnd + kCtx);
    Str sub = str::Dup(Utf8SliceByCodepoints(pageText, from, to - from));
    str::NormalizeWSInPlace(sub);
    TempStr u = str::DupTemp(sub);
    str::FreePtr(&sub);
    return fmt("%s%s%s", Str(from > 0 ? "..." : ""), u, Str(to < textLen ? "..." : ""));
}

struct CountThreadData {
    MainWindow* win = nullptr;
    EngineBase* engine = nullptr; // AddRef'd by the caller, released by the thread
    Str text;
    bool matchCase = false;
    bool matchWholeWord = false;
    bool wantMatchList = false; // build findMatches (for all-match painting or the results list)
    bool wantSnippets = false;  // build per-match snippet strings for the results list
    int startPage = 1;          // scan from here (the current page), wrapping around
    LONG epoch = 0;
    ThreadHandle thread = nullptr;

    CountThreadData(MainWindow* win, EngineBase* engine, Str text, bool matchCase, bool matchWholeWord,
                    bool wantMatchList, bool wantSnippets, int startPage, LONG epoch) {
        this->win = win;
        this->engine = engine;
        this->text = str::Dup(text);
        this->matchCase = matchCase;
        this->matchWholeWord = matchWholeWord;
        this->wantMatchList = wantMatchList;
        this->wantSnippets = wantSnippets;
        this->startPage = startPage;
        this->epoch = epoch;
    }
    ~CountThreadData() {
        str::Free(text);
        SafeCloseThreadHandle(&thread);
    }
};

static void FreeMatchSnippets(Vec<FindMatch>* matches) {
    if (!matches) {
        return;
    }
    for (int i = 0; i < len(*matches); i++) {
        str::Free((*matches)[i].snippet);
    }
}

struct CountEndTaskData {
    MainWindow* win = nullptr;
    CountThreadData* ctd = nullptr;
    Vec<u64>* positions = nullptr;
    bool capped = false;               // scan stopped at kMaxFindCount matches
    Vec<FindMatch>* matches = nullptr; // nullptr unless snippets were requested
    ~CountEndTaskData() {
        delete ctd;
        delete positions;
        FreeMatchSnippets(matches); // frees any snippets not transferred to win
        delete matches;
    }
};

static void StartFindCount(MainWindow* win, Str text, bool matchCase, bool matchWholeWord);

static void CountEndTask(CountEndTaskData* d) {
    AutoDelete delData(d);
    MainWindow* win = d->win;
    CountThreadData* ctd = d->ctd;
    if (!IsMainWindowValid(win)) {
        return;
    }
    if (win->findCountThread != ctd->thread) {
        return; // superseded (shouldn't happen with the single-worker model)
    }
    win->findCountThread = nullptr;
    if (win->findCountEpoch == ctd->epoch) {
        // not canceled: install the freshly built cache (steal text from ctd)
        str::FreePtr(&win->findCountText);
        win->findCountText = ctd->text;
        ctd->text = {};
        win->findCountMatchCase = ctd->matchCase;
        win->findCountMatchWholeWord = ctd->matchWholeWord;
        win->findCountEngine = ctd->engine;
        win->findCountPositions = *d->positions;
        win->findCountCapped = d->capped;
        win->findCountValid = true;
        if (d->matches) {
            // install the snippet list (steal ownership of the snippet strings)
            ClearFindMatches(win);
            win->findMatches = *d->matches;
            for (int i = 0; i < len(*d->matches); i++) {
                (*d->matches)[i].snippet = Str(); // transferred to win->findMatches
            }
            win->findCountHasSnippets = ctd->wantSnippets;
            if (ctd->wantSnippets) {
                FindWindowRefreshResults(win);
            }
        }
        InvalidateFindMatchPaintCache();
        ShowMatchCount(win);
        ScheduleRepaint(win, 0);
    }
    // a newer term arrived while we were scanning: run it now (no worker running)
    if (win->findCountPendingText) {
        Str pending = win->findCountPendingText;
        win->findCountPendingText = {};
        StartFindCount(win, pending, win->findCountPendingMatchCase, win->findCountPendingMatchWholeWord);
        str::Free(pending);
    }
}

static void CountProgress(CountThreadData* d, ProgressUpdateData* data) {
    if (data->wasCancelled) {
        *data->wasCancelled = (d->win->findCountEpoch != d->epoch);
    }
}

// streaming partial results to the floating results list while the scan runs:
// first batch after kFindResultsFirstBatch matches, then a batch only when
// both kFindResultsBatch new matches accumulated and kFindResultsBatchMs
// passed since the last one (avoids flooding the UI thread for common words)
constexpr int kFindResultsFirstBatch = 16;
constexpr int kFindResultsBatch = 100;
constexpr DWORD kFindResultsBatchMs = 500;

struct CountPartialTaskData {
    MainWindow* win = nullptr;
    LONG epoch = 0;
    bool firstBatch = false;
    int nFoundSoFar = 0;               // running match count (keeps growing past kMaxFindResults)
    Vec<FindMatch>* matches = nullptr; // owns the snippets until transferred
    ~CountPartialTaskData() {
        FreeMatchSnippets(matches);
        delete matches;
    }
};

static void CountPartialTask(CountPartialTaskData* d) {
    AutoDelete delData(d);
    MainWindow* win = d->win;
    if (!IsMainWindowValid(win)) {
        return;
    }
    if (win->findCountEpoch != d->epoch) {
        return; // canceled or superseded; drop stale partial results
    }
    // running count while the scan is in flight; ShowMatchCount switches this
    // to "n / m" when the scan finishes
    FindBarSetStatus(win, fmt("%d...", d->nFoundSoFar));
    if (len(*d->matches) > 0) {
        if (d->firstBatch) {
            ClearFindMatches(win);
        }
        for (int i = 0; i < len(*d->matches); i++) {
            win->findMatches.Append((*d->matches)[i]);
            (*d->matches)[i].snippet = Str(); // transferred to win->findMatches
        }
        win->findCountHasSnippets = true;
        InvalidateFindMatchPaintCache();
        FindWindowRefreshResults(win, false /* allowNavigation */);
        ScheduleRepaint(win, 0);
    }
}

// clones matches[from..to) incl. copies of the snippet strings
static Vec<FindMatch>* CloneMatchesRange(Vec<FindMatch>* matches, int from, int to) {
    auto res = new Vec<FindMatch>();
    for (int i = from; i < to; i++) {
        FindMatch fm = (*matches)[i];
        fm.snippet = str::Dup(fm.snippet);
        res->Append(fm);
    }
    return res;
}

static void CountThread(CountThreadData* d) {
    MainWindow* win = d->win;
    EngineBase* engine = d->engine;

    auto positions = new Vec<u64>();
    Vec<FindMatch>* matches = d->wantMatchList ? new Vec<FindMatch>() : nullptr;
    int nSent = 0;        // positions already reported via a partial batch
    int nSentMatches = 0; // matches already streamed to the results list
    DWORD lastSendMs = 0;
    bool capped = false; // scan stopped at kMaxFindCount matches
    {
        TextSearch ts(engine);
        ts.SetMatchCase(d->matchCase);
        ts.SetMatchWholeWord(d->matchWholeWord);
        ts.SetDirection(TextSearch::Direction::Forward);
        ts.progressCb = MkFunc1<CountThreadData, ProgressUpdateData*>(CountProgress, d);
        // scan from the current page so results near the reading position come
        // first; wrap around to cover pages 1..startPage-1 at the end
        bool wrapped = false;
        TextSel* m = ts.FindFirst(d->startPage, d->text);
        // check the epoch at the top so a cancel (AbortCount, which joins us on
        // the UI thread) bails before the expensive snippet build / next scan
        while (m && win->findCountEpoch == d->epoch) {
            if (len(*positions) >= kMaxFindCount) {
                capped = true;
                break;
            }
            positions->Append(MatchKey(ts.startPage, ts.startGlyph));
            if (matches && len(*matches) < kMaxFindResults) {
                FindMatch fm;
                fm.startPage = ts.startPage;
                fm.startGlyph = ts.startGlyph;
                fm.endPage = ts.endPage;
                fm.endGlyph = ts.endGlyph;
                if (d->wantSnippets) {
                    str::ReplaceWithCopy(&fm.snippet, BuildSnippet(engine, fm));
                }
                matches->Append(fm);
            }
            // stream partial results so a slow scan (common word, big doc)
            // shows results and a running count early; the final full list is
            // installed by CountEndTask. CountPartialTask re-checks the epoch
            // on the UI thread, so a stale batch can't clobber a newer search.
            // batching is driven by the (uncapped) position count so the
            // running count keeps updating after kMaxFindResults is reached.
            if (d->wantSnippets) {
                int n = len(*positions);
                bool send;
                if (nSent == 0) {
                    send = n >= kFindResultsFirstBatch;
                } else {
                    send = (n - nSent >= kFindResultsBatch) && (GetTickCount() - lastSendMs >= kFindResultsBatchMs);
                }
                if (send) {
                    auto pd = new CountPartialTaskData;
                    pd->win = win;
                    pd->epoch = d->epoch;
                    pd->firstBatch = (nSentMatches == 0);
                    pd->nFoundSoFar = n;
                    int nMatches = matches ? len(*matches) : 0;
                    pd->matches = CloneMatchesRange(matches, nSentMatches, nMatches);
                    nSent = n;
                    nSentMatches = nMatches;
                    lastSendMs = GetTickCount();
                    uitask::Post(MkFunc0<CountPartialTaskData>(CountPartialTask, pd), "TaskFindCountPartial");
                }
            }
            m = ts.FindNext();
            if (!m && !wrapped && d->startPage > 1) {
                wrapped = true;
                m = ts.FindFirst(1, d->text);
            }
            if (wrapped && m && ts.startPage >= d->startPage) {
                m = nullptr; // came full circle
            }
        }
    }
    SafeEngineRelease(&engine);

    // wait for StartFindCount to record the thread handle (mirrors FindThread)
    while (!win->findCountThread) {
        Sleep(1);
    }

    auto data = new CountEndTaskData;
    data->win = win;
    data->ctd = d;
    data->positions = positions;
    data->capped = capped;
    data->matches = matches;
    auto fn = MkFunc0<CountEndTaskData>(CountEndTask, data);
    uitask::Post(fn, "TaskFindCount");
    DestroyTempArena();
}

// cancel any running/pending count and wait for the worker to exit. The find
// thread and the count thread must never use the engine's text extraction at
// the same time (mupdf isn't safe for concurrent page access), so a find must
// not start while a count is running. The wait is bounded: the worker checks
// the epoch after every match, so it exits within one page's work.
static void AbortCount(MainWindow* win) {
    InterlockedIncrement(&win->findCountEpoch);
    str::FreePtr(&win->findCountPendingText);
    ThreadHandle th = win->findCountThread;
    if (th) {
        WaitForSingleObject(th, INFINITE);
        win->findCountThread = nullptr;
    }
}

// (re)build the match-position cache on a background thread. Coalesces: if a
// scan is already running, remember only the latest request and let the running
// worker start it when it finishes, so rapid typing never piles up scans and
// the UI thread never blocks waiting on a scan.
static void StartFindCount(MainWindow* win, Str text, bool matchCase, bool matchWholeWord) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return;
    }
    win->findCountValid = false;
    FindBarSetStatus(win, "..."); // counting; replaced with "n / m" when done

    if (win->findCountThread) {
        // a scan is in flight: cancel it and queue this request; the running
        // worker's CountEndTask will start it once it exits
        InterlockedIncrement(&win->findCountEpoch);
        str::FreePtr(&win->findCountPendingText);
        win->findCountPendingText = str::Dup(text);
        win->findCountPendingMatchCase = matchCase;
        win->findCountPendingMatchWholeWord = matchWholeWord;
        return;
    }

    engine->AddRef(); // released in CountThread
    // build per-match snippets only when the floating results list is showing;
    // also build the match list (without snippets) when painting all highlights
    bool wantSnippets = gGlobalPrefs->searchUIFloating && IsFindWindowVisible(win);
    bool wantMatchList = wantSnippets || gShowAllMatches;
    LONG epoch = InterlockedIncrement(&win->findCountEpoch);
    int startPage = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    auto d = new CountThreadData(win, engine, text, matchCase, matchWholeWord, wantMatchList, wantSnippets, startPage,
                                 epoch);
    win->findCountThread = nullptr;
    auto fn = MkFunc0<CountThreadData>(CountThread, d);
    win->findCountThread = StartThread(fn, "FindCountThread");
    d->thread = win->findCountThread;
}

// update the n/m counter after a search settles on a match: instant from cache
// when the term/match-case/document are unchanged, otherwise rebuild it
static void UpdateMatchCount(MainWindow* win, Str text) {
    DisplayModel* dm = win->AsFixed();
    void* engine = dm ? (void*)dm->GetEngine() : nullptr;
    bool wantSnippets = gGlobalPrefs->searchUIFloating && IsFindWindowVisible(win);
    bool wantMatchList = wantSnippets || gShowAllMatches;
    bool cacheHit = win->findCountValid && win->findCountText && str::Eq(win->findCountText, text) &&
                    win->findCountMatchCase == win->findMatchCase &&
                    win->findCountMatchWholeWord == win->findMatchWholeWord && win->findCountEngine == engine &&
                    (!wantMatchList || (wantSnippets ? win->findCountHasSnippets : len(win->findMatches) > 0));
    if (cacheHit) {
        // matches are unchanged: just refresh n/m. Don't rebuild the results
        // list here -- it's already populated and rebuilding clears the user's
        // selection (the list is rebuilt only when a new count installs matches).
        ShowMatchCount(win);
    } else {
        StartFindCount(win, text, win->findMatchCase, win->findMatchWholeWord);
    }
}

// navigate to a match chosen from the floating results list and select it, so
// Find Next/Prev and the n/m counter continue from there
void GoToFindMatch(MainWindow* win, int startPage, int startGlyph, int endPage, int endGlyph) {
    if (!win->IsDocLoaded()) {
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (md) {
        // for markdown, startGlyph is the in-page match index (see
        // BrowserFindAllResultReceived)
        win->browserFindCurrent = BrowserFindGlobalMatchIdx(win, startPage, startGlyph + 1);
        BrowserFindGotoMatch(win, md, startPage, startGlyph);
        return;
    }
    if (!win->AsFixed()) {
        return;
    }
    // join any in-flight find/count worker first: we're about to mutate
    // dm->textSearch and read engine page text on the UI thread, which must not
    // race a background thread doing the same (mupdf text extraction isn't
    // reentrant, and textSearch is shared state). skip the join when idle so
    // stepping through the floating results list stays responsive.
    if (win->findThread || win->findCountThread || win->findDebouncePending) {
        AbortFinding(win, true);
    }
    DisplayModel* dm = win->AsFixed();
    TextSearch* ts = dm->textSearch;
    ts->Reset();
    ts->StartAt(startPage, startGlyph);
    ts->SelectUpTo(endPage, endGlyph);
    if (ts->result.len == 0) {
        return;
    }
    // navigate to the match while ts->result is still populated. SetLastResult()
    // below calls SetText(), which clears ts->result whenever the matched text
    // differs from the last search text (e.g. a case-insensitive find where
    // "the" matched "The"), so ShowSearchResult() must run first
    ShowSearchResult(win, &ts->result, true);
    // hand the selection to TextSearch as its "last result" so Find Next/Prev
    // continue from here; SetLastResult owns the findPage/findIndex/pageText
    // bookkeeping (so we don't poke internals or leave pageText null). The match's
    // glyph range (start/end) survives this, so the bookkeeping stays correct.
    ts->SetLastResult(ts);
    ShowMatchCount(win);
}

static void FindEndTask(FindEndTaskData* d) {
    auto win = d->win;
    auto ftd = d->ftd;
    auto textSel = d->textSel;
    auto wasModifiedCanceled = d->wasModifiedCanceled;
    auto loopedAround = d->loopedAround;

    AutoDelete delData(d);
    if (!IsMainWindowValid(win)) {
        return;
    }
    if (win->findThread != ftd->thread) {
        // Race condition: FindTextOnThread/AbortFinding was
        // called after the previous find thread ended but
        // before this FindEndTask could be executed
        return;
    }
    if (!win->IsDocLoaded()) {
        // the UI has already been disabled and hidden
    } else if (textSel) {
        ShowSearchResult(win, textSel, wasModifiedCanceled);
        ftd->HideUI(true, loopedAround);
        UpdateMatchCount(win, ftd->text);
    } else {
        // nothing found, or find-as-you-type self-canceled before reaching a
        // far match. Still kick the full-document count: it does its own
        // complete scan, so the n/m counter and the results list reflect every
        // match even when the incremental find gave up. (Runs only now that the
        // find thread has exited, so the two never scan the engine at once.)
        ClearSearchResult(win);
        ftd->HideUI(false, !wasModifiedCanceled);
        UpdateMatchCount(win, ftd->text);
    }
    win->findThread = nullptr;
}

static void UpdateSearchProgress(FindThreadData* ftd, ProgressUpdateData* data) {
    if (data->wasCancelled) {
        bool wasCancelled = ftd->WasCanceled();
        *data->wasCancelled = wasCancelled;
        return;
    }
    ftd->UpdateProgress(data->current, data->total);
}

static void FindThread(FindThreadData* ftd) {
    ReportIf(!(ftd && ftd->win && ftd->win->ctrl && ftd->win->ctrl->AsFixed()));

    MainWindow* win = ftd->win;
    DisplayModel* dm = win->AsFixed();
    auto textSearch = dm->textSearch;
    auto ctrl = win->ctrl;

    auto engine = dm->GetEngine();
    engine->AddRef();
    AutoCall releaseEngine(SafeEngineRelease<EngineBase>, &engine);

    TextSel* rect;
    textSearch->progressCb = MkFunc1<FindThreadData, ProgressUpdateData*>(UpdateSearchProgress, ftd);
    textSearch->SetDirection(ftd->direction);
    if (ftd->wasModified || !ctrl->ValidPageNo(textSearch->GetCurrentPageNo()) ||
        !dm->GetPageInfo(textSearch->GetCurrentPageNo())->visibleRatio) {
        rect = textSearch->FindFirst(ctrl->CurrentPageNo(), ftd->text);
    } else {
        rect = textSearch->FindNext();
    }

    bool loopedAround = false;
    if (!win->findCancelled && !rect) {
        // With no further findings, start over (unless this was a new search from the beginning)
        int startPage = (TextSearch::Direction::Forward == ftd->direction) ? 1 : ctrl->PageCount();
        if (!ftd->wasModified || ctrl->CurrentPageNo() != startPage) {
            loopedAround = true;
            rect = textSearch->FindFirst(startPage, ftd->text);
        }
    }

    // wait for FindTextOnThread to return so that
    // FindEndTask closes the correct handle to
    // the current find thread
    while (!win->findThread) {
        Sleep(1);
    }

    auto data = new FindEndTaskData;
    data->win = win;
    data->ftd = ftd;
    data->textSel = nullptr;
    data->loopedAround = false;

    if (!win->findCancelled && rect) {
        data->textSel = rect;
        data->wasModifiedCanceled = ftd->wasModified;
        data->loopedAround = loopedAround;
    } else {
        data->wasModifiedCanceled = win->findCancelled;
    }
    auto fn = MkFunc0<FindEndTaskData>(FindEndTask, data);
    uitask::Post(fn, "TaskFindEnd");
    DestroyTempArena();
}

// returns true if did abort a thread or hidden the notification
bool AbortFinding(MainWindow* win, bool hideMessage) {
    bool res = false;
    // cancel any pending debounced find-as-you-type search
    if (win->findDebouncePending) {
        win->findDebouncePending = false;
        if (win->hwndFrame) {
            KillTimer(win->hwndFrame, kFindDebounceTimerId);
        }
    }
    AbortCount(win);
    if (win->findThread) {
        res = true;
        logf("AboftFinding: setting win->findCancelled to true\n");
        win->findCancelled = true;
        WaitForSingleObject(win->findThread, INFINITE);
        win->findThread = nullptr;
    }
    win->findCancelled = false;

    if (hideMessage) {
        bool didRemove = RemoveNotificationsForGroup(win->hwndCanvas, kNotifFindProgress);
        if (didRemove) {
            res = true;
        }
    }
    return res;
}

// wasModified
//   if true, starting a search for new term
//   if false, searching for the next occurrence of previous term
// TODO: should detect wasModified by comparing with the last search result
void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, Str text, bool wasModified, bool showProgress) {
    AbortFinding(win, false);
    if (len(text) == 0) {
        return;
    }
    FindThreadData* ftd = new FindThreadData(win, direction, text, wasModified);
    ftd->ShowUI(showProgress);
    win->findThread = nullptr;
    auto fn = MkFunc0(FindThread, ftd);
    win->findThread = StartThread(fn, "FindThread");
    ftd->thread = win->findThread; // safe because only accesssed on ui thread
}

// TODO: for https://github.com/sumatrapdfreader/sumatrapdf/issues/2655
TempStr ReverseTextTemp(Str s) {
    TempWStr ws = ToWStrTemp(s);
    int n = len(ws);
    for (int i = 0; i < n / 2; i++) {
        WCHAR c1 = ws.s[i];
        WCHAR c2 = ws.s[n - 1 - i];
        ws.s[i] = c2;
        ws.s[n - 1 - i] = c1;
    }
    return ToUtf8Temp(ws);
}

void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, bool showProgress) {
    TempStr s = HwndGetTextTemp(win->hwndFindEdit);
    // if document is rtl, need to reverse the text
    // s = ReverseTextTemp(s);
    bool wasModified = Edit_GetModify(win->hwndFindEdit);
    if (!wasModified) {
        // check if the find text differs from the current tab's cached search text
        // this happens when switching tabs: the find edit box shows the current text
        // but the per-tab textSearch still has the old search text cached
        DisplayModel* dm = win->AsFixed();
        if (dm && dm->textSearch) {
            // compare with lastText, not findText: SetText strips a trailing
            // space (match word end) from findText but keeps it in lastText, and
            // strips a leading space (match word start) from both. Normalize s
            // the same way (drop one leading space) so trailing/leading/whole-word
            // searches don't always look "modified" and find-next can advance.
            Str searchText = s;
            if (searchText && searchText.s[0] == ' ') {
                searchText = Str(searchText.s + 1, searchText.len - 1);
            }
            if (!str::Eq(searchText, dm->textSearch->lastText)) {
                wasModified = true;
            }
        }
    }
    Edit_SetModify(win->hwndFindEdit, FALSE);
    FindTextOnThread(win, direction, s, wasModified, showProgress);
}

static bool FindMatchTouchesVisiblePages(const FindMatch& fm, int firstPage, int lastPage) {
    return fm.endPage >= firstPage && fm.startPage <= lastPage;
}

static void GetVisiblePageRange(DisplayModel* dm, int& firstOut, int& lastOut) {
    firstOut = dm->FirstVisiblePageNo();
    lastOut = firstOut;
    if (!dm->ValidPageNo(firstOut)) {
        firstOut = lastOut = 0;
        return;
    }
    int pageCount = dm->PageCount();
    for (int pageNo = pageCount; pageNo >= firstOut; pageNo--) {
        if (dm->PageVisible(pageNo)) {
            lastOut = pageNo;
            break;
        }
    }
}

static void AppendMatchPageRects(EngineBase* engine, const FindMatch& fm, Vec<FindMatchPaintPageRect>& out) {
    TextSelection ts(engine);
    ts.StartAt(fm.startPage, fm.startGlyph);
    ts.SelectUpTo(fm.endPage, fm.endGlyph);
    for (int i = 0; i < ts.result.len; i++) {
        FindMatchPaintPageRect pr;
        pr.pageNo = ts.result.pages[i];
        pr.rect = ts.result.rects[i];
        out.Append(pr);
    }
}

static void AppendPageRectsToScreen(DisplayModel* dm, const Rect& clipRc, const FindMatchPaintPageRect* pageRects,
                                    int nRects, Vec<Rect>& out) {
    for (int i = 0; i < nRects; i++) {
        const FindMatchPaintPageRect& pr = pageRects[i];
        if (!dm->ValidPageNo(pr.pageNo) || !dm->PageVisible(pr.pageNo)) {
            continue;
        }
        Rect rc = dm->CvtToScreen(pr.pageNo, ToRectF(pr.rect));
        rc = rc.Intersect(clipRc);
        if (!rc.IsEmpty()) {
            out.Append(rc);
        }
    }
}

static void RebuildFindMatchPaintCache(MainWindow* win, DisplayModel* dm, int firstPage, int lastPage) {
    FreeFindMatchPaintCacheEntries();
    gFindMatchPaintCache.firstPage = firstPage;
    gFindMatchPaintCache.lastPage = lastPage;
    gFindMatchPaintCache.countEpoch = win->findCountEpoch;

    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return;
    }
    Vec<FindMatchPaintPageRect>& positions = gFindMatchPaintCache.positions;
    for (int i = 0; i < len(win->findMatches); i++) {
        const FindMatch& fm = win->findMatches[i];
        if (!FindMatchTouchesVisiblePages(fm, firstPage, lastPage)) {
            continue;
        }
        int firstPos = len(positions);
        AppendMatchPageRects(engine, fm, positions);
        int n = positions.len - firstPos;
        if (n == 0) {
            continue;
        }
        FindMatchPaintRects entry;
        entry.key = MatchKey(fm.startPage, fm.startGlyph);
        entry.firstPos = firstPos;
        entry.len = n;
        gFindMatchPaintCache.entries.Append(entry);
    }
}

static void AppendTextSelScreenRects(DisplayModel* dm, const Rect& clipRc, TextSel* sel, Vec<Rect>& out) {
    if (!sel || sel->len == 0 || !sel->pages || !sel->rects) {
        return;
    }
    for (int i = 0; i < sel->len; i++) {
        int pageNo = sel->pages[i];
        if (!dm->PageVisible(pageNo)) {
            continue;
        }
        Rect rc = dm->CvtToScreen(pageNo, ToRectF(sel->rects[i]));
        rc = rc.Intersect(clipRc);
        if (!rc.IsEmpty()) {
            out.Append(rc);
        }
    }
}

void PaintAllFindMatches(MainWindow* win, HDC hdc) {
    if (!gShowAllMatches || !win->IsDocLoaded() || !win->AsFixed()) {
        return;
    }
    if (!IsFindUIVisible(win)) {
        return;
    }
    if (!win->hwndFindEdit || HwndGetTextLen(win->hwndFindEdit) == 0) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    TextSearch* ts = dm->textSearch;
    if (!win->findCountValid && len(win->findMatches) == 0) {
        // count still running: at least highlight the current match
        if (!ts || ts->result.len == 0) {
            return;
        }
        ParsedColor* parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.selectionColor);
        u8 alpha = GetAlpha(parsedCol->col);
        if (alpha == 0) {
            alpha = kSelectionDefaultAlpha;
        }
        Vec<Rect> currentRects;
        AppendTextSelScreenRects(dm, win->canvasRc, &ts->result, currentRects);
        if (len(currentRects) > 0) {
            PaintTransparentRectangles(hdc, win->canvasRc, currentRects, parsedCol->col, alpha);
        }
        return;
    }
    if (len(win->findMatches) == 0) {
        return;
    }
    int firstPage = 0;
    int lastPage = 0;
    GetVisiblePageRange(dm, firstPage, lastPage);
    if (!dm->ValidPageNo(firstPage)) {
        return;
    }

    if (gFindMatchPaintCache.countEpoch != win->findCountEpoch || gFindMatchPaintCache.firstPage != firstPage ||
        gFindMatchPaintCache.lastPage != lastPage) {
        RebuildFindMatchPaintCache(win, dm, firstPage, lastPage);
    }

    u64 currentKey = 0;
    if (ts && ts->result.len > 0) {
        currentKey = MatchKey(ts->startPage, ts->startGlyph);
    }

    ParsedColor* parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.selectionColor);
    u8 alpha = GetAlpha(parsedCol->col);
    if (alpha == 0) {
        alpha = kSelectionDefaultAlpha;
    }

    Vec<Rect> otherRects;
    Vec<Rect> currentRects;
    Vec<FindMatchPaintPageRect>& positions = gFindMatchPaintCache.positions;
    for (int i = 0; i < len(gFindMatchPaintCache.entries); i++) {
        const FindMatchPaintRects& entry = gFindMatchPaintCache.entries[i];
        Vec<Rect>& out = (entry.key == currentKey) ? currentRects : otherRects;
        AppendPageRectsToScreen(dm, win->canvasRc, &positions[entry.firstPos], entry.len, out);
    }

    if (len(otherRects) > 0) {
        PaintTransparentRectangles(hdc, win->canvasRc, otherRects, kFindOtherMatchColor, alpha);
    }
    if (len(currentRects) == 0 && ts && ts->result.len > 0) {
        AppendTextSelScreenRects(dm, win->canvasRc, &ts->result, currentRects);
    }
    if (len(currentRects) > 0) {
        PaintTransparentRectangles(hdc, win->canvasRc, currentRects, parsedCol->col, alpha);
    }
}

void PaintForwardSearchMark(MainWindow* win, HDC hdc) {
    ReportIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();
    int pageNo = win->fwdSearchMark.page;
    PageInfo* pageInfo = dm->GetPageInfo(pageNo);
    if (!pageInfo || 0.0 == pageInfo->visibleRatio) {
        return;
    }

    int hiLiWidth = gGlobalPrefs->forwardSearch.highlightWidth;
    int hiLiOff = gGlobalPrefs->forwardSearch.highlightOffset;

    // Draw the rectangles highlighting the forward search results
    Vec<Rect> rects;
    for (int i = 0; i < len(win->fwdSearchMark.rects); i++) {
        Rect rect = win->fwdSearchMark.rects[i];
        rect = dm->CvtToScreen(pageNo, ToRectF(rect));
        if (hiLiOff > 0) {
            float zoom = dm->GetZoomReal(pageNo);
            rect.x = std::max(pageInfo->pageOnScreen.x, 0) + (int)(hiLiOff * zoom);
            rect.dx = (int)((hiLiWidth > 0 ? hiLiWidth : 15.0) * zoom);
            rect.y -= 4;
            rect.dy += 8;
        }
        rects.Append(rect);
    }

    u8 alpha = (u8)(0x5f * 1.0f * (HIDE_FWDSRCHMARK_STEPS - win->fwdSearchMark.hideStep) / HIDE_FWDSRCHMARK_STEPS);
    ParsedColor* parsedCol = GetPrefsColor(gGlobalPrefs->forwardSearch.highlightColor);
    PaintTransparentRectangles(hdc, win->canvasRc, rects, parsedCol->col, alpha);
}

// Replace in 'pattern' the macros %f %l %c by 'path', 'line' and 'col'
static TempStr BuildOpenFileCmdTemp(Str pattern, Str path, int line, int col) {
    str::Builder cmdline(256);

    logf("BuildOpenFileCmdTemp: path: '%s', pattern: '%s'\n", path, pattern);
    Str s = pattern;
    while (s) {
        int percIdx = str::IndexOfChar(s, '%');
        if (percIdx < 0) {
            cmdline.Append(s);
            break;
        }
        cmdline.Append(Str(s.s, percIdx));
        if (percIdx + 1 >= s.len) {
            cmdline.Append(Str(s.s + percIdx, s.len - percIdx));
            break;
        }
        char spec = s.s[percIdx + 1];
        if (spec == 'f') {
            cmdline.Append(path);
        } else if (spec == 'l') {
            cmdline.Append(fmt("%d", line));
        } else if (spec == 'c') {
            cmdline.Append(fmt("%d", col));
        } else if (spec == '%') {
            cmdline.AppendChar('%');
        } else {
            cmdline.Append(Str(s.s + percIdx, 2));
        }
        s = Str(s.s + percIdx + 2, s.len - percIdx - 2);
    }

    return ToStrTemp(cmdline);
}

// returns true if inverse search was performed
bool OnInverseSearch(MainWindow* win, int x, int y) {
    if (!CanAccessDisk() || gPluginMode) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || tab->GetEngineType() != kindEngineMupdf) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();

    // Clear the last forward-search result
    win->fwdSearchMark.rects.Reset();
    InvalidateRect(win->hwndCanvas, nullptr, FALSE);

    // On double-clicking error message will be shown to the user
    // if the PDF does not have a synchronization file
    if (!dm->pdfSync) {
        Str path = tab->filePath;
        int err = Synchronizer::Create(path, dm->GetEngine(), &dm->pdfSync);
        if (err == PDFSYNCERR_SYNCFILE_NOTFOUND) {
            // We used to warn that "No synchronization file found" at this
            // point if gGlobalPrefs->enableTeXEnhancements is set; we no longer
            // so do because a double-click has several other meanings
            // (selecting a word or an image, navigating quickly using links)
            // and showing an unrelated warning in all those cases seems wrong
            return false;
        }
        if (err != PDFSYNCERR_SUCCESS) {
            NotificationCreateArgs args;
            args.hwndParent = win->hwndCanvas;
            args.msg = _TRA("Synchronization file cannot be opened");
            ShowNotification(args);
            return true;
        }
    }

    int pageNo = dm->GetPageNoByPoint(Point(x, y));
    if (!tab->ctrl->ValidPageNo(pageNo)) {
        return false;
    }

    Point pt = ToPoint(dm->CvtFromScreen(Point(x, y), pageNo));
    Str srcfilepath;
    int line = 0;
    int col = 0;
    int err = dm->pdfSync->DocToSource(pageNo, pt, srcfilepath, &line, &col);
    if (err != PDFSYNCERR_SUCCESS) {
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.msg = _TRA("No synchronization info at this position");
        ShowNotification(args);
        return true;
    }

    Str inverseSearch = gGlobalPrefs->inverseSearchCmdLine;
    if (!inverseSearch) {
        Vec<TextEditor*> editors;
        DetectTextEditors(editors);
        if (len(editors) > 0) {
            inverseSearch = str::DupTemp(editors[0]->openFileCmd);
        }
    }

    Str cmdLine;
    if (inverseSearch) {
        cmdLine = BuildOpenFileCmdTemp(inverseSearch, srcfilepath, line, col);
    }
    str::Free(srcfilepath);

    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = _TRA("Cannot start inverse search command. Please check the command line in the settings.");
    if (len(cmdLine) > 0) {
        // resolve relative paths with relation to SumatraPDF.exe's directory
        TempStr appDir = GetSelfExeDirTemp();
        AutoCloseHandle process(LaunchProcessInDir(cmdLine, appDir));
        if (!process) {
            ShowNotification(args);
        }
    } else if (gGlobalPrefs->enableTeXEnhancements) {
        ShowNotification(args);
    }

    return true;
}

// Show the result of a PDF forward-search synchronization (initiated by a DDE command)
void ShowForwardSearchResult(MainWindow* win, Str fileName, int line, int /* col */, int ret, int page,
                             Vec<Rect>& rects) {
    ReportIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();
    win->fwdSearchMark.rects.Reset();
    const PageInfo* pi = dm->GetPageInfo(page);
    if ((ret == PDFSYNCERR_SUCCESS) && (len(rects) > 0) && (nullptr != pi)) {
        // remember the position of the search result for drawing the rect later on
        win->fwdSearchMark.rects = rects;
        win->fwdSearchMark.page = page;
        win->fwdSearchMark.show = true;
        win->fwdSearchMark.hideStep = 0;
        if (!gGlobalPrefs->forwardSearch.highlightPermanent) {
            SetTimer(win->hwndCanvas, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DELAY_IN_MS, nullptr);
        }

        // Scroll to show the overall highlighted zone
        int pageNo = page;
        Rect overallrc = rects[0];
        for (int i = 1; i < len(rects); i++) {
            overallrc = overallrc.Union(rects[i]);
        }
        TextSel res = {1, 1, &pageNo, &overallrc};
        if (!dm->PageVisible(page)) {
            win->ctrl->GoToPage(page, true);
        }
        if (!dm->ShowResultRectToScreen(&res)) {
            ScheduleRepaint(win, 0);
        }
        if (IsIconic(win->hwndFrame)) {
            ShowWindowAsync(win->hwndFrame, SW_RESTORE);
        }
        return;
    }

    TempStr buf = nullptr;
    NotificationCreateArgs args{};
    args.hwndParent = win->hwndCanvas;
    if (ret == PDFSYNCERR_SYNCFILE_NOTFOUND) {
        args.msg = _TRA("No synchronization file found");
    } else if (ret == PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED) {
        args.msg = _TRA("Synchronization file cannot be opened");
    } else if (ret == PDFSYNCERR_INVALID_PAGE_NUMBER) {
        buf = fmt(_TRA("Page number %u nonexistent").s, page);
    } else if (ret == PDFSYNCERR_NO_SYNC_AT_LOCATION) {
        args.msg = _TRA("No synchronization info at this position");
    } else if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE) {
        buf = fmt(_TRA("Unknown source file (%s)").s, fileName);
    } else if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE) {
        buf = fmt(_TRA("Source file %s has no synchronization point").s, fileName);
    } else if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE) {
        buf = fmt(_TRA("No result found around line %u in file %s").s, line, fileName);
    } else if (ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD) {
        buf = fmt(_TRA("No result found around line %u in file %s").s, line, fileName);
    }
    if (buf) {
        args.msg = buf;
        ShowNotification(args);
    }
}

// DDE commands handling

/*
Forward search (synchronization) DDE command

[ForwardSearch(["<pdffilepath>",]"<sourcefilepath>",<line>,<column>[,<newwindow>, <setfocus>])]
eg:
[ForwardSearch("c:\file.pdf","c:\folder\source.tex",298,0)]

if pdffilepath is provided, the file will be opened if no open window can be found for it
if newwindow = 1 then a new window is created even if the file is already open
if focus = 1 then the focus is set to the window
*/
static Str HandleSyncCmd(Str cmd, bool* ack) {
    TempStr pdfFile, srcFile;
    BOOL line = 0, col = 0, newWindow = 0, setFocus = 0;
    Str next = str::Parse(cmd, "[ForwardSearch(\"%s\",%? \"%s\",%u,%u)]", &pdfFile, &srcFile, &line, &col);
    if (str::IsNull(next)) {
        next = str::Parse(cmd, "[ForwardSearch(\"%s\",%? \"%s\",%u,%u,%u,%u)]", &pdfFile, &srcFile, &line, &col,
                          &newWindow, &setFocus);
    }
    // allow to omit the pdffile path, so that editors don't have to know about
    // multi-file projects (requires that the PDF has already been opened)
    if (str::IsNull(next)) {
        pdfFile = {};
        next = str::Parse(cmd, "[ForwardSearch(\"%s\",%u,%u)]", &srcFile, &line, &col);
        if (str::IsNull(next)) {
            next = str::Parse(cmd, "[ForwardSearch(\"%s\",%u,%u,%u,%u)]", &srcFile, &line, &col, &newWindow, &setFocus);
        }
    }

    if (str::IsNull(next)) {
        return {};
    }

    MainWindow* win = nullptr;
    if (pdfFile) {
        // check if the PDF is already opened
        win = FindMainWindowByFile(pdfFile, !newWindow);
        // if not then open it
        if (newWindow || !win) {
            LoadArgs args(pdfFile, !newWindow ? win : nullptr);
            win = LoadDocument(&args);
        } else if (!win->IsDocLoaded()) {
            ReloadDocument(win, false);
        }
    } else {
        // check if any opened PDF has sync information for the source file
        win = FindMainWindowBySyncFile(srcFile, true);
        if (win && newWindow) {
            LoadArgs args(win->CurrentTab()->filePath, nullptr);
            win = LoadDocument(&args);
        }
    }

    if (!win || !win->CurrentTab() || win->CurrentTab()->GetEngineType() != kindEngineMupdf) {
        return next;
    }

    DisplayModel* dm = win->AsFixed();
    if (!dm->pdfSync) {
        return next;
    }

    int page;
    Vec<Rect> rects;
    int ret = dm->pdfSync->SourceToDoc(srcFile, line, col, &page, rects);
    ShowForwardSearchResult(win, srcFile, line, col, ret, page, rects);
    if (setFocus) {
        win->Focus();
    }

    *ack = true;
    return next;
}

/*
Search DDE command

[Search("<pdffile>","<search-term>")]
*/
static Str HandleSearchCmd(Str cmd, bool* ack) {
    TempStr pdfFile;
    TempStr term;
    Str next = str::Parse(cmd, "[Search(\"%s\",\"%s\")]", &pdfFile, &term);
    // TODO: should un-quote text to allow searching text with '"' in them
    if (str::IsNull(next)) {
        return {};
    }
    if (len(term) == 0) {
        return next;
    }
    // check if the PDF is already opened
    // TODO: prioritize window with HWND so that if we have the same file
    // opened in multiple tabs / windows, we operate on the one that got the message
    MainWindow* win = FindMainWindowByFile(pdfFile, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win, false);
        if (!win->IsDocLoaded()) {
            return next;
        }
    }
    bool wasModified = true;
    bool showProgress = true;
    FindTextOnThread(win, TextSearch::Direction::Forward, term, wasModified, showProgress);
    win->Focus();
    *ack = true;
    return next;
}

/*
Go to a page and select the search term, but only if it's found on that page
(unlike Search, which keeps searching following pages and wraps around).

[GotoPageWord("<pdffile>",<page>,"<search-term>")]
*/
static Str HandleGotoPageWordCmd(Str cmd, bool* ack) {
    TempStr pdfFile;
    TempStr term;
    int page = 0;
    Str next = str::Parse(cmd, "[GotoPageWord(\"%s\",%d,\"%s\")]", &pdfFile, &page, &term);
    if (str::IsNull(next)) {
        return {};
    }
    MainWindow* win = FindMainWindowByFile(pdfFile, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win, false);
        if (!win->IsDocLoaded()) {
            return next;
        }
    }
    *ack = true;
    DisplayModel* dm = win->AsFixed();
    if (!dm || !win->ctrl->ValidPageNo(page)) {
        return next;
    }
    // stop any running async search, then go to the page
    AbortFinding(win, true);
    win->ctrl->GoToPage(page, true);
    if (len(term) > 0) {
        dm->textSearch->SetDirection(TextSearch::Direction::Forward);
        TextSel* sel = dm->textSearch->FindFirstOnPage(page, term);
        if (sel && sel->len > 0) {
            ShowSearchResult(win, sel, false);
        } else {
            // term not on this page: stay on the page, select nothing
            ClearSearchResult(win);
        }
    }
    win->Focus();
    return next;
}

/*
Open file DDE Command

[Open("<pdffilepath>"[,<newWindow>,<setFocus>,<forceRefresh>,<inCurrentTab>])]
    newWindow, setFocus, forceRefresh, inCurrentTab are flags that can be 0 or 1 (set)
if the flag is set to 1:
    newWindow    : new window is created even if the file is already open
    setFocus     : focus is set to the window
    forceRefresh : reloads document
    inCurrentTab : replaces document in current tab (if 0 loads in a new tab)
                   if newWindow != 0 => ignored
valid formats:
    [Open("c:\file.pdf")]
    [Open("c:\file.pdf",1,1,0)]
    [Open("c:\file.pdf",1,1,0,1)]
*/
static Str HandleOpenCmd(Str cmd, bool* ack) {
    TempStr filePath;
    int newWindow = 0;
    int setFocus = 0;
    int forceRefresh = 0;
    int inCurrentTab = 0;
    Str next = str::Parse(cmd, "[Open(\"%s\")]", &filePath);
    if (str::IsNull(next)) {
        next = str::Parse(cmd, "[Open(\"%s\",%u,%u,%u,%u)]", &filePath, &newWindow, &setFocus, &forceRefresh,
                          &inCurrentTab);
    }
    if (str::IsNull(next)) {
        next = str::Parse(cmd, "[Open(\"%s\",%u,%u,%u)]", &filePath, &newWindow, &setFocus, &forceRefresh);
    }
    if (str::IsNull(next)) {
        return {};
    }
    bool isCtrl = IsCtrlPressed();
    logf("HandleOpenCmd: '%s', newWindow: %d, setFocus: %d, forceRefresh: %d, inCurrentTab: %d, isCtrl: %d\n", filePath,
         newWindow, setFocus, forceRefresh, inCurrentTab, isCtrl);
    // on startup this is called while LoadDocument is in progress, which causes
    // all sort of mayhem. Queue files to be loaded in a sequence
    if (gIsStartup) {
        logf("HandleOpenCmd: gIsStartup, appending to gDdeOpenOnStartup\n");
        gDdeOpenOnStartup.Append(filePath);
        return next;
    }

    if (newWindow != 0 && inCurrentTab != 0) {
        inCurrentTab = 0;
        logf("HandleOpenCmd: setting inCurrentTab to 0 because newWindow != 0\n");
    }

    bool focusTab = (newWindow == 0);

    // intelligently pick a window or create one
    MainWindow* win = nullptr;
    MainWindow* emptyExistingWin = nullptr;
    auto nWindows = len(gWindows);
    for (auto& w : gWindows) {
        if (!w->HasDocsLoaded()) {
            emptyExistingWin = w;
            logf("HandleOpenCmd: found empty existing window\n");
            break;
        }
    }
    if (newWindow > 0) {
        if (emptyExistingWin) {
            // instead of opening new window, re-use exisitng open window
            win = emptyExistingWin;
            logf("HandleOpenCmd: newWindow > 0, using empty existing window\n");
        } else {
            win = CreateAndShowMainWindow(nullptr);
            logf("HandleOpenCmd: newWindow > 0, created new window\n");
        }
    }
    bool doLoad = true;
    if (!win) {
        win = FindMainWindowByFile(filePath, focusTab);
        if (win) {
            logf("HandleOpenCmd: found existing window with file '%s'\n", filePath);
            doLoad = false;
            if (!win->IsDocLoaded()) {
                ReloadDocument(win, false);
                forceRefresh = 0;
                logf("HandleOpenCmd: existing tab was not loaded, so reloaded, set forceRefresh = 0\n");
            }
        }
    }
    if (!win) {
        if (nWindows == 1) {
            // of only one window, use that one
            win = gWindows[0];
            logf("HandleOpenCmd: using the only window\n");
        }
        if (!win) {
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/2315
            // open in the last active window
            win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
            if (win) {
                logf("HandleOpenCmd: found last active window\n");
            } else {
                logf("HandleOpenCmd: didn't find last active window\n");
            }
        }
        if (!win && nWindows > 0) {
            // if can't find active, using the first
            win = gWindows[0];
            logf("HandleOpenCmd: first window\n");
        }
    }

    if (doLoad) {
        LoadArgs args(filePath, win);
        args.activateExisting = !isCtrl;
        if (newWindow) {
            args.activateExisting = false;
        }
        if (inCurrentTab) {
            args.forceReuse = true;
        }
        logf("HandleOpenCmd: calling LoadDocument(), activateExisting: %d, forceReuse: %d\n",
             (int)args.activateExisting, (int)args.forceReuse);
        win = LoadDocument(&args);
        if (!win) {
            logf("HandleOpenCmd: LoadDocument() for '%s' failed\n", filePath);
        }
    }

    // TODO: not sure why this triggers. Seems to happen when opening multiple files
    // via Open menu in explorer. The first one is opened via cmd-line arg, the
    // rest via DDE.
    // ReportIf(win && win->IsAboutWindow());
    if (win) {
        if (forceRefresh) {
            logf("HandleOpenCmd: forceRefresh != 0 so calling ReloadDocument()\n");
            ReloadDocument(win, true);
        }
        if (setFocus) {
            logf("HandleOpenCmd: setFocus != 0 so calling win->Focus()\n");
            win->Focus();
        }
    }

    *ack = true;
    return next;
}

/*
DDE command: jump to named destination in an already opened document.

[GoToNamedDest("<pdffilepath>","<destination name>")]
e.g.:
[GoToNamedDest("c:\file.pdf", "chapter.1")]
*/
static Str HandleGotoCmd(Str cmd, bool* ack) {
    TempStr pdfFile, destName;
    Str next = str::Parse(cmd, "[GotoNamedDest(\"%s\",%? \"%s\")]", &pdfFile, &destName);
    if (str::IsNull(next)) {
        return {};
    }

    MainWindow* win = FindMainWindowByFile(pdfFile, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win, false);
        if (!win->IsDocLoaded()) {
            return next;
        }
    }

    win->linkHandler->GotoNamedDest(destName);
    win->Focus();
    *ack = true;
    return next;
}

/*
DDE command: jump to a page in an already opened document.

[GoToPage("<pdffilepath>",<page number>)]

eg: [GoToPage("c:\file.pdf",37)]
*/
static Str HandlePageCmd(HWND, Str cmd, bool* ack) {
    TempStr pdfFile;
    uint page = 0;
    Str next = str::Parse(cmd, "[GotoPage(\"%S\",%u)]", &pdfFile, &page);
    if (str::IsNull(next)) {
        return {};
    }

    // check if the PDF is already opened
    // TODO: prioritize window with HWND so that if we have the same file
    // opened in multiple tabs / windows, we operate on the one that got the message
    MainWindow* win = FindMainWindowByFile(pdfFile, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win, false);
        if (!win->IsDocLoaded()) {
            return next;
        }
    }

    if (!win->ctrl->ValidPageNo(page)) {
        return next;
    }

    win->ctrl->GoToPage(page, true);
    *ack = true;
    win->Focus();
    return next;
}

/*
Set view mode and zoom level DDE command

[SetView("<filepath>", "<view mode>", <zoom level>[, <scrollX>, <scrollY>])]

eg: [SetView("c:\file.pdf", "book view", -2)]

use -1 for kZoomFitPage, -2 for kZoomFitWidth and -3 for kZoomFitContent
*/
static Str HandleSetViewCmd(Str cmd, bool* ack) {
    TempStr filePath, viewMode;
    float zoom = kInvalidZoom;
    Point scroll(-1, -1);
    Str next = str::Parse(cmd, "[SetView(\"%s\",%? \"%s\",%f)]", &filePath, &viewMode, &zoom);
    if (str::IsNull(next)) {
        next =
            str::Parse(cmd, "[SetView(\"%s\",%? \"%s\",%f,%d,%d)]", &filePath, &viewMode, &zoom, &scroll.x, &scroll.y);
    }
    if (str::IsNull(next)) {
        return {};
    }

    MainWindow* win = FindMainWindowByFile(filePath, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win, false);
        if (!win->IsDocLoaded()) {
            return next;
        }
    }

    DisplayMode mode = DisplayModeFromString(viewMode, DisplayMode::Automatic);
    if (mode != DisplayMode::Automatic) {
        SwitchToDisplayMode(win, mode);
    }

    // a zoom of 0 means "keep the current zoom". Re-applying a fit zoom (-1/-2/-3)
    // on every call re-fits the page and resets the scroll position, which made
    // scrolling via the scroll arguments jump to the next page (issue #5068). Use
    // zoom 0 to scroll without changing the zoom.
    if (zoom != kInvalidZoom && zoom != 0) {
        SmartZoom(win, zoom, nullptr, false);
    }

    if ((scroll.x != -1 || scroll.y != -1) && win->AsFixed()) {
        DisplayModel* dm = win->AsFixed();
        ScrollState ss = dm->GetScrollState();
        ss.x = scroll.x;
        ss.y = scroll.y;
        dm->SetScrollState(ss);
    }
    *ack = true;
    return next;
}

/*
Open new window.

[NewWindow]
*/
static Str HandleNewWindowCmd(Str cmd, bool* ack) {
    Str kNewWindowCmd = "[NewWindow]";
    if (!str::StartsWith(cmd, kNewWindowCmd)) {
        return {};
    }
    logf("HandleNewWindowCmd\n");
    Str next = Str(cmd.s + kNewWindowCmd.len, cmd.len - kNewWindowCmd.len);
    CreateAndShowMainWindow(nullptr);
    *ack = true;
    return next;
}

/*
[GetFileState("<filepath>")]
[GetFileState()]
[GetFileState]
Return info about document <filepath> or currently viewed document if no
<filepath> given.
Returns info in the format:

path: c:\file.pdf
zoom: 120
view: continuous
sumver: 3.7

zoom is a percentage, or -1 = fit page, -2 = fit width, -3 = fit content
(the same convention as the SetView command).
i.e. multiple lines, each line is
key: value
This should make parsing easy:
* split by `\n' to get the lines
* split each line by ':' to get key and value

Returns:
error: <error message>
if file doesn't exist or no opened file
*/
static Str HandleGetFileStateCmd(HWND hwnd, Str cmd, bool* ack, str::Builder& res) {
    TempStr filePath;
    Str next = str::Parse(cmd, "[GetFileState(\"%s\")]", &filePath);
    if (str::IsNull(next)) {
        next = str::Parse(cmd, "[GetFileState()]");
    }
    if (str::IsNull(next)) {
        next = str::Parse(cmd, "[GetFileState]");
    }
    if (str::IsNull(next)) {
        return {};
    }

    // we recognized the command, so from here on we always produce a response
    *ack = true;

    MainWindow* win = nullptr;
    if (len(filePath) > 0) {
        win = FindMainWindowByFile(filePath, true);
    } else {
        // no path given: report the currently active document
        win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
        if (!win && len(gWindows) > 0) {
            win = gWindows[0];
        }
    }
    if (!win) {
        res.Append("error: no opened file");
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win, false);
        if (!win->IsDocLoaded()) {
            res.Append("error: file not loaded");
            return next;
        }
    }

    DocController* ctrl = win->ctrl;
    Str docPath = ctrl->GetFilePath();
    // zoom uses the same convention as SetView: a percentage, or -1 = fit page,
    // -2 = fit width, -3 = fit content
    float zoom = ctrl->GetZoomVirtual();
    Str view = DisplayModeToString(ctrl->GetDisplayMode());
    res.Append(fmt("path: %s\n", docPath));
    res.Append(fmt("page: %d\n", ctrl->CurrentPageNo()));
    res.Append(fmt("pageCount: %d\n", ctrl->PageCount()));
    res.Append(fmt("zoom: %g\n", zoom));
    res.Append(fmt("view: %s\n", view));
    res.Append(fmt("sumver: %s\n", StrL(CURR_VERSION_STRA)));
    return next;
}

// returns the full path of every open document, one per line (issue #5060)
static Str HandleGetOpenFilesCmd(Str cmd, bool* ack, str::Builder& res) {
    Str next = str::Parse(cmd, "[GetOpenFiles()]");
    if (str::IsNull(next)) {
        next = str::Parse(cmd, "[GetOpenFiles]");
    }
    if (str::IsNull(next)) {
        return {};
    }
    *ack = true;
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (len(tab->filePath) > 0) {
                res.Append(fmt("%s\n", tab->filePath));
            }
        }
    }
    return next;
}

// returns the document position currently under the mouse cursor, in PDF points
// -- the same unit as the "pt" cursor-position notification and .smx files
// (issue #1411). page is 0 if the cursor isn't over a page.
static Str HandleGetMousePosCmd(Str cmd, bool* ack, str::Builder& res) {
    Str next = str::Parse(cmd, "[GetMousePos()]");
    if (str::IsNull(next)) {
        next = str::Parse(cmd, "[GetMousePos]");
    }
    if (str::IsNull(next)) {
        return {};
    }
    *ack = true;
    MainWindow* win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        res.Append("error: no document\n");
        return next;
    }
    Point pos = HwndGetCursorPos(win->hwndCanvas);
    int pageNo = dm->GetPageNoByPoint(pos);
    bool validPage = dm->ValidPageNo(pageNo);
    PointF pt = dm->CvtFromScreen(pos);
    // match FormatCursorPositionTemp's "pt" computation exactly
    EngineBase* engine = dm->GetEngine();
    float dpi = engine->GetFileDPI();
    float x = pt.x < 0 ? 0 : pt.x;
    float y = pt.y < 0 ? 0 : pt.y;
    double xPt = (double)x / dpi * 72.0;
    double yPt = (double)y / dpi * 72.0;
    res.Append(fmt("page: %d\n", validPage ? pageNo : 0));
    res.Append(fmt("x: %.2f\n", xPt));
    res.Append(fmt("y: %.2f\n", yPt)); // MuPDF convention: origin top-left, y down
    if (validPage) {
        // also provide PDF/Adobe coordinates: origin bottom-left, y up (#1411)
        double pageHeightPt = (double)engine->PageMediabox(pageNo).dy / dpi * 72.0;
        res.Append(fmt("ypdf: %.2f\n", pageHeightPt - yPt));
    }
    return next;
}

/*
Handle all commands as defined in Commands.h
eg: [CmdClose] or [CmdCreateAnnotHighlight #00ff00 openEdit]
*/
static Str HandleCmdCommand(HWND hwnd, Str cmd, bool* ack) {
    TempStr cmdContent;
    Str next = str::Parse(cmd, "[%s]", &cmdContent);
    if (str::IsNull(next)) {
        return {};
    }
    // cmdContent is the full content between [ and ]
    // it might be just "CmdClose" or "CmdCreateAnnotHighlight #00ff00 openEdit"
    // extract the command name (first space-delimited token)
    Str content = cmdContent;
    int spaceIdx = str::IndexOfChar(content, ' ');
    TempStr name;
    if (spaceIdx >= 0) {
        name = str::DupTemp(Str(content.s, spaceIdx));
    } else {
        name = str::DupTemp(content);
    }

    int cmdId = GetCommandIdByName(name);
    if (cmdId < 0) {
        return {};
    }
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        logfa("HandleCmdCommand: not executing DDE because MainWindow for hwnd 0x%p not found\n", hwnd);
        return {};
    }

    // if there are arguments after the command name, create a custom command with those args
    int idToSend = cmdId;
    if (spaceIdx >= 0) {
        CustomCommand* customCmd = CreateCommandFromDefinition(cmdContent);
        if (customCmd) {
            idToSend = customCmd->id;
        }
    }

    logfa("HandleCmdCommand: sending %d (%s) command\n", idToSend, cmdContent);
    SendMessageW(win->hwndFrame, WM_COMMAND, idToSend, 0);
    *ack = true;
    return next;
}

// returns true if did handle a message
static bool HandleExecuteCmds(HWND hwnd, Str cmd) {
    gMostRecentlyOpenedDoc = nullptr;

    bool didHandle = false;
    while (cmd) {
        {
            logf("HandleExecuteCmds: '%s'\n", cmd);
        }

        Str nextCmd = HandleSyncCmd(cmd, &didHandle);
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleOpenCmd(cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleGotoCmd(cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandlePageCmd(hwnd, cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleSetViewCmd(cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleSearchCmd(cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleGotoPageWordCmd(cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleCmdCommand(hwnd, cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleNewWindowCmd(cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            // forwards compatibility: ignore unknown commands (maybe from newer version)
            TempStr tmp;
            nextCmd = str::Parse(cmd, "%s]", &tmp);
        }
        cmd = nextCmd;
    }
    return didHandle;
}

static bool HandleRequestCmds(HWND hwnd, Str cmd, str::Builder& rsp) {
    bool didHandle = false;
    while (cmd) {
        {
            logf("HandleRequestCmds: '%s'\n", cmd);
        }

        Str nextCmd = HandleGetFileStateCmd(hwnd, cmd, &didHandle, rsp);
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleGetOpenFilesCmd(cmd, &didHandle, rsp);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleGetMousePosCmd(cmd, &didHandle, rsp);
        }
        if (str::IsNull(nextCmd)) {
            TempStr tmp;
            nextCmd = str::Parse(cmd, "%s]", &tmp);
        }
        cmd = nextCmd;
    }
    return didHandle;
}

LRESULT OnDDERequest(HWND hwnd, WPARAM wp, LPARAM lp) {
    // window that is sending us the message
    HWND hwndClient = (HWND)wp;

    UINT fmt = LOWORD(lp);
    switch (fmt) {
        case CF_TEXT:
        case CF_UNICODETEXT:
            // we handle those
            break;
        default:
            logf("OnDDERequest: invalid fmt '%d'\n", (int)fmt);
            return 0;
    }
    ATOM a = HIWORD(lp);
    TempStr cmd = AtomToStrTemp(a);
    if (!cmd) {
        return 0;
    }

    str::Builder str;
    bool didHandle = HandleRequestCmds(hwnd, cmd, str);
    if (!didHandle) {
        str.Reset(StrL("error: unknown command"));
    }

    void* data;
    int cbData;
    if (fmt == CF_TEXT) {
        data = (void*)ToStr(str).s;
        cbData = len(str) + 1;
    } else if (fmt == CF_UNICODETEXT) {
        int cch;
        WCHAR* tmp = CWStrTemp(ToStr(str), cch);
        data = (void*)tmp;
        cbData = (cch + 1) * 2;
    } else {
        ReportIf(true);
        return 0;
    }

    // the payload goes at DDEDATA.Value, i.e. offsetof(DDEDATA, Value) -- NOT
    // sizeof(DDEDATA), whose trailing Value[1] + padding would push it too far
    // and the client would read zeros
    int cbDdeData = (int)offsetof(DDEDATA, Value);
    u8* res = (u8*)AllocZero(GetTempArena(), cbDdeData + cbData);
    DDEDATA* ddeData = (DDEDATA*)res;
    ddeData->fResponse = 1; // this data answers a WM_DDE_REQUEST (not an advise)
    ddeData->fRelease = 1;  // tell client to free HGLOBAL
    ddeData->cfFormat = (short)fmt;
    memcpy(res + cbDdeData, data, cbData);

    HGLOBAL h = MemToHGLOBAL(res, cbDdeData + cbData, GMEM_MOVEABLE | GMEM_DDESHARE);
    // must use PackDDElParam, not MAKELPARAM: on 64-bit MAKELPARAM would
    // truncate the HGLOBAL to 16 bits and the DDE client would dereference a
    // garbage handle (crash in user32's WM_DDE_DATA handling)
    LPARAM lpres = PackDDElParam(WM_DDE_DATA, (UINT_PTR)h, a);
    if (!PostMessageW(hwndClient, WM_DDE_DATA, (WPARAM)hwnd, lpres)) {
        // the client went away: we still own the data and the packed lParam
        GlobalFree(h);
        FreeDDElParam(WM_DDE_DATA, lpres);
    }
    return 0;
}

LRESULT OnDDExecute(HWND hwnd, WPARAM wp, LPARAM lp) {
    HWND hwndClient = (HWND)wp;
    HGLOBAL hCommand = (HGLOBAL)lp;
    bool isUnicode = IsWindowUnicode(hwndClient);

    TempStr cmd = HGLOBALToStrTemp(hCommand, isUnicode);
    bool didHandle = HandleExecuteCmds(hwnd, cmd);
    DDEACK ack{};
    ack.fAck = didHandle ? 1 : 0;
    LPARAM lpres = PackDDElParam(WM_DDE_ACK, *(WORD*)&ack, (UINT_PTR)hCommand);
    PostMessageW(hwndClient, WM_DDE_ACK, (WPARAM)hwnd, lpres);
    return 0;
}

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wp, LPARAM lp) {
    ATOM aServer = GlobalAddAtom(kSumatraDdeServer);
    ATOM aTopic = GlobalAddAtom(kSumatraDdeTopic);

    if (LOWORD(lp) == aServer && HIWORD(lp) == aTopic) {
        SendMessageW((HWND)wp, WM_DDE_ACK, (WPARAM)hwnd, MAKELPARAM(aServer, 0));
    } else {
        GlobalDeleteAtom(aServer);
        GlobalDeleteAtom(aTopic);
    }
    return 0;
}

LRESULT OnDDETerminate(HWND hwnd, WPARAM wp, LPARAM) {
    PostMessageW((HWND)wp, WM_DDE_TERMINATE, (WPARAM)hwnd, 0L);
    return 0;
}

// Payload for async Open command carried in kCopyDataOpen WM_COPYDATA
struct OpenCopyDataAsync {
    Str path; // heap-allocated, freed by OpenCopyDataAsyncRun
    u32 newWindow;
};

static void OpenCopyDataAsyncRun(OpenCopyDataAsync* d) {
    // Pick a target window the same way HandleOpenCmd would, then kick off
    // the load on a worker thread. We stay off the UI thread for the heavy
    // bit so the sender (already returned from SendMessageW by now) never
    // had to wait on us in the first place.
    MainWindow* win = nullptr;
    if (d->newWindow) {
        MainWindow* emptyExistingWin = nullptr;
        for (auto& w : gWindows) {
            if (!w->HasDocsLoaded()) {
                emptyExistingWin = w;
                break;
            }
        }
        win = emptyExistingWin ? emptyExistingWin : CreateAndShowMainWindow(nullptr);
    } else {
        win = FindMainWindowByFile(d->path, true);
        if (!win) {
            win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
        }
        if (!win && len(gWindows) > 0) {
            win = gWindows[0];
        }
    }
    LoadArgs args(d->path, win);
    args.activateExisting = d->newWindow == 0;
    // Match the legacy DDE Open(..., setFocus=1) behavior used by
    // shell/reuseInstance launches: opening into an existing instance should
    // bring that window to the foreground.
    if (win) {
        win->Focus();
    }
    StartLoadDocument(&args);

    str::Free(d->path);
    delete d;
}

LRESULT OnCopyData(HWND hwnd, WPARAM wp, LPARAM lp) {
    COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lp;
    if (!cds || wp) {
        return FALSE;
    }

    if (cds->dwData == kCopyDataOpen) {
        // Simple-open fast path used by the reuseInstance handshake: the
        // sibling SumatraPDF that Explorer just spawned is blocked in
        // SendMessageW. Copy the path out, post an async task, return
        // immediately so the sender unblocks and exits.
        if (cds->cbData < sizeof(SumatraOpenCopyData) + 1) {
            return FALSE;
        }
        auto* data = (const SumatraOpenCopyData*)cds->lpData;
        size_t pathMax = cds->cbData - sizeof(SumatraOpenCopyData);
        Str pathZ = Str((char*)(const u8*)(data + 1), (int)pathMax);
        // require null-terminator within bounds
        if (strnlen_s(pathZ.s, pathMax) >= pathMax) {
            return FALSE;
        }
        auto* d = new OpenCopyDataAsync;
        d->path = str::Dup(pathZ);
        d->newWindow = data->newWindow;
        auto fn = MkFunc0<OpenCopyDataAsync>(OpenCopyDataAsyncRun, d);
        uitask::Post(fn, "OnCopyData/Open");
        return TRUE;
    }

    if (cds->dwData == kCopyDataDdeW) {
        int cmdCch = (int)(cds->cbData / sizeof(WCHAR));
        if (cmdCch == 0 || ((wchar_t*)cds->lpData)[cmdCch - 1] != 0) {
            return FALSE;
        }
        WStr cmdW((const wchar_t*)cds->lpData, cmdCch - 1);
        // Legacy DDE grammar — callers expect synchronous handling.
        TempStr cmd = ToUtf8Temp(cmdW);
        bool didHandle = HandleExecuteCmds(hwnd, cmd);
        return didHandle ? TRUE : FALSE;
    }

    return FALSE;
}

#if 0
bool RegisterDDeServer() {
    DWORD ddeInst = (DWORD)-1;
    auto err = DdeInitializeW(&ddeInst, nullptr, APPCMD_CLIENTONLY | CBF_FAIL_ADVISES, 0);
    if (err != DMLERR_NO_ERROR) {
        // Handle initialization error
        logf("RegisterDDeServer: DdeInitializeW() failed with '%d'\n", (int)err);
        return false;
    }
    return true;
}
#endif
