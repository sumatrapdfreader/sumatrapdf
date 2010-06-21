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

    obj = fz_dictgets(dict, "MediaBox");
    if (!fz_isarray(obj))
        return fz_throw("syntaxerror: Page missing MediaBox");
    bbox = pdf_torect(obj);

    obj = fz_dictgets(dict, "CropBox");
    if (obj && fz_isarray(obj)) {
        // crop MediaBox to CropBox
        fz_rect cropbox = pdf_torect(obj);
        bbox.x0 = MAX(bbox.x0, cropbox.x0);
        bbox.x1 = MIN(bbox.x1, cropbox.x1);
        bbox.y0 = MAX(bbox.y0, cropbox.y0);
        bbox.y1 = MIN(bbox.y1, cropbox.y1);
    }

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
    // Try to extract a 256 color palette so that we can print 8-bit images instead of 24-bit ones.
    // This should speed up printing for most monochrome documents by saving spool resources.
    int bmpDx = bmp->dx();
    int bmpDy = bmp->dy();
    int bmpRowSize24 = bmp->rowSize();
    int bmpRowSize8 = ((bmpDx + 3) / 4) * 4;

    BITMAPINFO *bmi = (BITMAPINFO *)malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    memset(bmi, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    unsigned char *bmpData24 = bmp->data();
    unsigned char *bmpData8 = (unsigned char *)malloc(bmpRowSize8 * bmpDy);
    int paletteSize = 0;

    bool hasPalette = false;
    for (int j = 0; j < bmpDy; j++) {
        for (int i = 0; i < bmpDx; i++) {
            RGBQUAD c = { 0 };
            int k;

            c.rgbRed = bmpData24[j * bmpRowSize24 + i * 3 + 2];
            c.rgbGreen = bmpData24[j * bmpRowSize24 + i * 3 + 1];
            c.rgbBlue = bmpData24[j * bmpRowSize24 + i * 3];

            // find this color in the palette
            for (k = 0; k < paletteSize; k++)
                if (*(int32_t *)&bmi->bmiColors[k] == *(int32_t *)&c)
                    break;
            // add it to the palette if it isn't in there and if there's still space left
            if (k == paletteSize) {
                if (k >= 256)
                    goto ProducingPaletteDone;
                *(int32_t *)&bmi->bmiColors[paletteSize] = *(int32_t *)&c;
                paletteSize++;
            }
            // 8-bit data consists of indices into the color palette
            bmpData8[j * bmpRowSize8 + i] = k;
        }
    }
    // TODO: convert images to grayscale for monochrome printers, so that we always have an 8-bit palette?
    hasPalette = paletteSize <= 256;

ProducingPaletteDone:
    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = bmpDx;
    bmi->bmiHeader.biHeight = -bmpDy;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biBitCount = hasPalette ? 8 : 24;
    bmi->bmiHeader.biSizeImage = bmpDy * (hasPalette ? bmpRowSize8 : bmpRowSize24);
    bmi->bmiHeader.biClrUsed = hasPalette ? paletteSize : 0;

    ::StretchDIBits(hdc,
        // destination rectangle
        leftMargin, topMargin, pageDx, pageDy,
        // source rectangle
        0, 0, bmpDx, bmpDy,
        hasPalette ? bmpData8 : bmpData24, bmi,
        DIB_RGB_COLORS,
        SRCCOPY);

    free(bmpData8);
    free(bmi);
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
        _fileName(NULL)
        , _pageCount(INVALID_PAGE_NO) 
        , _xref(NULL)
        , _outline(NULL)
        , _pages(NULL)
        , _drawcache(NULL)
{
    _getPageSem = CreateSemaphore(NULL, 1, 1, NULL);
}

PdfEngine::~PdfEngine()
{
    if (_pages) {
        for (int i=0; i < _pageCount; i++) {
            if (_pages[i].page)
                pdf_freepage(_pages[i].page);
        }
        free(_pages);
    }

    if (_outline)
        pdf_freeoutline(_outline);
    if (_xref) {
        if (_xref->store) {
            pdf_freestore(_xref->store);
            _xref->store = NULL;
        }
        pdf_closexref(_xref);
    }
    if (_drawcache)
        fz_freeglyphcache(_drawcache);

    CloseHandle(_getPageSem);

    free((void*)_fileName);
}

bool PdfEngine::load(const TCHAR *fileName, WindowInfo *win, bool tryrepair)
{
    _windowInfo = win;
    setFileName(fileName);

    int fd = _topen(fileName, O_BINARY | O_RDONLY);
    if (-1 == fd)
        return false;
    fz_stream *file = fz_openfile(fd);
    _xref = pdf_openxref(file);
    fz_dropstream(file);
    if (!_xref)
        return false;

    if (pdf_needspassword(_xref)) {
        bool okay = !!pdf_authenticatepassword(_xref, "");
        if (!okay && !win)
            // win might not be given if called from pdfbench.cc
            return false;

        for (int i = 0; !okay && i < 3; i++) {
            TCHAR *pwd = GetPasswordForFile(win, fileName);
            if (!pwd)
                // password not given
                return false;

            char *pwd_utf8 = tstr_to_utf8(pwd);
            char *pwd_ansi = tstr_to_multibyte(pwd, CP_ACP);
            if (pwd_utf8)
                okay = !!pdf_authenticatepassword(_xref, pwd_utf8);
            // for some documents, only the ANSI-encoded password works
            if (!okay && pwd_ansi)
                okay = !!pdf_authenticatepassword(_xref, pwd_ansi);

            free(pwd_utf8);
            free(pwd_ansi);
            free(pwd);
        }
        if (!okay)
            return false;
    }

    if (pdf_loadpagetree(_xref) != fz_okay)
        return false;
    _pageCount = pdf_getpagecount(_xref);
    _outline = pdf_loadoutline(_xref);
    // silently ignore errors from pdf_loadoutline()
    // this information is not critical and checking the
    // error might prevent loading some pdfs that would
    // otherwise get displayed

    _pages = (PdfPage *)calloc(_pageCount, sizeof(PdfPage));
    return _pageCount > 0;
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
            dest = D;
    }
    if (fz_isarray(dest))
        dest = fz_arrayget(dest, 0);

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
    pdf_freepage(page);
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
    pdf_page* page = getPdfPage(pageNo);
    if (!page)
        return NULL;
    zoomReal = zoomReal / 100.0;
    fz_matrix ctm = viewctm(page, zoomReal, rotation);
    if (!pageRect)
        pageRect = &page->mediabox;
    fz_bbox bbox = fz_roundrect(fz_transformrect(ctm, *pageRect));

    // make sure not to request too large a pixmap, as MuPDF just aborts on OOM;
    // instead we get a 1*y sized pixmap and try to resize it manually and just
    // fail to render if we run out of memory.
    fz_pixmap *image = fz_newpixmap(pdf_devicergb, bbox.x0, bbox.y0, 1, bbox.y1 - bbox.y0);
    image->w = bbox.x1 - bbox.x0;
    free(image->samples);
    image->samples = (unsigned char *)malloc(image->w * image->h * image->n);
    if (!image->samples)
        return NULL;

    fz_clearpixmap(image, 0xFF); // initialize white background
    if (!_drawcache)
        _drawcache = fz_newglyphcache();
    fz_device *dev = fz_newdrawdevice(_drawcache, image);
    fz_error error = pdf_runcontentstream(dev, ctm, _xref, page->resources, page->contents);
    fz_freedevice(dev);
#if CONSERVE_MEMORY
    dropPdfPage(pageNo);
#endif
    if (error) {
        fz_droppixmap(image);
        return NULL;
    }
    ConvertPixmapForWindows(image);
    return new RenderedBitmap(image);
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
    float tmp;

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

            // there are PDFs that have x,y positions in reverse order, so
            // fix them up
            if (link->rect.x0 > link->rect.x1) {
                tmp = link->rect.x0;
                link->rect.x0 = link->rect.x1;
                link->rect.x1 = tmp;
            }
            assert(link->rect.x1 >= link->rect.x0);
            pdfLink->rectPage.dx = link->rect.x1 - link->rect.x0;
            assert(pdfLink->rectPage.dx >= 0);

            if (link->rect.y0 > link->rect.y1) {
                tmp = link->rect.y0;
                link->rect.y0 = link->rect.y1;
                link->rect.y1 = tmp;
            }
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
    fz_bbox *coords;
    TCHAR *pageText = ExtractPageText(page, _T(" "), &coords);
    if (!pageText)
        return;

    pdf_link *firstLink = page->links;
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

        // make sure that no other link is associated with this area
        bbox.x0 = coords[start - pageText].x0;
        bbox.y0 = coords[start - pageText].y0;
        bbox.x1 = coords[end - pageText - 1].x1;
        bbox.y1 = coords[end - pageText - 1].y1;
        for (pdf_link *link = firstLink; link && *start; link = link->next) {
            fz_bbox isect = fz_intersectbbox(fz_roundrect(bbox), fz_roundrect(link->rect));
            if ((isect.x1 - isect.x0) != 0 && (isect.y1 - isect.y0) != 0)
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
    free(coords);
    free(pageText);
}

TCHAR *PdfEngine::ExtractPageText(pdf_page *page, TCHAR *lineSep, fz_bbox **coords_out)
{
    fz_textspan *text = fz_newtextspan();
    fz_device *dev = fz_newtextdevice(text);
    fz_error error = pdf_runcontentstream(dev, fz_identity(), _xref, page->resources, page->contents);
    fz_freedevice(dev);
    if (error) {
        fz_freetextspan(text);
        return NULL;
    }

    int lineSepLen = lstrlen(lineSep);
    int textLen = 0;
    for (fz_textspan *span = text; span; span = span->next)
        textLen += span->len + lineSepLen;

    WCHAR *content = (WCHAR *)malloc((textLen + 1) * sizeof(WCHAR)), *dest = content;
    if (!content)
        return NULL;
    fz_bbox *destRect = NULL;
    if (coords_out)
        destRect = *coords_out = (fz_bbox *)malloc(textLen * sizeof(fz_bbox));

    for (fz_textspan *span = text; span; span = span->next) {
        for (int i = 0; i < span->len; i++) {
            *dest = span->text[i].c;
            if (*dest < 32)
                *dest = '?';
            dest++;
            if (destRect)
                memcpy(destRect++, &span->text[i].bbox, sizeof(fz_bbox));
        }
#ifdef UNICODE
        lstrcpy(dest, lineSep);
        dest += lineSepLen;
#else
        dest += MultiByteToWideChar(CP_ACP, 0, lineSep, -1, dest, lineSepLen + 1);
#endif
        if (destRect) {
            memset(destRect, 0, lineSepLen * sizeof(fz_bbox));
            destRect += lineSepLen;
        }
    }

    fz_freetextspan(text);

#ifdef UNICODE
    return content;
#else
    TCHAR *contentT = wstr_to_tstr(content);
    free(content);
    return contentT;
#endif
}

char *PdfEngine::getPageLayoutName(void)
{
    return fz_toname(fz_dictgets(fz_dictgets(_xref->trailer, "Root"), "PageLayout"));
}
