/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "EbookController.h"

#include "EbookControls.h"
#include "FileUtil.h"
#include "MobiDoc.h"
#include "PageLayout.h"
#include "ThreadUtil.h"
#include "Timer.h"
#include "UiMsgEbook.h"
#include "UiMsg.h"

#include "DebugLog.h"

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12.5

// in EbookTest.cpp
void RestartLayoutTimer();

void LayoutTemp::DeletePages()
{
    DeleteVecMembers<PageData*>(pagesFromBeginning);
    DeleteVecMembers<PageData*>(pagesFromPage);
}

class ThreadLoadMobi : public ThreadBase {
public:
    TCHAR *             fileName; // we own this memory
    EbookController *   controller;

    ThreadLoadMobi(const TCHAR *fileName);
    virtual ~ThreadLoadMobi() { free(fileName); }

    // ThreadBase
    virtual void Run();
};

ThreadLoadMobi::ThreadLoadMobi(const TCHAR *fn)
{
    autoDeleteSelf = true;
    fileName = str::Dup(fn);
}

void ThreadLoadMobi::Run()
{
    lf(_T("ThreadLoadMobi::Run(%s)"), fileName);
    Timer t(true);
    MobiDoc *mobiDoc = MobiDoc::ParseFile(fileName);
    double loadingTimeMs = t.GetTimeInMs();
    lf(_T("Loaded %s in %.2f ms"), fileName, t.GetTimeInMs());

    UiMsg *msg = new UiMsg(UiMsg::FinishedMobiLoading);
    msg->finishedMobiLoading.mobiDoc = mobiDoc;
    msg->finishedMobiLoading.fileName = fileName;
    fileName = NULL;
    uimsg::Post(msg);
}

class ThreadLayoutMobi : public ThreadBase {
public:
    // provided by the caller
    LayoutInfo *        layoutInfo; // we own it
    PoolAllocator *     textAllocator;
    EbookController *   controller;
    // if set, we first send pages for text parsed from
    // this point and then we sent pages as parsed from
    // the beginning
    const char *        reparsePoint;

    // state used during layout process
    PageData *  pages[MobiLayoutData::MAX_PAGES];
    int         pageCount;

    void        SendPagesIfNecessary(bool force, bool finished, bool fromBeginning);
    bool        Layout(const char *reparsePoint);

    ThreadLayoutMobi();
    virtual ~ThreadLayoutMobi();

    // ThreadBase
    virtual void Run();
};

ThreadLayoutMobi::ThreadLayoutMobi() : layoutInfo(NULL), pageCount(0)
{
    autoDeleteSelf = true;
}

ThreadLayoutMobi::~ThreadLayoutMobi()
{
    free((void*)layoutInfo->fontName);
    delete layoutInfo;
}

static void DeletePages(Vec<PageData*> *pages)
{
    if (!pages)
        return;
    DeleteVecMembers<PageData*>(*pages);
    delete pages;
}

// send accumulated pages if we filled the buffer or the caller forces us
void ThreadLayoutMobi::SendPagesIfNecessary(bool force, bool finished, bool fromBeginning)
{
    if (finished)
        force = true;
    if (!force && (pageCount < dimof(pages)))
        return;
    UiMsg *msg = new UiMsg(UiMsg::MobiLayout);
    MobiLayoutData *ld = &msg->mobiLayout;
    ld->finished = finished;
    ld->fromBeginning = fromBeginning;
    if (pageCount > 0)
        memcpy(ld->pages, pages, pageCount * sizeof(PageData*));
    ld->pageCount = pageCount;
    ld->thread = this;
    uimsg::Post(msg);
    pageCount = 0;
}

// layout pages from a given reparse point (beginning if NULL)
// returns true if layout thread was cancelled
bool ThreadLayoutMobi::Layout(const char *reparsePoint)
{
    bool fromBeginning = (reparsePoint == NULL);
    lf("Started laying out mobi, fromBeginning=%d", (int)fromBeginning);
    int totalPageCount = 0;
    Timer t(true);
    PageLayout pl;
    layoutInfo->reparsePoint = reparsePoint;
    for (PageData *pd = pl.IterStart(layoutInfo); pd; pd = pl.IterNext()) {
        if (WasCancelRequested()) {
            lf("Layout cancelled");
            for (int i = 0; i < pageCount; i++) {
                delete pages[i];
            }
            return true;
        }
        pages[pageCount++] = pd;
        ++totalPageCount;
        // we send first 5 pages one by one and the rest in batches to minimize user-visible
        // latency but also not overload ui thread
        SendPagesIfNecessary(totalPageCount < 5, false, fromBeginning);
        CrashIf(pageCount >= dimof(pages));
    }
    // this is the last message only if we're laying out from the beginning
    bool finished = fromBeginning;
    SendPagesIfNecessary(true, finished, fromBeginning);
    lf("Laying out took %.2f ms", t.GetTimeInMs());
    return false;
}

void ThreadLayoutMobi::Run()
{
    // if we have reparsePoint, layout from that point and then
    // layout from the beginning. Otherwise just from beginning
    bool cancelled = Layout(reparsePoint);
    if (!cancelled && (NULL != reparsePoint))
        Layout(NULL);
}

EbookController::EbookController(EbookControls *ctrls) :
    ctrls(ctrls), mobiDoc(NULL), html(NULL),
    fileBeingLoaded(NULL), pagesFromBeginning(NULL), pagesFromPage(NULL),
    currPageNo(0), pageShown(NULL), deletePageShown(false),
    pageDx(0), pageDy(0), layoutThread(NULL)
{
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->next, this);
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->prev, this);
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->progress, this);
    ctrls->mainWnd->evtMgr->RegisterSizeChanged(ctrls->page, this);

    UpdateStatus();
}

EbookController::~EbookController()
{
    if (layoutThread) {
        layoutThread->RequestCancel();
        // TODO: wait until finishes, within limits
    }
    ctrls->mainWnd->evtMgr->UnRegisterClicked(this);
    ctrls->mainWnd->evtMgr->UnRegisterSizeChanged(this);
    ctrls->page->SetPage(NULL);
    DeletePageShown();
    DeletePages(&pagesFromBeginning);
    DeletePages(&pagesFromPage);
    delete mobiDoc;
}

void EbookController::DeletePageShown()
{
    if (deletePageShown)
        delete pageShown;
    pageShown = NULL;
}

void EbookController::DeletePages(Vec<PageData*>** pages)
{
    CrashIf((pagesFromBeginning != *pages) && 
            (pagesFromPage != *pages));
    if (pagesFromPage && (*pages == pagesFromPage))
        lf("Deleting pages from page");
    ::DeletePages(*pages);
    *pages = NULL;
}

static LayoutInfo *GetLayoutInfo(const char *html, MobiDoc *mobiDoc, int dx, int dy, PoolAllocator *textAllocator)
{
    LayoutInfo *li = new LayoutInfo();
    size_t len;
    if (html) {
        CrashIf(mobiDoc);
        len = strlen(html);
    } else {
        html = mobiDoc->GetBookHtmlData(len);
    }
    li->fontName = str::Dup(FONT_NAME);
    li->fontSize = FONT_SIZE;
    li->htmlStr = html;
    li->htmlStrLen = len;
    li->pageDx = dx;
    li->pageDy = dy;
    li->mobiDoc = mobiDoc;
    li->textAllocator = textAllocator;
    return li;
}

// returns page whose content contains reparsePoint
// page is in 1..$pageCount range to match currPageNo
// returns 0 if not found
static size_t PageForReparsePoint(Vec<PageData*> *pages, const char *reparsePoint)
{
    if (!pages)
        return 0;
    for (size_t i = 0; i < pages->Count(); i++) {
        PageData *pd = pages->At(i);
        if (pd->reparsePoint == reparsePoint)
            return i + 1;
        // this is the first page whose content is after reparsePoint, so
        // the page contining reparsePoint must be the one before
        if (pd->reparsePoint > reparsePoint) {
            CrashIf(0 == i);
            return i;
        }
    }
    return 0;
}

Vec<PageData*> *EbookController::GetPagesFromBeginning()
{
    if (pagesFromBeginning) {
        CrashIf(LayoutInProgress());
        return pagesFromBeginning;
    }
    if (LayoutInProgress() && (0 != layoutTemp.pagesFromBeginning.Count()))
        return &layoutTemp.pagesFromBeginning;
    return NULL;
}

// Given a page try to calculate its page number
void EbookController::UpdateCurrPageNoForPage(PageData *pd)
{
    currPageNo = 0;
    if (!pd)
        return;
    // pages can have have the same reparse point, so finding them by
    // reparse point is not reliable. Try 
    if (pagesFromBeginning) {
        int n = pagesFromBeginning->Find(pd) + 1;
        if (n > 0) {
            currPageNo = n;
            return;
        }
    }
    currPageNo = PageForReparsePoint(GetPagesFromBeginning(), pd->reparsePoint);
}

void EbookController::ShowPage(PageData *pd, bool deleteWhenDone)
{
    DeletePageShown();
    pageShown = pd;
    deletePageShown = deleteWhenDone;
    ctrls->page->SetPage(pageShown);

    UpdateCurrPageNoForPage(pageShown);
    if (pd) {
        char s[64] = { 0 };
        memcpy(s, pd->reparsePoint, dimof(s) - 1);
        lf("ShowPage() %d '%s'", currPageNo, s);
    }
}

void EbookController::HandleMobiLayoutMsg(UiMsg *msg)
{
    CrashIf(UiMsg::MobiLayout != msg->type);
    if (layoutThread != msg->mobiLayout.thread) {
        // this is a message from cancelled thread, we can disregard
        lf("EbookController::MobiLayout() thread message discarded");
        return;
    }

    MobiLayoutData *ld = &msg->mobiLayout;
    PageData *pageToShow = NULL;

    if (!ld->fromBeginning) {
        // if this is the first page sent, we're currently showing a page
        // formatted for old window size. Replace that page with new page
        if (0 == layoutTemp.pagesFromPage.Count()) {
            CrashIf(0 == ld->pageCount);
            pageToShow = ld->pages[0];
        }
        layoutTemp.pagesFromPage.Append(ld->pages, ld->pageCount);
        if (pageToShow) {
            CrashIf(pageToShow->reparsePoint != pageShown->reparsePoint);
            ShowPage(pageToShow, false);
        }
        //lf("Got %d pages from page, total %d", ld->pageCount, layoutTemp.pagesFromPage.Count());
        UpdateStatus();
        return;
    }

    if (0 == layoutTemp.pagesFromBeginning.Count()) {
        CrashIf(0 == ld->pageCount);
        pageToShow = ld->pages[0];
    }

    layoutTemp.pagesFromBeginning.Append(ld->pages, ld->pageCount);
    for (size_t i = 0; i < ld->pageCount; i++) {
        CrashIf(!ld->pages[i]->reparsePoint);
    }
    //lf("Got %d pages from beginning, total %d", ld->pageCount, layoutTemp.pagesFromBeginning.Count());

    if (NULL == layoutTemp.reparsePoint) {
        // if we're starting from the beginning, show the first page as
        // quickly as we can
        if (pageToShow)
            ShowPage(pageToShow, false);
    }

    if (ld->finished) {
        CrashIf(pagesFromBeginning || pagesFromPage);
        layoutThread = NULL;
        pagesFromBeginning = new Vec<PageData*>();
        PageData **pages = layoutTemp.pagesFromBeginning.LendData();
        size_t pageCount =  layoutTemp.pagesFromBeginning.Count();
        pagesFromBeginning->Append(pages, pageCount);
        layoutTemp.pagesFromBeginning.Reset();

        pageCount =  layoutTemp.pagesFromPage.Count();
        if (pageCount > 0) { // those are optional
            pages = layoutTemp.pagesFromPage.LendData();
            pagesFromPage = new Vec<PageData*>();
            pagesFromPage->Append(pages, pageCount);
            layoutTemp.pagesFromPage.Reset();
        }
    }
    UpdateStatus();
}

// if we're showing a temp page, it must be part of some collection that we're
// about to delete. Remove the page from its collection so that it doesn't get
// deleted along with it. The caller must assume ownership of the object
PageData *EbookController::PreserveTempPageShown()
{
    if (!pageShown)
        return NULL;
    if (deletePageShown) {
        CrashIf(true); // not sure if this should ever happen
        deletePageShown = false;
        return pageShown;
    }
    if (LayoutInProgress()) {
        CrashIf(pagesFromBeginning || pagesFromPage);
        if (layoutTemp.pagesFromBeginning.Remove(pageShown))
            return pageShown;
        if (layoutTemp.pagesFromPage.Remove(pageShown))
            return pageShown;
        CrashIf(true); // where did the page come from?
        return NULL;
    }
    if (pagesFromBeginning && pagesFromBeginning->Remove(pageShown))
        return pageShown;
    if (pagesFromPage && pagesFromPage->Remove(pageShown))
        return pageShown;
    CrashIf(true); // where did the page come from?
    return NULL;
}

void EbookController::TriggerLayout()
{
    Size s = ctrls->page->GetDrawableSize();
    int dx = s.Width; int dy = s.Height;
    if ((0 == dx) || (0 == dy)) {
        // we haven't been sized yet
        return;
    }
    lf("EbookController::TriggerLayout (%d, %d)", dx, dy);
    CrashIf((dx < 100) || (dy < 40));
    if (!html && !mobiDoc)
        return;

    PageData *newPage = PreserveTempPageShown();
    if (newPage)
        CrashIf((layoutTemp.reparsePoint != NULL) && (layoutTemp.reparsePoint != newPage->reparsePoint));
    else
        CrashIf(layoutTemp.reparsePoint != NULL);

    // nicely ask existing layout thread (if exists) to quit but we don't
    // rely on that. If it sends us some data anyway, we'll ignore it
    if (layoutThread) {
        layoutThread->RequestCancel();
        layoutTemp.DeletePages();
    }

    DeletePages(&pagesFromBeginning);
    DeletePages(&pagesFromPage);

    CrashIf(layoutTemp.pagesFromBeginning.Count() > 0);
    CrashIf(layoutTemp.pagesFromPage.Count() > 0);

    ShowPage(newPage, newPage != NULL);
    layoutThread = new ThreadLayoutMobi();
    layoutThread->controller = this;
    layoutThread->layoutInfo = GetLayoutInfo(html, mobiDoc, dx, dy, &textAllocator);
    // additional sanity check for reparsePoint: if it's the same as start of html,
    // mark it as NULL for the benefit of ThreadLayoutMobi
    if (layoutTemp.reparsePoint == layoutThread->layoutInfo->htmlStr)
        layoutTemp.reparsePoint = NULL;
    layoutThread->reparsePoint = layoutTemp.reparsePoint;
    layoutThread->Start();
    UpdateStatus();
}

void EbookController::OnLayoutTimer()
{
    layoutTemp.reparsePoint = NULL;
    if (pageShown)
        layoutTemp.reparsePoint = pageShown->reparsePoint;
    TriggerLayout();
}

void EbookController::SizeChanged(Control *c, int dx, int dy)
{
    CrashIf(c != ctrls->page);
    // delay re-layout so that we don't unnecessarily do the
    // work as long as the user is still resizing the window
    RestartLayoutTimer();
}

// (x, y) is in the coordinates of w
void EbookController::Clicked(Control *c, int x, int y)
{
    if (c == ctrls->next) {
        AdvancePage(1);
        return;
    }

    if (c == ctrls->prev) {
        AdvancePage(-1);
        return;
    }

    CrashIf(c != ctrls->progress);

    float perc = ctrls->progress->GetPercAt(x);
    CrashIf(!GetPagesFromBeginning()); // shouldn't be active if we don't have those
    int pageCount = GetPagesFromBeginning()->Count();
    int newPageNo = IntFromPerc(pageCount, perc) + 1;
    GoToPage(newPageNo);
}

size_t EbookController::GetMaxPageCount()
{
    Vec<PageData *> *pages1 = pagesFromBeginning;
    Vec<PageData *> *pages2 = pagesFromPage;
    if (LayoutInProgress()) {
        CrashIf(pages1 || pages2);
        pages1 = &layoutTemp.pagesFromBeginning;
        pages2 = &layoutTemp.pagesFromPage;
    }
    size_t n = 0;
    if (pages1 && pages1->Count() > n)
        n = pages1->Count();
    if (pages2 && pages2->Count() > n)
        n = pages2->Count();
    return n;
}

// show the status text based on current state
void EbookController::UpdateStatus()
{
    UpdateCurrPageNoForPage(pageShown);
    ctrls->progress->SetFilled(0.f);
    size_t pageCount = GetMaxPageCount();

    if (fileBeingLoaded) {
        ScopedMem<WCHAR> s(str::Format(L"Loading %s...", fileBeingLoaded));
        ctrls->status->SetText(s.Get());
        return;
    }

    if (LayoutInProgress()) {
        ScopedMem<WCHAR> s(str::Format(L"Formatting the book... %d pages", pageCount));
        ctrls->status->SetText(s.Get());
        return;
    }

    WCHAR *s = NULL;
    if (0 == currPageNo)
        s = str::Format(L"%d pages", pageCount);
    else
        s = str::Format(L"Page %d out of %d", currPageNo, pageCount);

    ctrls->status->SetText(s);
    free(s);
    if (GetPagesFromBeginning())
        ctrls->progress->SetFilled(PercFromInt(GetPagesFromBeginning()->Count(), currPageNo));
}

void EbookController::GoToPage(int newPageNo)
{
    if (currPageNo == newPageNo)
        return;
    CrashIf(!GetPagesFromBeginning());
    if ((newPageNo < 1) || ((size_t)newPageNo > GetPagesFromBeginning()->Count()))
        return;
    ShowPage(GetPagesFromBeginning()->At(newPageNo - 1), false);
    // even if we were showing a page from pagesFromPage before, we've
    // transitioned to using pagesFromBeginning so we no longer need pagesFromPage
    DeletePages(&pagesFromPage);
    UpdateStatus();
}

void EbookController::GoToLastPage()
{
    if (GetPagesFromBeginning())
        GoToPage(GetPagesFromBeginning()->Count());
}

bool EbookController::GoOnePageForward(Vec<PageData*> *pages)
{
    if (!pageShown || !pages)
        return false;
    int newPageIdx = pages->Find(pageShown) + 1;
    if (0 == newPageIdx)
        return false;
    if ((size_t)newPageIdx >= pages->Count())
        return false;
    ShowPage(pages->At(newPageIdx), false);
    return true;
}

// Going forward is a special case in that's the only case where we don't
// need pagesFromBeginning. If we're currently showing a page from pagesFromPage
// we'll show the next page
void EbookController::GoOnePageForward()
{
    if (GoOnePageForward(pagesFromPage))
        return;
    if (GoOnePageForward(&layoutTemp.pagesFromPage))
        return;
    if (!GetPagesFromBeginning() || (0 == currPageNo))
        return;
    GoToPage(currPageNo + 1);
}

void EbookController::AdvancePage(int dist)
{
    if (1 == dist) {
        // a special case for moving forward
        GoOnePageForward();
        return;
    }

    // if we don't know at which page we're on, we can't advance relative to it
    if (0 == currPageNo)
        return;

    GoToPage(currPageNo + dist);
}

void EbookController::SetHtml(const char *newHtml)
{
    html = newHtml;
    delete mobiDoc;
    layoutTemp.reparsePoint = NULL;
    TriggerLayout();
}

void EbookController::HandleFinishedMobiLoadingMsg(UiMsg *msg)
{
    CrashIf(UiMsg::FinishedMobiLoading != msg->type);
    delete mobiDoc;
    html = NULL;
    str::ReplacePtr(&fileBeingLoaded, NULL);
    if (NULL == msg->finishedMobiLoading.mobiDoc) {
        lf("ControlEbook::FinishedMobiLoading(): failed to load");
        // TODO: a better way to notify about this, should be a transient message
        ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), msg->finishedMobiLoading.fileName));
        ctrls->status->SetText(AsWStrQ(s.Get()));
        return;
    }
    mobiDoc = msg->finishedMobiLoading.mobiDoc;
    msg->finishedMobiLoading.mobiDoc = NULL; // just in case, it shouldn't be freed anyway

    layoutTemp.reparsePoint = NULL; // mark as being laid out from the beginning
    TriggerLayout();
}

void EbookController::LoadMobi(const TCHAR *fileName)
{
    // note: ThreadLoadMobi object will get automatically deleted, so no
    // need to keep it around
    ThreadLoadMobi *loadThread = new ThreadLoadMobi(fileName);
    loadThread->controller = this;
    loadThread->Start();

    fileBeingLoaded = str::Dup(path::GetBaseName(fileName));
    UpdateStatus();
}
