/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/ThreadUtil.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "PdfSync.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "AppTools.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "SumatraDialogs.h"
#include "Translations.h"

#include "utils/Log.h"

bool gIsStartup = false;
StrVec gDdeOpenOnStartup;

Kind kNotifFindProgress = "findProgress";

// don't show the Search UI for document types that don't
// support extracting text and/or navigating to a specific
// text selection; default to showing it, since most users
// will never use a format that does not support search
bool NeedsFindUI(MainWindow* win) {
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

void FindFirst(MainWindow* win) {
    if (win->AsChm()) {
        win->AsChm()->FindInCurrentPage();
        return;
    }

    if (!win->AsFixed() || !NeedsFindUI(win)) {
        return;
    }

    // copy any selected text to the find bar, if it's still empty
    DisplayModel* dm = win->AsFixed();
    // note: used to only copy selection to search edit ctrl
    // if search edit was empty
    // auto isEditEmpty = Edit_GetTextLength(win->hwndFindEdit) == 0
    if (dm->textSelection->result.len > 0) {
        AutoFreeWStr selection(dm->textSelection->ExtractText(" "));
        str::NormalizeWSInPlace(selection);
        if (!str::IsEmpty(selection.Get())) {
            TempStr s = ToUtf8Temp(selection);
            HwndSetText(win->hwndFindEdit, s);
            Edit_SetModify(win->hwndFindEdit, TRUE);
        }
    }

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs->showToolbar && !win->isFullScreen && !win->presentation) {
        if (HwndIsFocused(win->hwndFindEdit)) {
            SendMessageW(win->hwndFindEdit, WM_SETFOCUS, 0, 0);
        } else {
            HwndSetFocus(win->hwndFindEdit);
        }
        return;
    }

    TempStr previousFind = HwndGetTextTemp(win->hwndFindEdit);
    WORD state = (WORD)SendMessageW(win->hwndToolbar, TB_GETSTATE, CmdFindMatch, 0);
    bool matchCase = (state & TBSTATE_CHECKED) != 0;

    AutoFreeStr findString(Dialog_Find(win->hwndFrame, previousFind, &matchCase));
    if (!findString) {
        return;
    }

    HwndSetText(win->hwndFindEdit, findString);
    Edit_SetModify(win->hwndFindEdit, TRUE);

    bool matchCaseChanged = matchCase != (0 != (state & TBSTATE_CHECKED));
    if (matchCaseChanged) {
        if (matchCase) {
            state |= TBSTATE_CHECKED;
        } else {
            state &= ~TBSTATE_CHECKED;
        }
        SendMessageW(win->hwndToolbar, TB_SETSTATE, CmdFindMatch, state);
        dm->textSearch->SetSensitive(matchCase);
    }

    FindTextOnThread(win, TextSearch::Direction::Forward, true);
}

void FindNext(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (SendMessageW(win->hwndToolbar, TB_ISBUTTONENABLED, CmdFindNext, 0)) {
        FindTextOnThread(win, TextSearch::Direction::Forward, true);
    }
}

void FindPrev(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    if (SendMessageW(win->hwndToolbar, TB_ISBUTTONENABLED, CmdFindPrev, 0)) {
        FindTextOnThread(win, TextSearch::Direction::Backward, true);
    }
}

void FindToggleMatchCase(MainWindow* win) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    WORD state = (WORD)SendMessageW(win->hwndToolbar, TB_GETSTATE, CmdFindMatch, 0);
    win->AsFixed()->textSearch->SetSensitive((state & TBSTATE_CHECKED) != 0);
    Edit_SetModify(win->hwndFindEdit, TRUE);
}

void FindSelection(MainWindow* win, TextSearch::Direction direction) {
    if (!win->IsDocLoaded() || !NeedsFindUI(win)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!win->CurrentTab()->selectionOnPage || 0 == dm->textSelection->result.len) {
        return;
    }

    AutoFreeWStr selection(dm->textSelection->ExtractText(" "));
    str::NormalizeWSInPlace(selection);
    if (str::IsEmpty(selection.Get())) {
        return;
    }

    TempStr s = ToUtf8Temp(selection);
    HwndSetText(win->hwndFindEdit, s);
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

    dm->textSelection->CopySelection(dm->textSearch);
    UpdateTextSelection(win, false);
    dm->ShowResultRectToScreen(result);
    ScheduleRepaint(win, 0);
}

void ClearSearchResult(MainWindow* win) {
    DeleteOldSelectionInfo(win, true);
    ScheduleRepaint(win, 0);
}

struct UpdateFindStatusData {
    MainWindow* win;
    int current;
    int total;
};

static void UpdateFindStatus(UpdateFindStatusData* d) {
    AutoDelete delData(d);

    auto win = d->win;
    if (!IsMainWindowValid(win) || win->findCancelled) {
        return;
    }
    auto wnd = GetNotificationForGroup(win->hwndCanvas, kNotifFindProgress);
    if (!wnd) {
        logf("UpdateFindStatus: no wnd, setting win->findCancelled to true\n");
        win->findCancelled = true;
        return;
    }
    TempStr msg = str::FormatTemp(_TRA("Searching %d of %d..."), d->current, d->total);
    int perc = CalcPerc(d->current, d->total);
    if (!UpdateNotificationProgress(wnd, msg, perc)) {
        // the search has been canceled by closing the notification
        logf("UpdateFindStatus: UpdateNotificationProgress() returned false, setting win->findCancelled to true\n");
        win->findCancelled = true;
    }
}

struct FindThreadData {
    MainWindow* win = nullptr;
    TextSearch::Direction direction = TextSearch::Direction::Forward;
    bool wasModified = false;
    AutoFreeWStr text;
    HANDLE thread = nullptr;

    FindThreadData(MainWindow* win, TextSearch::Direction direction, const char* text, bool wasModified) {
        this->win = win;
        this->direction = direction;
        this->text = ToWStr(text);
        this->wasModified = wasModified;
    }
    ~FindThreadData() {
        CloseHandle(thread);
    }

    void ShowUI(bool showProgress) {
        const LPARAM disable = (LPARAM)MAKELONG(0, 0);

        auto wnd = GetNotificationForGroup(win->hwndCanvas, kNotifFindProgress);

        if (showProgress && wnd == nullptr) {
            NotificationCreateArgs args;
            args.hwndParent = win->hwndCanvas;
            args.timeoutMs = 0;
            args.onRemoved = MkFunc1Void(RemoveNotification);
            args.groupId = kNotifFindProgress;
            ShowNotification(args);
        }

        SendMessageW(win->hwndToolbar, TB_ENABLEBUTTON, CmdFindPrev, disable);
        SendMessageW(win->hwndToolbar, TB_ENABLEBUTTON, CmdFindNext, disable);
        SendMessageW(win->hwndToolbar, TB_ENABLEBUTTON, CmdFindMatch, disable);
    }

    void HideUI(bool success, bool loopedAround) const {
        LPARAM enable = (LPARAM)MAKELONG(1, 0);

        SendMessageW(win->hwndToolbar, TB_ENABLEBUTTON, CmdFindPrev, enable);
        SendMessageW(win->hwndToolbar, TB_ENABLEBUTTON, CmdFindNext, enable);
        SendMessageW(win->hwndToolbar, TB_ENABLEBUTTON, CmdFindMatch, enable);

        auto wnd = GetNotificationForGroup(win->hwndCanvas, kNotifFindProgress);

        if (!wnd) {
            /* our notification has been replaced or closed (or never created) */;
        } else if (!success && !loopedAround) {
            // i.e. canceled
            RemoveNotification(wnd);
        } else if (!success && loopedAround) {
            NotificationUpdateMessage(wnd, _TRA("No matches were found"), 0);
        } else {
            auto pageNo = win->AsFixed()->textSearch->GetSearchHitStartPageNo();
            TempStr label = win->ctrl->GetPageLabeTemp(pageNo);
            TempStr buf = str::FormatTemp(_TRA("Found text at page %s"), label);
            if (loopedAround) {
                buf = str::FormatTemp(_TRA("Found text at page %s (again)"), label);
                MessageBeep(MB_ICONINFORMATION);
            }
            NotificationUpdateMessage(wnd, buf, 0, loopedAround);
        }
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
    } else {
        // nothing found or search canceled
        ClearSearchResult(win);
        ftd->HideUI(false, !wasModifiedCanceled);
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
    defer {
        SafeEngineRelease(&engine);
    };

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
    DestroyTempAllocator();
}

// returns true if did abort a thread or hidden the notification
bool AbortFinding(MainWindow* win, bool hideMessage) {
    bool res = false;
    if (win->findThread) {
        res = true;
        logf("AboftFinding: setting win->findCancelled to true\n");
        win->findCancelled = true;
        WaitForSingleObject(win->findThread, INFINITE);
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
//   if false, searching for the next occurence of previous term
// TODO: should detect wasModified by comparing with the last search result
void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, const char* text, bool wasModified,
                      bool showProgress) {
    AbortFinding(win, false);
    if (str::IsEmpty(text)) {
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
char* ReverseTextTemp(char* s) {
    TempWStr ws = ToWStrTemp(s);
    int n = str::Leni(ws);
    for (int i = 0; i < n / 2; i++) {
        WCHAR c1 = ws[i];
        WCHAR c2 = ws[n - 1 - i];
        ws[i] = c2;
        ws[n - 1 - i] = c1;
    }
    return ToUtf8Temp(ws);
}

void FindTextOnThread(MainWindow* win, TextSearch::Direction direction, bool showProgress) {
    char* s = HwndGetTextTemp(win->hwndFindEdit);
    // if document is rtl, need to reverse the text
    // s = ReverseTextTemp(s);
    bool wasModified = Edit_GetModify(win->hwndFindEdit);
    Edit_SetModify(win->hwndFindEdit, FALSE);
    FindTextOnThread(win, direction, s, wasModified, showProgress);
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
    for (size_t i = 0; i < win->fwdSearchMark.rects.size(); i++) {
        Rect rect = win->fwdSearchMark.rects.at(i);
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

    BYTE alpha = (BYTE)(0x5f * 1.0f * (HIDE_FWDSRCHMARK_STEPS - win->fwdSearchMark.hideStep) / HIDE_FWDSRCHMARK_STEPS);
    ParsedColor* parsedCol = GetPrefsColor(gGlobalPrefs->forwardSearch.highlightColor);
    PaintTransparentRectangles(hdc, win->canvasRc, rects, parsedCol->col, alpha, 0);
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
        const char* path = tab->filePath;
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
        gGlobalPrefs->enableTeXEnhancements = true;
    }

    int pageNo = dm->GetPageNoByPoint(Point(x, y));
    if (!tab->ctrl->ValidPageNo(pageNo)) {
        return false;
    }

    Point pt = ToPoint(dm->CvtFromScreen(Point(x, y), pageNo));
    AutoFreeStr srcfilepath;
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

    if (!file::Exists(srcfilepath)) {
        // if the source file is missing, check if it's been moved to the same place as
        // the PDF document (which happens if all files are moved together)
        TempStr altsrcpath = path::GetDirTemp(tab->filePath);
        altsrcpath = path::JoinTemp(altsrcpath, path::GetBaseNameTemp(srcfilepath));
        if (!str::Eq(altsrcpath, srcfilepath) && file::Exists(altsrcpath)) {
            srcfilepath.SetCopy(altsrcpath);
        }
    }

    char* inverseSearch = gGlobalPrefs->inverseSearchCmdLine;
    if (!inverseSearch) {
        Vec<TextEditor*> editors;
        DetectTextEditors(editors);
        inverseSearch = str::DupTemp(editors[0]->openFileCmd);
    }

    AutoFreeStr cmdLine;
    if (inverseSearch) {
        cmdLine = BuildOpenFileCmd(inverseSearch, srcfilepath, line, col);
    }

    NotificationCreateArgs args;
    args.hwndParent = win->hwndCanvas;
    args.msg = _TRA("Cannot start inverse search command. Please check the command line in the settings.");
    if (!str::IsEmpty(cmdLine.Get())) {
        // resolve relative paths with relation to SumatraPDF.exe's directory
        char* appDir = GetSelfExeDirTemp();
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
void ShowForwardSearchResult(MainWindow* win, const char* fileName, int line, int /* col */, int ret, int page,
                             Vec<Rect>& rects) {
    ReportIf(!win->AsFixed());
    DisplayModel* dm = win->AsFixed();
    win->fwdSearchMark.rects.Reset();
    const PageInfo* pi = dm->GetPageInfo(page);
    if ((ret == PDFSYNCERR_SUCCESS) && (rects.size() > 0) && (nullptr != pi)) {
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
        Rect overallrc = rects.at(0);
        for (size_t i = 1; i < rects.size(); i++) {
            overallrc = overallrc.Union(rects.at(i));
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
        buf = str::FormatTemp(_TRA("Page number %u inexistant"), page);
    } else if (ret == PDFSYNCERR_NO_SYNC_AT_LOCATION) {
        args.msg = _TRA("No synchronization info at this position");
    } else if (ret == PDFSYNCERR_UNKNOWN_SOURCEFILE) {
        buf = str::FormatTemp(_TRA("Unknown source file (%s)"), fileName);
    } else if (ret == PDFSYNCERR_NORECORD_IN_SOURCEFILE) {
        buf = str::FormatTemp(_TRA("Source file %s has no synchronization point"), fileName);
    } else if (ret == PDFSYNCERR_NORECORD_FOR_THATLINE) {
        buf = str::FormatTemp(_TRA("No result found around line %u in file %s"), line, fileName);
    } else if (ret == PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD) {
        buf = str::FormatTemp(_TRA("No result found around line %u in file %s"), line, fileName);
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
static const char* HandleSyncCmd(const char* cmd, bool* ack) {
    AutoFreeStr pdfFile, srcFile;
    BOOL line = 0, col = 0, newWindow = 0, setFocus = 0;
    const char* next = str::Parse(cmd, "[ForwardSearch(\"%s\",%? \"%s\",%u,%u)]", &pdfFile, &srcFile, &line, &col);
    if (!next) {
        next = str::Parse(cmd, "[ForwardSearch(\"%s\",%? \"%s\",%u,%u,%u,%u)]", &pdfFile, &srcFile, &line, &col,
                          &newWindow, &setFocus);
    }
    // allow to omit the pdffile path, so that editors don't have to know about
    // multi-file projects (requires that the PDF has already been opened)
    if (!next) {
        pdfFile.Reset();
        next = str::Parse(cmd, "[ForwardSearch(\"%s\",%u,%u)]", &srcFile, &line, &col);
        if (!next) {
            next = str::Parse(cmd, "[ForwardSearch(\"%s\",%u,%u,%u,%u)]", &srcFile, &line, &col, &newWindow, &setFocus);
        }
    }

    if (!next) {
        return nullptr;
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
static const char* HandleSearchCmd(const char* cmd, bool* ack) {
    AutoFreeStr pdfFile;
    AutoFreeStr term;
    const char* next = str::Parse(cmd, "[Search(\"%s\",\"%s\")]", &pdfFile, &term);
    // TODO: should un-quote text to allow searching text with '"' in them
    if (!next) {
        return nullptr;
    }
    if (str::IsEmpty(term.Get())) {
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
// TODO: handle inCurrentTab flag
static const char* HandleOpenCmd(const char* cmd, bool* ack) {
    AutoFreeStr filePath;
    int newWindow = 0;
    int setFocus = 0;
    int forceRefresh = 0;
    int inCurrentTab = 0;
    const char* next = str::Parse(cmd, "[Open(\"%s\")]", &filePath);
    if (!next) {
        const char* pat = "[Open(\"%s\",%u,%u,%u,%u)]";
        next = str::Parse(cmd, pat, &filePath, &newWindow, &setFocus, &forceRefresh, &inCurrentTab);
    }
    if (!next) {
        const char* pat = "[Open(\"%s\",%u,%u,%u)]";
        next = str::Parse(cmd, pat, &filePath, &newWindow, &setFocus, &forceRefresh);
    }
    if (!next) {
        return nullptr;
    }
    bool isCtrl = IsCtrlPressed();
    logf("HandleOpenCmd: '%s', newWindow: %d, setFocus: %d, forceRefresh: %d, inCurrentTab: %d, isCtrl: %d\n",
         filePath.CStr(), newWindow, setFocus, forceRefresh, inCurrentTab, isCtrl);
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
    auto nWindows = gWindows.Size();
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
            logf("HandleOpenCmd: found existing window with file '%s'\n", filePath.Get());
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
        logf("HandleOpenCmd: calling LoadDocument(), activateExisting: %d\n", (int)args.activateExisting);
        win = LoadDocument(&args);
        if (!win) {
            logf("HandleOpenCmd: LoadDocument() for '%s' failed\n", filePath.Get());
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
static const char* HandleGotoCmd(const char* cmd, bool* ack) {
    AutoFreeStr pdfFile, destName;
    const char* next = str::Parse(cmd, "[GotoNamedDest(\"%s\",%? \"%s\")]", &pdfFile, &destName);
    if (!next) {
        return nullptr;
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
static const char* HandlePageCmd(HWND, const char* cmd, bool* ack) {
    AutoFreeStr pdfFile;
    uint page = 0;
    const char* next = str::Parse(cmd, "[GotoPage(\"%S\",%u)]", &pdfFile, &page);
    if (!next) {
        return nullptr;
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
static const char* HandleSetViewCmd(const char* cmd, bool* ack) {
    AutoFreeStr filePath, viewMode;
    float zoom = kInvalidZoom;
    Point scroll(-1, -1);
    const char* next = str::Parse(cmd, "[SetView(\"%s\",%? \"%s\",%f)]", &filePath, &viewMode, &zoom);
    if (!next) {
        next =
            str::Parse(cmd, "[SetView(\"%s\",%? \"%s\",%f,%d,%d)]", &filePath, &viewMode, &zoom, &scroll.x, &scroll.y);
    }
    if (!next) {
        return nullptr;
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

    if (zoom != kInvalidZoom) {
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
static const char* HandleNewWindowCmd(const char* cmd, bool* ack) {
    if (!str::StartsWith(cmd, "[NewWindow]")) {
        return nullptr;
    }
    logf("HandleNewWindowCmd\n");
    const char* next = cmd + str::Leni("[NewWindow]");
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
zoom: 1.34
view: continuous
sumver: 3.5

i.e. multiple lines, each line is
key: value
This should make parsing easy:
* split by `\n' to get the lines
* split each line by ':' to get key and value

Returns:
error: <error message>
if file doesn't exist or no opened file
*/
static const char* HandleGetFileStateCmd(HWND hwnd, const char* cmd, bool* ack, str::Str& res) {
    AutoFreeStr filePath;
    const char* next = str::Parse(cmd, "[GetFileState(\"%s\")]", &filePath);
    if (!next) {
        next = str::Parse(cmd, "[GetFileState()]");
    }
    if (!next) {
        next = str::Parse(cmd, "[GetFileState]");
    }
    if (!next) {
        return nullptr;
    }

    res.Append("error: hello");
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

    res.Append("error: hello");
    *ack = true;
    return next;
}

/*
Handle all commands as defined in Commands.h
eg: [CmdClose]
*/
static const char* HandleCmdCommand(HWND hwnd, const char* cmd, bool* ack) {
    AutoFreeStr cmdName;
    const char* next = str::Parse(cmd, "[%s]", &cmdName);
    if (!next) {
        return nullptr;
    }
    int cmdId = GetCommandIdByName(cmdName);
    if (cmdId < 0) {
        return nullptr;
    }
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        logfa("HandleCmdCommand: not executing DDE becaues MainWindow for hwnd 0x%p not found\n", hwnd);
        return nullptr;
    }
    logfa("HandleCmdCommand: sending %d (%s) command\n", cmdId, cmdName.Get());
    SendMessageW(win->hwndFrame, WM_COMMAND, cmdId, 0);
    *ack = true;
    return next;
}

// returns true if did handle a message
static bool HandleExecuteCmds(HWND hwnd, const char* cmd) {
    gMostRecentlyOpenedDoc = nullptr;

    bool didHandle = false;
    while (!str::IsEmpty(cmd)) {
        {
            logf("HandleExecuteCmds: '%s'\n", cmd);
        }

        const char* nextCmd = HandleSyncCmd(cmd, &didHandle);
        if (!nextCmd) {
            nextCmd = HandleOpenCmd(cmd, &didHandle);
        }
        if (!nextCmd) {
            nextCmd = HandleGotoCmd(cmd, &didHandle);
        }
        if (!nextCmd) {
            nextCmd = HandlePageCmd(hwnd, cmd, &didHandle);
        }
        if (!nextCmd) {
            nextCmd = HandleSetViewCmd(cmd, &didHandle);
        }
        if (!nextCmd) {
            nextCmd = HandleSearchCmd(cmd, &didHandle);
        }
        if (!nextCmd) {
            nextCmd = HandleCmdCommand(hwnd, cmd, &didHandle);
        }
        if (!nextCmd) {
            nextCmd = HandleNewWindowCmd(cmd, &didHandle);
        }
        if (!nextCmd) {
            // forwards compatibility: ignore unknown commands (maybe from newer version)
            AutoFreeStr tmp;
            nextCmd = str::Parse(cmd, "%s]", &tmp);
        }
        cmd = nextCmd;
    }
    return didHandle;
}

static bool HandleRequestCmds(HWND hwnd, const char* cmd, str::Str& rsp) {
    bool didHandle = false;
    while (!str::IsEmpty(cmd)) {
        {
            logf("HandleRequestCmds: '%s'\n", cmd);
        }

        const char* nextCmd = HandleGetFileStateCmd(hwnd, cmd, &didHandle, rsp);
        if (!nextCmd) {
            AutoFreeStr tmp;
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
            logf("OnDDERequest: invalid fmt '%s'\n", (int)fmt);
            return 0;
    }
    ATOM a = HIWORD(lp);
    TempStr cmd = AtomToStrTemp(a);
    if (!cmd) {
        return 0;
    }

    str::Str str;
    bool didHandle = HandleRequestCmds(hwnd, cmd, str);
    if (!didHandle) {
        str.Set("error: unknoqn command");
    }

    void* data;
    int cbData;
    if (fmt == CF_TEXT) {
        data = (void*)str.Get();
        cbData = str.Size() + 1;
    } else if (fmt == CF_UNICODETEXT) {
        TempWStr tmp = ToWStrTemp(str.Get());
        data = (void*)tmp;
        cbData = (str::Leni(tmp) + 1) * 2;
    } else {
        ReportIf(true);
        return 0;
    }

    int cbDdeData = sizeof(DDEDATA);
    u8* res = (u8*)Allocator::AllocZero(GetTempAllocator(), cbDdeData + cbData);
    DDEDATA* ddeData = (DDEDATA*)res;
    ddeData->fRelease = 1; // tell client to free HGLOBAL
    ddeData->cfFormat = fmt;
    memcpy(res + cbDdeData, data, cbData);

    HGLOBAL h = MemToHGLOBAL(res, cbDdeData + cbData, GMEM_MOVEABLE | GMEM_DDESHARE);
    LPARAM lpres = MAKELPARAM(h, a);
    PostMessageW(hwndClient, WM_DDE_DATA, (WPARAM)hwnd, lpres);
    return 0;
}

LRESULT OnDDExecute(HWND hwnd, WPARAM wp, LPARAM lp) {
    HWND hwndClient = (HWND)wp;
    HGLOBAL hCommand = (HGLOBAL)lp;
    bool isUnicode = IsWindowUnicode(hwndClient);

    TempStr cmd = HGLOBALToStrTemp((HGLOBAL)hCommand, isUnicode);
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

LRESULT OnCopyData(HWND hwnd, WPARAM wp, LPARAM lp) {
    COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lp;
    if (!cds || cds->dwData != 0x44646557 /* DdeW */ || wp) {
        return FALSE;
    }

    const WCHAR* cmdW = (const WCHAR*)cds->lpData;
    if (cmdW[cds->cbData / sizeof(WCHAR) - 1]) {
        return FALSE;
    }

    TempStr cmd = ToUtf8Temp(cmdW);
    bool didHandle = HandleExecuteCmds(hwnd, cmd);
    return didHandle ? TRUE : FALSE;
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
