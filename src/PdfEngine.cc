/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv3 */
#include "PdfEngine.h"

// in SumatraPDF.cpp
extern "C" TCHAR *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName);

static fz_error pdf_getpageinfo(pdf_xref *xref, fz_obj *dict, fz_rect *bboxp, int *rotatep)
{
    fz_rect bbox;
    int rotate;
    fz_obj *obj;

    obj = fz_dictgets(dict, "CropBox");
    if (!obj)
        obj = fz_dictgets(dict, "MediaBox");

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
    return fz_okay;
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

RenderedBitmap::RenderedBitmap(fz_pixmap *bitmap)
{
    _bitmap = bitmap;
}

RenderedBitmap::~RenderedBitmap()
{
    if (_bitmap)
        fz_droppixmap(_bitmap);
}

int RenderedBitmap::rowSize()
{
    int rowSize = ((_bitmap->w * 3 + 3) / 4) * 4;
    return rowSize;
}

unsigned char *RenderedBitmap::data()
{
    unsigned char* bmpData = _bitmap->samples;
    return bmpData;
}

HBITMAP RenderedBitmap::createDIBitmap(HDC hdc)
{
    return createDIBitmapCommon(this, hdc);
}

void RenderedBitmap::stretchDIBits(HDC hdc, int leftMargin, int topMargin, int pageDx, int pageDy)
{
    stretchDIBitsCommon(this, hdc, leftMargin, topMargin, pageDx, pageDy);
}

fz_matrix PdfEngine::viewctm(pdf_page *page, float zoom, int rotate)
{
    fz_matrix ctm;
    ctm = fz_identity();
    //ctm = fz_concat(ctm, fz_translate(0, -page->mediabox.y1));
    ctm = fz_concat(ctm, fz_translate(-page->mediabox.x0, -page->mediabox.y1));
    ctm = fz_concat(ctm, fz_scale(zoom, -zoom));
    rotate += page->rotate;
    if (rotate != 0)
        ctm = fz_concat(ctm, fz_rotate(rotate));
    return ctm;
}

PdfEngine::PdfEngine() : 
        _fileName(0)
        , _pageCount(INVALID_PAGE_NO) 
        , _xref(NULL)
        , _outline(NULL)
        , _pages(NULL)
        , _rast(NULL)
{
    _getPageSem = CreateSemaphore(NULL, 1, 1, NULL);
}

PdfEngine::~PdfEngine()
{
    if (_pages) {
        for (int i=0; i < _pageCount; i++) {
            if (_pages[i].page)
                pdf_droppage(_pages[i].page);
        }
        free(_pages);
    }

    if (_outline)
        pdf_dropoutline(_outline);

    if (_xref) {
        if (_xref->store)
            pdf_dropstore(_xref->store);
        _xref->store = 0;
        pdf_closexref(_xref);
    }

    if (_rast)
        fz_droprenderer(_rast);

    CloseHandle(_getPageSem);

    free((void*)_fileName);
}

bool PdfEngine::load(const TCHAR *fileName, WindowInfo *win, bool tryrepair)
{
	fz_error error;
    _windowInfo = win;
    setFileName(fileName);
    _xref = pdf_newxref();

    char *utf8path = tstr_to_utf8(fileName);
    error = pdf_loadxref(_xref, utf8path);
    if (error) {
        if (tryrepair)
            error = pdf_repairxref(_xref, utf8path);
        if (error) {
            free(utf8path);
            goto Error;
        }
    }
    free(utf8path);

    error = pdf_decryptxref(_xref);
    if (error)
        goto Error;

    if (pdf_needspassword(_xref)) {
        int okay = pdf_authenticatepassword(_xref, "");
        if (okay)
            goto DecryptedOk;
        if (!win) {
            // win might not be given if called from pdfbench.cc
            goto Error;
        }
        for (int i=0; i<3; i++) {
            TCHAR *pwd = GetPasswordForFile(win, fileName);
            if (!pwd) {
                // password not given
                goto Error;
            }
            char *pwd_utf8 = tstr_to_utf8(pwd);
            if (pwd_utf8) {
                okay = pdf_authenticatepassword(_xref, pwd_utf8);
                free(pwd_utf8);
            }
            else
                okay = 0;
            free(pwd);
            if (okay)
                goto DecryptedOk;
        }
        goto Error;
    }

DecryptedOk:
    /*
     * Load meta information
     * TODO: move this into mupdf library
     * TODO: more descriptive errors?
     */

    /* TODO: pdftool.c doesn't fz_resolveindirect() of "Root" object but
        pdf_getpagecount() does. Who is right? (leaving resolving in since it
        doesn't hurt) */
    fz_obj *obj;
    obj = fz_dictgets(_xref->trailer, "Root");
    _xref->root = fz_resolveindirect(obj);
    if (!_xref->root)
        goto Error;
    fz_keepobj(_xref->root);

    obj = fz_dictgets(_xref->trailer, "Info");
    _xref->info = fz_resolveindirect(obj);
    if (_xref->info)
        fz_keepobj(_xref->info);

    _pageCount = pdf_getpagecount(_xref);
    _outline = pdf_loadoutline(_xref);
    // silently ignore errors from pdf_loadoutline()
    // this information is not critical and checking the
    // error might prevent loading some pdfs that would
    // otherwise get displayed

    _pages = (PdfPage *)malloc(sizeof(PdfPage) * _pageCount);
    memset(_pages, 0, sizeof(PdfPage) * _pageCount);
    return true;
Error:
    return false;
}

PdfTocItem *PdfEngine::buildTocTree(pdf_outline *entry)
{
    TCHAR *name = utf8_to_tstr(entry->title);
    PdfTocItem *node = new PdfTocItem(name, entry->link);
    node->open = entry->count >= 0;
    if (entry->link && PDF_LGOTO == entry->link->kind)
        node->pageNo = findPageNo(entry->link->dest);

    if (entry->child)
        node->AddChild(buildTocTree(entry->child));
    
    if (entry->next)
        node->AddSibling(buildTocTree(entry->next));

    return node;
}

PdfTocItem *PdfEngine::getTocTree()
{
    if (!hasTocTree())
        return NULL;

    return buildTocTree(_outline);
}

int PdfEngine::findPageNo(fz_obj *dest)
{
    if (fz_isint(dest))
        return fz_toint(dest) + 1;

    if (fz_isdict(dest)) {
        // The destination is linked from a Go-To action's D array
        fz_obj * D = fz_dictgets(dest, "D");
        if (D && fz_isarray(D))
            dest = fz_arrayget(D, 0);
    }
    int n = fz_tonum(dest);

    for (int i = 0; i < _pageCount; i++)
    {
        if (0 == _pages[i].num)
            _pages[i].num = fz_tonum(pdf_getpageobject(_xref, i + 1));
        if (n == _pages[i].num)
            return i + 1;
    }

    return 0;
}

fz_obj *PdfEngine::getNamedDest(const char *name)
{
    fz_obj *nameobj = fz_newstring((char*)name, strlen(name));
    return pdf_lookupdest(_xref, nameobj);
}

pdf_page *PdfEngine::getPdfPage(int pageNo)
{
    if (!_pages)
        return NULL;

    WaitForSingleObject(_getPageSem, INFINITE);
    pdf_page* page = _pages[pageNo-1].page;
    if (page) {
        if (!ReleaseSemaphore(_getPageSem, 1, NULL))
            DBG_OUT("Fitz: ReleaseSemaphore error!\n");
        return page;
    }
    fz_obj * obj = pdf_getpageobject(_xref, pageNo);
    fz_error error = pdf_loadpage(&page, _xref, obj);
    if (error) {
        if (!ReleaseSemaphore(_getPageSem, 1, NULL))
            DBG_OUT("Fitz: ReleaseSemaphore error!\n");
        return NULL;
    }
    _pages[pageNo-1].page = page;
    _pages[pageNo-1].num = fz_tonum(obj);
    linkifyPageText(page);
    if (!ReleaseSemaphore(_getPageSem, 1, NULL))
        DBG_OUT("Fitz: ReleaseSemaphore error!\n");
    return page;
}

void PdfEngine::dropPdfPage(int pageNo)
{
    assert(_pages);
    if (!_pages) return;
    pdf_page* page = _pages[pageNo-1].page;
    assert(page);
    if (!page) return;
    pdf_droppage(page);
    _pages[pageNo-1].page = NULL;
}

int PdfEngine::pageRotation(int pageNo)
{
    assert(validPageNo(pageNo));
    int rotation = INVALID_ROTATION;
    fz_obj *page = pdf_getpageobject(_xref, pageNo);
	pdf_getpageinfo(_xref, page, NULL, &rotation);
    return rotation;
}

SizeD PdfEngine::pageSize(int pageNo)
{
    assert(validPageNo(pageNo));
    fz_rect bbox;
    fz_obj *page = pdf_getpageobject(_xref, pageNo);
    if (fz_okay != pdf_getpageinfo(_xref, page, &bbox, NULL))
        return SizeD(0,0);
    return SizeD(fabs(bbox.x1 - bbox.x0), fabs(bbox.y1 - bbox.y0));
}

bool PdfEngine::hasPermission(int permission)
{
    assert(_xref);
    if (!_xref || !_xref->crypt)
        return true;
    if (_xref->crypt->p & permission)
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

RenderedBitmap *PdfEngine::renderBitmap(
                           int pageNo, double zoomReal, int rotation,
                           fz_rect *pageRect,
                           BOOL (*abortCheckCbkA)(void *data),
                           void *abortCheckCbkDataA)
{
    fz_error error;
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
    if (!pageRect)
        pageRect = &page->mediabox;
    bbox = fz_transformaabb(ctm, *pageRect);
    error = fz_rendertree(&image, _rast, page->tree, ctm, fz_roundrect(bbox), 1);
#if CONSERVE_MEMORY
    dropPdfPage(pageNo);
#endif
    if (error)
        goto Error;
    ConvertPixmapForWindows(image);
    return new RenderedBitmap(image);
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
int PdfEngine::linkCount() {
    if (!_pages)
        return 0;
    int count = 0;

    for (int i=0; i < _pageCount; i++)
    {
        pdf_page* page = _pages[i].page;
        if (page)
            count += linksLinkCount(page->links);
    }
    return count;
}

void PdfEngine::fillPdfLinks(PdfLink *pdfLinks, int linkCount)
{
    pdf_link *link = 0;
    int pageNo;
    PdfLink *pdfLink = pdfLinks;
    int linkNo = 0;

    for (pageNo=0; pageNo < _pageCount; pageNo++)
    {
        pdf_page* page = _pages[pageNo].page;
        if (!page)
            continue;
        link = page->links;
        while (link)
        {
            assert(link);
            pdfLink->pageNo = pageNo + 1;
            pdfLink->link = link;
            pdfLink->rectPage.x = link->rect.x0;
            pdfLink->rectPage.y = link->rect.y0;
            assert(link->rect.x1 >= link->rect.x0);
            pdfLink->rectPage.dx = link->rect.x1 - link->rect.x0;
            assert(pdfLink->rectPage.dx >= 0);
            assert(link->rect.y1 >= link->rect.y0);
            pdfLink->rectPage.dy = link->rect.y1 - link->rect.y0;
            assert(pdfLink->rectPage.dy >= 0);
            link = link->next;
            linkNo++;
            pdfLink++;
        }
    }
    assert(linkCount == linkNo);
}

void PdfEngine::linkifyPageText(pdf_page *page)
{
    pdf_textline *line;
    TCHAR *pageText = ExtractPageText(page, _T(" "), &line);
    if (!pageText)
        return;

    pdf_textline *current = line;
    for (TCHAR *start = pageText; *start; start++) {
        TCHAR *end;
        fz_rect bbox;

        // look for words starting with "http://", "https://" or "www."
        if (('h' != *start || _tcsncmp(start, _T("http://"), 7) != 0 && _tcsncmp(start, _T("https://"), 8) != 0) &&
            ('w' != *start || _tcsncmp(start, _T("www."), 4) != 0) ||
            (start > pageText && (_istalnum(start[-1]) || '/' == start[-1])))
            continue;

        // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
        for (end = start; !_istspace(*end); end++);
        assert(*end);
        if (',' == *(end - 1) || '.' == *(end - 1))
            end--;
        // also ignore a closing parenthesis, if the URL doesn't contain any opening one
        if (')' == *(end - 1) && (!_tcschr(start, '(') || _tcschr(start, '(') > end))
            end--;
        *end = 0;

        // get positioning information for the matching line
        pdf_textline *ln;
        TCHAR *lineText = pageText;
        for (ln = line; lineText + ln->len < start; ln = ln->next)
            lineText += ln->len + 1;

        // make sure that no other link is associated with this word
        bbox.x0 = ln->text[start - lineText].bbox.x0;
        bbox.y0 = ln->text[start - lineText].bbox.y0;
        bbox.x1 = ln->text[end - lineText - 1].bbox.x1;
        bbox.y1 = ln->text[end - lineText - 1].bbox.y1;
        for (pdf_link *link = page->links; link && start < end; link = link->next) {
            fz_rect isect = fz_intersectrects(bbox, link->rect);
            if (0 != memcmp(&isect, &fz_emptyrect, sizeof(fz_rect)))
                start = end;
        }

        // add the link, if it's a new one (ignoring www. links without a toplevel domain)
        if (*start && (tstr_startswith(start, _T("http")) || _tcschr(start + 5, '.') != NULL)) {
            char *uri = tstr_to_utf8(start);
            char *httpUri = str_startswith(uri, "http") ? uri : str_cat("http://", uri);
            fz_obj *dest = fz_newstring(httpUri, strlen(httpUri));
            pdf_link *link = pdf_newlink(PDF_LURI, bbox, dest);
            link->next = page->links;
            page->links = link;
            fz_dropobj(dest);
            if (httpUri != uri)
                free(httpUri);
            free(uri);
        }
        start = end;
    }
    pdf_droptextline(line);
    free(pageText);
}

TCHAR *PdfEngine::ExtractPageText(pdf_page *page, TCHAR *lineSep, pdf_textline **line_out)
{
    pdf_textline *line;
    fz_error error = pdf_loadtextfromtree(&line, page->tree, fz_identity());
    if (error)
        return NULL;

    int lineSepLen = lstrlen(lineSep);
    int textLen = 0;
    for (pdf_textline *ln = line; ln; ln = ln->next)
        textLen += ln->len + lineSepLen;

    TCHAR *content = (TCHAR *)malloc((textLen + 1) * sizeof(TCHAR)), *dest = content;

    for (pdf_textline *ln = line; ln; ln = ln->next) {
        for (int i = 0; i < ln->len; i++) {
            *dest = ln->text[i].c;
            if (*dest < 32)
                *dest = '?';
            dest++;
        }
        lstrcpy(dest, lineSep);
        dest += lineSepLen;
    }

    if (line_out)
        *line_out = line;
    else
        pdf_droptextline(line);

    return content;
}

char *PdfEngine::getPageLayoutName(void)
{
    return fz_toname(fz_dictgets(_xref->root, "PageLayout"));
}
