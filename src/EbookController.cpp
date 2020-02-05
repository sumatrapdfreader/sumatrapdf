/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Archive.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "mui/Mui.h"
#include "utils/ThreadUtil.h"
#include "utils/Timer.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "HtmlFormatter.h"
#include "Doc.h"

#include "SettingsStructs.h"
#include "Controller.h"
#include "EbookController.h"
#include "GlobalPrefs.h"

#include "EbookControls.h"
#include "Translations.h"

static const WCHAR* GetFontName() {
    // TODO: validate the name?
    return gGlobalPrefs->ebookUI.fontName;
}

static float GetFontSize() {
    float fontSize = gGlobalPrefs->ebookUI.fontSize;
    if (fontSize < 7.f || fontSize > 32.f) {
        fontSize = 12.5;
    }
    return fontSize;
}

HtmlFormatterArgs* CreateFormatterArgsDoc(Doc doc, int dx, int dy, Allocator* textAllocator) {
    HtmlFormatterArgs* args = CreateFormatterDefaultArgs(dx, dy, textAllocator);
    args->htmlStr = doc.GetHtmlData();
    args->SetFontName(GetFontName());
    args->fontSize = GetFontSize();
    return args;
}

static TocItem* newEbookTocDest(TocItem* parent, const WCHAR* title, int reparseIdx) {
    auto res = new TocItem(parent, title, reparseIdx);
    res->dest = new PageDestination();
    res->dest->kind = kindDestinationScrollTo;
    res->dest->pageNo = reparseIdx;
    return res;
}

static TocItem* newEbookTocDest(TocItem* parent, const WCHAR* title, const WCHAR* url) {
    auto res = new TocItem(parent, title, 0);
    res->dest = new PageDestination();
    res->dest->kind = kindDestinationLaunchURL;
    res->dest->value = str::Dup(url);
    return res;
}

struct EbookFormattingData {
    enum { MAX_PAGES = 256 };
    HtmlPage* pages[MAX_PAGES];
    size_t pageCount;
    bool finished;
    LONG threadNo;

    EbookFormattingData(HtmlPage** pages, size_t pageCount, bool finished, LONG threadNo)
        : pageCount(pageCount), finished(finished), threadNo(threadNo) {
        CrashIf(pageCount > MAX_PAGES);
        memcpy(this->pages, pages, pageCount * sizeof(*pages));
    }

    void DeletePages() {
        for (size_t i = 0; i < this->pageCount; i++) {
            delete this->pages[i];
        }
    }
};

class EbookFormattingThread : public ThreadBase {
    HtmlFormatterArgs* formatterArgs = nullptr; // we own it

    Doc doc;
    EbookController* controller = nullptr;
    ControllerCallback* cb = nullptr;

    // state used during layout process
    HtmlPage* pages[EbookFormattingData::MAX_PAGES];
    int pageCount = 0;

    // we want to send 2 pages after reparseIdx as soon as we have them,
    // so that we can show them to the user as quickly as possible
    // We want 2 to accomodate possible 2 page view
    int reparseIdx = 0;
    int pagesAfterReparseIdx = 0;

  public:
    void SendPagesIfNecessary(bool force, bool finished);
    bool Format();

    EbookFormattingThread(Doc doc, HtmlFormatterArgs* args, EbookController* ctrl, int reparseIdx,
                          ControllerCallback* cb);
    virtual ~EbookFormattingThread();

    // ThreadBase
    virtual void Run();
};

EbookFormattingThread::EbookFormattingThread(Doc doc, HtmlFormatterArgs* args, EbookController* ctrl, int reparseIdx,
                                             ControllerCallback* cb) {
    this->doc = doc;
    this->cb = cb;
    this->controller = ctrl;
    this->formatterArgs = args;
    this->reparseIdx = reparseIdx;
    CrashIf(reparseIdx < 0);
    AssertCrash(doc.IsDocLoaded() || (doc.IsNone() && (nullptr != args->htmlStr)));
}

EbookFormattingThread::~EbookFormattingThread() {
    // lf("ThreadLayoutEbook::~ThreadLayoutEbook()");
    delete formatterArgs;
}

// send accumulated pages if we filled the buffer or the caller forces us
void EbookFormattingThread::SendPagesIfNecessary(bool force, bool finished) {
    if (finished) {
        force = true;
    }
    if (!force && (pageCount < dimof(pages))) {
        return;
    }
    EbookFormattingData* msg = new EbookFormattingData(pages, pageCount, finished, GetNo());
    // lf("ThreadLayoutEbook::SendPagesIfNecessary() sending %d pages, finished=%d", pageCount, (int)finished);
    pageCount = 0;
    memset(pages, 0, sizeof(pages));
    cb->HandleLayoutedPages(controller, msg);
}

// layout pages from a given reparse point (beginning if nullptr)
// returns true if layout thread was cancelled
bool EbookFormattingThread::Format() {
    // lf("Started laying out ebook, reparseIdx=%d", reparseIdx);
    int totalPageCount = 0;
    formatterArgs->reparseIdx = 0;
    pagesAfterReparseIdx = 0;
    HtmlFormatter* formatter = doc.CreateFormatter(formatterArgs);
    for (HtmlPage* pd = formatter->Next(); pd; pd = formatter->Next()) {
        if (WasCancelRequested()) {
            // lf("layout cancelled");
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
            // lf("EbookFormattingThread::Format: sending pages because pagesAfterReparseIdx == %d",
            // pagesAfterReparseIdx);
        }
        SendPagesIfNecessary(force, false);
        CrashIf(pageCount >= dimof(pages));
    }
    SendPagesIfNecessary(true, true /* finished */);
    delete formatter;
    return false;
}

void EbookFormattingThread::Run() {
    // auto t = TimeGet();
    Format();
    // lf("Formatting time: %.2f ms", t.Stop());
}

static void DeletePages(Vec<HtmlPage*>** toDeletePtr) {
    if (!*toDeletePtr) {
        return;
    }

    DeleteVecMembers(**toDeletePtr);
    delete *toDeletePtr;
    *toDeletePtr = nullptr;
}

EbookController::EbookController(Doc doc, EbookControls* ctrls, ControllerCallback* cb)
    : Controller(cb), pageSize(0, 0) {
    this->doc = doc;
    this->ctrls = ctrls;

    CrashIf(!doc.IsDocLoaded());

    EventMgr* em = ctrls->mainWnd->evtMgr;
    em->EventsForName("next")->Clicked = [=](Control* c, int x, int y) { this->ClickedNext(c, x, y); };
    em->EventsForName("prev")->Clicked = [=](Control* c, int x, int y) { this->ClickedPrev(c, x, y); };
    em->EventsForControl(ctrls->progress)->Clicked = [=](Control* c, int x, int y) { this->ClickedProgress(c, x, y); };
    PageControl* page1 = ctrls->pagesLayout->GetPage1();
    PageControl* page2 = ctrls->pagesLayout->GetPage2();
    em->EventsForControl(page1)->SizeChanged = [=](Control* c, int dx, int dy) { this->SizeChangedPage(c, dx, dy); };
    em->EventsForControl(page2)->SizeChanged = [=](Control* c, int dx, int dy) { this->SizeChangedPage(c, dx, dy); };
    em->EventsForControl(page1)->Clicked = [=](Control* c, int x, int y) { this->ClickedPage1(c, x, y); };
    em->EventsForControl(page2)->Clicked = [=](Control* c, int x, int y) { this->ClickedPage2(c, x, y); };
}

EbookController::~EbookController() {
    StopFormattingThread();
    // we must manually disconnect all events becuase evtMgr is
    // destroyed after EbookController, and EbookController destructor
    // will disconnect slots without deleting them, causing leaks
    // TODO: this seems fragile
    EnableMessageHandling(false);
    CloseCurrentDocument();
    DestroyEbookControls(ctrls);
    delete pageAnchorIds;
    delete pageAnchorIdxs;
    delete tocTree;
}

// stop layout thread (if we're closing a document we'll delete
// the ebook data, so we can't have the thread keep using it)
void EbookController::StopFormattingThread() {
    if (!formattingThread) {
        return;
    }
    formattingThread->RequestCancel();
    bool ok = formattingThread->Join();
    CrashIf(!ok);
    delete formattingThread;
    formattingThread = nullptr;
    formattingThreadNo = -1;
    DeletePages(&incomingPages);
}

void EbookController::CloseCurrentDocument() {
    ctrls->pagesLayout->GetPage1()->SetPage(nullptr);
    ctrls->pagesLayout->GetPage2()->SetPage(nullptr);
    StopFormattingThread();
    DeletePages(&pages);
    doc.Delete();
    pageSize = SizeI(0, 0);
}

// returns page whose content contains reparseIdx
// page is in 1..$pageCount range to match currPageNo
// returns 0 if not found (or maybe on the last page)
// returns -1 if no pages are available
static int PageForReparsePoint(Vec<HtmlPage*>* pages, int reparseIdx) {
    if (!pages || pages->size() == 0) {
        return -1;
    }

    // sometimes reparseIdx of first page is > 0 and the code below
    // doesn't handle that, so do that case first
    if (reparseIdx < pages->at(0)->reparseIdx) {
        return 1;
    }

    for (size_t i = 0; i < pages->size(); i++) {
        HtmlPage* pd = pages->at(i);
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
Vec<HtmlPage*>* EbookController::GetPages() {
    return pages;
}

void EbookController::HandlePagesFromEbookLayout(EbookFormattingData* ft) {
    if (formattingThreadNo != ft->threadNo) {
        // this is a message from cancelled thread, we can disregard
        logf(
            "EbookController::HandlePagesFromEbookLayout() thread msg discarded, curr thread: %d, sending thread: %d\n",
            formattingThreadNo, ft->threadNo);
        DeleteEbookFormattingData(ft);
        return;
    }
    // lf("EbookController::HandlePagesFromEbookLayout() %d pages, ft=0x%x", ft->pageCount, (int)ft);
    if (incomingPages) {
        for (size_t i = 0; i < ft->pageCount; i++) {
            incomingPages->Append(ft->pages[i]);
        }
        int pageNo = PageForReparsePoint(incomingPages, currPageReparseIdx);
        if (pageNo > 0) {
            Vec<HtmlPage*>* toDelete = pages;
            pages = incomingPages;
            incomingPages = nullptr;
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
        // I guess !pages can happen if formatting interrupted quickly
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/1156
        SubmitCrashIf(!pages);
        StopFormattingThread();
    }
    UpdateStatus();
    // don't call DeleteEbookFormattingData since
    // ft->pages are now owned by incomingPages or pages
    delete ft;
}

void EbookController::TriggerLayout() {
    Size s = ctrls->pagesLayout->GetPage1()->GetDrawableSize();
    SizeI size(s.Width, s.Height);
    if (size.IsEmpty()) {
        // we haven't been sized yet
        return;
    }
    // CrashIf(size.dx < 100 || size.dy < 40);
    if (!doc.IsDocLoaded()) {
        return;
    }

    if (pageSize == size) {
        // lf("EbookController::TriggerLayout() - skipping layout because same as last size");
        return;
    }

    logf("EbookController::TriggerLayout(): starting formatting thread\n");
    // lf("(%3d,%3d) EbookController::TriggerLayout",size.dx, size.dy);
    pageSize = size; // set it early to prevent re-doing layout at the same size

    StopFormattingThread();
    CrashIf(incomingPages);
    incomingPages = new Vec<HtmlPage*>(1024);

    HtmlFormatterArgs* args = CreateFormatterArgsDoc(doc, size.dx, size.dy, &textAllocator);
    formattingThread = new EbookFormattingThread(doc, args, this, currPageReparseIdx, cb);
    formattingThreadNo = formattingThread->GetNo();
    formattingThread->Start();
    UpdateStatus();
}

void EbookController::SizeChangedPage(Control* c, int dx, int dy) {
    UNUSED(dx);
    UNUSED(dy);
    CrashIf(!(c == ctrls->pagesLayout->GetPage1() || c == ctrls->pagesLayout->GetPage2()));
    // delay re-layout so that we don't unnecessarily do the
    // work as long as the user is still resizing the window
    // TODO: previously, the delay was 100 while inSizeMove and 600 else
    // (to delay a bit if the user resizes but not when e.g. switching to fullscreen)
    cb->RequestDelayedLayout(200);
}

void EbookController::ClickedNext(Control* c, int x, int y) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    // CrashIf(c != ctrls->next);
    GoToNextPage();
}

void EbookController::ClickedPrev(Control* c, int x, int y) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    // CrashIf(c != ctrls->prev);
    GoToPrevPage();
}

// (x, y) is in the coordinates of c
void EbookController::ClickedProgress(Control* c, int x, int y) {
    UNUSED(x);
    UNUSED(y);
    CrashIf(c != ctrls->progress);
    float perc = ctrls->progress->GetPercAt(x);
    int pageCount = (int)GetPages()->size();
    int newPageNo = IntFromPerc(pageCount, perc) + 1;
    GoToPage(newPageNo, true);
}

void EbookController::OnClickedLink(int pageNo, DrawInstr* link) {
    AutoFreeWstr url(strconv::FromHtmlUtf8(link->str.s, link->str.len));
    if (url::IsAbsolute(url)) {
        // TODO: optimize: create just the destination
        auto dest = newEbookTocDest(nullptr, nullptr, url);
        cb->GotoLink(dest->GetPageDestination());
        delete dest;
        return;
    }

    if (DocType::Epub == doc.Type() && pages && (size_t)pageNo <= pages->size()) {
        // normalize the URL by combining it with the chapter's base path
        for (int j = pageNo; j > 0; j--) {
            HtmlPage* p = pages->at(j - 1);
            // <pagebreak src="..." page_marker /> is usually the second instruction on a page
            for (size_t k = 0; k < std::min((size_t)2, p->instructions.size()); k++) {
                DrawInstr& di = p->instructions.at(k);
                if (DrawInstrType::Anchor == di.type && str::StartsWith(di.str.s + di.str.len, "\" page_marker />")) {
                    AutoFree basePath(str::DupN(di.str.s, di.str.len));
                    AutoFree relPath(ResolveHtmlEntities(link->str.s, link->str.len));
                    AutoFree absPath(NormalizeURL(relPath, basePath));
                    url.Set(strconv::Utf8ToWstr(absPath.get()));
                    j = 0; // done
                    break;
                }
            }
        }
    }

    int idx = ResolvePageAnchor(url);
    if (-1 == idx && str::FindChar(url, '%')) {
        url::DecodeInPlace(url);
        idx = ResolvePageAnchor(url);
    }
    if (idx != -1) {
        // TODO: optimize, create just a destination
        auto dest = newEbookTocDest(nullptr, nullptr, idx);
        cb->GotoLink(dest->GetPageDestination());
        delete dest;
    }
}

void EbookController::ClickedPage1(Control* c, int x, int y) {
    cb->FocusFrame(true);

    CrashIf(c != ctrls->pagesLayout->GetPage1());
    DrawInstr* link = ctrls->pagesLayout->GetPage1()->GetLinkAt(x, y);
    if (link) {
        OnClickedLink(currPageNo, link);
    }
}

void EbookController::ClickedPage2(Control* c, int x, int y) {
    cb->FocusFrame(true);

    CrashIf(!IsDoublePage());
    CrashIf(c != ctrls->pagesLayout->GetPage2());
    DrawInstr* link = ctrls->pagesLayout->GetPage2()->GetLinkAt(x, y);
    if (link) {
        OnClickedLink(currPageNo + 1, link);
    }
}

int EbookController::GetMaxPageCount() const {
    Vec<HtmlPage*>* pagesTmp = pages;
    if (incomingPages) {
        CrashIf(!FormattingInProgress());
        pagesTmp = incomingPages;
    }
    if (!pagesTmp) {
        return 0;
    }
    return (int)pagesTmp->size();
}

// show the status text based on current state
void EbookController::UpdateStatus() {
    int pageCount = GetMaxPageCount();
    if (FormattingInProgress()) {
        AutoFreeWstr s(str::Format(_TR("Formatting the book... %d pages"), pageCount));
        ctrls->status->SetText(s);
        ctrls->progress->SetFilled(0.f);
        return;
    }

    AutoFreeWstr s(str::Format(L"%s %d / %d", _TR("Page:"), currPageNo, pageCount));
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

void EbookController::GoToPage(int pageNo, bool addNavPoint) {
    // we're still formatting, disable page movement
    if (incomingPages) {
        // lf("EbookController::GoToPage(%d): skipping because incomingPages != nullptr", pageNo);
        return;
    }

    // Hopefully prevent crashes like 55175
    if (!pages) {
        // TODO: remove when we figure out why this happens
        logf("EbookController::GoToPage(): pageNo: %d, currentPageNo: %d\n", pageNo, this->currPageNo);
        SubmitCrashIf(!pages);
        return;
    }

    if (addNavPoint) {
        AddNavPoint();
    }

    int pageCount = PageCount();
    int n = IsDoublePage() ? 1 : 0;
    if (pageNo + n > pageCount) {
        pageNo = pageCount - n;
    }
    // if have only 1 page and showing double, we could go below 1
    if (pageNo < 1) {
        pageNo = 1;
    }

    HtmlPage* p = pages->at(pageNo - 1);
    currPageNo = pageNo;
    currPageReparseIdx = p->reparseIdx;
    ctrls->pagesLayout->GetPage1()->SetPage(p);
    if (IsDoublePage() && pages->size() > 1) {
        p = pages->at(pageNo);
        ctrls->pagesLayout->GetPage2()->SetPage(p);
    } else {
        ctrls->pagesLayout->GetPage2()->SetPage(nullptr);
    }
    UpdateStatus();
    // update the ToC selection
    cb->PageNoChanged(this, pageNo);
}

bool EbookController::GoToNextPage() {
    int dist = IsDoublePage() ? 2 : 1;
    if (currPageNo + dist > PageCount()) {
        return false;
    }
    GoToPage(currPageNo + dist, false);
    return true;
}

bool EbookController::GoToPrevPage(bool toBottom) {
    UNUSED(toBottom);
    int dist = IsDoublePage() ? 2 : 1;
    if (currPageNo <= dist) {
        // seen a crash were currPageNo here was 0
        return false;
    }
    GoToPage(currPageNo - dist, false);
    return true;
}

void EbookController::StartLayouting(int startReparseIdxArg, DisplayMode displayMode) {
    auto d = doc.GetHtmlData();
    if ((size_t)startReparseIdxArg >= d.size()) {
        startReparseIdxArg = 0;
    }
    currPageReparseIdx = startReparseIdxArg;
    // displayMode could be any value if alternate UI was used, we have to limit it to
    // either DM_SINGLE_PAGE or DM_FACING
    if (DM_AUTOMATIC == displayMode) {
        displayMode = gGlobalPrefs->defaultDisplayModeEnum;
    }

    EnableMessageHandling(true);
    SetDisplayMode(displayMode);
    TriggerLayout();
    UpdateStatus();
}

bool EbookController::IsDoublePage() const {
    return ctrls->pagesLayout->GetPage2()->IsVisible();
}

static RenderedBitmap* RenderFirstDocPageToBitmap(Doc doc, SizeI pageSize, SizeI bmpSize, int border) {
    PoolAllocator textAllocator;
    auto dx = pageSize.dx - 2 * border;
    auto dy = pageSize.dy - 2 * border;
    HtmlFormatterArgs* args = CreateFormatterArgsDoc(doc, dx, dy, &textAllocator);
    TextRenderMethod renderMethod = args->textRenderMethod;
    HtmlFormatter* formatter = doc.CreateFormatter(args);
    HtmlPage* pd = formatter->Next();
    delete formatter;
    delete args;
    args = nullptr;
    if (!pd) {
        return nullptr;
    }

    Bitmap pageBmp(pageSize.dx, pageSize.dy, PixelFormat24bppRGB);
    Graphics g(&pageBmp);
    Rect r(0, 0, pageSize.dx, pageSize.dy);
    r.Inflate(1, 1);
    SolidBrush br(Color(255, 255, 255));
    g.FillRectangle(&br, r);

    ITextRender* textRender = CreateTextRender(renderMethod, &g, pageSize.dx, pageSize.dy);
    textRender->SetTextBgColor(Color(255, 255, 255));
    DrawHtmlPage(&g, textRender, &pd->instructions, (REAL)border, (REAL)border, false, Color((ARGB)Color::Black));
    delete pd;
    delete textRender;

    Bitmap res(bmpSize.dx, bmpSize.dy, PixelFormat24bppRGB);
    Graphics g2(&res);
    g2.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g2.DrawImage(&pageBmp, Rect(0, 0, bmpSize.dx, bmpSize.dy), 0, 0, pageSize.dx, pageSize.dy, UnitPixel);

    HBITMAP hbmp;
    Status status = res.GetHBITMAP((ARGB)Color::White, &hbmp);
    if (status != Ok) {
        return nullptr;
    }
    return new RenderedBitmap(hbmp, bmpSize);
}

static RenderedBitmap* ThumbFromCoverPage(Doc doc, SizeI size) {
    ImageData* coverImage = doc.GetCoverImage();
    if (!coverImage) {
        return nullptr;
    }
    Bitmap* coverBmp = BitmapFromData(coverImage->data, coverImage->len);
    if (!coverBmp) {
        return nullptr;
    }

    Bitmap res(size.dx, size.dy, PixelFormat24bppRGB);
    float scale = (float)size.dx / (float)coverBmp->GetWidth();
    int fromDy = size.dy;
    if (scale < 1.f) {
        fromDy = (int)((float)coverBmp->GetHeight() * scale);
    }
    Graphics g(&res);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    Status status = g.DrawImage(coverBmp, Rect(0, 0, size.dx, size.dy), 0, 0, coverBmp->GetWidth(), fromDy, UnitPixel);
    if (status != Ok) {
        delete coverBmp;
        return nullptr;
    }
    HBITMAP hbmp;
    status = res.GetHBITMAP((ARGB)Color::White, &hbmp);
    delete coverBmp;
    if (status == Ok) {
        return new RenderedBitmap(hbmp, SizeI(size.dx, size.dy));
    }
    return nullptr;
}

void EbookController::CreateThumbnail(SizeI size, const onBitmapRenderedCb& saveThumbnail) {
    // TODO: create thumbnail asynchronously
    CrashIf(!doc.IsDocLoaded());
    // if there is cover image, we use it to generate thumbnail by scaling
    // image width to thumbnail dx, scaling height proportionally and using
    // as much of it as fits in thumbnail dy
    RenderedBitmap* bmp = ThumbFromCoverPage(doc, size);
    if (!bmp) {
        // no cover image so generate thumbnail from first page
        SizeI pageSize(size.dx * 3, size.dy * 3);
        bmp = RenderFirstDocPageToBitmap(doc, pageSize, size, 10);
    }
    saveThumbnail(bmp);
}

void EbookController::SetDisplayMode(DisplayMode mode, bool keepContinuous) {
    UNUSED(keepContinuous);
    bool newDouble = !IsSingle(mode);
    if (IsDoublePage() == newDouble) {
        return;
    }
    // showing/hiding a control will trigger re-layout which will
    // trigger book re-formatting
    if (newDouble) {
        ctrls->pagesLayout->GetPage2()->Show();
    } else {
        ctrls->pagesLayout->GetPage2()->Hide();
    }
}

void EbookController::ExtractPageAnchors() {
    if (pageAnchorIds || pageAnchorIdxs) {
        CrashIf(!pageAnchorIds || !pageAnchorIdxs);
        return;
    }

    pageAnchorIds = new WStrVec();
    pageAnchorIdxs = new Vec<int>();

    AutoFreeWstr epubPagePath;
    int fb2TitleCount = 0;
    auto data = doc.GetHtmlData();
    HtmlPullParser parser(data.data(), data.size());
    HtmlToken* tok;
    while ((tok = parser.Next()) != nullptr && !tok->IsError()) {
        if (!tok->IsStartTag() && !tok->IsEmptyElementEndTag()) {
            continue;
        }
        AttrInfo* attr = tok->GetAttrByName("id");
        if (!attr && Tag_A == tok->tag && doc.Type() != DocType::Fb2) {
            attr = tok->GetAttrByName("name");
        }
        if (attr) {
            AutoFreeWstr id = strconv::Utf8ToWstr({attr->val, attr->valLen});
            pageAnchorIds->Append(str::Format(L"%s#%s", epubPagePath ? epubPagePath.get() : L"", id.Get()));
            pageAnchorIdxs->Append((int)(tok->GetReparsePoint() - parser.Start()));
        }
        // update EPUB page paths and create an anchor per chapter
        if (Tag_Pagebreak == tok->tag && (attr = tok->GetAttrByName("page_path")) != nullptr &&
            str::StartsWith(attr->val + attr->valLen, "\" page_marker />")) {
            CrashIf(doc.Type() != DocType::Epub);
            epubPagePath.Set(strconv::Utf8ToWstr({attr->val, attr->valLen}));
            pageAnchorIds->Append(str::Dup(epubPagePath));
            pageAnchorIdxs->Append((int)(tok->GetReparsePoint() - parser.Start()));
        }
        // create FB2 title anchors (cf. Fb2Doc::ParseToc)
        if (Tag_Title == tok->tag && tok->IsStartTag() && DocType::Fb2 == doc.Type()) {
            AutoFreeWstr id(str::Format(TEXT(FB2_TOC_ENTRY_MARK) L"%d", ++fb2TitleCount));
            pageAnchorIds->Append(id.StealData());
            pageAnchorIdxs->Append((int)(tok->GetReparsePoint() - parser.Start()));
        }
    }
}

// Mobi uses filepos (reparseIdx) for in-document links
static bool UseMobiReparseIdx(const Doc& doc, const WCHAR* id, size_t& reparseIdx) {
    if (doc.Type() != DocType::Mobi) {
        return false;
    }
    int n;
    bool ok = str::Parse(id, L"%d%$", &n);
    if (!ok || (n < 0)) {
        return false;
    }
    auto d = doc.GetHtmlData();
    reparseIdx = static_cast<size_t>(n);
    return reparseIdx < d.size();
}

int EbookController::ResolvePageAnchor(const WCHAR* id) {
    ExtractPageAnchors();

    size_t mobiReparseIdx = static_cast<size_t>(-1);
    if (UseMobiReparseIdx(doc, id, mobiReparseIdx)) {
        return static_cast<int>(mobiReparseIdx);
    }

    int idx = pageAnchorIds->Find(id);
    if (idx != -1) {
        return pageAnchorIdxs->at(idx);
    }
    if (doc.Type() != DocType::Epub || !str::FindChar(id, '#')) {
        return -1;
    }

    AutoFreeWstr chapterPath(str::DupN(id, str::FindChar(id, '#') - id));
    idx = pageAnchorIds->Find(chapterPath);
    if (idx != -1) {
        return pageAnchorIdxs->at(idx);
    }
    return -1;
}

class EbookTocCollector : public EbookTocVisitor {
    EbookController* ctrl = nullptr;
    TocItem* root = nullptr;
    int idCounter = 0;

  public:
    explicit EbookTocCollector(EbookController* ctrl) {
        this->ctrl = ctrl;
    }

    virtual void Visit(const WCHAR* name, const WCHAR* url, int level) {
        TocItem* item = nullptr;
        // TODO: set parent for newEbookTocDest()
        if (!url) {
            item = newEbookTocDest(nullptr, name, 0);
        } else if (url::IsAbsolute(url)) {
            item = newEbookTocDest(nullptr, name, url);
        } else {
            int idx = ctrl->ResolvePageAnchor(url);
            if (-1 == idx && str::FindChar(url, '%')) {
                AutoFreeWstr decodedUrl(str::Dup(url));
                url::DecodeInPlace(decodedUrl);
                idx = ctrl->ResolvePageAnchor(decodedUrl);
            }
            item = newEbookTocDest(nullptr, name, idx + 1);
        }
        item->id = ++idCounter;
        // find the last child at each level, until finding the parent of the new item
        if (!root) {
            root = item;
        } else {
            TocItem* r2 = root;
            for (level--; level > 0; level--) {
                for (; r2->next; r2 = r2->next) {
                    // no-op
                }
                if (!r2->child) {
                    break;
                }
                r2 = r2->child;
            }
            if (level <= 0) {
                r2->AddSibling(item);
            } else {
                r2->child = item;
            }
        }
    }

    TocItem* GetRoot() {
        return root;
    }
};

TocTree* EbookController::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocCollector visitor(this);
    doc.ParseToc(&visitor);
    TocItem* root = visitor.GetRoot();
    if (!root) {
        return nullptr;
    }
    tocTree = new TocTree(root);
    return tocTree;
}

void EbookController::ScrollToLink(PageDestination* dest) {
    int reparseIdx = dest->GetPageNo() - 1;
    int pageNo = PageForReparsePoint(pages, reparseIdx);
    if (pageNo > 0) {
        GoToPage(pageNo, true);
    } else if (0 == pageNo) {
        GoToLastPage();
    }
}

PageDestination* EbookController::GetNamedDest(const WCHAR* name) {
    int reparseIdx = -1;
    auto d = doc.GetHtmlData();
    if (DocType::Mobi == doc.Type() && str::Parse(name, L"%d%$", &reparseIdx) && 0 <= reparseIdx &&
        (size_t)reparseIdx <= d.size()) {
        // Mobi uses filepos (reparseIdx) for in-document links
    } else if (!str::FindChar(name, '#')) {
        AutoFreeWstr id(str::Format(L"#%s", name));
        reparseIdx = ResolvePageAnchor(id);
    } else {
        reparseIdx = ResolvePageAnchor(name);
    }
    if (reparseIdx < 0) {
        return nullptr;
    }
    CrashIf((size_t)reparseIdx > d.size());
    auto toc = newEbookTocDest(nullptr, nullptr, reparseIdx + 1);
    auto res = toc->GetPageDestination();
    toc->dest = nullptr;
    delete toc;
    return res;
}

int EbookController::CurrentTocPageNo() const {
    return currPageReparseIdx + 1;
}

void EbookController::GetDisplayState(DisplayState* ds) {
    if (!ds->filePath || !str::EqI(ds->filePath, doc.GetFilePath())) {
        str::ReplacePtr(&ds->filePath, doc.GetFilePath());
    }

    ds->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;

    // don't modify any of the other DisplayState values
    // as long as they're not used, so that the same
    // DisplayState settings can also be used for EngineEbook;
    // we get reasonable defaults from DisplayState's constructor anyway
    ds->reparseIdx = currPageReparseIdx;
    str::ReplacePtr(&ds->displayMode, prefs::conv::FromDisplayMode(GetDisplayMode()));
}

void EbookController::SetViewPortSize(SizeI size) {
    UNUSED(size);
    // relayouting gets the size from the canvas hwnd
    ctrls->mainWnd->RequestLayout();
}

LRESULT EbookController::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled) {
    if (!handleMsgs) {
        wasHandled = false;
        return 0;
    }
    return ctrls->mainWnd->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
}

// TODO: also needs to update for font name/size changes, but it's more complicated
// because requires re-layout
void EbookController::UpdateDocumentColors() {
    SetMainWndBgCol(ctrls);
    // changing background will repaint mainWnd control but changing
    // of text color will not, so we request uncoditional repaint
    // TODO: in PageControl::Paint() use a property for text color, instead of
    // taking it directly from prefs
    ::RequestRepaint(ctrls->mainWnd);
}

void EbookController::RequestRepaint() {
    ctrls->mainWnd->MarkForRepaint();
}

// cf. DisplayModel.cpp
#define MAX_NAV_HISTORY_LEN 50

void EbookController::AddNavPoint() {
    int idx = currPageReparseIdx;
    // remove the current and all Forward history entries
    if (navHistoryIdx < navHistory.size()) {
        navHistory.RemoveAt(navHistoryIdx, navHistory.size() - navHistoryIdx);
    }
    // don't add another entry for the exact same position
    if (navHistoryIdx > 0 && idx == navHistory.at(navHistoryIdx - 1)) {
        return;
    }
    // make sure that the history doesn't grow overly large
    if (navHistoryIdx >= MAX_NAV_HISTORY_LEN) {
        CrashIf(navHistoryIdx > MAX_NAV_HISTORY_LEN);
        navHistory.RemoveAt(0, navHistoryIdx - MAX_NAV_HISTORY_LEN + 1);
        navHistoryIdx = MAX_NAV_HISTORY_LEN - 1;
    }
    // add a new Back history entry
    navHistory.Append(idx);
    navHistoryIdx++;
}

bool EbookController::CanNavigate(int dir) const {
    CrashIf(navHistoryIdx > navHistory.size());
    if (dir < 0) {
        return navHistoryIdx >= (size_t)-dir;
    }
    return navHistoryIdx + dir < navHistory.size();
}

void EbookController::Navigate(int dir) {
    if (!CanNavigate(dir)) {
        return;
    }
    // update the current history entry
    int idx = currPageReparseIdx;
    if (navHistoryIdx < navHistory.size()) {
        navHistory.at(navHistoryIdx) = idx;
    } else {
        navHistory.Append(idx);
    }
    navHistoryIdx += dir;
    idx = navHistory.at(navHistoryIdx);
    int pageNo = PageForReparsePoint(pages, idx);
    if (0 == pageNo) {
        pageNo = GetMaxPageCount();
    }
    if (pageNo > 0) {
        GoToPage(pageNo, false);
    }
}

void EbookController::CopyNavHistory(EbookController& orig) {
    navHistory = orig.navHistory;
    navHistoryIdx = orig.navHistoryIdx;
}

EbookController* EbookController::Create(Doc doc, HWND hwnd, ControllerCallback* cb, FrameRateWnd* frameRateWnd) {
    EbookControls* ctrls = CreateEbookControls(hwnd, frameRateWnd);
    if (!ctrls) {
        return nullptr;
    }
    return new EbookController(doc, ctrls, cb);
}

// not a destructor so that EbookFormattingData don't have to be exposed in EbookController.h
// and so that EbookFormattingData::pages aren't always deleted (when ownership has been passed on)
void EbookController::DeleteEbookFormattingData(EbookFormattingData* data) {
    data->DeletePages();
    delete data;
}
