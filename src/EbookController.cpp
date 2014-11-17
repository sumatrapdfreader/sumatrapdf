/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "ArchUtil.h"
#include "GdiPlusUtil.h"
#include "ThreadUtil.h"
#include "Timer.h"
#include "Translations.h"
#include "BaseEngine.h"
#include "SettingsStructs.h"
#include "DisplayState.h"
#include "Controller.h"
#include "Doc.h"
#include "EbookController.h"
#include "AppPrefs.h" // for gGlobalPrefs
#include "HtmlParserLookup.h"
#include "EbookBase.h"
#include "Mui.h"
#include "EbookControls.h"
#include "EbookDoc.h"
#include "HtmlFormatter.h"
#include "HtmlPullParser.h"
#include "MobiDoc.h"
#include "TrivialHtmlParser.h"
//#define NOLOG 0
#include "DebugLog.h"

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

class EbookTocDest : public DocTocItem, public PageDestination {
    ScopedMem<WCHAR> url;

public:
    EbookTocDest(const WCHAR *title, int reparseIdx) :
        DocTocItem(str::Dup(title), reparseIdx), url(NULL) { }
    EbookTocDest(const WCHAR *title, const WCHAR *url) :
        DocTocItem(str::Dup(title)), url(str::Dup(url)) { }

    virtual PageDestination *GetLink() { return this; }

    virtual PageDestType GetDestType() const { return url ? Dest_LaunchURL : Dest_ScrollTo; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return RectD(); }
    virtual WCHAR *GetDestValue() const { return str::Dup(url); }
};

struct EbookFormattingData {
    enum { MAX_PAGES = 256 };
    HtmlPage *         pages[MAX_PAGES];
    size_t             pageCount;
    bool               finished;
    LONG               threadNo;

    EbookFormattingData(HtmlPage **pages, size_t pageCount, bool finished, LONG threadNo) :
        pageCount(pageCount), finished(finished), threadNo(threadNo) {
        CrashIf(pageCount > MAX_PAGES);
        memcpy(this->pages, pages, pageCount * sizeof(*pages));
    }
};

class EbookFormattingThread : public ThreadBase {
    HtmlFormatterArgs * formatterArgs; // we own it

    Doc                 doc;
    EbookController *   controller;
    ControllerCallback *cb;

    // state used during layout process
    HtmlPage *  pages[EbookFormattingData::MAX_PAGES];
    int         pageCount;

    // we want to send 2 pages after reparseIdx as soon as we have them,
    // so that we can show them to the user as quickly as possible
    // We want 2 to accomodate possible 2 page view
    int         reparseIdx;
    int         pagesAfterReparseIdx;

public:
    void        SendPagesIfNecessary(bool force, bool finished);
    bool        Format();

    EbookFormattingThread(Doc doc, HtmlFormatterArgs *args,
                          EbookController *ctrl, int reparseIdx, ControllerCallback *cb);
    virtual ~EbookFormattingThread();

    // ThreadBase
    virtual void Run();
};

EbookFormattingThread::EbookFormattingThread(Doc doc, HtmlFormatterArgs *args, EbookController *ctrl, int reparseIdx, ControllerCallback *cb) :
    doc(doc), formatterArgs(args), cb(cb), controller(ctrl), pageCount(0), reparseIdx(reparseIdx), pagesAfterReparseIdx(0)
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
    EbookFormattingData *msg = new EbookFormattingData(pages, pageCount, finished, GetNo());
    //lf("ThreadLayoutEbook::SendPagesIfNecessary() sending %d pages, finished=%d", pageCount, (int)finished);
    pageCount = 0;
    memset(pages, 0, sizeof(pages));
    cb->HandleLayoutedPages(controller, msg);
}

// layout pages from a given reparse point (beginning if NULL)
// returns true if layout thread was cancelled
bool EbookFormattingThread::Format()
{
    //lf("Started laying out ebook, reparseIdx=%d", reparseIdx);
    int totalPageCount = 0;
    formatterArgs->reparseIdx = 0;
    pagesAfterReparseIdx = 0;
    HtmlFormatter *formatter = doc.CreateFormatter(formatterArgs);
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
    Timer t;
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

EbookController::EbookController(EbookControls *ctrls, ControllerCallback *cb) :
    Controller(cb), ctrls(ctrls), pages(NULL), incomingPages(NULL),
    currPageNo(0), pageSize(0, 0), formattingThread(NULL), formattingThreadNo(-1),
    currPageReparseIdx(0), handleMsgs(true), pageAnchorIds(NULL), pageAnchorIdxs(NULL),
    navHistoryIx(0)
{
    EventMgr *em = ctrls->mainWnd->evtMgr;
    // TODO: do I need lambada here, can I just pass EbookController::ClickedNext directly?
    em->EventsForName("next")->Clicked = [=](Control *c, int x, int y) {
        this->ClickedNext(c, x, y);
    };
    em->EventsForName("prev")->Clicked = [=](Control *c, int x, int y) {
        this->ClickedPrev(c, x, y);
    }; 
    em->EventsForControl(ctrls->progress)->Clicked = [=](Control *c, int x, int y) {
        this->ClickedProgress(c, x, y);
    };
    PageControl *page1 = ctrls->pagesLayout->GetPage1();
    PageControl *page2 = ctrls->pagesLayout->GetPage2();
    em->EventsForControl(page1)->SizeChanged = [=](Control *c, int dx, int dy) {
        this->SizeChangedPage(c, dx, dy);
    };
    em->EventsForControl(page2)->SizeChanged = [=](Control *c, int dx, int dy) {
        this->SizeChangedPage(c, dx, dy);
    };
    em->EventsForControl(page1)->Clicked = [=](Control *c, int x, int y) {
        this->ClickedPage1(c, x, y);
    };
    em->EventsForControl(page2)->Clicked = [=](Control *c, int x, int y) {
        this->ClickedPage2(c, x, y);
    };
}

EbookController::~EbookController()
{
    StopFormattingThread();
    EventMgr *evtMgr = ctrls->mainWnd->evtMgr;
    // we must manually disconnect all events becuase evtMgr is
    // destroyed after EbookController, and EbookController destructor
    // will disconnect slots without deleting them, causing leaks
    // TODO: this seems fragile
    EnableMessageHandling(false);
    CloseCurrentDocument();
    DestroyEbookControls(ctrls);
    delete pageAnchorIds;
    delete pageAnchorIdxs;
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
// returns 0 if not found (or maybe on the last page)
// returns -1 if no pages are available
static int PageForReparsePoint(Vec<HtmlPage*> *pages, int reparseIdx)
{
    if (!pages || pages->Count() == 0) {
        return -1;
    }

    // sometimes reparseIdx of first page is > 0 and the code below
    // doesn't handle that, so do that case first
    if (reparseIdx < pages->At(0)->reparseIdx) {
        return 1;
    }

    for (size_t i = 0; i < pages->Count(); i++) {
        HtmlPage *pd = pages->At(i);
        if (pd->reparseIdx == reparseIdx) {
            return (int)i + 1;
        }
        // this is the first page whose content is after reparseIdx, so
        // the page contining reparseIdx must be the one before
        if (pd->reparseIdx > reparseIdx) {
            CrashIf(0 == i);
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

void EbookController::HandlePagesFromEbookLayout(EbookFormattingData *ft)
{
    if (formattingThreadNo != ft->threadNo) {
        // this is a message from cancelled thread, we can disregard
        lf("EbookController::HandlePagesFromEbookLayout() thread msg discarded, curr thread: %d, sending thread: %d", formattingThreadNo, ft->threadNo);
        delete ft;
        return;
    }
    //lf("EbookController::HandlePagesFromEbookLayout() %d pages, ft=0x%x", ft->pageCount, (int)ft);
    if (incomingPages) {
        for (size_t i = 0; i < ft->pageCount; i++) {
            incomingPages->Append(ft->pages[i]);
        }
        int pageNo = PageForReparsePoint(incomingPages, currPageReparseIdx);
        if (pageNo > 0) {
            Vec<HtmlPage*> *toDelete = pages;
            pages = incomingPages;
            incomingPages = NULL;
            DeletePages(&toDelete);
            GoToPage(pageNo, false);
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
    delete ft;
}

void EbookController::TriggerLayout()
{
    Size s = ctrls->pagesLayout->GetPage1()->GetDrawableSize();
    SizeI size(s.Width, s.Height);
    if (size.IsEmpty()) {
        // we haven't been sized yet
        return;
    }
    // CrashIf(size.dx < 100 || size.dy < 40);
    if (!doc.IsDocLoaded())
        return;

    if (pageSize == size) {
        //lf("EbookController::TriggerLayout() - skipping layout because same as last size");
        return;
    }

    //lf("(%3d,%3d) EbookController::TriggerLayout",size.dx, size.dy);
    pageSize = size; // set it early to prevent re-doing layout at the same size

    StopFormattingThread();
    CrashIf(incomingPages);
    incomingPages = new Vec<HtmlPage*>(1024);

    HtmlFormatterArgs *args = CreateFormatterArgsDoc(doc, size.dx, size.dy, &textAllocator);
    formattingThread = new EbookFormattingThread(doc, args, this, currPageReparseIdx, cb);
    formattingThreadNo = formattingThread->GetNo();
    formattingThread->Start();
    UpdateStatus();
}

void EbookController::SizeChangedPage(Control *c, int dx, int dy)
{
    CrashIf(!(c == ctrls->pagesLayout->GetPage1() || c==ctrls->pagesLayout->GetPage2()));
    // delay re-layout so that we don't unnecessarily do the
    // work as long as the user is still resizing the window
    // TODO: previously, the delay was 100 while inSizeMove and 600 else
    // (to delay a bit if the user resizes but not when e.g. switching to fullscreen)
    cb->RequestDelayedLayout(200);
}

void EbookController::ClickedNext(Control *c, int x, int y)
{
    //CrashIf(c != ctrls->next);
    GoToNextPage();
}

void EbookController::ClickedPrev(Control *c, int x, int y)
{
    //CrashIf(c != ctrls->prev);
    GoToPrevPage();
}

// (x, y) is in the coordinates of w
void EbookController::ClickedProgress(Control *c, int x, int y)
{
    CrashIf(c != ctrls->progress);
    float perc = ctrls->progress->GetPercAt(x);
    int pageCount = (int)GetPages()->Count();
    int newPageNo = IntFromPerc(pageCount, perc) + 1;
    GoToPage(newPageNo, true);
}

void EbookController::OnClickedLink(int pageNo, DrawInstr *link)
{
    ScopedMem<WCHAR> url(str::conv::FromHtmlUtf8(link->str.s, link->str.len));
    if (url::IsAbsolute(url)) {
        EbookTocDest dest(NULL, url);
        cb->GotoLink(&dest);
        return;
    }

    if (Doc_Epub == doc.Type() && pages && (size_t)pageNo <= pages->Count()) {
        // normalize the URL by combining it with the chapter's base path
        for (int j = pageNo; j > 0; j--) {
            HtmlPage *p = pages->At(j - 1);
            // <pagebreak src="..." page_marker /> is usually the second instruction on a page
            for (size_t k = 0; k < std::min((size_t)2, p->instructions.Count()); k++) {
                DrawInstr& di = p->instructions.At(k);
                if (InstrAnchor == di.type && str::StartsWith(di.str.s + di.str.len, "\" page_marker />")) {
                    ScopedMem<char> basePath(str::DupN(di.str.s, di.str.len));
                    ScopedMem<char> relPath(ResolveHtmlEntities(link->str.s, link->str.len));
                    ScopedMem<char> absPath(NormalizeURL(relPath, basePath));
                    url.Set(str::conv::FromUtf8(absPath));
                    j = 0; // done
                    break;
                }
            }
        }
    }

    int idx = ResolvePageAnchor(url);
    if (idx != -1) {
        EbookTocDest dest(NULL, idx);
        cb->GotoLink(&dest);
    }
}

void EbookController::ClickedPage1(Control *c, int x, int y)
{
    cb->FocusFrame(true);

    CrashIf(c != ctrls->pagesLayout->GetPage1());
    DrawInstr *link = ctrls->pagesLayout->GetPage1()->GetLinkAt(x, y);
    if (link)
        OnClickedLink(currPageNo, link);
}

void EbookController::ClickedPage2(Control *c, int x, int y)
{
    cb->FocusFrame(true);

    CrashIf(!IsDoublePage());
    CrashIf(c != ctrls->pagesLayout->GetPage2());
    DrawInstr *link = ctrls->pagesLayout->GetPage2()->GetLinkAt(x, y);
    if (link)
        OnClickedLink(currPageNo + 1, link);
}

int EbookController::GetMaxPageCount() const
{
    Vec<HtmlPage *> *pagesTmp = pages;
    if (incomingPages) {
        CrashIf(!FormattingInProgress());
        pagesTmp = incomingPages;
    }
    if (!pagesTmp)
        return 0;
    return (int)pagesTmp->Count();
}

// show the status text based on current state
void EbookController::UpdateStatus()
{
    int pageCount = GetMaxPageCount();
    if (FormattingInProgress()) {
        ScopedMem<WCHAR> s(str::Format(_TR("Formatting the book... %d pages"), pageCount));
        ctrls->status->SetText(s);
        ctrls->progress->SetFilled(0.f);
        return;
    }

    ScopedMem<WCHAR> s(str::Format(L"%s %d / %d", _TR("Page:"), currPageNo, pageCount));
    ctrls->status->SetText(s);
#if 1
    ctrls->progress->SetFilled(PercFromInt(pageCount, currPageNo));
#else
    if (GetPages())
        ctrls->progress->SetFilled(PercFromInt(pageCount, currPageNo));
    else
        ctrls->progress->SetFilled(0.f);
#endif
}

void EbookController::GoToPage(int pageNo, bool addNavPoint)
{
    // we're still formatting, disable page movement
    if (incomingPages) {
        //lf("EbookController::GoToPage(%d): skipping because incomingPages != NULL", pageNo);
        return;
    }

    CrashIf(!pages);
    // Hopefully prevent crashes like 55175
    if (!pages) {
        return;
    }

    if (addNavPoint)
        AddNavPoint();

    int pageCount = PageCount();
    int n = IsDoublePage() ? 1 : 0;
    if (pageNo + n > pageCount)
        pageNo = pageCount - n;
    // if have only 1 page and showing double, we could go below 1
    if (pageNo < 1)
        pageNo = 1;

    HtmlPage *p = pages->At(pageNo - 1);
    currPageNo = pageNo;
    currPageReparseIdx = p->reparseIdx;
    ctrls->pagesLayout->GetPage1()->SetPage(p);
    if (IsDoublePage() && pages->Count() > 1) {
        p = pages->At(pageNo);
        ctrls->pagesLayout->GetPage2()->SetPage(p);
    } else {
        ctrls->pagesLayout->GetPage2()->SetPage(NULL);
    }
    UpdateStatus();
    // update the ToC selection
    cb->PageNoChanged(pageNo);
}

bool EbookController::GoToNextPage()
{
    int dist = IsDoublePage() ? 2 : 1;
    if (currPageNo + dist > PageCount())
        return false;
    GoToPage(currPageNo + dist, false);
    return true;
}

bool EbookController::GoToPrevPage(bool toBottom)
{
    int dist = IsDoublePage() ? 2 : 1;
    if (currPageNo - dist < 1)
        return false;
    GoToPage(currPageNo - dist, false);
    return true;
}

void EbookController::SetDoc(Doc newDoc, int startReparseIdxArg, DisplayMode displayMode)
{
    CrashIf(!newDoc.IsDocLoaded());
    currPageReparseIdx = startReparseIdxArg;
    if ((size_t)currPageReparseIdx >= newDoc.GetHtmlDataSize())
        currPageReparseIdx = 0;
    CloseCurrentDocument();

    doc = newDoc;
    // displayMode could be any value if alternate UI was used, we have to limit it to
    // either DM_SINGLE_PAGE or DM_FACING
    if (DM_AUTOMATIC == displayMode)
        displayMode = gGlobalPrefs->defaultDisplayModeEnum;
    SetDisplayMode(displayMode);
    TriggerLayout();
    UpdateStatus();
}

bool EbookController::IsDoublePage() const
{
    return ctrls->pagesLayout->GetPage2()->IsVisible();
}

static RenderedBitmap *RenderFirstDocPageToBitmap(Doc doc, SizeI pageSize, SizeI bmpSize, int border)
{
    PoolAllocator textAllocator;
    HtmlFormatterArgs *args = CreateFormatterArgsDoc(doc, pageSize.dx - 2 * border, pageSize.dy - 2 * border, &textAllocator);
    TextRenderMethod renderMethod = args->textRenderMethod;
    HtmlFormatter *formatter = doc.CreateFormatter(args);
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

    ITextRender *textRender = CreateTextRender(renderMethod, &g, pageSize.dx, pageSize.dy);
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

void EbookController::CreateThumbnail(SizeI size, ThumbnailCallback *tnCb)
{
    // TODO: create thumbnail asynchronously
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
    tnCb->SaveThumbnail(bmp);
}

void EbookController::SetDisplayMode(DisplayMode mode, bool keepContinuous)
{
    bool newDouble = !IsSingle(mode);
    if (IsDoublePage() == newDouble)
        return;
    // showing/hiding a control will trigger re-layout which will
    // trigger book re-formatting
    if (newDouble)
        ctrls->pagesLayout->GetPage2()->Show();
    else
        ctrls->pagesLayout->GetPage2()->Hide();
}

void EbookController::ExtractPageAnchors()
{
    if (pageAnchorIds || pageAnchorIdxs) {
        CrashIf(!pageAnchorIds || !pageAnchorIdxs);
        return;
    }

    pageAnchorIds = new WStrVec();
    pageAnchorIdxs = new Vec<int>();

    ScopedMem<WCHAR> epubPagePath;
    int fb2TitleCount = 0;
    size_t len;
    const char *data = doc.GetHtmlData(len);
    HtmlPullParser parser(data, len);
    HtmlToken *tok;
    while ((tok = parser.Next()) != NULL && !tok->IsError()) {
        if (!tok->IsStartTag() && !tok->IsEmptyElementEndTag())
            continue;
        AttrInfo *attr = tok->GetAttrByName("id");
        if (!attr && Tag_A == tok->tag && doc.Type() != Doc_Fb2)
            attr = tok->GetAttrByName("name");
        if (attr) {
            ScopedMem<WCHAR> id(str::conv::FromUtf8(attr->val, attr->valLen));
            pageAnchorIds->Append(str::Format(L"%s#%s", epubPagePath ? epubPagePath : L"", id.Get()));
            pageAnchorIdxs->Append((int)(tok->GetReparsePoint() - parser.Start()));
        }
        // update EPUB page paths and create an anchor per chapter
        if (Tag_Pagebreak == tok->tag &&
            (attr = tok->GetAttrByName("page_path")) != NULL &&
            str::StartsWith(attr->val + attr->valLen, "\" page_marker />")) {
            CrashIf(doc.Type() != Doc_Epub);
            epubPagePath.Set(str::conv::FromUtf8(attr->val, attr->valLen));
            pageAnchorIds->Append(str::Dup(epubPagePath));
            pageAnchorIdxs->Append((int)(tok->GetReparsePoint() - parser.Start()));
        }
        // create FB2 title anchors (cf. Fb2Doc::ParseToc)
        if (Tag_Title == tok->tag && tok->IsStartTag() && Doc_Fb2 == doc.Type()) {
            ScopedMem<WCHAR> id(str::Format(TEXT(FB2_TOC_ENTRY_MARK) L"%d", ++fb2TitleCount));
            pageAnchorIds->Append(id.StealData());
            pageAnchorIdxs->Append((int)(tok->GetReparsePoint() - parser.Start()));
        }
    }
}

int EbookController::ResolvePageAnchor(const WCHAR *id)
{
    ExtractPageAnchors();

    int reparseIdx = -1;
    if (Doc_Mobi == doc.Type() && str::Parse(id, L"%d%$", &reparseIdx) &&
        0 <= reparseIdx && (size_t)reparseIdx <= doc.GetHtmlDataSize()) {
        // Mobi uses filepos (reparseIdx) for in-document links
        return reparseIdx;
    }

    int idx = pageAnchorIds->Find(id);
    if (idx != -1)
        return pageAnchorIdxs->At(idx);
    if (doc.Type() != Doc_Epub || !str::FindChar(id, '#'))
        return -1;

    ScopedMem<WCHAR> chapterPath(str::DupN(id, str::FindChar(id, '#') - id));
    idx = pageAnchorIds->Find(chapterPath);
    if (idx != -1)
        return pageAnchorIdxs->At(idx);
    return -1;
}

class EbookTocCollector : public EbookTocVisitor {
    EbookController *ctrl;
    EbookTocDest *root;
    int idCounter;

public:
    explicit EbookTocCollector(EbookController *ctrl) :
        ctrl(ctrl), root(NULL), idCounter(0) { }

    virtual void Visit(const WCHAR *name, const WCHAR *url, int level) {
        EbookTocDest *item = NULL;
        if (!url)
            item = new EbookTocDest(name, 0);
        else if (url::IsAbsolute(url))
            item = new EbookTocDest(name, url);
        else {
            int idx = ctrl->ResolvePageAnchor(url);
            if (idx < 0 && str::FindChar(url, '%')) {
                ScopedMem<WCHAR> decodedUrl(str::Dup(url));
                url::DecodeInPlace(decodedUrl);
                idx = ctrl->ResolvePageAnchor(url);
            }
            item = new EbookTocDest(name, idx + 1);
        }
        item->id = ++idCounter;
        // find the last child at each level, until finding the parent of the new item
        if (!root) {
            root = item;
        }
        else {
            DocTocItem *r2 = root;
            for (level--; level > 0; level--) {
                for (; r2->next; r2 = r2->next);
                if (!r2->child)
                    break;
                r2 = r2->child;
            }
            if (level <= 0)
                r2->AddSibling(item);
            else
                r2->child = item;
        }
    }

    EbookTocDest *GetRoot() { return root; }
};

DocTocItem *EbookController::GetTocTree()
{
    EbookTocCollector visitor(this);
    doc.ParseToc(&visitor);
    EbookTocDest *root = visitor.GetRoot();
    if (root)
        root->OpenSingleNode();
    return root;
}

void EbookController::ScrollToLink(PageDestination *dest)
{
    int reparseIdx = dest->GetDestPageNo() - 1;
    int pageNo = PageForReparsePoint(pages, reparseIdx);
    if (pageNo > 0)
        GoToPage(pageNo, true);
    else if (0 == pageNo)
        GoToLastPage();
}

PageDestination *EbookController::GetNamedDest(const WCHAR *name)
{
    int reparseIdx = -1;
    if (Doc_Mobi == doc.Type() && str::Parse(name, L"%d%$", &reparseIdx) &&
        0 <= reparseIdx && (size_t)reparseIdx <= doc.GetHtmlDataSize()) {
        // Mobi uses filepos (reparseIdx) for in-document links
    }
    else if (!str::FindChar(name, '#')) {
        ScopedMem<WCHAR> id(str::Format(L"#%s", name));
        reparseIdx = ResolvePageAnchor(id);
    }
    else {
        reparseIdx = ResolvePageAnchor(name);
    }
    if (reparseIdx < 0)
        return NULL;
    CrashIf((size_t)reparseIdx > doc.GetHtmlDataSize());
    return new EbookTocDest(NULL, reparseIdx + 1);
}

int EbookController::CurrentTocPageNo() const
{
     return currPageReparseIdx + 1;
}

void EbookController::UpdateDisplayState(DisplayState *ds)
{
    if (!ds->filePath || !str::EqI(ds->filePath, doc.GetFilePath()))
        str::ReplacePtr(&ds->filePath, doc.GetFilePath());

    ds->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;

    // don't modify any of the other DisplayState values
    // as long as they're not used, so that the same
    // DisplayState settings can also be used for EbookEngine;
    // we get reasonable defaults from DisplayState's constructor anyway
    ds->reparseIdx = currPageReparseIdx;
    str::ReplacePtr(&ds->displayMode, prefs::conv::FromDisplayMode(GetDisplayMode()));
}

void EbookController::SetViewPortSize(SizeI size)
{
    // relayouting gets the size from the canvas hwnd
    ctrls->mainWnd->RequestLayout();
}

LRESULT EbookController::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled)
{
    if (!handleMsgs) {
        wasHandled = false;
        return 0;
    }
    return ctrls->mainWnd->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
}

// TODO: also needs to update for font name/size changes, but it's more complicated
// because requires re-layout
void EbookController::UpdateDocumentColors()
{
    SetMainWndBgCol(ctrls);
    // changing background will repaint mainWnd control but changing
    // of text color will not, so we request uncoditional repaint
    // TODO: in PageControl::Paint() use a property for text color, instead of
    // taking it directly from prefs
    ::RequestRepaint(ctrls->mainWnd);
}

void EbookController::RequestRepaint()
{
    ctrls->mainWnd->MarkForRepaint();
}

// cf. DisplayModel.cpp
#define MAX_NAV_HISTORY_LEN 50

void EbookController::AddNavPoint()
{
    int idx = currPageReparseIdx;
    // remove the current and all Forward history entries
    if (navHistoryIx < navHistory.Count())
        navHistory.RemoveAt(navHistoryIx, navHistory.Count() - navHistoryIx);
    // don't add another entry for the exact same position
    if (navHistoryIx > 0 && idx == navHistory.At(navHistoryIx - 1))
        return;
    // make sure that the history doesn't grow overly large
    if (navHistoryIx >= MAX_NAV_HISTORY_LEN) {
        CrashIf(navHistoryIx > MAX_NAV_HISTORY_LEN);
        navHistory.RemoveAt(0, navHistoryIx - MAX_NAV_HISTORY_LEN + 1);
        navHistoryIx = MAX_NAV_HISTORY_LEN - 1;
    }
    // add a new Back history entry
    navHistory.Append(idx);
    navHistoryIx++;
}

bool EbookController::CanNavigate(int dir) const
{
    CrashIf(navHistoryIx > navHistory.Count());
    if (dir < 0)
        return navHistoryIx >= (size_t)-dir;
    return navHistoryIx + dir < navHistory.Count();
}

void EbookController::Navigate(int dir)
{
    if (!CanNavigate(dir))
        return;
    // update the current history entry
    int idx = currPageReparseIdx;
    if (navHistoryIx < navHistory.Count())
        navHistory.At(navHistoryIx) = idx;
    else
        navHistory.Append(idx);
    navHistoryIx += dir;
    idx = navHistory.At(navHistoryIx);
    int pageNo = PageForReparsePoint(pages, idx);
    if (0 == pageNo)
        pageNo = GetMaxPageCount();
    if (pageNo > 0)
        GoToPage(pageNo, false);
}

void EbookController::CopyNavHistory(EbookController& orig)
{
    navHistory = orig.navHistory;
    navHistoryIx = orig.navHistoryIx;
}

EbookController *EbookController::Create(HWND hwnd, ControllerCallback *cb, FrameRateWnd *frameRateWnd)
{
    EbookControls *ctrls = CreateEbookControls(hwnd, frameRateWnd);
    if (!ctrls)
        return NULL;
    return new EbookController(ctrls, cb);
}
