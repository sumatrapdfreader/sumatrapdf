/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* Code related to:
* user-initiated search
* PDF forward-search via DDE
*/

#include "BaseUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"

#include "Search.h"
#include "resource.h"
#include "translations.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "Notifications.h"
#include "PdfSync.h"

#include "SumatraDialogs.h"
#include "AppTools.h"

bool NeedsFindUI(WindowInfo *win)
{
    return !win->IsDocLoaded() || win->dm->engine && !win->dm->engine->IsImageCollection();
}

void OnMenuFind(WindowInfo& win)
{
    if (!win.IsDocLoaded() || !NeedsFindUI(&win))
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.toolbarVisible && !win.fullScreen && PM_DISABLED == win.presentation) {
        if (GetFocus() == win.hwndFindBox)
            SendMessage(win.hwndFindBox, WM_SETFOCUS, 0, 0);
        else
            SetFocus(win.hwndFindBox);
        return;
    }

    ScopedMem<TCHAR> previousFind(win::GetText(win.hwndFindBox));
    WORD state = (WORD)SendMessage(win.hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    bool matchCase = (state & TBSTATE_CHECKED) != 0;

    ScopedMem<TCHAR> findString(Dialog_Find(win.hwndFrame, previousFind, &matchCase));
    if (!findString)
        return;

    win::SetText(win.hwndFindBox, findString);
    Edit_SetModify(win.hwndFindBox, TRUE);

    bool matchCaseChanged = matchCase != (0 != (state & TBSTATE_CHECKED));
    if (matchCaseChanged) {
        if (matchCase)
            state |= TBSTATE_CHECKED;
        else
            state &= ~TBSTATE_CHECKED;
        SendMessage(win.hwndToolbar, TB_SETSTATE, IDM_FIND_MATCH, state);
        win.dm->textSearch->SetSensitive(matchCase);
    }

    FindTextOnThread(&win);
}

void OnMenuFindNext(WindowInfo& win)
{
    if (!NeedsFindUI(&win))
        return;
    if (SendMessage(win.hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_NEXT, 0))
        FindTextOnThread(&win, FIND_FORWARD);
}

void OnMenuFindPrev(WindowInfo& win)
{
    if (!NeedsFindUI(&win))
        return;
    if (SendMessage(win.hwndToolbar, TB_ISBUTTONENABLED, IDM_FIND_PREV, 0))
        FindTextOnThread(&win, FIND_BACKWARD);
}

void OnMenuFindMatchCase(WindowInfo& win)
{
    if (!NeedsFindUI(&win))
        return;
    WORD state = (WORD)SendMessage(win.hwndToolbar, TB_GETSTATE, IDM_FIND_MATCH, 0);
    win.dm->textSearch->SetSensitive((state & TBSTATE_CHECKED) != 0);
    Edit_SetModify(win.hwndFindBox, TRUE);
}

static void ShowSearchResult(WindowInfo& win, TextSel *result, bool addNavPt)
{
   assert(result->len > 0);
   if (addNavPt || !win.dm->pageShown(result->pages[0]) ||
       (win.dm->zoomVirtual() == ZOOM_FIT_PAGE || win.dm->zoomVirtual() == ZOOM_FIT_CONTENT))
       win.dm->goToPage(result->pages[0], 0, addNavPt);

   win.dm->textSelection->CopySelection(win.dm->textSearch);

   UpdateTextSelection(win, false);
   win.dm->ShowResultRectToScreen(result);
   win.RepaintAsync();
}

void ClearSearchResult(WindowInfo& win)
{
   DeleteOldSelectionInfo(win, true);
   win.RepaintAsync();
}

class UpdateFindStatusWorkItem : public UIThreadWorkItem {
   NotificationWnd *wnd;
   int current, total;

public:
   UpdateFindStatusWorkItem(WindowInfo *win, NotificationWnd *wnd, int current, int total)
       : UIThreadWorkItem(win), wnd(wnd), current(current), total(total) { }

   virtual void Execute() {
       if (WindowInfoStillValid(win) && !win->findCanceled && win->notifications->Contains(wnd))
           wnd->UpdateProgress(current, total);
   }
};

struct FindThreadData : public ProgressUpdateUI {
   WindowInfo *win;
   TextSearchDirection direction;
   bool wasModified;
   TCHAR *text;

   FindThreadData(WindowInfo& win, TextSearchDirection direction, HWND findBox) :
       win(&win), direction(direction) {
       text = win::GetText(findBox);
       wasModified = Edit_GetModify(findBox);
   }
   ~FindThreadData() { free(text); }

   void ShowUI() const {
       const LPARAM disable = (LPARAM)MAKELONG(0, 0);

       NotificationWnd *wnd = new NotificationWnd(win->hwndCanvas, _T(""), _TR("Searching %d of %d..."), win->notifications);
       // let win->messages own the NotificationWnd (FindThreadData might get deleted before)
       win->notifications->Add(wnd, NG_FIND_PROGRESS);

       SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, disable);
       SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, disable);
       SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, disable);
   }

   void HideUI(NotificationWnd *wnd, bool success, bool loopedAround) const {
       LPARAM enable = (LPARAM)MAKELONG(1, 0);

       SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_PREV, enable);
       SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_NEXT, enable);
       SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, IDM_FIND_MATCH, enable);

       if (!success && !loopedAround || !wnd) // i.e. canceled
           win->notifications->RemoveNotification(wnd);
       else if (!success && loopedAround)
           wnd->UpdateMessage(_TR("No matches were found"), 3000);
       else if (!loopedAround) {
           ScopedMem<TCHAR> buf(str::Format(_TR("Found text at page %d"), win->dm->currentPageNo()));
           wnd->UpdateMessage(buf, 3000);
       } else {
           ScopedMem<TCHAR> buf(str::Format(_TR("Found text at page %d (again)"), win->dm->currentPageNo()));
           wnd->UpdateMessage(buf, 3000, true);
       }    
   }

   virtual bool UpdateProgress(int current, int total) {
       if (!WindowInfoStillValid(win) || !win->notifications->GetFirstInGroup(NG_FIND_PROGRESS) || win->findCanceled)
           return false;
       QueueWorkItem(new UpdateFindStatusWorkItem(win, win->notifications->GetFirstInGroup(NG_FIND_PROGRESS), current, total));
       return true;
   }
};

class FindEndWorkItem : public UIThreadWorkItem {
   FindThreadData *ftd;
   TextSel*textSel;
   bool    wasModifiedCanceled;
   bool    loopedAround;

public:
   FindEndWorkItem(WindowInfo *win, FindThreadData *ftd, TextSel *textSel,
                   bool wasModifiedCanceled, bool loopedAround=false) :
       UIThreadWorkItem(win), ftd(ftd), textSel(textSel),
       loopedAround(loopedAround), wasModifiedCanceled(wasModifiedCanceled) { }
   ~FindEndWorkItem() { delete ftd; }

   virtual void Execute() {
       if (!WindowInfoStillValid(win))
           return;
       if (!win->IsDocLoaded()) {
           // the UI has already been disabled and hidden
       } else if (textSel) {
           ShowSearchResult(*win, textSel, wasModifiedCanceled);
           ftd->HideUI(win->notifications->GetFirstInGroup(NG_FIND_PROGRESS), true, loopedAround);
       } else {
           // nothing found or search canceled
           ClearSearchResult(*win);
           ftd->HideUI(win->notifications->GetFirstInGroup(NG_FIND_PROGRESS), false, !wasModifiedCanceled);
       }

       HANDLE hThread = win->findThread;
       win->findThread = NULL;
       CloseHandle(hThread);
   }
};

static DWORD WINAPI FindThread(LPVOID data)
{
   FindThreadData *ftd = (FindThreadData *)data;
   assert(ftd && ftd->win && ftd->win->dm);
   WindowInfo *win = ftd->win;

   TextSel *rect;
   win->dm->textSearch->SetDirection(ftd->direction);
   if (ftd->wasModified || !win->dm->validPageNo(win->dm->textSearch->GetCurrentPageNo()) ||
       !win->dm->getPageInfo(win->dm->textSearch->GetCurrentPageNo())->visibleRatio)
       rect = win->dm->textSearch->FindFirst(win->dm->currentPageNo(), ftd->text, ftd);
   else
       rect = win->dm->textSearch->FindNext(ftd);

   bool loopedAround = false;
   if (!win->findCanceled && !rect) {
       // With no further findings, start over (unless this was a new search from the beginning)
       int startPage = (FIND_FORWARD == ftd->direction) ? 1 : win->dm->pageCount();
       if (!ftd->wasModified || win->dm->currentPageNo() != startPage) {
           loopedAround = true;
           MessageBeep(MB_ICONINFORMATION);
           rect = win->dm->textSearch->FindFirst(startPage, ftd->text, ftd);
       }
   }

   if (!win->findCanceled && rect)
       QueueWorkItem(new FindEndWorkItem(win, ftd, rect, ftd->wasModified, loopedAround));
   else
       QueueWorkItem(new FindEndWorkItem(win, ftd, NULL, win->findCanceled));

   return 0;
}

void FindTextOnThread(WindowInfo* win, TextSearchDirection direction)
{
   win->AbortFinding(true);

   FindThreadData *ftd = new FindThreadData(*win, direction, win->hwndFindBox);
   Edit_SetModify(win->hwndFindBox, FALSE);

   if (str::IsEmpty(ftd->text)) {
       delete ftd;
       return;
   }

   ftd->ShowUI();
   win->findThread = CreateThread(NULL, 0, FindThread, ftd, 0, 0);
}

void PaintForwardSearchMark(WindowInfo& win, HDC hdc)
{
    PageInfo *pageInfo = win.dm->getPageInfo(win.fwdSearchMark.page);
    if (!pageInfo || 0.0 == pageInfo->visibleRatio)
        return;
    
    // Draw the rectangles highlighting the forward search results
    for (UINT i = 0; i < win.fwdSearchMark.rects.Count(); i++) {
        RectD recD = win.fwdSearchMark.rects[i].Convert<double>();
        RectI recI = win.dm->CvtToScreen(win.fwdSearchMark.page, recD);
        if (gGlobalPrefs.fwdSearchOffset > 0) {
            recI.x = max(pageInfo->pageOnScreen.x, 0) + (int)(gGlobalPrefs.fwdSearchOffset * win.dm->zoomReal());
            recI.dx = (int)((gGlobalPrefs.fwdSearchWidth > 0 ? gGlobalPrefs.fwdSearchWidth : 15.0) * win.dm->zoomReal());
            recI.y -= 4;
            recI.dy += 8;
        }
        BYTE alpha = (BYTE)(0x5f * 1.0f * (HIDE_FWDSRCHMARK_STEPS - win.fwdSearchMark.hideStep) / HIDE_FWDSRCHMARK_STEPS);
        PaintTransparentRectangle(hdc, win.canvasRc, &recI, gGlobalPrefs.fwdSearchColor, alpha, 0);
    }
}

// returns true if the double-click was handled and false if it wasn't
bool OnInverseSearch(WindowInfo& win, int x, int y)
{
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) return false;
    if (!win.IsDocLoaded() || win.dm->engineType != Engine_PDF) return false;

    // Clear the last forward-search result
    win.fwdSearchMark.rects.Reset();
    InvalidateRect(win.hwndCanvas, NULL, FALSE);

    // On double-clicking error message will be shown to the user
    // if the PDF does not have a synchronization file
    if (!win.pdfsync) {
        int err = Synchronizer::Create(win.loadedFilePath, win.dm, &win.pdfsync);
        if (err == PDFSYNCERR_SYNCFILE_NOTFOUND) {
            DBG_OUT("Pdfsync: Sync file not found!\n");
            // Fall back to selecting a word when double-clicking over text in
            // a document with no corresponding synchronization file
            if (win.dm->IsOverText(PointI(x, y)))
                return false;
            // In order to avoid confusion for non-LaTeX users, we do not show
            // any error message if the SyncTeX enhancements are hidden from UI
            if (gGlobalPrefs.enableTeXEnhancements)
                ShowNotification(&win, _TR("No synchronization file found"));
            return true;
        }
        if (err != PDFSYNCERR_SUCCESS) {
            DBG_OUT("Pdfsync: Sync file cannot be loaded!\n");
            ShowNotification(&win, _TR("Synchronization file cannot be opened"));
            return true;
        }
        gGlobalPrefs.enableTeXEnhancements = true;
    }

    int pageNo = win.dm->GetPageNoByPoint(PointI(x, y));
    if (!win.dm->validPageNo(pageNo))
        return false;

    PointI pt = win.dm->CvtFromScreen(PointI(x, y), pageNo).Convert<int>();
    ScopedMem<TCHAR> srcfilepath;
    UINT line, col;
    int err = win.pdfsync->pdf_to_source(pageNo, pt, srcfilepath, &line, &col);
    if (err != PDFSYNCERR_SUCCESS) {
        DBG_OUT("cannot sync from pdf to source!\n");
        ShowNotification(&win, _TR("No synchronization info at this position"));
        return true;
    }

    TCHAR *inverseSearch = gGlobalPrefs.inverseSearchCmdLine;
    if (!inverseSearch)
        // Detect a text editor and use it as the default inverse search handler for now
        inverseSearch = AutoDetectInverseSearchCommands();

    ScopedMem<TCHAR> cmdline;
    if (inverseSearch)
        cmdline.Set(win.pdfsync->prepare_commandline(inverseSearch, srcfilepath, line, col));
    if (!str::IsEmpty(cmdline.Get())) {
        ScopedHandle process(LaunchProcess(cmdline));
        if (!process)
            ShowNotification(&win, _TR("Cannot start inverse search command. Please check the command line in the settings."));
    }
    else if (gGlobalPrefs.enableTeXEnhancements)
        ShowNotification(&win, _TR("Cannot start inverse search command. Please check the command line in the settings."));

    if (inverseSearch != gGlobalPrefs.inverseSearchCmdLine)
        free(inverseSearch);

    return true;
}

// Show the result of a PDF forward-search synchronization (initiated by a DDE command)
void WindowInfo::ShowForwardSearchResult(const TCHAR *fileName, UINT line, UINT col, UINT ret, UINT page, Vec<RectI> &rects)
{
    this->fwdSearchMark.rects.Reset();
    if (ret == PDFSYNCERR_SUCCESS && rects.Count() > 0) {
        // remember the position of the search result for drawing the rect later on
        const PageInfo *pi = this->dm->getPageInfo(page);
        if (pi) {
            fwdSearchMark.rects = rects;
            fwdSearchMark.page = page;
            fwdSearchMark.show = true;
            if (!gGlobalPrefs.fwdSearchPermanent)  {
                fwdSearchMark.hideStep = 0;
                SetTimer(this->hwndCanvas, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DELAY_IN_MS, NULL);
            }

            // Scroll to show the overall highlighted zone
            int pageNo = page;
            RectI overallrc = rects[0];
            for (size_t i = 1; i < rects.Count(); i++)
                overallrc = overallrc.Union(rects[i]);
            TextSel res = { 1, &pageNo, &overallrc };
            if (!this->dm->pageVisible(page))
                this->dm->goToPage(page, 0, true);
            if (!this->dm->ShowResultRectToScreen(&res))
                this->RepaintAsync();
            if (IsIconic(this->hwndFrame))
                ShowWindowAsync(this->hwndFrame, SW_RESTORE);
            return;
        }
    }

    ScopedMem<TCHAR> buf;
    if (ret == PDFSYNCERR_SYNCFILE_NOTFOUND )
        ShowNotification(this, _TR("No synchronization file found"));
    else if (ret == PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED)
        ShowNotification(this, _TR("Synchronization file cannot be opened"));
    else if (ret == PDFSYNCERR_INVALID_PAGE_NUMBER)
        buf.Set(str::Format(_TR("Page number %u inexistant"), page));
    else if (ret == PDFSYNCERR_NO_SYNC_AT_LOCATION)
        ShowNotification(this, _TR("No synchronization info at this position"));
    else if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE)
        buf.Set(str::Format(_TR("Unknown source file (%s)"), fileName));
    else if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE)
        buf.Set(str::Format(_TR("Source file %s has no synchronization point"), fileName));
    else if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE)
        buf.Set(str::Format(_TR("No result found around line %u in file %s"), line, fileName));
    else if (ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD)
        buf.Set(str::Format(_TR("No result found around line %u in file %s"), line, fileName));
    if (buf)
        ShowNotification(this, buf);
}

