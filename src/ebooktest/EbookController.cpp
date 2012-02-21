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
#define FONT_SIZE              13

// in EbookTest.cpp
void RestartLayoutTimer();

// TODO: embed EbookController object to notify when finished, we use the current one now.
class ThreadLoadMobi : public ThreadBase {
public:
    TCHAR *     fileName; // we own this memory

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
    TCHAR *             fontName;  // we own it
    int                 pageDx, pageDy;
    float               fontSize;
    PoolAllocator *     textAllocator;
    MobiDoc *           mobiDoc;
    EbookController *   controller;

    ThreadLayoutMobi();
    virtual ~ThreadLayoutMobi();

    // ThreadBase
    virtual void Run();
};

ThreadLayoutMobi::ThreadLayoutMobi() : 
    pageDx(0), pageDy(0), fontName(NULL), fontSize(0), mobiDoc(NULL)
{ }

ThreadLayoutMobi::~ThreadLayoutMobi()
{
    free(fontName);
}

static void DeletePages(Vec<PageData*> *pages)
{
    if (!pages)
        return;
    DeleteVecMembers<PageData*>(*pages);
    delete pages;
}

void ThreadLayoutMobi::Run()
{
    PageData *pd;

    LayoutInfo li;
    li.fontName = fontName;
    li.fontSize = fontSize;
    li.pageDx = pageDx;
    li.pageDy = pageDy;
    li.textAllocator = textAllocator;
    li.htmlStr = mobiDoc->GetBookHtmlData(li.htmlStrLen);

    l("Started laying out mob");
    Timer t(true);
    Vec<PageData*> *pages = new Vec<PageData*>();
    PageLayout pl;
    for (pd = pl.IterStart(&li); pd; pd = pl.IterNext()) {
        if (WasCancelRequested()) {
            l("Layout cancelled");
            ::DeletePages(pages);
            pages = NULL;
            break;
        }
        pages->Append(pd);
    }

    if (pages)
        lf("Laying out took %.2f ms", t.GetTimeInMs());
    else
        l("ThreadLayoutMobi::Run() interrupted");

    UiMsg *msg = new UiMsg(UiMsg::FinishedMobiLayout);
    msg->finishedMobiLayout.pages = pages;
    msg->finishedMobiLayout.thread = this;
    uimsg::Post(msg);
}

EbookController::EbookController(EbookControls *ctrls) : ctrls(ctrls)
{
    mobiDoc = NULL;
    html = NULL;
    pages = NULL;
    currPageNo = 0;
    layoutThread = NULL;

    pageDx = 0; pageDy = 0;
    SetStatusText();

    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->next, this);
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->prev, this);
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->progress, this);
    ctrls->mainWnd->evtMgr->RegisterSizeChanged(ctrls->page, this);
}

EbookController::~EbookController()
{
    ctrls->mainWnd->evtMgr->UnRegisterClicked(this);
    ctrls->mainWnd->evtMgr->UnRegisterSizeChanged(this);
    DeletePages();
    delete mobiDoc;
}

void EbookController::DeletePages()
{
    ctrls->page->SetPage(NULL);
    ::DeletePages(pages);
    pages = NULL;
}

static LayoutInfo *GetLayoutInfo(const char *html, MobiDoc *mb, int dx, int dy, PoolAllocator *textAllocator)
{
    LayoutInfo *li = new LayoutInfo();

    size_t len;
    if (html)
        len = strlen(html);
    else
        html = mb->GetBookHtmlData(len);

    li->fontName = FONT_NAME;
    li->fontSize = FONT_SIZE;
    li->htmlStr = html;
    li->htmlStrLen = len;
    li->pageDx = dx;
    li->pageDy = dy;
    li->textAllocator = textAllocator;
    return li;
}

// a special-case: html is just for testing so we don't do it on a thread
void EbookController::LayoutHtml(int dx, int dy)
{
    LayoutInfo *li = GetLayoutInfo(html, NULL, dx, dy, &textAllocator);
    Vec<PageData*> *htmlPages = ::LayoutHtml(li);
    delete li;
    pageDx = dx;
    pageDy = dy;
    DeletePages();
    pages = htmlPages;
    GoToPage(1);
}

void EbookController::FinishedMobiLayout(UiMsg *msg)
{
    CrashIf(UiMsg::FinishedMobiLayout != msg->type);
    if (layoutThread != msg->finishedMobiLayout.thread) {
        l("EbookController::FinishedMobiLayout() thread discarded");
        CrashIf(msg->finishedMobiLayout.pages);
        // this is a cancelled thread, we can disregard
        delete msg->finishedMobiLayout.thread;
        return;
    }
    l("EbookController::FinishedMobiLayout() got new pages");
    layoutThread = NULL;
    DeletePages();
    pages =  msg->finishedMobiLayout.pages;
    // TODO: should set the page to a page we were on the last time
    GoToPage(1);
    delete msg->finishedMobiLayout.thread;
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
    if (html) {
        LayoutHtml(dx, dy);
        return;
    }

    if (!mobiDoc)
        return;

    if (layoutThread)
        layoutThread->RequestCancel();

    layoutThread = new ThreadLayoutMobi();
    layoutThread->controller = this;
    layoutThread->fontName = str::Dup(FONT_NAME);
    layoutThread->fontSize = FONT_SIZE;
    layoutThread->mobiDoc = mobiDoc;
    layoutThread->pageDx = dx;
    layoutThread->pageDy = dy;
    layoutThread->textAllocator = &textAllocator;
    layoutThread->Start();
    ctrls->status->SetText(_T("Please wait, laying out the text"));
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
        if (pages) {
            int pageCount = pages->Count();
            int pageNo = IntFromPerc(pageCount, perc);
            GoToPage(pageNo + 1);
        }
        return;
    }

    CrashAlwaysIf(true);
}

void EbookController::SetStatusText() const
{
    if (!pages) {
        ctrls->status->SetText(_T(" "));
        ctrls->progress->SetFilled(0.f);
        return;
    }
    size_t pageCount = pages->Count();
    ScopedMem<TCHAR> s(str::Format(_T("Page %d out of %d"), currPageNo, (int)pageCount));
    ctrls->status->SetText(s.Get());
    ctrls->progress->SetFilled(PercFromInt(pageCount, currPageNo));
}

void EbookController::GoToPage(int newPageNo)
{
    CrashIf((newPageNo < 1) || (newPageNo > (int)pages->Count()));
    currPageNo = newPageNo;
    SetStatusText();
    PageData *pageData = pages->At(currPageNo-1);
    ctrls->page->SetPage(pageData);
}

void EbookController::GoToLastPage()
{
    GoToPage(pages->Count());
}

void EbookController::AdvancePage(int dist)
{
    if (!pages)
        return;
    int newPageNo = currPageNo + dist;
    if (newPageNo < 1)
        return;
    if (newPageNo > (int)pages->Count())
        return;
    GoToPage(newPageNo);
}

void EbookController::SetHtml(const char *newHtml)
{
    html = newHtml;
    TriggerLayout();
}

void EbookController::FinishedMobiLoading(UiMsg *msg)
{
    CrashIf(UiMsg::FinishedMobiLoading != msg->type);
    delete mobiDoc;
    html = NULL;
    if (NULL == msg->finishedMobiLoading.mobiDoc) {
        l("ControlEbook::FinishedMobiLoading(): failed to load");
        // TODO: a better way to notify about this, should be a transient message
        ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), msg->finishedMobiLoading.fileName));
        ctrls->status->SetText(s.Get());
    } else {
        DeletePages();
        mobiDoc = msg->finishedMobiLoading.mobiDoc;
        msg->finishedMobiLoading.mobiDoc = NULL;
        TriggerLayout();
    }
}

void EbookController::LoadMobi(const TCHAR *fileName)
{
    // note: ThreadLoadMobi object will get automatically deleted, so no
    // need to keep it around
    ThreadLoadMobi *loadThread = new ThreadLoadMobi(fileName);
    loadThread->Start();

    // TODO: better way to show this message
    ScopedMem<TCHAR> s(str::Format(_T("Please wait, loading %s"), fileName));
    ctrls->status->SetText(s.Get());
}
