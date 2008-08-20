/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */
#include "base_util.h"
#include "PdfEngine.h"

#include "str_util.h"
#include "utf_util.h"

// in SumatraPDF.cpp
extern "C" char *GetPasswordForFile(WindowInfo *win, const char *fileName);

/* hack to make fz_throw work in C++ */
#ifdef nil
#undef nil
#define nil ((fz_error*)0)
#endif

static fz_error *pdf_getpageinfo(pdf_xref *xref, fz_obj *dict, fz_rect *bboxp, int *rotatep)
{
    fz_rect bbox;
    int rotate;
    fz_obj *obj;
    fz_error *error;

    obj = fz_dictgets(dict, "CropBox");
    if (!obj)
        obj = fz_dictgets(dict, "MediaBox");

    if (fz_isindirect(obj)) {
        fz_obj* obj2;
        error = pdf_loadindirect(&obj2, xref, obj);
        if (error)
            return error;
        obj = obj2;
    }

    if (!fz_isarray(obj))
        return fz_throw("syntaxerror: Page missing MediaBox");
    bbox = pdf_torect(obj);

    //pdf_logpage("bbox [%g %g %g %g]\n", bbox.x0, bbox.y0, bbox.x1, bbox.y1);

    obj = fz_dictgets(dict, "Rotate");
    rotate = 0;
    if (fz_isint(obj))
        rotate = fz_toint(obj);

    //pdf_logpage("rotate %d\n", rotate);

    if (bboxp)
        *bboxp = bbox;
    if (rotatep)
        *rotatep = rotate;
    return nil;
}

static HBITMAP createDIBitmapCommon(RenderedBitmap *bmp, HDC hdc)
{
    int bmpDx = bmp->dx();
    int bmpDy = bmp->dy();
    int bmpRowSize = bmp->rowSize();

    BITMAPINFOHEADER bmih;
    bmih.biSize = sizeof(bmih);
    bmih.biHeight = -bmpDy;
    bmih.biWidth = bmpDx;
    bmih.biPlanes = 1;
    bmih.biBitCount = 24;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = bmpDy * bmpRowSize;;
    bmih.biXPelsPerMeter = bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = bmih.biClrImportant = 0;

    unsigned char* bmpData = bmp->data();
    HBITMAP hbmp = ::CreateDIBitmap(hdc, &bmih, CBM_INIT, bmpData, (BITMAPINFO *)&bmih , DIB_RGB_COLORS);
    return hbmp;
}

static void stretchDIBitsCommon(RenderedBitmap *bmp, HDC hdc, int leftMargin, int topMargin, int pageDx, int pageDy)
{
    int bmpDx = bmp->dx();
    int bmpDy = bmp->dy();
    int bmpRowSize = bmp->rowSize();

    BITMAPINFOHEADER bmih;
    bmih.biSize = sizeof(bmih);
    bmih.biHeight = -bmpDy;
    bmih.biWidth = bmpDx;
    bmih.biPlanes = 1;
    // we could create this dibsection in monochrome
    // if the printer is monochrome, to reduce memory consumption
    // but splash is currently setup to return a full colour bitmap
    bmih.biBitCount = 24;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = bmpDy * bmpRowSize;;
    bmih.biXPelsPerMeter = bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = bmih.biClrImportant = 0;
    unsigned char* bmpData = bmp->data();

    ::StretchDIBits(hdc,
        // destination rectangle
        leftMargin, topMargin, pageDx, pageDy,
        // source rectangle
        0, 0, bmpDx, bmpDy,
        bmpData,
        (BITMAPINFO *)&bmih ,
        DIB_RGB_COLORS,
        SRCCOPY);
}

RenderedBitmapFitz::RenderedBitmapFitz(fz_pixmap *bitmap)
{
    _bitmap = bitmap;
}

RenderedBitmapFitz::~RenderedBitmapFitz()
{
    if (_bitmap)
        fz_droppixmap(_bitmap);
}

int RenderedBitmapFitz::rowSize()
{
    int rowSize = ((_bitmap->w * 3 + 3) / 4) * 4;
    return rowSize;
}

unsigned char *RenderedBitmapFitz::data()
{
    unsigned char* bmpData = _bitmap->samples;
    return bmpData;
}

HBITMAP RenderedBitmapFitz::createDIBitmap(HDC hdc)
{
    return createDIBitmapCommon(this, hdc);
}

void RenderedBitmapFitz::stretchDIBits(HDC hdc, int leftMargin, int topMargin, int pageDx, int pageDy)
{
    stretchDIBitsCommon(this, hdc, leftMargin, topMargin, pageDx, pageDy);
}

fz_matrix PdfEngineFitz::viewctm (pdf_page *page, float zoom, int rotate)
{
    fz_matrix ctm;
    ctm = fz_identity();
    //ctm = fz_concat(ctm, fz_translate(0, -page->mediabox.y1));
    ctm = fz_concat(ctm, fz_translate(-page->mediabox.x0, -page->mediabox.y1));
    ctm = fz_concat(ctm, fz_scale(zoom, -zoom));
    ctm = fz_concat(ctm, fz_rotate(rotate + page->rotate));
    return ctm;
}

PdfEngineFitz::PdfEngineFitz() : 
        PdfEngine()
        , _xref(NULL)
        , _outline(NULL)
        , _pageTree(NULL)
        , _pages(NULL)
        , _rast(NULL)
{
    _getPageSem = CreateSemaphore(NULL, 1, 1, NULL);
}

PdfEngineFitz::~PdfEngineFitz()
{
    if (_pages) {
        for (int i=0; i < _pageCount; i++) {
            if (_pages[i])
                pdf_droppage(_pages[i]);
        }
        free(_pages);
    }

    if (_pageTree)
        pdf_droppagetree(_pageTree);

    if (_outline)
        pdf_dropoutline(_outline);

    if (_xref) {
        if (_xref->store)
            pdf_dropstore(_xref->store);
        _xref->store = 0;
        pdf_closexref(_xref);
    }

    if (_rast) {
        fz_droprenderer(_rast);
    }

    CloseHandle(_getPageSem);
}

bool PdfEngineFitz::load(const char *fileName, WindowInfo *win, bool tryrepair)
{
    _windowInfo = win;
    setFileName(fileName);
    fz_error *error = pdf_newxref(&_xref);
    if (error)
        goto Error;

    error = pdf_loadxref(_xref, (char*)fileName);
    if (error) {
        if (!strncmp(error->msg, "ioerror", 7))
            goto Error;
        if (tryrepair) {
            error = pdf_repairxref(_xref, (char*)fileName);
            if (error)
                goto Error;
        }
    }

    error = pdf_decryptxref(_xref);
    if (error)
        goto Error;

    if (_xref->crypt) {
        int okay = pdf_setpassword(_xref->crypt, "");
        if (okay)
            goto DecryptedOk;
        if (!win) {
            // win might not be given if called from pdfbench.cc
            goto Error;
        }
        for (int i=0; i<3; i++) {
            char *pwd = GetPasswordForFile(win, fileName);
            okay = pdf_setpassword(_xref->crypt, pwd);
            free(pwd);
            if (okay)
                goto DecryptedOk;
        }
        goto Error;
    }

DecryptedOk:
    error = pdf_loadpagetree(&_pageTree, _xref);
    if (error)
        goto Error;

    /*
     * Load meta information
     * TODO: move this into mupdf library
     * TODO: more descriptive errors?
     */

    fz_obj *obj;
    obj = fz_dictgets(_xref->trailer, "Root");
    if (!obj)
        goto Error;

    error = pdf_loadindirect(&_xref->root, _xref, obj);
    if (error)
        goto Error;

    obj = fz_dictgets(_xref->trailer, "Info");
    if (obj)
    {
        error = pdf_loadindirect(&_xref->info, _xref, obj);
        if (error)
            goto Error;
    }

    error = pdf_loadnametrees(_xref);
    if (error)
        goto Error;

    error = pdf_loadoutline(&_outline, _xref);
    if (error)
        goto Error;

    _pageCount = _pageTree->count;
    _pages = (pdf_page**)malloc(sizeof(pdf_page*) * _pageCount);
    for (int i = 0; i < _pageCount; i++)
        _pages[i] = NULL;
    return true;
Error:
    return false;
}

PdfTocItem *PdfEngineFitz::buildTocTree(pdf_outline *entry)
{
    wchar_t *name = utf8_to_utf16(entry->title);

    PdfTocItem *node = new PdfTocItem(name, entry->link);

    if (entry->child)
        node->AddChild(buildTocTree(entry->child));
    
    if (entry->next)
        node->AddSibling(buildTocTree(entry->next));

    return node;
}

PdfTocItem *PdfEngineFitz::getTocTree()
{
    if (!hasTocTree())
        return NULL;

    return buildTocTree(_outline);
}

int PdfEngineFitz::findPageNo(fz_obj *dest)
{
    int p = 0;
    if (fz_isint(dest)) {
        p = fz_toint(dest);
        return p+1;
    }
    int n = fz_tonum(dest);
    int g = fz_togen(dest);

    while (p++ < _pageCount) {
        fz_obj *page = _pageTree->pref[p];
        int np = fz_tonum(page);
        int gp = fz_togen(page);
        if (n == np && g == gp)
            return p + 1;
    }

    return 0;
}

fz_obj *PdfEngineFitz::getNamedDest(const char *name)
{
    fz_obj *obj = fz_dictgets(_xref->dests, (char*)name);
    if (obj)
        pdf_resolve(&obj, _xref);
    return resolvedest(_xref, obj);
}

pdf_page *PdfEngineFitz::getPdfPage(int pageNo)
{
    if (!_pages)
        return NULL;

    WaitForSingleObject(_getPageSem, INFINITE);
    pdf_page* page = _pages[pageNo-1];
    if (page) {
        if (!ReleaseSemaphore(_getPageSem, 1, NULL))
            DBG_OUT("Fitz: ReleaseSemaphore error!\n");
        return page;
    }
    // TODO: should check for error from pdf_getpageobject?
    fz_obj * obj = pdf_getpageobject(_pageTree, pageNo - 1);
    fz_error * error = pdf_loadpage(&page, _xref, obj);
    if (error) {
        if (!ReleaseSemaphore(_getPageSem, 1, NULL))
            DBG_OUT("Fitz: ReleaseSemaphore error!\n");
        fz_droperror(error);
        return NULL;
    }
    _pages[pageNo-1] = page;
    if (!ReleaseSemaphore(_getPageSem, 1, NULL))
        DBG_OUT("Fitz: ReleaseSemaphore error!\n");
    return page;
}

void PdfEngineFitz::dropPdfPage(int pageNo)
{
    assert(_pages);
    if (!_pages) return;
    pdf_page* page = _pages[pageNo-1];
    assert(page);
    if (!page) return;
    pdf_droppage(page);
    _pages[pageNo-1] = NULL;
}

int PdfEngineFitz::pageRotation(int pageNo)
{
    assert(validPageNo(pageNo));
    fz_obj *dict = pdf_getpageobject(pages(), pageNo - 1);
    int rotation;
    fz_error *error = pdf_getpageinfo(_xref, dict, NULL, &rotation);
    if (error)
        return INVALID_ROTATION;
    return rotation;
}

SizeD PdfEngineFitz::pageSize(int pageNo)
{
    assert(validPageNo(pageNo));
    fz_obj *dict = pdf_getpageobject(pages(), pageNo - 1);
    fz_rect bbox;
    fz_error *error = pdf_getpageinfo(_xref, dict, &bbox, NULL);
    if (error)
        return SizeD(0,0);
    return SizeD(fabs(bbox.x1 - bbox.x0), fabs(bbox.y1 - bbox.y0));
}

bool PdfEngineFitz::printingAllowed()
{
    assert(_xref);
    int permissionFlags = PDF_DEFAULT_PERM_FLAGS;
    if (_xref && _xref->crypt)
        permissionFlags = _xref->crypt->p;
    if (permissionFlags & PDF_PERM_PRINT)
        return true;
    return false;
}

static void ConvertPixmapForWindows(fz_pixmap *image)
{
   int bmpstride = ((image->w * 3 + 3) / 4) * 4;
   int imageh = image->h;
   int imagew = image->w;
   unsigned char *bmpdata = (unsigned char*)fz_malloc(image->h * bmpstride);
   if (!bmpdata)
       return;

   unsigned char *p = bmpdata;
   unsigned char *s = image->samples;
   for (int y = 0; y < imageh; y++)
   {
       unsigned char *pl = p;
       unsigned char *sl = s;
       for (int x = 0; x < imagew; x++)
       {
           pl[0] = sl[3];
           pl[1] = sl[2];
           pl[2] = sl[1];
           pl += 3;
           sl += 4;
       }
       p += bmpstride;
       s += imagew * 4;
   }
   fz_free(image->samples);
   image->samples = bmpdata;
}

RenderedBitmap *PdfEngineFitz::renderBitmap(
                           int pageNo, double zoomReal, int rotation,
                           BOOL (*abortCheckCbkA)(void *data),
                           void *abortCheckCbkDataA)
{
    fz_error* error;
    fz_matrix ctm;
    fz_rect bbox;

    if (!_rast) {
        error = fz_newrenderer(&_rast, pdf_devicergb, 0, 1024 * 512);
    }

    fz_pixmap* image = NULL;
    pdf_page* page = getPdfPage(pageNo);
    if (!page)
        goto Error;
    zoomReal = zoomReal / 100.0;
    ctm = viewctm(page, zoomReal, rotation);
    bbox = fz_transformaabb(ctm, page->mediabox);
    error = fz_rendertree(&image, _rast, page->tree, ctm, fz_roundrect(bbox), 1);
#if CONSERVE_MEMORY
    dropPdfPage(pageNo);
#endif
    if (error)
        goto Error;
    ConvertPixmapForWindows(image);
    return new RenderedBitmapFitz(image);
Error:
    return NULL;
}

static int linksLinkCount(pdf_link *currLink) {
    if (!currLink)
        return 0;
    int count = 1;
    while (currLink->next) {
        ++count;
        currLink = currLink->next;
    }
    return count;
}

/* Return number of all links in all loaded pages */
int PdfEngineFitz::linkCount() {
    if (!_pages)
        return 0;
    int count = 0;

    for (int i=0; i < _pageCount; i++)
    {
        pdf_page* page = _pages[i];
        if (page)
            count += linksLinkCount(page->links);
    }
    return count;
}

pdf_linkkind PdfEngineFitz::linkType(int pageNo, int linkNo) {
    pdf_page* page = getPdfPage(pageNo);
    pdf_link* currLink = page->links;
    for (int i = 0; i < linkNo; i++) {
        assert(currLink);
        if (!currLink)
            return PDF_LUNKNOWN;
        currLink = currLink->next;
    }
    return currLink->kind;
}

