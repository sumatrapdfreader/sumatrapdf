/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/AutoWin.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

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
#include "Toolbar.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Favorites.h"
#include "Translations.h"
#include "Version.h"
#include "SearchAndDDE.h"
#include "SearchAndDDECommon.h"

void ApplyFindHistory(DropDown* dd) {
    if (dd) {
        dd->SetItemsKeepText(gFindHistory);
    }
}

// update the find bar's "n / m" status (and the results list selection) from
// the current in-page match and the all-pages sweep
// mirrors the current match in the results list
void BrowserFindStatusShown(MainWindow* win) {
    FindWindowRefreshResults(win);
}

void FindFirst(MainWindow* win) {
    // Only open/focus the find UI here. The search-start favorite ("/") is set
    // when a real search begins (non-empty term in FindTextOnThread /
    // BrowserFindStartSearch), not merely when the find box is opened
    // (issue #5862 / #5726).
    if (!win) {
        return;
    }
    bool hadFindFocus = win->findEdit && win->findEdit->IsFocused();
    if (!hadFindFocus) {
        win->searchStartMarked = false;
    }

    if (BrowserFindCtrl(win)) {
        // chm / markdown in a webview: our own find bar drives the search
        // inside the webview
        ShowFindBar(win);
        if (win->findEdit) {
            win->findEdit->SetFocus();
            CbEditSelectAll(win->findEdit);
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

    // show the floating Chrome-style find bar (creates it lazily if needed)
    ShowFindBar(win);

    // If focus was in the document (not find bar), copy selected text
    // to find edit only if it's different from current text. Setting the text
    // triggers find-as-you-type via the bar's onTextChanged handler.
    if (!hadFindFocus && len(dm->textSelection->result) > 0) {
        TempStr selection = dm->textSelection->ExtractTextTemp(StrL(" "));
        selection.len -= str::NormalizeWSInPlace(selection);
        if (len(selection) > 0) {
            TempStr current = win->findEdit ? win->findEdit->GetTextTemp() : TempStr{};
            if (!str::EqI(selection, current)) {
                AbortFinding(win, false);
                dm->textSearch->SetLastResult(dm->textSelection);
                if (win->findEdit) {
                    win->findEdit->SetText(selection);
                }
            }
        }
    }

    if (win->findEdit) {
        win->findEdit->SetFocus();
        CbEditSelectAll(win->findEdit);
    }
    HighlightRestoredFindTerm(win);
}

// debounce delays (ms) for find-as-you-type. Short terms (1-2 chars) match a
// lot of text and the search is expensive, so wait longer before starting them
// (issue #4626). Enter bypasses the wait (see FindFlushPendingSearch).
constexpr UINT kFindDebounceDelayMs = 500;
constexpr UINT kFindDebounceShortDelayMs = 1000;

TempStr FindPagesEditTextTemp(MainWindow* win) {
    return win->findPagesEdit ? win->findPagesEdit->GetTextTemp() : TempStr{};
}

// called when the user edits the find bar's text (find-as-you-type)
void OnFindBarTextChanged(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    TempStr s = win->findEdit ? win->findEdit->GetTextTemp() : TempStr{};
    if (len(s) == 0) {
        AbortFinding(win, true); // also cancels a pending debounce timer
        DocController* md = BrowserFindCtrl(win);
        if (md) {
            md->FindClear(); // remove the highlights in the webview
        }
        ClearSearchResult(win);
        FindBarSetStatus(win, StrL(""));
        ClearFindMatches(win);
        FindWindowRefreshResults(win); // empty the results list
        return;
    }
    size_t nChars = CbGetTextLen(win->findEdit);
    UINT delay = (nChars <= 2) ? kFindDebounceShortDelayMs : kFindDebounceDelayMs;
    // SetTimer with the same id replaces the previous timer, so each keystroke
    // restarts the countdown
    SetTimer(win->hwndFrame, kFindDebounceTimerId, delay, nullptr);
    win->findDebouncePending = true;
}

// fired by the debounce WM_TIMER on hwndFrame: runs the deferred search
void FindDebounceTimerFired(MainWindow* win) {
    KillTimer(win->hwndFrame, kFindDebounceTimerId);
    if (!win->findDebouncePending) {
        return;
    }
    win->findDebouncePending = false;
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (CbGetTextLen(win->findEdit) > 0) {
        StartIncrementalFind(win);
    }
}

// if a debounced search is pending, cancel the timer and start it now (so Enter
// forces the search to start immediately). Returns true if one was pending.
void FindDebounceStop(MainWindow* win) {
    KillTimer(win->hwndFrame, kFindDebounceTimerId);
}

// the find buttons are disabled while a find runs
void FindSetToolbarBusy(MainWindow* win, bool busy) {
    SetToolbarButtonEnableState(win, CmdFindPrev, !busy);
    SetToolbarButtonEnableState(win, CmdFindNext, !busy);
    SetToolbarButtonEnableState(win, CmdFindToggleMatchCase, !busy);
    SetToolbarButtonEnableState(win, CmdFindToggleMatchWholeWord, !busy);
}

void FindWindowDocChanged(MainWindow* win) {
    FindWindowUpdatePagesLabel(win);
    FindWindowRefreshResults(win);
}

void FindResultsInstalled(MainWindow* win, bool gotSnippets) {
    if (gotSnippets) {
        FindWindowRefreshResults(win);
    }
}

void FindCountShown(MainWindow* win) {
    // Enable/disable Find Next/Prev once we know whether any matches exist.
    ToolbarUpdateStateForWindow(win, false);
    ScheduleRepaint(win, 0);
}

// the find controls repaint themselves
void FindStatusChanged(MainWindow*) {}

void UpdateFindStatus(UpdateFindStatusData* d) {
    AutoDelete delData(d);

    auto* win = d->win;
    if (!IsMainWindowValidAndNotClosing(win) || win->findCancelled) {
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

// ---- find bar "n / m" match counter ----------------------------------------
//
// We show the position of the current match among all matches in the document.
// Counting all matches requires a full-document scan, so it runs on a background
// thread and the per-match positions are cached: prev/next (which don't change
// the term) recompute the index instantly from the cache, and a new scan only
// runs when the search term or match-case option changes.

// returns true if did abort a thread or hidden the notification
// cancel a pending debounced find-as-you-type search
void CancelPendingFind(MainWindow* win) {
    if (!win->findDebouncePending) {
        return;
    }
    win->findDebouncePending = false;
    if (win->hwndFrame) {
        KillTimer(win->hwndFrame, kFindDebounceTimerId);
    }
}

// TODO: for https://github.com/sumatrapdfreader/sumatrapdf/issues/2655
__unused static TempStr ReverseTextTemp(Str s) {
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

// the canvas area find marks are clipped to
Rect FindPaintClipRc(MainWindow* win, DisplayModel*) {
    return win->canvasRc;
}

void PaintAllFindMatches(MainWindow* win, Gfx* gfx) {
    if (!win->IsDocLoaded() || !win->AsFixed()) {
        return;
    }
    if (CbGetTextLen(win->findEdit) == 0) {
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
        PaintCurrentFindMatch(win, dm, ts, gfx);
        return;
    }
    if (!win->findCountValid && len(win->findMatches) == 0) {
        // count still running: at least highlight the current match
        PaintCurrentFindMatch(win, dm, ts, gfx);
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

    Vec<Rect> otherRects;
    Vec<Rect> currentRects;
    Vec<FindMatchPaintPageRect>& positions = gFindMatchPaintCache.positions;
    for (int i = 0; i < len(gFindMatchPaintCache.entries); i++) {
        const FindMatchPaintRects& entry = gFindMatchPaintCache.entries[i];
        Vec<Rect>& out = (entry.key == currentKey) ? currentRects : otherRects;
        AppendPageRectsToScreen(dm, win->canvasRc, &positions[entry.firstPos], entry.len, out);
    }

    if (len(otherRects) > 0) {
        PaintTransparentRectangles(gfx, win->canvasRc, otherRects, kFindOtherMatchColor, alpha);
    }
    if (len(currentRects) == 0 && ts && len(ts->result) > 0) {
        AppendTextSelScreenRects(dm, win->canvasRc, &ts->result, currentRects);
    }
    if (len(currentRects) > 0) {
        PaintTransparentRectangles(gfx, win->canvasRc, currentRects, parsedCol->col, alpha);
    }
}

// starts fading the forward search mark after delayMs
void HideFwdSearchMarkAfter(MainWindow* win, int delayMs) {
    SetTimer(win->hwndCanvas, kHideFwdSearchMarkTimerID, delayMs, nullptr);
}

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
        if (!gSettings->forwardSearch.highlightPermanent) {
            SetTimer(win->hwndCanvas, kHideFwdSearchMarkTimerID, kHideFwdSearchMarkDelayInMs, nullptr);
        }

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
        if (!dm->ShowResultRectToScreen(&res)) {
            ScheduleRepaint(win, 0);
        }
        if (IsIconic(win->hwndFrame)) {
            ShowWindowAsync(win->hwndFrame, SW_RESTORE);
        }
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

MainWindow* WindowFromHwnd(HWND hwnd) {
    return FindMainWindowByHwnd(hwnd);
}

MainWindow* LastActiveWindow() {
    MainWindow* win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    return win;
}

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
    bool isCtrl = IsCtrlPressed();
    logf("HandleOpenCmd: '%s', newWindow: %d, setFocus: %d, forceRefresh: %d, inCurrentTab: %d, isCtrl: %d\n", filePath,
         newWindow, setFocus, forceRefresh, inCurrentTab, isCtrl);
    // on startup this is called while LoadDocument is in progress, which causes
    // all sort of mayhem. Queue files to be loaded in a sequence
    if (gIsStartup) {
        // Dedupe: Explorer multi-open / password dialog reentrancy can deliver
        // the same path more than once before we drain the queue (fixes #4576).
        if (IsDocumentOpenOrLoading(filePath)) {
            logf("HandleOpenCmd: gIsStartup, already open/loading '%s', skip queue\n", filePath);
            return next;
        }
        for (Str queued : gDdeOpenOnStartup) {
            if (path::IsSame(queued, filePath)) {
                logf("HandleOpenCmd: gIsStartup, already queued '%s'\n", filePath);
                return next;
            }
        }
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
Return info about document <filepath> or currently viewed document if no
<filepath> given.
Returns info in the format:

path: c:\file.pdf
zoom: 120
view: continuous
sumver: 3.7

zoom is a percentage, or -1 = fit page, -2 = fit width, -3 = fit content, -6 = fit height
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
        res.Append(StrL("error: no document\n"));
        return next;
    }
    Point pos = HwndGetCursorPos(win->hwndCanvas);
    int pageNo = dm->GetPageNoByPoint(pos);
    bool validPage = dm->ValidPageNo(pageNo);
    PointF pt = dm->CvtFromScreen(pos);
    // match FormatCursorPositionTemp's "pt" computation exactly
    EngineBase* engine = dm->GetEngine();
    float dpi = engine->fileDPI;
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

// returns true if did handle a message
// a new batch of commands forgets the document the last one opened
void DdeExecuteBegin() {
    gMostRecentlyOpenedDoc = nullptr;
}

// requests only orig answers
Str HandleAppRequestCmd(Str cmd, bool* ack, str::Builder& res) {
    return HandleGetMousePosCmd(cmd, ack, res);
}

static void OpenManyCopyDataAsyncRun(OpenManyCopyDataAsync* d) {
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
        win = FindMainWindowByHwnd(d->hwnd);
        if (!win) {
            win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
        }
        if (!win && len(gWindows) > 0) {
            win = gWindows[0];
        }
    }
    if (win) {
        win->Focus();
    }
    StartLoadDocuments(d->paths, win);
    delete d;
}

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
        if (win) {
            // Already open: just focus (matches activateExisting).
            win->Focus();
            str::Free(d->path);
            delete d;
            return;
        }
        // Mid-load (e.g. password dialog): do not start a second load of the
        // same path — that is what produced the 2N-1 tabs in #4576.
        if (IsDocumentOpenOrLoading(d->path)) {
            logf("OpenCopyDataAsyncRun: skipping already open/loading '%s'\n", d->path);
            str::Free(d->path);
            delete d;
            return;
        }
        win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
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
        // During startup (cmdline load, often blocked on a password dialog) the
        // message pump can deliver COPYDATA opens. Match HandleOpenCmd: queue
        // them so they load after the current LoadDocument finishes, instead of
        // racing a second async load of the same path (fixes #4576).
        if (gIsStartup) {
            TempStr path = path::NormalizeTemp(pathZ);
            if (IsDocumentOpenOrLoading(path)) {
                logf("OnCopyData/Open: gIsStartup, already open/loading '%s'\n", path);
                return TRUE;
            }
            for (Str queued : gDdeOpenOnStartup) {
                if (path::IsSame(queued, path)) {
                    logf("OnCopyData/Open: gIsStartup, already queued '%s'\n", path);
                    return TRUE;
                }
            }
            logf("OnCopyData/Open: gIsStartup, queueing '%s'\n", path);
            gDdeOpenOnStartup.Append(path);
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
                if (!IsDocumentOpenOrLoading(normalized)) {
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
