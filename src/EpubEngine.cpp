/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Test engines to see how well the BaseEngine API fits flowed ebook formats
// (pages are layed out the same as for a "B Format" paperback: 5.12" x 7.8")

#include "EpubEngine.h"
#include "Scoped.h"
#include "Allocator.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "ZipUtil.h"
#include "MiniMui.h"
#include "GdiPlusUtil.h"
#include "TrivialHtmlParser.h"
#include "HtmlPullParser.h"
#include "PageLayout.h"
#include "MobiDoc.h"

// disable warning C4250 which is wrongly issued due to a compiler bug; cf.
// http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function
#pragma warning( disable: 4250 ) /* 'class1' : inherits 'class2::member' via dominance */

/* common classes for EPUB, FictionBook2 and Mobi engines */

namespace str {
    namespace conv {

inline TCHAR *FromUtf8N(const char *s, size_t len)
{
    ScopedMem<char> tmp(str::DupN(s, len));
    return str::conv::FromUtf8(tmp);
}

    }
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
    const TCHAR *fileName;
    Vec<PageData *> *pages;
    Vec<PageAnchor> anchors;
    // needed so that memory allocated by ResolveHtmlEntities isn't leaked
    PoolAllocator allocator;
    // needed since pages::IterStart/IterNext aren't thread-safe
    CRITICAL_SECTION pagesAccess;
    // needed to undo the DPI specific UnitPoint-UnitPixel conversion
    int currFontDpi;

    RectD pageRect;
    float pageBorder;

    void GetTransform(Matrix& m, float zoom, int rotation) {
        GetBaseTransform(m, RectF(0, 0, (REAL)pageRect.dx, (REAL)pageRect.dy),
                         zoom, rotation);
    }
    bool ExtractPageAnchors();
    void FixFontSizeForResolution(HDC hDC);
    PageElement *CreatePageLink(DrawInstr *link, RectI rect, int pageNo);

    Vec<DrawInstr> *GetPageData(int pageNo) {
        CrashIf(pageNo < 1 || PageCount() < pageNo);
        if (pageNo < 1 || PageCount() < pageNo)
            return NULL;
        return &pages->At(pageNo - 1)->instructions;
    }
};

class SimpleDest2 : public PageDestination {
    int pageNo;
    RectD rect;
    ScopedMem<TCHAR> value;

public:
    SimpleDest2(int pageNo, RectD rect, TCHAR *value=NULL) :
        pageNo(pageNo), rect(rect), value(value) { }

    virtual const char *GetDestType() const { return value ? "LaunchURL" : "ScrollTo"; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return rect; }
    virtual TCHAR *GetDestValue() const { return value ? str::Dup(value) : NULL; }
};

class EbookLink : public PageElement, public PageDestination {
    PageDestination *dest; // required for internal links, NULL for external ones
    DrawInstr *link; // owned by *EngineImpl::pages
    RectI rect;
    int pageNo;

public:
    EbookLink() : dest(NULL), link(NULL), pageNo(-1) { }
    EbookLink(DrawInstr *link, RectI rect, PageDestination *dest, int pageNo=-1) :
        link(link), rect(rect), dest(dest), pageNo(pageNo) { }
    virtual ~EbookLink() { delete dest; }

    virtual PageElementType GetType() const { return Element_Link; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return rect.Convert<double>(); }
    virtual TCHAR *GetValue() const {
        if (!dest)
            return str::conv::FromUtf8N(link->str.s, link->str.len);
        return NULL;
    }
    virtual PageDestination *AsLink() { return dest ? dest : this; }

    virtual const char *GetDestType() const { return "LaunchURL"; }
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
    free((void *)fileName);

    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

bool EbookEngine::ExtractPageAnchors()
{
    ScopedCritSec scope(&pagesAccess);

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
        if (!pageInstrs)
            return false;
        for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
            if (InstrAnchor == i->type)
                anchors.Append(PageAnchor(i, pageNo));
        }
    }

    return true;
}

PointD EbookEngine::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotation, inverse);
    return PointD(rect.x, rect.y);
}

RectD EbookEngine::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    PointF pts[2] = {
        PointF((REAL)rect.x, (REAL)rect.y),
        PointF((REAL)(rect.x + rect.dx), (REAL)(rect.y + rect.dy))
    };
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
        Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
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
    m.Translate((REAL)(screenRect.x - screen.x), (REAL)(screenRect.y - screen.y), MatrixOrderAppend);
    g.SetTransform(&m);

    ScopedCritSec scope(&pagesAccess);
    FixFontSizeForResolution(hDC);
    DrawPageLayout(&g, GetPageData(pageNo), pageBorder, pageBorder, false, &Color(Color::Black));
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

    Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
    for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
        RectI bbox = GetInstrBbox(i, pageBorder);
        switch (i->type) {
        case InstrString:
            if (coords.Count() > 0 && bbox.x <= coords.Last().BR().x) {
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
                ScopedMem<TCHAR> s(str::conv::FromUtf8N(i->str.s, i->str.len));
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

    const char *id = (const char *)memchr(link->str.s, '#', link->str.len);
    if (!id)
        id = link->str.s;
    ScopedMem<TCHAR> idt(str::conv::FromUtf8N(id, link->str.len - (id - link->str.s)));
    PageDestination *dest = GetNamedDest(idt);
    if (!dest)
        return NULL;
    return new EbookLink(link, rect, dest, pageNo);
}

Vec<PageElement *> *EbookEngine::GetElements(int pageNo)
{
    Vec<PageElement *> *els = new Vec<PageElement *>();

    Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
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
    if ('#' == *name)
        name++;
    ScopedMem<char> name_utf8(str::conv::ToUtf8(name));
    size_t name_len = str::Len(name_utf8);

    for (size_t i = 0; i < anchors.Count(); i++) {
        PageAnchor *anchor = &anchors.At(i);
        if (name_len == anchor->instr->str.len &&
            str::EqN(name_utf8, anchor->instr->str.s, name_len)) {
            RectD rect(0, anchor->instr->bbox.Y + pageBorder, pageRect.dx, 10);
            rect.Inflate(-pageBorder, 0);
            return new SimpleDest2(anchor->pageNo, rect);
        }
    }

    return NULL;
}

/* EPUB loading code */

struct ImageData2 {
    ImageData base;
    char *  id; // path by which content refers to this image
    size_t  idx; // document specific index at which to find this image
};

class EpubEngineImpl;

class EpubDoc {
    friend EpubEngineImpl;

    ZipFile zip;
    str::Str<char> htmlData;
    Vec<ImageData2> images;
    Vec<const char *> props;
    ScopedMem<TCHAR> tocPath;

    bool Load();
    void ParseMetadata(const char *content);

    static bool VerifyEpub(ZipFile& zip) {
        ScopedMem<char> firstFileData(zip.GetFileData(_T("mimetype")));
        // a proper EPUB documents has a "mimetype" file with content
        // "application/epub+zip" as the first entry in its ZIP structure
        return str::Eq(zip.GetFileName(0), _T("mimetype")) &&
               str::Eq(firstFileData, "application/epub+zip");
    }

public:
    EpubDoc(const TCHAR *fileName) : zip(fileName) { }
    EpubDoc(IStream *stream) : zip(stream) { }
    ~EpubDoc() {
        for (size_t i = 0; i < images.Count(); i++) {
            free(images.At(i).base.data);
            free(images.At(i).id);
        }
        for (size_t i = 1; i < props.Count(); i += 2) {
            free((void *)props.At(i));
        }
    }

    const char *GetBookData(size_t *lenOut) {
        *lenOut = htmlData.Size();
        return htmlData.Get();
    }

    ImageData *GetImageData(const char *id) {
        // TODO: paths are relative from the html document to the image
        for (size_t i = 0; i < images.Count(); i++) {
            if (str::EndsWith(id, images.At(i).id)) {
                if (!images.At(i).base.data)
                    images.At(i).base.data = zip.GetFileData(images.At(i).idx, &images.At(i).base.len);
                if (images.At(i).base.data)
                    return &images.At(i).base;
            }
        }
        return NULL;
    }

    TCHAR *GetProperty(const char *name) {
        for (size_t i = 0; i < props.Count(); i += 2) {
            if (str::Eq(props.At(i), name))
                return str::conv::FromUtf8(props.At(i + 1));
        }
        return NULL;
    }

    char *GetToC() {
        if (!tocPath)
            return NULL;
        return zip.GetFileData(tocPath);
    }

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff) {
        if (sniff) {
            return VerifyEpub(ZipFile(fileName));
        }
        return str::EndsWithI(fileName, _T(".epub"));
    }
};

static void UrlDecode(TCHAR *url)
{
    for (TCHAR *src = url; *src; src++, url++) {
        int val;
        if (*src == '%' && str::Parse(src, _T("%%%2x"), &val)) {
            *url = (char)val;
            src += 2;
        } else {
            *url = *src;
        }
    }
    *url = '\0';
}

bool EpubDoc::Load()
{
    if (!VerifyEpub(zip))
        return false;

    ScopedMem<char> container(zip.GetFileData(_T("META-INF/container.xml")));
    HtmlParser parser;
    HtmlElement *node = parser.ParseInPlace(container);
    if (!node)
        return false;
    // only consider the first <rootfile> element (default rendition)
    node = parser.FindElementByName("rootfile");
    if (!node)
        return false;
    ScopedMem<TCHAR> contentPath(node->GetAttribute("full-path"));
    if (!contentPath)
        return false;

    ScopedMem<char> content(zip.GetFileData(contentPath));
    if (!content)
        return false;
    ParseMetadata(content);
    node = parser.ParseInPlace(content);
    if (!node)
        return false;
    node = parser.FindElementByName("manifest");
    if (!node)
        return false;

    if (str::FindChar(contentPath, '/'))
        *(TCHAR *)(str::FindCharLast(contentPath, '/') + 1) = '\0';
    else
        *contentPath = '\0';
    StrVec idPathMap;

    for (node = node->down; node; node = node->next) {
        ScopedMem<TCHAR> mediatype(node->GetAttribute("media-type"));
        if (str::Eq(mediatype, _T("application/xhtml+xml"))) {
            ScopedMem<TCHAR> htmlPath(node->GetAttribute("href"));
            ScopedMem<TCHAR> htmlId(node->GetAttribute("id"));
            if (htmlPath && htmlId) {
                idPathMap.Append(htmlId.StealData());
                idPathMap.Append(htmlPath.StealData());
            }
        }
        else if (str::Eq(mediatype, _T("image/png"))  ||
                 str::Eq(mediatype, _T("image/jpeg")) ||
                 str::Eq(mediatype, _T("image/gif"))) {
            ScopedMem<TCHAR> imgPath(node->GetAttribute("href"));
            if (!imgPath)
                continue;
            ScopedMem<TCHAR> zipPath(str::Join(contentPath, imgPath));
            UrlDecode(zipPath);
            // load the image lazily
            ImageData2 data = { 0 };
            data.id = str::conv::ToUtf8(imgPath);
            data.idx = zip.GetFileIndex(zipPath);
            images.Append(data);
        }
        else if (str::Eq(mediatype, _T("application/x-dtbncx+xml"))) {
            tocPath.Set(node->GetAttribute("href"));
            if (tocPath)
                tocPath.Set(str::Join(contentPath, tocPath));
        }
    }

    node = parser.FindElementByName("spine");
    if (!node)
        return false;
    for (node = node->down; node; node = node->next) {
        if (!str::Eq(node->name, "itemref"))
            continue;
        ScopedMem<TCHAR> idref(node->GetAttribute("idref"));
        if (!idref)
            continue;
        const TCHAR *htmlPath = NULL;
        for (size_t i = 0; i < idPathMap.Count() && !htmlPath; i += 2) {
            if (str::Eq(idref, idPathMap.At(i)))
                htmlPath = idPathMap.At(i+1);
        }
        if (!htmlPath)
            continue;

        ScopedMem<TCHAR> fullPath(str::Join(contentPath, htmlPath));
        ScopedMem<char> html(zip.GetFileData(fullPath));
        if (!html)
            continue;
        if (htmlData.Count() > 0) {
            // insert explicit page-breaks between sections
            htmlData.Append("<pagebreak />");
        }
        // add an anchor with the file name at the top (for internal links)
        ScopedMem<char> utf8_path(str::conv::ToUtf8(htmlPath));
        htmlData.AppendFmt("<a name=\"%s\" />", utf8_path);
        // TODO: merge/remove <head>s and drop everything else outside of <body>s(?)
        htmlData.Append(html);
    }

    return htmlData.Count() > 0;
}

void EpubDoc::ParseMetadata(const char *content)
{
    const char *metadataMap[] = {
        "dc:title",         "Title",
        "dc:creator",       "Author",
        "dc:date",          "CreationDate",
        "dc:description",   "Subject",
        "dc:rights",        "Copyright",
    };

    HtmlPullParser pullParser(content, str::Len(content));
    int insideMetadata = 0;
    HtmlToken *tok;

    while ((tok = pullParser.Next())) {
        if (tok->IsStartTag() && tok->NameIs("metadata"))
            insideMetadata++;
        else if (tok->IsEndTag() && tok->NameIs("metadata"))
            insideMetadata--;
        if (!insideMetadata)
            continue;
        if (!tok->IsStartTag())
            continue;

        for (int i = 0; i < dimof(metadataMap); i += 2) {
            if (tok->NameIs(metadataMap[i])) {
                tok = pullParser.Next();
                if (!tok->IsText())
                    break;
                ScopedMem<char> value(str::DupN(tok->s, tok->sLen));
                char *text = (char *)ResolveHtmlEntities(value, value + tok->sLen, NULL);
                if (text == value)
                    text = str::Dup(text);
                if (text) {
                    props.Append(metadataMap[i+1]);
                    props.Append(text);
                }
                break;
            }
        }
    }
}

/* formatting extensions for EPUB */

class EpubFormatter : public HtmlFormatter {
protected:
    void HandleTagImg_Epub(HtmlToken *t);
    void HandleHtmlTag_Epub(HtmlToken *t);

    EpubDoc *epubDoc;

public:
    EpubFormatter(LayoutInfo *li, EpubDoc *doc) : HtmlFormatter(li), epubDoc(doc) { }

    Vec<PageData*> *Layout();
};

void EpubFormatter::HandleTagImg_Epub(HtmlToken *t)
{
    CrashIf(!epubDoc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByName("src");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = epubDoc->GetImageData(src);
    if (img)
        EmitImage(img);
}

void EpubFormatter::HandleHtmlTag_Epub(HtmlToken *t)
{
    HtmlTag tag = FindTag(t);
    if (Tag_Img == tag) {
        HandleTagImg_Epub(t);
        HandleAnchorTag(t);
    }
    else if (Tag_Pagebreak == tag)
        ForceNewPage();
    else
        HandleHtmlTag(t);
}

Vec<PageData*> *EpubFormatter::Layout()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleHtmlTag_Epub(t);
        else if (!IgnoreText())
            HandleText(t);
    }

    FlushCurrLine(true);
    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = NULL;

    Vec<PageData *> *result = new Vec<PageData *>(pagesToSend);
    pagesToSend.Reset();
    return result;
}

/* BaseEngine for handling EPUB documents */

class EpubEngineImpl : public EbookEngine, public EpubEngine {
    friend EpubEngine;

public:
    EpubEngineImpl() : EbookEngine(), doc(NULL) { }
    virtual ~EpubEngineImpl() { delete doc; }
    virtual EpubEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual TCHAR *GetProperty(char *name) {
        return doc ? doc->GetProperty(name) : NULL;
    }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".epub"); }

    virtual bool HasTocTree() const {
        return doc && ScopedMem<char>(doc->GetToC()) != NULL;
    }
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
    doc = new EpubDoc(fileName);
    return FinishLoading();
}

bool EpubEngineImpl::Load(IStream *stream)
{
    doc = new EpubDoc(stream);
    return FinishLoading();
}

bool EpubEngineImpl::FinishLoading()
{
    if (!doc || !doc->Load())
        return false;

    LayoutInfo li;
    li.htmlStr = doc->GetBookData(&li.htmlStrLen);
    li.pageDx = (int)(pageRect.dx - 2 * pageBorder);
    li.pageDy = (int)(pageRect.dy - 2 * pageBorder);
    li.fontName = L"Georgia";
    li.fontSize = 11;
    li.textAllocator = &allocator;

    pages = EpubFormatter(&li, doc).Layout();
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

static void AppendTocItem(EbookTocItem *& root, EbookTocItem *item, int level)
{
    if (!root) {
        root = item;
        return;
    }
    // find the last child at each level, until finding the parent of the new item
    DocTocItem *r2 = root;
    while (level-- > 0) {
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

DocTocItem *EpubEngineImpl::BuildTocTree(HtmlPullParser& parser, int& idCounter)
{
    ScopedMem<TCHAR> itemText, itemSrc;
    EbookTocItem *root = NULL;
    int level = -1;

    HtmlToken *tok;
    while ((tok = parser.Next()) && !tok->IsError() && (!tok->IsEndTag() || !tok->NameIs("navMap") && !tok->NameIs("ncx:navMap"))) {
        if (tok->IsTag() && (tok->NameIs("navPoint") || tok->NameIs("ncx:navPoint"))) {
            if (itemText) {
                PageDestination *dest = NULL;
                if (itemSrc && str::FindChar(itemSrc, ':'))
                    dest = new SimpleDest2(0, RectD(), itemSrc.StealData());
                else if (itemSrc && str::FindChar(itemSrc, '#'))
                    dest = GetNamedDest(str::FindChar(itemSrc, '#'));
                else if (itemSrc)
                    dest = GetNamedDest(itemSrc);
                itemSrc.Set(NULL);
                EbookTocItem *item = new EbookTocItem(itemText.StealData(), dest);
                item->id = ++idCounter;
                AppendTocItem(root, item, level);
            }
            if (tok->IsStartTag())
                level++;
            else if (tok->IsEndTag())
                level--;
        }
        else if (tok->IsStartTag() && (tok->NameIs("text") || tok->NameIs("ncx:text"))) {
            tok = parser.Next();
            if (tok->IsText())
                itemText.Set(str::conv::FromUtf8N(tok->s, tok->sLen));
            else if (tok->IsError())
                break;
        }
        else if (tok->IsTag() && !tok->IsEndTag() && (tok->NameIs("content") || tok->NameIs("ncx:content"))) {
            AttrInfo *attrInfo = tok->GetAttrByName("src");
            if (attrInfo)
                itemSrc.Set(str::conv::FromUtf8N(attrInfo->val, attrInfo->valLen));
        }
    }

    return root;
}

DocTocItem *EpubEngineImpl::GetTocTree()
{
    ScopedMem<char> tocXml(doc->GetToC());
    if (!tocXml)
        return NULL;

    HtmlPullParser parser(tocXml, str::Len(tocXml));
    HtmlToken *tok;
    // skip to the start of the navMap
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (tok->IsStartTag() && (tok->NameIs("navMap") || tok->NameIs("ncx:navMap"))) {
            int idCounter = 0;
            return BuildTocTree(parser, idCounter);
        }
    }
    return NULL;
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

/* FictionBook2 loading code */

class Fb2EngineImpl;

class Fb2Doc {
    friend Fb2EngineImpl;

    ScopedMem<TCHAR> fileName;
    str::Str<char> xmlData;
    Vec<ImageData2> images;
    ScopedMem<TCHAR> docTitle;

    bool Load();
    void ExtractImage(HtmlPullParser& parser, HtmlToken *tok);

public:
    Fb2Doc(const TCHAR *fileName) : fileName(str::Dup(fileName)) { }
    ~Fb2Doc() {
        for (size_t i = 0; i < images.Count(); i++) {
            free(images.At(i).base.data);
            free(images.At(i).id);
        }
    }

    const char *GetBookData(size_t *lenOut) {
        *lenOut = xmlData.Size();
        return xmlData.Get();
    }

    ImageData *GetImageData(const char *id) {
        for (size_t i = 0; i < images.Count(); i++) {
            if (str::Eq(images.At(i).id, id))
                return &images.At(i).base;
        }
        return NULL;
    }

    TCHAR *GetProperty(const char *name) {
        if (str::Eq(name, "Title") && docTitle)
            return str::Dup(docTitle);
        return NULL;
    }

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff) {
        return str::EndsWithI(fileName, _T(".fb2")) ||
               str::EndsWithI(fileName, _T(".fb2.zip"));
    }
};

bool Fb2Doc::Load()
{
    size_t len;
    ScopedMem<char> data;
    if (str::EndsWithI(fileName, _T(".zip"))) {
        ZipFile archive(fileName);
        data.Set(archive.GetFileData((size_t)0, &len));
    }
    else {
        data.Set(file::ReadAll(fileName, &len));
    }
    if (!data)
        return false;

    const char *xmlPI = str::Find(data, "<?xml");
    if (xmlPI && str::Find(xmlPI, "?>")) {
        HtmlToken pi;
        pi.SetValue(HtmlToken::EmptyElementTag, xmlPI + 2, str::Find(xmlPI, "?>"));
        AttrInfo *enc = pi.GetAttrByName("encoding");
        if (enc) {
            ScopedMem<char> tmp(str::DupN(enc->val, enc->valLen));
            if (str::Find(tmp, "1251")) {
                data.Set(str::ToMultiByte(data, 1251, CP_UTF8));
                len = str::Len(data);
            }
        }
    }

    HtmlPullParser parser(data, len);
    HtmlToken *tok;
    int inBody = 0, inTitleInfo = 0;
    const char *bodyStart = NULL;
    while ((tok = parser.Next()) && !tok->IsError()) {
        if (!inTitleInfo && tok->IsStartTag() && tok->NameIs("body")) {
            if (!inBody++)
                bodyStart = tok->s;
        }
        else if (inBody && tok->IsEndTag() && tok->NameIs("body")) {
            if (!--inBody) {
                if (xmlData.Count() > 0)
                    xmlData.Append("<pagebreak />");
                xmlData.Append('<');
                xmlData.Append(bodyStart, tok->s - bodyStart + tok->sLen);
                xmlData.Append('>');
            }
        }
        else if (!inBody && tok->IsStartTag() && tok->NameIs("binary"))
            ExtractImage(parser, tok);
        else if (!inBody && tok->IsStartTag() && tok->NameIs("title-info"))
            inTitleInfo++;
        else if (inTitleInfo && tok->IsEndTag() && tok->NameIs("title-info"))
            inTitleInfo--;
        else if (inTitleInfo && tok->IsStartTag() && tok->NameIs("book-title")) {
            tok = parser.Next();
            if (tok->IsText()) {
                ScopedMem<char> tmp(str::DupN(tok->s, tok->sLen));
                docTitle.Set(DecodeHtmlEntitites(tmp, CP_UTF8));
            }
            else if (tok->IsError())
                break;
        }
    }

    return xmlData.Size() > 0;
}

inline char decode64(char c)
{
    if ('A' <= c && c <= 'Z')
        return c - 'A';
    if ('a' <= c && c <= 'z')
        return c - 'a' + 26;
    if ('0' <= c && c <= '9')
        return c - '0' + 52;
    if ('+' == c)
        return 62;
    if ('/' == c)
        return 63;
    return -1;
}

char *Base64Decode(const char *s, const char *end, size_t *len)
{
    size_t bound = (end - s) * 3 / 4;
    char *result = SAZA(char, bound);
    char *curr = result;
    unsigned char c = 0;
    int step = 0;
    for (; s < end && *s != '='; s++) {
        char n = decode64(*s);
        if (-1 == n) {
            if (isspace(*s))
                continue;
            free(result);
            return NULL;
        }
        switch (step++ % 4) {
        case 0: c = n; break;
        case 1: *curr++ = (c << 2) | (n >> 4); c = n & 0xF; break;
        case 2: *curr++ = (c << 4) | (n >> 2); c = n & 0x3; break;
        case 3: *curr++ = (c << 6) | (n >> 0); break;
        }
    }
    if (len)
        *len = curr - result;
    return result;
}

void Fb2Doc::ExtractImage(HtmlPullParser& parser, HtmlToken *tok)
{
    ScopedMem<char> id;
    AttrInfo *attrInfo = tok->GetAttrByName("id");
    if (attrInfo)
        id.Set(str::DupN(attrInfo->val, attrInfo->valLen));

    tok = parser.Next();
    if (!tok || !tok->IsText())
        return;

    ImageData2 data = { 0 };
    data.base.data = Base64Decode(tok->s, tok->s + tok->sLen, &data.base.len);
    if (!data.base.data)
        return;
    data.id = str::Join("#", id);
    data.idx = images.Count();
    images.Append(data);
}

/* formatting extensions for FictionBook2 */

class Fb2Formatter : public HtmlFormatter {
    int section;

    void HandleTagImg_Fb2(HtmlToken *t);
    void HandleTagAsHtml(HtmlToken *t, const char *name);
    void HandleFb2Tag(HtmlToken *t);

    Fb2Doc *fb2Doc;

public:
    Fb2Formatter(LayoutInfo *li, Fb2Doc *doc) :
        HtmlFormatter(li), fb2Doc(doc), section(1) { }

    Vec<PageData*> *Layout();
};

void Fb2Formatter::HandleTagImg_Fb2(HtmlToken *t)
{
    CrashIf(!fb2Doc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByName("xlink:href");
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
    if (t->NameIs("title")) {
        HtmlToken tok;
        ScopedMem<char> name(str::Format("h%d", section));
        tok.SetValue(t->type, name, name + str::Len(name));
        HandleTagHx(&tok);
        HandleAnchorTag(t);
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
        HandleTagA(t, "xlink:href");
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

Vec<PageData*> *Fb2Formatter::Layout()
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

    Vec<PageData *> *result = new Vec<PageData *>(pagesToSend);
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

    virtual TCHAR *GetProperty(char *name) {
        return doc ? doc->GetProperty(name) : NULL;
    }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".fb2"); }

protected:
    Fb2Doc *doc;

    bool Load(const TCHAR *fileName);
};

bool Fb2EngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = new Fb2Doc(fileName);
    if (!doc || !doc->Load())
        return false;

    LayoutInfo li;
    li.htmlStr = doc->GetBookData(&li.htmlStrLen);
    li.pageDx = (int)(pageRect.dx - 2 * pageBorder);
    li.pageDy = (int)(pageRect.dy - 2 * pageBorder);
    li.fontName = L"Georgia";
    li.fontSize = 11;
    li.textAllocator = &allocator;

    pages = Fb2Formatter(&li, doc).Layout();
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
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

/* BaseEngine for handling Mobi documents (for reference testing) */

class MobiEngineImpl : public EbookEngine, public MobiEngine {
    friend MobiEngine;

public:
    MobiEngineImpl() : EbookEngine(), doc(NULL), tocReparsePoint(NULL) { }
    virtual ~MobiEngineImpl() { delete doc; }
    virtual MobiEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual PageDestination *GetNamedDest(const TCHAR *name);
    virtual bool HasTocTree() const { return tocReparsePoint != NULL; }
    virtual DocTocItem *GetTocTree();

    virtual const TCHAR *GetDefaultFileExt() const { return _T(".mobi"); }

protected:
    MobiDoc *doc;
    const char *tocReparsePoint;

    bool Load(const TCHAR *fileName);
};

bool MobiEngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = MobiDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    LayoutInfo li;
    li.htmlStr = doc->GetBookHtmlData(li.htmlStrLen);
    li.pageDx = (int)(pageRect.dx - 2 * pageBorder);
    li.pageDy = (int)(pageRect.dy - 2 * pageBorder);
    li.fontName = L"Georgia";
    li.fontSize = 11;
    li.textAllocator = &allocator;

    pages = MobiFormatter(&li, doc).FormatAllPages();
    if (!ExtractPageAnchors())
        return false;

    HtmlParser parser;
    if (parser.Parse(li.htmlStr)) {
        HtmlElement *ref = NULL;
        while ((ref = parser.FindElementByName("reference", ref))) {
            ScopedMem<TCHAR> type(ref->GetAttribute("type"));
            ScopedMem<TCHAR> filepos(ref->GetAttribute("filepos"));
            if (str::EqI(type, _T("toc")) && filepos) {
                unsigned int pos;
                if (str::Parse(filepos, _T("%u%$"), &pos) && pos < li.htmlStrLen) {
                    tocReparsePoint = li.htmlStr + pos;
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
    if (!filePos)
        return NULL;
    int foundPageNo = -1;
    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        if (PageCount() == pageNo || pages->At(pageNo)->reparseIdx > filePos) {
            foundPageNo = pageNo;
            break;
        }
    }
    if (-1 == foundPageNo)
        return NULL;

    size_t htmlLen;
    char *start = doc->GetBookHtmlData(htmlLen);
    ScopedCritSec scope(&pagesAccess);
    Vec<DrawInstr> *pageInstrs = GetPageData(foundPageNo);
    float currY = 0;
    for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
        if (InstrString == i->type && i->str.s >= start &&
            i->str.s <= start + htmlLen && i->str.s - start >= filePos) {
            currY = i->bbox.Y;
            break;
        }
    }
    RectD rect(0, currY + pageBorder, pageRect.dx, 10);
    rect.Inflate(-pageBorder, 0);
    return new SimpleDest2(foundPageNo, rect);
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
            ScopedMem<TCHAR> linkText(str::conv::FromUtf8N(tok->s, tok->sLen));
            if (itemText)
                itemText.Set(str::Join(itemText, _T(" "), linkText));
            else
                itemText.Set(linkText.StealData());
        }
        else if (!tok->IsTag())
            continue;
        else if (tok->NameIs("mbp:pagebreak"))
            break;
        else if (!itemLink && tok->IsStartTag() && tok->NameIs("a")) {
            AttrInfo *attr = tok->GetAttrByName("filepos");
            if (!attr)
                attr = tok->GetAttrByName("href");
            if (attr)
                itemLink.Set(str::conv::FromUtf8N(attr->val, attr->valLen));
        }
        else if (itemLink && tok->IsEndTag() && tok->NameIs("a")) {
            PageDestination *dest = NULL;
            if (!itemText) {
                itemLink.Set(NULL);
                continue;
            }
            if (str::FindChar(itemLink, ':'))
                dest = new SimpleDest2(0, RectD(), itemLink.StealData());
            else
                dest = GetNamedDest(itemLink);
            EbookTocItem *item = new EbookTocItem(itemText.StealData(), dest);
            item->id = ++idCounter;
            AppendTocItem(root, item, itemLevel);
            itemLink.Set(NULL);
        }
        else if (tok->NameIs("blockquote") || tok->NameIs("ul") || tok->NameIs("ol")) {
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
    return str::EndsWithI(fileName, _T(".mobi")) ||
           str::EndsWithI(fileName, _T(".azw"))  ||
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
