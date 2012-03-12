/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Minimal engine around MobiDoc for reference testing
// (pages are layed out the same as for a "B Format" paperback: 5.12" x 7.8")

#include "MobiEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Scoped.h"
#include "Allocator.h"
#include "MobiDoc.h"
#include "PageLayout.h"
#include "MiniMui.h"
#include "GdiPlusUtil.h"

class MobiEngineImpl : public MobiEngine {
    friend MobiEngine;

public:
    MobiEngineImpl();
    virtual ~MobiEngineImpl();
    virtual MobiEngine *Clone() {
        return CreateFromFile(fileName);
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pages.Count(); }

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
        return (unsigned char *)file::ReadAll(fileName, cbCount);
    }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View);
    // make RenderCache request larger tiles than per default
    virtual bool HasClipOptimizations(int pageNo) { return false; }
    virtual PageLayoutType PreferredLayout() { return Layout_Book; }

    virtual const TCHAR *GetDefaultFileExt() const { return _T(".mobi"); }

    virtual bool BenchLoadPage(int pageNo) { return true; }

protected:
    const TCHAR *fileName;
    MobiDoc *doc;
    Vec<PageData *> pages;
    // needed so that memory allocated by ResolveHtmlEntities isn't leaked
    PoolAllocator allocator;
    // needed since pages::IterStart/IterNext aren't thread-safe
    CRITICAL_SECTION iterAccess;
    // needed to undo the DPI specific UnitPoint-UnitPixel conversion
    int currFontDpi;

    RectD pageRect;
    float pageBorder;

    bool Load(const TCHAR *fileName);
    void GetTransform(Matrix& m, float zoom, int rotation) {
        GetBaseTransform(m, RectF(0, 0, (REAL)pageRect.dx, (REAL)pageRect.dy),
                         zoom, rotation);
    }

    Vec<DrawInstr> *GetPageData(int pageNo) {
        CrashIf(pageNo < 1 || PageCount() < pageNo);
        if (pageNo < 1 || PageCount() < pageNo)
            return NULL;
        return &pages.At(pageNo - 1)->instructions;
    }
};

MobiEngineImpl::MobiEngineImpl() : fileName(NULL), doc(NULL),
    pageRect(0, 0, 5.12 * GetFileDPI(), 7.8 * GetFileDPI()), // "B Format" paperback
    pageBorder(0.4f * GetFileDPI()), currFontDpi(96)
{
    InitializeCriticalSection(&iterAccess);
}

MobiEngineImpl::~MobiEngineImpl()
{
    EnterCriticalSection(&iterAccess);

    delete doc;
    DeleteVecMembers(pages);
    free((void *)fileName);

    LeaveCriticalSection(&iterAccess);
    DeleteCriticalSection(&iterAccess);
}

bool MobiEngineImpl::Load(const TCHAR *fileName)
{
    this->fileName = str::Dup(fileName);

    doc = MobiDoc::CreateFromFile(fileName);
    if (!doc)
        return false;

    LayoutInfo li;
    li.mobiDoc = doc;
    li.htmlStr = doc->GetBookHtmlData(li.htmlStrLen);
    li.pageDx = (int)(pageRect.dx - 2 * pageBorder);
    li.pageDy = (int)(pageRect.dy - 2 * pageBorder);
    li.fontName = L"Georgia";
    li.fontSize = 11;
    li.textAllocator = &allocator;

    PageLayout layout;
    for (PageData *pd = layout.IterStart(&li); pd; pd = layout.IterNext()) {
        pages.Append(pd);
    }
    
    return true;
}

PointD MobiEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    RectD rect = Transform(RectD(pt, SizeD()), pageNo, zoom, rotation, inverse);
    return PointD(rect.x, rect.y);
}

RectD MobiEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
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

RenderedBitmap *MobiEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
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

bool MobiEngineImpl::RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
    if (!pageInstrs)
        return false;

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

    ScopedCritSec scope(&iterAccess);
    DrawPageLayout(&g, pageInstrs, pageBorder, pageBorder, false, &Color(Color::Black));
    return true;
}

static RectI GetInstrBbox(DrawInstr *instr, float pageBorder)
{
    RectT<float> bbox(instr->bbox.X, instr->bbox.Y, instr->bbox.Width, instr->bbox.Height);
    bbox.Offset(pageBorder, pageBorder);
    return bbox.Round();
}

TCHAR *MobiEngineImpl::ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out, RenderTarget target)
{
    Vec<DrawInstr> *pageInstrs = GetPageData(pageNo);
    if (!pageInstrs)
        return NULL;

    ScopedCritSec scope(&iterAccess);

    str::Str<TCHAR> content;
    Vec<RectI> coords;
    bool insertSpace = false;

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
                ScopedMem<char> tmp(str::DupN(i->str.s, i->str.len));
                ScopedMem<TCHAR> s(str::conv::FromUtf8(tmp));
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
