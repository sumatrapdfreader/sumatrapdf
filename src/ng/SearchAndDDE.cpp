/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the search half of orig's SearchAndDDE.cpp - find-as-you-type, the
// interactive find worker (FindThread), the full-document match counter
// (CountThread), the all-match highlights, inverse search and the
// forward-search mark. The DDE command grammar below runs everywhere; the
// DDE window messages stay Windows only. `Gfx*` is
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
#include "SumatraConfig.h"
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
#include "SearchAndDDECommon.h"
#include "Toolbar.h"

#include "SumatraLog.h"

const StrVec& FindHistory() {
    return gFindHistory;
}

// update the find bar's "n / m" status from the current in-page match and the
// all-pages sweep
// ng's results list reads the current match on every frame
void BrowserFindStatusShown(MainWindow*) {}

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
    if (!hadFindFocus && len(dm->textSelection->result) > 0) {
        TempStr selection = dm->textSelection->ExtractTextTemp(StrL(" "));
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

TempStr FindPagesEditTextTemp(MainWindow* win) {
    return win->findPagesEdit ? str::DupTemp(FromGpui(gp::InputValue(win->findPagesEdit))) : TempStr{};
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

// if a debounced search is pending, cancel the timer and start it now (so Enter
// forces the search to start immediately). Returns true if one was pending.
// ng's debounce is a countdown that findDebouncePending gates
void FindDebounceStop(MainWindow*) {}

// ng's find buttons read the find state on every frame
void FindSetToolbarBusy(MainWindow*, bool) {}

void FindWindowDocChanged(MainWindow*) {}

void FindResultsInstalled(MainWindow* win, bool) {
    FindWindowRefreshResults(win);
}

void FindCountShown(MainWindow* win) {
    AppShellInvalidate(win);
}

void FindStatusChanged(MainWindow* win) {
    AppShellInvalidate(win);
}

void UpdateFindStatus(UpdateFindStatusData* d) {
    AutoDelete delData(d);

    auto* win = d->win;
    if (!IsMainWindowValidAndNotClosing(win) || win->findCancelled) {
        return;
    }
    if (!d->showProgress && d->firstPage) {
        // The first report is the page being searched. Cancelling there drops
        // its match. Stop only when a later page is reported; the count thread
        // still scans the whole document.
        if (*d->firstPage == 0) {
            *d->firstPage = d->current;
        } else if (d->current != *d->firstPage) {
            win->findCancelled = true;
        }
    }
    // explicit Find Next/Prev (showProgress): keep going to completion. There's
    // no progress notification now -- the n/m counter is the only feedback.
}

// ---- find bar "n / m" match counter ----------------------------------------
//
// We show the position of the current match among all matches in the document.
// Counting all matches requires a full-document scan, so it runs on a background
// thread and the per-match positions are cached: prev/next (which don't change
// the term) recompute the index instantly from the cache, and a new scan only
// runs when the search term or match-case option changes.

// cancel a pending debounced find-as-you-type search
void CancelPendingFind(MainWindow* win) {
    win->findDebouncePending = false;
    win->findDebounceLeftMs = 0;
}

// ng: orig's win->canvasRc is the canvas HWND's client rect, i.e. the origin
// of document coordinates. Here canvasRc sits below the menu bar and the tab
// strip, so the clip for document coordinates is the viewport.
static Rect CanvasClipRc(DisplayModel* dm) {
    return Rect(Point(), dm->GetViewPort().Size());
}

// the canvas area find marks are clipped to
Rect FindPaintClipRc(MainWindow*, DisplayModel* dm) {
    return CanvasClipRc(dm);
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
    if (ts && len(ts->result) > 0) {
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
    if (len(currentRects) == 0 && ts && len(ts->result) > 0) {
        AppendTextSelScreenRects(dm, clipRc, &ts->result, currentRects);
    }
    if (len(currentRects) > 0) {
        PaintTransparentRectangles(ctx, clipRc, currentRects, parsedCol->col, alpha);
    }
}

// --- the forward-search mark (step 18) --------------------------------------

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

// starts fading the forward search mark after delayMs
void HideFwdSearchMarkAfter(MainWindow* win, int delayMs) {
    win->fwdSearchMark.hideLeftMs = delayMs;
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
        Rect overallrc = rects[0];
        for (int i = 1; i < len(rects); i++) {
            overallrc = overallrc.Union(rects[i]);
        }
        Vec<TextSel> res;
        VecAppend(res, TextSel{page, overallrc, {}});
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
        out.Append(fmt("current=page %d glyph %d rects %d\n", ts->startPage, ts->startGlyph, len(ts->result)));
    }
    // page of the active hit (0: none), whether a search is still running
    // and the page in view (orig's TestFindUiState fields)
    int hitPage = (ts && len(ts->result) > 0) ? ts->result[0].pageNo : 0;
    bool busy = win->findThread || win->findCountThread || win->findDebouncePending;
    int page = win->ctrl ? win->ctrl->CurrentPageNo() : 0;
    out.Append(fmt("hitPage=%d busy=%d page=%d\n", hitPage, busy ? 1 : 0, page));
    return ToStrTemp(out);
}

// ─── DDE command grammar (orig's). Window messages stay Windows-only. ───

MainWindow* WindowFromHwnd(HWND hwnd) {
#if OS_WIN
    return AppShellWindowFromHwnd(hwnd);
#else
    (void)hwnd;
    return len(gWindows) > 0 ? gWindows[0] : nullptr;
#endif
}

// ng: orig tracks the window the user last worked in with gLastActiveFrameHwnd;
// here the foreground window answers the same question
MainWindow* LastActiveWindow() {
#if OS_WIN
    MainWindow* win = AppShellWindowFromHwnd(GetForegroundWindow());
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    return win;
#else
    return len(gWindows) > 0 ? gWindows[0] : nullptr;
#endif
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

/*
Forward search (synchronization) DDE command

[ForwardSearch(["<pdffilepath>",]"<sourcefilepath>",<line>,<column>[,<newwindow>, <setfocus>])]
eg:
[ForwardSearch("c:\file.pdf","c:\folder\source.tex",298,0)]

if pdffilepath is provided, the file will be opened if no open window can be found for it
if newwindow = 1 then a new window is created even if the file is already open
if focus = 1 then the focus is set to the window
*/

/*
Search DDE command

[Search("<pdffile>","<search-term>")]
Quotes inside the term/path are escaped as "" (standard DDE-style).
*/

/*
Go to a page and select the search term, but only if it's found on that page
(unlike Search, which keeps searching following pages and wraps around).

[GotoPageWord("<pdffile>",<page>,"<search-term>")]
*/

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
Str HandleOpenCmd(Str cmd, bool* ack) {
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

/*
DDE command: jump to a page in an already opened document.

[GoToPage("<pdffilepath>",<page number>)]

eg: [GoToPage("c:\file.pdf",37)]
*/

/*
Set view mode and zoom level DDE command

[SetView("<filepath>", "<view mode>", <zoom level>[, <scrollX>, <scrollY>])]

eg: [SetView("c:\file.pdf", "book view", -2)]

use -1 for kZoomFitPage, -2 for kZoomFitWidth, -3 for kZoomFitContent, -6 for kZoomFitHeight
*/

/*
Show the document in presentation or fullscreen mode, like -presentation /
-fullscreen do for a file opened on the command line.

[Presentation("<pdffile>")]
[FullScreen("<pdffile>")]
*/

/*
Open new window.

[NewWindow]
*/

/*
[GetFileState("<filepath>")]
[GetFileState()]
[GetFileState]
Info about document <filepath>, or the currently viewed one when no path is
given, as "key: value" lines. zoom is a percentage, or -1 = fit page,
-2 = fit width, -3 = fit content, -6 = fit height (as in SetView).
*/

/*
Handle all commands as defined in Commands.h
eg: [CmdClose] or [CmdCreateAnnotHighlight #00ff00 openEdit]
*/

// returns true if did handle a message
// ng: orig also has [ForwardSearch(...)], which needs the forward-search mark
// this port doesn't draw yet
void DdeExecuteBegin() {}

// [Open] / [GotoPageWord] / the rest of the DDE execute grammar.
bool ExecuteDdeCmds(Str cmd) {
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    HWND hwnd = nullptr;
#if OS_WIN
    hwnd = win ? AppShellNativeHwnd(win) : nullptr;
#else
    (void)win;
#endif
    return HandleExecuteCmds(hwnd, cmd);
}

#if OS_WIN

// ng answers no requests of its own
Str HandleAppRequestCmd(Str, bool*, str::Builder&) {
    return {};
}

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
        // cbData includes the trailing NUL. Counting it makes the path miss
        // the tab that is already open (issue #4576).
        const char* pathS = (const char*)(const u8*)(data + 1);
        size_t pathLen = strnlen_s(pathS, pathMax);
        if (pathLen >= pathMax) {
            return FALSE;
        }
        Str pathZ((char*)pathS, (int)pathLen);
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
