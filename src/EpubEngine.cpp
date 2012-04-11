/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// engines which render flowed ebook formats into fixed pages through the BaseEngine API
// (pages are mostly layed out the same as for a "B Format" paperback: 5.12" x 7.8")

#include "BaseUtil.h"
#include "EpubEngine.h"

#include "EpubDoc.h"
#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlPullParser.h"
#include "HtmlFormatter.h"
#include "MiniMui.h"
#include "TrivialHtmlParser.h"
#include "WinUtil.h"
#include "ZipUtil.h"

// disable warning C4250 which is wrongly issued due to a compiler bug; cf.
// http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function
#pragma warning( disable: 4250 ) /* 'class1' : inherits 'class2::member' via dominance */

/* common classes for EPUB, FictionBook2, Mobi, PalmDOC, CHM, HTML and TXT engines */

namespace str {
    namespace conv {

inline TCHAR *FromHtmlCP(const char *s, size_t len, UINT codePage)
{
    ScopedMem<char> tmp(str::DupN(s, len));
    return DecodeHtmlEntitites(tmp, codePage);
}

inline TCHAR *FromHtmlUtf8(const char *s, size_t len)
{
    return FromHtmlCP(s, len, CP_UTF8);
}

    }
}

inline bool IsExternalUrl(const TCHAR *url)
{
    return str::FindChar(url, ':') != NULL;
}

struct PageAnchor {
    DrawInstr *instr;
    int pageNo;

    PageAnchor(DrawInstr *instr=NULL, int pageNo=-1) : instr(instr), pageNo(pageNo) { }
};

class EbookEngine : public virtual BaseEngine {
public:
    EbookEngine();
    virtual ~EbookEngine();

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages ? pages->Count() : 0; }

    virtual RectD PageMediabox(int pageNo) { return pageRect; }
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View) {
        RectD mbox = PageMediabox(pageNo);
        mbox.Inflate(-pageBorder, -pageBorder);
        return mbox;
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount) {
        return fileName ? (unsigned char *)file::ReadAll(fileName, cbCount) : NULL;
    }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View);
    // make RenderCache request larger tiles than per default
    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Book; }

    virtual Vec<PageElement *> *GetElements(int pageNo);
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt);

    virtual PageDestination *GetNamedDest(const TCHAR *name);

    virtual bool BenchLoadPage(int pageNo) { return true; }

protected:
    TCHAR *fileName;
    Vec<HtmlPage *> *pages;
    Vec<PageAnchor> anchors;
    // contains for each page the last anchor indicating
    // a break between two merged documents
    Vec<DrawInstr *> baseAnchors;
    // needed so that memory allocated by ResolveHtmlEntities isn't leaked
    PoolAllocator allocator;
    // needed since pages::IterStart/IterNext aren't thread-safe
    CRITICAL_SECTION pagesAccess;
    // needed to undo the DPI specific UnitPoint-UnitPixel conversion
    int currFontDpi;

    RectD pageRect;
    float pageBorder;

    void GetTransform(Matrix& m, float zoom, int rotation) {
        GetBaseTransform(m, RectF(0, 0, (float)pageRect.dx, (float)pageRect.dy),
                         zoom, rotation);
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
    ScopedMem<TCHAR> value;

public:
    SimpleDest2(int pageNo, RectD rect, TCHAR *value=NULL) :
        pageNo(pageNo), rect(rect), value(value) { }

    virtual PageDestType GetDestType() const { return value ? Dest_LaunchURL : Dest_ScrollTo; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return rect; }
    virtual TCHAR *GetDestValue() const { return value ? str::Dup(value) : NULL; }
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
    virtual TCHAR *GetValue() const {
        if (!dest || showUrl)
            return str::conv::FromHtmlUtf8(link->str.s, link->str.len);
        return NULL;
    }
    virtual PageDestination *AsLink() { return dest ? dest : this; }

    virtual PageDestType GetDestType() const { return Dest_LaunchURL; }
    virtual int GetDestPageNo() const { return 0; }
    virtual RectD GetDestRect() const { return RectD(); }
    virtual TCHAR *GetDestValue() const { return GetValue(); }
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
    virtual TCHAR *GetValue() const { return NULL; }

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
    EbookTocItem(TCHAR *title, PageDestination *dest) :
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

RenderedBitmap *EbookEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    screen.Offset(-screen.x, -screen.y);

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, screen.dx, screen.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));

    bool ok = RenderPage(hDCMem, screen, pageNo, zoom, rotation, pageRect, target);
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

bool EbookEngine::RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();

    Graphics g(hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    Color white(0xFF, 0xFF, 0xFF);
    Rect screenR(screenRect.x, screenRect.y, screenRect.dx, screenRect.dy);
    g.SetClip(screenR);
    screenR.Inflate(1, 1);
    g.FillRectangle(&SolidBrush(white), screenR);

    Matrix m;
    GetTransform(m, zoom, rotation);
    m.Translate((float)(screenRect.x - screen.x), (float)(screenRect.y - screen.y), MatrixOrderAppend);
    g.SetTransform(&m);

    ScopedCritSec scope(&pagesAccess);
    FixFontSizeForResolution(hDC);
    DrawHtmlPage(&g, GetHtmlPage(pageNo), pageBorder, pageBorder, false, &Color(Color::Black));
    return true;
}

static RectI GetInstrBbox(DrawInstr *instr, float pageBorder)
{
    RectT<float> bbox(instr->bbox.X, instr->bbox.Y, instr->bbox.Width, instr->bbox.Height);
    bbox.Offset(pageBorder, pageBorder);
    return bbox.Round();
}

TCHAR *EbookEngine::ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out, RenderTarget target)
{
    ScopedCritSec scope(&pagesAccess);

    str::Str<TCHAR> content;
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
                ScopedMem<TCHAR> s(str::conv::FromHtmlUtf8(i->str.s, i->str.len));
                content.Append(s);
                size_t len = str::Len(s);
                double cwidth = 1.0 * bbox.dx / len;
                for (size_t k = 0; k < len; k++)
                    coords.Append(RectI((int)(bbox.x + k * cwidth), bbox.y, (int)cwidth, bbox.dy));
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

PageElement *EbookEngine::CreatePageLink(DrawInstr *link, RectI rect, int pageNo)
{
    // internal links don't start with a protocol
    bool isInternal = !memchr(link->str.s, ':', link->str.len);
    if (!isInternal)
        return new EbookLink(link, rect, NULL, pageNo);

    ScopedMem<TCHAR> id;
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

PageDestination *EbookEngine::GetNamedDest(const TCHAR *name)
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

public:
    EbookTocBuilder(BaseEngine *engine) :
        engine(engine),root(NULL), idCounter(0) { }

    virtual void visit(const TCHAR *name, const TCHAR *url, int level) {
        PageDestination *dest;
        if (!url)
            dest = NULL;
        else if (IsExternalUrl(url))
            dest = new SimpleDest2(0, RectD(), str::Dup(url));
        else
            dest = engine->GetNamedDest(url);

        EbookTocItem *item = new EbookTocItem(str::Dup(name), dest);
        item->id = ++idCounter;
        item->open = level <= 2;
        AppendTocItem(root, item, level);
    }

    EbookTocItem *GetRoot() { return root; }
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

    virtual TCHAR *GetProperty(const char *name) { return doc->GetProperty(name); }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".epub"); }

    virtual bool HasTocTree() const { return doc->HasToc(); }
    virtual DocTocItem *GetTocTree();

protected:
    EpubDoc *doc;

    bool Load(const TCHAR *fileName);
    bool Load(IStream *stream);
    bool FinishLoading();

    DocTocItem *BuildTocTree(HtmlPullParser& parser, int& idCounter);
};

bool EpubEngineImpl::Load(const TCHAR *fileName)
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
    args.fontName = L"Georgia";
    args.fontSize = 11;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = EpubFormatter(&args, doc).FormatAllPages();
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

DocTocItem *EpubEngineImpl::GetTocTree()
{
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    return builder.GetRoot();
}

bool EpubEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return EpubDoc::IsSupportedFile(fileName, sniff);
}

EpubEngine *EpubEngine::CreateFromFile(const TCHAR *fileName)
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

/* formatting extensions for FictionBook2 */

#define FB2_TOC_ENTRY_MARK "ToC!Entry!"

class Fb2Formatter : public HtmlFormatter {
    int section;
    int titleCount;

    void HandleTagImg_Fb2(HtmlToken *t);
    void HandleTagAsHtml(HtmlToken *t, const char *name);
    void HandleFb2Tag(HtmlToken *t);

    Fb2Doc *fb2Doc;

public:
    Fb2Formatter(HtmlFormatterArgs *args, Fb2Doc *doc) :
        HtmlFormatter(args), fb2Doc(doc), section(1), titleCount(0) { }

    Vec<HtmlPage*> *FormatAllPages();
};

void Fb2Formatter::HandleTagImg_Fb2(HtmlToken *t)
{
    CrashIf(!fb2Doc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByName(fb2Doc->GetHrefName());
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = fb2Doc->GetImageData(src);
    if (img)
        EmitImage(img);
}

void Fb2Formatter::HandleTagAsHtml(HtmlToken *t, const char *name)
{
    HtmlToken tok;
    tok.SetValue(t->type, name, name + str::Len(name));
    HandleHtmlTag(&tok);
}

void Fb2Formatter::HandleFb2Tag(HtmlToken *t)
{
    if (t->NameIs("title") || t->NameIs("subtitle")) {
        bool isSubtitle = t->NameIs("subtitle");
        ScopedMem<char> name(str::Format("h%d", section + (isSubtitle ? 1 : 0)));
        HtmlToken tok;
        tok.SetValue(t->type, name, name + str::Len(name));
        HandleTagHx(&tok);
        HandleAnchorTag(t);
        if (!isSubtitle && t->IsStartTag()) {
            char *link = (char *)Allocator::Alloc(textAllocator, 24);
            sprintf(link, FB2_TOC_ENTRY_MARK "%d", ++titleCount);
            currPage->instructions.Append(DrawInstr::Anchor(link, str::Len(link), RectF(0, currY, pageDx, 0)));
        }
    }
    else if (t->NameIs("section")) {
        if (t->IsStartTag())
            section++;
        else if (t->IsEndTag() && section > 1)
            section--;
        FlushCurrLine(true);
        HandleAnchorTag(t);
    }
    else if (t->NameIs("p")) {
        if (htmlParser->tagNesting.Find(Tag_Title) == -1)
            HandleHtmlTag(t);
    }
    else if (t->NameIs("image")) {
        HandleTagImg_Fb2(t);
        HandleAnchorTag(t);
    }
    else if (t->NameIs("a")) {
        HandleTagA(t, fb2Doc->GetHrefName());
        HandleAnchorTag(t, true);
    }
    else if (t->NameIs("pagebreak"))
        ForceNewPage();
    else if (t->NameIs("strong"))
        HandleTagAsHtml(t, "b");
    else if (t->NameIs("emphasis"))
        HandleTagAsHtml(t, "i");
    else if (t->NameIs("epigraph"))
        HandleTagAsHtml(t, "blockquote");
    else if (t->NameIs("empty-line")) {
        if (!t->IsEndTag())
            EmitParagraph(0);
    }
}

Vec<HtmlPage*> *Fb2Formatter::FormatAllPages()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleFb2Tag(t);
        else
            HandleText(t);
    }

    FlushCurrLine(true);
    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = NULL;

    Vec<HtmlPage *> *result = new Vec<HtmlPage *>(pagesToSend);
    pagesToSend.Reset();
    return result;
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

    virtual TCHAR *GetProperty(const char *name) { return doc->GetProperty(name); }
    virtual const TCHAR *GetDefaultFileExt() const {
        return doc && doc->IsZipped() ? _T(".fb2z") : _T(".fb2");
    }

    virtual bool HasTocTree() const;
    virtual DocTocItem *GetTocTree();

protected:
    Fb2Doc *doc;

    bool Load(const TCHAR *fileName);
};

bool Fb2EngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = Fb2Doc::CreateFromFile(fileName);
    if (!doc)
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = L"Georgia";
    args.fontSize = 11;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = Fb2Formatter(&args, doc).FormatAllPages();
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
    ScopedMem<TCHAR> itemText;
    int titleCount = 0;
    bool inTitle = false;
    int level = 0;

    size_t xmlLen;
    const char *xmlData = doc->GetTextData(&xmlLen);
    HtmlPullParser parser(xmlData, xmlLen);
    HtmlToken *tok;
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (tok->IsStartTag() && tok->NameIs("section"))
            level++;
        else if (tok->IsEndTag() && tok->NameIs("section") && level > 0)
            level--;
        else if (tok->IsStartTag() && tok->NameIs("title")) {
            inTitle = true;
            titleCount++;
        }
        else if (tok->IsEndTag() && tok->NameIs("title")) {
            if (itemText)
                str::NormalizeWS(itemText);
            if (!str::IsEmpty(itemText.Get())) {
                ScopedMem<TCHAR> name(str::Format(_T(FB2_TOC_ENTRY_MARK) _T("%d"), titleCount));
                PageDestination *dest = GetNamedDest(name);
                EbookTocItem *item = new EbookTocItem(itemText.StealData(), dest);
                item->id = titleCount;
                item->open = level <= 2;
                AppendTocItem(root, item, level);
            }
            inTitle = false;
        }
        else if (inTitle && tok->IsText()) {
            ScopedMem<TCHAR> text(str::conv::FromHtmlUtf8(tok->s, tok->sLen));
            if (str::IsEmpty(itemText.Get()))
                itemText.Set(text.StealData());
            else
                itemText.Set(str::Join(itemText, _T(" "), text));
        }
    }

    return root;
}

bool Fb2Engine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return Fb2Doc::IsSupportedFile(fileName, sniff);
}

Fb2Engine *Fb2Engine::CreateFromFile(const TCHAR *fileName)
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

    virtual const TCHAR *GetDefaultFileExt() const { return _T(".mobi"); }

    virtual PageDestination *GetNamedDest(const TCHAR *name);
    virtual bool HasTocTree() const { return tocReparsePoint != NULL; }
    virtual DocTocItem *GetTocTree();

protected:
    MobiDoc *doc;
    const char *tocReparsePoint;
    ScopedMem<char> pdbHtml;

    bool Load(const TCHAR *fileName);
};

bool MobiEngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = MobiDoc::CreateFromFile(fileName);
    if (!doc || Pdb_Mobipocket != doc->GetDocType())
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetBookHtmlData(args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = L"Georgia";
    args.fontSize = 11;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = MobiFormatter(&args, doc).FormatAllPages();
    if (!ExtractPageAnchors())
        return false;

    HtmlParser parser;
    if (parser.Parse(args.htmlStr)) {
        HtmlElement *ref = NULL;
        while ((ref = parser.FindElementByName("reference", ref))) {
            ScopedMem<TCHAR> type(ref->GetAttribute("type"));
            ScopedMem<TCHAR> filepos(ref->GetAttribute("filepos"));
            if (str::EqI(type, _T("toc")) && filepos) {
                unsigned int pos;
                if (str::Parse(filepos, _T("%u%$"), &pos) && pos < args.htmlStrLen) {
                    tocReparsePoint = args.htmlStr + pos;
                    break;
                }
            }
        }
    }

    return pages->Count() > 0;
}

PageDestination *MobiEngineImpl::GetNamedDest(const TCHAR *name)
{
    int filePos = _ttoi(name);
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
        if (InstrString == i->type && i->str.s >= start &&
            i->str.s <= start + htmlLen && i->str.s - start >= filePos) {
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
    ScopedMem<TCHAR> itemText;
    ScopedMem<TCHAR> itemLink;
    int itemLevel = 0;
    int idCounter = 0;

    // there doesn't seem to be a standard for Mobi ToCs, so we try to
    // determine the author's intentions by looking at commonly used tags
    HtmlPullParser parser(tocReparsePoint, str::Len(tocReparsePoint));
    HtmlToken *tok;
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (itemLink && tok->IsText()) {
            ScopedMem<TCHAR> linkText(str::conv::FromHtmlUtf8(tok->s, tok->sLen));
            if (itemText)
                itemText.Set(str::Join(itemText, _T(" "), linkText));
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

bool MobiEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (sniff) {
        char header[kPdbHeaderLen];
        ZeroMemory(header, sizeof(header));
        file::ReadAll(fileName, header, sizeof(header));
        return str::EqN(header + 60, "BOOKMOBI", 8);
    }

    return str::EndsWithI(fileName, _T(".mobi")) ||
           str::EndsWithI(fileName, _T(".prc"));
}

MobiEngine *MobiEngine::CreateFromFile(const TCHAR *fileName)
{
    MobiEngineImpl *engine = new MobiEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* BaseEngine for handling PalmDOC documents (and extensions such as TealDoc) */

#define PDB_TOC_ENTRY_MARK "ToC!Entry!"

class PdbEngineImpl : public EbookEngine, public PdbEngine {
    friend PdbEngine;

public:
    PdbEngineImpl() : EbookEngine(), doc(NULL) { }
    virtual ~PdbEngineImpl() { delete doc; }
    virtual PdbEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual const TCHAR *GetDefaultFileExt() const { return _T(".pdb"); }

    virtual bool HasTocTree() const { return tocEntries.Count() > 0; }
    virtual DocTocItem *GetTocTree();

protected:
    MobiDoc *doc;
    ScopedMem<char> htmlData;
    StrVec tocEntries;

    bool Load(const TCHAR *fileName);
    const char *HandleTealDocTag(str::Str<char>& builder, const char *text, size_t len, UINT codePage);
};

const char *PdbEngineImpl::HandleTealDocTag(str::Str<char>& builder, const char *text, size_t len, UINT codePage)
{
    if (len < 9) {
Fallback:
        builder.Append("&lt;");
        return text + 1;
    }
    if (!str::StartsWithI(text, "<BOOKMARK") &&
        !str::StartsWithI(text, "<HEADER") &&
        !str::StartsWithI(text, "<TEALPAINT")) {
        goto Fallback;
    }
    HtmlPullParser parser(text, len);
    HtmlToken *tok = parser.Next();
    if (!tok->IsStartTag())
        goto Fallback;

    if (tok->NameIs("BOOKMARK")) {
        // <BOOKMARK NAME="ToC entry">
        AttrInfo *attr = tok->GetAttrByName("NAME");
        if (attr && attr->valLen > 0) {
            tocEntries.Append(str::conv::FromHtmlCP(attr->val, attr->valLen, codePage));
            builder.AppendFmt("<a name=" PDB_TOC_ENTRY_MARK "%d>", tocEntries.Count());
            return tok->s + tok->sLen;
        }
    }
    else if (tok->NameIs("HEADER")) {
        // <HEADER TEXT="Title" FONT=1 ALIGN=CENTER>
        int hx = 2;
        AttrInfo *attr = tok->GetAttrByName("FONT");
        if (attr && attr->valLen > 0)
            hx = '0' == *attr->val ? 1 : '2' == *attr->val ? 3 : 2;
        attr = tok->GetAttrByName("TEXT");
        if (attr) {
            builder.AppendFmt("<h%d>", hx);
            builder.Append(attr->val, attr->valLen);
            builder.AppendFmt("</h%d>", hx);
            return tok->s + tok->sLen;
        }
    }
    else if (tok->NameIs("TEALPAINT")) {
        // <TEALPAINT SRC="file.pdb" IMAGE=0 WIDTH=10 HEIGHT=10 SX=0 SY=7>
        // skip images (for now)
        return tok->s + tok->sLen;
    }
    goto Fallback;
}

bool PdbEngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = MobiDoc::CreateFromFile(fileName);
    if (!doc || Pdb_PalmDoc != doc->GetDocType() && Pdb_TealDoc != doc->GetDocType())
        return false;

    size_t textLen;
    const char *text = doc->GetBookHtmlData(textLen);
    UINT codePage = GuessTextCodepage((char *)text, textLen, CP_ACP);

    str::Str<char> builder;
    builder.Append("<body>");
    for (const char *curr = text; curr < text + textLen; curr++) {
        if ('&' == *curr)
            builder.Append("&amp;");
        else if ('<' == *curr)
            curr = HandleTealDocTag(builder, curr, text + textLen - curr, codePage);
        else if ('\n' == *curr || '\r' == *curr && curr + 1 < text + textLen && '\n' != *(curr + 1))
            builder.Append("\n<br>");
        else
            builder.Append(*curr);
    }
    builder.Append("</body>");

    htmlData.Set(str::ToMultiByte(builder.Get(), codePage, CP_UTF8));

    HtmlFormatterArgs args;
    args.htmlStr = htmlData.Get();
    args.htmlStrLen = str::Len(htmlData.Get());
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = L"Georgia";
    args.fontSize = 11;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = MobiFormatter(&args, doc).FormatAllPages();
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

DocTocItem *PdbEngineImpl::GetTocTree()
{
    EbookTocItem *root = NULL;
    for (size_t i = 0; i < tocEntries.Count(); i++) {
        ScopedMem<TCHAR> name(str::Format(_T(PDB_TOC_ENTRY_MARK) _T("%d"), i + 1));
        PageDestination *dest = GetNamedDest(name);
        EbookTocItem *item = new EbookTocItem(str::Dup(tocEntries.At(i)), dest);
        item->id = i + 1;
        AppendTocItem(root, item, 1);
    }
    return root;
}

bool PdbEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    if (sniff) {
        char header[kPdbHeaderLen];
        ZeroMemory(header, sizeof(header));
        file::ReadAll(fileName, header, sizeof(header));
        return str::EqN(header + 60, "TEXtREAd", 8) ||
               str::EqN(header + 60, "TEXtTlDc", 8);
    }

    return str::EndsWithI(fileName, _T(".pdb"));
}

PdbEngine *PdbEngine::CreateFromFile(const TCHAR *fileName)
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
    void HandleTagImg_Chm(HtmlToken *t);
    void HandleHtmlTag_Chm(HtmlToken *t);

    ChmDataCache *chmDoc;
    ScopedMem<char> pagePath;

public:
    ChmFormatter(HtmlFormatterArgs *args, ChmDataCache *doc) :
        HtmlFormatter(args), chmDoc(doc) { }

    Vec<HtmlPage*> *FormatAllPages();
};

void ChmFormatter::HandleTagImg_Chm(HtmlToken *t)
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

void ChmFormatter::HandleHtmlTag_Chm(HtmlToken *t)
{
    if (Tag_Img == t->tag) {
        HandleTagImg_Chm(t);
        HandleAnchorTag(t);
    }
    else if (Tag_Pagebreak == t->tag) {
        AttrInfo *attr = t->GetAttrByName("page_path");
        if (!attr || pagePath)
            ForceNewPage();
        if (attr) {
            RectF bbox(0, currY, pageDx, 0);
            currPage->instructions.Append(DrawInstr::Anchor(attr->val, attr->valLen, bbox));
            pagePath.Set(str::DupN(attr->val, attr->valLen));
        }
    }
    else
        HandleHtmlTag(t);
}

Vec<HtmlPage*> *ChmFormatter::FormatAllPages()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleHtmlTag_Chm(t);
        else if (!IgnoreText())
            HandleText(t);
    }

    FlushCurrLine(true);
    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = NULL;

    Vec<HtmlPage *> *result = new Vec<HtmlPage *>(pagesToSend);
    pagesToSend.Reset();
    return result;
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

    virtual TCHAR *GetProperty(const char *name) { return doc->GetProperty(name); }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".chm"); }

    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

    virtual bool HasTocTree() const { return doc->HasToc(); }
    virtual DocTocItem *GetTocTree();

protected:
    ChmDoc *doc;
    ChmDataCache *dataCache;

    bool Load(const TCHAR *fileName);

    DocTocItem *BuildTocTree(HtmlPullParser& parser, int& idCounter);
};

static TCHAR *ToPlainUrl(const TCHAR *url)
{
    TCHAR *plainUrl = str::Dup(url);
    str::TransChars(plainUrl, _T("#?"), _T("\0\0"));
    return plainUrl;
}

class ChmHtmlCollector : public EbookTocVisitor {
    ChmDoc *doc;
    StrVec added;
    str::Str<char> html;

public:
    ChmHtmlCollector(ChmDoc *doc) : doc(doc) { }

    char *GetHtml() {
        // first add the homepage
        const char *index = doc->GetIndexPath();
        ScopedMem<TCHAR> url(doc->ToStr(index));
        visit(NULL, url, 0);

        // then add all pages linked to from the table of contents
        doc->ParseToc(this);

        // finally add all the remaining HTML files
        Vec<char *> *paths = doc->GetAllPaths();
        for (size_t i = 0; i < paths->Count(); i++) {
            char *path = paths->At(i);
            if (str::EndsWithI(path, ".htm") || str::EndsWithI(path, ".html")) {
                if (*path == '/')
                    path++;
                url.Set(doc->ToStr(path));
                visit(NULL, url, -1);
            }
        }
        FreeVecMembers(*paths);
        delete paths;

        return html.StealData();
    }

    virtual void visit(const TCHAR *name, const TCHAR *url, int level) {
        if (!url || IsExternalUrl(url))
            return;
        ScopedMem<TCHAR> plainUrl(ToPlainUrl(url));
        if (added.FindI(plainUrl) != -1)
            return;
        ScopedMem<char> urlUtf8(str::conv::ToUtf8(plainUrl));
        // TODO: use the native codepage for the path to GetData
        ScopedMem<unsigned char> pageHtml(doc->GetData(urlUtf8, NULL));
        if (!pageHtml)
            return;
        html.AppendFmt("<pagebreak page_path=\"%s\" page_marker />", urlUtf8);
        html.AppendAndFree(doc->ToUtf8(pageHtml));
        added.Append(plainUrl.StealData());
    }
};

bool Chm2EngineImpl::Load(const TCHAR *fileName)
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
    args.fontName = L"Georgia";
    args.fontSize = 11;
    args.textAllocator = &allocator;
    args.measureAlgo = MeasureTextQuick;

    pages = ChmFormatter(&args, dataCache).FormatAllPages();
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

DocTocItem *Chm2EngineImpl::GetTocTree()
{
    EbookTocBuilder builder(this);
    doc->ParseToc(&builder);
    return builder.GetRoot();
}

bool Chm2Engine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return ChmDoc::IsSupportedFile(fileName, sniff);
}

Chm2Engine *Chm2Engine::CreateFromFile(const TCHAR *fileName)
{
    Chm2EngineImpl *engine = new Chm2EngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* formatting extensions for HTML */

class HtmlFormatter2 : public HtmlFormatter {
protected:
    void HandleTagImg_Html(HtmlToken *t);
    void HandleHtmlTag2(HtmlToken *t);

    HtmlDoc *htmlDoc;

public:
    HtmlFormatter2(HtmlFormatterArgs *args, HtmlDoc *doc) :
        HtmlFormatter(args), htmlDoc(doc) { }

    Vec<HtmlPage*> *FormatAllPages();
};

void HtmlFormatter2::HandleTagImg_Html(HtmlToken *t)
{
    CrashIf(!htmlDoc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByName("src");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = htmlDoc->GetImageData(src);
    if (img)
        EmitImage(img);
}

void HtmlFormatter2::HandleHtmlTag2(HtmlToken *t)
{
    if (Tag_Img == t->tag) {
        HandleTagImg_Html(t);
        HandleAnchorTag(t);
    }
    else
        HandleHtmlTag(t);
}

Vec<HtmlPage*> *HtmlFormatter2::FormatAllPages()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleHtmlTag2(t);
        else if (!IgnoreText())
            HandleText(t);
    }

    FlushCurrLine(true);
    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = NULL;

    Vec<HtmlPage *> *result = new Vec<HtmlPage *>(pagesToSend);
    pagesToSend.Reset();
    return result;
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

    virtual const TCHAR *GetDefaultFileExt() const { return _T(".html"); }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

protected:
    HtmlDoc *doc;

    bool Load(const TCHAR *fileName);

    virtual PageElement *CreatePageLink(DrawInstr *link, RectI rect, int pageNo);
};

bool HtmlEngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = HtmlDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    HtmlFormatterArgs args;
    args.htmlStr = doc->GetTextData(&args.htmlStrLen);
    args.pageDx = (float)pageRect.dx - 2 * pageBorder;
    args.pageDy = (float)pageRect.dy - 2 * pageBorder;
    args.fontName = L"Georgia";
    args.fontSize = 11;
    args.textAllocator = &allocator;

    pages = HtmlFormatter2(&args, doc).FormatAllPages();
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

class RemoteHtmlDest : public SimpleDest2 {
    ScopedMem<TCHAR> name;

public:
    RemoteHtmlDest(const TCHAR *relativeURL) : SimpleDest2(0, RectD()) {
        const TCHAR *id = str::FindChar(relativeURL, '#');
        if (id) {
            value.Set(str::DupN(relativeURL, id - relativeURL));
            name.Set(str::Dup(id));
        }
        else
            value.Set(str::Dup(relativeURL));
    }

    virtual PageDestType GetDestType() const { return Dest_LaunchFile; }
    virtual TCHAR *GetDestName() const { return name ? str::Dup(name) : NULL; }
};

PageElement *HtmlEngineImpl::CreatePageLink(DrawInstr *link, RectI rect, int pageNo)
{
    bool isInternal = !memchr(link->str.s, ':', link->str.len);
    if (!isInternal || !link->str.len || '#' == *link->str.s)
        return EbookEngine::CreatePageLink(link, rect, pageNo);

    ScopedMem<TCHAR> url(str::conv::FromHtmlUtf8(link->str.s, link->str.len));
    PageDestination *dest = new RemoteHtmlDest(url);
    return new EbookLink(link, rect, dest, pageNo, true);
}

bool HtmlEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return HtmlDoc::IsSupportedFile(fileName, sniff);
}

HtmlEngine *HtmlEngine::CreateFromFile(const TCHAR *fileName)
{
    HtmlEngineImpl *engine = new HtmlEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

/* formatting extensions for TXT */

class TxtFormatter : public HtmlFormatter {
protected:
    void HandleHtmlTag_Txt(HtmlToken *t);

public:
    TxtFormatter(HtmlFormatterArgs *args) : HtmlFormatter(args) { }

    Vec<HtmlPage*> *FormatAllPages();
};

void TxtFormatter::HandleHtmlTag_Txt(HtmlToken *t)
{
    if (Tag_Pagebreak == t->tag)
        ForceNewPage();
    else
        HandleHtmlTag(t);
}

Vec<HtmlPage*> *TxtFormatter::FormatAllPages()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleHtmlTag_Txt(t);
        else
            HandleText(t);
    }

    FlushCurrLine(true);
    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = NULL;

    Vec<HtmlPage *> *result = new Vec<HtmlPage *>(pagesToSend);
    pagesToSend.Reset();
    return result;
}

/* BaseEngine for handling TXT documents */

class TxtEngineImpl : public EbookEngine, public TxtEngine {
    friend TxtEngine;

public:
    TxtEngineImpl() : EbookEngine() {
        // ISO 216 A4 (210mm x 297mm)
        pageRect = RectD(0, 0, 8.27 * GetFileDPI(), 11.693 * GetFileDPI());
    }
    virtual ~TxtEngineImpl() { delete doc; }
    virtual TxtEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual const TCHAR *GetDefaultFileExt() const {
        return fileName ? path::GetExt(fileName) : _T(".txt");
    }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

    virtual bool HasTocTree() const { return doc->HasToc(); }
    virtual DocTocItem *GetTocTree();

protected:
    TxtDoc *doc;

    bool Load(const TCHAR *fileName);
};

bool TxtEngineImpl::Load(const TCHAR *fileName)
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
    args.fontSize = 11;
    args.textAllocator = &allocator;

    pages = TxtFormatter(&args).FormatAllPages();
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

bool TxtEngine::IsSupportedFile(const TCHAR *fileName, bool sniff)
{
    return TxtDoc::IsSupportedFile(fileName, sniff);
}

TxtEngine *TxtEngine::CreateFromFile(const TCHAR *fileName)
{
    TxtEngineImpl *engine = new TxtEngineImpl();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}
