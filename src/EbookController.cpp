/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookController.h"

#include "AppPrefs.h"
#include "BaseEngine.h"
//#define NOLOG 0
#include "DebugLog.h"
#include "EbookControls.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "MobiDoc.h"
#include "EbookFormatter.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "ThreadUtil.h"
#include "Timer.h"
#include "UITask.h"
#include "WindowInfo.h"

static const WCHAR *GetFontName()
{
    // TODO: validate the name?
    return gGlobalPrefs->ebookUI.fontName;
}

static float GetFontSize()
{
    float fontSize = gGlobalPrefs->ebookUI.fontSize;
    if (fontSize < 7.f || fontSize > 32.f)
        fontSize = 12.5;
    return fontSize;
}

HtmlFormatterArgs *CreateFormatterArgsDoc(Doc doc, int dx, int dy, Allocator *textAllocator)
{
    HtmlFormatterArgs *args = CreateFormatterDefaultArgs(dx, dy, textAllocator);
    args->htmlStr = doc.GetHtmlData(args->htmlStrLen);
    args->SetFontName(GetFontName());
    args->fontSize = GetFontSize();
    return args;
}

HtmlFormatter *CreateFormatter(Doc doc, HtmlFormatterArgs* args)
{
    if (doc.AsEpub())
        return new EpubFormatter(args, doc.AsEpub());
    if (doc.AsFb2())
        return new Fb2Formatter(args, doc.AsFb2());
    if (doc.AsMobi())
        return new MobiFormatter(args, doc.AsMobi());
    CrashIf(true);
    return NULL;
}

static WindowInfo *FindWindowInfoByController(EbookController *controller)
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        if (win->AsEbook() && win->AsEbook()->ctrl() == controller)
            return win;
    }
    return NULL;
}

class EbookFormattingTask : public UITask {
public:
    enum { MAX_PAGES = 256 };
    HtmlPage *         pages[MAX_PAGES];
    size_t             pageCount;
    bool               finished;
    EbookController *  controller;
    LONG               threadNo;

    EbookFormattingTask(HtmlPage **pages, size_t pageCount, bool finished, EbookController *controller, LONG threadNo) :
        pageCount(pageCount), finished(finished),
        controller(controller), threadNo(threadNo) {
        CrashIf(pageCount > MAX_PAGES);
        memcpy(this->pages, pages, pageCount * sizeof(*pages));
    }

    virtual void Execute() {
        if (FindWindowInfoByController(controller))
            controller->HandlePagesFromEbookLayout(this);
    }
};

class EbookFormattingThread : public ThreadBase {
    Doc                 doc; // we own it
    HtmlFormatterArgs * formatterArgs; // we own it

    EbookController *   controller;

    // state used during layout process
    HtmlPage *  pages[EbookFormattingTask::MAX_PAGES];
    int         pageCount;

    // we want to send 2 pages after reparseIdx as soon as we have them,
    // so that we can show them to the user as quickly as possible
    // We want 2 to accomodate possible 2 page view
    int         reparseIdx;
    int         pagesAfterReparseIdx;

public:
    void        SendPagesIfNecessary(bool force, bool finished);
    bool        Format();

    EbookFormattingThread(Doc doc, HtmlFormatterArgs *args, EbookController *ctrl, int reparseIdx);
    virtual ~EbookFormattingThread();

    // ThreadBase
    virtual void Run();
};

EbookFormattingThread::EbookFormattingThread(Doc doc, HtmlFormatterArgs *args, EbookController *ctrl, int reparseIdx) :
    doc(doc), formatterArgs(args), controller(ctrl), pageCount(0), reparseIdx(reparseIdx), pagesAfterReparseIdx(0)
{
    CrashIf(reparseIdx < 0);
    AssertCrash(doc.IsDocLoaded() || (doc.IsNone() && (NULL != args->htmlStr)));
}

EbookFormattingThread::~EbookFormattingThread()
{
    //lf("ThreadLayoutEbook::~ThreadLayoutEbook()");
    delete formatterArgs;
}

// send accumulated pages if we filled the buffer or the caller forces us
void EbookFormattingThread::SendPagesIfNecessary(bool force, bool finished)
{
    if (finished)
        force = true;
    if (!force && (pageCount < dimof(pages)))
        return;
    EbookFormattingTask *msg = new EbookFormattingTask(pages, pageCount, finished, controller, GetNo());
    //lf("ThreadLayoutEbook::SendPagesIfNecessary() sending %d pages, finished=%d", pageCount, (int)finished);
    pageCount = 0;
    memset(pages, 0, sizeof(pages));
    uitask::Post(msg);
}

// layout pages from a given reparse point (beginning if NULL)
// returns true if layout thread was cancelled
bool EbookFormattingThread::Format()
{
    //lf("Started laying out ebook, reparseIdx=%d", reparseIdx);
    int totalPageCount = 0;
    formatterArgs->reparseIdx = 0;
    pagesAfterReparseIdx = 0;
    HtmlFormatter *formatter = CreateFormatter(doc, formatterArgs);
    for (HtmlPage *pd = formatter->Next(); pd; pd = formatter->Next()) {
        if (WasCancelRequested()) {
            //lf("layout cancelled");
            for (int i = 0; i < pageCount; i++) {
                delete pages[i];
            }
            pageCount = 0;
            delete pd;
            // send a 'finished' message so that the thread object gets deleted
            SendPagesIfNecessary(true, true /* finished */);
            delete formatter;
            return true;
        }
        pages[pageCount++] = pd;
        ++totalPageCount;
        if (pd->reparseIdx >= reparseIdx) {
            ++pagesAfterReparseIdx;
        }
        // force sending accumulated pages
        bool force = false;
        if (2 == pagesAfterReparseIdx) {
            force = true;
            //lf("EbookFormattingThread::Format: sending pages because pagesAfterReparseIdx == %d", pagesAfterReparseIdx);
        }
        SendPagesIfNecessary(force, false);
        CrashIf(pageCount >= dimof(pages));
    }
    SendPagesIfNecessary(true, true /* finished */);
    delete formatter;
    return false;
}

void EbookFormattingThread::Run()
{
    Timer t(true);
    Format();
    //lf("Formatting time: %.2f ms", t.Stop());
}

static void DeletePages(Vec<HtmlPage*>** toDeletePtr)
{
    if (!*toDeletePtr)
        return;

    DeleteVecMembers(**toDeletePtr);
    delete *toDeletePtr;
    *toDeletePtr = NULL;
}

EbookController::EbookController(EbookControls *ctrls, DisplayMode displayMode) : ctrls(ctrls),
    pages(NULL), incomingPages(NULL),
    currPageNo(0), pageSize(0, 0), formattingThread(NULL), formattingThreadNo(-1),
    currPageReparseIdx(0)
{
    EventMgr *em = ctrls->mainWnd->evtMgr;
    em->EventsForName("next")->Clicked.connect(this, &EbookController::ClickedNext);
    em->EventsForName("prev")->Clicked.connect(this, &EbookController::ClickedPrev);
    em->EventsForControl(ctrls->progress)->Clicked.connect(this, &EbookController::ClickedProgress);
    PageControl *page1 = ctrls->pagesLayout->GetPage1();
    em->EventsForControl(page1)->SizeChanged.connect(this, &EbookController::SizeChangedPage);
    PageControl *page2 = ctrls->pagesLayout->GetPage2();
    em->EventsForControl(page2)->SizeChanged.connect(this, &EbookController::SizeChangedPage);
    // displayMode could be any value if alternate UI was used, we have to limit it to
    // either DM_SINGLE_PAGE or DM_FACING
    if ((DM_SINGLE_PAGE == displayMode) || (DM_AUTOMATIC == displayMode)) {
        SetSinglePage();
    } else {
        SetDoublePage();
    }
    UpdateStatus();
}

EbookController::~EbookController()
{
    StopFormattingThread();
    EventMgr *evtMgr = ctrls->mainWnd->evtMgr;
    // we must manually disconnect all events becuase evtMgr is
    // destroyed after EbookController, and EbookController destructor
    // will disconnect slots without deleting them, causing leaks
    // TODO: this seems fragile
    evtMgr->DisconnectEvents(this);
    CloseCurrentDocument();
}

// stop layout thread (if we're closing a document we'll delete
// the ebook data, so we can't have the thread keep using it)
void EbookController::StopFormattingThread()
{
    if (!formattingThread)
        return;
    formattingThread->RequestCancel();
    bool ok = formattingThread->Join();
    CrashIf(!ok);
    delete formattingThread;
    formattingThread = NULL;
    formattingThreadNo = -1;
    DeletePages(&incomingPages);
}

void EbookController::CloseCurrentDocument()
{
    ctrls->pagesLayout->GetPage1()->SetPage(NULL);
    ctrls->pagesLayout->GetPage2()->SetPage(NULL);
    StopFormattingThread();
    DeletePages(&pages);
    doc.Delete();
    pageSize = SizeI(0, 0);
}

// returns page whose content contains reparseIdx
// page is in 1..$pageCount range to match currPageNo
// returns 0 if not found
// TODO: return -1 on not found
static int PageForReparsePoint(Vec<HtmlPage*> *pages, int reparseIdx)
{
    if (!pages)
        return 0;
    for (size_t i = 0; i < pages->Count(); i++) {
        HtmlPage *pd = pages->At(i);
        if (pd->reparseIdx == reparseIdx)
            return (int)i + 1;
        // this is the first page whose content is after reparseIdx, so
        // the page contining reparseIdx must be the one before
        if (pd->reparseIdx > reparseIdx) {
            // TODO: happened in e.g. crash 54140
            //CrashIf(0 == i);
            return (int)i;
        }
    }
    return 0;
}

// gets pages as formatted from beginning, either from a temporary state
// when layout is in progress or final formatted pages
Vec<HtmlPage*> *EbookController::GetPages()
{
    return pages;
}

void EbookController::HandlePagesFromEbookLayout(EbookFormattingTask *ft)
{
    if (formattingThreadNo != ft->threadNo) {
        // this is a message from cancelled thread, we can disregard
        lf("EbookController::HandlePagesFromEbookLayout() thread msg discarded, curr thread: %d, sending thread: %d", formattingThreadNo, ft->threadNo);
        return;
    }
    //lf("EbookController::HandlePagesFromEbookLayout() %d pages, ft=0x%x", ft->pageCount, (int)ft);
    if (incomingPages) {
        for (size_t i = 0; i < ft->pageCount; i++) {
            incomingPages->Append(ft->pages[i]);
        }
        int pageNo = PageForReparsePoint(incomingPages, currPageReparseIdx);
        if (0 != pageNo) {
            Vec<HtmlPage*> *toDelete = pages;
            pages = incomingPages;
            incomingPages = NULL;
            DeletePages(&toDelete);
            GoToPage(pageNo);
        }
    } else {
        CrashIf(!pages);
        for (size_t i = 0; i < ft->pageCount; i++) {
            pages->Append(ft->pages[i]);
        }
    }

    if (ft->finished) {
        CrashIf(!pages);
        StopFormattingThread();
    }
    UpdateStatus();
}

void EbookController::TriggerBookFormatting()
{
    Size s = ctrls->pagesLayout->GetPage1()->GetDrawableSize();
    SizeI size(s.Width, s.Height);
    if (size.IsEmpty()) {
        // we haven't been sized yet
        return;
    }
    CrashIf(size.dx < 100 || size.dy < 40);
    if (!doc.IsDocLoaded())
        return;

    if (pageSize == size) {
        //lf("EbookController::TriggerBookFormatting() - skipping layout because same as last size");
        return;
    }

    //lf("(%3d,%3d) EbookController::TriggerBookFormatting",size.dx, size.dy);
    pageSize = size; // set it early to prevent re-doing layout at the same size

    StopFormattingThread();
    CrashIf(incomingPages);
    incomingPages = new Vec<HtmlPage*>(1024);

    HtmlFormatterArgs *args = CreateFormatterArgsDoc(doc, size.dx, size.dy, &textAllocator);
    formattingThread = new EbookFormattingThread(doc, args, this, currPageReparseIdx);
    formattingThreadNo = formattingThread->GetNo();
    formattingThread->Start();
    UpdateStatus();
}

void EbookController::OnLayoutTimer()
{
    TriggerBookFormatting();
}

void EbookController::SizeChangedPage(Control *c, int dx, int dy)
{
    CrashIf(!(c == ctrls->pagesLayout->GetPage1() || c==ctrls->pagesLayout->GetPage2()));
    // delay re-layout so that we don't unnecessarily do the
    // work as long as the user is still resizing the window

    WindowInfo *win = FindWindowInfoByController(this);
    // TODO: should always succeed once FindWindowInfoByController searches background tabs
    if (!win) return;

    // TODO: previously, the delay was 100 while inSizeMove and 600 else
    // (to delay a bit if the user resizes but not when e.g. switching to fullscreen)
    SetTimer(win->hwndCanvas, EBOOK_LAYOUT_TIMER_ID, EBOOK_LAYOUT_DELAY_IN_MS, NULL);
}

void EbookController::ClickedNext(Control *c, int x, int y)
{
    //CrashIf(c != ctrls->next);
    AdvancePage(1);
}

void EbookController::ClickedPrev(Control *c, int x, int y)
{
    //CrashIf(c != ctrls->prev);
    AdvancePage(-1);
}

// (x, y) is in the coordinates of w
void EbookController::ClickedProgress(Control *c, int x, int y)
{
    CrashIf(c != ctrls->progress);
    float perc = ctrls->progress->GetPercAt(x);
    int pageCount = (int)GetPages()->Count();
    int newPageNo = IntFromPerc(pageCount, perc) + 1;
    GoToPage(newPageNo);
}

size_t EbookController::GetMaxPageCount()
{
    Vec<HtmlPage *> *pagesTmp = pages;
    if (incomingPages) {
        CrashIf(!FormattingInProgress());
        pagesTmp = incomingPages;
    }
    if (!pagesTmp)
        return 0;
    return pagesTmp->Count();
}

// show the status text based on current state
void EbookController::UpdateStatus()
{
    size_t pageCount = GetMaxPageCount();
    if (FormattingInProgress()) {
        ScopedMem<WCHAR> s(str::Format(_TR("Formatting the book... %d pages"), pageCount));
        ctrls->status->SetText(s);
        ctrls->progress->SetFilled(0.f);
        return;
    }

    ScopedMem<WCHAR> s(str::Format(L"%s %d / %d", _TR("Page:"), currPageNo, pageCount));
    ctrls->status->SetText(s);
#if 1
    ctrls->progress->SetFilled(PercFromInt((int)pageCount, currPageNo));
#else
    if (GetPages())
        ctrls->progress->SetFilled(PercFromInt((int)pageCount, currPageNo));
    else
        ctrls->progress->SetFilled(0.f);
#endif
}

void EbookController::GoToPage(int newPageNo)
{
    // we're still formatting, disable page movement
    if (incomingPages) {
        //lf("EbookController::GoToPage(%d): skipping because incomingPages != NULL", newPageNo);
        return;
    }

    CrashIf(!pages);
    // Hopefully prevent crashes like 55175
    if (!pages) {
        return;
    }

    int pageCount = (int)pages->Count();
    int n = IsSinglePage() ? 0 : 1;
    if (newPageNo + n > pageCount)
        newPageNo = pageCount - n;
    // if have only 1 page and showing double, we could go below 1
    if (newPageNo < 1)
        newPageNo = 1;

    HtmlPage *p = pages->At(newPageNo - 1);
    currPageNo = newPageNo;
    currPageReparseIdx = p->reparseIdx;
    ctrls->pagesLayout->GetPage1()->SetPage(p);
    if (IsDoublePage() && pages->Count() > 1) {
        p = pages->At(newPageNo);
        ctrls->pagesLayout->GetPage2()->SetPage(p);
    } else {
        ctrls->pagesLayout->GetPage2()->SetPage(NULL);
    }
    UpdateStatus();
}

void EbookController::GoToLastPage()
{
    GoToPage((int)pages->Count());
}

void EbookController::AdvancePage(int dist)
{
    if (IsDoublePage())
        dist = dist * 2;
    GoToPage(currPageNo + dist);
}

void EbookController::SetDoc(Doc newDoc, int startReparseIdxArg)
{
    CrashIf(!newDoc.IsDocLoaded());
    currPageReparseIdx = startReparseIdxArg;
    if ((size_t)currPageReparseIdx >= newDoc.GetHtmlDataSize())
        currPageReparseIdx = 0;
    CloseCurrentDocument();
    doc = newDoc;
    TriggerBookFormatting();
}

void EbookController::SetSinglePage()
{
    if (IsSinglePage())
        return;
    // hiding a control will trigger re-layout which will
    // trigger book re-formatting
    ctrls->pagesLayout->GetPage2()->Hide();
}

void EbookController::SetDoublePage()
{
    if (IsDoublePage())
        return;
    // hiding a control will trigger re-layout which will
    // trigger book re-formatting
    ctrls->pagesLayout->GetPage2()->Show();
}

bool EbookController::IsSinglePage() const
{
    return !ctrls->pagesLayout->GetPage2()->IsVisible();
}

static RenderedBitmap *RenderFirstDocPageToBitmap(Doc doc, SizeI pageSize, SizeI bmpSize, int border)
{
    PoolAllocator textAllocator;
    HtmlFormatterArgs *args = CreateFormatterArgsDoc(doc, pageSize.dx - 2 * border, pageSize.dy - 2 * border, &textAllocator);
    TextRenderMethod renderMethod = args->textRenderMethod;
    HtmlFormatter *formatter = CreateFormatter(doc, args);
    HtmlPage *pd = formatter->Next();
    delete formatter;
    delete args;
    args = NULL;
    if (!pd)
        return NULL;

    Bitmap pageBmp(pageSize.dx, pageSize.dy, PixelFormat24bppRGB);
    Graphics g(&pageBmp);
    Rect r(0, 0, pageSize.dx, pageSize.dy);
    r.Inflate(1, 1);
    SolidBrush br(Color(255, 255, 255));
    g.FillRectangle(&br, r);

    ITextRender *textRender = CreateTextRender(renderMethod, &g);
    textRender->SetTextBgColor(Color(255,255,255));
    DrawHtmlPage(&g, textRender, &pd->instructions, (REAL)border, (REAL)border, false, Color((ARGB)Color::Black));
    delete pd;
    delete textRender;

    Bitmap res(bmpSize.dx, bmpSize.dy, PixelFormat24bppRGB);
    Graphics g2(&res);
    g2.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g2.DrawImage(&pageBmp, Rect(0, 0, bmpSize.dx, bmpSize.dy),
                 0, 0, pageSize.dx, pageSize.dy, UnitPixel);

    HBITMAP hbmp;
    Status ok = res.GetHBITMAP((ARGB)Color::White, &hbmp);
    if (ok != Ok)
        return NULL;
    return new RenderedBitmap(hbmp, bmpSize);
}

static RenderedBitmap *ThumbFromCoverPage(Doc doc, SizeI size)
{
    ImageData *coverImage = doc.GetCoverImage();
    if (!coverImage)
        return NULL;
    Bitmap *coverBmp = BitmapFromData(coverImage->data, coverImage->len);
    if (!coverBmp)
        return NULL;

    Bitmap res(size.dx, size.dy, PixelFormat24bppRGB);
    float scale = (float)size.dx / (float)coverBmp->GetWidth();
    int fromDy = size.dy;
    if (scale < 1.f)
        fromDy = (int)((float)coverBmp->GetHeight() * scale);
    Graphics g(&res);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    Status ok = g.DrawImage(coverBmp, Rect(0, 0, size.dx, size.dy),
                            0, 0, coverBmp->GetWidth(), fromDy, UnitPixel);
    if (ok != Ok) {
        delete coverBmp;
        return NULL;
    }
    HBITMAP hbmp;
    ok = res.GetHBITMAP((ARGB)Color::White, &hbmp);
    delete coverBmp;
    if (ok == Ok)
        return new RenderedBitmap(hbmp, SizeI(size.dx, size.dy));
    return NULL;
}

RenderedBitmap *EbookController::CreateThumbnail(SizeI size)
{
    CrashIf(!doc.IsDocLoaded());
    // if there is cover image, we use it to generate thumbnail by scaling
    // image width to thumbnail dx, scaling height proportionally and using
    // as much of it as fits in thumbnail dy
    RenderedBitmap *bmp = ThumbFromCoverPage(doc, size);
    if (!bmp) {
        // no cover image so generate thumbnail from first page
        SizeI pageSize(size.dx * 3, size.dy * 3);
        bmp = RenderFirstDocPageToBitmap(doc, pageSize, size, 10);
    }
    return bmp;
}
