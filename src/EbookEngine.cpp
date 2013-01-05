/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// engines which render flowed ebook formats into fixed pages through the BaseEngine API
// (pages are mostly layed out the same as for a "B Format" paperback: 5.12" x 7.8")

#include "BaseUtil.h"
#include "EbookEngine.h"

#include "EbookDoc.h"
#include "EbookFormatter.h"
#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlPullParser.h"
#include "MiniMui.h"
#include "PdbReader.h"
#include "TrivialHtmlParser.h"
#include "ZipUtil.h"

// disable warning C4250 which is wrongly issued due to a compiler bug; cf.
// http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function
#pragma warning(disable: 4250) /* 'class1' : inherits 'class2::member' via dominance */

#define DEFAULT_FONT_NAME L"Georgia"
#define DEFAULT_FONT_SIZE 10

/* common classes for EPUB, FictionBook2, Mobi, PalmDOC, CHM, TCR, HTML and TXT engines */

namespace str {
    namespace conv {

inline WCHAR *FromHtmlUtf8(const char *s, size_t len)
{
    ScopedMem<char> tmp(str::DupN(s, len));
    return DecodeHtmlEntitites(tmp, CP_UTF8);
}

    }
}

inline bool IsExternalUrl(const WCHAR *url)
{
    return str::FindChar(url, ':') != NULL;
}

struct PageAnchor {
    DrawInstr *instr;
    int pageNo;

    PageAnchor(DrawInstr *instr=NULL, int pageNo=-1) : instr(instr), pageNo(pageNo) { }
};

class EbookAbortCookie : public AbortCookie {
public:
    bool abort;
    EbookAbortCookie() : abort(false) { }
    virtual void Abort() { abort = true; }
};

class EbookEngine : public virtual BaseEngine {
public:
    EbookEngine();
    virtual ~EbookEngine();

    virtual const WCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages ? pages->Count() : 0; }

    virtual RectD PageMediabox(int pageNo) { return pageRect; }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        RectD mbox = PageMediabox(pageNo);
        mbox.Inflate(-pageBorder, -pageBorder);
        return mbox;
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=NULL);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View, AbortCookie **cookie_out=NULL);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount) {
        return fileName ? (unsigned char *)file::ReadAll(fileName, cbCount) : NULL;
    }
    virtual bool SaveFileAs(const WCHAR *copyFileName) {
        return fileName ? CopyFile(fileName, copyFileName, FALSE) : false;
    }
    virtual WCHAR * ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View);
    // make RenderCache request larger tiles than per default
    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Book; }

    virtual bool SupportsAnnotation(PageAnnotType type, bool forSaving=false) const;
    virtual void UpdateUserAnnotations(Vec<PageAnnotation> *list);

    virtual Vec<PageElement *> *GetElements(int pageNo);
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt);

    virtual PageDestination *GetNamedDest(const WCHAR *name);

    virtual bool BenchLoadPage(int pageNo) { return true; }

protected:
    WCHAR *fileName;
    Vec<HtmlPage *> *pages;
    Vec<PageAnchor> anchors;
    // contains for each page the last anchor indicating
    // a break between two merged documents
    Vec<DrawInstr *> baseAnchors;
    // needed so that memory allocated by ResolveHtmlEntities isn't leaked
    PoolAllocator allocator;
    // needed since pages::IterStart/IterNext aren't thread-safe
    CRITICAL_SECTION pagesAccess;
    // access to userAnnots is protected by pagesAccess
    Vec<PageAnnotation> userAnnots;
    // needed to undo the DPI specific UnitPoint-UnitPixel conversion
    int currFontDpi;
    // page dimensions can vary between filetypes
    RectD pageRect;
    float pageBorder;

    void GetTransform(Matrix& m, float zoom, int rotation) {
        GetBaseTransform(m, pageRect.ToGdipRectF(), zoom, rotation);
    }
    bool ExtractPageAnchors();
    void FixFontSizeForResolution(HDC hDC);
    virtual PageElement *CreatePageLink(DrawInstr *link, RectI rect, int pageNo);

    Vec<DrawInstr> *GetHtmlPage(int pageNo) {
        CrashIf(pageNo < 1 || PageCount() < pageNo);
        if (pageNo < 1 || PageCount() < pageNo)
            return NULL;
        return &pages->At(pageNo - 1)->instructions;
    }
};

class SimpleDest2 : public PageDestination {
protected:
    int pageNo;
    RectD rect;
    ScopedMem<WCHAR> value;

public:
    SimpleDest2(int pageNo, RectD rect, WCHAR *value=NULL) :
        pageNo(pageNo), rect(rect), value(value) { }

    virtual PageDestType GetDestType() const { return value ? Dest_LaunchURL : Dest_ScrollTo; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return rect; }
    virtual WCHAR *GetDestValue() const { return str::Dup(value); }
};

class EbookLink : public PageElement, public PageDestination {
    PageDestination *dest; // required for internal links, NULL for external ones
    DrawInstr *link; // owned by *EngineImpl::pages
    RectI rect;
    int pageNo;
    bool showUrl;

public:
    EbookLink() : dest(NULL), link(NULL), pageNo(-1), showUrl(false) { }
    EbookLink(DrawInstr *link, RectI rect, PageDestination *dest, int pageNo=-1, bool showUrl=false) :
        link(link), rect(rect), dest(dest), pageNo(pageNo), showUrl(showUrl) { }
    virtual ~EbookLink() { delete dest; }

    virtual PageElementType GetType() const { return Element_Link; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return rect.Convert<double>(); }
    virtual WCHAR *GetValue() const {
        if (!dest || showUrl)
            return str::conv::FromHtmlUtf8(link->str.s, link->str.len);
        return NULL;
    }
    virtual PageDestination *AsLink() { return dest ? dest : this; }

    virtual PageDestType GetDestType() const { return Dest_LaunchURL; }
    virtual int GetDestPageNo() const { return 0; }
    virtual RectD GetDestRect() const { return RectD(); }
    virtual WCHAR *GetDestValue() const { return GetValue(); }
};

class ImageDataElement : public PageElement {
    int pageNo;
    ImageData *id; // owned by *EngineImpl::pages
    RectI bbox;

public:
    ImageDataElement(int pageNo, ImageData *id, RectI bbox) :
        pageNo(pageNo), id(id), bbox(bbox) { }

    virtual PageElementType GetType() const { return Element_Image; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return bbox.Convert<double>(); }
    virtual WCHAR *GetValue() const { return NULL; }

    virtual RenderedBitmap *GetImage() {
        HBITMAP hbmp;
        Bitmap *bmp = BitmapFromData(id->data, id->len);
        if (!bmp || bmp->GetHBITMAP(Color::White, &hbmp) != Ok) {
            delete bmp;
            return NULL;
        }
        SizeI size(bmp->GetWidth(), bmp->GetHeight());
        delete bmp;
        return new RenderedBitmap(hbmp, size);
    }
};

class EbookTocItem : public DocTocItem {
    PageDestination *dest;

public:
    EbookTocItem(WCHAR *title, PageDestination *dest) :
        DocTocItem(title, dest ? dest->GetDestPageNo() : 0), dest(dest) { }
    ~EbookTocItem() { delete dest; }

    virtual PageDestination *GetLink() { return dest; }
};

EbookEngine::EbookEngine() : fileName(NULL), pages(NULL),
    pageRect(0, 0, 5.12 * GetFileDPI(), 7.8 * GetFileDPI()), // "B Format" paperback
    pageBorder(0.4f * GetFileDPI()), currFontDpi(96)
{
    InitializeCriticalSection(&pagesAccess);
}

EbookEngine::~EbookEngine()
{
    EnterCriticalSection(&pagesAccess);

    if (pages)
        DeleteVecMembers(*pages);
    delete pages;
    free(fileName);

    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

bool EbookEngine::ExtractPageAnchors()
{
    ScopedCritSec scope(&pagesAccess);

    DrawInstr *baseAnchor = NULL;
    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        Vec<DrawInstr> *pageInstrs = GetHtmlPage(pageNo);
        if (!pageInstrs)
            return false;

        for (size_t k = 0; k < pageInstrs->Count(); k++) {
            DrawInstr *i = &pageInstrs->At(k);
            if (InstrAnchor != i->type)
                continue;
            anchors.Append(PageAnchor(i, pageNo));
            if (k < 2 && str::StartsWith(i->str.s + i->str.len, "\" page_marker />"))
                baseAnchor = i;
        }
        baseAnchors.Append(baseAnchor);
    }

    CrashIf(baseAnchors.Count() != pages->Count());
    return true;
}

PointD EbookEngine::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotation, inverse);
    return PointD(rect.x, rect.y);
}

RectD EbookEngine::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    RectT<REAL> rcF = rect.Convert<REAL>();
    PointF pts[2] = { PointF(rcF.x, rcF.y), PointF(rcF.x + rcF.dx, rcF.y + rcF.dy) };
    Matrix m;
    GetTransform(m, zoom, rotation);
    if (inverse)
        m.Invert();
    m.TransformPoints(pts, 2);
    return RectD::FromXY(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y);
}

RenderedBitmap *EbookEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool ok = RenderPage(hDCMem, screen, pageNo, zoom, rotation, pageRect, target, cookie_out);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);
    if (!ok) {
        DeleteObject(hbmp);
        return NULL;
    }

    return new RenderedBitmap(hbmp, screen.Size());
}

void EbookEngine::FixFontSizeForResolution(HDC hDC)
{
    int dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    if (dpi == currFontDpi)
        return;

    ScopedCritSec scope(&pagesAccess);

    float dpiFactor = 1.0f * currFontDpi / dpi;
    Graphics g(hDC);
    LOGFONTW lfw;

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        Vec<DrawInstr> *pageInstrs = GetHtmlPage(pageNo);
        for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
            if (InstrSetFont == i->type) {
                Status ok = i->font->GetLogFontW(&g, &lfw);
                if (Ok == ok) {
                    REAL newSize = i->font->GetSize() * dpiFactor;
                    FontStyle newStyle = (FontStyle)i->font->GetStyle();
                    i->font = mui::GetCachedFont(lfw.lfFaceName, newSize, newStyle);
                }
            }
        }
    }
    currFontDpi = dpi;
}

static void DrawAnnotations(Graphics& g, Vec<PageAnnotation>& userAnnots, int pageNo)
{
    for (size_t i = 0; i < userAnnots.Count(); i++) {
        PageAnnotation& annot = userAnnots.At(i);
        if (annot.pageNo != pageNo || annot.type != Annot_Highlight)
            continue;
        g.FillRectangle(&SolidBrush(Color(95, 135, 67, 135)), annot.rect.ToGdipRectF());
    }
}

bool EbookEngine::RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    Color white(0xFF, 0xFF, 0xFF);
    Rect screenR(screenRect.ToGdipRect());
    g.SetClip(screenR);
    screenR.Inflate(1, 1);
    g.FillRectangle(&SolidBrush(white), screenR);

    Matrix m;
    GetTransform(m, zoom, rotation);
    m.Translate((float)(screenRect.x - screen.x), (float)(screenRect.y - screen.y), MatrixOrderAppend);
    g.SetTransform(&m);

    EbookAbortCookie *cookie = NULL;
    if (cookie_out)
        *cookie_out = cookie = new EbookAbortCookie();

    ScopedCritSec scope(&pagesAccess);
    FixFontSizeForResolution(hDC);
    DrawHtmlPage(&g, GetHtmlPage(pageNo), pageBorder, pageBorder, false, &Color(Color::Black), cookie ? &cookie->abort : NULL);
    DrawAnnotations(g, userAnnots, pageNo);
    return !(cookie && cookie->abort);
}

static RectI GetInstrBbox(DrawInstr *instr, float pageBorder)
{
    RectT<float> bbox(instr->bbox.X, instr->bbox.Y, instr->bbox.Width, instr->bbox.Height);
    bbox.Offset(pageBorder, pageBorder);
    return bbox.Round();
}

WCHAR *EbookEngine::ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out, RenderTarget target)
{
    ScopedCritSec scope(&pagesAccess);

    str::Str<WCHAR> content;
    Vec<RectI> coords;
    bool insertSpace = false;

    Vec<DrawInstr> *pageInstrs = GetHtmlPage(pageNo);
    for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
        RectI bbox = GetInstrBbox(i, pageBorder);
        switch (i->type) {
        case InstrString:
            if (coords.Count() > 0 && bbox.x < coords.Last().BR().x) {
                content.Append(lineSep);
                coords.AppendBlanks(str::Len(lineSep));
                CrashIf(*lineSep && !coords.Last().IsEmpty());
            }
            else if (insertSpace && coords.Count() > 0) {
                int swidth = bbox.x - coords.Last().BR().x;
                if (swidth > 0) {
                    content.Append(' ');
                    coords.Append(RectI(bbox.x - swidth, bbox.y, swidth, bbox.dy));
                }
            }
            insertSpace = false;
            {
                ScopedMem<WCHAR> s(str::conv::FromHtmlUtf8(i->str.s, i->str.len));
                content.Append(s);
                size_t len = str::Len(s);
                double cwidth = 1.0 * bbox.dx / len;
                for (size_t k = 0; k < len; k++)
                    coords.Append(RectI((int)(bbox.x + k * cwidth), bbox.y, (int)cwidth, bbox.dy));
            }
            break;
        case InstrRtlString:
            if (coords.Count() > 0 && bbox.BR().x > coords.Last().x) {
                content.Append(lineSep);
                coords.AppendBlanks(str::Len(lineSep));
                CrashIf(*lineSep && !coords.Last().IsEmpty());
            }
            else if (insertSpace && coords.Count() > 0) {
                int swidth = coords.Last().x - bbox.BR().x;
                if (swidth > 0) {
                    content.Append(' ');
                    coords.Append(RectI(bbox.BR().x, bbox.y, swidth, bbox.dy));
                }
            }
            insertSpace = false;
            {
                ScopedMem<WCHAR> s(str::conv::FromHtmlUtf8(i->str.s, i->str.len));
                content.Append(s);
                size_t len = str::Len(s);
                double cwidth = 1.0 * bbox.dx / len;
                for (size_t k = 0; k < len; k++)
                    coords.Append(RectI((int)(bbox.x + (len - k - 1) * cwidth), bbox.y, (int)cwidth, bbox.dy));
            }
            break;
        case InstrElasticSpace:
        case InstrFixedSpace:
            insertSpace = true;
            break;
        }
    }

    if (coords_out) {
        CrashIf(coords.Count() != content.Count());
        *coords_out = new RectI[coords.Count()];
        memcpy(*coords_out, coords.LendData(), coords.Count() * sizeof(RectI));
    }
    return content.StealData();
}

bool EbookEngine::SupportsAnnotation(PageAnnotType type, bool forSaving) const
{
    return !forSaving && Annot_Highlight == type;
}

void EbookEngine::UpdateUserAnnotations(Vec<PageAnnotation> *list)
{
    ScopedCritSec scope(&pagesAccess);
    if (list)
        userAnnots = *list;
    else
        userAnnots.Reset();
}

PageElement *EbookEngine::CreatePageLink(DrawInstr *link, RectI rect, int pageNo)
{
    // internal links don't start with a protocol
    bool isInternal = !memchr(link->str.s, ':', link->str.len);
    if (!isInternal)
        return new EbookLink(link, rect, NULL, pageNo);

    ScopedMem<WCHAR> id;
    DrawInstr *baseAnchor = baseAnchors.At(pageNo-1);
    if (baseAnchor) {
        ScopedMem<char> basePath(str::DupN(baseAnchor->str.s, baseAnchor->str.len));
        ScopedMem<char> url(str::DupN(link->str.s, link->str.len));
        url.Set(NormalizeURL(url, basePath));
        id.Set(str::conv::FromUtf8(url));
    }
    else
        id.Set(str::conv::FromHtmlUtf8(link->str.s, link->str.len));

    PageDestination *dest = GetNamedDest(id);
    if (!dest)
        return NULL;
    return new EbookLink(link, rect, dest, pageNo);
}

Vec<PageElement *> *EbookEngine::GetElements(int pageNo)
{
    Vec<PageElement *> *els = new Vec<PageElement *>();

    Vec<DrawInstr> *pageInstrs = GetHtmlPage(pageNo);
    // CreatePageLink -> GetNamedDest might use pageInstrs->IterStart()
    for (size_t k = 0; k < pageInstrs->Count(); k++) {
        DrawInstr *i = &pageInstrs->At(k);
        if (InstrImage == i->type)
            els->Append(new ImageDataElement(pageNo, &i->img, GetInstrBbox(i, pageBorder)));
        else if (InstrLinkStart == i->type && !i->bbox.IsEmptyArea()) {
            PageElement *link = CreatePageLink(i, GetInstrBbox(i, pageBorder), pageNo);
            if (link)
                els->Append(link);
        }
    }

    return els;
}

PageElement *EbookEngine::GetElementAtPos(int pageNo, PointD pt)
{
    Vec<PageElement *> *els = GetElements(pageNo);
    if (!els)
        return NULL;

    PageElement *el = NULL;
    for (size_t i = 0; i < els->Count() && !el; i++)
        if (els->At(i)->GetRect().Contains(pt))
            el = els->At(i);

    if (el)
        els->Remove(el);
    DeleteVecMembers(*els);
    delete els;

    return el;
}

PageDestination *EbookEngine::GetNamedDest(const WCHAR *name)
{
    ScopedMem<char> name_utf8(str::conv::ToUtf8(name));
    const char *id = name_utf8;
    if (str::FindChar(id, '#'))
        id = str::FindChar(id, '#') + 1;

    // if the name consists of both path and ID,
    // try to first skip to the page with the desired
    // path before looking for the ID to allow
    // for the same ID to be reused on different pages
    DrawInstr *baseAnchor = NULL;
    int basePageNo = 0;
    if (id > name_utf8 + 1) {
        size_t base_len = id - name_utf8 - 1;
        for (size_t i = 0; i < baseAnchors.Count(); i++) {
            DrawInstr *anchor = baseAnchors.At(i);
            if (anchor && base_len == anchor->str.len &&
                str::EqNI(name_utf8, anchor->str.s, base_len)) {
                baseAnchor = anchor;
                basePageNo = (int)i + 1;
                break;
            }
        }
    }

    size_t id_len = str::Len(id);
    for (size_t i = 0; i < anchors.Count(); i++) {
        PageAnchor *anchor = &anchors.At(i);
        if (baseAnchor) {
            if (anchor->instr == baseAnchor)
                baseAnchor = NULL;
            continue;
        }
        // note: at least CHM treats URLs as case-independent
        if (id_len == anchor->instr->str.len &&
            str::EqNI(id, anchor->instr->str.s, id_len)) {
            RectD rect(0, anchor->instr->bbox.Y + pageBorder, pageRect.dx, 10);
            rect.Inflate(-pageBorder, 0);
            return new SimpleDest2(anchor->pageNo, rect);
        }
    }

    // don't fail if an ID doesn't exist in a merged document
    if (basePageNo != 0) {
        RectD rect(0, pageBorder, pageRect.dx, 10);
        rect.Inflate(-pageBorder, 0);
        return new SimpleDest2(basePageNo, rect);
    }

    return NULL;
}

static void AppendTocItem(EbookTocItem *& root, EbookTocItem *item, int level)
{
    if (!root) {
        root = item;
        return;
    }
    // find the last child at each level, until finding the parent of the new item
    DocTocItem *r2 = root;
    while (--level > 0) {
        for (; r2->next; r2 = r2->next);
        if (r2->child)
            r2 = r2->child;
        else {
            r2->child = item;
            return;
        }
    }
    r2->AddSibling(item);
}

class EbookTocBuilder : public EbookTocVisitor {
    BaseEngine *engine;
    EbookTocItem *root;
    int idCounter;
    bool isIndex;

public:
    EbookTocBuilder(BaseEngine *engine) :
        engine(engine), root(NULL), idCounter(0), isIndex(false) { }

    virtual void Visit(const WCHAR *name, const WCHAR *url, int level) {
        PageDestination *dest;
        if (!url)
            dest = NULL;
        else if (IsExternalUrl(url))
            dest = new SimpleDest2(0, RectD(), str::Dup(url));
        else if (str::FindChar(url, '%')) {
            ScopedMem<WCHAR> decodedUrl(str::Dup(url));
            str::UrlDecodeInPlace(decodedUrl);
            dest = engine->GetNamedDest(decodedUrl);
        }
        else
            dest = engine->GetNamedDest(url);

        EbookTocItem *item = new EbookTocItem(str::Dup(name), dest);
        item->id = ++idCounter;
        item->open = level <= 2;
        if (isIndex) {
            item->pageNo = 0;
            item->open = level != 1;
            level++;
        }
        AppendTocItem(root, item, level);
    }

    EbookTocItem *GetRoot() { return root; }
    void SetIsIndex(bool value) { isIndex = value; }
};

/* BaseEngine for handling EPUB documents */

class EpubEngineImpl : public EbookEngine, public EpubEngine {
    friend EpubEngine;

public:
    EpubEngineImpl() : EbookEngine(), doc(NULL) { }
    virtual ~EpubEngineImpl() { delete doc; }
    virtual EpubEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual PageLayoutType PreferredLayout();

    virtual WCHAR *GetProperty(DocumentProperty prop) { return doc->GetProperty(prop); }
    virtual const WCHAR *GetDefaultFileExt() const { return L".epub"; }

    virtual bool HasTocTree() const { return doc->HasToc(); }
    virtual DocTocItem *GetTocTree();

protected:
    EpubDoc *doc;

    bool Load(const WCHAR *fileName);
    bool Load(IStream *stream);
    bool FinishLoading();

    DocTocItem *BuildTocTree(HtmlPullParser& parser, int& idCounter);
};

bool EpubEngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);
    doc = EpubDoc::CreateFromFile(fileName);
    return FinishLoading();
}

bool EpubEngineImpl::Load(IStream *stream)
{
    doc = EpubDoc::CreateFromStream(stream);
    return FinishLoading();
}

bool EpubEngineImpl::FinishLoading()
{
    if (!doc)
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = DEFAULT_FONT_NAME;
    args.fontSize = DEFAULT_FONT_SIZE;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = EpubFormatter(&args, doc).FormatAllPages(false);
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

PageLayoutType EpubEngineImpl::PreferredLayout()
{
    if (doc->IsRTL())
        return (PageLayoutType)(Layout_Book | Layout_R2L);
    return Layout_Book;
}

DocTocItem *EpubEngineImpl::GetTocTree()
{
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    return builder.GetRoot();
}

bool EpubEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return EpubDoc::IsSupportedFile(fileName, sniff);
}

EpubEngine *EpubEngine::CreateFromFile(const WCHAR *fileName)
{
    EpubEngineImpl *engine = new EpubEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

EpubEngine *EpubEngine::CreateFromStream(IStream *stream)
{
    EpubEngineImpl *engine = new EpubEngineImpl();
    if (!engine->Load(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* BaseEngine for handling FictionBook2 documents */

class Fb2EngineImpl : public EbookEngine, public Fb2Engine {
    friend Fb2Engine;

public:
    Fb2EngineImpl() : EbookEngine(), doc(NULL) { }
    virtual ~Fb2EngineImpl() { delete doc; }
    virtual Fb2Engine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual WCHAR *GetProperty(DocumentProperty prop) { return doc->GetProperty(prop); }
    virtual const WCHAR *GetDefaultFileExt() const {
        return doc && doc->IsZipped() ? L".fb2z" : L".fb2";
    }

    virtual bool HasTocTree() const;
    virtual DocTocItem *GetTocTree();

protected:
    Fb2Doc *doc;

    bool Load(const WCHAR *fileName);
};

bool Fb2EngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = Fb2Doc::CreateFromFile(fileName);
    if (!doc)
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = DEFAULT_FONT_NAME;
    args.fontSize = DEFAULT_FONT_SIZE;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = Fb2Formatter(&args, doc).FormatAllPages(false);
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

bool Fb2EngineImpl::HasTocTree() const
{
    CrashIf(str::Len(FB2_TOC_ENTRY_MARK) != 10);
    for (size_t i = 0; i < anchors.Count(); i++) {
        DrawInstr *instr = anchors.At(i).instr;
        if (instr->str.len == 11 && str::EqN(instr->str.s, FB2_TOC_ENTRY_MARK "1", 11))
            return true;
    }
    return false;
}

DocTocItem *Fb2EngineImpl::GetTocTree()
{
    EbookTocItem *root = NULL;
    ScopedMem<WCHAR> itemText;
    int titleCount = 0;
    bool inTitle = false;
    int level = 0;

    size_t xmlLen;
    const char *xmlData = doc->GetTextData(&xmlLen);
    HtmlPullParser parser(xmlData, xmlLen);
    HtmlToken *tok;
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (tok->IsStartTag() && Tag_Section == tok->tag)
            level++;
        else if (tok->IsEndTag() && Tag_Section == tok->tag && level > 0)
            level--;
        else if (tok->IsStartTag() && Tag_Title == tok->tag) {
            inTitle = true;
            titleCount++;
        }
        else if (tok->IsEndTag() && Tag_Title == tok->tag) {
            if (itemText)
                str::NormalizeWS(itemText);
            if (!str::IsEmpty(itemText.Get())) {
                ScopedMem<WCHAR> name(str::Format(TEXT(FB2_TOC_ENTRY_MARK) L"%d", titleCount));
                PageDestination *dest = GetNamedDest(name);
                EbookTocItem *item = new EbookTocItem(itemText.StealData(), dest);
                item->id = titleCount;
                item->open = level <= 2;
                AppendTocItem(root, item, level);
            }
            inTitle = false;
        }
        else if (inTitle && tok->IsText()) {
            ScopedMem<WCHAR> text(str::conv::FromHtmlUtf8(tok->s, tok->sLen));
            if (str::IsEmpty(itemText.Get()))
                itemText.Set(text.StealData());
            else
                itemText.Set(str::Join(itemText, L" ", text));
        }
    }

    return root;
}

bool Fb2Engine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return Fb2Doc::IsSupportedFile(fileName, sniff);
}

Fb2Engine *Fb2Engine::CreateFromFile(const WCHAR *fileName)
{
    Fb2EngineImpl *engine = new Fb2EngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* BaseEngine for handling Mobi documents */

#include "MobiDoc.h"

class MobiEngineImpl : public EbookEngine, public MobiEngine {
    friend MobiEngine;

public:
    MobiEngineImpl() : EbookEngine(), doc(NULL), tocReparsePoint(NULL) { }
    virtual ~MobiEngineImpl() { delete doc; }
    virtual MobiEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual WCHAR *GetProperty(DocumentProperty prop) { return doc->GetProperty(prop); }
    virtual const WCHAR *GetDefaultFileExt() const { return L".mobi"; }

    virtual PageDestination *GetNamedDest(const WCHAR *name);
    virtual bool HasTocTree() const { return tocReparsePoint != NULL; }
    virtual DocTocItem *GetTocTree();

protected:
    MobiDoc *doc;
    const char *tocReparsePoint;
    ScopedMem<char> pdbHtml;

    bool Load(const WCHAR *fileName);
};

bool MobiEngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = MobiDoc::CreateFromFile(fileName);
    if (!doc || Pdb_Mobipocket != doc->GetDocType())
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetBookHtmlData(args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = DEFAULT_FONT_NAME;
    args.fontSize = DEFAULT_FONT_SIZE;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = MobiFormatter(&args, doc).FormatAllPages();
    if (!ExtractPageAnchors())
        return false;

    HtmlParser parser;
    if (parser.Parse(args.htmlStr)) {
        HtmlElement *ref = NULL;
        while ((ref = parser.FindElementByName("reference", ref))) {
            ScopedMem<WCHAR> type(ref->GetAttribute("type"));
            ScopedMem<WCHAR> filepos(ref->GetAttribute("filepos"));
            if (str::EqI(type, L"toc") && filepos) {
                unsigned int pos;
                if (str::Parse(filepos, L"%u%$", &pos) && pos < args.htmlStrLen) {
                    tocReparsePoint = args.htmlStr + pos;
                    break;
                }
            }
        }
    }

    return pages->Count() > 0;
}

PageDestination *MobiEngineImpl::GetNamedDest(const WCHAR *name)
{
    int filePos = _wtoi(name);
    if (filePos < 0 || 0 == filePos && *name != '0')
        return NULL;
    int pageNo;
    for (pageNo = 1; pageNo < PageCount(); pageNo++) {
        if (pages->At(pageNo)->reparseIdx > filePos)
            break;
    }
    CrashIf(pageNo < 1 || pageNo > PageCount());

    size_t htmlLen;
    char *start = doc->GetBookHtmlData(htmlLen);
    if ((size_t)filePos > htmlLen)
        return NULL;

    ScopedCritSec scope(&pagesAccess);
    Vec<DrawInstr> *pageInstrs = GetHtmlPage(pageNo);
    // link to the bottom of the page, if filePos points
    // beyond the last visible DrawInstr of a page
    float currY = (float)pageRect.dy;
    for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
        if ((InstrString == i->type || InstrRtlString == i->type) &&
            i->str.s >= start && i->str.s <= start + htmlLen &&
            i->str.s - start >= filePos) {
            currY = i->bbox.Y;
            break;
        }
    }
    RectD rect(0, currY + pageBorder, pageRect.dx, 10);
    rect.Inflate(-pageBorder, 0);
    return new SimpleDest2(pageNo, rect);
}

DocTocItem *MobiEngineImpl::GetTocTree()
{
    if (!tocReparsePoint)
        return NULL;

    EbookTocItem *root = NULL;
    ScopedMem<WCHAR> itemText;
    ScopedMem<WCHAR> itemLink;
    int itemLevel = 0;
    int idCounter = 0;

    // there doesn't seem to be a standard for Mobi ToCs, so we try to
    // determine the author's intentions by looking at commonly used tags
    HtmlPullParser parser(tocReparsePoint, str::Len(tocReparsePoint));
    HtmlToken *tok;
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (itemLink && tok->IsText()) {
            ScopedMem<WCHAR> linkText(str::conv::FromHtmlUtf8(tok->s, tok->sLen));
            if (itemText)
                itemText.Set(str::Join(itemText, L" ", linkText));
            else
                itemText.Set(linkText.StealData());
        }
        else if (!tok->IsTag())
            continue;
        else if (Tag_Mbp_Pagebreak == tok->tag)
            break;
        else if (!itemLink && tok->IsStartTag() && Tag_A == tok->tag) {
            AttrInfo *attr = tok->GetAttrByName("filepos");
            if (!attr)
                attr = tok->GetAttrByName("href");
            if (attr)
                itemLink.Set(str::conv::FromHtmlUtf8(attr->val, attr->valLen));
        }
        else if (itemLink && tok->IsEndTag() && Tag_A == tok->tag) {
            PageDestination *dest = NULL;
            if (!itemText) {
                itemLink.Set(NULL);
                continue;
            }
            if (IsExternalUrl(itemLink))
                dest = new SimpleDest2(0, RectD(), itemLink.StealData());
            else
                dest = GetNamedDest(itemLink);
            EbookTocItem *item = new EbookTocItem(itemText.StealData(), dest);
            item->id = ++idCounter;
            item->open = itemLevel <= 2;
            AppendTocItem(root, item, itemLevel);
            itemLink.Set(NULL);
        }
        else if (Tag_Blockquote == tok->tag || Tag_Ul == tok->tag || Tag_Ol == tok->tag) {
            if (tok->IsStartTag())
                itemLevel++;
            else if (tok->IsEndTag() && itemLevel > 0)
                itemLevel--;
        }
    }

    return root;
}

bool MobiEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        PdbReader pdbReader(fileName);
        return str::Eq(pdbReader.GetDbType(), "BOOKMOBI");
    }

    return str::EndsWithI(fileName, L".mobi") ||
           str::EndsWithI(fileName, L".prc");
}

MobiEngine *MobiEngine::CreateFromFile(const WCHAR *fileName)
{
    MobiEngineImpl *engine = new MobiEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* BaseEngine for handling PalmDOC documents (and extensions such as TealDoc) */

class PdbEngineImpl : public EbookEngine, public PdbEngine {
    friend PdbEngine;

public:
    PdbEngineImpl() : EbookEngine(), doc(NULL) { }
    virtual ~PdbEngineImpl() { delete doc; }
    virtual PdbEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual WCHAR *GetProperty(DocumentProperty prop) { return NULL; }
    virtual const WCHAR *GetDefaultFileExt() const { return L".pdb"; }

    virtual bool HasTocTree() const { return doc->HasToc(); }
    virtual DocTocItem *GetTocTree();

protected:
    PalmDoc *doc;

    bool Load(const WCHAR *fileName);
};

bool PdbEngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = PalmDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = DEFAULT_FONT_NAME;
    args.fontSize = DEFAULT_FONT_SIZE;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = PdbFormatter(&args, doc).FormatAllPages();
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

DocTocItem *PdbEngineImpl::GetTocTree()
{
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    return builder.GetRoot();
}

bool PdbEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return PalmDoc::IsSupportedFile(fileName, sniff);
}

PdbEngine *PdbEngine::CreateFromFile(const WCHAR *fileName)
{
    PdbEngineImpl *engine = new PdbEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* formatting extensions for CHM */

#include "ChmDoc.h"

class ChmDataCache {
    ChmDoc *doc; // owned by creator
    ScopedMem<char> html;
    Vec<ImageData2> images;

public:
    ChmDataCache(ChmDoc *doc, char *html) : doc(doc), html(html) { }
    ~ChmDataCache() {
        for (size_t i = 0; i < images.Count(); i++) {
            free(images.At(i).base.data);
            free(images.At(i).id);
        }
    }

    const char *GetTextData(size_t *lenOut) {
        *lenOut = html ? str::Len(html) : 0;
        return html;
    }

    ImageData *GetImageData(const char *id, const char *pagePath) {
        ScopedMem<char> url(NormalizeURL(id, pagePath));
        str::UrlDecodeInPlace(url);
        for (size_t i = 0; i < images.Count(); i++) {
            if (str::Eq(images.At(i).id, url))
                return &images.At(i).base;
        }

        ImageData2 data = { 0 };
        data.base.data = (char *)doc->GetData(url, &data.base.len);
        if (!data.base.data)
            return NULL;
        data.id = url.StealData();
        images.Append(data);
        return &images.Last().base;
    }
};

class ChmFormatter : public HtmlFormatter {
protected:
    virtual void HandleTagImg(HtmlToken *t);
    virtual void HandleTagPagebreak(HtmlToken *t);

    ChmDataCache *chmDoc;
    ScopedMem<char> pagePath;

public:
    ChmFormatter(HtmlFormatterArgs *args, ChmDataCache *doc) :
        HtmlFormatter(args), chmDoc(doc) { }
};

void ChmFormatter::HandleTagImg(HtmlToken *t)
{
    CrashIf(!chmDoc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByName("src");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = chmDoc->GetImageData(src, pagePath);
    if (img)
        EmitImage(img);
}

void ChmFormatter::HandleTagPagebreak(HtmlToken *t)
{
    AttrInfo *attr = t->GetAttrByName("page_path");
    if (!attr || pagePath)
        ForceNewPage();
    if (attr) {
        RectF bbox(0, currY, pageDx, 0);
        currPage->instructions.Append(DrawInstr::Anchor(attr->val, attr->valLen, bbox));
        pagePath.Set(str::DupN(attr->val, attr->valLen));
    }
}

/* BaseEngine for handling CHM documents */

class Chm2EngineImpl : public EbookEngine, public Chm2Engine {
    friend Chm2Engine;

public:
    Chm2EngineImpl() : EbookEngine(), doc(NULL), dataCache(NULL) {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectD(0, 0, 8.27 * GetFileDPI(), 11.693 * GetFileDPI());
    }
    virtual ~Chm2EngineImpl() {
        delete dataCache;
        delete doc;
    }
    virtual Chm2Engine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual WCHAR *GetProperty(DocumentProperty prop) { return doc->GetProperty(prop); }
    virtual const WCHAR *GetDefaultFileExt() const { return L".chm"; }

    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

    virtual bool HasTocTree() const { return doc->HasToc() || doc->HasIndex(); }
    virtual DocTocItem *GetTocTree();

protected:
    ChmDoc *doc;
    ChmDataCache *dataCache;

    bool Load(const WCHAR *fileName);

    DocTocItem *BuildTocTree(HtmlPullParser& parser, int& idCounter);
};

// cf. http://www.w3.org/TR/html4/charset.html#h-5.2.2
static UINT ExtractHttpCharset(const char *html, size_t htmlLen)
{
    if (!strstr(html, "charset="))
        return 0;

    HtmlPullParser parser(html, min(htmlLen, 1024));
    HtmlToken *tok;
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (tok->tag != Tag_Meta)
            continue;
        AttrInfo *attr = tok->GetAttrByName("http-equiv");
        if (!attr || !attr->ValIs("Content-Type"))
            continue;
        attr = tok->GetAttrByName("content");
        ScopedMem<char> mimetype, charset;
        if (!attr || !str::Parse(attr->val, attr->valLen, "%S;%_charset=%S", &mimetype, &charset))
            continue;

        static struct {
            const char *name;
            UINT codepage;
        } codepages[] = {
            { "ISO-8859-1", 1252 }, { "Latin1", 1252 }, { "CP1252", 1252 }, { "Windows-1252", 1252 },
            { "ISO-8859-2", 28592 }, { "Latin2", 28592 },
            { "CP1251", 1251 }, { "Windows-1251", 1251 }, { "KOI8-R", 20866 },
            { "shift-jis", 932 }, { "x-euc", 932 }, { "euc-kr", 949 },
            { "Big5", 950 }, { "GB2312", 936 },
            { "UTF-8", CP_UTF8 },
        };
        for (int i = 0; i < dimof(codepages); i++) {
            if (str::EqI(charset, codepages[i].name))
                return codepages[i].codepage;
        }
        break;
    }
    
    return 0;
}

class ChmHtmlCollector : public EbookTocVisitor {
    ChmDoc *doc;
    WStrList added;
    str::Str<char> html;

public:
    ChmHtmlCollector(ChmDoc *doc) : doc(doc) { }

    char *GetHtml() {
        // first add the homepage
        const char *index = doc->GetHomePath();
        ScopedMem<WCHAR> url(doc->ToStr(index));
        Visit(NULL, url, 0);

        // then add all pages linked to from the table of contents
        doc->ParseToc(this);

        // finally add all the remaining HTML files
        Vec<char *> *paths = doc->GetAllPaths();
        for (size_t i = 0; i < paths->Count(); i++) {
            char *path = paths->At(i);
            if (str::EndsWithI(path, ".htm") || str::EndsWithI(path, ".html")) {
                if (*path == '/')
                    path++;
                url.Set(str::conv::FromUtf8(path));
                Visit(NULL, url, -1);
            }
        }
        FreeVecMembers(*paths);
        delete paths;

        return html.StealData();
    }

    virtual void Visit(const WCHAR *name, const WCHAR *url, int level) {
        if (!url || IsExternalUrl(url))
            return;
        ScopedMem<WCHAR> plainUrl(str::ToPlainUrl(url));
        if (added.FindI(plainUrl) != -1)
            return;
        ScopedMem<char> urlUtf8(str::conv::ToUtf8(plainUrl));
        size_t pageHtmlLen;
        ScopedMem<unsigned char> pageHtml(doc->GetData(urlUtf8, &pageHtmlLen));
        if (!pageHtml)
            return;
        html.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", urlUtf8);
        html.AppendAndFree(doc->ToUtf8(pageHtml, ExtractHttpCharset((const char *)pageHtml.Get(), pageHtmlLen)));
        added.Append(plainUrl.StealData());
    }
};

bool Chm2EngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);
    doc = ChmDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    char *html = ChmHtmlCollector(doc).GetHtml();
    dataCache = new ChmDataCache(doc, html);

    HtmlFormatterArgs args;
    args.htmlStr = dataCache->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = DEFAULT_FONT_NAME;
    args.fontSize = DEFAULT_FONT_SIZE;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = ChmFormatter(&args, dataCache).FormatAllPages(false);
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

DocTocItem *Chm2EngineImpl::GetTocTree()
{
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    if (doc->HasIndex()) {
        // TODO: ToC code doesn't work too well for displaying an index,
        //       so this should really become a tree of its own (which
        //       doesn't rely on entries being in the same order as pages)
        builder.Visit(L"Index", NULL, 1);
        builder.SetIsIndex(true);
        doc->ParseIndex(&builder);
    }
    return builder.GetRoot();
}

bool Chm2Engine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return ChmDoc::IsSupportedFile(fileName, sniff);
}

Chm2Engine *Chm2Engine::CreateFromFile(const WCHAR *fileName)
{
    Chm2EngineImpl *engine = new Chm2EngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* BaseEngine for handling TCR documents */

class TcrEngineImpl : public EbookEngine, public TcrEngine {
    friend TcrEngine;

public:
    TcrEngineImpl() : EbookEngine(), doc(NULL) {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectD(0, 0, 8.27 * GetFileDPI(), 11.693 * GetFileDPI());
    }
    virtual ~TcrEngineImpl() { delete doc; }
    virtual TcrEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual WCHAR *GetProperty(DocumentProperty prop) { return NULL; }
    virtual const WCHAR *GetDefaultFileExt() const { return L".tcr"; }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

protected:
    TcrDoc *doc;

    bool Load(const WCHAR *fileName);
};

bool TcrEngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = TcrDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = DEFAULT_FONT_NAME;
    args.fontSize = DEFAULT_FONT_SIZE;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = HtmlFormatter(&args).FormatAllPages(false);

    return pages->Count() > 0;
}

bool TcrEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return TcrDoc::IsSupportedFile(fileName, sniff);
}

TcrEngine *TcrEngine::CreateFromFile(const WCHAR *fileName)
{
    TcrEngineImpl *engine = new TcrEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* BaseEngine for handling HTML documents */
/* (mainly to allow creating minimal regression test testcases more easily) */

class HtmlEngineImpl : public EbookEngine, public HtmlEngine {
    friend HtmlEngine;

public:
    HtmlEngineImpl() : EbookEngine(), doc(NULL) {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectD(0, 0, 8.27 * GetFileDPI(), 11.693 * GetFileDPI());
    }
    virtual ~HtmlEngineImpl() {
        delete doc;
    }
    virtual HtmlEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual WCHAR *GetProperty(DocumentProperty prop) { return doc->GetProperty(prop); }
    virtual const WCHAR *GetDefaultFileExt() const { return L".html"; }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

protected:
    HtmlDoc *doc;

    bool Load(const WCHAR *fileName);

    virtual PageElement *CreatePageLink(DrawInstr *link, RectI rect, int pageNo);
};

bool HtmlEngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = HtmlDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = DEFAULT_FONT_NAME;
    args.fontSize = DEFAULT_FONT_SIZE;
    args.textAllocator = &allocator;

    pages = HtmlFileFormatter(&args, doc).FormatAllPages(false);
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

class RemoteHtmlDest : public SimpleDest2 {
    ScopedMem<WCHAR> name;

public:
    RemoteHtmlDest(const WCHAR *relativeURL) : SimpleDest2(0, RectD()) {
        const WCHAR *id = str::FindChar(relativeURL, '#');
        if (id) {
            value.Set(str::DupN(relativeURL, id - relativeURL));
            name.Set(str::Dup(id));
        }
        else
            value.Set(str::Dup(relativeURL));
    }

    virtual PageDestType GetDestType() const { return Dest_LaunchFile; }
    virtual WCHAR *GetDestName() const { return str::Dup(name); }
};

PageElement *HtmlEngineImpl::CreatePageLink(DrawInstr *link, RectI rect, int pageNo)
{
    bool isInternal = !memchr(link->str.s, ':', link->str.len);
    if (!isInternal || !link->str.len || '#' == *link->str.s)
        return EbookEngine::CreatePageLink(link, rect, pageNo);

    ScopedMem<WCHAR> url(str::conv::FromHtmlUtf8(link->str.s, link->str.len));
    PageDestination *dest = new RemoteHtmlDest(url);
    return new EbookLink(link, rect, dest, pageNo, true);
}

bool HtmlEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return HtmlDoc::IsSupportedFile(fileName, sniff);
}

HtmlEngine *HtmlEngine::CreateFromFile(const WCHAR *fileName)
{
    HtmlEngineImpl *engine = new HtmlEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* BaseEngine for handling TXT documents */

class TxtEngineImpl : public EbookEngine, public TxtEngine {
    friend TxtEngine;

public:
    TxtEngineImpl() : EbookEngine(), doc(NULL) {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectD(0, 0, 8.27 * GetFileDPI(), 11.693 * GetFileDPI());
    }
    virtual ~TxtEngineImpl() { delete doc; }
    virtual TxtEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual WCHAR *GetProperty(DocumentProperty prop) { return NULL; }
    virtual const WCHAR *GetDefaultFileExt() const {
        return fileName ? path::GetExt(fileName) : L".txt";
    }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

    virtual bool HasTocTree() const { return doc->HasToc(); }
    virtual DocTocItem *GetTocTree();

protected:
    TxtDoc *doc;

    bool Load(const WCHAR *fileName);
};

bool TxtEngineImpl::Load(const WCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = TxtDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    if (doc->IsRFC()) {
        // RFCs are targeted at letter size pages
        pageRect = RectD(0, 0, 8.5 * GetFileDPI(), 11 * GetFileDPI());
    }

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = L"Courier New";
    args.fontSize = DEFAULT_FONT_SIZE;
    args.textAllocator = &allocator;

    pages = TxtFormatter(&args).FormatAllPages(false);
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

DocTocItem *TxtEngineImpl::GetTocTree()
{
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    return builder.GetRoot();
}

bool TxtEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    return TxtDoc::IsSupportedFile(fileName, sniff);
}

TxtEngine *TxtEngine::CreateFromFile(const WCHAR *fileName)
{
    TxtEngineImpl *engine = new TxtEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}
