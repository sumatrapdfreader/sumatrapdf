/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* Code related to:
 * user-initiated search
 * DDE commands, including search
 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineManager.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "PdfSync.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "AppTools.h"
#include "Search.h"
#include "Selection.h"
#include "SumatraDialogs.h"
#include "Translations.h"

// open file command
//  format: [Open("<pdffilepath>"[,<newwindow>,<setfocus>,<forcerefresh>])]
//    if newwindow = 1 then a new window is created even if the file is already open
//    if focus = 1 then the focus is set to the window
//  eg: [Open("c:\file.pdf", 1, 1, 0)]

bool gIsStartup = false;
WStrVec gDdeOpenOnStartup;

NotificationGroupId NG_FIND_PROGRESS = "findProgress";

// don't show the Search UI for document types that don't
// support extracting text and/or navigating to a specific
// text selection; default to showing it, since most users
// will never use a format that does not support search
bool NeedsFindUI(WindowInfo* win) {
    if (!win->IsDocLoaded()) {
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

void OnMenuFind(WindowInfo* win) {
    if (win->AsChm()) {
        win->AsChm()->FindInCurrentPage();
        return;
    }

    if (!win->AsFixed() || !NeedsFindUI(win)) {
        return;
    }

    // copy any selected text to the find bar, if it's still empty
    DisplayModel* dm = win->AsFixed();
    if (dm->textSelection->result.len > 0 && Edit_GetTextLength(win->hwndFindBox) == 0) {
        AutoFreeWstr selection(dm->textSelection->ExtractText(L" "));
        str::NormalizeWS(selection);
        if (!str::IsEmpty(selection.Get())) {
            win::SetText(win->hwndFindBox, selection);
            Edit_SetModify(win->hwndFindBox, TRUE);
        }
    }

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs->showToolbar && !win->isFullScreen && !win->presentation) {
        if (IsFocused(win->hwndFindBox)) {
            SendMessage(win->hwndFindBox, WM_SETFOCUS, 0, 0);
        } else {
            SetFocus(win->hwndFindBox);
        }
        return;
    }

    AutoFreeWstr previousFind(win::GetText(win->hwndFindBox));
    WORD state = (WORD)SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    bool matchCase = (state & TBSTATE_CHECKED) != 0;

    AutoFreeWstr findString(Dialog_Find(win->hwndFrame, previousFind, &matchCase));
    if (!findString) {
        return;
    }

    win::SetText(win->hwndFindBox, findString);
    Edit_SetModify(win->hwndFindBox, TRUE);

    bool matchCaseChanged = matchCase != (0 != (state & TBSTATE_CHECKED));
    if (matchCaseChanged) {
        if (matchCase) {
            state |= TBSTATE_CHECKED;
        } else {
            state &= ~TBSTATE_CHECKED;
        }
        SendMessage(win->hwndToolbar, TB_SETSTATE, IDM_FIND_MATCH, state);
        dm->textSearch->SetSensitive(matchCase);
    }

    FindTextOnThread(win, TextSearchDirection::Forward, true);
}

void OnMenuFindNext(WindowInfo* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (SendMessage(win->hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_NEXT, 0)) {
        FindTextOnThread(win, TextSearchDirection::Forward, true);
    }
}

void OnMenuFindPrev(WindowInfo* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (SendMessage(win->hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_PREV, 0)) {
        FindTextOnThread(win, TextSearchDirection::Backward, true);
    }
}

void OnMenuFindMatchCase(WindowInfo* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    WORD state = (WORD)SendMessage(win->hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    win->AsFixed()->textSearch->SetSensitive((state & TBSTATE_CHECKED) != 0);
    Edit_SetModify(win->hwndFindBox, TRUE);
}

void OnMenuFindSel(WindowInfo* win, TextSearchDirection direction) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!win->currentTab->selectionOnPage || 0 == dm->textSelection->result.len) {
        return;
    }

    AutoFreeWstr selection(dm->textSelection->ExtractText(L" "));
    str::NormalizeWS(selection);
    if (str::IsEmpty(selection.Get())) {
        return;
    }

    win::SetText(win->hwndFindBox, selection);
    AbortFinding(win, false); // cancel "find as you type"
    Edit_SetModify(win->hwndFindBox, FALSE);
    dm->textSearch->SetLastResult(dm->textSelection);

    FindTextOnThread(win, direction, true);
}

static void ShowSearchResult(WindowInfo* win, TextSel* result, bool addNavPt) {
    CrashIf(0 == result->len || !result->pages || !result->rects);
    if (0 == result->len || !result->pages || !result->rects) {
        return;
    }

    DisplayModel* dm = win->AsFixed();
    if (addNavPt || !dm->PageShown(result->pages[0]) ||
        (dm->GetZoomVirtual() == ZOOM_FIT_PAGE || dm->GetZoomVirtual() == ZOOM_FIT_CONTENT)) {
        win->ctrl->GoToPage(result->pages[0], addNavPt);
    }

    dm->textSelection->CopySelection(dm->textSearch);
    UpdateTextSelection(win, false);
    dm->ShowResultRectToScreen(result);
    win->RepaintAsync();
}

void ClearSearchResult(WindowInfo* win) {
    DeleteOldSelectionInfo(win, true);
    win->RepaintAsync();
}

static void UpdateFindStatusTask(WindowInfo* win, NotificationWnd* wnd, int current, int total) {
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
    WindowInfo* win;
    TextSearchDirection direction;
    bool wasModified;
    AutoFreeWstr text;
    // owned by win->notifications, as FindThreadData
    // can be deleted before the notification times out
    NotificationWnd* wnd;
    HANDLE thread;

    FindThreadData(WindowInfo* win, TextSearchDirection direction, HWND findBox)
        : win(win),
          direction(direction),
          text(win::GetText(findBox)),
          wasModified(Edit_GetModify(findBox)),
          wnd(nullptr),
          thread(nullptr) {
    }
    ~FindThreadData() {
        CloseHandle(thread);
    }

    void ShowUI(bool showProgress) {
        const LPARAM disable = (LPARAM)MAKELONG(0, 0);

        if (showProgress) {
            auto notificationsInCb = this->win->notifications;
            wnd = new NotificationWnd(win->hwndCanvas, 0);
            wnd->wndRemovedCb = [notificationsInCb](NotificationWnd* wnd) {
                notificationsInCb->RemoveNotification(wnd);
            };
            wnd->Create(L"", _TR("Searching %d of %d..."));
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

        if (!win->notifications->Contains(wnd)) {
            /* our notification has been replaced or closed (or never created) */;
        } else if (!success && !loopedAround) {
            // i.e. canceled
            win->notifications->RemoveNotification(wnd);
        } else if (!success && loopedAround) {
            wnd->UpdateMessage(_TR("No matches were found"), 3000);
        } else {
            AutoFreeWstr label(win->ctrl->GetPageLabel(win->AsFixed()->textSearch->GetSearchHitStartPageNo()));
            AutoFreeWstr buf(str::Format(_TR("Found text at page %s"), label.Get()));
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
        uitask::Post([=] { UpdateFindStatusTask(win, wnd, current, total); });
    }

    virtual bool WasCanceled() {
        return !WindowInfoStillValid(win) || win->findCanceled;
    }
};

static void FindEndTask(WindowInfo* win, FindThreadData* ftd, TextSel* textSel, bool wasModifiedCanceled,
                        bool loopedAround) {
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
        ShowSearchResult(win, textSel, wasModifiedCanceled);
        ftd->HideUI(true, loopedAround);
    } else {
        // nothing found or search canceled
        ClearSearchResult(win);
        ftd->HideUI(false, !wasModifiedCanceled);
    }
    win->findThread = nullptr;
    delete ftd;
}

static DWORD WINAPI FindThread(LPVOID data) {
    FindThreadData* ftd = (FindThreadData*)data;
    AssertCrash(ftd && ftd->win && ftd->win->ctrl && ftd->win->ctrl->AsFixed());
    WindowInfo* win = ftd->win;
    DisplayModel* dm = win->AsFixed();

    TextSel* rect;
    dm->textSearch->SetDirection(ftd->direction);
    if (ftd->wasModified || !win->ctrl->ValidPageNo(dm->textSearch->GetCurrentPageNo()) ||
        !dm->GetPageInfo(dm->textSearch->GetCurrentPageNo())->visibleRatio) {
        rect = dm->textSearch->FindFirst(win->ctrl->CurrentPageNo(), ftd->text, ftd);
    } else {
        rect = dm->textSearch->FindNext(ftd);
    }

    bool loopedAround = false;
    if (!win->findCanceled && !rect) {
        // With no further findings, start over (unless this was a new search from the beginning)
        int startPage = (TextSearchDirection::Forward == ftd->direction) ? 1 : win->ctrl->PageCount();
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
        uitask::Post([=] { FindEndTask(win, ftd, rect, ftd->wasModified, loopedAround); });
    } else {
        uitask::Post([=] { FindEndTask(win, ftd, nullptr, win->findCanceled, false); });
    }

    return 0;
}

void AbortFinding(WindowInfo* win, bool hideMessage) {
    if (win->findThread) {
        win->findCanceled = true;
        WaitForSingleObject(win->findThread, INFINITE);
    }
    win->findCanceled = false;

    if (hideMessage) {
        win->notifications->RemoveForGroup(NG_FIND_PROGRESS);
    }
}

void FindTextOnThread(WindowInfo* win, TextSearchDirection direction, bool showProgress) {
    AbortFinding(win, true);

    FindThreadData* ftd = new FindThreadData(win, direction, win->hwndFindBox);
    Edit_SetModify(win->hwndFindBox, FALSE);

    if (str::IsEmpty(ftd->text.Get())) {
        delete ftd;
        return;
    }

    ftd->ShowUI(showProgress);
    win->findThread = nullptr;
    win->findThread = CreateThread(nullptr, 0, FindThread, ftd, 0, 0);
    ftd->thread = win->findThread; // safe because only accesssed on ui thread
}

void PaintForwardSearchMark(WindowInfo* win, HDC hdc) {
    CrashIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();
    int pageNo = win->fwdSearchMark.page;
    PageInfo* pageInfo = dm->GetPageInfo(pageNo);
    if (!pageInfo || 0.0 == pageInfo->visibleRatio) {
        return;
    }

    int hiLiWidth = gGlobalPrefs->forwardSearch.highlightWidth;
    int hiLiOff = gGlobalPrefs->forwardSearch.highlightOffset;

    // Draw the rectangles highlighting the forward search results
    Vec<RectI> rects;
    for (size_t i = 0; i < win->fwdSearchMark.rects.size(); i++) {
        RectI rect = win->fwdSearchMark.rects.at(i);
        rect = dm->CvtToScreen(pageNo, rect.Convert<double>());
        if (hiLiOff > 0) {
            float zoom = dm->GetZoomReal(pageNo);
            rect.x = std::max(pageInfo->pageOnScreen.x, 0) + (int)(hiLiOff * zoom);
            rect.dx = (int)((hiLiWidth > 0 ? hiLiWidth : 15.0) * zoom);
            rect.y -= 4;
            rect.dy += 8;
        }
        rects.Append(rect);
    }

    BYTE alpha = (BYTE)(0x5f * 1.0f * (HIDE_FWDSRCHMARK_STEPS - win->fwdSearchMark.hideStep) / HIDE_FWDSRCHMARK_STEPS);
    PaintTransparentRectangles(hdc, win->canvasRc, rects, gGlobalPrefs->forwardSearch.highlightColor, alpha, 0);
}

// returns true if the double-click was handled and false if it wasn't
bool OnInverseSearch(WindowInfo* win, int x, int y) {
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) {
        return false;
    }
    TabInfo* tab = win->currentTab;
    if (!tab || tab->GetEngineType() != kindEnginePdf) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();

    // Clear the last forward-search result
    win->fwdSearchMark.rects.Reset();
    InvalidateRect(win->hwndCanvas, nullptr, FALSE);

    // On double-clicking error message will be shown to the user
    // if the PDF does not have a synchronization file
    if (!dm->pdfSync) {
        int err = Synchronizer::Create(tab->filePath, dm->GetEngine(), &dm->pdfSync);
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
    if (!tab->ctrl->ValidPageNo(pageNo))
        return false;

    PointI pt = dm->CvtFromScreen(PointI(x, y), pageNo).ToInt();
    AutoFreeWstr srcfilepath;
    UINT line, col;
    int err = dm->pdfSync->DocToSource(pageNo, pt, srcfilepath, &line, &col);
    if (err != PDFSYNCERR_SUCCESS) {
        win->ShowNotification(_TR("No synchronization info at this position"));
        return true;
    }

    if (!file::Exists(srcfilepath)) {
        // if the source file is missing, check if it's been moved to the same place as
        // the PDF document (which happens if all files are moved together)
        AutoFreeWstr altsrcpath(path::GetDir(tab->filePath));
        altsrcpath.Set(path::Join(altsrcpath, path::GetBaseNameNoFree(srcfilepath)));
        if (!str::Eq(altsrcpath, srcfilepath) && file::Exists(altsrcpath))
            srcfilepath.Set(altsrcpath.StealData());
    }

    WCHAR* inverseSearch = gGlobalPrefs->inverseSearchCmdLine;
    if (!inverseSearch) {
        // Detect a text editor and use it as the default inverse search handler for now
        inverseSearch = AutoDetectInverseSearchCommands(nullptr);
    }

    AutoFreeWstr cmdline;
    if (inverseSearch)
        cmdline.Set(dm->pdfSync->PrepareCommandline(inverseSearch, srcfilepath, line, col));
    if (!str::IsEmpty(cmdline.Get())) {
        // resolve relative paths with relation to SumatraPDF.exe's directory
        AutoFreeWstr appDir(GetExePath());
        if (appDir)
            appDir.Set(path::GetDir(appDir));
        AutoCloseHandle process(LaunchProcess(cmdline, appDir));
        if (!process)
            win->ShowNotification(
                _TR("Cannot start inverse search command. Please check the command line in the settings."));
    } else if (gGlobalPrefs->enableTeXEnhancements)
        win->ShowNotification(
            _TR("Cannot start inverse search command. Please check the command line in the settings."));

    if (inverseSearch != gGlobalPrefs->inverseSearchCmdLine)
        free(inverseSearch);

    return true;
}

// Show the result of a PDF forward-search synchronization (initiated by a DDE command)
void ShowForwardSearchResult(WindowInfo* win, const WCHAR* fileName, UINT line, UINT col, UINT ret, UINT page,
                             Vec<RectI>& rects) {
    UNUSED(col);
    CrashIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();
    win->fwdSearchMark.rects.Reset();
    const PageInfo* pi = dm->GetPageInfo(page);
    if ((ret == PDFSYNCERR_SUCCESS) && (rects.size() > 0) && (nullptr != pi)) {
        // remember the position of the search result for drawing the rect later on
        win->fwdSearchMark.rects = rects;
        win->fwdSearchMark.page = page;
        win->fwdSearchMark.show = true;
        win->fwdSearchMark.hideStep = 0;
        if (!gGlobalPrefs->forwardSearch.highlightPermanent)
            SetTimer(win->hwndCanvas, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DELAY_IN_MS, nullptr);

        // Scroll to show the overall highlighted zone
        int pageNo = page;
        RectI overallrc = rects.at(0);
        for (size_t i = 1; i < rects.size(); i++) {
            overallrc = overallrc.Union(rects.at(i));
        }
        TextSel res = {1, 1, &pageNo, &overallrc};
        if (!dm->PageVisible(page)) {
            win->ctrl->GoToPage(page, true);
        }
        if (!dm->ShowResultRectToScreen(&res)) {
            win->RepaintAsync();
        }
        if (IsIconic(win->hwndFrame)) {
            ShowWindowAsync(win->hwndFrame, SW_RESTORE);
        }
        return;
    }

    AutoFreeWstr buf;
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

LRESULT OnDDEInitiate(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    ATOM aServer = GlobalAddAtom(PDFSYNC_DDE_SERVICE);
    ATOM aTopic = GlobalAddAtom(PDFSYNC_DDE_TOPIC);

    if (LOWORD(lparam) == aServer && HIWORD(lparam) == aTopic) {
        SendMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)hwnd, MAKELPARAM(aServer, 0));
    } else {
        GlobalDeleteAtom(aServer);
        GlobalDeleteAtom(aTopic);
    }
    return 0;
}

// DDE commands

// Synchronization command format:
// [<DDECOMMAND_SYNC>(["<pdffile>",]"<srcfile>",<line>,<col>[,<newwindow>,<setfocus>])]
static const WCHAR* HandleSyncCmd(const WCHAR* cmd, DDEACK& ack) {
    AutoFreeWstr pdfFile, srcFile;
    BOOL line = 0, col = 0, newWindow = 0, setFocus = 0;
    const WCHAR* next = str::Parse(cmd, L"[ForwardSearch(\"%S\",%? \"%S\",%u,%u)]", &pdfFile, &srcFile, &line, &col);
    if (!next)
        next = str::Parse(cmd, L"[ForwardSearch(\"%S\",%? \"%S\",%u,%u,%u,%u)]", &pdfFile, &srcFile, &line, &col,
                          &newWindow, &setFocus);
    // allow to omit the pdffile path, so that editors don't have to know about
    // multi-file projects (requires that the PDF has already been opened)
    if (!next) {
        pdfFile.Reset();
        next = str::Parse(cmd, L"[ForwardSearch(\"%S\",%u,%u)]", &srcFile, &line, &col);
        if (!next)
            next =
                str::Parse(cmd, L"[ForwardSearch(\"%S\",%u,%u,%u,%u)]", &srcFile, &line, &col, &newWindow, &setFocus);
    }

    if (!next) {
        return nullptr;
    }

    WindowInfo* win = nullptr;
    if (pdfFile) {
        // check if the PDF is already opened
        win = FindWindowInfoByFile(pdfFile, !newWindow);
        // if not then open it
        if (newWindow || !win) {
            LoadArgs args(pdfFile, !newWindow ? win : nullptr);
            win = LoadDocument(args);
        } else if (win && !win->IsDocLoaded()) {
            ReloadDocument(win);
        }
    } else {
        // check if any opened PDF has sync information for the source file
        win = FindWindowInfoBySyncFile(srcFile, true);
        if (win && newWindow) {
            LoadArgs args(win->currentTab->filePath, nullptr);
            win = LoadDocument(args);
        }
    }

    if (!win || !win->currentTab || win->currentTab->GetEngineType() != kindEnginePdf) {
        return next;
    }

    DisplayModel* dm = win->AsFixed();
    if (!dm->pdfSync) {
        return next;
    }

    ack.fAck = 1;
    UINT page;
    Vec<RectI> rects;
    int ret = dm->pdfSync->SourceToDoc(srcFile, line, col, &page, rects);
    ShowForwardSearchResult(win, srcFile, line, col, ret, page, rects);
    if (setFocus) {
        win->Focus();
    }

    return next;
}

// Open file DDE command, format:
// [<DDECOMMAND_OPEN>("<pdffilepath>"[,<newwindow>,<setfocus>,<forcerefresh>])]
static const WCHAR* HandleOpenCmd(const WCHAR* cmd, DDEACK& ack) {
    AutoFreeWstr pdfFile;
    BOOL newWindow = 0, setFocus = 0, forceRefresh = 0;
    const WCHAR* next = str::Parse(cmd, L"[Open(\"%S\")]", &pdfFile);
    if (!next) {
        const WCHAR* pat = L"[Open(\"%S\",%u,%u,%u)]";
        next = str::Parse(cmd, pat, &pdfFile, &newWindow, &setFocus, &forceRefresh);
    }
    if (!next) {
        return nullptr;
    }

    // on startup this is called while LoadDocument is in progress, which causes
    // all sort of mayhem. Queue files to be loaded in a sequence
    if (gIsStartup) {
        gDdeOpenOnStartup.Append(pdfFile.StealData());
        return next;
    }

    bool focusTab = !newWindow;
    WindowInfo* win = FindWindowInfoByFile(pdfFile, focusTab);
    if (newWindow || !win) {
        LoadArgs args(pdfFile, !newWindow ? win : nullptr);
        win = LoadDocument(args);
    } else if (win && !win->IsDocLoaded()) {
        ReloadDocument(win);
        forceRefresh = 0;
    }

    // TODO: not sure why this triggers. Seems to happen when opening multiple files
    // via Open menu in explorer. The first one is opened via cmd-line arg, the
    // rest via DDE.
    // CrashIf(win && win->IsAboutWindow());
    if (!win) {
        return next;
    }

    ack.fAck = 1;
    if (forceRefresh) {
        ReloadDocument(win, true);
    }
    if (setFocus) {
        win->Focus();
    }

    return next;
}

// DDE command: jump to named destination in an already opened document.
// Format:
// [GoToNamedDest("<pdffilepath>","<destination name>")], e.g.:
// [GoToNamedDest("c:\file.pdf", "chapter.1")]
static const WCHAR* HandleGotoCmd(const WCHAR* cmd, DDEACK& ack) {
    AutoFreeWstr pdfFile, destName;
    const WCHAR* next = str::Parse(cmd, L"[GotoNamedDest(\"%S\",%? \"%S\")]", &pdfFile, &destName);
    if (!next) {
        return nullptr;
    }

    WindowInfo* win = FindWindowInfoByFile(pdfFile, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win);
        if (!win->IsDocLoaded()) {
            return next;
        }
    }

    win->linkHandler->GotoNamedDest(destName);
    ack.fAck = 1;
    win->Focus();
    return next;
}

// DDE command: jump to a page in an already opened document.
// Format:
// [GoToPage("<pdffilepath>",<page number>)]
//  eg:
// [GoToPage("c:\file.pdf",37)]
#define DDECOMMAND_PAGE L"GotoPage"

static const WCHAR* HandlePageCmd(HWND hwnd, const WCHAR* cmd, DDEACK& ack) {
    UNUSED(hwnd);

    AutoFreeWstr pdfFile;
    UINT page = 0;
    const WCHAR* next = str::Parse(cmd, L"[GotoPage(\"%S\",%u)]", &pdfFile, &page);
    if (!next) {
        return nullptr;
    }

    // check if the PDF is already opened
    // TODO: prioritize window with HWND so that if we have the same file
    // opened in multiple tabs / windows, we operate on the one that got the message
    WindowInfo* win = FindWindowInfoByFile(pdfFile, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win);
        if (!win->IsDocLoaded()) {
            return next;
        }
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
static const WCHAR* HandleSetViewCmd(const WCHAR* cmd, DDEACK& ack) {
    AutoFreeWstr pdfFile, viewMode;
    float zoom = INVALID_ZOOM;
    PointI scroll(-1, -1);
    const WCHAR* next = str::Parse(cmd, L"[SetView(\"%S\",%? \"%S\",%f)]", &pdfFile, &viewMode, &zoom);
    if (!next) {
        next =
            str::Parse(cmd, L"[SetView(\"%S\",%? \"%S\",%f,%d,%d)]", &pdfFile, &viewMode, &zoom, &scroll.x, &scroll.y);
    }
    if (!next) {
        return nullptr;
    }

    WindowInfo* win = FindWindowInfoByFile(pdfFile, true);
    if (!win) {
        return next;
    }
    if (!win->IsDocLoaded()) {
        ReloadDocument(win);
        if (!win->IsDocLoaded()) {
            return next;
        }
    }

    DisplayMode mode = prefs::conv::ToDisplayMode(viewMode, DM_AUTOMATIC);
    if (mode != DM_AUTOMATIC) {
        SwitchToDisplayMode(win, mode);
    }

    if (zoom != INVALID_ZOOM) {
        ZoomToSelection(win, zoom);
    }

    if ((scroll.x != -1 || scroll.y != -1) && win->AsFixed()) {
        DisplayModel* dm = win->AsFixed();
        ScrollState ss = dm->GetScrollState();
        ss.x = scroll.x;
        ss.y = scroll.y;
        dm->SetScrollState(ss);
    }

    ack.fAck = 1;
    return next;
}

static void HandleDdeCmds(HWND hwnd, const WCHAR* cmd, DDEACK& ack) {
    if (str::IsEmpty(cmd)) {
        return;
    }

    {
        AutoFree tmp = strconv::WstrToUtf8(cmd);
        logf("HandleDdeCmds: '%s'\n", tmp.get());
    }

    while (!str::IsEmpty(cmd)) {
        const WCHAR* nextCmd = nullptr;
        if (!nextCmd) {
            nextCmd = HandleSyncCmd(cmd, ack);
        }
        if (!nextCmd) {
            nextCmd = HandleOpenCmd(cmd, ack);
        }
        if (!nextCmd) {
            nextCmd = HandleGotoCmd(cmd, ack);
        }
        if (!nextCmd) {
            nextCmd = HandlePageCmd(hwnd, cmd, ack);
        }
        if (!nextCmd) {
            nextCmd = HandleSetViewCmd(cmd, ack);
        }
        if (!nextCmd) {
            AutoFreeWstr tmp;
            nextCmd = str::Parse(cmd, L"%S]", &tmp);
        }
        cmd = nextCmd;

        {
            AutoFree tmp = strconv::WstrToUtf8(cmd);
            logf("HandleDdeCmds: cmd='%s'\n", tmp.get());
        }
    }
}

LRESULT OnDDExecute(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    UINT_PTR lo = 0, hi = 0;
    if (!UnpackDDElParam(WM_DDE_EXECUTE, lparam, &lo, &hi)) {
        return 0;
    }

    DDEACK ack = {0};
    LPVOID command = GlobalLock((HGLOBAL)hi);
    if (!command) {
        return 0;
    }

    AutoFreeWstr cmd;
    if (IsWindowUnicode((HWND)wparam)) {
        cmd = str::Dup((WCHAR*)command);
    } else {
        cmd = strconv::FromAnsi((const char*)command);
    }
    HandleDdeCmds(hwnd, cmd, ack);
    GlobalUnlock((HGLOBAL)hi);

    lparam = ReuseDDElParam(lparam, WM_DDE_EXECUTE, WM_DDE_ACK, *(WORD*)&ack, hi);
    PostMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)hwnd, lparam);
    return 0;
}

LRESULT OnDDETerminate(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    UNUSED(lparam);
    // Respond with another WM_DDE_TERMINATE message
    PostMessage((HWND)wparam, WM_DDE_TERMINATE, (WPARAM)hwnd, 0L);
    return 0;
}

LRESULT OnCopyData(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    UNUSED(hwnd);
    COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lparam;
    if (!cds || cds->dwData != 0x44646557 /* DdeW */ || wparam) {
        return FALSE;
    }

    const WCHAR* cmd = (const WCHAR*)cds->lpData;
    if (cmd[cds->cbData / sizeof(WCHAR) - 1]) {
        return FALSE;
    }

    DDEACK ack = {0};
    HandleDdeCmds(hwnd, cmd, ack);
    return ack.fAck ? TRUE : FALSE;
}
