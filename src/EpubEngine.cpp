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
    bool FixImageJustification();
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

bool EbookEngine::FixImageJustification()
{
    ScopedCritSec scope(&pagesAccess);

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
        if (!pageInstrs)
            return false;
        for (size_t k = 0; k < pageInstrs->Count(); k++) {
            DrawInstr *i = &pageInstrs->At(k);
            if (InstrImage != i->type)
                continue;
            CrashIf(k == 0 || k + 1 == pageInstrs->Count());
            DrawInstr *i2 = &pageInstrs->At(k+1);
            CrashIf(i2->type != InstrString || i2->str.len != 0 || i2->bbox.Width != i->bbox.Width);
            i->bbox.X = i2->bbox.X;
            // center images that are alone on a line
            DrawInstr *i3 = NULL, *i4 = NULL;
            for (size_t j = k; j > 0 && !i3; j--) {
                if (InstrString == pageInstrs->At(j).type)
                    i3 = &pageInstrs->At(j);
            }
            for (size_t j = k + 2; j < pageInstrs->Count() && !i4; j++) {
                if (InstrString == pageInstrs->At(j).type)
                    i4 = &pageInstrs->At(j);
            }
            if ((!i3 || i3->bbox.Y != i->bbox.Y) && (!i4 || i4->bbox.Y != i->bbox.Y))
                i2->bbox.X = i->bbox.X = ((REAL)pageRect.dx - 2 * pageBorder - i->bbox.Width) / 2;
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
    ScopedCritSec scope(&pagesAccess);

    Vec<PageElement *> *els = new Vec<PageElement *>();

    DrawInstr *linkInstr = NULL;
    RectI linkRect;

    Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
    for (DrawInstr *i = pageInstrs->IterStart(); i; i = pageInstrs->IterNext()) {
        if (InstrImage == i->type)
            els->Append(new ImageDataElement(pageNo, &i->img, GetInstrBbox(i, pageBorder)));
        else if (InstrLinkStart == i->type) {
            linkInstr = i;
            linkRect = RectI();
        }
        else if (InstrLinkEnd == i->type && linkInstr) {
            if (!linkInstr)
                /* TODO: link started on a previous page */;
            else if (!linkRect.IsEmpty()) {
                PageElement *link = CreatePageLink(linkInstr, linkRect, pageNo);
                if (link)
                    els->Append(link);
            }
            linkInstr = NULL;
        }
        else if (InstrString == i->type && linkInstr) {
            RectI bbox = GetInstrBbox(i, pageBorder);
            // split multi-line links into multiple Fb2Links
            if (!linkRect.IsEmpty() && bbox.x <= linkRect.x) {
                PageElement *link = CreatePageLink(linkInstr, linkRect, pageNo);
                if (link)
                    els->Append(link);
                linkRect = bbox;
            }
            else
                linkRect = linkRect.Union(bbox);
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

/* common layout extensions (to be integrated into PageLayout/EbookFormatter) */

class PageLayoutCommon : public PageLayout {
protected:
    void EmitParagraph2(float indentation=0);
    void EmitImage2(ImageData *img);

    void HandleTagHeader(HtmlToken *t);
    void HandleTagA2(HtmlToken *t, const char *linkAttr="href");
    void HandleHtmlTag2(HtmlToken *t);

    int listDepth;

public:
    PageLayoutCommon(LayoutInfo *li);
};

PageLayoutCommon::PageLayoutCommon(LayoutInfo* li) : listDepth(0)
{
    CrashIf(currPage);
    finishedParsing = false;
    layoutInfo = li;
    pageDx = (REAL)layoutInfo->pageDx;
    pageDy = (REAL)layoutInfo->pageDy;
    textAllocator = layoutInfo->textAllocator;
    htmlParser = new HtmlPullParser(layoutInfo->htmlStr, layoutInfo->htmlStrLen);

    CrashIf(gfx);
    gfx = mui::AllocGraphicsForMeasureText();
    defaultFontName.Set(str::Dup(layoutInfo->fontName));
    defaultFontSize = layoutInfo->fontSize;
    SetCurrentFont(FontStyleRegular, defaultFontSize);

    coverImage = NULL;
    pageCount = 0;
    inLink = false;

    lineSpacing = currFont->GetHeight(gfx);
    spaceDx = currFontSize / 2.5f; // note: a heuristic
    float spaceDx2 = GetSpaceDx(gfx, currFont);
    if (spaceDx2 < spaceDx)
        spaceDx = spaceDx2;

    currJustification = Align_Justify;
    currX = 0; currY = 0;
    currPage = new PageData;
    currReparsePoint = NULL;

    currLineTopPadding = 0;
}

void PageLayoutCommon::EmitParagraph2(float indentation)
{
    EmitParagraph(indentation, 0);
    // prevent accidental double-indentation
    for (size_t i = currLineInstr.Count(); i > 0; i--) {
        if (InstrFixedSpace == currLineInstr.At(i-1).type &&
            (!indentation || i < currLineInstr.Count())) {
            currLineInstr.RemoveAt(i-1);
        }
    }
}

void PageLayoutCommon::EmitImage2(ImageData *img)
{
    Rect imgSize = BitmapSizeFromData(img->data, img->len);
    if (imgSize.IsEmptyArea())
        return;

    SizeF newSize((REAL)imgSize.Width, (REAL)imgSize.Height);
    // move overly large images to a new line
    if (!IsCurrLineEmpty() && currX + newSize.Width > pageDx)
        FlushCurrLine(false);
    // move overly large images to a new page
    if (currY > 0 && currY + newSize.Height / 2.f > pageDy)
        ForceNewPage();
    // if image is still bigger than available space, scale it down
    if ((newSize.Width > pageDx) || (currY + newSize.Height) > pageDy) {
        REAL scale = min(pageDx / newSize.Width, (pageDy - currY) / newSize.Height);
        newSize.Width *= scale;
        newSize.Height *= scale;
    }

    RectF bbox(PointF(currX, 0), newSize);
    AppendInstr(DrawInstr::Image(img->data, img->len, bbox));
    // add an empty string in place of the image so that
    // we can make the image flow with the text (as long
    // as justification code doesn't take into account images)
    AppendInstr(DrawInstr::Str("", 0, bbox));
    currX += bbox.Width;
}

void PageLayoutCommon::HandleTagHeader(HtmlToken *t)
{
    if (t->IsEndTag()) {
        FlushCurrLine(true);
        currY += currFontSize / 2;
        currFontSize = defaultFontSize;
        currJustification = Align_Justify;
    }
    else {
        currJustification = Align_Left;
        EmitParagraph2();
        currFontSize = defaultFontSize * pow(1.1f, '5' - t->s[1]);
        if (currY > 0)
            currY += currFontSize / 2;
    }
    ChangeFontStyle(FontStyleBold, t->IsStartTag());
}

void PageLayoutCommon::HandleTagA2(HtmlToken *t, const char *linkAttr)
{
    if (t->IsStartTag() && !inLink) {
        AttrInfo *attr = t->GetAttrByName(linkAttr);
        if (attr) {
            DrawInstr i(InstrLinkStart);
            i.str.s = attr->val;
            i.str.len = attr->valLen;
            AppendInstr(i);
            inLink = true;
        }
    }
    else if (t->IsEndTag() && inLink) {
        AppendInstr(DrawInstr(InstrLinkEnd));
        inLink = false;
    }
}

void PageLayoutCommon::HandleHtmlTag2(HtmlToken *t)
{
    HtmlTag tag = FindTag(t);
    switch (tag) {
    case Tag_Pagebreak:
        ForceNewPage();
        break;
    case Tag_Ul: case Tag_Ol: case Tag_Dl:
        if (t->IsStartTag())
            listDepth++;
        else if (t->IsEndTag()) {
            if (listDepth > 0)
                listDepth--;
            FlushCurrLine(true);
        }
        HandleAnchorTag(t);
        break;
    case Tag_Li: case Tag_Dd:
        if (t->IsStartTag())
            EmitParagraph2(15.f * listDepth);
        else if (t->IsEndTag())
            FlushCurrLine(true);
        HandleAnchorTag(t);
        break;
    case Tag_Dt:
        if (t->IsStartTag()) {
            EmitParagraph2(15.f * (listDepth - 1));
            currJustification = Align_Left;
        }
        else if (t->IsEndTag())
            FlushCurrLine(true);
        ChangeFontStyle(FontStyleBold, t->IsStartTag());
        HandleAnchorTag(t);
        break;
    case Tag_Center:
        currJustification = Align_Center;
        HandleAnchorTag(t);
        break;
    case Tag_H1: case Tag_H2: case Tag_H3:
    case Tag_H4: case Tag_H5:
        HandleTagHeader(t);
        HandleAnchorTag(t);
        break;
    case Tag_A:
        HandleAnchorTag(t);
        HandleTagA2(t);
        break;
    default:
        HandleHtmlTag(t);
        break;
    }
}

/* EPUB loading code (was ebooktest2/EpubDoc.cpp) */

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

/* PageLayout extensions for EPUB (was ebooktest2/PageLayout.cpp) */

class PageLayoutEpub : public PageLayoutCommon {
protected:
    void HandleTagImgEpub(HtmlToken *t);
    void HandleHtmlTagEpub(HtmlToken *t);

    EpubDoc *epubDoc;

public:
    PageLayoutEpub(LayoutInfo *li, EpubDoc *doc) :
        PageLayoutCommon(li), epubDoc(doc) { }

    Vec<PageData*> *Layout();
};

void PageLayoutEpub::HandleTagImgEpub(HtmlToken *t)
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
        EmitImage2(img);
}

void PageLayoutEpub::HandleHtmlTagEpub(HtmlToken *t)
{
    HtmlTag tag = FindTag(t);
    if (Tag_Img == tag) {
        HandleTagImgEpub(t);
        HandleAnchorTag(t);
    }
    else
        HandleHtmlTag2(t);
}

Vec<PageData*> *PageLayoutEpub::Layout()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleHtmlTagEpub(t);
        else if (!IgnoreText())
            HandleText(t);
    }

    FlushCurrLine(true);
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

class EpubTocItem : public DocTocItem {
    PageDestination *dest;

public:
    EpubTocItem(TCHAR *title, PageDestination *dest) :
        DocTocItem(title, dest ? dest->GetDestPageNo() : 0), dest(dest) { }
    ~EpubTocItem() { delete dest; }

    virtual PageDestination *GetLink() { return dest; }
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
    li.mobiDoc = (MobiDoc *)1; // crash on use
    li.htmlStr = doc->GetBookData(&li.htmlStrLen);
    li.pageDx = (int)(pageRect.dx - 2 * pageBorder);
    li.pageDy = (int)(pageRect.dy - 2 * pageBorder);
    li.fontName = L"Georgia";
    li.fontSize = 11;
    li.textAllocator = &allocator;

    pages = PageLayoutEpub(&li, doc).Layout();
    if (!FixImageJustification())
        return false;
    if (!ExtractPageAnchors())
        return false;

    return pages->Count() > 0;
}

static void AppendTocItem(EpubTocItem *& root, EpubTocItem *item, int level)
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
    EpubTocItem *root = NULL;
    int level = -1;

    HtmlToken *tok;
    while ((tok = parser.Next()) && (!tok->IsEndTag() || !tok->NameIs("navMap") && !tok->NameIs("ncx:navMap"))) {
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
                EpubTocItem *item = new EpubTocItem(itemText.StealData(), dest);
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
    while ((tok = parser.Next())) {
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

/* FictionBook2 loading code (was ebooktest2/Fb2Doc.cpp) */

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
    while ((tok = parser.Next())) {
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

/* PageLayout extensions for FictionBook2 */

class PageLayoutFb2 : public PageLayoutCommon {
    int section;

    void HandleTagImgFb2(HtmlToken *t);
    void HandleTagAsHtml(HtmlToken *t, const char *name);
    void HandleFb2Tag(HtmlToken *t);

    Fb2Doc *fb2Doc;

public:
    PageLayoutFb2(LayoutInfo *li, Fb2Doc *doc) :
        PageLayoutCommon(li), fb2Doc(doc), section(1) { }

    Vec<PageData*> *Layout();
};

void PageLayoutFb2::HandleTagImgFb2(HtmlToken *t)
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
        EmitImage2(img);
}

void PageLayoutFb2::HandleTagAsHtml(HtmlToken *t, const char *name)
{
    HtmlToken tok;
    tok.SetValue(t->type, name, name + str::Len(name));
    HandleHtmlTag2(&tok);
}

void PageLayoutFb2::HandleFb2Tag(HtmlToken *t)
{
    if (t->NameIs("title")) {
        HtmlToken tok2;
        ScopedMem<char> name(str::Format("h%d", section));
        tok2.SetValue(t->type, name, name + str::Len(name));
        HandleTagHeader(&tok2);
        HandleAnchorTag(t, true);
    }
    else if (t->NameIs("section")) {
        if (t->IsStartTag())
            section++;
        else if (t->IsEndTag() && section > 1)
            section--;
        FlushCurrLine(true);
        HandleAnchorTag(t, true);
    }
    else if (t->NameIs("p")) {
        if (htmlParser->tagNesting.Find(Tag_Title) == -1)
            HandleHtmlTag2(t);
    }
    else if (t->NameIs("image")) {
        HandleTagImgFb2(t);
        HandleAnchorTag(t, true);
    }
    else if (t->NameIs("a"))
        HandleTagA2(t, "xlink:href");
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
            EmitParagraph2();
    }
}

Vec<PageData*> *PageLayoutFb2::Layout()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleFb2Tag(t);
        else
            HandleText(t);
    }

    FlushCurrLine(true);
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
    li.mobiDoc = (MobiDoc *)1; // crash on use
    li.htmlStr = doc->GetBookData(&li.htmlStrLen);
    li.pageDx = (int)(pageRect.dx - 2 * pageBorder);
    li.pageDy = (int)(pageRect.dy - 2 * pageBorder);
    li.fontName = L"Georgia";
    li.fontSize = 11;
    li.textAllocator = &allocator;

    pages = PageLayoutFb2(&li, doc).Layout();
    if (!FixImageJustification())
        return false;
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

/* PageLayout extensions for Mobi */

class PageLayoutMobi : public PageLayout {
    LayoutInfo *li2;

public:
    PageLayoutMobi(LayoutInfo *li, MobiDoc *doc) : li2(li) {
        li2->mobiDoc = doc;
    }

    Vec<PageData*> *Layout();
};

Vec<PageData*> *PageLayoutMobi::Layout()
{
    Vec<PageData *> *pages = new Vec<PageData *>();

    for (PageData *pd = IterStart(li2); pd; pd = IterNext()) {
        pages->Append(pd);
    }

    return pages;
}

/* BaseEngine for handling Mobi documents (for reference testing) */

class MobiEngineImpl : public EbookEngine, public MobiEngine {
    friend MobiEngine;

public:
    MobiEngineImpl() : EbookEngine(), doc(NULL) { }
    virtual ~MobiEngineImpl() { delete doc; }
    virtual MobiEngine *Clone() {
        return fileName ? CreateFromFile(fileName) : NULL;
    }

    virtual PageDestination *GetNamedDest(const TCHAR *name) { return NULL; }

    virtual const TCHAR *GetDefaultFileExt() const { return _T(".mobi"); }

protected:
    MobiDoc *doc;

    bool Load(const TCHAR *fileName);
};

bool MobiEngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = MobiDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    LayoutInfo li;
    li.mobiDoc = (MobiDoc *)1; // crash on use
    li.htmlStr = doc->GetBookHtmlData(li.htmlStrLen);
    li.pageDx = (int)(pageRect.dx - 2 * pageBorder);
    li.pageDy = (int)(pageRect.dy - 2 * pageBorder);
    li.fontName = L"Georgia";
    li.fontSize = 11;
    li.textAllocator = &allocator;

    pages = PageLayoutMobi(&li, doc).Layout();
    if (!ExtractPageAnchors())
        return false;
    
    return pages->Count() > 0;
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
