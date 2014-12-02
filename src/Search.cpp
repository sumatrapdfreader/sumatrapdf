/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* Code related to:
* user-initiated search
* DDE commands, including search
*/

#include "BaseUtil.h"
#include "BaseEngine.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "ChmModel.h"
#include "EngineManager.h"
#include "DisplayModel.h"
#include "FileUtil.h"
#include "Notifications.h"
#include "PdfEngine.h"
#include "PdfSync.h"
#include "resource.h"
#include "SumatraDialogs.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "UITask.h"
#include "WindowInfo.h"
#include "WinUtil.h"
#include "Selection.h"
#include "Search.h"

// don't show the Search UI for document types that don't
// support extracting text and/or navigating to a specific
// text selection; default to showing it, since most users
// will never use a format that does not support search
bool NeedsFindUI(WindowInfo *win)
{
    if (!win->IsDocLoaded())
        return true;
    if (!win->AsFixed())
        return false;
    if (win->AsFixed()->GetEngine()->IsImageCollection())
        return false;
    return true;
}

void OnMenuFind(WindowInfo *win)
{
    if (win->AsChm()) {
        win->AsChm()->FindInCurrentPage();
        return;
    }

    if (!win->AsFixed() || !NeedsFindUI(win))
        return;

    // copy any selected text to the find bar, if it's still empty
    DisplayModel *dm = win->AsFixed();
    if (dm->textSelection->result.len > 0 &&
        Edit_GetTextLength(win->hwndFindBox) == 0) {
        ScopedMem<WCHAR> selection(dm->textSelection->ExtractText(L" "));
        str::NormalizeWS(selection);
        if (!str::IsEmpty(selection.Get())) {
            win::SetText(win->hwndFindBox, selection);
            Edit_SetModify(win->hwndFindBox, TRUE);
        }
    }

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs->showToolbar && !win->isFullScreen && !win->presentation) {
        if (GetFocus() == win->hwndFindBox)
            SendMessage(win->hwndFindBox, WM_SETFOCUS, 0, 0);
        else
            SetFocus(win->hwndFindBox);
        return;
    }

    ScopedMem<WCHAR> previousFind(win::GetText(win->hwndFindBox));
    WORD state = (WORD)SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    bool matchCase = (state & TBSTATE_CHECKED) != 0;

    ScopedMem<WCHAR> findString(Dialog_Find(win->hwndFrame, previousFind, &matchCase));
    if (!findString)
        return;

    win::SetText(win->hwndFindBox, findString);
    Edit_SetModify(win->hwndFindBox, TRUE);

    bool matchCaseChanged = matchCase != (0 != (state & TBSTATE_CHECKED));
    if (matchCaseChanged) {
        if (matchCase)
            state |= TBSTATE_CHECKED;
        else
            state &= ~TBSTATE_CHECKED;
        SendMessage(win->hwndToolbar, TB_SETSTATE, IDM_FIND_MATCH, state);
        dm->textSearch->SetSensitive(matchCase);
    }

    FindTextOnThread(win, FIND_FORWARD, true);
}

void OnMenuFindNext(WindowInfo *win)
{
    if (!win->IsDocLoaded() || !NeedsFindUI(win))
        return;
    if (SendMessage(win->hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_NEXT, 0))
        FindTextOnThread(win, FIND_FORWARD, true);
}

void OnMenuFindPrev(WindowInfo *win)
{
    if (!win->IsDocLoaded() || !NeedsFindUI(win))
        return;
    if (SendMessage(win->hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_PREV, 0))
        FindTextOnThread(win, FIND_BACKWARD, true);
}

void OnMenuFindMatchCase(WindowInfo *win)
{
    if (!win->IsDocLoaded() || !NeedsFindUI(win))
        return;
    WORD state = (WORD)SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    win->AsFixed()->textSearch->SetSensitive((state & TBSTATE_CHECKED) != 0);
    Edit_SetModify(win->hwndFindBox, TRUE);
}

void OnMenuFindSel(WindowInfo *win, TextSearchDirection direction)
{
    if (!win->IsDocLoaded() || !NeedsFindUI(win))
        return;
    DisplayModel *dm = win->AsFixed();
    if (!win->currentTab->selectionOnPage || 0 == dm->textSelection->result.len)
        return;

    ScopedMem<WCHAR> selection(dm->textSelection->ExtractText(L" "));
    str::NormalizeWS(selection);
    if (str::IsEmpty(selection.Get()))
        return;

    win::SetText(win->hwndFindBox, selection);
    AbortFinding(win, false); // cancel "find as you type"
    Edit_SetModify(win->hwndFindBox, FALSE);
    dm->textSearch->SetLastResult(dm->textSelection);

    FindTextOnThread(win, direction, true);
}

static void ShowSearchResult(WindowInfo& win, TextSel *result, bool addNavPt)
{
    CrashIf(0 == result->len || !result->pages || !result->rects);
    if (0 == result->len || !result->pages || !result->rects)
        return;

    DisplayModel *dm = win.AsFixed();
    if (addNavPt || !dm->PageShown(result->pages[0]) ||
        (dm->GetZoomVirtual() == ZOOM_FIT_PAGE || dm->GetZoomVirtual() == ZOOM_FIT_CONTENT))
    {
        win.ctrl->GoToPage(result->pages[0], addNavPt);
    }

    dm->textSelection->CopySelection(dm->textSearch);
    UpdateTextSelection(&win, false);
    dm->ShowResultRectToScreen(result);
    win.RepaintAsync();
}

void ClearSearchResult(WindowInfo *win)
{
    DeleteOldSelectionInfo(win, true);
    win->RepaintAsync();
}

static void UpdateFindStatusTask(WindowInfo *win, NotificationWnd *wnd, int current, int total) {
    if (!WindowInfoStillValid(win) || win->findCanceled) {
        return;
    }
    if (win->notifications->Contains(wnd)) {
        wnd->UpdateProgress(current, total);
    } else {
        // the search has been canceled by closing the notification
        win->findCanceled = true;
    }
}

struct FindThreadData : public ProgressUpdateUI {
    WindowInfo *win;
    TextSearchDirection direction;
    bool wasModified;
    ScopedMem<WCHAR> text;
    // owned by win->notifications, as FindThreadData
    // can be deleted before the notification times out
    NotificationWnd *wnd;
    HANDLE thread;

    FindThreadData(WindowInfo *win, TextSearchDirection direction, HWND findBox) :
        win(win), direction(direction), text(win::GetText(findBox)),
        wasModified(Edit_GetModify(findBox)), wnd(NULL) { }
    ~FindThreadData() { CloseHandle(thread); }

    void ShowUI(bool showProgress) {
        const LPARAM disable = (LPARAM)MAKELONG(0, 0);

        if (showProgress) {
            wnd = new NotificationWnd(win->hwndCanvas, L"",
                                      _TR("Searching %d of %d..."), win->notifications);
            win->notifications->Add(wnd, NG_FIND_PROGRESS);
        }

        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, disable);
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, disable);
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, disable);
    }

    void HideUI(bool success, bool loopedAround) {
        LPARAM enable = (LPARAM)MAKELONG(1, 0);

        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, enable);
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, enable);
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, enable);

        if (!win->notifications->Contains(wnd))
            /* our notification has been replaced or closed (or never created) */;
        else if (!success && !loopedAround) // i.e. canceled
            win->notifications->RemoveNotification(wnd);
        else if (!success && loopedAround)
            wnd->UpdateMessage(_TR("No matches were found"), 3000);
        else {
            ScopedMem<WCHAR> label(win->ctrl->GetPageLabel(win->AsFixed()->textSearch->GetCurrentPageNo()));
            ScopedMem<WCHAR> buf(str::Format(_TR("Found text at page %s"), label.Get()));
            if (loopedAround) {
                buf.Set(str::Format(_TR("Found text at page %s (again)"), label.Get()));
                MessageBeep(MB_ICONINFORMATION);
            }
            wnd->UpdateMessage(buf, 3000, loopedAround);
        }
    }

    virtual void UpdateProgress(int current, int total) {
        if (!wnd || WasCanceled()) {
            return;
        }
        uitask::Post([=] {
            UpdateFindStatusTask(win, wnd, current, total);
        });
    }

    virtual bool WasCanceled() {
        return !WindowInfoStillValid(win) || win->findCanceled;
    }
};

static void FindEndTask(WindowInfo *win, FindThreadData *ftd, TextSel *textSel, 
    bool wasModifiedCanceled, bool loopedAround) {
    if (!WindowInfoStillValid(win)) {
        delete ftd;
        return;
    }
    if (win->findThread != ftd->thread) {
        // Race condition: FindTextOnThread/AbortFinding was
        // called after the previous find thread ended but
        // before this FindEndTask could be executed
        delete ftd;
        return;
    }
    if (!win->IsDocLoaded()) {
        // the UI has already been disabled and hidden
    } else if (textSel) {
        ShowSearchResult(*win, textSel, wasModifiedCanceled);
        ftd->HideUI(true, loopedAround);
    } else {
        // nothing found or search canceled
        ClearSearchResult(win);
        ftd->HideUI(false, !wasModifiedCanceled);
    }
    win->findThread = NULL;
    delete ftd;
}

static DWORD WINAPI FindThread(LPVOID data)
{
    FindThreadData *ftd = (FindThreadData *)data;
    AssertCrash(ftd && ftd->win && ftd->win->ctrl && ftd->win->ctrl->AsFixed());
    WindowInfo *win = ftd->win;
    DisplayModel *dm = win->AsFixed();

    TextSel *rect;
    dm->textSearch->SetDirection(ftd->direction);
    if (ftd->wasModified || !win->ctrl->ValidPageNo(dm->textSearch->GetCurrentPageNo()) ||
        !dm->GetPageInfo(dm->textSearch->GetCurrentPageNo())->visibleRatio)
        rect = dm->textSearch->FindFirst(win->ctrl->CurrentPageNo(), ftd->text, ftd);
    else
        rect = dm->textSearch->FindNext(ftd);

    bool loopedAround = false;
    if (!win->findCanceled && !rect) {
        // With no further findings, start over (unless this was a new search from the beginning)
        int startPage = (FIND_FORWARD == ftd->direction) ? 1 : win->ctrl->PageCount();
        if (!ftd->wasModified || win->ctrl->CurrentPageNo() != startPage) {
            loopedAround = true;
            rect = dm->textSearch->FindFirst(startPage, ftd->text, ftd);
        }
    }

    // wait for FindTextOnThread to return so that
    // FindEndTask closes the correct handle to
    // the current find thread
    while (!win->findThread) {
        Sleep(1);
    }

    if (!win->findCanceled && rect) {
        uitask::Post([=] {
            FindEndTask(win, ftd, rect, ftd->wasModified, loopedAround);
        });
    } else {
        uitask::Post([=] {
            FindEndTask(win, ftd, nullptr, win->findCanceled, false);
        });
    }

    return 0;
}

void AbortFinding(WindowInfo *win, bool hideMessage)
{
    if (win->findThread) {
        win->findCanceled = true;
        WaitForSingleObject(win->findThread, INFINITE);
    }
    win->findCanceled = false;

    if (hideMessage)
        win->notifications->RemoveForGroup(NG_FIND_PROGRESS);
}

void FindTextOnThread(WindowInfo* win, TextSearchDirection direction, bool showProgress)
{
    AbortFinding(win, true);

    FindThreadData *ftd = new FindThreadData(win, direction, win->hwndFindBox);
    Edit_SetModify(win->hwndFindBox, FALSE);

    if (str::IsEmpty(ftd->text.Get())) {
        delete ftd;
        return;
    }

    ftd->ShowUI(showProgress);
    win->findThread = NULL;
    win->findThread = CreateThread(NULL, 0, FindThread, ftd, 0, 0);
    ftd->thread = win->findThread; // safe because only accesssed on ui thread
}

void PaintForwardSearchMark(WindowInfo *win, HDC hdc)
{
    CrashIf(!win->AsFixed());
    DisplayModel *dm = win->AsFixed();
    PageInfo *pageInfo = dm->GetPageInfo(win->fwdSearchMark.page);
    if (!pageInfo || 0.0 == pageInfo->visibleRatio)
        return;

    // Draw the rectangles highlighting the forward search results
    Vec<RectI> rects;
    for (size_t i = 0; i < win->fwdSearchMark.rects.Count(); i++) {
        RectI rect = win->fwdSearchMark.rects.At(i);
        rect = dm->CvtToScreen(win->fwdSearchMark.page, rect.Convert<double>());
        if (gGlobalPrefs->forwardSearch.highlightOffset > 0) {
            rect.x = std::max(pageInfo->pageOnScreen.x, 0) + (int)(gGlobalPrefs->forwardSearch.highlightOffset * dm->GetZoomReal());
            rect.dx = (int)((gGlobalPrefs->forwardSearch.highlightWidth > 0 ? gGlobalPrefs->forwardSearch.highlightWidth : 15.0) * dm->GetZoomReal());
            rect.y -= 4;
            rect.dy += 8;
        }
        rects.Append(rect);
    }

    BYTE alpha = (BYTE)(0x5f * 1.0f * (HIDE_FWDSRCHMARK_STEPS - win->fwdSearchMark.hideStep) / HIDE_FWDSRCHMARK_STEPS);
    PaintTransparentRectangles(hdc, win->canvasRc, rects, gGlobalPrefs->forwardSearch.highlightColor, alpha, 0);
}

// returns true if the double-click was handled and false if it wasn't
bool OnInverseSearch(WindowInfo *win, int x, int y)
{
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) return false;
    if (win->GetEngineType() != Engine_PDF) return false;
    DisplayModel *dm = win->AsFixed();

    // Clear the last forward-search result
    win->fwdSearchMark.rects.Reset();
    InvalidateRect(win->hwndCanvas, NULL, FALSE);

    // On double-clicking error message will be shown to the user
    // if the PDF does not have a synchronization file
    if (!win->AsFixed()->pdfSync) {
        int err = Synchronizer::Create(win->loadedFilePath, win->AsFixed()->GetEngine(), &win->AsFixed()->pdfSync);
        if (err == PDFSYNCERR_SYNCFILE_NOTFOUND) {
            // We used to warn that "No synchronization file found" at this
            // point if gGlobalPrefs->enableTeXEnhancements is set; we no longer
            // so do because a double-click has several other meanings
            // (selecting a word or an image, navigating quickly using links)
            // and showing an unrelated warning in all those cases seems wrong
            return false;
        }
        if (err != PDFSYNCERR_SUCCESS) {
            win->ShowNotification(_TR("Synchronization file cannot be opened"));
            return true;
        }
        gGlobalPrefs->enableTeXEnhancements = true;
    }

    int pageNo = dm->GetPageNoByPoint(PointI(x, y));
    if (!win->ctrl->ValidPageNo(pageNo))
        return false;

    PointI pt = dm->CvtFromScreen(PointI(x, y), pageNo).ToInt();
    ScopedMem<WCHAR> srcfilepath;
    UINT line, col;
    int err = win->AsFixed()->pdfSync->DocToSource(pageNo, pt, srcfilepath, &line, &col);
    if (err != PDFSYNCERR_SUCCESS) {
        win->ShowNotification(_TR("No synchronization info at this position"));
        return true;
    }

    if (!file::Exists(srcfilepath)) {
        // if the source file is missing, check if it's been moved to the same place as
        // the PDF document (which happens if all files are moved together)
        ScopedMem<WCHAR> altsrcpath(path::GetDir(win->loadedFilePath));
        altsrcpath.Set(path::Join(altsrcpath, path::GetBaseName(srcfilepath)));
        if (!str::Eq(altsrcpath, srcfilepath) && file::Exists(altsrcpath))
            srcfilepath.Set(altsrcpath.StealData());
    }

    WCHAR *inverseSearch = gGlobalPrefs->inverseSearchCmdLine;
    if (!inverseSearch)
        // Detect a text editor and use it as the default inverse search handler for now
        inverseSearch = AutoDetectInverseSearchCommands();

    ScopedMem<WCHAR> cmdline;
    if (inverseSearch)
        cmdline.Set(win->AsFixed()->pdfSync->PrepareCommandline(inverseSearch, srcfilepath, line, col));
    if (!str::IsEmpty(cmdline.Get())) {
        // resolve relative paths with relation to SumatraPDF.exe's directory
        ScopedMem<WCHAR> appDir(GetExePath());
        if (appDir)
            appDir.Set(path::GetDir(appDir));
        ScopedHandle process(LaunchProcess(cmdline, appDir));
        if (!process)
            win->ShowNotification(_TR("Cannot start inverse search command. Please check the command line in the settings."));
    }
    else if (gGlobalPrefs->enableTeXEnhancements)
        win->ShowNotification(_TR("Cannot start inverse search command. Please check the command line in the settings."));

    if (inverseSearch != gGlobalPrefs->inverseSearchCmdLine)
        free(inverseSearch);

    return true;
}

// Show the result of a PDF forward-search synchronization (initiated by a DDE command)
void ShowForwardSearchResult(WindowInfo *win, const WCHAR *fileName, UINT line, UINT col, UINT ret, UINT page, Vec<RectI> &rects)
{
    CrashIf(!win->AsFixed());
    DisplayModel *dm = win->AsFixed();
    win->fwdSearchMark.rects.Reset();
    const PageInfo *pi = dm->GetPageInfo(page);
    if ((ret == PDFSYNCERR_SUCCESS) && (rects.Count() > 0) && (NULL != pi)) {
        // remember the position of the search result for drawing the rect later on
        win->fwdSearchMark.rects = rects;
        win->fwdSearchMark.page = page;
        win->fwdSearchMark.show = true;
        win->fwdSearchMark.hideStep = 0;
        if (!gGlobalPrefs->forwardSearch.highlightPermanent)
            SetTimer(win->hwndCanvas, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DELAY_IN_MS, NULL);

        // Scroll to show the overall highlighted zone
        int pageNo = page;
        RectI overallrc = rects.At(0);
        for (size_t i = 1; i < rects.Count(); i++)
            overallrc = overallrc.Union(rects.At(i));
        TextSel res = { 1, &pageNo, &overallrc };
        if (!dm->PageVisible(page))
            win->ctrl->GoToPage(page, true);
        if (!dm->ShowResultRectToScreen(&res))
            win->RepaintAsync();
        if (IsIconic(win->hwndFrame))
            ShowWindowAsync(win->hwndFrame, SW_RESTORE);
        return;
    }

    ScopedMem<WCHAR> buf;
    if (ret == PDFSYNCERR_SYNCFILE_NOTFOUND)
        win->ShowNotification(_TR("No synchronization file found"));
    else if (ret == PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED)
        win->ShowNotification(_TR("Synchronization file cannot be opened"));
    else if (ret == PDFSYNCERR_INVALID_PAGE_NUMBER)
        buf.Set(str::Format(_TR("Page number %u inexistant"), page));
    else if (ret == PDFSYNCERR_NO_SYNC_AT_LOCATION)
        win->ShowNotification(_TR("No synchronization info at this position"));
    else if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE)
        buf.Set(str::Format(_TR("Unknown source file (%s)"), fileName));
    else if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE)
        buf.Set(str::Format(_TR("Source file %s has no synchronization point"), fileName));
    else if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE)
        buf.Set(str::Format(_TR("No result found around line %u in file %s"), line, fileName));
    else if (ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD)
        buf.Set(str::Format(_TR("No result found around line %u in file %s"), line, fileName));
    if (buf)
        win->ShowNotification(buf);
}

// DDE commands handling

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    ATOM aServer = GlobalAddAtom(PDFSYNC_DDE_SERVICE);
    ATOM aTopic = GlobalAddAtom(PDFSYNC_DDE_TOPIC);

    if (LOWORD(lparam) == aServer && HIWORD(lparam) == aTopic) {
        SendMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)hwnd, MAKELPARAM(aServer, 0));
    }
    else {
        GlobalDeleteAtom(aServer);
        GlobalDeleteAtom(aTopic);
    }
    return 0;
}

// DDE commands

// Synchronization command format:
// [<DDECOMMAND_SYNC>(["<pdffile>",]"<srcfile>",<line>,<col>[,<newwindow>,<setfocus>])]
static const WCHAR *HandleSyncCmd(const WCHAR *cmd, DDEACK& ack)
{
    ScopedMem<WCHAR> pdfFile, srcFile;
    BOOL line = 0, col = 0, newWindow = 0, setFocus = 0;
    const WCHAR *next = str::Parse(cmd, L"[" DDECOMMAND_SYNC L"(\"%S\",%? \"%S\",%u,%u)]",
                                   &pdfFile, &srcFile, &line, &col);
    if (!next)
        next = str::Parse(cmd, L"[" DDECOMMAND_SYNC L"(\"%S\",%? \"%S\",%u,%u,%u,%u)]",
                          &pdfFile, &srcFile, &line, &col, &newWindow, &setFocus);
    // allow to omit the pdffile path, so that editors don't have to know about
    // multi-file projects (requires that the PDF has already been opened)
    if (!next) {
        pdfFile.Set(NULL);
        next = str::Parse(cmd, L"[" DDECOMMAND_SYNC L"(\"%S\",%u,%u)]",
                          &srcFile, &line, &col);
        if (!next)
            next = str::Parse(cmd, L"[" DDECOMMAND_SYNC L"(\"%S\",%u,%u,%u,%u)]",
                              &srcFile, &line, &col, &newWindow, &setFocus);
    }

    if (!next)
        return NULL;

    WindowInfo *win = NULL;
    if (pdfFile) {
        // check if the PDF is already opened
        win = FindWindowInfoByFile(pdfFile, !newWindow);
        // if not then open it
        if (newWindow || !win) {
            LoadArgs args(pdfFile, !newWindow ? win : NULL);
            win = LoadDocument(args);
        } else if (win && !win->IsDocLoaded()) {
            ReloadDocument(win);
        }
    }
    else {
        // check if any opened PDF has sync information for the source file
        win = FindWindowInfoBySyncFile(srcFile, true);
        if (win && newWindow) {
            LoadArgs args(win->loadedFilePath);
            win = LoadDocument(args);
        }
    }

    if (!win || win->GetEngineType() != Engine_PDF)
        return next;
    if (!win->AsFixed()->pdfSync)
        return next;

    ack.fAck = 1;
    CrashIf(!win->AsFixed());
    UINT page;
    Vec<RectI> rects;
    int ret = win->AsFixed()->pdfSync->SourceToDoc(srcFile, line, col, &page, rects);
    ShowForwardSearchResult(win, srcFile, line, col, ret, page, rects);
    if (setFocus)
        win->Focus();

    return next;
}

// Open file DDE command, format:
// [<DDECOMMAND_OPEN>("<pdffilepath>"[,<newwindow>,<setfocus>,<forcerefresh>])]
static const WCHAR *HandleOpenCmd(const WCHAR *cmd, DDEACK& ack)
{
    ScopedMem<WCHAR> pdfFile;
    BOOL newWindow = 0, setFocus = 0, forceRefresh = 0;
    const WCHAR *next = str::Parse(cmd, L"[" DDECOMMAND_OPEN L"(\"%S\")]", &pdfFile);
    if (!next)
        next = str::Parse(cmd, L"[" DDECOMMAND_OPEN L"(\"%S\",%u,%u,%u)]",
                          &pdfFile, &newWindow, &setFocus, &forceRefresh);
    if (!next)
        return NULL;

    WindowInfo *win = FindWindowInfoByFile(pdfFile, !newWindow);
    if (newWindow || !win) {
        LoadArgs args(pdfFile, !newWindow ? win : NULL);
        win = LoadDocument(args);
    } else if (win && !win->IsDocLoaded()) {
        ReloadDocument(win);
        forceRefresh = 0;
    }

    assert(!win || !win->IsAboutWindow());
    if (!win)
        return next;

    ack.fAck = 1;
    if (forceRefresh)
        ReloadDocument(win, true);
    if (setFocus)
        win->Focus();

    return next;
}

// Jump to named destination DDE command. Command format:
// [<DDECOMMAND_GOTO>("<pdffilepath>", "<destination name>")]
static const WCHAR *HandleGotoCmd(const WCHAR *cmd, DDEACK& ack)
{
    ScopedMem<WCHAR> pdfFile, destName;
    const WCHAR *next = str::Parse(cmd, L"[" DDECOMMAND_GOTO L"(\"%S\",%? \"%S\")]",
                                   &pdfFile, &destName);
    if (!next)
        return NULL;

    WindowInfo *win = FindWindowInfoByFile(pdfFile, true);
    if (!win)
        return next;
    if (!win->IsDocLoaded()) {
        ReloadDocument(win);
        if (!win->IsDocLoaded())
            return next;
    }

    win->linkHandler->GotoNamedDest(destName);
    ack.fAck = 1;
    win->Focus();
    return next;
}

// Jump to page DDE command. Format:
// [<DDECOMMAND_PAGE>("<pdffilepath>", <page number>)]
static const WCHAR *HandlePageCmd(const WCHAR *cmd, DDEACK& ack)
{
    ScopedMem<WCHAR> pdfFile;
    UINT page;
    const WCHAR *next = str::Parse(cmd, L"[" DDECOMMAND_PAGE L"(\"%S\",%u)]",
                                   &pdfFile, &page);
    if (!next)
        return NULL;

    // check if the PDF is already opened
    WindowInfo *win = FindWindowInfoByFile(pdfFile, true);
    if (!win)
        return next;
    if (!win->IsDocLoaded()) {
        ReloadDocument(win);
        if (!win->IsDocLoaded())
            return next;
    }

    if (!win->ctrl->ValidPageNo(page))
        return next;

    win->ctrl->GoToPage(page, true);
    ack.fAck = 1;
    win->Focus();
    return next;
}

// Set view mode and zoom level. Format:
// [<DDECOMMAND_SETVIEW>("<pdffilepath>", "<view mode>", <zoom level>[, <scrollX>, <scrollY>])]
static const WCHAR *HandleSetViewCmd(const WCHAR *cmd, DDEACK& ack)
{
    ScopedMem<WCHAR> pdfFile, viewMode;
    float zoom = INVALID_ZOOM;
    PointI scroll(-1, -1);
    const WCHAR *next = str::Parse(cmd, L"[" DDECOMMAND_SETVIEW L"(\"%S\",%? \"%S\",%f)]",
                                   &pdfFile, &viewMode, &zoom);
    if (!next)
        next = str::Parse(cmd, L"[" DDECOMMAND_SETVIEW L"(\"%S\",%? \"%S\",%f,%d,%d)]",
                          &pdfFile, &viewMode, &zoom, &scroll.x, &scroll.y);
    if (!next)
        return NULL;

    WindowInfo *win = FindWindowInfoByFile(pdfFile, true);
    if (!win)
        return next;
    if (!win->IsDocLoaded()) {
        ReloadDocument(win);
        if (!win->IsDocLoaded())
            return next;
    }

    DisplayMode mode = prefs::conv::ToDisplayMode(viewMode, DM_AUTOMATIC);
    if (mode != DM_AUTOMATIC)
        SwitchToDisplayMode(win, mode);

    if (zoom != INVALID_ZOOM)
        ZoomToSelection(win, zoom);

    if ((scroll.x != -1 || scroll.y != -1) && win->AsFixed()) {
        DisplayModel *dm = win->AsFixed();
        ScrollState ss = dm->GetScrollState();
        ss.x = scroll.x;
        ss.y = scroll.y;
        dm->SetScrollState(ss);
    }

    ack.fAck = 1;
    return next;
}

static void HandleDdeCmds(const WCHAR *cmd, DDEACK& ack)
{
    while (!str::IsEmpty(cmd)) {
        const WCHAR *nextCmd = NULL;
        if (!nextCmd) nextCmd = HandleSyncCmd(cmd, ack);
        if (!nextCmd) nextCmd = HandleOpenCmd(cmd, ack);
        if (!nextCmd) nextCmd = HandleGotoCmd(cmd, ack);
        if (!nextCmd) nextCmd = HandlePageCmd(cmd, ack);
        if (!nextCmd) nextCmd = HandleSetViewCmd(cmd, ack);
        if (!nextCmd) {
            ScopedMem<WCHAR> tmp;
            nextCmd = str::Parse(cmd, L"%S]", &tmp);
        }
        cmd = nextCmd;
    }
}

LRESULT OnDDExecute(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    UINT_PTR lo, hi;
    UnpackDDElParam(WM_DDE_EXECUTE, lparam, &lo, &hi);

    DDEACK ack = { 0 };
    LPVOID command = GlobalLock((HGLOBAL)hi);
    if (command) {
        ScopedMem<WCHAR> cmd;
        if (IsWindowUnicode((HWND)wparam))
            cmd.Set(str::Dup((const WCHAR *)command));
        else
            cmd.Set(str::conv::FromAnsi((const char *)command));
        HandleDdeCmds(cmd, ack);
    }
    GlobalUnlock((HGLOBAL)hi);

    lparam = ReuseDDElParam(lparam, WM_DDE_EXECUTE, WM_DDE_ACK, *(WORD *)&ack, hi);
    PostMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)hwnd, lparam);
    return 0;
}

LRESULT OnDDETerminate(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    // Respond with another WM_DDE_TERMINATE message
    PostMessage((HWND)wparam, WM_DDE_TERMINATE, (WPARAM)hwnd, 0L);
    return 0;
}

LRESULT OnCopyData(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lparam;
    if (!cds || cds->dwData != 0x44646557 /* DdeW */ || wparam)
        return FALSE;

    const WCHAR *cmd = (const WCHAR *)cds->lpData;
    if (cmd[cds->cbData / sizeof(WCHAR) - 1])
        return FALSE;

    DDEACK ack = { 0 };
    HandleDdeCmds(cmd, ack);
    return ack.fAck ? TRUE : FALSE;
}
