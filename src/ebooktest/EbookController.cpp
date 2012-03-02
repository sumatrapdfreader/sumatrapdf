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

EbookController::EbookController(EbookControls *ctrls) : ctrls(ctrls)
{
    mobiDoc = NULL;
    html = NULL;
    fileBeingLoaded = NULL;
    pagesFromBeginning = NULL;
    pagesFromPage = NULL;
    pagesShowing = NULL;

    currPageNo = 0;
    layoutThread = NULL;

    pageDx = 0; pageDy = 0;

    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->next, this);
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->prev, this);
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->progress, this);
    ctrls->mainWnd->evtMgr->RegisterSizeChanged(ctrls->page, this);

    UpdateStatus();
}

EbookController::~EbookController()
{
    ctrls->mainWnd->evtMgr->UnRegisterClicked(this);
    ctrls->mainWnd->evtMgr->UnRegisterSizeChanged(this);
    ctrls->page->SetPage(NULL);
    DeletePages(&pagesFromBeginning);
    DeletePages(&pagesFromPage);
    delete mobiDoc;
}

void EbookController::DeletePages(Vec<PageData*>** pages)
{
    CrashIf((pagesFromBeginning != *pages) && 
            (pagesFromPage != *pages));
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

static size_t PageForReparsePoint(Vec<PageData*> *pages, const char *reparsePoint)
{
    for (size_t i = 0; i < pages->Count(); i++) {
        PageData *pd = pages->At(i);
        if (pd->reparsePoint == reparsePoint)
            return i;
        // this is the first page whose content is after reparsePoint, so
        // the page contining reparsePoint must be the one before
        if (pd->reparsePoint > reparsePoint) {
            CrashIf(0 == i);
            return i - 1;
        }
    }
    return -1;
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

    if (!ld->fromBeginning) {
        // TODO: handle the pages
        lf("Abandoning %d pages that are not from beginning", ld->pageCount);
        for (size_t i = 0; i < ld->pageCount; i++) {
            delete ld->pages[i];
        }
        return;
    }

    bool firstPageArrived = (0 == layoutTmp.pagesFromBeginning.Count());
    layoutTmp.pagesFromBeginning.Append(ld->pages, ld->pageCount);
    for (size_t i = 0; i < ld->pageCount; i++) {
        CrashIf(!ld->pages[i]->reparsePoint);
    }
    lf("EbookController::HandleMobiLayoutMsg() got %d pages, fromBeginning = %d", ld->pageCount, (int)ld->fromBeginning);

    if (!layoutTmp.startPageReparsePoint) {
        // if we're starting from the beginning, show the first page as
        // quickly as we can
        if (firstPageArrived) {
            ctrls->page->SetPage(layoutTmp.pagesFromBeginning.At(0));
            currPageNo = 1;
        }
    } else {
        // if we're starting from some page, see if the reparse point is within
        // pages that arrived
        // TODO: we do it every time we get new pages even if we've found a page before
        size_t pageNo = PageForReparsePoint(&layoutTmp.pagesFromBeginning, layoutTmp.startPageReparsePoint);
        if (-1 != pageNo) {
            PageData *pd = layoutTmp.pagesFromBeginning.At(pageNo);
            ctrls->page->SetPage(pd);
            currPageNo = pageNo + 1;
        }
    }

    if (ld->finished) {
        layoutThread = NULL;
        DeletePages(&pagesFromBeginning);
        pagesFromBeginning = new Vec<PageData*>();
        PageData **pages = layoutTmp.pagesFromBeginning.LendData();
        size_t pageCount =  layoutTmp.pagesFromBeginning.Count();
        pagesFromBeginning->Append(pages, pageCount);
        layoutTmp.pagesFromBeginning.Reset();
        layoutTmp.pagesFromPage.Reset();
        pagesShowing = pagesFromBeginning;
    }
    UpdateStatus();
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

    pagesShowing = NULL;

    // nicely ask existing layout thread (if exists) to quit but we don't
    // rely on that. If it sends us some data anyway, we'll ignore it
    if (layoutThread)
        layoutThread->RequestCancel();

    CrashIf(layoutTmp.pagesFromBeginning.Count() > 0);  // those should be reset in HandleMobiLayoutMsg()
    CrashIf(layoutTmp.pagesFromPage.Count() > 0);

    layoutThread = new ThreadLayoutMobi();
    layoutThread->controller = this;
    layoutThread->reparsePoint = layoutTmp.startPageReparsePoint;
    layoutThread->layoutInfo = GetLayoutInfo(html, mobiDoc, dx, dy, &textAllocator);
    // additional sanity check for reparsePoint: if it's the same as start of html,
    // mark it as NULL for the benefit of ThreadLayoutMobi
    if (layoutThread->reparsePoint == layoutThread->layoutInfo->htmlStr)
        layoutThread->reparsePoint = NULL;
    layoutThread->Start();
    UpdateStatus();
}

void EbookController::OnLayoutTimer()
{
    layoutTmp.startPageReparsePoint = NULL;
    if (currPageNo != 1) {
        PageData *pd = ctrls->page->GetPage();
        if (pd)
            layoutTmp.startPageReparsePoint = pd->reparsePoint;
    }
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

    if (c == ctrls->progress) {
        float perc = ctrls->progress->GetPercAt(x);
        if (pagesShowing) {
            int pageCount = pagesShowing->Count();
            int pageNo = IntFromPerc(pageCount, perc);
            GoToPage(pageNo + 1);
        }
        return;
    }

    CrashAlwaysIf(true);
}

// show the status text based on current state
void EbookController::UpdateStatus() const
{
    ctrls->progress->SetFilled(0.f);

    if (fileBeingLoaded) {
        ScopedMem<TCHAR> s(str::Format(_T("Loading %s..."), fileBeingLoaded));
        ctrls->status->SetText(AsWStrQ(s.Get()));
        return;
    }

    if (LayoutInProgress()) {
        int pageCount = layoutTmp.pagesFromBeginning.Count();
        ScopedMem<TCHAR> s(str::Format(_T("Formatting the book... %d pages"), pageCount));
        ctrls->status->SetText(AsWStrQ(s.Get()));
        return;
    }

    if (!pagesShowing) {
        ctrls->status->SetText(L" ");
        return;
    }

    size_t pageCount = pagesShowing->Count();
    ScopedMem<TCHAR> s(str::Format(_T("Page %d out of %d"), currPageNo, (int)pageCount));
    ctrls->status->SetText(AsWStrQ(s.Get()));
    ctrls->progress->SetFilled(PercFromInt(pageCount, currPageNo));
}

void EbookController::GoToPage(int newPageNo)
{
    if (LayoutInProgress() || !pagesShowing)
        return;

    CrashIf((newPageNo < 1) || (newPageNo > (int)pagesShowing->Count()));
    CrashIf(pagesShowing != pagesFromBeginning);
    currPageNo = newPageNo;
    PageData *pageData = pagesShowing->At(currPageNo-1);
    ctrls->page->SetPage(pageData);
    UpdateStatus();
}

void EbookController::GoToLastPage()
{
    if (LayoutInProgress())
        return;

    GoToPage(pagesShowing->Count());
}

// Going back is a special case in that we might need to switch which
// pages we're showing
void EbookController::GoOnePageBack()
{
    if (pagesShowing == pagesFromBeginning) {
        if (currPageNo > 1)
            GoToPage(currPageNo - 1);
        return;
    }

    CrashIf(!((pagesShowing == pagesFromPage) || (pagesShowing == NULL)));

    // The complicated case: we can go back if we can map
    // currently shown page (formatted from arbitrary point)
    // to a page as formatted from beginning. This is not
    // always possible (this page might not be created yet)
    PageData *page = ctrls->page->GetPage();
    CrashIf(!page);
    const char *currPageReparsePoint = page->reparsePoint;
    size_t newPage;
    if (pagesFromBeginning) {
        newPage = PageForReparsePoint(pagesFromBeginning, currPageReparsePoint);
        lf("EbookController::GoOnePageBack(): mapped curr page to page %d in pagesFromBeginning", newPage);
    } else {
        CrashIf(!LayoutInProgress());
        newPage = PageForReparsePoint(&layoutTmp.pagesFromBeginning, currPageReparsePoint);
        lf("EbookController::GoOnePageBack(): mapped curr page to page %d in layoutTmp.pagesFromBeginning", newPage);
    }
    if (-1 == newPage)
        return;
    // TODO: switch from showing pagesFromPage to showing pagesFromBeginning
    return;
}

void EbookController::AdvancePage(int dist)
{
    // if we're not showing any 
    if (!pagesShowing && !LayoutInProgress())
        return;

    if (-1 == dist) {
        GoOnePageBack();
        return;
    }

    // TODO: relax this, we can somethimes navigate pages even during
    // layout
    if (LayoutInProgress())
        return;

    int newPageNo = currPageNo + dist;
    if (newPageNo < 1)
        return;
    // TODO: this is more complicated
    if (newPageNo > (int)pagesShowing->Count())
        return;
    GoToPage(newPageNo);
}

void EbookController::SetHtml(const char *newHtml)
{
    html = newHtml;
    delete mobiDoc;
    layoutTmp.startPageReparsePoint = NULL;
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

    layoutTmp.startPageReparsePoint = NULL; // mark as being laid out from the beginning
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
