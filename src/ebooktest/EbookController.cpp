/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "EbookController.h"

#include "EbookControls.h"
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

    // state used during layout process
    PageData *  pages[MobiLayoutData::MAX_PAGES];
    int         pageCount;

    void        SendPagesIfNecessary(bool force, bool finished);

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
void ThreadLayoutMobi::SendPagesIfNecessary(bool force, bool finished)
{
    if (finished)
        force = true;
    if (!force && (pageCount < dimof(pages)))
        return;
    UiMsg *msg = new UiMsg(UiMsg::MobiLayout);
    MobiLayoutData *ld = &msg->mobiLayout;
    ld->finished = finished;
    ld->fromBeginning = true; // TODO: will not always be true
    if (pageCount > 0)
        memcpy(ld->pages, pages, pageCount * sizeof(PageData*));
    ld->pageCount = pageCount;
    ld->thread = this;
    uimsg::Post(msg);
    pageCount = 0;
}

void ThreadLayoutMobi::Run()
{
    l("Started laying out mobi");
    int totalPageCount = 0;
    Timer t(true);
    PageLayout pl;
    for (PageData *pd = pl.IterStart(layoutInfo); pd; pd = pl.IterNext()) {
        if (WasCancelRequested()) {
            l("Layout cancelled");
            for (int i = 0; i < pageCount; i++) {
                delete pages[i];
            }
            return;
        }
        pages[pageCount++] = pd;
        ++totalPageCount;
        // we send first 5 pages one by one and the rest in batches to minimize user-visible
        // latency but also not overload ui thread
        SendPagesIfNecessary(totalPageCount < 5, false);
        CrashIf(pageCount >= dimof(pages));
    }
    SendPagesIfNecessary(true, true);
    lf("Laying out took %.2f ms", t.GetTimeInMs());
}

EbookController::EbookController(EbookControls *ctrls) : ctrls(ctrls)
{
    mobiDoc = NULL;
    html = NULL;
    pagesFromBeginning = NULL;
    newPagesFromBeginning = NULL;
    pagesFromPage = NULL;
    pagesShowing = NULL;

    shownPageNo = 0;
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
    DeletePages(&newPagesFromBeginning);
    DeletePages(&pagesFromPage);
    delete mobiDoc;
}

void EbookController::DeletePages(Vec<PageData*>** pages)
{
    CrashIf((pagesFromBeginning != *pages) && 
            (pagesFromPage != *pages) &&
            (newPagesFromBeginning != *pages));
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

void EbookController::HandleMobiLayoutMsg(UiMsg *msg)
{
    CrashIf(UiMsg::MobiLayout != msg->type);
    if (layoutThread != msg->mobiLayout.thread) {
        // this is a message from cancelled thread, we can disregard
        l("EbookController::MobiLayout() thread message discarded");
        return;
    }
    MobiLayoutData *ld = &msg->mobiLayout;
    l("EbookController::HandleMobiLayoutMsg() got new pages");
    if (!newPagesFromBeginning)
        newPagesFromBeginning = new Vec<PageData*>();
    newPagesFromBeginning->Append(ld->pages, ld->pageCount);

    if (ld->finished) {
        layoutThread = NULL;
        DeletePages(&pagesFromBeginning);
        pagesFromBeginning = newPagesFromBeginning;
        pagesShowing = pagesFromBeginning;
        newPagesFromBeginning = NULL;
        // TODO: should set the page to a page we were on the last time
        GoToPage(1);
    }
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

    DeletePages(&newPagesFromBeginning);
    pagesShowing = NULL;

    // nicely ask existing layout thread (if exists) to quit but we don't
    // rely on that. If it sends us some data anyway, we'll ignore it
    if (layoutThread)
        layoutThread->RequestCancel();

    layoutThread = new ThreadLayoutMobi();
    layoutThread->controller = this;
    layoutThread->layoutInfo = GetLayoutInfo(html, mobiDoc, dx, dy, &textAllocator);
    layoutThread->Start();
    UpdateStatus();
}

void EbookController::OnLayoutTimer()
{
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
    if (!pagesShowing) {
        ctrls->status->SetText(_T(" "));
        ctrls->progress->SetFilled(0.f);
        return;
    }

    if (LayoutInProgress()) {
        ctrls->status->SetText(_T("Formatting the book..."));
        ctrls->progress->SetFilled(0.f);
        return;
    }

    size_t pageCount = pagesShowing->Count();
    ScopedMem<TCHAR> s(str::Format(_T("Page %d out of %d"), currPageNoFromBeginning, (int)pageCount));
    ctrls->status->SetText(s.Get());
    ctrls->progress->SetFilled(PercFromInt(pageCount, currPageNoFromBeginning));
}

void EbookController::GoToPage(int newPageNo)
{
    if (LayoutInProgress() || !pagesShowing)
        return;

    CrashIf((newPageNo < 1) || (newPageNo > (int)pagesShowing->Count()));
    CrashIf(pagesShowing != pagesFromBeginning);
    currPageNoFromBeginning = newPageNo;
    UpdateStatus();
    PageData *pageData = pagesShowing->At(currPageNoFromBeginning-1);
    ctrls->page->SetPage(pageData);
}

void EbookController::GoToLastPage()
{
    if (LayoutInProgress())
        return;

    GoToPage(pagesShowing->Count());
}

void EbookController::AdvancePage(int dist)
{
    if (LayoutInProgress() || !pagesShowing)
        return;
    int newPageNo = currPageNoFromBeginning + dist;
    if (newPageNo < 1)
        return;
    if (newPageNo > (int)pagesShowing->Count())
        return;
    GoToPage(newPageNo);
}

void EbookController::SetHtml(const char *newHtml)
{
    html = newHtml;
    delete mobiDoc;
    TriggerLayout();
}

void EbookController::HandleFinishedMobiLoadingMsg(UiMsg *msg)
{
    CrashIf(UiMsg::FinishedMobiLoading != msg->type);
    delete mobiDoc;
    html = NULL;
    if (NULL == msg->finishedMobiLoading.mobiDoc) {
        l("ControlEbook::FinishedMobiLoading(): failed to load");
        // TODO: a better way to notify about this, should be a transient message
        ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), msg->finishedMobiLoading.fileName));
        ctrls->status->SetText(s.Get());
        return;
    }

    mobiDoc = msg->finishedMobiLoading.mobiDoc;
    msg->finishedMobiLoading.mobiDoc = NULL;
    TriggerLayout();
}

void EbookController::LoadMobi(const TCHAR *fileName)
{
    // note: ThreadLoadMobi object will get automatically deleted, so no
    // need to keep it around
    ThreadLoadMobi *loadThread = new ThreadLoadMobi(fileName);
    loadThread->controller = this;
    loadThread->Start();

    // TODO: better way to show this message
    ScopedMem<TCHAR> s(str::Format(_T("Loading %s..."), fileName));
    ctrls->status->SetText(s.Get());
}
