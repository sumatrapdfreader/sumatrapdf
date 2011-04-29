/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// hack to prevent libdjvu from being built as an export/import library
#define DDJVUAPI /**/
#define MINILISPAPI /**/

#include <ddjvuapi.h>
#include <miniexp.h>
#include "DjVuEngine.h"
#include "FileUtil.h"

// TODO: libdjvu seems to leak memory - where?
//       maybe http://sourceforge.net/projects/djvu/forums/forum/103286/topic/3553602

class RenderedDjVuPixmap : public RenderedBitmap {
public:
    RenderedDjVuPixmap(char *data, int width, int height);
};

RenderedDjVuPixmap::RenderedDjVuPixmap(char *data, int width, int height) :
    RenderedBitmap(NULL, width, height)
{
    int stride = ((width * 3 + 3) / 4) * 4;

    BITMAPINFO *bmi = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFOHEADER));

    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = width;
    bmi->bmiHeader.biHeight = -height;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biBitCount = 24;
    bmi->bmiHeader.biSizeImage = height * stride;
    bmi->bmiHeader.biClrUsed = 0;

    HDC hDC = GetDC(NULL);
    _hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT, data, bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hDC);

    free(bmi);
}

// Ensure that libdjvu doesn't trigger the CRT's leak detection
// at least when it's never been used at all
class DjVuCleanUp { public: ~DjVuCleanUp() { minilisp_finish(); } };
DjVuCleanUp _globalCleanup;

class DjVuToCItem : public DocToCItem, public PageDestination {
public:
    DjVuToCItem(const char *title) : DocToCItem(Str::Conv::FromUtf8(title)) { }

    virtual PageDestination *GetLink() { return this; }

    virtual const char *GetType() const { return "ScrollTo"; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT); }
};

class CDjVuEngine : public DjVuEngine {
    friend DjVuEngine;

public:
    CDjVuEngine();
    virtual ~CDjVuEngine();
    virtual DjVuEngine *Clone() {
        return CreateFromFileName(fileName);
    }

    virtual const TCHAR *FileName() const { return fileName; };
    virtual int PageCount() const { return pageCount; }

    virtual RectD PageMediabox(int pageNo) {
        assert(1 <= pageNo && pageNo <= PageCount());
        return mediaboxes[pageNo-1];
    }

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false);
    virtual bool RenderPage(HDC hDC, int pageNo, RectI screenRect,
                         float zoom=0, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View);

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool HasTextContent() { return false; }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) { return NULL; }
    virtual bool IsImagePage(int pageNo) { return true; }
    virtual PageLayoutType PreferredLayout() { return Layout_Single; }

    // DPI isn't constant for all pages and thus premultiplied
    virtual float GetFileDPI() const { return 300.0f; }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".djvu"); }

    // we currently don't load pages lazily, so there's nothing to do here
    virtual bool BenchLoadPage(int pageNo) { return true; }

    virtual bool HasToCTree() const { return outline != miniexp_nil; }
    virtual DocToCItem *GetToCTree();

protected:
    const TCHAR *fileName;

    int pageCount;
    RectD *mediaboxes;

    ddjvu_context_t *ctx;
    ddjvu_document_t *doc;
    miniexp_t outline;

    CRITICAL_SECTION ctxAccess;

    DjVuToCItem *BuildToCTree(miniexp_t entry, int& idCounter);
    bool Load(const TCHAR *fileName);
};

void SpinDdjvuMessageLoop(ddjvu_context_t *ctx, bool wait=true)
{
    const ddjvu_message_t *msg;
    if (wait)
        msg = ddjvu_message_wait(ctx);
    while ((msg = ddjvu_message_peek(ctx)))
        ddjvu_message_pop(ctx);
}

CDjVuEngine::CDjVuEngine() : fileName(NULL), pageCount(0), mediaboxes(NULL), outline(miniexp_nil)
{
    InitializeCriticalSection(&ctxAccess);

    // for now, create one ddjvu context per document
    // TODO: share between all DjVuEngines
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ScopedMem<char> unique(_MemToHex(&ft));
    ctx = ddjvu_context_create(unique);
}

CDjVuEngine::~CDjVuEngine()
{
    EnterCriticalSection(&ctxAccess);

    delete[] mediaboxes;
    free((void *)fileName);

    if (outline != miniexp_nil)
        ddjvu_miniexp_release(doc, outline);
    if (doc)
        ddjvu_document_release(doc);
    if (ctx)
        ddjvu_context_release(ctx);

    LeaveCriticalSection(&ctxAccess);
    DeleteCriticalSection(&ctxAccess);
}

bool CDjVuEngine::Load(const TCHAR *fileName)
{
    if (!ctx)
        return false;

    this->fileName = Str::Dup(fileName);

    ScopedMem<char> fileNameUtf8(Str::Conv::ToUtf8(fileName));
    doc = ddjvu_document_create_by_filename_utf8(ctx, fileNameUtf8, /* cache */ TRUE);
    if (!doc)
        return false;

    while (!ddjvu_document_decoding_done(doc))
        SpinDdjvuMessageLoop(ctx);

    pageCount = ddjvu_document_get_pagenum(doc);
    mediaboxes = new RectD[pageCount];

    for (int i = 0; i < pageCount; i++) {
        ddjvu_status_t status;
        ddjvu_pageinfo_t info;
        while ((status = ddjvu_document_get_pageinfo(doc, i, &info)) < DDJVU_JOB_OK)
            SpinDdjvuMessageLoop(ctx);
        if (status != DDJVU_JOB_OK)
            continue;

        mediaboxes[i] = RectD(0, 0, info.width * GetFileDPI() / info.dpi,
                                    info.height * GetFileDPI() / info.dpi);
    }

    while ((outline = ddjvu_document_get_outline(doc)) == miniexp_dummy)
        SpinDdjvuMessageLoop(ctx);
    if (!miniexp_consp(outline) || miniexp_car(outline) != miniexp_symbol("bookmarks")) {
        ddjvu_miniexp_release(doc, outline);
        outline = miniexp_nil;
    }

    return true;
}

RenderedBitmap *CDjVuEngine::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, bool useGdi)
{
    ScopedCritSec scope(&ctxAccess);

    RectD pageRc = pageRect ? *pageRect : PageMediabox(pageNo);
    RectI screen = Transform(pageRc, pageNo, zoom, rotation).Round();
    RectI full = Transform(PageMediabox(pageNo), pageNo, zoom, rotation).Round();
    screen = full.Intersect(screen);

    ddjvu_page_t *page = ddjvu_page_create_by_pageno(doc, pageNo-1);
    if (!page)
        return NULL;
    int rotation4 = (((-rotation / 90) % 4) + 4) % 4;
    ddjvu_page_set_rotation(page, (ddjvu_page_rotation_t)rotation4);

    while (!ddjvu_page_decoding_done(page))
        SpinDdjvuMessageLoop(ctx);
    if (ddjvu_page_decoding_error(page))
        return NULL;

    ddjvu_format_t *fmt = ddjvu_format_create(DDJVU_FORMAT_BGR24, 0, NULL);
    ddjvu_format_set_row_order(fmt, /* top_to_bottom */ TRUE);
    ddjvu_rect_t prect = { full.x, full.y, full.dx, full.dy };
    ddjvu_rect_t rrect = { screen.x, 2 * full.y - screen.y + full.dy - screen.dy, screen.dx, screen.dy };

    RenderedBitmap *bmp = NULL;
    int stride = ((screen.dx * 3 + 3) / 4) * 4;
    ScopedMem<char> bmpData(SAZA(char, stride * (screen.dy + 5)));
    if (bmpData) {
#ifndef DEBUG
        if (ddjvu_page_render(page, DDJVU_RENDER_COLOR, &prect, &rrect, fmt, stride, bmpData.Get()))
#else
        // TODO: there seems to be a heap corruption in IW44Image.cpp
        //       in debug builds when passing in DDJVU_RENDER_COLOR
        if (ddjvu_page_render(page, DDJVU_RENDER_MASKONLY, &prect, &rrect, fmt, stride, bmpData.Get()))
#endif
            bmp = new RenderedDjVuPixmap(bmpData, screen.dx, screen.dy);
    }

    ddjvu_format_release(fmt);
    ddjvu_page_release(page);
    return bmp;
}

bool CDjVuEngine::RenderPage(HDC hDC, int pageNo, RectI screenRect, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    RenderedBitmap *bmp = RenderBitmap(pageNo, zoom, rotation, pageRect, target);
    if (!bmp)
        return false;
    bmp->StretchDIBits(hDC, screenRect);
    delete bmp;
    return true;
}

PointD CDjVuEngine::Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse)
{
    assert(zoom > 0);
    if (zoom <= 0)
        return pt;

    SizeD page = PageMediabox(pageNo).Size();

    if (inverse) {
        // transform the page size to get a correct frame of reference
        page.dx *= zoom; page.dy *= zoom;
        if (rotate % 180 != 0)
            swap(page.dx, page.dy);
        // invert rotation and zoom
        rotate = -rotate;
        zoom = 1.0f / zoom;
    }

    PointD res;
    rotate = rotate % 360;
    if (rotate < 0) rotate += 360;
    if (90 == rotate)
        res = PointD(page.dy - pt.y, pt.x);
    else if (180 == rotate)
        res = PointD(page.dx - pt.x, page.dy - pt.y);
    else if (270 == rotate)
        res = PointD(pt.y, page.dx - pt.x);
    else // if (0 == rotate)
        res = pt;

    res.x *= zoom; res.y *= zoom;
    return res;
}

RectD CDjVuEngine::Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse)
{
    PointD TL = Transform(rect.TL(), pageNo, zoom, rotate, inverse);
    PointD BR = Transform(rect.BR(), pageNo, zoom, rotate, inverse);
    return RectD::FromXY(TL, BR);
}

unsigned char *CDjVuEngine::GetFileData(size_t *cbCount)
{
    return (unsigned char *)File::ReadAll(fileName, cbCount);
}

DjVuToCItem *CDjVuEngine::BuildToCTree(miniexp_t entry, int& idCounter)
{
    DjVuToCItem *node = NULL;
    DocToCItem *last = node;

    for (miniexp_t rest = miniexp_cdr(entry); miniexp_consp(rest); rest = miniexp_cdr(rest)) {
        miniexp_t item = miniexp_car(rest);
        if (!miniexp_consp(item) || !miniexp_consp(miniexp_cdr(item)) ||
            !miniexp_stringp(miniexp_car(item)) || !miniexp_stringp(miniexp_cadr(item)))
            continue;

        const char *name = miniexp_to_str(miniexp_car(item));
        const char *link = miniexp_to_str(miniexp_cadr(item));

        if (!node)
            last = node = new DjVuToCItem(name);
        else
            last = last->next = new DjVuToCItem(name);
        last->id = ++idCounter;
        // TODO: resolve all link types
        if (Str::StartsWith(link, "#"))
            last->pageNo = atoi(link + 1);

        last->child = BuildToCTree(miniexp_cddr(item), idCounter);
    }

    return node;
}

static DocToCItem *CleanOutTree(DocToCItem *root)
{
    if (!root)
        return NULL;

    root->child = CleanOutTree(root->child);
    root->next = CleanOutTree(root->next);
    // remove all leaf nodes without a destination
    if (!root->child && !root->next && !root->pageNo) {
        delete root;
        return NULL;
    }

    return root;
}

DocToCItem *CDjVuEngine::GetToCTree()
{
    if (outline == miniexp_nil)
        return NULL;

    int idCounter = 0;
    return CleanOutTree(BuildToCTree(outline, idCounter));
}

DjVuEngine *DjVuEngine::CreateFromFileName(const TCHAR *fileName)
{
    CDjVuEngine *engine = new CDjVuEngine();
    if (!engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;    
}
