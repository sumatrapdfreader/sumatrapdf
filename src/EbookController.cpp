/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "EbookController.h"

#include "EbookControls.h"
#include "EpubDoc.h"
#include "FileUtil.h"
#include "MobiDoc.h"
#include "EbookWindow.h"
#include "PageLayout.h"
#include "SumatraWindow.h"
#include "Translations.h"
#include "ThreadUtil.h"
#include "Timer.h"
#include "UiMsgSumatra.h"
#include "UiMsg.h"
#include "DebugLog.h"

/* TODO: when showing a page from pagesFromPage, its page number can be 1
   so that we can't go back even though we should. This will happen if we resize
   while showing second page whose reparse point will be within the first page
   after resize. */

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12.5f

// in EbookWindow.cpp
extern void RestartLayoutTimer(EbookController *controller);

void LayoutTemp::DeletePages()
{
    DeleteVecMembers(pagesFromBeginning);
    DeleteVecMembers(pagesFromPage);
}

ThreadLoadEbook::ThreadLoadEbook(const TCHAR *fn, EbookController *controller, const SumatraWindow& sumWin) :
    controller(controller)
{
    fileName = str::Dup(fn);
    win = sumWin;
}

static bool IsMobiFile(const TCHAR *fileName)
{
    return str::EndsWithI(fileName, _T(".mobi"));
}

static bool IsEpubFile(const TCHAR *fileName)
{
    return str::EndsWithI(fileName, _T(".epub"));
}

void ThreadLoadEbook::Run()
{
    //lf(_T("ThreadLoadEbook::Run(%s)"), fileName);
    Timer t(true);
    Doc doc;

    if (IsMobiFile(fileName)) {
        // TODO: make it Doc::CreateFromFile()
        doc.Set(MobiDoc::CreateFromFile(fileName));
    } else if (IsEpubFile(fileName)) {
        // TODO: make it Doc::CreateFromFile()
        doc.Set(EpubDoc::CreateFromFile(fileName));
    } else {
        CrashIf(true);
    }

    double loadingTimeMs = t.GetTimeInMs();
    //lf(_T("Loaded %s in %.2f ms"), fileName, t.GetTimeInMs());

    UiMsg *msg = new UiMsg(UiMsg::FinishedMobiLoading);
    msg->finishedMobiLoading.doc = new Doc(doc);
    msg->finishedMobiLoading.fileName = fileName;
    msg->finishedMobiLoading.win = win;
    fileName = NULL;
    uimsg::Post(msg);
}

class ThreadLayoutEbook : public ThreadBase {
public:
    // provided by the caller
    LayoutInfo *        layoutInfo; // we own it
    Doc                 doc;
    PoolAllocator *     textAllocator;
    EbookController *   controller;
    int                 reparseIdx;

    // state used during layout process
    PageData *  pages[MobiLayoutData::MAX_PAGES];
    int         pageCount;

    void        SendPagesIfNecessary(bool force, bool finished, bool fromBeginning);
    bool        Layout(int reparseIdx);

    ThreadLayoutEbook(LayoutInfo *li, Doc doc, EbookController *ctrl);
    virtual ~ThreadLayoutEbook();

    // ThreadBase
    virtual void Run();
};

ThreadLayoutEbook::ThreadLayoutEbook(LayoutInfo *li, Doc doc, EbookController *ctrl) :
    layoutInfo(li), doc(doc), controller(ctrl), pageCount(0)
{
    AssertCrash(doc.IsEbook());
}

ThreadLayoutEbook::~ThreadLayoutEbook()
{
    //lf("ThreadLayoutEbook::~ThreadLayoutEbook()");
    free((void*)layoutInfo->fontName);
    delete layoutInfo;
}

static void DeletePages(Vec<PageData*> *pages)
{
    if (!pages)
        return;
    DeleteVecMembers(*pages);
    delete pages;
}

// send accumulated pages if we filled the buffer or the caller forces us
void ThreadLayoutEbook::SendPagesIfNecessary(bool force, bool finished, bool fromBeginning)
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
    //lf("ThreadLayoutEbook::SendPagesIfNecessary() sending %d pages, finished=%d", pageCount, (int)finished);
    ld->pageCount = pageCount;
    ld->threadNo = threadNo;
    ld->controller = controller;
    pageCount = 0;
    memset(pages, 0, sizeof(pages));
    uimsg::Post(msg);
}

HtmlFormatter *CreateFormatterForDoc(Doc doc, LayoutInfo* li)
{
    if (doc.AsMobi())
        return new MobiFormatter(li, doc.AsMobi());
    if (doc.AsEpub())
        return new EpubFormatter(li, doc.AsEpub());

    CrashIf(true);
    return NULL;
}

// layout pages from a given reparse point (beginning if NULL)
// returns true if layout thread was cancelled
bool ThreadLayoutEbook::Layout(int reparseIdx)
{
    bool fromBeginning = (0 == reparseIdx);
    //lf("Started laying out mobi, fromBeginning=%d", (int)fromBeginning);
    int totalPageCount = 0;
    Timer t(true);
    layoutInfo->reparseIdx = reparseIdx;
    HtmlFormatter *formatter = CreateFormatterForDoc(doc, layoutInfo);
    int lastReparseIdx = reparseIdx;
    for (PageData *pd = formatter->Next(); pd; pd = formatter->Next()) {
        CrashIf(pd->reparseIdx < lastReparseIdx);
        lastReparseIdx = pd->reparseIdx;
        if (WasCancelRequested()) {
            lf("layout cancelled");
            for (int i = 0; i < pageCount; i++) {
                delete pages[i];
            }
            pageCount = 0;
            delete pd;
            // send a 'finished' message so that the thread object gets deleted
            SendPagesIfNecessary(true, true, fromBeginning);
            delete formatter;
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
    //lf("Laying out took %.2f ms", t.GetTimeInMs());
    delete formatter;
    return false;
}

void ThreadLayoutEbook::Run()
{
    //lf("ThreadLayoutEbook::Run()");

    // if we have reparsePoint, layout from that point and then
    // layout from the beginning. Otherwise just from beginning
    bool cancelled = Layout(reparseIdx);
    if (cancelled || (0 == reparseIdx))
        return;
    Layout(0);
}

EbookController::EbookController(EbookControls *ctrls) :
    ctrls(ctrls), html(NULL),
    fileBeingLoaded(NULL), pagesFromBeginning(NULL), pagesFromPage(NULL),
    currPageNo(0), pageShown(NULL), deletePageShown(false),
    pageDx(0), pageDy(0), layoutThread(NULL), layoutThreadNo(-1), startReparseIdx(-1)
{
    EventMgr *em = ctrls->mainWnd->evtMgr;
    em->EventsForControl(ctrls->next)->Clicked.connect(this, &EbookController::ClickedNext);
    em->EventsForControl(ctrls->prev)->Clicked.connect(this, &EbookController::ClickedPrev);
    em->EventsForControl(ctrls->progress)->Clicked.connect(this, &EbookController::ClickedProgress);
    em->EventsForControl(ctrls->page)->SizeChanged.connect(this, &EbookController::SizeChangedPage);
    UpdateStatus();
}

EbookController::~EbookController()
{
    StopLayoutThread(true);
    EventMgr *evtMgr = ctrls->mainWnd->evtMgr;
    evtMgr->RemoveEventsForControl(ctrls->next);
    evtMgr->RemoveEventsForControl(ctrls->prev);
    evtMgr->RemoveEventsForControl(ctrls->progress);
    evtMgr->RemoveEventsForControl(ctrls->page);
    CloseCurrentDocument();
}

void EbookController::DeletePageShown()
{
    if (deletePageShown)
        delete pageShown;
    pageShown = NULL;
}

// stop layout thread and optionally terminate it (if we're closing
// a document we'll delete the ebook data so we can't have the thread
// keep using it)
void EbookController::StopLayoutThread(bool forceTerminate)
{
    if (!layoutThread)
        return;
    if (layoutThread->RequestCancelAndWaitToStop(1000, forceTerminate))
        layoutThread->Release();
    layoutThread = NULL;
    layoutThreadNo = -1;
    layoutTemp.DeletePages();
}

void EbookController::CloseCurrentDocument()
{
    ctrls->page->SetPage(NULL);
    StopLayoutThread(true);
    DeletePageShown();
    DeletePages(&pagesFromBeginning);
    DeletePages(&pagesFromPage);
    doc.Delete();
    html = NULL;
    layoutTemp.reparseIdx = 0; // mark as being laid out from the beginning
    pageDx = 0; pageDy = 0;
}

void EbookController::DeletePages(Vec<PageData*>** pages)
{
    CrashIf((pagesFromBeginning != *pages) && 
            (pagesFromPage != *pages));
#if 0
    if (pagesFromPage && (*pages == pagesFromPage))
        lf("Deleting pages from page");
#endif
    ::DeletePages(*pages);
    *pages = NULL;
}


LayoutInfo *GetLayoutInfo(const char *html, Doc doc, int dx, int dy, PoolAllocator *textAllocator)
{
    LayoutInfo *li = new LayoutInfo();
    size_t len;
    if (html) {
        CrashIf(!doc.IsNone());
        len = strlen(html);
    } else {
        if (doc.AsMobi())
            html = doc.AsMobi()->GetBookHtmlData(len);
        else if (doc.AsEpub())
            html = doc.AsEpub()->GetTextData(&len);
        else
            CrashIf(true);
    }
    li->fontName = str::Dup(FONT_NAME);
    li->fontSize = FONT_SIZE;
    li->htmlStr = html;
    li->htmlStrLen = len;
    li->pageDx = dx;
    li->pageDy = dy;
    li->textAllocator = textAllocator;
    return li;
}

// returns page whose content contains reparseIdx
// page is in 1..$pageCount range to match currPageNo
// returns 0 if not found
static size_t PageForReparsePoint(Vec<PageData*> *pages, int reparseIdx)
{
    if (!pages)
        return 0;
    for (size_t i = 0; i < pages->Count(); i++) {
        PageData *pd = pages->At(i);
        if (pd->reparseIdx == reparseIdx)
            return i + 1;
        // this is the first page whose content is after reparseIdx, so
        // the page contining reparseIdx must be the one before
        if (pd->reparseIdx > reparseIdx) {
            CrashIf(0 == i);
            return i;
        }
    }
    return 0;
}

// gets pages as formatted from beginning, either from a temporary state
// when layout is in progress or final formatted pages
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
    // reparse point is not reliable. try first to find by page object
    if (GetPagesFromBeginning()) {
        int n = GetPagesFromBeginning()->Find(pd) + 1;
        if (n > 0) {
            currPageNo = n;
            return;
        }
    }
    currPageNo = PageForReparsePoint(GetPagesFromBeginning(), pd->reparseIdx);
}

void EbookController::ShowPage(PageData *pd, bool deleteWhenDone)
{
    DeletePageShown();
    pageShown = pd;
    deletePageShown = deleteWhenDone;
    ctrls->page->SetPage(pageShown);

    UpdateCurrPageNoForPage(pageShown);
#if 0
    if (pd) {
        char s[64] = { 0 };
        //TODO: this would have to be html + pd->reparseIdx
        //memcpy(s, pd->reparseIdx, dimof(s) - 1);
        lf("ShowPage() %d '%s'", currPageNo, s);
    }
#endif
}

void EbookController::HandleMobiLayoutMsg(MobiLayoutData *ld)
{
    if (layoutThreadNo != ld->threadNo) {
        // this is a message from cancelled thread, we can disregard
        //lf("EbookController::MobiLayout() thread msg discarded, curr thread: %d, sending thread: %d", layoutThreadNo, ld->threadNo);
        return;
    }
    //lf("EbookController::HandleMobiLayoutMsg() %d pages, ld=0x%x", ld->pageCount, (int)ld);
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
            CrashIf(pageToShow->reparseIdx != pageShown->reparseIdx);
            ShowPage(pageToShow, false);
        }
        //lf("Got %d pages from page, total %d", ld->pageCount, layoutTemp.pagesFromPage.Count());
        UpdateStatus();
        return;
    }

    // we're showing pages from the beginning
    if (-1 != startReparseIdx) {
        // we're formatting a book for which we need to restore
        // page from previous session
        CrashIf(layoutTemp.reparseIdx != 0);
        for (size_t i = 0; i < ld->pageCount; i++) {
            PageData *pd = ld->pages[i];
            if (pd->reparseIdx == startReparseIdx) {
                pageToShow = pd;
            } else if (pd->reparseIdx >= startReparseIdx) {
                // this is the first page whose reparseIdx is greater than
                // the one we're looking for, so previous page has the data
                if (i > 0) {
                    pageToShow = ld->pages[i];
                    //lf("showing page %d", i);
                } else {
                    // TODO: showed up in crash reports
                    CrashIf(0 == layoutTemp.pagesFromBeginning.Count());
                    size_t pageNo = layoutTemp.pagesFromBeginning.Count() - 1;
                    //lf("showing page %d from layoutTemp.pagesFromBeginning", (int)pageNo);
                    pageToShow = layoutTemp.pagesFromBeginning.At(pageNo);
                }
            }
            if (pageToShow) {
                startReparseIdx = -1;
                break;
            }
        }
    } else {
        if (0 == layoutTemp.pagesFromBeginning.Count()) {
            CrashIf(0 == ld->pageCount);
            pageToShow = ld->pages[0];
            //lf("showing ld->pages[0], pageCount = %d", ld->pageCount);
        }
    }

    layoutTemp.pagesFromBeginning.Append(ld->pages, ld->pageCount);
    //lf("Got %d pages from beginning, total %d", ld->pageCount, layoutTemp.pagesFromBeginning.Count());

    if (0 == layoutTemp.reparseIdx) {
        // if we're starting from the beginning, show the first page as
        // quickly as we can
        if (pageToShow)
            ShowPage(pageToShow, false);
    }

    if (ld->finished) {
        CrashIf(pagesFromBeginning || pagesFromPage);
        pagesFromBeginning = new Vec<PageData*>();
        PageData **pages = layoutTemp.pagesFromBeginning.LendData();
        size_t pageCount =  layoutTemp.pagesFromBeginning.Count();
        pagesFromBeginning->Append(pages, pageCount);
        layoutTemp.pagesFromBeginning.Reset();

        pageCount =  layoutTemp.pagesFromPage.Count();
        if (pageCount > 0) {
            pages = layoutTemp.pagesFromPage.LendData();
            pagesFromPage = new Vec<PageData*>();
            pagesFromPage->Append(pages, pageCount);
            layoutTemp.pagesFromPage.Reset();
        }
        StopLayoutThread(false);
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
        // TODO: this can happen due to a race condition
        //       (if there are too many WM_PAINT messages?)
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
    Size s = ctrls->page->GetDrawableSize();    int dx = s.Width; int dy = s.Height;
    if ((0 == dx) || (0 == dy)) {
        // we haven't been sized yet
        return;
    }
    CrashIf((dx < 100) || (dy < 40));
    if (!html && !doc.IsEbook())
        return;

    if ((pageDx == dx) && (pageDy == dy)) {
        //lf("EbookController::TriggerLayout() - skipping layout because same as last size");
        return;
    }

    //lf("(%3d,%3d) EbookController::TriggerLayout", dx, dy);
    pageDx = dx; pageDy = dy; // set it early to prevent re-doing layout at the same size
    PageData *newPage = PreserveTempPageShown();
    if (newPage) {
        CrashIf((layoutTemp.reparseIdx != 0) && 
                (layoutTemp.reparseIdx != newPage->reparseIdx));
    } else {
        CrashIf(layoutTemp.reparseIdx != 0);
    }

    StopLayoutThread(false);

    DeletePages(&pagesFromBeginning);
    DeletePages(&pagesFromPage);

    CrashIf(layoutTemp.pagesFromBeginning.Count() > 0);
    CrashIf(layoutTemp.pagesFromPage.Count() > 0);

    ShowPage(newPage, newPage != NULL);
    LayoutInfo *li = GetLayoutInfo(html, doc, dx, dy, &textAllocator);
    layoutThread = new ThreadLayoutEbook(li, doc, this);
    layoutThreadNo = layoutThread->GetNo();
    CrashIf(layoutTemp.reparseIdx < 0);
    CrashIf(layoutTemp.reparseIdx > (int)li->htmlStrLen);
    layoutThread->reparseIdx = layoutTemp.reparseIdx;
    layoutThread->Start();
    UpdateStatus();
}

void EbookController::OnLayoutTimer()
{
    layoutTemp.reparseIdx = NULL;
    if (pageShown)
        layoutTemp.reparseIdx = pageShown->reparseIdx;
    TriggerLayout();
}

void EbookController::SizeChangedPage(Control *c, int dx, int dy)
{
    CrashIf(c != ctrls->page);
    // delay re-layout so that we don't unnecessarily do the
    // work as long as the user is still resizing the window
    RestartLayoutTimer(this);
}

void EbookController::ClickedNext(Control *c, int x, int y)
{
    CrashIf(c != ctrls->next);
    AdvancePage(1);
}

void EbookController::ClickedPrev(Control *c, int x, int y)
{
    CrashIf(c != ctrls->prev);
    AdvancePage(-1);
}

// (x, y) is in the coordinates of w
void EbookController::ClickedProgress(Control *c, int x, int y)
{
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
    size_t pageCount = GetMaxPageCount();

    if (fileBeingLoaded) {
        ScopedMem<WCHAR> s(str::Format(_TR("Loading file %s..."), fileBeingLoaded));
        ctrls->status->SetText(s.Get());
        ctrls->progress->SetFilled(0.f);
        return;
    }

    if (LayoutInProgress()) {
        ScopedMem<WCHAR> s(str::Format(_TR("Formatting the book... %d pages"), pageCount));
        ctrls->status->SetText(s.Get());
        ctrls->progress->SetFilled(0.f);
        return;
    }

    ScopedMem<WCHAR> s(str::Format(_T("%s %d / %d"), _TR("Page:"), currPageNo, pageCount));
    ctrls->status->SetText(s.Get());
    if (GetPagesFromBeginning())
        ctrls->progress->SetFilled(PercFromInt(GetPagesFromBeginning()->Count(), currPageNo));
    else
        ctrls->progress->SetFilled(0.f);
}

void EbookController::GoToPage(int newPageNo)
{
    if ((newPageNo < 1) || (newPageNo == currPageNo))
        return;
    if (!GetPagesFromBeginning())
        return;
    if ((size_t)newPageNo > GetPagesFromBeginning()->Count())
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
    CloseCurrentDocument();
    startReparseIdx = -1;
    html = newHtml;
    TriggerLayout();
}

static int GetEbookHtmlSize(Doc doc)
{
    if (doc.AsMobi())
        return doc.AsMobi()->GetBookHtmlSize();

    if (doc.AsEpub()) {
        size_t len;
        doc.AsEpub()->GetTextData(&len);
        return len;
    }
    CrashIf(true);
    return 0;
}

void EbookController::SetDoc(Doc newDoc, int startReparseIdxArg)
{
    CrashIf(!newDoc.IsEbook());
    startReparseIdx = startReparseIdxArg;
    if (startReparseIdx >= GetEbookHtmlSize(newDoc))
        startReparseIdx = -1;
    CloseCurrentDocument();
    doc = newDoc;
    TriggerLayout();
}

void EbookController::HandleFinishedMobiLoadingMsg(FinishedMobiLoadingData *finishedMobiLoading)
{
    str::ReplacePtr(&fileBeingLoaded, NULL);
    if (NULL == finishedMobiLoading->doc) {
        // TODO: a better way to notify about this, should be a transient message
        ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), finishedMobiLoading->fileName));
        ctrls->status->SetText(AsWStrQ(s.Get()));
        return;
    }
    SetDoc(*finishedMobiLoading->doc);
}

int EbookController::CurrPageReparseIdx() const
{
    if (!pageShown)
        return 0;
    return pageShown->reparseIdx;
}

