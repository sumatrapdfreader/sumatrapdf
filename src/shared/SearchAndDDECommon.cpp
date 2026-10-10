/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "AppSettings.h"
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
#include "ExplorerQuickLook.h"
#include "Selection.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Favorites.h"
#include "Translations.h"
#include "Version.h"
#include "SearchAndDDE.h"
#include "SearchAndDDECommon.h"

StrVec gFindHistory;

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

// the controller if the current document is rendered in a webview that
// supports our in-page find (chm / markdown with a WebView2 backend: native
// find bar + highlighting driven from JS injected into the webview)
DocController* BrowserFindCtrl(MainWindow* win) {
    DocController* ctrl = win->ctrl;
    if (ctrl && ctrl->CanFindInPage()) {
        return ctrl;
    }
    return nullptr;
}

// A find session's first real search records the view it starts from: as the
// session-only "/" favorite (#5862) and as a nav point, so Back returns there
// even after find-as-you-type moved through intermediate matches (#6230).
// The session ends when the find UI is closed or reopened from the document.
void MarkSearchStart(MainWindow* win) {
    if (win->searchStartMarked) {
        return;
    }
    win->searchStartMarked = true;
    SetSearchStartFavorite(win);
    if (DisplayModel* dm = win->AsFixed()) {
        dm->AddNavPoint();
    }
}

// index into win->findMatches of the in-page match pageCur (1-based) on
// pageNo, or -1. findMatches is in (page, in-page index) order
int BrowserFindGlobalMatchIdx(MainWindow* win, int pageNo, int pageCur) {
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

// Match SetText()'s normalization: strip one leading space (word-start) so a
// trailing/whole-word space still compares as the same term.
Str FindTermWithoutWordStartSpace(Str text) {
    if (text && len(text) > 0 && text.s[0] == ' ') {
        return Str(text.s + 1, text.len - 1);
    }
    return text;
}

// jump to the idxInPage-th match on pageNo: directly if that page is showing,
// otherwise navigate there and re-run the in-page find once it has loaded
void BrowserFindGotoMatch(MainWindow* win, DocController* md, int pageNo, int idxInPage) {
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
    if (win->AsFixed()->GetEngine()->isImageCollection) {
        return false;
    }
    return true;
}

// run the actual incremental search; assumes there is non-empty find text
void StartIncrementalFind(MainWindow* win) {
    DocController* md = BrowserFindCtrl(win);
    if (md) {
        BrowserFindStartSearch(win, md); // sets search-start mark
        return;
    }
    // find-as-you-type is an intentional search start even when Edit_GetModify
    // is false (e.g. Ctrl+F copied selection via HwndSetText after SetLastResult)
    MarkSearchStart(win);
    // the full-document count (n/m + results list) is kicked from FindEndTask,
    // after this find thread exits, so the two never touch the engine's text
    // extraction concurrently (mupdf isn't safe for that)
    FindTextOnThread(win, TextSearch::Direction::Forward, false);
}

// find-as-you-type: called when the find bar's edit text changes. Instead of
// searching on every keystroke, (re)arm a debounce timer; the search starts a
// short while after the user stops typing (issue #4626).
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

u64 MatchKey(int page, int offset) {
    return ((u64)(u32)page << 32) | (u32)offset;
}

// The scan starts at the page that was current when it began and wraps around,
// so it produces matches out of document order (e.g. 89, 104, 47). Both the
// results list and the "n / m" counter present matches in document order, so
// re-sort by (page, glyph) as matches are installed. MatchKey packs page into
// the high half, so sorting the u64 keys sorts by (page, glyph) too.
int CmpFindMatchByPos(const FindMatch* a, const FindMatch* b) {
    if (a->startPage != b->startPage) {
        return a->startPage - b->startPage;
    }
    return a->startGlyph - b->startGlyph;
}

int CmpMatchKey(const u64* a, const u64* b) {
    if (*a == *b) {
        return 0;
    }
    return (*a < *b) ? -1 : 1;
}

// 1-based index of `key` within the positions cache, or 0 if not found.
// positions are in document order, but a linear lookup is cheap enough here
// (n <= kMaxFindCount)
int MatchIndexInCache(MainWindow* win, u64 key) {
    Vec<u64>& pos = win->findCountPositions;
    int n = len(pos);
    for (int i = 0; i < n; i++) {
        if (pos[i] == key) {
            return i + 1;
        }
    }
    return 0;
}

// free the cached per-match snippets (win->findMatches)
void ClearFindMatches(MainWindow* win) {
    int n = len(win->findMatches);
    for (int i = 0; i < n; i++) {
        str::Free(win->findMatches[i].snippet);
    }
    VecReset(win->findMatches);
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
TempStr BuildSnippet(EngineBase* engine, const FindMatch& m) {
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

void FreeMatchSnippets(Vec<FindMatch>* matches) {
    if (!matches) {
        return;
    }
    for (int i = 0; i < len(*matches); i++) {
        str::Free((*matches)[i].snippet);
    }
}

// Page the running scan is on, so the in-progress status can show it. Only one
// scan runs at a time (older ones are canceled by epoch), so a single global is
// enough; reset when a scan starts.
int gFindCountCurPage = 0;

// status while a scan is in flight: matches so far and the page being scanned,
// e.g. "12 34". ShowMatchCount replaces it with "n / m" when the scan ends.
void SetFindCountProgressStatus(MainWindow* win, int nFound, int pageNo) {
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

// clones matches[from..to) incl. copies of the snippet strings
Vec<FindMatch>* CloneMatchesRange(Vec<FindMatch>* matches, int from, int to) {
    auto* res = new Vec<FindMatch>();
    for (int i = from; i < to; i++) {
        FindMatch fm = (*matches)[i];
        fm.snippet = str::Dup(fm.snippet);
        VecAppend(*res, fm);
    }
    return res;
}

// Rebuild the match-count / snippet list for the current term without starting
// a new interactive Find Next (opening Find after -search must not skip a hit).
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

bool FindMatchTouchesVisiblePages(const FindMatch& fm, int firstPage, int lastPage) {
    return fm.endPage >= firstPage && fm.startPage <= lastPage;
}

void GetVisiblePageRange(DisplayModel* dm, int& firstOut, int& lastOut) {
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

void AppendMatchPageRects(EngineBase* engine, const FindMatch& fm, Vec<FindMatchPaintPageRect>& out) {
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
    for (const TextSel& part : ts.result) {
        VecAppend(out, FindMatchPaintPageRect{part.pageNo, part.rect});
    }
}

void AppendPageRectsToScreen(DisplayModel* dm, const Rect& clipRc, const FindMatchPaintPageRect* pageRects, int nRects,
                             Vec<Rect>& out) {
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

void AppendTextSelScreenRects(DisplayModel* dm, const Rect& clipRc, Vec<TextSel>* sel, Vec<Rect>& out) {
    if (!sel || len(*sel) == 0) {
        return;
    }
    for (const TextSel& part : *sel) {
        int pageNo = part.pageNo;
        if (!dm->PageVisible(pageNo)) {
            continue;
        }
        Rect rc = dm->CvtToScreen(pageNo, ToRectF(part.rect));
        rc = rc.Intersect(clipRc);
        if (!rc.IsEmpty()) {
            VecAppend(out, rc);
        }
    }
}

// Show the result of a PDF forward-search synchronization (initiated by a DDE command)
// Build a page-space box to flash after an internal jump. FitR dests already
// have a rectangle. /XYZ is a point: a short strip at dest Y from dest X to
// the right of the page, like the LaTeX forward-search mark. Page-level
// /Fit with no coordinates returns empty (nothing useful to highlight).
bool LinkDestHighlightRect(DisplayModel* dm, int pageNo, RectF dest, Rect* out) {
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

Str HandleSearchCmd(HWND hwnd, Str cmd, bool* ack) {
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

Str HandleGotoPageWordCmd(HWND hwnd, Str cmd, bool* ack) {
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
        Vec<TextSel>* sel = dm->textSearch->FindFirstOnPage(page, term);
        if (sel && len(*sel) > 0) {
            ShowSearchResult(win, sel, false);
        } else {
            // term not on this page: stay on the page, select nothing
            ClearSearchResult(win);
        }
    }
    win->Focus();
    return next;
}

Str HandleGotoCmd(HWND hwnd, Str cmd, bool* ack) {
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

Str HandlePageCmd(HWND hwnd, Str cmd, bool* ack) {
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

Str HandleSetViewCmd(HWND hwnd, Str cmd, bool* ack) {
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

Str HandleFullScreenCmd(HWND hwnd, Str cmd, bool* ack) {
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

Str HandleNewWindowCmd(Str cmd, bool* ack) {
    Str kNewWindowCmd = StrL("[NewWindow]");
    if (!str::TrimPrefix(cmd, kNewWindowCmd)) {
        return {};
    }
    logf("HandleNewWindowCmd\n");
    CreateAndShowMainWindow(nullptr);
    *ack = true;
    return cmd;
}

// returns the full path of every open document, one per line (issue #5060)
Str HandleGetOpenFilesCmd(Str cmd, bool* ack, str::Builder& res) {
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

// start a new find in the browser-hosted (chm / markdown) webview for the find
// bar's text: highlight the current page and sweep all pages for the match
// list. Results arrive asynchronously via BrowserFindResultReceived() /
// BrowserFindAllResultReceived()
void BrowserFindStartSearch(MainWindow* win, DocController* md) {
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

// Ctrl+F with a term left in the box: highlight its matches on the pages in
// view without moving to one; Enter is what restarts the search
void HighlightRestoredFindTerm(MainWindow* win) {
    if (!HasFindText(win)) {
        return;
    }
    // typing (or a copied selection) is about to start its own search
    if (win->findDebouncePending || len(win->findMatches) > 0) {
        return;
    }
    EnsureFindSnippets(win);
}

bool HasFindText(MainWindow* win) {
    return FindEditTextLen(win) > 0;
}

void FindSelection(MainWindow* win, TextSearch::Direction direction) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win) || !win->AsFixed()) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!win->CurrentTab()->selectionOnPage || 0 == len(dm->textSelection->result)) {
        return;
    }

    TempStr selection = dm->textSelection->ExtractTextTemp(StrL(" "));
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

// navigate to a match chosen from the floating results list and select it, so
// Find Next/Prev and the n/m counter continue from there
// navigate to and select a match chosen from the floating results list
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
    // Join an in-flight interactive find first: it drives dm->textSearch, which
    // we're about to mutate. Deliberately not AbortFinding(): the counting scan
    // has its own TextSearch and reads page text through the engine's locked
    // text cache, so picking a match doesn't have to stop the rest of the
    // document from being searched - that only happens when the find window is
    // closed. Skip the join when idle so stepping through the floating results
    // list stays responsive.
    if (win->findThread || win->findDebouncePending) {
        CancelPendingFind(win);
        JoinFindThread(win, true);
    }
    DisplayModel* dm = win->AsFixed();
    TextSearch* ts = dm->textSearch;
    ts->Reset();
    ts->StartAt(startPage, startGlyph);
    ts->SelectUpTo(endPage, endGlyph);
    if (len(ts->result) == 0) {
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
    // ...and put the result back if SetText() dropped it. PaintAllFindMatches
    // only treats a match as the current one (selection color) when ts->result
    // is populated, so without this the match we just navigated to paints as a
    // plain match - and with the find UI closed it isn't highlighted at all.
    // Only bites when the document text differs from what was typed, which is
    // why it looked intermittent (issue #5889)
    if (len(ts->result) == 0) {
        ts->StartAt(startPage, startGlyph);
        ts->SelectUpTo(endPage, endGlyph);
    }
    ShowMatchCount(win);
}

bool AbortFinding(MainWindow* win, bool hideMessage) {
    CancelPendingFind(win);
    AbortCount(win);
    return JoinFindThread(win, hideMessage);
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
