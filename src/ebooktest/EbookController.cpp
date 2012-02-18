/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "EbookController.h"

#include "EbookControls.h"
#include "MobiDoc.h"
#include "PageLayout.h"
#include "ThreadUtil.h"
#include "Timer.h"
#include "EbookUiMsg.h"
#include "UiMsg.h"

#include "DebugLog.h"

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12

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
    lf(_T("ControlEbook::LoadMobiBackground(%s)"), fileName);
    Timer t(true);
    MobiDoc *mobiDoc = MobiDoc::ParseFile(fileName);
    double loadingTimeMs = t.GetCurrTimeInMs();
    lf(_T("Loaded %s in %.2f ms"), fileName, t.GetCurrTimeInMs());

    UiMsg *msg = new UiMsg(UiMsg::FinishedMobiLoading);
    msg->finishedMobiLoading.mobiDoc = mobiDoc;
    msg->finishedMobiLoading.fileName = fileName;
    fileName = NULL;
    uimsg::Post(msg);
}

EbookController::EbookController(EbookControls *ctrls) : ctrls(ctrls)
{
    mb = NULL;
    html = NULL;
    pages = NULL;
    currPageNo = 0;
    pageDx = 0; pageDy = 0;

    SetStatusText();

    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->next, this);
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->prev, this);
    ctrls->mainWnd->evtMgr->RegisterClicked(ctrls->horizProgress, this);
    ctrls->mainWnd->evtMgr->RegisterSizeChanged(ctrls->page, this);
}

EbookController::~EbookController()
{
    ctrls->mainWnd->evtMgr->UnRegisterClicked(this);
    ctrls->mainWnd->evtMgr->UnRegisterSizeChanged(this);
    DeletePages();
    delete mb;
}

void EbookController::DeletePages()
{
    if (!pages)
        return;
    ctrls->page->SetPage(NULL);
    DeleteVecMembers<PageData*>(*pages);
    delete pages;
    pages = NULL;
}

static LayoutInfo *GetLayoutInfo(const char *html, MobiDoc *mb, int dx, int dy)
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
    return li;
}

// a special-case: html is just for testing so we don't do it on a thread
void EbookController::LayoutHtml(int dx, int dy)
{
    LayoutInfo *li = GetLayoutInfo(html, NULL, dx, dy);
    Vec<PageData*> *htmlPages = ::LayoutHtml(li);
    delete li;
    pageDx = dx;
    pageDy = dy;
    DeletePages();
    pages = htmlPages;
    SetPage(1);
}

void EbookController::TriggerLayout()
{
    Size s = ctrls->page->GetDrawableSize();
    int dx = s.Width; int dy = s.Height;
    if ((0 == dx) || (0 == dy)) {
        // we haven't yet been sized, so quit
        return;
    }
    lf("EbookController::TriggerLayout (%d, %d)", dx, dy);
    CrashIf((dx < 100) || (dy < 40));
    if (html) {
        LayoutHtml(dx, dy);
        return;
    }
    // TODO: layout mobi on a background thread
}

void EbookController::SizeChanged(Control *c, int dx, int dy)
{
    CrashIf(c != ctrls->page);
    TriggerLayout();
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

    if (c == ctrls->horizProgress) {
        float perc = ctrls->horizProgress->GetPercAt(x);
        if (pages) {
            int pageCount = pages->Count();
            int pageNo = IntFromPerc(pageCount, perc);
            SetPage(pageNo + 1);
        }
        return;
    }

    CrashAlwaysIf(true);
}

void EbookController::SetStatusText() const
{
    if (!pages) {
        ctrls->status->SetText(_T(" "));
        ctrls->horizProgress->SetFilled(0.f);
        return;
    }
    size_t pageCount = pages->Count();
    ScopedMem<TCHAR> s(str::Format(_T("Page %d out of %d"), currPageNo, (int)pageCount));
    ctrls->status->SetText(s.Get());
    ctrls->horizProgress->SetFilled(PercFromInt(pageCount, currPageNo));
}

void EbookController::SetPage(int newPageNo)
{
    CrashIf((newPageNo < 1) || (newPageNo > (int)pages->Count()));
    currPageNo = newPageNo;
    SetStatusText();
    PageData *pageData = pages->At(currPageNo-1);
    ctrls->page->SetPage(pageData);
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
    SetPage(newPageNo);
}

void EbookController::PageLayout(int dx, int dy)
{
    lf("ControlEbook::PageLayout: (%d,%d)", dx, dy);
    if ((dx == pageDx) && (dy == pageDy) && pages)
        return;

    if (pages)
        return;

    LayoutInfo *li = GetLayoutInfo(NULL, mb, dx, dy);
    pages = ::LayoutHtml(li);
    delete li;
    pageDx = dx;
    pageDy = dy;

#if 0
    pageLayoutThread = new base::Thread("ControlEbook::PageLayoutBackground");
    pageLayoutThread->Start();
    pageLayoutThread->message_loop()->PostTask(base::Bind(&ControlEbook::PageLayoutBackground,
                                             base::Unretained(this), li));
#endif
}

void EbookController::SetHtml(const char *newHtml)
{
    html = newHtml;
    TriggerLayout();
}

void EbookController::FinishedMobiLoading(UiMsg *msg)
{
    CrashIf(UiMsg::FinishedMobiLoading != msg->type);
    delete mb;
    html = NULL;
    if (NULL == msg->finishedMobiLoading.mobiDoc) {
        l("ControlEbook::FinishedMobiLoading(): failed to load");
        // TODO: a better way to notify about this, should be a transient message
        ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), msg->finishedMobiLoading.fileName));
        ctrls->status->SetText(s.Get());
    } else {
        DeletePages();
        mb = msg->finishedMobiLoading.mobiDoc;
        msg->finishedMobiLoading.mobiDoc = NULL;
        TriggerLayout();
    }
}

void EbookController::LoadMobi(const TCHAR *fileName)
{
    // note: ThreadLoadMobi object will get automatically deleted, so no
    // need to keep it aroun
    ThreadLoadMobi *loadThread = new ThreadLoadMobi(fileName);
    loadThread->Start();

    // TODO: better way to show this message
    ScopedMem<TCHAR> s(str::Format(_T("Please wait, loading %s"), fileName));
    ctrls->status->SetText(s.Get());
}


