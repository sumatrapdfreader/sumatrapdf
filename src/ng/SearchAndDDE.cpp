/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the search half of orig's SearchAndDDE.cpp - find-as-you-type, the
// interactive find worker (FindThread), the full-document match counter
// (CountThread), the all-match highlights, inverse search and the
// forward-search mark. The DDE server below is Windows only. `Gfx*` is
// `gpui::PaintCtx*` and the debounce / progress timers are the shell's tick.

#include "gui/GpuiBridge.h"
#include "base/UITask.h"
#include "base/Timer.h"
#include "base/File.h"
#if OS_WIN
#include "base/Win.h"
#include "base/ScopedWin.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Version.h"
#include "ExplorerQuickLook.h"
#include "Tabs.h"
#include "Selection.h"
#include "PdfSync.h"
#include "AppTools.h"
#include "base/Launch.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Favorites.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "SearchAndDDE.h"

#include "SumatraLog.h"

// last 10 find queries, newest first. Session-only (issue #893).
constexpr int kFindHistoryMax = 10;
static StrVec gFindHistory;

const StrVec& FindHistory() {
    return gFindHistory;
}

void RememberFindQuery(Str q) {
    if (len(q) == 0) {
        return;
    }
    TempStr trimmed = str::DupTemp(q);
    str::TrimWSInPlace(trimmed, str::TrimOpt::Both);
    if (len(trimmed) == 0) {
        return;
    }
    int existing = gFindHistory.Find(trimmed);
    if (existing == 0) {
        return; // already the most recent
    }
    if (existing > 0) {
        gFindHistory.RemoveAt(existing);
    }
    gFindHistory.InsertAt(0, trimmed);
    while (len(gFindHistory) > kFindHistoryMax) {
        gFindHistory.RemoveAt(len(gFindHistory) - 1);
    }
}

TempStr FindHistoryResultTemp(int* exitCodeOut) {
    str::Builder out;
    for (int i = 0; i < len(gFindHistory); i++) {
        if (i > 0) {
            out.AppendChar('\n');
        }
        out.Append(gFindHistory[i]);
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Chrome-style orange for the non-active find matches. The active (current)
// match uses the user-customizable FixedPageUI.SelectionColor instead, so it
// stands out with the color the user finds most noticeable (issue #5740).
constexpr Color kFindOtherMatchColor = MkRgb(0xff, 0x96, 0x32);

struct FindMatchPaintPageRect {
    int pageNo = 0;
    Rect rect;
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
    int countEpoch = 0;
    // all page rects for all entries, laid out contiguously; each entry
    // references its rects as a [firstPos, firstPos + len) slice. Both entries
    // and positions are plain POD so they can live in a Vec by value; entries
    // hold indices (not pointers), so they stay valid as positions reallocates.
    Vec<FindMatchPaintPageRect> positions;
    Vec<FindMatchPaintRects> entries;
} gFindMatchPaintCache;

static void FreeFindMatchPaintCacheEntries() {
    VecReset(gFindMatchPaintCache.entries);
    VecReset(gFindMatchPaintCache.positions);
}

void InvalidateFindMatchPaintCache() {
    FreeFindMatchPaintCacheEntries();
    gFindMatchPaintCache.firstPage = 0;
    gFindMatchPaintCache.lastPage = 0;
    gFindMatchPaintCache.countEpoch = 0;
}

static Kind kNotifFindProgress = "findProgress";

// the controller if the current document is rendered in a webview that
// supports our in-page find (chm / markdown)
static DocController* BrowserFindCtrl(MainWindow* win) {
    DocController* ctrl = win->ctrl;
    if (ctrl && ctrl->CanFindInPage()) {
        return ctrl;
    }
    return nullptr;
}

// Record a find session's starting view once, so Back returns there rather
// than to an intermediate find-as-you-type result.
static void MarkSearchStart(MainWindow* win) {
    if (win->searchStartMarked) {
        return;
    }
    win->searchStartMarked = true;
    SetSearchStartFavorite(win);
    if (DisplayModel* dm = win->AsFixed()) {
        dm->AddNavPoint();
    }
}

// start a new find in the browser-hosted (chm / markdown) webview for the find
// bar's text: highlight the current page and sweep all pages for the match
// list. Results arrive asynchronously via BrowserFindResultReceived() /
// BrowserFindAllResultReceived()
static void BrowserFindStartSearch(MainWindow* win, DocController* md) {
    TempStr term = FindEditTextTemp(win);
    if (len(term) == 0) {
        return;
    }
    RememberFindQuery(term);
    MarkSearchStart(win);
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

// update the find bar's "n / m" status from the current in-page match and the
// all-pages sweep
static void BrowserFindUpdateStatus(MainWindow* win, DocController* md, int pageCur, int pageTotal) {
    if (win->browserFindTotal < 0) {
        // the all-pages sweep hasn't finished: show per-page numbers for now
        TempStr s = fmt("%d / %d", pageCur, pageTotal);
        FindBarSetStatus(win, s, pageTotal);
        return;
    }
    win->browserFindCurrent = BrowserFindGlobalMatchIdx(win, md->CurrentPageNo(), pageCur);
    TempStr s = fmt("%d / %d", win->browserFindCurrent + 1, win->browserFindTotal);
    FindBarSetStatus(win, s, win->browserFindTotal);
}

// in-page find result posted by a chm / markdown webview: update the find bar status
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
// each "<page>\x1f<idx>\x1f<snippet>" (\x1f: unit sep)
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
        VecAppend(win->findMatches, fm);
    }
    win->browserFindTotal = total;
    win->findCountHasSnippets = true;
    BrowserFindUpdateStatus(win, md, win->browserFindPageCurrent, total);
}

// Match SetText()'s normalization: strip one leading space (word-start) so a
// trailing/whole-word space still compares as the same term.
static Str FindTermWithoutWordStartSpace(Str text) {
    if (text && len(text) > 0 && text.s[0] == ' ') {
        return Str(text.s + 1, text.len - 1);
    }
    return text;
}

// True when the find box holds a different term than the current results /
// last search, so Enter / Find Next should start a new search instead of
// stepping a stale list (issue #893).
bool FindTermDiffersFromLast(MainWindow* win) {
    if (!win) {
        return false;
    }
    TempStr term = FindEditTextTemp(win);
    Str searchText = FindTermWithoutWordStartSpace(term);
    if (len(searchText) == 0) {
        return false;
    }
    if (win->findCountText && len(win->findCountText) > 0) {
        return !str::Eq(searchText, FindTermWithoutWordStartSpace(win->findCountText));
    }
    if (DisplayModel* dm = win->AsFixed()) {
        if (dm->textSearch && dm->textSearch->lastText) {
            return !str::Eq(searchText, dm->textSearch->lastText);
        }
    }
    if (win->browserFindTerm) {
        return !str::Eq(searchText, FindTermWithoutWordStartSpace(win->browserFindTerm));
    }
    return true;
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
    if (FindTermDiffersFromLast(win)) {
        BrowserFindStartSearch(win, md);
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

static bool HasFindText(MainWindow* win);

// Ctrl+F with a term left in the box: highlight its matches on the pages in
// view without moving to one; Enter is what restarts the search
static void HighlightRestoredFindTerm(MainWindow* win) {
    if (!HasFindText(win)) {
        return;
    }
    // typing (or a copied selection) is about to start its own search
    if (win->findDebouncePending || len(win->findMatches) > 0) {
        return;
    }
    EnsureFindSnippets(win);
}

void FindFirst(MainWindow* win) {
    // Only open/focus the find UI here. The search-start favorite ("/") is set
    // when a real search begins (non-empty term in FindTextOnThread /
    // BrowserFindStartSearch), not merely when the find box is opened
    // (issue #5862 / #5726).
    if (!win) {
        return;
    }
    bool hadFindFocus = IsFindEditFocused(win);
    if (!hadFindFocus) {
        win->searchStartMarked = false;
    }

    if (BrowserFindCtrl(win)) {
        // chm / markdown in a webview: our own find bar drives the search
        // inside the webview
        ShowFindBar(win);
        FocusFindEditSelectAll(win);
        return;
    }

    if (!win->AsFixed() || !NeedsFindUI(win)) {
        return;
    }

    DisplayModel* dm = win->AsFixed();

    // show the floating Chrome-style find bar (creates it lazily if needed)
    ShowFindBar(win);

    // If focus was in the document (not find bar), copy selected text
    // to find edit only if it's different from current text. Setting the text
    // triggers find-as-you-type via the bar's onTextChanged handler.
    if (!hadFindFocus && dm->textSelection->result.len > 0) {
        Str sel = dm->textSelection->ExtractText(StrL(" "));
        TempStr selection = str::DupTemp(sel);
        str::Free(sel);
        selection.len -= str::NormalizeWSInPlace(selection);
        if (len(selection) > 0) {
            TempStr current = FindEditTextTemp(win);
            if (!str::EqI(selection, current)) {
                AbortFinding(win, false);
                dm->textSearch->SetLastResult(dm->textSelection);
                FindEditSetText(win, selection);
            }
        }
    }

    FocusFindEditSelectAll(win);
    HighlightRestoredFindTerm(win);
}

// debounce delays (ms) for find-as-you-type. Short terms (1-2 chars) match a
// lot of text and the search is expensive, so wait longer before starting them
// (issue #4626). Enter bypasses the wait (see FindFlushPendingSearch).
constexpr int kFindDebounceDelayMs = 500;
constexpr int kFindDebounceShortDelayMs = 1000;

// run the actual incremental search; assumes there is non-empty find text
static void StartIncrementalFind(MainWindow* win) {
    DocController* md = BrowserFindCtrl(win);
    if (md) {
        BrowserFindStartSearch(win, md); // sets search-start mark
        return;
    }
    // Find-as-you-type is an intentional search start even when the edit was
    // not modified (e.g. Ctrl+F copied the selection after SetLastResult).
    MarkSearchStart(win);
    // the full-document count (n/m + results list) is kicked from FindEndTask,
    // after this find thread exits, so the two never touch the engine's text
    // extraction concurrently (mupdf isn't safe for that)
    FindTextOnThread(win, TextSearch::Direction::Forward, false);
}

// Parse a find-UI page range: empty, "10", "10-25", "10-", "-25", or a
// comma-separated list such as "3,4-6,18-". Whitespace around tokens is
// allowed. Invalid input returns false (caller treats that as all pages).
bool ParseFindPageRange(Str s, int nPages, Vec<bool>& allowedOut) {
    VecReset(allowedOut);
    if (len(s) == 0 || nPages < 1) {
        return true;
    }
    const char* p = s.s;
    const char* end = s.s + s.len;
    auto skipWs = [&]() {
        while (p < end && str::IsWs(*p)) {
            p++;
        }
    };
    auto parseNum = [&](int& out) -> bool {
        if (p >= end || *p < '0' || *p > '9') {
            return false;
        }
        int n = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            n = (n * 10) + (*p - '0');
            p++;
        }
        out = n;
        return true;
    };
    auto parseDash = [&]() -> bool {
        if (p >= end) {
            return false;
        }
        if (*p == '-') {
            p++;
            return true;
        }
        // UTF-8 en-dash U+2013 (e2 80 93)
        if ((u8)*p == 0xe2 && p + 2 < end && (u8)p[1] == 0x80 && (u8)p[2] == 0x93) {
            p += 3;
            return true;
        }
        return false;
    };

    VecResize(allowedOut, nPages);
    for (int i = 0; i < nPages; i++) {
        allowedOut[i] = false;
    }
    bool any = false;
    while (p < end) {
        skipWs();
        if (p >= end) {
            break;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        int first = 0;
        int last = 0;
        bool haveFirst = parseNum(first);
        skipWs();
        bool haveDash = parseDash();
        skipWs();
        bool haveLast = parseNum(last);
        skipWs();
        if (p < end && *p != ',') {
            VecReset(allowedOut);
            return false;
        }
        if (!haveFirst && !haveDash && !haveLast) {
            VecReset(allowedOut);
            return false;
        }
        if (haveFirst && !haveDash && !haveLast) {
            last = first;
        } else if (!haveFirst && haveDash && haveLast) {
            first = 1;
        } else if (haveFirst && haveDash && !haveLast) {
            last = nPages;
        } else if (!haveFirst && haveDash && !haveLast) {
            VecReset(allowedOut);
            return false;
        }
        if (first > last) {
            int tmp = first;
            first = last;
            last = tmp;
        }
        if (last < 1 || first > nPages) {
            if (p < end && *p == ',') {
                p++;
            }
            continue;
        }
        if (first < 1) {
            first = 1;
        }
        if (last > nPages) {
            last = nPages;
        }
        for (int page = first; page <= last; page++) {
            allowedOut[page - 1] = true;
        }
        any = true;
        if (p < end && *p == ',') {
            p++;
        }
    }
    if (!any) {
        VecReset(allowedOut);
    }
    return true;
}

static bool ApplyFindPageRange(MainWindow* win) {
    TempStr spec = win->findPagesEdit ? str::DupTemp(FromGpui(gp::InputValue(win->findPagesEdit))) : TempStr{};
    bool changed = !str::Eq(spec, win->findPageRangeText);
    str::ReplaceWithCopy(&win->findPageRangeText, spec);
    DisplayModel* dm = win->AsFixed();
    if (dm && dm->textSearch) {
        Vec<bool> allowed;
        int nPages = dm->PageCount();
        if (!ParseFindPageRange(spec, nPages, allowed)) {
            VecReset(allowed);
        }
        dm->textSearch->SetAllowedPages(allowed);
    }
    return changed;
}

// find-as-you-type: called when the find bar's edit text changes. Instead of
// searching on every keystroke, (re)arm a debounce timer; the search starts a
// short while after the user stops typing (issue #4626).
void OnFindBarTextChanged(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    TempStr s = FindEditTextTemp(win);
    if (len(s) == 0) {
        AbortFinding(win, true); // also cancels a pending debounce timer
        DocController* md = BrowserFindCtrl(win);
        if (md) {
            md->FindClear(); // remove the highlights in the webview
        }
        ClearSearchResult(win);
        FindBarSetStatus(win, StrL(""));
        ClearFindMatches(win);
        return;
    }
    int nChars = FindEditTextLen(win);
    int delay = (nChars <= 2) ? kFindDebounceShortDelayMs : kFindDebounceDelayMs;
    // re-arming replaces the previous countdown, so each keystroke restarts it
    win->findDebounceLeftMs = delay;
    win->findDebouncePending = true;
}

// the shell's tick: runs the deferred search once the countdown expires
void FindDebounceTick(MainWindow* win, int elapsedMs) {
    if (!win->findDebouncePending) {
        return;
    }
    win->findDebounceLeftMs -= elapsedMs;
    if (win->findDebounceLeftMs > 0) {
        return;
    }
    win->findDebouncePending = false;
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (FindEditTextLen(win) > 0) {
        StartIncrementalFind(win);
    }
}

static bool HasFindText(MainWindow* win) {
    return FindEditTextLen(win) > 0;
}

// if a debounced search is pending, cancel the timer and start it now (so Enter
// forces the search to start immediately). Returns true if one was pending.
bool FindFlushPendingSearch(MainWindow* win) {
    if (!win->findDebouncePending) {
        return false;
    }
    win->findDebouncePending = false;
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
    FindEditSetModified(win, true);
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
    FindEditSetModified(win, true);
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

    Str sel = dm->textSelection->ExtractText(StrL(" "));
    TempStr selection = str::DupTemp(sel);
    str::Free(sel);
    selection.len -= str::NormalizeWSInPlace(selection);
    if (len(selection) == 0) {
        return;
    }

    FindEditSetText(win, selection);
    FindEditSetModified(win, false);
    AbortFinding(win, false); // cancel "find as you type"
    dm->textSearch->SetLastResult(dm->textSelection);

    // wasModified stays false so FindNext continues from the selection; this
    // selection begins a new find session.
    win->searchStartMarked = false;
    MarkSearchStart(win);
    FindTextOnThread(win, direction, true);
}

static void ShowSearchResult(MainWindow* win, TextSel* result, bool goToPage) {
    ReportIf(0 == result->len || !result->pages || !result->rects);
    if (0 == result->len || !result->pages || !result->rects) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (goToPage || !dm->PageShown(result->pages[0]) ||
        (dm->GetZoomVirtual() == kZoomFitPage || dm->GetZoomVirtual() == kZoomFitContent)) {
        bool suppress = dm->stableNavPoint.suppress;
        dm->stableNavPoint.suppress = true;
        win->ctrl->GoToPage(result->pages[0], false);
        dm->stableNavPoint.suppress = suppress;
    }

    // Find never changes the text selection: all matches (including the active
    // one) are highlighted independently by PaintAllFindMatches, so the user's
    // selection highlight is separate and survives searching (issue #5737).
    dm->ShowResultRectToScreen(result);
    InvalidateFindMatchPaintCache();
    AppShellInvalidate(win);
}

void ClearSearchResult(MainWindow* win) {
    // clear only the find-match highlights, never the user's text selection:
    // find and selection highlights are tracked independently (issue #5737)
    ClearFindMatches(win); // also invalidates the find-match paint cache
    AppShellInvalidate(win);
}

struct UpdateFindStatusData {
    MainWindow* win;
    int current;
    int total;
    bool showProgress;
};

static void UpdateFindStatus(UpdateFindStatusData* d) {
    AutoDelete delData(d);

    auto* win = d->win;
    if (!IsMainWindowValidAndNotClosing(win) || win->findCancelled) {
        return;
    }
    if (!d->showProgress) {
        // find-as-you-type: don't let the incremental find scan the whole
        // document. The n/m counter is built by the count thread (which does
        // its own full scan), so bail out early and leave it the heavy lifting.
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
    // ng: the handle is owned by MainWindow::findThread (JoinThread() closes
    // it); this is only compared, to tell whether we are still the live find
    ThreadHandle thread = nullptr;

    FindThreadData(MainWindow* win, TextSearch::Direction direction, Str text, bool wasModified) {
        this->win = win;
        this->direction = direction;
        this->text = str::Dup(text);
        this->wasModified = wasModified;
    }
    ~FindThreadData() { str::Free(text); }

    void ShowUI(bool showProgressIn) {
        // no "Searching n of m..." notification anymore: the find UI's own n/m
        // counter is the feedback. We still remember showProgress to decide
        // whether the incremental find may bail early (see UpdateFindStatus).
        this->showProgress = showProgressIn;
    }

    void HideUI(bool success, bool loopedAround) const {
        if (!success && !loopedAround) {
            // i.e. canceled
            FindBarSetStatus(win, StrL(""));
        } else if (!success && loopedAround) {
            // keep it compact and consistent with the "n / m" counter
            FindBarSetStatus(win, StrL("0 / 0"), 0);
        }
        // else: a match was found; the "n / m" counter (set by UpdateMatchCount
        // after this) is the only feedback - no beep on wrap-around
    }

    bool WasCanceled() {
        bool winValid = IsMainWindowValidAndNotClosing(win);
        auto res = !winValid || win->findCancelled;
        if (res) {
            logf("FindThreadData: WasCanceled() returns true, isMainWindowValid: %d, win->findCancelled: %d\n",
                 (int)winValid, (int)win->findCancelled);
        }
        return res;
    }

    void UpdateProgress(int current, int total) {
        auto* data = new UpdateFindStatusData;
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

// The scan starts at the page that was current when it began and wraps around,
// so it produces matches out of document order (e.g. 89, 104, 47). The "n / m"
// counter presents matches in document order, so re-sort by (page, glyph) as
// matches are installed. MatchKey packs page into the high half, so sorting the
// u64 keys sorts by (page, glyph) too.
static int CmpFindMatchByPos(const FindMatch* a, const FindMatch* b) {
    if (a->startPage != b->startPage) {
        return a->startPage - b->startPage;
    }
    return a->startGlyph - b->startGlyph;
}

static int CmpMatchKey(const u64* a, const u64* b) {
    if (*a == *b) {
        return 0;
    }
    return (*a < *b) ? -1 : 1;
}

// 1-based index of `key` within the positions cache, or 0 if not found.
// positions are in document order, but a linear lookup is cheap enough here
// (n <= kMaxFindCount)
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
    FindBarSetStatus(win, s, total);
    logf("ShowMatchCount: %s (page %d)\n", s, dm && dm->textSearch ? dm->textSearch->startPage : 0);
}

// cap on how many per-match snippets we build for the floating results list
// (matches beyond this still count toward "n / m", just aren't listed)
constexpr int kMaxFindResults = 5000;

// stop scanning after this many matches: with a common word the full count
// isn't useful, only slow. The status then shows "n / 999+".
constexpr int kMaxFindCount = 999;

// free the cached per-match snippets (win->findMatches)
void ClearFindMatches(MainWindow* win) {
    int n = len(win->findMatches);
    for (int i = 0; i < n; i++) {
        str::Free(win->findMatches[i].snippet);
    }
    VecReset(win->findMatches);
    win->findCountHasSnippets = false;
    InvalidateFindMatchPaintCache();
    win->browserFindPageCurrent = 0;
    win->browserFindCurrent = -1;
    win->browserFindTotal = -1;
}

static void StartFindCount(MainWindow* win, Str text, bool matchCase, bool matchWholeWord);

// Drop find-match / match-count state that only applies to the previous document
// (tab switch, close-current, reload). Keeps find box text (#5308). If the find
// UI is still open, starts a new count so all-match highlights rebuild.
void InvalidateFindForDocumentChange(MainWindow* win) {
    if (!win) {
        return;
    }
    // Page/glyph coords and the match-count cache are for the previous engine.
    // Keep the find box text so the user can re-search after close/reload (#5308).
    ClearFindMatches(win);
    win->findCountValid = false;
    win->findCountCapped = false;
    win->findCountEngine = nullptr;
    VecReset(win->findCountPositions);
    str::FreePtr(&win->findCountText);

    if (!IsFindUIVisible(win)) {
        return;
    }
    TempStr s = FindEditTextTemp(win);
    if (len(s) == 0) {
        FindBarSetStatus(win, StrL(""));
        return;
    }
    if (win->AsFixed()) {
        StartFindCount(win, s, win->findMatchCase, win->findMatchWholeWord);
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (md) {
        BrowserFindStartSearch(win, md);
    }
}

// build a one-line "...context match context..." snippet (UTF-8) around a match
static TempStr BuildSnippet(EngineBase* engine, const FindMatch& m) {
    int textLen = 0;
    Str pageText = engine->GetTextForPage(m.startPage, &textLen);
    if (len(pageText) == 0) {
        return {};
    }
    int mStart = limitValue(m.startGlyph, 0, textLen);
    int mEnd = (m.endPage == m.startPage) ? m.endGlyph : textLen;
    mEnd = limitValue(mEnd, mStart, textLen);
    const int kCtx = 40;
    int from = std::max(0, mStart - kCtx);
    int to = std::min(textLen, mEnd + kCtx);
    TempStr sub = str::DupTemp(Utf8SliceByCodepoints(pageText, from, to - from));
    sub.len -= str::NormalizeWSInPlace(sub);
    return fmt("%s%s%s", Str(from > 0 ? "..." : ""), sub, Str(to < textLen ? "..." : ""));
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
    Str rangeSpec;              // Pages box text (issue #5694)
    int epoch = 0;
    // ng: owned by MainWindow::findCountThread, compared here only
    ThreadHandle thread = nullptr;
    // worker-thread only: drive the "<found>... <page>" progress status
    int nFoundSoFar = 0;
    TimeStamp lastProgressAt{};
    bool hasProgressAt = false;

    CountThreadData(MainWindow* winIn, EngineBase* engineIn, Str textIn, bool matchCaseIn, bool matchWholeWordIn,
                    bool wantMatchListIn, bool wantSnippetsIn, int startPageIn, Str rangeSpecIn, int epochIn) {
        this->win = winIn;
        this->engine = engineIn;
        this->text = str::Dup(textIn);
        this->matchCase = matchCaseIn;
        this->matchWholeWord = matchWholeWordIn;
        this->wantMatchList = wantMatchListIn;
        this->wantSnippets = wantSnippetsIn;
        this->startPage = startPageIn;
        this->rangeSpec = str::Dup(rangeSpecIn);
        this->epoch = epochIn;
    }
    ~CountThreadData() {
        str::Free(text);
        str::Free(rangeSpec);
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
    Vec<FindMatch>* matches = nullptr; // nullptr unless the match list was requested
    ~CountEndTaskData() {
        delete ctd;
        delete positions;
        FreeMatchSnippets(matches); // frees any snippets not transferred to win
        delete matches;
    }
};

static void CountEndTask(CountEndTaskData* d) {
    AutoDelete delData(d);
    MainWindow* win = d->win;
    CountThreadData* ctd = d->ctd;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (win->findCountThread != ctd->thread) {
        return; // superseded (or already joined by AbortCount)
    }
    SafeCloseThreadHandle(&win->findCountThread);
    if (win->findCountEpoch == ctd->epoch) {
        // not canceled: install the freshly built cache (steal text from ctd)
        str::FreePtr(&win->findCountText);
        win->findCountText = ctd->text;
        ctd->text = {};
        win->findCountMatchCase = ctd->matchCase;
        win->findCountMatchWholeWord = ctd->matchWholeWord;
        str::ReplaceWithCopy(&win->findCountRangeText, ctd->rangeSpec);
        win->findCountEngine = ctd->engine;
        win->findCountPositions = *d->positions;
        VecSort(win->findCountPositions, CmpMatchKey);
        win->findCountCapped = d->capped;
        win->findCountValid = true;
        if (d->matches) {
            // install the match list (steal ownership of the snippet strings)
            FindWindowSaveSelectedMatch(win);
            ClearFindMatches(win);
            win->findMatches = *d->matches;
            for (int i = 0; i < len(*d->matches); i++) {
                (*d->matches)[i].snippet = Str(); // transferred to win->findMatches
            }
            VecSort(win->findMatches, CmpFindMatchByPos);
            win->findCountHasSnippets = ctd->wantSnippets;
        }
        InvalidateFindMatchPaintCache();
        FindWindowRefreshResults(win);
        ShowMatchCount(win);
        AppShellInvalidate(win);
    }
    // a newer term arrived while we were scanning: run it now (no worker running)
    if (win->findCountPendingText) {
        Str pending = win->findCountPendingText;
        win->findCountPendingText = {};
        StartFindCount(win, pending, win->findCountPendingMatchCase, win->findCountPendingMatchWholeWord);
        str::Free(pending);
    }
}

// Page the running scan is on, so the in-progress status can show it. Only one
// scan runs at a time (older ones are canceled by epoch), so a single global is
// enough; reset when a scan starts.
static int gFindCountCurPage = 0;

// status while a scan is in flight: matches so far and the page being scanned,
// e.g. "12 34". ShowMatchCount replaces it with "n / m" when the scan ends.
static void SetFindCountProgressStatus(MainWindow* win, int nFound, int pageNo) {
    if (pageNo > 0) {
        gFindCountCurPage = pageNo;
    }
    pageNo = gFindCountCurPage;
    TempStr pageStr = {};
    if (pageNo > 0 && win->ctrl) {
        pageStr = win->ctrl->GetPageLabeTemp(pageNo);
    }
    if (nFound > 0) {
        FindBarSetStatus(win, fmt("%d %s", nFound, pageStr));
    } else {
        FindBarSetStatus(win, pageStr);
    }
}

struct CountProgressTaskData {
    MainWindow* win = nullptr;
    int epoch = 0;
    int nFound = 0;
    int pageNo = 0;
};

static void CountProgressTask(CountProgressTaskData* d) {
    AutoDelete delData(d);
    MainWindow* win = d->win;
    if (!IsMainWindowValidAndNotClosing(win) || win->findCountEpoch != d->epoch) {
        return;
    }
    SetFindCountProgressStatus(win, d->nFound, d->pageNo);
    AppShellInvalidate(win);
}

// don't post a status update more often than this while scanning
constexpr double kFindProgressMs = 100;

static void CountProgress(CountThreadData* d, ProgressUpdateData* data) {
    if (data->wasCancelled) {
        *data->wasCancelled = (d->win->findCountEpoch != d->epoch);
    }
    // TextSearch reports once per page, which is often enough to show where the
    // scan is even when a long stretch of pages has no match at all
    if (data->current <= 0) {
        return;
    }
    if (d->hasProgressAt && TimeSinceInMs(d->lastProgressAt) < kFindProgressMs) {
        return;
    }
    d->lastProgressAt = TimeGet();
    d->hasProgressAt = true;
    auto* pd = new CountProgressTaskData;
    pd->win = d->win;
    pd->epoch = d->epoch;
    pd->nFound = d->nFoundSoFar;
    pd->pageNo = data->current;
    uitask::Post(MkFunc0<CountProgressTaskData>(CountProgressTask, pd), "TaskFindCountProgress");
}

// streaming partial results while the scan runs: first batch after
// kFindResultsFirstBatch matches, then a batch only when both
// kFindResultsBatch new matches accumulated and kFindResultsBatchMs passed
// since the last one (avoids flooding the UI thread for common words)
constexpr int kFindResultsFirstBatch = 16;
constexpr int kFindResultsBatch = 100;
constexpr double kFindResultsBatchMs = 500;

struct CountPartialTaskData {
    MainWindow* win = nullptr;
    int epoch = 0;
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
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (win->findCountEpoch != d->epoch) {
        return; // canceled or superseded; drop stale partial results
    }
    // running count while the scan is in flight; ShowMatchCount switches this
    // to "n / m" when the scan finishes. Pass 0 for the page: keep whatever the
    // progress callback last reported instead of clearing it.
    SetFindCountProgressStatus(win, d->nFoundSoFar, 0);
    if (len(*d->matches) > 0) {
        if (d->firstBatch) {
            ClearFindMatches(win);
        }
        for (int i = 0; i < len(*d->matches); i++) {
            VecAppend(win->findMatches, (*d->matches)[i]);
            (*d->matches)[i].snippet = Str(); // transferred to win->findMatches
        }
        VecSort(win->findMatches, CmpFindMatchByPos);
        win->findCountHasSnippets = true;
        InvalidateFindMatchPaintCache();
        // a streamed partial update must not navigate: that would cancel the
        // very scan producing these results
        FindWindowRefreshResults(win, false);
    }
    AppShellInvalidate(win);
}

// clones matches[from..to) incl. copies of the snippet strings
static Vec<FindMatch>* CloneMatchesRange(Vec<FindMatch>* matches, int from, int to) {
    auto* res = new Vec<FindMatch>();
    for (int i = from; i < to; i++) {
        FindMatch fm = (*matches)[i];
        fm.snippet = str::Dup(fm.snippet);
        VecAppend(*res, fm);
    }
    return res;
}

static void CountThread(CountThreadData* d) {
    MainWindow* win = d->win;
    EngineBase* engine = d->engine;

    auto* positions = new Vec<u64>();
    Vec<FindMatch>* matches = d->wantMatchList ? new Vec<FindMatch>() : nullptr;
    int nSent = 0;        // positions already reported via a partial batch
    int nSentMatches = 0; // matches already streamed to the results list
    TimeStamp lastSendAt{};
    bool capped = false; // scan stopped at kMaxFindCount matches
    {
        TextSearch ts(engine);
        ts.SetMatchCase(d->matchCase);
        ts.SetMatchWholeWord(d->matchWholeWord);
        Vec<bool> allowed;
        if (!ParseFindPageRange(d->rangeSpec, engine->PageCount(), allowed)) {
            VecReset(allowed);
        }
        ts.SetAllowedPages(allowed);
        ts.SetDirection(TextSearch::Direction::Forward);
        ts.progressCb = MkFunc1<CountThreadData, ProgressUpdateData*>(CountProgress, d);
        // scan from the current page so results near the reading position come
        // first; wrap around to cover the rest of the (restricted) range
        int wrapStart = ts.RestrictFirst();
        bool wrapped = false;
        TextSel* m = ts.FindFirst(d->startPage, d->text);
        if (!m && d->startPage > wrapStart) {
            // Nothing at or after startPage. The wrap-around below only runs
            // from inside the loop, so without this the loop is never entered
            // and the scan reports zero matches even though earlier pages have
            // them (issue #5874)
            wrapped = true;
            m = ts.FindFirst(wrapStart, d->text);
        }
        // check the epoch at the top so a cancel (AbortCount, which joins us on
        // the UI thread) bails before the expensive snippet build / next scan
        while (m && win->findCountEpoch == d->epoch) {
            if (len(*positions) >= kMaxFindCount) {
                capped = true;
                break;
            }
            VecAppend(*positions, MatchKey(ts.startPage, ts.startGlyph));
            d->nFoundSoFar = len(*positions); // read by CountProgress
            if (matches && len(*matches) < kMaxFindResults) {
                FindMatch fm;
                fm.startPage = ts.startPage;
                fm.startGlyph = ts.startGlyph;
                fm.endPage = ts.endPage;
                fm.endGlyph = ts.endGlyph;
                if (d->wantSnippets) {
                    str::ReplaceWithCopy(&fm.snippet, BuildSnippet(engine, fm));
                }
                VecAppend(*matches, fm);
            }
            // stream partial results so a slow scan (common word, big doc)
            // shows a running count early; the final full list is installed by
            // CountEndTask. CountPartialTask re-checks the epoch on the UI
            // thread, so a stale batch can't clobber a newer search.
            if (d->wantSnippets) {
                int n = len(*positions);
                bool send;
                if (nSent == 0) {
                    send = n >= kFindResultsFirstBatch;
                } else {
                    send = (n - nSent >= kFindResultsBatch) && (TimeSinceInMs(lastSendAt) >= kFindResultsBatchMs);
                }
                if (send) {
                    auto* pd = new CountPartialTaskData;
                    pd->win = win;
                    pd->epoch = d->epoch;
                    pd->firstBatch = (nSentMatches == 0);
                    pd->nFoundSoFar = n;
                    int nMatches = matches ? len(*matches) : 0;
                    pd->matches = CloneMatchesRange(matches, nSentMatches, nMatches);
                    nSent = n;
                    nSentMatches = nMatches;
                    lastSendAt = TimeGet();
                    uitask::Post(MkFunc0<CountPartialTaskData>(CountPartialTask, pd), "TaskFindCountPartial");
                }
            }
            m = ts.FindNext();
            if (!m && !wrapped && d->startPage > wrapStart) {
                wrapped = true;
                m = ts.FindFirst(wrapStart, d->text);
            }
            if (wrapped && m && ts.startPage >= d->startPage) {
                m = nullptr; // came full circle
            }
        }
    }
    SafeEngineRelease(&engine);

    // wait for StartFindCount to record the thread handle (mirrors FindThread)
    while (!win->findCountThread) {
        SleepInMs(1);
    }

    auto* data = new CountEndTaskData;
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
    AtomicIntInc(&win->findCountEpoch);
    str::FreePtr(&win->findCountPendingText);
    if (win->findCountThread) {
        JoinThread(&win->findCountThread, -1);
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
    // CountThread runs on a worker thread and can't touch DisplayModel; do the
    // layout and the resync it needs here, before it starts
    EnsureFullLayout(dm);
    win->findCountValid = false;
    // seed the progress status with the page the scan starts from, so it shows a
    // page right away instead of going blank until the first progress tick;
    // replaced with "n / m" when the scan finishes
    gFindCountCurPage = 0;
    SetFindCountProgressStatus(win, 0, win->ctrl ? win->ctrl->CurrentPageNo() : 0);

    if (win->findCountThread) {
        // a scan is in flight: cancel it and queue this request; the running
        // worker's CountEndTask will start it once it exits
        AtomicIntInc(&win->findCountEpoch);
        str::FreePtr(&win->findCountPendingText);
        win->findCountPendingText = str::Dup(text);
        win->findCountPendingMatchCase = matchCase;
        win->findCountPendingMatchWholeWord = matchWholeWord;
        return;
    }

    engine->AddRef(); // released in CountThread
    ApplyFindPageRange(win);
    // always build the match list so PaintAllFindMatches can highlight every
    // hit; the snippets feed the floating results list
    bool wantSnippets = IsFindWindowVisible(win);
    bool wantMatchList = true;
    int epoch = AtomicIntInc(&win->findCountEpoch);
    int startPage = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    auto* d = new CountThreadData(win, engine, text, matchCase, matchWholeWord, wantMatchList, wantSnippets, startPage,
                                  win->findPageRangeText, epoch);
    win->findCountThread = nullptr;
    auto fn = MkFunc0<CountThreadData>(CountThread, d);
    win->findCountThread = StartThread(fn, StrL("FindCountThread"));
    d->thread = win->findCountThread;
}

// Term currently being searched: the find edit if it has text, else the last
// completed count / TextSearch.
TempStr CurrentFindTermTemp(MainWindow* win) {
    if (!win) {
        return {};
    }
    TempStr s = FindEditTextTemp(win);
    if (len(s) > 0) {
        return s;
    }
    if (win->findCountText && len(win->findCountText) > 0) {
        return str::DupTemp(win->findCountText);
    }
    if (DisplayModel* dm = win->AsFixed()) {
        if (dm->textSearch && dm->textSearch->lastText) {
            return str::DupTemp(dm->textSearch->lastText);
        }
    }
    return {};
}

// update the n/m counter after a search settles on a match: instant from cache
// when the term/match-case/document are unchanged, otherwise rebuild it
static void UpdateMatchCount(MainWindow* win, Str text) {
    DisplayModel* dm = win->AsFixed();
    void* engine = dm ? (void*)dm->GetEngine() : nullptr;
    bool wantSnippets = IsFindWindowVisible(win);
    bool wantMatchList = true;
    ApplyFindPageRange(win);
    bool cacheHit = win->findCountValid && win->findCountText && str::Eq(win->findCountText, text) &&
                    win->findCountMatchCase == win->findMatchCase &&
                    win->findCountMatchWholeWord == win->findMatchWholeWord && win->findCountEngine == engine &&
                    str::Eq(win->findCountRangeText, win->findPageRangeText) &&
                    (!wantMatchList || (wantSnippets ? win->findCountHasSnippets : len(win->findMatches) > 0));
    if (cacheHit) {
        // Matches are unchanged, but Find Next/Prev moved the active match
        ShowMatchCount(win);
    } else {
        StartFindCount(win, text, win->findMatchCase, win->findMatchWholeWord);
    }
}

// Rebuild the match-count list for the current term without starting a new
// interactive Find Next (opening Find after -search must not skip a hit).
void EnsureFindSnippets(MainWindow* win) {
    if (!win || win->findThread) {
        return;
    }
    TempStr text = CurrentFindTermTemp(win);
    if (len(text) == 0) {
        return;
    }
    UpdateMatchCount(win, text);
}

static void CancelPendingFind(MainWindow* win);
static bool JoinFindThread(MainWindow* win, bool hideMessage);

// navigate to and select a match, so Find Next/Prev and the n/m counter
// continue from there
void GoToFindMatch(MainWindow* win, int startPage, int startGlyph, int endPage, int endGlyph) {
    if (!win->IsDocLoaded()) {
        return;
    }
    DocController* md = BrowserFindCtrl(win);
    if (md) {
        // for markdown, startGlyph is the in-page match index
        win->browserFindCurrent = BrowserFindGlobalMatchIdx(win, startPage, startGlyph + 1);
        BrowserFindGotoMatch(win, md, startPage, startGlyph);
        return;
    }
    if (!win->AsFixed()) {
        return;
    }
    // Join an in-flight interactive find first: it drives dm->textSearch, which
    // we're about to mutate. Deliberately not AbortFinding(): the counting scan
    // has its own TextSearch and reads page text through the engine's locked
    // text cache.
    if (win->findThread || win->findDebouncePending) {
        CancelPendingFind(win);
        JoinFindThread(win, true);
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
    // differs from the last search text, so ShowSearchResult() must run first
    ShowSearchResult(win, &ts->result, true);
    // hand the selection to TextSearch as its "last result" so Find Next/Prev
    // continue from here
    ts->SetLastResult(ts);
    // ...and put the result back if SetText() dropped it. PaintAllFindMatches
    // only treats a match as the current one (selection color) when ts->result
    // is populated (issue #5889)
    if (ts->result.len == 0) {
        ts->StartAt(startPage, startGlyph);
        ts->SelectUpTo(endPage, endGlyph);
    }
    ShowMatchCount(win);
}

// progressCb on the document's TextSearch points at ftd. Drop it before ftd is
// deleted: a later search on the UI thread would call into freed memory.
static void DropFindProgressCb(MainWindow* win, FindThreadData* ftd) {
    if (!IsMainWindowValid(win)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSearch) {
        return;
    }
    ProgressUpdateCb& cb = dm->textSearch->progressCb;
    if ((cb.userData & ~ProgressUpdateCb::kDropsArgBit) != (uintptr_t)ftd) {
        return;
    }
    cb = {};
}

static void FindEndTask(FindEndTaskData* d) {
    auto* win = d->win;
    auto* ftd = d->ftd;
    auto* textSel = d->textSel;
    auto wasModifiedCanceled = d->wasModifiedCanceled;
    auto loopedAround = d->loopedAround;

    AutoDelete delData(d);
    DropFindProgressCb(win, ftd);
    if (!IsMainWindowValidAndNotClosing(win)) {
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
        // complete scan, so the n/m counter reflects every match even when the
        // incremental find gave up. (Runs only now that the find thread has
        // exited, so the two never scan the engine at once.)
        ClearSearchResult(win);
        ftd->HideUI(false, !wasModifiedCanceled);
        UpdateMatchCount(win, ftd->text);
    }
    SafeCloseThreadHandle(&win->findThread);
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
    auto* textSearch = dm->textSearch;
    auto* ctrl = win->ctrl;

    auto* engine = dm->GetEngine();
    engine->AddRef();
    AutoCall releaseEngine(SafeEngineRelease<EngineBase>, &engine);

    TextSel* rect;
    textSearch->progressCb = MkFunc1<FindThreadData, ProgressUpdateData*>(UpdateSearchProgress, ftd);
    textSearch->SetDirection(ftd->direction);
    if (ftd->wasModified || !ctrl->ValidPageNo(textSearch->GetCurrentPageNo()) ||
        !(bool)dm->GetPageInfo(textSearch->GetCurrentPageNo())->visibleRatio) {
        rect = textSearch->FindFirst(ctrl->CurrentPageNo(), ftd->text);
    } else {
        rect = textSearch->FindNext();
    }

    bool loopedAround = false;
    if (!win->findCancelled && !rect) {
        // With no further findings, start over (unless this was a new search from the beginning)
        int startPage = (TextSearch::Direction::Forward == ftd->direction) ? textSearch->RestrictFirst()
                                                                           : textSearch->RestrictLast();
        if (!ftd->wasModified || ctrl->CurrentPageNo() != startPage) {
            loopedAround = true;
            rect = textSearch->FindFirst(startPage, ftd->text);
        }
    }

    // wait for FindTextOnThread to return so that
    // FindEndTask closes the correct handle to
    // the current find thread
    while (!win->findThread) {
        SleepInMs(1);
    }

    auto* data = new FindEndTaskData;
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

// cancel a pending debounced find-as-you-type search
static void CancelPendingFind(MainWindow* win) {
    win->findDebouncePending = false;
    win->findDebounceLeftMs = 0;
}

// join the interactive find worker, which drives dm->textSearch. Leaves a
// counting scan running: that one has its own TextSearch, so only callers that
// mean to stop searching the document need AbortFinding()
static bool JoinFindThread(MainWindow* win, bool hideMessage) {
    bool res = false;
    if (win->findThread) {
        res = true;
        logf("JoinFindThread: setting win->findCancelled to true\n");
        win->findCancelled = true;
        JoinThread(&win->findThread, -1);
    }
    win->findCancelled = false;

    if (hideMessage) {
        bool didRemove = RemoveNotificationsForGroup(win, kNotifFindProgress);
        if (didRemove) {
            res = true;
        }
    }
    return res;
}

bool AbortFinding(MainWindow* win, bool hideMessage) {
    CancelPendingFind(win);
    AbortCount(win);
    return JoinFindThread(win, hideMessage);
}

// wasModified
//   if true, starting a search for new term
//   if false, searching for the next occurrence of previous term
// Callers may pass wasModified=false incorrectly (e.g. tab switch). If the term
// differs from TextSearch::lastText we force wasModified=true. Callers can
// still pass true for the same text (restart after match-case toggle, etc.).
void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, Str text, bool wasModified, bool showProgress) {
    if (!win) {
        return;
    }
    AbortFinding(win, false);
    if (len(text) == 0) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSearch) {
        return;
    }
    // FindThread runs on a worker thread and can't touch DisplayModel; do
    // the layout and the resync it needs here, before it starts
    EnsureFullLayout(dm);
    RememberFindQuery(text);
    if (ApplyFindPageRange(win)) {
        wasModified = true;
    }
    // Match SetText()'s normalization: strip one leading space (word-start)
    // so trailing/whole-word spaces still compare correctly.
    Str searchText = text;
    if (searchText && searchText.s[0] == ' ') {
        searchText = Str(searchText.s + 1, searchText.len - 1);
    }
    if (!str::Eq(searchText, dm->textSearch->lastText)) {
        wasModified = true;
    }
    // closing the find UI dropped the search position: start over from the
    // current page instead of continuing from nowhere
    if (len(dm->textSearch->pageText) == 0) {
        wasModified = true;
    }
    // A new term starts a search if the find UI did not already mark it.
    if (wasModified) {
        MarkSearchStart(win);
    }
    logf("FindTextOnThread: '%s' %s, wasModified %d\n", text,
         direction == TextSearch::Direction::Forward ? StrL("forward") : StrL("backward"), (int)wasModified);
    FindThreadData* ftd = new FindThreadData(win, direction, text, wasModified);
    ftd->ShowUI(showProgress);
    win->findThread = nullptr;
    auto fn = MkFunc0(FindThread, ftd);
    win->findThread = StartThread(fn, StrL("FindThread"));
    ftd->thread = win->findThread; // safe because only accessed on the ui thread
}

// A command-line search can target a session-restored tab that is not loaded
// yet (a lazy tab). Keep the newest request on that tab and start it once
// the controller is attached.
void StartSearchFromCommandLine(MainWindow* win, Str text) {
    if (!win || len(text) == 0) {
        return;
    }
    if (!win->IsDocLoaded()) {
        WindowTab* tab = win->CurrentTab();
        if (tab && tab->type == WindowTab::Type::Document) {
            str::ReplaceWithCopy(&tab->pendingFindText, text);
        }
        return;
    }
    // Command-line search should leave the same find UI visible as Ctrl+F,
    // with the search term ready for another search or navigation (#6067).
    ShowFindBar(win);
    win->searchStartMarked = false;
    FindEditSetText(win, text);
    if (DocController* browser = BrowserFindCtrl(win)) {
        BrowserFindStartSearch(win, browser);
        return;
    }
    FindTextOnThread(win, TextSearch::Direction::Forward, text, true, true);
}

// Consume a command-line search once its target tab is current and loaded.
void StartPendingSearch(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->pendingFindText) == 0) {
        return;
    }
    TempStr text = str::DupTemp(tab->pendingFindText);
    str::FreePtr(&tab->pendingFindText);
    StartSearchFromCommandLine(win, text);
}

void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, bool showProgress) {
    TempStr s = FindEditTextTemp(win);
    bool wasModified = FindEditIsModified(win);
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
    FindEditSetModified(win, false);
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
    if (!engine) {
        return;
    }
    int pageCount = engine->PageCount();
    if (fm.startPage < 1 || fm.endPage < 1 || fm.startPage > pageCount || fm.endPage > pageCount) {
        return;
    }
    TextSelection ts(engine);
    ts.StartAt(fm.startPage, fm.startGlyph);
    ts.SelectUpTo(fm.endPage, fm.endGlyph);
    for (int i = 0; i < ts.result.len; i++) {
        FindMatchPaintPageRect pr;
        pr.pageNo = ts.result.pages[i];
        pr.rect = ts.result.rects[i];
        VecAppend(out, pr);
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
            VecAppend(out, rc);
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
        VecAppend(gFindMatchPaintCache.entries, entry);
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
            VecAppend(out, rc);
        }
    }
}

// ng: orig's win->canvasRc is the canvas HWND's client rect, i.e. the origin
// of document coordinates. Here canvasRc sits below the menu bar and the tab
// strip, so the clip for document coordinates is the viewport.
static Rect CanvasClipRc(DisplayModel* dm) {
    return Rect(Point(), dm->GetViewPort().Size());
}

static void PaintCurrentFindMatch(MainWindow* win, DisplayModel* dm, TextSearch* ts, gp::PaintCtx* ctx) {
    if (!ts || ts->result.len == 0) {
        return;
    }
    ParsedColor* parsedCol = GetPrefsColor(gSettings->fixedPageUI.selectionColor);
    u8 alpha = GetAlpha(parsedCol->col);
    if (alpha == 0) {
        alpha = kSelectionDefaultAlpha;
    }
    Rect clipRc = CanvasClipRc(dm);
    Vec<Rect> currentRects;
    AppendTextSelScreenRects(dm, clipRc, &ts->result, currentRects);
    if (len(currentRects) > 0) {
        PaintTransparentRectangles(ctx, clipRc, currentRects, parsedCol->col, alpha);
    }
}

void PaintAllFindMatches(MainWindow* win, gp::PaintCtx* ctx) {
    if (!win->IsDocLoaded() || !win->AsFixed()) {
        return;
    }
    if (FindEditTextLen(win) == 0) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    // Matches/count cache are tied to the engine they were built for. After a
    // tab close or reload without InvalidateFindForDocumentChange, refuse to
    // map stale page/glyph coords onto a different document.
    void* engine = (void*)dm->GetEngine();
    if (win->findCountEngine && win->findCountEngine != engine) {
        ClearFindMatches(win);
        win->findCountValid = false;
        win->findCountEngine = nullptr;
        VecReset(win->findCountPositions);
        str::FreePtr(&win->findCountText);
        return;
    }
    TextSearch* ts = dm->textSearch;
    // After the find UI is closed, still highlight the active match so F3 /
    // FindNext navigation is visible (issue #5802). The full match list was
    // cleared on hide; only paint the current TextSearch hit.
    if (!IsFindUIVisible(win)) {
        PaintCurrentFindMatch(win, dm, ts, ctx);
        return;
    }
    if (!win->findCountValid && len(win->findMatches) == 0) {
        // count still running: at least highlight the current match
        PaintCurrentFindMatch(win, dm, ts, ctx);
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

    ParsedColor* parsedCol = GetPrefsColor(gSettings->fixedPageUI.selectionColor);
    u8 alpha = GetAlpha(parsedCol->col);
    if (alpha == 0) {
        alpha = kSelectionDefaultAlpha;
    }

    Rect clipRc = CanvasClipRc(dm);
    Vec<Rect> otherRects;
    Vec<Rect> currentRects;
    Vec<FindMatchPaintPageRect>& positions = gFindMatchPaintCache.positions;
    for (int i = 0; i < len(gFindMatchPaintCache.entries); i++) {
        const FindMatchPaintRects& entry = gFindMatchPaintCache.entries[i];
        Vec<Rect>& out = (entry.key == currentKey) ? currentRects : otherRects;
        AppendPageRectsToScreen(dm, clipRc, &positions[entry.firstPos], entry.len, out);
    }

    if (len(otherRects) > 0) {
        PaintTransparentRectangles(ctx, clipRc, otherRects, kFindOtherMatchColor, alpha);
    }
    if (len(currentRects) == 0 && ts && ts->result.len > 0) {
        AppendTextSelScreenRects(dm, clipRc, &ts->result, currentRects);
    }
    if (len(currentRects) > 0) {
        PaintTransparentRectangles(ctx, clipRc, currentRects, parsedCol->col, alpha);
    }
}

// --- the forward-search mark (step 18) --------------------------------------

void PaintForwardSearchMark(MainWindow* win, gp::PaintCtx* ctx) {
    ReportIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();
    int pageNo = win->fwdSearchMark.page;
    PageInfo* pageInfo = dm->GetPageInfo(pageNo);
    if (!pageInfo || 0.0 == pageInfo->visibleRatio) {
        return;
    }

    int hiLiWidth = gSettings->forwardSearch.highlightWidth;
    int hiLiOff = gSettings->forwardSearch.highlightOffset;

    // Draw the rectangles highlighting the forward search results
    Vec<Rect> rects;
    for (int i = 0; i < len(win->fwdSearchMark.rects); i++) {
        Rect rect = win->fwdSearchMark.rects[i];
        rect = dm->CvtToScreen(pageNo, ToRectF(rect));
        if (hiLiOff > 0) {
            float zoom = dm->GetZoomReal(pageNo);
            rect.x = std::max(pageInfo->pageOnScreen.x, 0) + (int)((float)hiLiOff * zoom);
            rect.dx = (int)((hiLiWidth > 0 ? hiLiWidth : 15.0) * zoom);
            rect.y -= 4;
            rect.dy += 8;
        }
        VecAppend(rects, rect);
    }

    u8 alpha =
        (u8)(0x5f * 1.0f * (float)(kHideFwdSearchMarkSteps - win->fwdSearchMark.hideStep) / kHideFwdSearchMarkSteps);
    ParsedColor* parsedCol = GetPrefsColor(gSettings->forwardSearch.highlightColor);
    PaintTransparentRectangles(ctx, CanvasClipRc(dm), rects, parsedCol->col, alpha);
}

// ng: orig's kHideFwdSearchMarkTimerID handler in Canvas.cpp
void ForwardSearchMarkTick(MainWindow* win, int elapsedMs) {
    if (!win || win->fwdSearchMark.hideLeftMs < 0) {
        return;
    }
    win->fwdSearchMark.hideLeftMs -= elapsedMs;
    if (win->fwdSearchMark.hideLeftMs > 0) {
        return;
    }
    win->fwdSearchMark.hideStep++;
    if (win->fwdSearchMark.hideStep >= kHideFwdSearchMarkSteps) {
        win->fwdSearchMark.hideLeftMs = -1;
        win->fwdSearchMark.show = false;
    } else {
        win->fwdSearchMark.hideLeftMs = kHideFwdSearchMarkDecayIntervalInMs;
    }
    AppShellInvalidate(win);
}

// Replace in 'pattern' the macros %f %l %c by 'path', 'line' and 'col'
static TempStr BuildOpenFileCmdTemp(Str pattern, Str path, int line, int col) {
    str::Builder cmdline;
    str::BuilderReserve(cmdline, 256);

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

// ng: orig runs the editor with LaunchProcessInDir(cmdLine, appDir). Off
// Windows there is no CreateProcess wrapper for a whole command line, so the
// first token is the program and the rest its arguments.
static bool LaunchInverseSearchCmd(Str cmdLine) {
#if OS_WIN
    // resolve relative paths with relation to SumatraPDF.exe's directory
    TempStr appDir = GetSelfExeDirTemp();
    AutoCloseHandle process(LaunchProcessInDir(cmdLine, appDir));
    return process != nullptr;
#else
    Str rest = cmdLine;
    TempStr exe;
    if (rest.len > 0 && rest.s[0] == '"') {
        int endIdx = str::IndexOfChar(Str(rest.s + 1, rest.len - 1), '"');
        if (endIdx < 0) {
            return false;
        }
        exe = str::DupTemp(Str(rest.s + 1, endIdx));
        rest = Str(rest.s + endIdx + 2, rest.len - endIdx - 2);
    } else {
        int spaceIdx = str::IndexOfChar(rest, ' ');
        if (spaceIdx < 0) {
            exe = str::DupTemp(rest);
            rest = {};
        } else {
            exe = str::DupTemp(Str(rest.s, spaceIdx));
            rest = Str(rest.s + spaceIdx + 1, rest.len - spaceIdx - 1);
        }
    }
    while (rest.len > 0 && rest.s[0] == ' ') {
        rest = Str(rest.s + 1, rest.len - 1);
    }
    return LaunchFileShell(exe, rest);
#endif
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
    VecReset(win->fwdSearchMark.rects);
    AppShellInvalidate(win);

    // On double-clicking error message will be shown to the user
    // if the PDF does not have a synchronization file
    if (!dm->pdfSync) {
        Str path = tab->filePath;
        int err = Synchronizer::Create(path, dm->GetEngine(), &dm->pdfSync);
        if (err == PDFSYNCERR_SYNCFILE_NOTFOUND) {
            // We used to warn that "No synchronization file found" at this
            // point if gSettings->enableTeXEnhancements is set; we no longer
            // so do because a double-click has several other meanings
            // (selecting a word or an image, navigating quickly using links)
            // and showing an unrelated warning in all those cases seems wrong
            return false;
        }
        if (err != PDFSYNCERR_SUCCESS) {
            NotificationCreateArgs args;
            args.win = win;
            args.msg = Tr("Synchronization file cannot be opened");
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
        args.win = win;
        args.msg = Tr("No synchronization info at this position");
        ShowNotification(args);
        return true;
    }

    Str inverseSearch = gSettings->inverseSearchCmdLine;
    if (len(inverseSearch) == 0) {
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
    args.win = win;
    args.plainText = true;
    args.msg = Tr("Cannot start the inverse search command. Check its command line in Settings.");
    if (len(cmdLine) > 0) {
        if (!LaunchInverseSearchCmd(cmdLine)) {
            ShowNotification(args);
        }
    } else if (gSettings->enableTeXEnhancements) {
        ShowNotification(args);
    }

    return true;
}

// Build a page-space box to flash after an internal jump. FitR dests already
// have a rectangle. /XYZ is a point: a short strip at dest Y from dest X to
// the right of the page, like the LaTeX forward-search mark. Page-level
// /Fit with no coordinates returns empty (nothing useful to highlight).
static bool LinkDestHighlightRect(DisplayModel* dm, int pageNo, RectF dest, Rect* out) {
    EngineBase* engine = dm->GetEngine();
    if (!engine || !out) {
        return false;
    }
    RectF box = engine->PageMediabox(pageNo);
    if (box.IsEmpty()) {
        return false;
    }
    bool hasX = dest.x != kDestUseDefault;
    bool hasY = dest.y != kDestUseDefault;
    bool hasWH = dest.dx != kDestUseDefault && dest.dy != kDestUseDefault && dest.dx > 1.f && dest.dy > 1.f;
    if (hasWH) {
        Rect r = dest.Round();
        if (r.IsEmpty()) {
            return false;
        }
        *out = r;
        return true;
    }
    if (!hasX && !hasY) {
        return false;
    }
    float x = hasX ? dest.x : box.x;
    float y = hasY ? dest.y : box.y;
    float lineH = 20.f;
    float y0 = y - 2.f;
    if (y0 < box.y) {
        y0 = box.y;
    }
    float w = (box.x + box.dx) - x;
    if (w < 8.f) {
        x = box.x;
        w = box.dx;
    }
    Rect r = RectF{x, y0, w, lineH}.Round();
    if (r.IsEmpty()) {
        return false;
    }
    *out = r;
    return true;
}

// Flash the same mark used for LaTeX forward search at an internal-link dest
// (issues #1085, #5945). Always fades; ForwardSearch.HighlightPermanent stays
// a SyncTeX-only option. Held longer than SyncTeX (kHideLinkDestMarkDelayInMs)
// so the mark is still visible after the page jump.
void ShowLinkDestHighlight(MainWindow* win, int pageNo, RectF dest) {
    if (!win || !win->AsFixed()) {
        return;
    }
    VecReset(win->fwdSearchMark.rects);
    win->fwdSearchMark.show = false;
    if (!gSettings || !gSettings->highlightLinkDestination) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm->ValidPageNo(pageNo)) {
        return;
    }
    Rect hl;
    if (!LinkDestHighlightRect(dm, pageNo, dest, &hl)) {
        return;
    }
    VecAppend(win->fwdSearchMark.rects, hl);
    win->fwdSearchMark.page = pageNo;
    win->fwdSearchMark.show = true;
    win->fwdSearchMark.hideStep = 0;
    win->fwdSearchMark.hideLeftMs = kHideLinkDestMarkDelayInMs;
    AppShellInvalidate(win);
}

TempStr LinkDestHighlightResultTemp(int* exitCodeOut) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    int n = len(win->fwdSearchMark.rects);
    return finish(0, fmt("OK show=%d page=%d nrects=%d enabled=%d", win->fwdSearchMark.show ? 1 : 0,
                         win->fwdSearchMark.page, n, gSettings && gSettings->highlightLinkDestination ? 1 : 0));
}

// Show the result of a PDF forward-search synchronization (initiated by a DDE
// command or by -forward-search)
void ShowForwardSearchResult(MainWindow* win, Str fileName, int line, int /* col */, int ret, int page,
                             Vec<Rect>& rects) {
    ReportIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();
    VecReset(win->fwdSearchMark.rects);
    const PageInfo* pi = dm->GetPageInfo(page);
    if ((ret == PDFSYNCERR_SUCCESS) && (len(rects) > 0) && (nullptr != pi)) {
        // remember the position of the search result for drawing the rect later on
        win->fwdSearchMark.rects = rects;
        win->fwdSearchMark.page = page;
        win->fwdSearchMark.show = true;
        win->fwdSearchMark.hideStep = 0;
        win->fwdSearchMark.hideLeftMs = gSettings->forwardSearch.highlightPermanent ? -1 : kHideFwdSearchMarkDelayInMs;

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
        dm->ShowResultRectToScreen(&res);
        AppShellInvalidate(win);
        // ng: orig restores the frame when it is minimized (IsIconic)
        AppShellActivateWindow(win);
        return;
    }

    TempStr buf;
    NotificationCreateArgs args{};
    args.win = win;
    // several of these embed a file name read from the .synctex / .pdfsync file
    args.plainText = true;
    if (ret == PDFSYNCERR_SYNCFILE_NOTFOUND) {
        args.msg = Tr("No synchronization file found");
    } else if (ret == PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED) {
        args.msg = Tr("Synchronization file cannot be opened");
    } else if (ret == PDFSYNCERR_INVALID_PAGE_NUMBER) {
        buf = fmt(Tr("Page %u does not exist").s, page);
    } else if (ret == PDFSYNCERR_NO_SYNC_AT_LOCATION) {
        args.msg = Tr("No synchronization info at this position");
    } else if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE) {
        buf = fmt(Tr("Unknown source file (%s)").s, fileName);
    } else if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE) {
        buf = fmt(Tr("Source file %s has no synchronization point").s, fileName);
    } else if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE || ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD) {
        buf = fmt(Tr("No result found around line %u in file %s").s, line, fileName);
    }
    if (buf) {
        args.msg = buf;
        ShowNotification(args);
    }
}

// ng: what the scripted tests read back instead of driving orig's -dbg-control
TempStr FindStateResultTemp(MainWindow* win) {
    str::Builder out;
    if (!win) {
        out.Append(StrL("no-window\n"));
        return ToStrTemp(out);
    }
    DisplayModel* dm = win->AsFixed();
    TextSearch* ts = dm ? dm->textSearch : nullptr;
    out.Append(fmt("visible=%d text=%s status=%s\n", IsFindBarVisible(win) ? 1 : 0, FindEditTextTemp(win),
                   win->findBar ? win->findBar->status : Str{}));
    out.Append(fmt("matchCase=%d matchWholeWord=%d matches=%d countValid=%d\n", win->findMatchCase ? 1 : 0,
                   win->findMatchWholeWord ? 1 : 0, len(win->findMatches), win->findCountValid ? 1 : 0));
    out.Append(fmt("editDx=%d\n", win->findBar ? win->findBar->editDx : 0));
    if (ts) {
        out.Append(fmt("current=page %d glyph %d rects %d\n", ts->startPage, ts->startGlyph, ts->result.len));
    }
    // page of the active hit (0: none), whether a search is still running
    // and the page in view (orig's TestFindUiState fields)
    int hitPage = (ts && ts->result.len > 0) ? ts->result.pages[0] : 0;
    bool busy = win->findThread || win->findCountThread || win->findDebouncePending;
    int page = win->ctrl ? win->ctrl->CurrentPageNo() : 0;
    out.Append(fmt("hitPage=%d busy=%d page=%d\n", hitPage, busy ? 1 : 0, page));
    return ToStrTemp(out);
}

#if OS_WIN

// ─── DDE commands handling (orig's, Windows only) ─────────────────────────

bool gIsStartup = false;
StrVec gDdeOpenOnStartup;

// Prefer the MainWindow that owns hwnd when it already has pdfFile open
// (any tab); otherwise fall back to the global FindMainWindowByFile.
static MainWindow* FindDdeTargetWindow(HWND hwnd, Str pdfFile, bool focusTab) {
    MainWindow* prefer = AppShellWindowFromHwnd(hwnd);
    if (prefer) {
        WindowTab* tab = FindTabByFilePath(pdfFile, prefer);
        if (tab) {
            if (focusTab) {
                SelectTabInWindow(tab);
            }
            return prefer;
        }
    }
    return FindMainWindowByFile(pdfFile, focusTab);
}

// ng: orig tracks the window the user last worked in with gLastActiveFrameHwnd;
// here the foreground window answers the same question
static MainWindow* LastActiveWindow() {
    MainWindow* win = AppShellWindowFromHwnd(GetForegroundWindow());
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    return win;
}

// the window an Open with newWindow set should land in: an existing empty one,
// or a brand new one
static MainWindow* WindowForNewWindowOpen() {
    for (MainWindow* w : gWindows) {
        if (!HasOpenedDocuments(w)) {
            return w;
        }
    }
    return CreateAndShowMainWindow(nullptr);
}

// Parse a DDE quoted string starting at off (content after the opening ").
// Stops at an unescaped "; treats "" as a literal quote. Sets *endOff past
// the closing quote. Returns false on missing closing quote.
static bool ParseDdeQuoted(Str cmd, int off, TempStr* out, int* endOff) {
    str::Builder b;
    int i = off;
    while (i < cmd.len) {
        char c = cmd.s[i];
        if (c == '"') {
            if (i + 1 < cmd.len && cmd.s[i + 1] == '"') {
                b.AppendChar('"');
                i += 2;
                continue;
            }
            *endOff = i + 1;
            *out = ToStrTemp(b);
            return true;
        }
        b.AppendChar(c);
        i++;
    }
    return false;
}

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
    Str next = str::Parse(cmd, R"([ForwardSearch("%s",%? "%s",%u,%u)])", &pdfFile, &srcFile, &line, &col);
    if (str::IsNull(next)) {
        next = str::Parse(cmd, R"([ForwardSearch("%s",%? "%s",%u,%u,%u,%u)])", &pdfFile, &srcFile, &line, &col,
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
            win = LoadDocument(!newWindow ? win : nullptr, pdfFile);
        } else if (!win->IsDocLoaded()) {
            ReloadDocument(win, false);
        }
    } else {
        // check if any opened PDF has sync information for the source file
        win = FindMainWindowBySyncFile(srcFile, true);
        if (win && newWindow) {
            win = LoadDocument(nullptr, win->CurrentTab()->filePath);
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
Quotes inside the term/path are escaped as "" (standard DDE-style).
*/
static Str HandleSearchCmd(HWND hwnd, Str cmd, bool* ack) {
    // Manual parse so search terms may contain " via "" escapes; str::Parse
    // stops at the first " and cannot express that.
    Str kPrefix = StrL("[Search(\"");
    if (!str::TrimPrefix(cmd, kPrefix)) {
        return {};
    }
    int endFile = 0;
    TempStr pdfFile;
    if (!ParseDdeQuoted(cmd, 0, &pdfFile, &endFile)) {
        return {};
    }
    // expect "," after the closing quote of the path
    if (endFile >= cmd.len || cmd.s[endFile] != ',' || endFile + 1 >= cmd.len || cmd.s[endFile + 1] != '"') {
        return {};
    }
    int endTerm = 0;
    TempStr term;
    if (!ParseDdeQuoted(cmd, endFile + 2, &term, &endTerm)) {
        return {};
    }
    if (endTerm >= cmd.len || cmd.s[endTerm] != ']') {
        return {};
    }
    Str next = Str(cmd.s + endTerm + 1, cmd.len - endTerm - 1);
    if (len(term) == 0) {
        return next;
    }
    MainWindow* win = FindDdeTargetWindow(hwnd, pdfFile, true);
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
static Str HandleGotoPageWordCmd(HWND hwnd, Str cmd, bool* ack) {
    TempStr pdfFile;
    TempStr term;
    int page = 0;
    Str next = str::Parse(cmd, R"([GotoPageWord("%s",%d,"%s")])", &pdfFile, &page, &term);
    if (str::IsNull(next)) {
        return {};
    }
    MainWindow* win = FindDdeTargetWindow(hwnd, pdfFile, true);
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
    logf("HandleOpenCmd: '%s', newWindow: %d, setFocus: %d, forceRefresh: %d, inCurrentTab: %d\n", filePath, newWindow,
         setFocus, forceRefresh, inCurrentTab);
    // on startup this runs while the command line is still being opened, so
    // queue the files and load them in sequence afterwards
    if (gIsStartup) {
        if (FindTabByFilePath(filePath)) {
            return next;
        }
        AppendIfNotExists(&gDdeOpenOnStartup, filePath);
        return next;
    }

    if (newWindow != 0 && inCurrentTab != 0) {
        inCurrentTab = 0;
        logf("HandleOpenCmd: setting inCurrentTab to 0 because newWindow != 0\n");
    }

    bool focusTab = (newWindow == 0);

    // intelligently pick a window or create one
    MainWindow* win = nullptr;
    int nWindows = len(gWindows);
    if (newWindow > 0) {
        win = WindowForNewWindowOpen();
    }
    bool doLoad = true;
    if (!win) {
        win = FindMainWindowByFile(filePath, focusTab);
        if (win) {
            doLoad = false;
            if (!win->IsDocLoaded()) {
                ReloadDocument(win, false);
                forceRefresh = 0;
            }
        }
    }
    if (!win) {
        win = (nWindows == 1) ? gWindows[0] : LastActiveWindow();
    }

    if (doLoad) {
        LoadReuse reuse = inCurrentTab ? LoadReuse::CurrentTab : LoadReuse::NewTab;
        win = LoadDocument(win, filePath, LoadPrefs::Save, reuse);
        if (!win) {
            logf("HandleOpenCmd: LoadDocument() for '%s' failed\n", filePath);
        }
    }

    if (win) {
        if (forceRefresh) {
            ReloadDocument(win, true);
        }
        if (setFocus) {
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
static Str HandleGotoCmd(HWND hwnd, Str cmd, bool* ack) {
    TempStr pdfFile, destName;
    Str next = str::Parse(cmd, R"([GotoNamedDest("%s",%? "%s")])", &pdfFile, &destName);
    if (str::IsNull(next)) {
        return {};
    }

    MainWindow* win = FindDdeTargetWindow(hwnd, pdfFile, true);
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
static Str HandlePageCmd(HWND hwnd, Str cmd, bool* ack) {
    TempStr pdfFile;
    uint page = 0;
    Str next = str::Parse(cmd, "[GotoPage(\"%S\",%u)]", &pdfFile, &page);
    if (str::IsNull(next)) {
        return {};
    }

    MainWindow* win = FindDdeTargetWindow(hwnd, pdfFile, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win, false);
        if (!win->IsDocLoaded()) {
            return next;
        }
    }

    if (!win->ctrl->ValidPageNo((int)page)) {
        return next;
    }

    win->ctrl->GoToPage((int)page, true);
    *ack = true;
    win->Focus();
    return next;
}

/*
Set view mode and zoom level DDE command

[SetView("<filepath>", "<view mode>", <zoom level>[, <scrollX>, <scrollY>])]

eg: [SetView("c:\file.pdf", "book view", -2)]

use -1 for kZoomFitPage, -2 for kZoomFitWidth, -3 for kZoomFitContent, -6 for kZoomFitHeight
*/
static Str HandleSetViewCmd(HWND hwnd, Str cmd, bool* ack) {
    TempStr filePath, viewMode;
    float zoom = kInvalidZoom;
    Point scroll(-1, -1);
    Str next = str::Parse(cmd, R"([SetView("%s",%? "%s",%f)])", &filePath, &viewMode, &zoom);
    if (str::IsNull(next)) {
        next =
            str::Parse(cmd, R"([SetView("%s",%? "%s",%f,%d,%d)])", &filePath, &viewMode, &zoom, &scroll.x, &scroll.y);
    }
    if (str::IsNull(next)) {
        return {};
    }

    MainWindow* win = FindDdeTargetWindow(hwnd, filePath, true);
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
Show the document in presentation or fullscreen mode, like -presentation /
-fullscreen do for a file opened on the command line.

[Presentation("<pdffile>")]
[FullScreen("<pdffile>")]
*/
static Str HandleFullScreenCmd(HWND hwnd, Str cmd, bool* ack) {
    TempStr filePath;
    bool presentation = true;
    Str next = str::Parse(cmd, R"([Presentation("%s")])", &filePath);
    if (str::IsNull(next)) {
        presentation = false;
        next = str::Parse(cmd, R"([FullScreen("%s")])", &filePath);
    }
    if (str::IsNull(next)) {
        return {};
    }

    MainWindow* win = FindDdeTargetWindow(hwnd, filePath, true);
    if (!win || !win->IsDocLoaded()) {
        return next;
    }
    SwitchToFullScreen(win, presentation);
    *ack = true;
    return next;
}

/*
Open new window.

[NewWindow]
*/
static Str HandleNewWindowCmd(Str cmd, bool* ack) {
    Str kNewWindowCmd = StrL("[NewWindow]");
    if (!str::TrimPrefix(cmd, kNewWindowCmd)) {
        return {};
    }
    logf("HandleNewWindowCmd\n");
    CreateAndShowMainWindow(nullptr);
    *ack = true;
    return cmd;
}

/*
[GetFileState("<filepath>")]
[GetFileState()]
[GetFileState]
Info about document <filepath>, or the currently viewed one when no path is
given, as "key: value" lines. zoom is a percentage, or -1 = fit page,
-2 = fit width, -3 = fit content, -6 = fit height (as in SetView).
*/
static Str HandleGetFileStateCmd(Str cmd, bool* ack, str::Builder& res) {
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
        win = LastActiveWindow();
    }
    if (!win) {
        res.Append(StrL("error: no opened file"));
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win, false);
        if (!win->IsDocLoaded()) {
            res.Append(StrL("error: file not loaded"));
            return next;
        }
    }

    DocController* ctrl = win->ctrl;
    Str docPath = ctrl->GetFilePath();
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
    MainWindow* win = AppShellWindowFromHwnd(hwnd);
    if (!win) {
        logf("HandleCmdCommand: not executing DDE because MainWindow for hwnd 0x%p not found\n", hwnd);
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

    logf("HandleCmdCommand: sending %d (%s) command\n", idToSend, cmdContent);
    ExecuteCmd(win, idToSend);
    *ack = true;
    return next;
}

// returns true if did handle a message
// ng: orig also has [ForwardSearch(...)], which needs the forward-search mark
// this port doesn't draw yet
static bool HandleExecuteCmds(HWND hwnd, Str cmd) {
    bool didHandle = false;
    while (cmd) {
        logf("HandleExecuteCmds: '%s'\n", cmd);

        Str nextCmd = HandleOpenCmd(cmd, &didHandle);
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleGotoCmd(hwnd, cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandlePageCmd(hwnd, cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleSetViewCmd(hwnd, cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleFullScreenCmd(hwnd, cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleSearchCmd(hwnd, cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleSyncCmd(cmd, &didHandle);
        }
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleGotoPageWordCmd(hwnd, cmd, &didHandle);
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

static bool HandleRequestCmds(Str cmd, str::Builder& rsp) {
    bool didHandle = false;
    while (cmd) {
        logf("HandleRequestCmds: '%s'\n", cmd);

        Str nextCmd = HandleGetFileStateCmd(cmd, &didHandle, rsp);
        if (str::IsNull(nextCmd)) {
            nextCmd = HandleGetOpenFilesCmd(cmd, &didHandle, rsp);
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
    if (fmt != CF_TEXT && fmt != CF_UNICODETEXT) {
        logf("OnDDERequest: invalid fmt '%d'\n", (int)fmt);
        return 0;
    }
    ATOM a = HIWORD(lp);
    TempStr cmd = AtomToStrTemp(a);
    if (len(cmd) == 0) {
        return 0;
    }

    str::Builder rsp;
    bool didHandle = HandleRequestCmds(cmd, rsp);
    if (!didHandle) {
        rsp.Reset(StrL("error: unknown command"));
    }

    void* data;
    int cbData;
    int cch = 0;
    if (fmt == CF_TEXT) {
        data = (void*)ToStr(rsp).s;
        cbData = len(rsp) + 1;
    } else {
        WCHAR* tmp = CWStrTemp(ToStr(rsp), cch);
        data = (void*)tmp;
        cbData = (cch + 1) * 2;
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
    memcpy(res + cbDdeData, data, (size_t)cbData);

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
    ATOM aServer = GlobalAddAtomW(kSumatraDdeServer);
    ATOM aTopic = GlobalAddAtomW(kSumatraDdeTopic);

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

struct OpenManyCopyDataAsync {
    StrVec paths;
    HWND hwnd;
    u32 newWindow;
};

static void OpenManyCopyDataAsyncRun(OpenManyCopyDataAsync* d) {
    MainWindow* win = d->newWindow ? WindowForNewWindowOpen() : AppShellWindowFromHwnd(d->hwnd);
    if (!win) {
        win = LastActiveWindow();
    }
    if (win) {
        win->Focus();
        for (Str path : d->paths) {
            LoadDocument(win, path);
        }
    }
    delete d;
}

static void OpenCopyDataAsyncRun(OpenCopyDataAsync* d) {
    // Pick a target window the same way HandleOpenCmd would, then load. We are
    // off the sender's clock: it already returned from SendMessageW.
    MainWindow* win = nullptr;
    if (d->newWindow) {
        win = WindowForNewWindowOpen();
    } else {
        win = FindMainWindowByFile(d->path, true);
        if (win) {
            // Already open: just focus (matches activateExisting).
            win->Focus();
            str::Free(d->path);
            delete d;
            return;
        }
        win = LastActiveWindow();
    }
    // Match the legacy DDE Open(..., setFocus=1) behavior used by
    // shell/reuseInstance launches: opening into an existing instance should
    // bring that window to the foreground.
    if (win) {
        win->Focus();
        LoadDocument(win, d->path);
    }

    str::Free(d->path);
    delete d;
}

LRESULT OnCopyData(HWND hwnd, WPARAM wp, LPARAM lp) {
    COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lp;
    if (!cds || wp) {
        return FALSE;
    }

    if (HandleExplorerQuickLookCopyData(cds)) {
        return TRUE;
    }

    if (cds->dwData == kCopyDataOpen) {
        // Simple-open fast path used by the reuseInstance handshake: the
        // sibling SumatraPDF that Explorer just spawned is blocked in
        // SendMessageW. Copy the path out, post an async task, return
        // immediately so the sender unblocks and exits.
        if (cds->cbData < sizeof(SumatraOpenCopyData) + 1) {
            return FALSE;
        }
        const auto* data = (const SumatraOpenCopyData*)cds->lpData;
        size_t pathMax = cds->cbData - sizeof(SumatraOpenCopyData);
        Str pathZ = Str((char*)(const u8*)(data + 1), (int)pathMax);
        // require null-terminator within bounds
        if (strnlen_s(pathZ.s, pathMax) >= pathMax) {
            return FALSE;
        }
        // during startup the message pump can deliver COPYDATA opens; match
        // HandleOpenCmd and queue them instead of racing the command line
        if (gIsStartup) {
            TempStr path = path::NormalizeTemp(pathZ);
            if (!FindTabByFilePath(path)) {
                AppendIfNotExists(&gDdeOpenOnStartup, path);
            }
            return TRUE;
        }
        auto* d = new OpenCopyDataAsync;
        d->path = str::Dup(pathZ);
        d->newWindow = data->newWindow;
        auto fn = MkFunc0<OpenCopyDataAsync>(OpenCopyDataAsyncRun, d);
        uitask::Post(fn, "OnCopyData/Open");
        return TRUE;
    }

    if (cds->dwData == kCopyDataOpenMany) {
        if (cds->cbData < sizeof(SumatraOpenManyCopyData) + 1) {
            return FALSE;
        }
        const auto* data = (const SumatraOpenManyCopyData*)cds->lpData;
        if (data->pathCount == 0) {
            return FALSE;
        }
        const char* s = (const char*)(data + 1);
        size_t bytesLeft = cds->cbData - sizeof(*data);
        StrVec paths;
        for (u32 i = 0; i < data->pathCount; i++) {
            size_t pathLen = strnlen_s(s, bytesLeft);
            if (pathLen >= bytesLeft) {
                return FALSE;
            }
            paths.Append(Str(s, (int)pathLen));
            s += pathLen + 1;
            bytesLeft -= pathLen + 1;
        }
        if (gIsStartup) {
            for (Str path : paths) {
                TempStr normalized = path::NormalizeTemp(path);
                if (!FindTabByFilePath(normalized)) {
                    AppendIfNotExists(&gDdeOpenOnStartup, normalized);
                }
            }
            return TRUE;
        }
        auto* d = new OpenManyCopyDataAsync;
        d->paths = paths;
        d->hwnd = hwnd;
        d->newWindow = data->newWindow;
        auto fn = MkFunc0<OpenManyCopyDataAsync>(OpenManyCopyDataAsyncRun, d);
        uitask::Post(fn, "OnCopyData/OpenMany");
        return TRUE;
    }

    if (cds->dwData == kCopyDataDdeW) {
        int cmdCch = (int)(cds->cbData / sizeof(WCHAR));
        if (cmdCch == 0 || ((WCHAR*)cds->lpData)[cmdCch - 1] != 0) {
            return FALSE;
        }
        WStr cmdW((WCHAR*)cds->lpData, cmdCch - 1);
        // legacy DDE grammar: callers expect synchronous handling
        TempStr cmd = ToUtf8Temp(cmdW);
        bool didHandle = HandleExecuteCmds(hwnd, cmd);
        return didHandle ? TRUE : FALSE;
    }

    return FALSE;
}

void LoadDdeOpenOnStartup(MainWindow* win) {
    int n = len(gDdeOpenOnStartup);
    if (n == 0) {
        return;
    }
    logf("Loading %d documents queued by dde open\n", n);
    SortNatural(&gDdeOpenOnStartup);
    for (Str path : gDdeOpenOnStartup) {
        if (FindTabByFilePath(path)) {
            continue;
        }
        LoadDocument(win, path);
    }
    gDdeOpenOnStartup.Reset();
}

#endif // OS_WIN
