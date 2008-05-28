/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */
#include "base_util.h"
#include "PdfEngine.h"

#include "str_util.h"
#include "utf_util.h"

// in SumatraPDF.cpp
extern "C" char *GetPasswordForFile(WindowInfo *win, const char *fileName);

#define LINK_ACTION_GOTO "linkActionGoTo";
#define LINK_ACTION_GOTOR "linkActionGoToR";
#define LINK_ACTION_LAUNCH "linkActionLaunch";
#define LINK_ACTION_URI "linkActionUri";
#define LINK_ACTION_NAMED "linkActionNamed";
#define LINK_ACTION_MOVIE "linkActionMovie";
#define LINK_ACTION_UNKNOWN "linkActionUnknown";

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
        -leftMargin, -topMargin, pageDx, pageDy,
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
#ifdef FITZ_HEAD
    unsigned char* bmpData = _bitmap->p;
#else
    unsigned char* bmpData = _bitmap->samples;
#endif
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
#ifdef FITZ_HEAD
        fz_dropgraphics(_rast);
#else
        fz_droprenderer(_rast);
#endif
    }

    CloseHandle(_getPageSem);
}

bool PdfEngineFitz::load(const char *fileName, WindowInfo *win)
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
        error = pdf_repairxref(_xref, (char*)fileName);
        if (error)
            goto Error;
    }

    error = pdf_decryptxref(_xref);
    if (error)
        goto Error;

    if (_xref->crypt) {
#ifdef FITZ_HEAD
        int okay = pdf_setpassword(_xref->crypt, "");
        if (!okay)
            goto Error;
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
#else
        error = pdf_setpassword(_xref->crypt, "");
        if (!error)
            goto DecryptedOk;
        if (!win) {
            // win might not be given if called from pdfbench.cc
            goto Error;
        }
        for (int i=0; i<3; i++) {
            char *pwd = GetPasswordForFile(win, fileName);
            // dialog box was cancelled
            if (!pwd)
                goto Error;
            error = pdf_setpassword(_xref->crypt, pwd);
            free(pwd);
            if (!error)
                goto DecryptedOk;
        }
        goto Error;
#endif
    }

DecryptedOk:
    error = pdf_loadpagetree(&_pageTree, _xref);
    if (error)
        goto Error;

    error = pdf_loadoutline(&_outline, _xref);
    // TODO: can I ignore this error?
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
    wchar_t *dname = utf8_to_utf16(entry->title);
    wchar_t *name = (wchar_t *)malloc((wcslen(dname) + 2) * sizeof(wchar_t));
    swprintf(name, L"\x202A%s", dname);
    free(dname);

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
    int n = fz_tonum(dest), g = fz_togen(dest);

    while (p < _pageCount) {
        if (n == fz_tonum(_pageTree->pref[p]) &&
            g == fz_togen(_pageTree->pref[p]))
            return p + 1;

        p++;
    }

    return 0;
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
    //assert (!error);
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
    unsigned char *bmpdata = (unsigned char*)fz_malloc(image->h * bmpstride);
    if (!bmpdata)
        return;

    for (int y = 0; y < image->h; y++)
    {
        unsigned char *p = bmpdata + y * bmpstride;
#ifdef FITZ_HEAD
        unsigned char *s = image->p + y * image->w * 4;
#else
        unsigned char *s = image->samples + y * image->w * 4;
#endif
        for (int x = 0; x < image->w; x++)
        {
            p[x * 3 + 0] = s[x * 4 + 3];
            p[x * 3 + 1] = s[x * 4 + 2];
            p[x * 3 + 2] = s[x * 4 + 1];
        }
    }
#ifdef FITZ_HEAD
    fz_free(image->p);
    image->p = bmpdata;
#else
    fz_free(image->samples);
    image->samples = bmpdata;
#endif
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
#ifdef FITZ_HEAD
        error = fz_newgraphics(&_rast, 1024 * 512);
#else
        error = fz_newrenderer(&_rast, pdf_devicergb, 0, 1024 * 512);
#endif
    }

    fz_pixmap* image = NULL;
    pdf_page* page = getPdfPage(pageNo);
    if (!page)
        goto Error;
    zoomReal = zoomReal / 100.0;
    ctm = viewctm(page, zoomReal, rotation);
    bbox = fz_transformaabb(ctm, page->mediabox);
#ifdef FITZ_HEAD
    error = fz_drawtree(&image, _rast, page->tree, ctm, pdf_devicergb, fz_roundrect(bbox), 1);
#else
    error = fz_rendertree(&image, _rast, page->tree, ctm, fz_roundrect(bbox), 1);
#endif
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

static int getLinkCount(pdf_link *currLink) {
    if (!currLink)
        return 0;
    int count = 1;
    while (currLink->next) {
        ++count;
        currLink = currLink->next;
    }
    return count;
}

int PdfEngineFitz::linkCount(int pageNo) {
    pdf_page* page = getPdfPage(pageNo);
    if (!page)
        return 0;
    return getLinkCount(page->links);
}

static const char *linkToLinkType(pdf_link *link) {
    switch (link->kind) {
        case PDF_LGOTO:
            return LINK_ACTION_GOTO;
        case PDF_LURI:
            return LINK_ACTION_URI;
    }
    return LINK_ACTION_UNKNOWN;
}

const char* PdfEngineFitz::linkType(int pageNo, int linkNo) {
    pdf_page* page = getPdfPage(pageNo);
    pdf_link* currLink = page->links;
    for (int i = 0; i < linkNo; i++) {
        assert(currLink);
        if (!currLink)
            return NULL;
        currLink = currLink->next;
    }
    return linkToLinkType(currLink);
}

