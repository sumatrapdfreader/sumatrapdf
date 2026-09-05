/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// engines which render flowed ebook formats into fixed pages through the EngineBase API
// (pages are mostly layed out the same as for a "B Format" paperback: 5.12" x 7.8")

#include "base/Base.h"
#include "base/Archive.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/HtmlTags.h"
#include "base/Pixmap.h"
#include "gui/Dpi.h"

#include "GumboHelpers.h"
#include "GumboHtmlParser.h"

#include "DocProperties.h"
#include "ImageReader.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "PalmDbReader.h"
#include "EbookDoc.h"
#include "gui/PlatformFont.h"
#include "gui/PlatformText.h"
#include "HtmlFormatter.h"
#include "EbookFormatter.h"

#if OS_WIN
#include "base/ScopedWin.h"
#include "base/GdiPlusUtil.h"
#include "base/Win.h"
#include "base/Zip.h"
#endif

#include "EngineAll.h"

#if OS_WIN
using Gdiplus::ARGB;
using Gdiplus::Bitmap;
using Gdiplus::FontFamily;
using Gdiplus::Graphics;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
#endif

Kind kindEngineEpub = "engineEpub";
Kind kindEngineFb2 = "engineFb2";
Kind kindEngineMobi = "engineMobi";
Kind kindEnginePdb = "enginePdb";
Kind kindEngineChm = "engineChm";
Kind kindEngineHtml = "engineHtml";
Kind kindEngineTxt = "engineTxt";

static Str gDefaultFontName;
static Str gDefaultChmFontName;
static float gDefaultFontSize = 10.f;

static WStr GetDefaultFontName() {
    Str s = gDefaultFontName;
    if (s) {
        return ToWStrTemp(s);
    }
    return WStrL(L"Georgia");
}

static WStr GetDefaultChmFontName() {
    if (gDefaultChmFontName) {
        return ToWStrTemp(gDefaultChmFontName);
    }
    return GetDefaultFontName();
}

static float GetDefaultFontSize() {
    // fonts are scaled at higher DPI settings,
    // undo this here for (mostly) consistent results
    if (gDefaultFontSize == 0) {
        gDefaultFontSize = 10;
    }
    return gDefaultFontSize * 96.0f / (float)DpiGetForHwnd(nullptr);
}

void SetDefaultEbookFont(Str name, float size) {
    // intentionally don't validate the input
    if (str::Eq(name, StrL("default"))) {
        // "default" is used for mupdf engine to indicate
        // we should use the font as given in css
        name = StrL("Georgia");
    }
    gDefaultFontName = str::Dup(GetPermArena(), name);
    // use a somewhat smaller size than in the EbookUI, since fit page/width
    // is likely to be above 100% for the paperback page dimensions
    gDefaultFontSize = size * 0.8f;
}

void SetDefaultChmFont(Str name) {
    gDefaultChmFontName = name ? str::Dup(GetPermArena(), name) : Str();
}

/* common classes for EPUB, FictionBook2, Mobi, PalmDOC, CHM, HTML and TXT engines */

struct PageAnchor {
    DrawInstr* instr;
    int pageNo;

    explicit PageAnchor(DrawInstr* instr = nullptr, int pageNo = -1) : instr(instr), pageNo(pageNo) {}
};

class EbookAbortCookie : public AbortCookie {
  public:
    bool abort = false;
    EbookAbortCookie() {}
    void Abort() override { abort = true; }
    void* GetData() override { return nullptr; }
};

class EngineEbook : public EngineBase {
  public:
    EngineEbook();
    ~EngineEbook() override;

    RectF PageMediabox(int pageNo) override;
    RectF PageContentBox(int pageNo, RenderTarget target = RenderTarget::View) override;

    Pixmap* RenderPage(RenderPageArgs& args) override;

    RectF Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse = false) override;

    Str GetFileData() override;

    bool SaveFileAs(Str dstPath) override;
    PageText ExtractPageText(int pageNo) override;
    // make RenderCache request larger tiles than per default
    bool HasClipOptimizations(int pageNo) override;

    Vec<IPageElement*> GetElements(int pageNo) override;
    IPageElement* GetElementAtPos(int pageNo, PointF pt) override;
    bool HandleLink(IPageDestination* dest, ILinkHandler* linkHandler) override {
        ReportIf(!dest || !linkHandler);
        if (!dest || !linkHandler) {
            return false;
        }
        linkHandler->GotoLink(dest);
        return true;
    }

    IPageDestination* GetNamedDest(Str name) override;
    RenderedBitmap* GetImageForPageElement(IPageElement* el) override;
    Str GetImageDataForPageElement(IPageElement* el) override;

    bool BenchLoadPage(int pageNo) override;

    // resolves a ToC/link url to a destination without laying out a chapter
    // just to build the ToC; chaptered engines (EngineMobi) override this
    virtual IPageDestination* GetNamedDestLazy(Str url) { return GetNamedDest(url); }

  protected:
    Vec<HtmlPage*>* pages = nullptr;
    Vec<PageAnchor> anchors;
    // contains for each page the last anchor indicating
    // a break between two merged documents
    Vec<DrawInstr*> baseAnchors;
    // needed so that memory allocated by ResolveHtmlEntities isn't leaked
    Arena* a = nullptr;
    // protects pages / HtmlPage data shared by rendering, text extraction and
    // link lookup. Recursive: GetHtmlPage2() may lay out a chapter (which
    // itself takes this lock) while a caller already holds it (RenderPage,
    // ExtractPageText, ...)
    RecursiveMutex pagesAccess;
    Str sourceData;
    // page dimensions can vary between filetypes
    RectF pageRect;
    float pageBorder;

#if OS_WIN
    void GetTransform(Matrix& m, float zoom, int rotation);
#endif
    PointF TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse);
    bool ExtractPageAnchors();
    TempStr ExtractFontListTemp();
    void ExtractFontListFromPage(Location loc, Vec<PlatformFont*>& seenFonts, StrVec& fonts);

    virtual IPageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo);

    Vec<DrawInstr>* GetHtmlPage(int pageNo);
    Vec<DrawInstr>* GetHtmlPage(Location loc);
    HtmlPage* GetHtmlPage2(int pageNo);
    // single-chapter default; chaptered engines (EngineMobi) override this
    virtual HtmlPage* GetHtmlPage2(Location loc);
};

static IPageElement* NewEbookLink(Rect rect, IPageDestination* dest, int pageNo = 0) {
    if (!dest) {
        // TODO: this doesn't make sense
        dest = new PageDestination();
        dest->kind = kindDestinationLaunchURL;
        // TODO: not sure about this
        // dest->value = str::Dup(res->value);
        dest->rect = ToRectF(rect);
    }

    auto res = new PageElementDestination(dest);
    res->pageNo = pageNo;
    res->rect = ToRectF(rect);

    return res;
}

static IPageElement* NewImageDataElement(int pageNo, Rect bbox, int imageID) {
    auto res = new PageElementImage();
    res->pageNo = pageNo;
    res->rect = ToRectF(bbox);
    res->imageID = imageID;
    return res;
}

static TocItem* newEbookTocItem(Arena* arena, TocItem* parent, Str title, IPageDestination* dest) {
    auto res = AllocTocItem(arena, title, 0);
    res->parent = parent;
    res->dest = dest;
    if (dest) {
        res->pageNo = PageDestGetPageNo(dest);
        // a lazily-chaptered dest (pageNo == -1) still carries a resolved
        // chapter; keep it so the ToC can navigate before the page is known
        res->loc = dest->loc;
    }
    return res;
}

// GetNamedDestLazy returns a heap dest (page links / callers delete it). Copy
// onto the engine arena for a ToC item, then delete the original.
static IPageDestination* DestToArena(Arena* a, IPageDestination* src) {
    if (!src) {
        return nullptr;
    }
    Kind k = src->GetKind();
    IPageDestination* dst;
    if (k == kindDestinationLaunchURL) {
        dst = New<PageDestinationURL>(a, src->GetValue2());
    } else {
        auto* pd = New<PageDestination>(a);
        pd->kind = k;
        pd->value = str::Dup(src->GetValue2());
        pd->name = str::Dup(src->GetName2());
        dst = pd;
    }
    dst->pageNo = src->pageNo;
    dst->loc = src->loc;
    dst->rect = src->GetRect2();
    dst->zoom = src->GetZoom2();
    delete src;
    return dst;
}

EngineEbook::EngineEbook() {
    pageCount = 0;
    // "B Format" paperback
    pageRect = RectF(0, 0, 5.12f * GetFileDPI(), 7.8f * GetFileDPI());
    pageBorder = 0.4f * GetFileDPI();
    preferredLayout = preferredLayout = PageLayout(PageLayout::Type::Single);
    a = ArenaNew();
}

EngineEbook::~EngineEbook() {
    pagesAccess.Lock();

    if (pages) {
        for (HtmlPage* page : *pages) {
            DeleteVecMembers(page->elements);
        }
        DeleteVecMembers(*pages);
    }
    delete pages;

    pagesAccess.Unlock();
    str::Free(sourceData);
    ArenaDelete(a);
}

RectF EngineEbook::PageMediabox(int) {
    return pageRect;
}

RectF EngineEbook::PageContentBox(int pageNo, RenderTarget) {
    RectF mbox = PageMediabox(pageNo);
    mbox.Inflate(-pageBorder, -pageBorder);
    return mbox;
}

Str EngineEbook::GetFileData() {
    Str fileName = FilePath();
    if (fileName) {
        return file::ReadFile(fileName);
    }
    return str::Dup(sourceData);
}

bool EngineEbook::SaveFileAs(Str dstPath) {
    return SaveFileOrData(FilePath(), sourceData, dstPath);
}

// make RenderCache request larger tiles than per default
bool EngineEbook::HasClipOptimizations(int) {
    return false;
}

bool EngineEbook::BenchLoadPage(int) {
    return true;
}

#if OS_WIN
void EngineEbook::GetTransform(Matrix& m, float zoom, int rotation) {
    GetBaseTransform(m, ToGdipRectF(pageRect), zoom, rotation);
}
#endif

Vec<DrawInstr>* EngineEbook::GetHtmlPage(int pageNo) {
    return GetHtmlPage(LocationFromPageNo(pageNo));
}

Vec<DrawInstr>* EngineEbook::GetHtmlPage(Location loc) {
    HtmlPage* p = GetHtmlPage2(loc);
    return p ? &p->instructions : nullptr;
}

HtmlPage* EngineEbook::GetHtmlPage2(int pageNo) {
    return GetHtmlPage2(LocationFromPageNo(pageNo));
}

// single-chapter engines store everything flat in pages; a chaptered engine
// (EngineMobi) overrides this to index its own per-chapter storage instead
HtmlPage* EngineEbook::GetHtmlPage2(Location loc) {
    ReportIf(!loc.IsValid() || loc.chapter != 1);
    if (!loc.IsValid() || loc.chapter != 1 || !pages || loc.page > len(*pages)) {
        return nullptr;
    }
    return (*pages)[loc.page - 1];
}

bool EngineEbook::ExtractPageAnchors() {
    ScopedRecursiveMutex scope(&pagesAccess);

    DrawInstr* baseAnchor = nullptr;
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
        if (!pageInstrs) {
            return false;
        }

        for (int k = 0; k < len(*pageInstrs); k++) {
            DrawInstr* i = &(*pageInstrs)[k];
            if (DrawInstrType::Anchor != i->type && DrawInstrType::PageMarkerAnchor != i->type) {
                continue;
            }
            VecAppend(anchors, PageAnchor(i, pageNo));
            if (k < 2 && DrawInstrType::PageMarkerAnchor == i->type) {
                baseAnchor = i;
            }
        }
        VecAppend(baseAnchors, baseAnchor);
    }

    ReportIf(len(baseAnchors) != len(*pages));
    return true;
}

PointF EngineEbook::TransformPoint(PointF pt, int pageNo, float zoom, int rotation, bool inverse) {
    ReportIf(zoom <= 0);
    if (zoom <= 0) {
        return pt;
    }
    SizeF page = PageMediabox(pageNo).Size();
    if (inverse) {
        page.dx *= zoom;
        page.dy *= zoom;
        if (rotation % 180 != 0) {
            std::swap(page.dx, page.dy);
        }
        rotation = -rotation;
        zoom = 1.0f / zoom;
    }
    rotation = NormalizeRotation(rotation);
    PointF res = pt;
    if (rotation == 90) {
        res = PointF(page.dy - pt.y, pt.x);
    } else if (rotation == 180) {
        res = PointF(page.dx - pt.x, page.dy - pt.y);
    } else if (rotation == 270) {
        res = PointF(pt.y, page.dx - pt.x);
    }
    res.x *= zoom;
    res.y *= zoom;
    return res;
}

RectF EngineEbook::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    PointF tl = TransformPoint(rect.TL(), pageNo, zoom, rotation, inverse);
    PointF br = TransformPoint(rect.BR(), pageNo, zoom, rotation, inverse);
    RectF res = RectF::FromXY(tl, br);
    if (rotation != 0) {
        res.Inflate(-0.01f, -0.01f);
    }
    return res;
}

Pixmap* EngineEbook::RenderPage(RenderPageArgs& args) {
    auto pageNo = args.pageNo;
    auto zoom = args.zoom;
    auto rotation = args.rotation;
    // prefer loc: pageNo can be stale if a chapter got laid out (and later
    // ones renumbered) between the request and this render
    Location loc = args.loc.IsValid() ? args.loc : LocationFromPageNo(pageNo);

    RectF pageRc = args.pageRect ? *args.pageRect : PageMediabox(pageNo);
    Rect screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    Point screenTL = screen.TL();
    screen.Offset(-screen.x, -screen.y);

#if !OS_WIN
    EbookAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new EbookAbortCookie();
        *args.cookie_out = cookie;
    }
    if (cookie && cookie->abort) {
        return nullptr;
    }
    Pixmap* pixmap = AllocPixmap(screen.dx, screen.dy);
    if (!pixmap) {
        return nullptr;
    }
    for (int y = 0; y < pixmap->height; y++) {
        u8* dst = pixmap->data + (size_t)y * pixmap->stride;
        for (int x = 0; x < pixmap->width; x++) {
            dst[0] = 0xff;
            dst[1] = 0xff;
            dst[2] = 0xff;
            dst[3] = 0xff;
            dst += 4;
        }
    }
    return pixmap;
#else
    HANDLE hMap = nullptr;
    HBITMAP hbmp = CreateMemoryBitmap(screen.Size(), &hMap);
    HDC hDC = CreateCompatibleDC(nullptr);
    DeleteObject(SelectObject(hDC, hbmp));

    Graphics g(hDC);
    InitGraphicsMode(&g);

    Gdiplus::Color white(0xFF, 0xFF, 0xFF);
    SolidBrush tmpBrush(white);
    Gdiplus::Rect screenR(ToGdipRect(screen));
    screenR.Inflate(1, 1);
    g.FillRectangle(&tmpBrush, screenR);

    Matrix m;
    GetTransform(m, zoom, rotation);
    m.Translate((float)-screenTL.x, (float)-screenTL.y, MatrixOrderAppend);
    g.SetTransform(&m);

    EbookAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new EbookAbortCookie();
        *args.cookie_out = cookie;
    }

    ScopedRecursiveMutex scope(&pagesAccess);

    PlatformTextRender* textDraw = CreateGdiplusTextRender(&g);
    DrawHtmlPage(&g, textDraw, GetHtmlPage(loc), pageBorder, pageBorder, false, kColBlack,
                 cookie ? &cookie->abort : nullptr);
    delete textDraw;
    DeleteDC(hDC);

    if (cookie && cookie->abort) {
        DeleteObject(hbmp);
        CloseHandle(hMap);
        return nullptr;
    }

    return PixmapFromHBITMAP(hbmp, screen.Size(), hMap);
#endif
}

static Rect GetInstrBbox(DrawInstr& instr, float pageBorder) {
    RectF bbox(instr.bbox.x, instr.bbox.y, instr.bbox.dx, instr.bbox.dy);
    bbox.Offset(pageBorder, pageBorder);
    return bbox.Round();
}

PageText EngineEbook::ExtractPageText(int pageNo) {
    const Str lineSep = StrL("\n");
    ScopedRecursiveMutex scope(&pagesAccess);

    AtomicIntInc(&gAllowAllocFailure);
    AutoCall decAllowAlloc(AtomicIntDec, &gAllowAllocFailure);

    str::Builder content;
    Vec<Rect> coords;
    bool insertSpace = false;

    Vec<DrawInstr>* pageInstrs = GetHtmlPage(pageNo);
    for (DrawInstr& i : *pageInstrs) {
        Rect bbox = GetInstrBbox(i, pageBorder);
        bool hasCoords = len(coords) > 0;
        Rect lastCoord = hasCoords ? VecLast(coords) : Rect{};
        switch (i.type) {
            case DrawInstrType::String:
                if (hasCoords && (bbox.x < lastCoord.BR().x || bbox.y > lastCoord.y + (lastCoord.dy * 0.8))) {
                    content.Append(lineSep);
                    VecAppendBlanks(coords, len(lineSep));
                    ReportIf(lineSep && !VecLast(coords).IsEmpty());
                } else if (insertSpace && hasCoords) {
                    int swidth = bbox.x - lastCoord.BR().x;
                    if (swidth > 0) {
                        content.AppendChar(' ');
                        VecAppend(coords, Rect(bbox.x - swidth, bbox.y, swidth, bbox.dy));
                    }
                }
                insertSpace = false;
                {
                    TempStr s = strconv::HtmlUtf8ToStrTemp(i.str);
                    int nCodepoints = Utf8CodepointCount(s);
                    content.Append(s);
                    if (nCodepoints > 0) {
                        double cwidth = 1.0 * bbox.dx / (double)nCodepoints;
                        for (int k = 0; k < nCodepoints; k++) {
                            VecAppend(coords, Rect((int)(bbox.x + ((double)k * cwidth)), bbox.y, (int)cwidth, bbox.dy));
                        }
                    }
                }
                break;
            case DrawInstrType::RtlString:
                if (hasCoords && (bbox.BR().x > lastCoord.x || bbox.y > lastCoord.y + (lastCoord.dy * 0.8))) {
                    content.Append(lineSep);
                    VecAppendBlanks(coords, len(lineSep));
                    ReportIf(lineSep && !VecLast(coords).IsEmpty());
                } else if (insertSpace && hasCoords) {
                    int swidth = lastCoord.x - bbox.BR().x;
                    if (swidth > 0) {
                        content.AppendChar(' ');
                        VecAppend(coords, Rect(bbox.BR().x, bbox.y, swidth, bbox.dy));
                    }
                }
                insertSpace = false;
                {
                    TempStr s = strconv::HtmlUtf8ToStrTemp(i.str);
                    int nCodepoints = Utf8CodepointCount(s);
                    content.Append(s);
                    if (nCodepoints > 0) {
                        double cwidth = 1.0 * bbox.dx / (double)nCodepoints;
                        for (int k = 0; k < nCodepoints; k++) {
                            VecAppend(coords, Rect((int)(bbox.x + ((double)(nCodepoints - k - 1) * cwidth)), bbox.y,
                                                   (int)cwidth, bbox.dy));
                        }
                    }
                }
                break;
            case DrawInstrType::ElasticSpace:
            case DrawInstrType::FixedSpace:
                insertSpace = true;
                break;
        }
    }
    if (len(content) > 0 && !str::EndsWith(ToStr(content), lineSep)) {
        content.Append(lineSep);
        VecAppendBlanks(coords, len(lineSep));
    }
    int nCodepoints = Utf8CodepointCount(ToStr(content));
    ReportIf(len(coords) != nCodepoints);

    PageText res;
    res.len = len(content);
    res.nCodepoints = nCodepoints;
    res.text = content.TakeStr();
    res.coords = VecTake(coords);
    return res;
}

IPageElement* EngineEbook::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    Str linkStr = link->str;
    TempStr url = strconv::HtmlUtf8ToStrTemp(linkStr);
    if (url::IsAbsolute(url)) {
        return NewEbookLink(rect, nullptr, pageNo);
    }

    // out of range for a chaptered engine (EngineMobi never populates
    // baseAnchors: its pagebreak marker doesn't emit a PageMarkerAnchor)
    DrawInstr* baseAnchor = (pageNo >= 1 && pageNo <= len(baseAnchors)) ? baseAnchors[pageNo - 1] : nullptr;
    if (baseAnchor) {
        TempStr basePath = str::DupTemp(baseAnchor->str);
        TempStr relPath = ResolveHtmlEntitiesTemp(linkStr);
        url = NormalizeURLTemp(relPath, basePath);
    }

    // lazy: resolving eagerly would lay out the target chapter just to build
    // this page's link list; ResolveDest() finishes the job on click
    IPageDestination* dest = GetNamedDestLazy(url);
    if (!dest) {
        return nullptr;
    }
    return NewEbookLink(rect, dest, pageNo);
}

Vec<IPageElement*> EngineEbook::GetElements(int pageNo) {
    HtmlPage* pi = GetHtmlPage2(pageNo);
    if (pi->gotElements) {
        return pi->elements;
    }
    pi->gotElements = true;
    Vec<IPageElement*>& els = pi->elements;
    // stable Location for this page, so a later chapter shift can't
    // re-associate these cached elements with the wrong flat pageNo
    Location loc = LocationFromPageNo(pageNo);

    Vec<DrawInstr>* pageInstrs = &pi->instructions;
    int n = len(*pageInstrs);
    for (int idx = 0; idx < n; idx++) {
        DrawInstr& i = (*pageInstrs)[idx];
        if (DrawInstrType::Image == i.type) {
            auto box = GetInstrBbox(i, pageBorder);
            auto el = NewImageDataElement(pageNo, box, idx);
            el->loc = loc;
            VecAppend(els, el);
        } else if (DrawInstrType::LinkStart == i.type && !i.bbox.IsEmpty()) {
            IPageElement* link = CreatePageLink(&i, GetInstrBbox(i, pageBorder), pageNo);
            if (link) {
                link->loc = loc;
                VecAppend(els, link);
            }
        }
    }

    return els;
}

#if OS_WIN
static RenderedBitmap* getImageFromData(Str imageData) {
    HBITMAP hbmp = nullptr;
    Bitmap* bmp = NewGdiplusBitmapFromPixmap(PixmapFromData(imageData));
    if (!bmp || bmp->GetHBITMAP((ARGB)Gdiplus::Color::White, &hbmp) != Ok) {
        delete bmp;
        return nullptr;
    }
    Size size((int)bmp->GetWidth(), (int)bmp->GetHeight());
    delete bmp;
    return new RenderedBitmap(hbmp, size);
}
#endif

RenderedBitmap* EngineEbook::GetImageForPageElement(IPageElement* iel) {
#if !OS_WIN
    (void)iel;
    return nullptr;
#else
    ReportIf(iel->GetKind() != kindPageElementImage);
    PageElementImage* el = (PageElementImage*)iel;
    int idx = el->imageID;
    Vec<DrawInstr>* pageInstrs = el->loc.IsValid() ? GetHtmlPage(el->loc) : GetHtmlPage(el->pageNo);
    auto&& i = (*pageInstrs)[idx];
    ReportIf(i.type != DrawInstrType::Image);
    return getImageFromData(i.GetImage());
#endif
}

Str EngineEbook::GetImageDataForPageElement(IPageElement* iel) {
    if (!iel || iel->GetKind() != kindPageElementImage) {
        return {};
    }
    PageElementImage* el = (PageElementImage*)iel;
    Vec<DrawInstr>* pageInstrs = el->loc.IsValid() ? GetHtmlPage(el->loc) : GetHtmlPage(el->pageNo);
    if (!pageInstrs || el->imageID < 0 || el->imageID >= len(*pageInstrs)) {
        return {};
    }
    auto&& i = (*pageInstrs)[el->imageID];
    if (i.type != DrawInstrType::Image) {
        return {};
    }
    return str::Dup(i.GetImage());
}

// don't delete the result
IPageElement* EngineEbook::GetElementAtPos(int pageNo, PointF pt) {
    auto els = GetElements(pageNo);

    for (auto& el : els) {
        if (el->GetRect().Contains(pt)) {
            return el;
        }
    }
    return nullptr;
}

IPageDestination* EngineEbook::GetNamedDest(Str name) {
    Str id = name;
    Str hash = str::SliceFromChar(name, '#');
    if (hash) {
        id = Str(hash.s + 1, hash.len - 1);
    }

    // if the name consists of both path and ID,
    // try to first skip to the page with the desired
    // path before looking for the ID to allow
    // for the same ID to be reused on different pages
    DrawInstr* baseAnchor = nullptr;
    int basePageNo = 0;
    if (hash && hash.s > name.s) {
        int base_len = (int)(hash.s - name.s - 1);
        for (int i = 0; i < len(baseAnchors); i++) {
            DrawInstr* anchor = baseAnchors[i];
            if (anchor && base_len == anchor->str.len && str::EqNI(name, anchor->str, base_len)) {
                baseAnchor = anchor;
                basePageNo = (int)i + 1;
                break;
            }
        }
    }

    int id_len = id.len;
    for (int i = 0; i < len(anchors); i++) {
        PageAnchor* anchor = &anchors[i];
        if (baseAnchor) {
            if (anchor->instr == baseAnchor) {
                baseAnchor = nullptr;
            }
            continue;
        }
        // note: at least CHM treats URLs as case-independent
        if (id_len == anchor->instr->str.len && str::EqNI(id, anchor->instr->str, id_len)) {
            RectF rect(0, anchor->instr->bbox.y + pageBorder, pageRect.dx, 10);
            rect.Inflate(-pageBorder, 0);
            return NewSimpleDest(anchor->pageNo, rect);
        }
    }

    // don't fail if an ID doesn't exist in a merged document
    if (basePageNo != 0) {
        RectF rect(0, pageBorder, pageRect.dx, 10);
        rect.Inflate(-pageBorder, 0);
        return NewSimpleDest(basePageNo, rect);
    }

    return nullptr;
}

TempStr EngineEbook::ExtractFontListTemp() {
    ScopedRecursiveMutex scope(&pagesAccess);

    Vec<PlatformFont*> seenFonts;
    StrVec fonts;

    // skip chapters that haven't been laid out yet: walking every flat pageNo
    // would force-layout every chapter just to list fonts
    int nChapters = ChapterCount();
    for (int chapter = 1; chapter <= nChapters; chapter++) {
        if (!IsChapterLaidOut(chapter)) {
            continue;
        }
        int nPages = ChapterPageCount(chapter);
        for (int page = 1; page <= nPages; page++) {
            ExtractFontListFromPage({chapter, page}, seenFonts, fonts);
        }
    }
    if (len(fonts) == 0) {
        return {};
    }

    SortNatural(&fonts);
    return JoinTemp(&fonts, StrL("\n"));
}

// collects the fonts used on one page into seenFonts/fonts; shared by
// ExtractFontListTemp's per-chapter loop
void EngineEbook::ExtractFontListFromPage(Location loc, Vec<PlatformFont*>& seenFonts, StrVec& fonts) {
    Vec<DrawInstr>* pageInstrs = GetHtmlPage(loc);
    if (!pageInstrs) {
        return;
    }

    for (DrawInstr& i : *pageInstrs) {
        if (DrawInstrType::SetFont != i.type || VecContains(seenFonts, i.font)) {
            continue;
        }
        VecAppend(seenFonts, i.font);

#if OS_WIN
        PlatformFont* font = i.font;
        FontFamily family;
        if (!font || !font->gdiFont) {
            // TODO: handle gdi
            ReportIf(font && !font->GetHFont());
            continue;
        }
        Status ok = font->gdiFont->GetFamily(&family);
        if (ok != Ok) {
            continue;
        }
        WCHAR fontNameW[LF_FACESIZE];
        ok = family.GetFamilyName(fontNameW);
        if (ok != Ok) {
            continue;
        }
        TempStr fontName = ToUtf8Temp(fontNameW);
        AppendIfNotExists(&fonts, fontName);
#else
        AppendIfNotExists(&fonts, i.font->GetName());
#endif
    }
}

static void AppendTocItem(TocItem*& root, TocItem* item, int level) {
    if (!root) {
        root = item;
        return;
    }
    // find the last child at each level, until finding the parent of the new item
    TocItem* r2 = root;
    while (--level > 0) {
        for (; r2->next; r2 = r2->next) {
            ;
        }
        if (r2->child) {
            r2 = r2->child;
        } else {
            r2->child = item;
            return;
        }
    }
    r2->AddSiblingAtEnd(item);
}

struct EbookTocBuilder : EbookTocVisitor {
    EngineEbook* engine = nullptr;
    TocItem* root = nullptr;
    int idCounter = 0;
    bool isIndex = false;

  public:
    explicit EbookTocBuilder(EngineEbook* engine) { this->engine = engine; }

    void Visit(Str name, Str url, int level) override;

    TocItem* GetRoot() { return root; }
    void SetIsIndex(bool value) { isIndex = value; }
};

void EbookTocBuilder::Visit(Str name, Str url, int level) {
    Arena* arena = engine->arena;
    IPageDestination* dest;
    if (len(url) == 0) {
        dest = nullptr;
    } else if (url::IsAbsolute(url)) {
        dest = NewSimpleDest(arena, 0, RectF(), 0.f, url);
    } else {
        // GetNamedDestLazy(), not GetNamedDest(): building the ToC must not
        // lay out a chapter for every entry it points to
        dest = DestToArena(arena, engine->GetNamedDestLazy(url));
        if (!dest && str::ContainsChar(url, '%')) {
            TempStr decodedUrl = url::DecodeTemp(url);
            dest = DestToArena(arena, engine->GetNamedDestLazy(decodedUrl));
        }
    }

    // TODO: send parent to newEbookTocItem
    TocItem* item = newEbookTocItem(arena, nullptr, name, dest);
    item->id = ++idCounter;
    if (isIndex) {
        item->pageNo = 0;
        level++;
    }
    AppendTocItem(root, item, level);
}

/* EngineBase for handling EPUB documents */

class EngineEpub : public EngineEbook {
  public:
    EngineEpub();
    ~EngineEpub() override;
    EngineBase* Clone() override;

    TempStr GetPropertyTemp(DocProp prop) override {
        if (prop == DocProp::FontList) {
            return ExtractFontListTemp();
        }
        return doc->GetPropertyTemp(prop);
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(Str path);
    static EngineBase* CreateFromData(Str data);

  protected:
    EpubDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(Str fileName);
    bool LoadFromData(Str data);
    bool FinishLoading();
};

EngineEpub::EngineEpub() {
    kind = kindEngineEpub;
    SetDefaultExt(defaultExt, StrL(".epub"));
}

EngineEpub::~EngineEpub() {
    delete doc;
    DestroyTocTree(tocTree);
}

EngineBase* EngineEpub::Clone() {
    if (sourceData) {
        auto res = CreateFromData(sourceData);
        if (!res) {
            log(StrL("EngineEpub::Clone() failed: CreateFromData() failed\n"));
        }
        return res;
    }
    Str path = FilePath();
    if (path) {
        auto res = CreateFromFile(path);
        if (!res) {
            logf("EngineEpub::Clone() failed: CreateFromFile('%s') failed\n", path);
        }
        return res;
    }
    logf("EngineEpub::Clone() failed: no stream or file path\n");
    return nullptr;
}

bool EngineEpub::Load(Str fileName) {
    SetFilePath(fileName);
#if OS_WIN
    if (dir::Exists(fileName)) {
        // load uncompressed documents as recompressed ZIP data
        Str data = ZipDirToData(fileName, true);
        if (len(data) == 0) {
            return false;
        }
        bool ok = LoadFromData(data);
        str::Free(data);
        return ok;
    }
#endif
    doc = EpubDoc::CreateFromFile(fileName);
    return FinishLoading();
}

bool EngineEpub::LoadFromData(Str data) {
    sourceData = str::Dup(data);
    doc = EpubDoc::CreateFromData(data);
    return FinishLoading();
}

bool EngineEpub::FinishLoading() {
    if (!doc) {
        return false;
    }

    HtmlFormatterArgs args{};
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - (2 * pageBorder);
    args.pageDy = (float)pageRect.dy - (2 * pageBorder);
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = a;
    args.textRenderMethod = GetTextRenderMethod();

    pages = EpubFormatter(&args, doc).FormatAllPages(false);

    // must set pageCount before ExtractPageAnchors
    pageCount = len(*pages);
    if (!ExtractPageAnchors()) {
        return false;
    }

    preferredLayout = PageLayout(PageLayout::Type::Book);
    preferredLayout.r2lDeclared = doc->HasReadingDirection();
    if (doc->IsRTL()) {
        preferredLayout.r2l = true;
    }

    return pageCount > 0;
}

TocTree* EngineEpub::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = AllocTocItem(arena, {}, 0);
    realRoot->child = root;
    tocTree = AllocTocTree(arena, realRoot);
    return tocTree;
}

EngineBase* EngineEpub::CreateFromFile(Str path) {
    EngineEpub* engine = new EngineEpub();
    if (!engine->Load(path)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* EngineEpub::CreateFromData(Str data) {
    EngineEpub* engine = new EngineEpub();
    if (!engine->LoadFromData(data)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

/* EngineEbook.cpp */
EngineBase* CreateEngineEpubFromFile(Str fileName) {
    return EngineEpub::CreateFromFile(fileName);
}

EngineBase* CreateEngineEpubFromData(Str data) {
    return EngineEpub::CreateFromData(data);
}

/* EngineBase for handling FictionBook2 documents */

class EngineFb2 : public EngineEbook {
  public:
    EngineFb2() {
        kind = kindEngineFb2;
        SetDefaultExt(defaultExt, StrL(".fb2"));
    }
    ~EngineFb2() override {
        DestroyTocTree(tocTree);
        delete doc;
    }
    EngineBase* Clone() override {
        Str fileName = FilePath();
        if (fileName) {
            return CreateFromFile(fileName);
        }
        if (sourceData) {
            return CreateFromData(sourceData);
        }
        return {};
    }

    TempStr GetPropertyTemp(DocProp prop) override {
        if (prop == DocProp::FontList) {
            return ExtractFontListTemp();
        }
        return doc->GetPropertyTemp(prop);
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(Str path);
    static EngineBase* CreateFromData(Str data);

  protected:
    Fb2Doc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(Str fileName);
    bool LoadFromData(Str data);
    bool FinishLoading();
};

bool EngineFb2::Load(Str fileName) {
    SetFilePath(fileName);
    doc = Fb2Doc::CreateFromFile(fileName);
    return FinishLoading();
}

bool EngineFb2::LoadFromData(Str data) {
    sourceData = str::Dup(data);
    doc = Fb2Doc::CreateFromData(data);
    return FinishLoading();
}

bool EngineFb2::FinishLoading() {
    if (!doc) {
        return false;
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetXmlData();
    args.pageDx = (float)pageRect.dx - (2 * pageBorder);
    args.pageDy = (float)pageRect.dy - (2 * pageBorder);
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = a;
    args.textRenderMethod = GetTextRenderMethod();

    if (doc->IsZipped()) {
        SetDefaultExt(defaultExt, StrL(".fb2z"));
    }

    pages = Fb2Formatter(&args, doc).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = len(*pages);
    if (!ExtractPageAnchors()) {
        return false;
    }
    return pageCount > 0;
}

TocTree* EngineFb2::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = AllocTocItem(arena, {}, 0);
    realRoot->child = root;
    tocTree = AllocTocTree(arena, realRoot);
    return tocTree;
}

EngineBase* EngineFb2::CreateFromFile(Str path) {
    EngineFb2* engine = new EngineFb2();
    if (!engine->Load(path)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* EngineFb2::CreateFromData(Str data) {
    EngineFb2* engine = new EngineFb2();
    if (!engine->LoadFromData(data)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineFb2FromFile(Str fileName) {
    return EngineFb2::CreateFromFile(fileName);
}

EngineBase* CreateEngineFb2FromData(Str data) {
    return EngineFb2::CreateFromData(data);
}

/* EngineBase for handling Mobi documents */

#include "MobiDoc.h"

class EngineMobi : public EngineEbook {
  public:
    EngineMobi() {
        kind = kindEngineMobi;
        SetDefaultExt(defaultExt, StrL(".mobi"));
    }
    ~EngineMobi() override;
    EngineBase* Clone() override {
        Str fileName = FilePath();
        if (fileName) {
            return CreateFromFile(fileName);
        }
        if (sourceData) {
            return CreateFromData(sourceData);
        }
        return {};
    }

    TempStr GetPropertyTemp(DocProp prop) override {
        if (prop == DocProp::FontList) {
            return ExtractFontListTemp();
        }
        return doc->GetPropertyTemp(prop);
    }

    IPageDestination* GetNamedDest(Str name) override;
    IPageDestination* GetNamedDestLazy(Str url) override;
    TocTree* GetToc() override;

    int LayOutChapter(int chapter) override;
    TempStr MakeBookmarkTemp(Location loc) override;
    Location LookupBookmark(Str s) override;
    Location ResolveDest(IPageDestination* dest) override;

    static EngineBase* CreateFromFile(Str path);
    static EngineBase* CreateFromData(Str data);

  protected:
    MobiDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    // byte offsets into doc's html where each chapter starts (chapter 1 is
    // always 0); fewer than 2 entries means the book stays single-chapter
    Vec<int> chapterStart;
    // chapter-major page storage; chapterPages[c-1] is null until
    // LayOutChapter(c) runs
    Vec<Vec<HtmlPage*>*> chapterPages;

    HtmlPage* GetHtmlPage2(Location loc) override;
    int ChapterForFilePos(int filePos);

    bool Load(Str fileName);
    bool LoadFromData(Str data);
    bool FinishLoading();
};

// case-insensitive scan for "<mbp:pagebreak" markers; chapter 1 always
// starts at offset 0, every marker starts a new chapter
static void FindMobiChapterStarts(Str html, Vec<int>& starts) {
    VecAppend(starts, 0);
    Str needle = StrL("<mbp:pagebreak");
    int pos = 0;
    while (pos < len(html)) {
        Str rest(html.s + pos, len(html) - pos);
        int idx = str::IndexOfI(rest, needle);
        if (idx < 0) {
            break;
        }
        int abs = pos + idx;
        if (abs > 0) {
            VecAppend(starts, abs);
        }
        pos = abs + len(needle);
    }
}

EngineMobi::~EngineMobi() {
    DestroyTocTree(tocTree);
    delete doc;
    ScopedRecursiveMutex scope(&pagesAccess);
    for (Vec<HtmlPage*>* v : chapterPages) {
        if (!v) {
            continue;
        }
        for (HtmlPage* page : *v) {
            DeleteVecMembers(page->elements);
        }
        DeleteVecMembers(*v);
        delete v;
    }
}

bool EngineMobi::Load(Str fileName) {
    SetFilePath(fileName);
    doc = MobiDoc::CreateFromFile(fileName);
    return FinishLoading();
}

bool EngineMobi::LoadFromData(Str data) {
    sourceData = str::Dup(data);
    doc = MobiDoc::CreateFromData(data);
    return FinishLoading();
}

// formats only chapter 1 when the book has chapter markers, so opening a
// long, many-chapter mobi (e.g. one page per <mbp:pagebreak>) stays fast;
// other chapters are formatted on demand by LayOutChapter()
bool EngineMobi::FinishLoading() {
    if (!doc || PdbDocType::Mobipocket != doc->GetDocType()) {
        return false;
    }

    Str html = doc->GetHtmlData();
    FindMobiChapterStarts(html, chapterStart);

    if (len(chapterStart) < 2) {
        VecReset(chapterStart);

        HtmlFormatterArgs args;
        args.htmlStr = html;
        args.pageDx = (float)pageRect.dx - (2 * pageBorder);
        args.pageDy = (float)pageRect.dy - (2 * pageBorder);
        args.SetFontName(GetDefaultFontName());
        args.fontSize = GetDefaultFontSize();
        args.textAllocator = a;
        args.textRenderMethod = GetTextRenderMethod();

        VecResize(chapterPages, 1);
        chapterPages[0] = MobiFormatter(&args, doc).FormatAllPages();
        pageCount = len(*chapterPages[0]);
        return pageCount > 0;
    }

    int nCh = len(chapterStart);
    chapters.Init(nCh);
    VecResize(chapterPages, nCh);
    for (int i = 0; i < nCh; i++) {
        chapterPages[i] = nullptr;
    }

    int n1 = LayOutChapter(1);
    SetPageCountFromChapters();
    logf("EngineMobi::FinishLoading: %d chapters, chapter 1 has %d pages\n", nCh, n1);
    return n1 > 0;
}

// formats one chapter's html slice; called lazily (from the render thread
// too) the first time a page in that chapter is needed. pagesAccess is held
// across the whole format, not just the store, so two threads racing for the
// same not-yet-laid-out chapter never both format it (the loser just waits
// and reuses the winner's result); a render thread blocking here is fine, it
// needs the pages anyway.
int EngineMobi::LayOutChapter(int chapter) {
    ReportIf(chapter < 1 || chapter > len(chapterStart));
    if (chapter < 1 || chapter > len(chapterStart)) {
        return 1;
    }
    ScopedRecursiveMutex scope(&pagesAccess);
    if (chapterPages[chapter - 1] != nullptr) {
        return chapters.PageCount(chapter);
    }
    Str html = doc->GetHtmlData();
    int start = chapterStart[chapter - 1];
    int end = (chapter < len(chapterStart)) ? chapterStart[chapter] : len(html);

    HtmlFormatterArgs args;
    args.htmlStr = Str(html.s + start, end - start);
    args.pageDx = (float)pageRect.dx - (2 * pageBorder);
    args.pageDy = (float)pageRect.dy - (2 * pageBorder);
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = a;
    args.textRenderMethod = GetTextRenderMethod();

    // only chapter 1 may show the book's cover image
    MobiCoverImage coverImage = chapter == 1 ? MobiCoverImage::Show : MobiCoverImage::Skip;
    Vec<HtmlPage*>* v = MobiFormatter(&args, doc, coverImage).FormatAllPages();
    for (HtmlPage* p : *v) {
        // reparseIdx from the formatter is relative to the chapter slice
        p->reparseIdx += start;
    }
    if (len(*v) == 0) {
        VecAppend(*v, new HtmlPage(start));
    }
    int n = len(*v);

    chapterPages[chapter - 1] = v;
    chapters.SetPageCount(chapter, n);
    SetPageCountFromChapters();

    logf("EngineMobi::LayOutChapter: chapter %d -> %d pages\n", chapter, n);
    return n;
}

HtmlPage* EngineMobi::GetHtmlPage2(Location loc) {
    if (!loc.IsValid()) {
        return nullptr;
    }
    ScopedRecursiveMutex scope(&pagesAccess);
    if (!IsChapterLaidOut(loc.chapter)) {
        LayOutChapter(loc.chapter);
    }
    if (loc.chapter < 1 || loc.chapter > len(chapterPages)) {
        return nullptr;
    }
    Vec<HtmlPage*>* v = chapterPages[loc.chapter - 1];
    if (!v || loc.page < 1 || loc.page > len(*v)) {
        return nullptr;
    }
    return (*v)[loc.page - 1];
}

// largest chapter (1-based) whose start offset is <= filePos; pure lookup,
// never lays out a chapter
int EngineMobi::ChapterForFilePos(int filePos) {
    int lo = 0, hi = len(chapterStart) - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (chapterStart[mid] <= filePos) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo + 1;
}

// page (1-based, within its chapter) whose reparseIdx window contains
// filePos: the last page whose own reparseIdx is <= filePos. Shared by
// GetNamedDest() (filePos -> dest) and LookupBookmark() (saved reparseIdx ->
// Location after re-pagination)
static int PageForFilePosInChapter(Vec<HtmlPage*>* v, int filePos) {
    int page = 1;
    for (int i = 0; i < len(*v); i++) {
        if ((*v)[i]->reparseIdx > filePos) {
            break;
        }
        page = i + 1;
    }
    return page;
}

IPageDestination* EngineMobi::GetNamedDest(Str name) {
    int filePos = ParseInt(name);
    if (filePos < 0 || (0 == filePos && (!name.s || name.s[0] != '0'))) {
        return nullptr;
    }
    Str htmlData = doc->GetHtmlData();
    if (filePos > len(htmlData)) {
        return nullptr;
    }

    ScopedRecursiveMutex scope(&pagesAccess);
    int chapter = HasChapters() ? ChapterForFilePos(filePos) : 1;
    ChapterPageCount(chapter); // lay it out if needed
    ReportIf(chapter < 1 || chapter > len(chapterPages) || !chapterPages[chapter - 1]);
    Vec<HtmlPage*>* v = chapterPages[chapter - 1];

    int pageInChapter = PageForFilePosInChapter(v, filePos);

    Location loc{chapter, pageInChapter};
    int pageNo = PageNoFromLocation(loc);
    Vec<DrawInstr>* pageInstrs = &(*v)[pageInChapter - 1]->instructions;
    // link to the bottom of the page, if filePos points
    // beyond the last visible DrawInstr of a page
    float currY = (float)pageRect.dy;
    for (DrawInstr& i : *pageInstrs) {
        if ((DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type) && i.str.s >= htmlData.s &&
            i.str.s <= htmlData.s + len(htmlData) && i.str.s - htmlData.s >= filePos) {
            currY = i.bbox.y;
            break;
        }
    }
    RectF rect(0, currY + pageBorder, pageRect.dx, 10);
    rect.Inflate(-pageBorder, 0);
    auto* dest = NewSimpleDest(pageNo, rect);
    dest->loc = loc;
    return dest;
}

// cheap: just the chapter, no formatting; the page is resolved on click by
// ResolveDest() via GetNamedDest()
IPageDestination* EngineMobi::GetNamedDestLazy(Str url) {
    if (!HasChapters()) {
        return GetNamedDest(url);
    }
    int filePos = ParseInt(url);
    if (filePos < 0 || (0 == filePos && (!url.s || url.s[0] != '0'))) {
        return nullptr;
    }
    auto* dest = new PageDestination();
    dest->kind = kindDestinationScrollTo;
    dest->pageNo = -1;
    dest->loc = {ChapterForFilePos(filePos), 0};
    dest->name = str::Dup(url);
    return dest;
}

Location EngineMobi::ResolveDest(IPageDestination* dest) {
    if (!dest) {
        return kInvalidLocation;
    }
    if (dest->loc.IsValid()) {
        return dest->loc;
    }
    Str filePos = dest->loc.chapter >= 1 ? dest->GetName2() : Str{};
    if (len(filePos) == 0) {
        return EngineBase::ResolveDest(dest);
    }
    IPageDestination* resolved = GetNamedDest(filePos);
    if (!resolved) {
        return kInvalidLocation;
    }
    dest->loc = resolved->loc;
    dest->pageNo = resolved->pageNo;
    dest->rect = resolved->GetRect2();
    delete resolved;
    return dest->loc;
}

TempStr EngineMobi::MakeBookmarkTemp(Location loc) {
    if (!HasChapters()) {
        return EngineBase::MakeBookmarkTemp(loc);
    }
    int n = ChapterPageCount(loc.chapter);
    HtmlPage* p = GetHtmlPage2(loc);
    int reparseIdx = p ? p->reparseIdx : chapterStart[loc.chapter - 1];
    return fmt("%d:%d:%d:r%d", loc.chapter, loc.page, n, reparseIdx);
}

Location EngineMobi::LookupBookmark(Str s) {
    if (!HasChapters()) {
        return EngineBase::LookupBookmark(s);
    }
    int chapter = 0, page = 0, savedCount = 0, reparseIdx = -1;
    Str end = str::Parse(s, "%d:%d:%d:r%d%$", &chapter, &page, &savedCount, &reparseIdx);
    if (str::IsNull(end) || reparseIdx < 0) {
        return EngineBase::LookupBookmark(s);
    }

    int ch = ChapterForFilePos(reparseIdx);
    ChapterPageCount(ch);
    ScopedRecursiveMutex scope(&pagesAccess);
    Vec<HtmlPage*>* v = chapterPages[ch - 1];
    int pg = PageForFilePosInChapter(v, reparseIdx);
    return ClampLocation({ch, pg});
}

TocTree* EngineMobi::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = AllocTocItem(arena, {}, 0);
    realRoot->child = root;
    tocTree = AllocTocTree(arena, realRoot);
    return tocTree;
}

EngineBase* EngineMobi::CreateFromFile(Str path) {
    EngineMobi* engine = new EngineMobi();
    if (!engine->Load(path)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* EngineMobi::CreateFromData(Str data) {
    EngineMobi* engine = new EngineMobi();
    if (!engine->LoadFromData(data)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineMobiFromFile(Str fileName) {
    return EngineMobi::CreateFromFile(fileName);
}

EngineBase* CreateEngineMobiFromData(Str data) {
    return EngineMobi::CreateFromData(data);
}

/* EngineBase for handling PalmDOC documents (and extensions such as TealDoc) */

class EnginePdb : public EngineEbook {
  public:
    EnginePdb() {
        kind = kindEnginePdb;
        SetDefaultExt(defaultExt, StrL(".pdb"));
    }
    ~EnginePdb() override {
        DestroyTocTree(tocTree);
        delete doc;
    }
    EngineBase* Clone() override {
        Str fileName = FilePath();
        if (len(fileName) == 0) {
            return {};
        }
        return CreateFromFile(fileName);
    }

    TempStr GetPropertyTemp(DocProp prop) override {
        if (prop == DocProp::FontList) {
            return ExtractFontListTemp();
        }
        return doc->GetPropertyTemp(prop);
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(Str path);

  protected:
    PalmDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(Str fileName);
};

bool EnginePdb::Load(Str fileName) {
    SetFilePath(fileName);

    doc = PalmDoc::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - (2 * pageBorder);
    args.pageDy = (float)pageRect.dy - (2 * pageBorder);
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = a;
    args.textRenderMethod = GetTextRenderMethod();

    pages = HtmlFormatter(&args).FormatAllPages();
    // must set pageCount before ExtractPageAnchors
    pageCount = len(*pages);
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

TocTree* EnginePdb::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    auto* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = AllocTocItem(arena, {}, 0);
    realRoot->child = root;
    tocTree = AllocTocTree(arena, realRoot);
    return tocTree;
}

EngineBase* EnginePdb::CreateFromFile(Str path) {
    EnginePdb* engine = new EnginePdb();
    if (!engine->Load(path)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEnginePdbFromFile(Str fileName) {
    return EnginePdb::CreateFromFile(fileName);
}

/* formatting extensions for CHM */

#include "ChmFile.h"

class ChmDataCache {
    ChmFile* doc = nullptr; // owned by creator
    Str html;
    Vec<ImageData> images;

  public:
    ChmDataCache(ChmFile* doc, Str html) : doc(doc), html(html.s) {}

    ~ChmDataCache() {
        for (auto&& img : images) {
            str::Free(img.base);
            str::Free(img.fileName);
        }
        str::Free(html);
    }

    Str GetHtmlData() { return html; }

    Str GetImageData(Str id, Str pagePath) {
        if (len(id) == 0 || len(pagePath) == 0) {
            return {};
        }
        TempStr url = NormalizeURLTemp(id, pagePath);
        for (int i = 0; i < len(images); i++) {
            if (str::Eq(images[i].fileName, url)) {
                return images[i].base;
            }
        }

        TempStr tmp = doc->GetDataTemp(url);
        if (len(tmp) == 0) {
            return {};
        }

        ImageData data;
        data.base = str::Dup(tmp);

        data.fileName = str::Dup(url);
        VecAppend(images, data);
        return VecLast(images).base;
    }

    TempStr GetFileData(Str relPath, Str pagePath) {
        if (len(relPath) == 0 || len(pagePath) == 0) {
            return {};
        }
        TempStr url = NormalizeURLTemp(relPath, pagePath);
        return doc->GetDataTemp(url);
    }
};

struct ChmFormatter : HtmlFormatter {
  protected:
    void HandleTagImg(HtmlToken* t) override;
    void HandleTagPagebreak(HtmlToken* t) override;
    void HandleTagLink(HtmlToken* t) override;

    ChmDataCache* chmDoc = nullptr;
    Str pagePath;

  public:
    ChmFormatter(HtmlFormatterArgs* args, ChmDataCache* doc) : HtmlFormatter(args), chmDoc(doc) {}
    ~ChmFormatter() override { str::Free(pagePath); }
};

void ChmFormatter::HandleTagImg(HtmlToken* t) {
    ReportIf(!chmDoc);
    if (t->IsEndTag()) {
        return;
    }
    bool needAlt = true;
    AttrInfo* attr = t->GetAttrByName(StrL("src"));
    if (attr) {
        TempStr src = url::DecodeTemp(attr->val);
        Str img = chmDoc->GetImageData(src, pagePath);
        needAlt = len(img) == 0 || !EmitImage(img);
    }
    if (needAlt) {
        attr = t->GetAttrByName(StrL("alt"));
        if (attr != nullptr) {
            HandleText(str::Dup(textAllocator, attr->val));
        }
    }
}

void ChmFormatter::HandleTagPagebreak(HtmlToken* t) {
    AttrInfo* attr = t->GetAttrByName(StrL("page_path"));
    if (!attr || pagePath) {
        ForceNewPage();
    }
    if (attr) {
        RectF bbox(0, currY, pageDx, 0);
        // attr->val is owned by the gumbo parse tree which doesn't outlive
        // the formatter, so copy it into textAllocator
        VecAppend(currPage->instructions, DrawInstr::PageMarkerAnchor(str::Dup(textAllocator, attr->val), bbox));
        str::ReplaceWithCopy(&pagePath, attr->val);
        // reset CSS style rules for the new document
        VecReset(styleRules);
    }
}

void ChmFormatter::HandleTagLink(HtmlToken* t) {
    ReportIf(!chmDoc);
    if (t->IsEndTag()) {
        return;
    }
    AttrInfo* attr = t->GetAttrByName(StrL("rel"));
    if (!attr || !attr->ValIs(StrL("stylesheet"))) {
        return;
    }
    attr = t->GetAttrByName(StrL("type"));
    if (attr && !attr->ValIs(StrL("text/css"))) {
        return;
    }
    attr = t->GetAttrByName(StrL("href"));
    if (!attr) {
        return;
    }

    TempStr src = url::DecodeTemp(attr->val);
    TempStr data = chmDoc->GetFileData(src, pagePath);
    if ((u8*)data.s) {
        ParseStyleSheet(data);
    }
}

/* EngineBase for handling CHM documents */

class EngineChm : public EngineEbook {
  public:
    EngineChm() {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        kind = kindEngineChm;
        SetDefaultExt(defaultExt, StrL(".chm"));
    }
    ~EngineChm() override {
        delete dataCache;
        delete doc;
        DestroyTocTree(tocTree);
    }
    EngineBase* Clone() override {
        Str fileName = FilePath();
        if (len(fileName) == 0) {
            return {};
        }
        return CreateFromFile(fileName);
    }

    TempStr GetPropertyTemp(DocProp prop) override {
        if (prop == DocProp::FontList) {
            return ExtractFontListTemp();
        }
        return doc->GetPropertyTemp(prop);
    }

    IPageDestination* GetNamedDest(Str name) override;
    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(Str path);

  protected:
    ChmFile* doc = nullptr;
    ChmDataCache* dataCache = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(Str fileName);

    IPageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo) override;
};

static uint CharsetNameToCodepage(Str charset) {
    static struct {
        Str name;
        uint codepage;
    } codepages[] = {
        {StrL("ISO-8859-1"), 1252},  {StrL("Latin1"), 1252},   {StrL("CP1252"), 1252},   {StrL("Windows-1252"), 1252},
        {StrL("ISO-8859-2"), 28592}, {StrL("Latin2"), 28592},  {StrL("CP1251"), 1251},   {StrL("Windows-1251"), 1251},
        {StrL("KOI8-R"), 20866},     {StrL("shift-jis"), 932}, {StrL("x-euc"), 932},     {StrL("euc-kr"), 949},
        {StrL("Big5"), 950},         {StrL("GB2312"), 936},    {StrL("UTF-8"), CP_UTF8},
    };
    for (int i = 0; i < dimofi(codepages); i++) {
        if (str::EqI(charset, codepages[i].name)) {
            return codepages[i].codepage;
        }
    }
    return 0;
}

static uint HttpCharsetFromMetaNode(const GumboNode* node) {
    if (node->type != GUMBO_NODE_ELEMENT || !GumboTagNameIs(node, StrL("meta"))) {
        return 0;
    }
    const GumboAttribute* httpEquiv = gumbo_get_attribute(&node->v.element.attributes, "http-equiv");
    if (!httpEquiv || !str::EqI(Str(httpEquiv->value), StrL("Content-Type"))) {
        return 0;
    }
    const GumboAttribute* content = gumbo_get_attribute(&node->v.element.attributes, "content");
    TempStr mimetype, charset;
    if (!content || str::IsNull(str::Parse(Str(content->value), "%S;%_charset=%S", &mimetype, &charset))) {
        return 0;
    }
    return CharsetNameToCodepage(charset);
}

static uint FindHttpCharsetInNode(const GumboNode* node) {
    // iterative pre-order DFS so a deeply nested document can't overflow the stack
    Vec<const GumboNode*> toVisit;
    VecAppend(toVisit, node);
    while (len(toVisit) > 0) {
        const GumboNode* n = VecPop(toVisit);
        if (!n) {
            continue;
        }
        uint cp = HttpCharsetFromMetaNode(n);
        if (cp) {
            return cp;
        }
        const GumboVector* children = nullptr;
        if (n->type == GUMBO_NODE_ELEMENT) {
            children = &n->v.element.children;
        } else if (n->type == GUMBO_NODE_DOCUMENT) {
            children = &n->v.document.children;
        }
        if (children) {
            // push in reverse so children are visited in document order
            for (unsigned int i = children->length; i > 0; i--) {
                VecAppend(toVisit, (const GumboNode*)children->data[i - 1]);
            }
        }
    }
    return 0;
}

// cf. http://www.w3.org/TR/html4/charset.html#h-5.2.2
static uint ExtractHttpCharset(Str html) {
    if (!str::Contains(html, StrL("charset="))) {
        return 0;
    }
    int parseLen = std::min(html.len, 1024);
    GumboOptions opts = GumboMakeOptions();
    GumboOutput* output = gumbo_parse_with_options(&opts, html.s, (size_t)parseLen);
    if (!output) {
        return 0;
    }
    uint cp = FindHttpCharsetInNode(output->document);
    gumbo_destroy_output_iter(&opts, output);
    return cp;
}

struct ChmHtmlCollector : EbookTocVisitor {
    ChmFile* doc = nullptr;
    StrVec added;
    str::Builder html;

  public:
    explicit ChmHtmlCollector(ChmFile* doc) : doc(doc) {
        // can be big
    }

    TempStr GetHtml() {
        // first add the homepage
        TempStr index = doc->GetHomePath();
        TempWStr urlW = strconv::StrCPToWStrTemp(index, doc->codepage);
        TempStr url = ToUtf8Temp(urlW);
        Visit({}, url, 0);

        // then add all pages linked to from the table of contents
        doc->ParseToc(this);

        // finally add all the remaining HTML files
        StrVec paths;
        doc->GetAllPaths(&paths);
        for (Str path : paths) {
            if (str::EndsWithI(path, StrL(".htm")) || str::EndsWithI(path, StrL(".html"))) {
                if (path.s[0] == '/') {
                    path = Str(path.s + 1, path.len - 1);
                }
                urlW = ToWStrTemp(path);
                url = ToUtf8Temp(urlW);
                Visit({}, url, -1);
            }
        }
        return html.TakeStr();
    }

    void Visit(Str, Str url, int) override {
        if (len(url) == 0 || url::IsAbsolute(url)) {
            return;
        }
        TempStr plainUrl = url::GetFullPathTemp(url);
        if (added.FindI(plainUrl) != -1) {
            return;
        }
        AtomicIntInc(&gAllowAllocFailure);
        AutoCall decAllowAlloc(AtomicIntDec, &gAllowAllocFailure);
        TempStr pageHtml = doc->GetDataTemp(plainUrl);
        if (len(pageHtml) == 0) {
            return;
        }
        html.Append(fmt("<pagebreak page_path=\"%s\" page_marker />", plainUrl));
        uint charset = ExtractHttpCharset(pageHtml);
        if (!charset) {
            charset = doc->codepage;
        }
        TempStr s = SmartToUtf8Temp(pageHtml, charset);
        html.Append(s);
        added.Append(plainUrl);
    }
};

bool EngineChm::Load(Str fileName) {
    SetFilePath(fileName);
    doc = ChmFile::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    TempStr html = ChmHtmlCollector(doc).GetHtml();
    dataCache = new ChmDataCache(doc, html);

    HtmlFormatterArgs args;
    args.htmlStr = dataCache->GetHtmlData();
    args.pageDx = (float)pageRect.dx - (2 * pageBorder);
    args.pageDy = (float)pageRect.dy - (2 * pageBorder);
    args.SetFontName(GetDefaultChmFontName());
    args.overrideFontName = len(gDefaultChmFontName) > 0;
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = a;
    args.textRenderMethod = GetTextRenderMethod();

    pages = ChmFormatter(&args, dataCache).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = len(*pages);
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

IPageDestination* EngineChm::GetNamedDest(Str name) {
    IPageDestination* dest = EngineEbook::GetNamedDest(name);
    if (dest) {
        return dest;
    }
    unsigned int topicID;
    if (!str::IsNull(str::Parse(name, "%u%$", &topicID))) {
        TempStr url = doc->ResolveTopicID(topicID);
        if (url) {
            dest = EngineEbook::GetNamedDest(url);
        }
    }
    return dest;
}

TocTree* EngineChm::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    if (doc->HasIndex()) {
        // TODO: ToC code doesn't work too well for displaying an index,
        //       so this should really become a tree of its own (which
        //       doesn't rely on entries being in the same order as pages)
        builder.Visit(StrL("Index"), {}, 1);
        builder.SetIsIndex(true);
        doc->ParseIndex(&builder);
    }
    TocItem* root = builder.GetRoot();
    if (!root) {
        return nullptr;
    }
    auto realRoot = AllocTocItem(arena, {}, 0);
    realRoot->child = root;
    tocTree = AllocTocTree(arena, realRoot);
    return tocTree;
}

static IPageDestination* newChmEmbeddedDest(Str path) {
    auto res = new PageDestination();
    res->kind = kindDestinationLaunchEmbedded;
    res->value = str::Dup(path::GetBaseNameTemp(path));
    return res;
}

IPageElement* EngineChm::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    IPageElement* linkEl = EngineEbook::CreatePageLink(link, rect, pageNo);
    if (linkEl) {
        return linkEl;
    }

    DrawInstr* baseAnchor = baseAnchors[pageNo - 1];
    TempStr url = NormalizeURLTemp(link->str, baseAnchor->str);
    if (!doc->HasData(url)) {
        return nullptr;
    }

    IPageDestination* dest = newChmEmbeddedDest(url);
    return NewEbookLink(rect, dest, pageNo);
}

EngineBase* EngineChm::CreateFromFile(Str path) {
    EngineChm* engine = new EngineChm();
    if (!engine->Load(path)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineChmFromFile(Str fileName) {
    return EngineChm::CreateFromFile(fileName);
}

/* EngineBase for handling HTML documents */
/* (mainly to allow creating minimal regression test testcases more easily) */

class EngineHtml : public EngineEbook {
  public:
    EngineHtml() {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        SetDefaultExt(defaultExt, StrL(".html"));
    }
    ~EngineHtml() override { delete doc; }
    EngineBase* Clone() override {
        Str fileName = FilePath();
        if (len(fileName) == 0) {
            return {};
        }
        return CreateFromFile(fileName);
    }

    TempStr GetPropertyTemp(DocProp prop) override {
        if (prop == DocProp::FontList) {
            return ExtractFontListTemp();
        }
        return doc->GetPropertyTemp(prop);
    }

    static EngineBase* CreateFromFile(Str path);

  protected:
    HtmlDoc* doc = nullptr;

    bool Load(Str fileName);

    IPageElement* CreatePageLink(DrawInstr* link, Rect rect, int pageNo) override;
};

bool EngineHtml::Load(Str fileName) {
    SetFilePath(fileName);

    doc = HtmlDoc::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - (2 * pageBorder);
    args.pageDy = (float)pageRect.dy - (2 * pageBorder);
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = a;
    args.textRenderMethod = GetTextRenderMethod();

    pages = HtmlFileFormatter(&args, doc).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = len(*pages);
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

static IPageDestination* newRemoteHtmlDest(Str relativeURL) {
    auto* res = new PageDestination();
    Str hash = str::SliceFromChar(relativeURL, '#');
    if (hash) {
        res->value = str::Dup(Str(relativeURL.s, (int)(hash.s - relativeURL.s)));
        res->name = str::Dup(hash);
    } else {
        res->value = str::Dup(relativeURL);
    }
    res->kind = kindDestinationLaunchFile;
    return res;
}

IPageElement* EngineHtml::CreatePageLink(DrawInstr* link, Rect rect, int pageNo) {
    if (len(link->str) == 0) {
        return nullptr;
    }

    TempStr url = strconv::HtmlUtf8ToStrTemp(link->str);
    if (url::IsAbsolute(url) || '#' == url.s[0]) {
        return EngineEbook::CreatePageLink(link, rect, pageNo);
    }

    IPageDestination* dest = newRemoteHtmlDest(url);
    return NewEbookLink(rect, dest, pageNo);
}

EngineBase* EngineHtml::CreateFromFile(Str path) {
    EngineHtml* engine = new EngineHtml();
    if (!engine->Load(path)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineHtmlFromFile(Str fileName) {
    return EngineHtml::CreateFromFile(fileName);
}

/* EngineBase for handling TXT documents */

class EngineTxt : public EngineEbook {
  public:
    EngineTxt() {
        kind = kindEngineTxt;
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectF(0, 0, 8.27f * GetFileDPI(), 11.693f * GetFileDPI());
        SetDefaultExt(defaultExt, StrL(".txt"));
    }
    ~EngineTxt() override {
        DestroyTocTree(tocTree);
        delete doc;
    }
    EngineBase* Clone() override {
        Str fileName = FilePath();
        if (len(fileName) == 0) {
            return {};
        }
        return CreateFromFile(fileName);
    }

    TempStr GetPropertyTemp(DocProp prop) override {
        if (prop == DocProp::FontList) {
            return ExtractFontListTemp();
        }
        return doc->GetPropertyTemp(prop);
    }

    TocTree* GetToc() override;

    static EngineBase* CreateFromFile(Str path);

  protected:
    TxtDoc* doc = nullptr;
    TocTree* tocTree = nullptr;

    bool Load(Str fileName);
};

bool EngineTxt::Load(Str fileName) {
    if (len(fileName) == 0) {
        return false;
    }

    SetFilePath(fileName);

    SetDefaultExt(defaultExt, path::GetExtTemp(fileName));

    doc = TxtDoc::CreateFromFile(fileName);
    if (!doc) {
        return false;
    }

    if (doc->IsRFC()) {
        // RFCs are targeted at letter size pages
        pageRect = RectF(0, 0, 8.5f * GetFileDPI(), 11.f * GetFileDPI());
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetHtmlData();
    args.pageDx = (float)pageRect.dx - (2 * pageBorder);
    args.pageDy = (float)pageRect.dy - (2 * pageBorder);
    args.SetFontName(GetDefaultFontName());
    args.fontSize = GetDefaultFontSize();
    args.textAllocator = a;
    args.textRenderMethod = GetTextRenderMethod();

    pages = TxtFormatter(&args).FormatAllPages(false);
    // must set pageCount before ExtractPageAnchors
    pageCount = len(*pages);
    if (!ExtractPageAnchors()) {
        return false;
    }

    return pageCount > 0;
}

TocTree* EngineTxt::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    auto* root = builder.GetRoot();

    auto realRoot = AllocTocItem(arena, {}, 0);
    realRoot->child = root;
    tocTree = AllocTocTree(arena, realRoot);
    return tocTree;
}

EngineBase* EngineTxt::CreateFromFile(Str path) {
    EngineTxt* engine = new EngineTxt();
    if (!engine->Load(path)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineTxtFromFile(Str fileName) {
    return EngineTxt::CreateFromFile(fileName);
}

void EngineEbookCleanup() {
    gDefaultFontName = {};
    gDefaultChmFontName = {};
}
