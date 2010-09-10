/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
#include <windows.h>
#include "PdfEngine.h"

// in SumatraPDF.cpp
TCHAR *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName, pdf_xref *xref, unsigned char *decryptionKey, bool *saveKey);

// adapted from pdf_page.c's pdf_loadpageinfo
fz_error pdf_getmediabox(fz_rect *mediabox, fz_obj *page)
{
    fz_obj *obj;
    fz_bbox bbox;

    obj = fz_dictgets(page, "MediaBox");
    if (!fz_isarray(obj))
    {
        fz_warn("cannot find page bounds (%d %d R)", fz_tonum(page), fz_togen(page));
        bbox.x0 = 0; bbox.x1 = 612;
        bbox.y0 = 0; bbox.y1 = 792;
    }
    else
        bbox = fz_roundrect(pdf_torect(obj));

    obj = fz_dictgets(page, "CropBox");
    if (fz_isarray(obj))
    {
        fz_bbox cropbox = fz_roundrect(pdf_torect(obj));
        bbox = fz_intersectbbox(bbox, cropbox);
    }

    mediabox->x0 = MIN(bbox.x0, bbox.x1);
    mediabox->y0 = MIN(bbox.y0, bbox.y1);
    mediabox->x1 = MAX(bbox.x0, bbox.x1);
    mediabox->y1 = MAX(bbox.y0, bbox.y1);

    if (mediabox->x1 - mediabox->x0 < 1 || mediabox->y1 - mediabox->y0 < 1)
        return fz_throw("invalid page size");

    return fz_okay;
}

HBITMAP fz_pixtobitmap(HDC hDC, fz_pixmap *pixmap, BOOL paletted)
{
    int w, h, rows8;
    int paletteSize = 0;
    BOOL hasPalette = FALSE;
    int i, j, k;
    BITMAPINFO *bmi;
    HBITMAP hbmp = NULL;
    unsigned char *bmpData, *source, *dest;
    fz_pixmap *bgrPixmap;
    
    w = pixmap->w;
    h = pixmap->h;
    
    /* abgr is a GDI compatible format */
    bgrPixmap = fz_newpixmap(fz_devicebgr, pixmap->x, pixmap->y, w, h);
    fz_convertpixmap(pixmap, bgrPixmap);
    pixmap = bgrPixmap;
    
    assert(pixmap->n == 4);
    
    bmi = (BITMAPINFO *)zmalloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    
    if (paletted)
    {
        rows8 = ((w + 3) / 4) * 4;    
        dest = bmpData = (unsigned char *)malloc(rows8 * h);
        source = pixmap->samples;
        
        for (j = 0; j < h; j++)
        {
            for (i = 0; i < w; i++)
            {
                RGBQUAD c = { 0 };
                
                c.rgbBlue = *source++;
                c.rgbGreen = *source++;
                c.rgbRed = *source++;
                source++;
                
                /* find this color in the palette */
                for (k = 0; k < paletteSize; k++)
                    if (*(int *)&bmi->bmiColors[k] == *(int *)&c)
                        break;
                /* add it to the palette if it isn't in there and if there's still space left */
                if (k == paletteSize)
                {
                    if (k >= 256)
                        goto ProducingPaletteDone;
                    *(int *)&bmi->bmiColors[paletteSize] = *(int *)&c;
                    paletteSize++;
                }
                /* 8-bit data consists of indices into the color palette */
                *dest++ = k;
            }
            dest += rows8 - w;
        }
ProducingPaletteDone:
        hasPalette = paletteSize <= 256;
        if (!hasPalette)
            free(bmpData);
    }
    if (!hasPalette)
        bmpData = pixmap->samples;
    
    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = w;
    bmi->bmiHeader.biHeight = -h;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biBitCount = hasPalette ? 8 : 32;
    bmi->bmiHeader.biSizeImage = h * (hasPalette ? rows8 : w * 4);
    bmi->bmiHeader.biClrUsed = hasPalette ? paletteSize : 0;
    
    hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT, bmpData, bmi, DIB_RGB_COLORS);
    
    fz_droppixmap(bgrPixmap);
    if (hasPalette)
        free(bmpData);
    free(bmi);
    
    return hbmp;
}

void fz_pixmaptodc(HDC hDC, fz_pixmap *pixmap, fz_rect *dest)
{
    // Try to extract a 256 color palette so that we can use 8-bit images instead of 24-bit ones.
    // This should e.g. speed up printing for most monochrome documents by saving spool resources.
    HBITMAP hbmp = fz_pixtobitmap(hDC, pixmap, TRUE);
    HDC bmpDC = CreateCompatibleDC(hDC);
    
    SelectObject(bmpDC, hbmp);
    if (!dest)
        BitBlt(hDC, 0, 0, pixmap->w, pixmap->h, bmpDC, 0, 0, SRCCOPY);
    else if (dest->x1 - dest->x0 == pixmap->w && dest->y1 - dest->y0 == pixmap->h)
        BitBlt(hDC, dest->x0, dest->y0, pixmap->w, pixmap->h, bmpDC, 0, 0, SRCCOPY);
    else
        StretchBlt(hDC, dest->x0, dest->y0, dest->x1 - dest->x0, dest->y1 - dest->y0,
            bmpDC, 0, 0, pixmap->w, pixmap->h, SRCCOPY);
    
    DeleteDC(bmpDC);
    DeleteObject(hbmp);
}

pdf_outline *pdf_newoutline(char *title, fz_obj *dest)
{
    pdf_outline *node = (pdf_outline *)zmalloc(sizeof(pdf_outline));
    node->title = title;

    fz_obj *type = fz_dictgets(dest, "Type");
    if (fz_isname(type) && !strcmp(fz_toname(type), "Filespec")) {
        node->link = (pdf_link *)zmalloc(sizeof(pdf_link));
        node->link->kind = PDF_LLAUNCH;
        node->link->dest = fz_keepobj(dest);
    }

    return node;
}

void pdf_loadattachmentsimp(pdf_xref *xref, pdf_outline *current, fz_obj *node)
{
    fz_obj *kids = fz_dictgets(node, "Kids");
    fz_obj *names = fz_dictgets(node, "Names");

    if (!names && !kids)
        fz_warn("Ignoring name tree node without names nor kids (%d %d R)", fz_tonum(node), fz_togen(node));

    if (fz_isarray(kids))
        for (int i = 0; i < fz_arraylen(kids); i++)
            pdf_loadattachmentsimp(xref, current, fz_arrayget(kids, i));

    if (fz_isarray(names)) {
        for (int i = 0; i < fz_arraylen(names) - 1; i += 2) {
            fz_obj *name = fz_arrayget(names, i);
            fz_obj *dest = fz_arrayget(names, i + 1);

            current = current->next = pdf_newoutline(pdf_toutf8(name), dest);
        }
    }
}

pdf_outline *pdf_loadattachments(pdf_xref *xref)
{
    fz_obj *names = fz_dictgets(fz_dictgets(xref->trailer, "Root"), "Names");
    fz_obj *obj = fz_dictgets(names, "EmbeddedFiles");
    if (!obj)
        return NULL;

    pdf_outline *root = pdf_newoutline(NULL, NULL);
    pdf_loadattachmentsimp(xref, root, obj);

    pdf_outline *first = root->next;
    root->next = NULL;
    pdf_freeoutline(root);

    return first;
}

void pdf_streamfingerprint(fz_stream *file, unsigned char *digest)
{
    fz_seek(file, 0, 2);
    int fileLen = fz_tell(file);

    fz_buffer *buffer;
    fz_seek(file, 0, 0);
    fz_readall(&buffer, file, fileLen);
    assert(fileLen == buffer->len);

    fz_md5 md5;
    fz_md5init(&md5);
    fz_md5update(&md5, buffer->data, buffer->len);
    fz_md5final(&md5, digest);

    fz_dropbuffer(buffer);
}


HBITMAP RenderedBitmap::createDIBitmap(HDC hdc) {
    if (!_pixmap)
        // clone the bitmap (callers will delete it)
        return (HBITMAP)CopyImage(_hbmp, IMAGE_BITMAP, _width, _height, 0);

    return fz_pixtobitmap(hdc, _pixmap, FALSE);
}

void RenderedBitmap::stretchDIBits(HDC hdc, int leftMargin, int topMargin, int pageDx, int pageDy) {
    if (!_pixmap) {
        HDC bmpDC = CreateCompatibleDC(hdc);
        HGDIOBJ oldBmp = SelectObject(bmpDC, _hbmp);
        SetStretchBltMode(hdc, HALFTONE);
        StretchBlt(hdc, leftMargin, topMargin, pageDx, pageDy,
            bmpDC, 0, 0, _width, _height, SRCCOPY);
        SelectObject(bmpDC, oldBmp);
        DeleteDC(bmpDC);
        return;
    }

    fz_rect dest = { leftMargin, topMargin, leftMargin + pageDx, topMargin + pageDy };
    fz_pixmaptodc(hdc, _pixmap, &dest);
}

void RenderedBitmap::invertColors() {
    if (!_pixmap) {
        HDC hDC = GetDC(NULL);
        HDC bmpDC = CreateCompatibleDC(hDC);
        HGDIOBJ oldBmp = SelectObject(bmpDC, _hbmp);
        BITMAPINFO bmi = { 0 };

        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biHeight = _height;
        bmi.bmiHeader.biWidth = _width;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        unsigned char *bmpData = (unsigned char *)malloc(_width * _height * 4);
        if (GetDIBits(bmpDC, _hbmp, 0, _height, bmpData, &bmi, DIB_RGB_COLORS)) {
            int dataLen = _width * _height * 4;
            for (int i = 0; i < dataLen; i++)
                if ((i + 1) % 4) // don't invert alpha channel
                    bmpData[i] = 255 - bmpData[i];
            SetDIBits(bmpDC, _hbmp, 0, _height, bmpData, &bmi, DIB_RGB_COLORS);
        }

        free(bmpData);
        SelectObject(bmpDC, oldBmp);
        DeleteDC(bmpDC);
        ReleaseDC(NULL, hDC);
        return;
    }

    unsigned char *bmpData = _pixmap->samples;
    int dataLen = _width * _height * _pixmap->n;
    for (int i = 0; i < dataLen; i++)
        if ((i + 1) % _pixmap->n) // don't invert alpha channel
            bmpData[i] = 255 - bmpData[i];
}

PdfEngine::PdfEngine() : 
        _fileName(NULL)
        , _pageCount(INVALID_PAGE_NO) 
        , _xref(NULL)
        , _outline(NULL)
        , _attachments(NULL)
        , _pages(NULL)
        , _drawcache(NULL)
        , _windowInfo(NULL)
        , _decryptionKey(NULL)
{
    InitializeCriticalSection(&_pagesAccess);
    InitializeCriticalSection(&_xrefAccess);
}

PdfEngine::~PdfEngine()
{
    EnterCriticalSection(&_pagesAccess);
    if (_pages) {
        for (int i=0; i < _pageCount; i++) {
            if (_pages[i])
                pdf_freepage(_pages[i]);
        }
        free(_pages);
    }
    LeaveCriticalSection(&_pagesAccess);
    DeleteCriticalSection(&_pagesAccess);

    if (_outline)
        pdf_freeoutline(_outline);
    if (_attachments)
        pdf_freeoutline(_attachments);

    EnterCriticalSection(&_xrefAccess);
    if (_xref) {
        if (_xref->store) {
            pdf_freestore(_xref->store);
            _xref->store = NULL;
        }
        pdf_freexref(_xref);
    }
    LeaveCriticalSection(&_xrefAccess);
    DeleteCriticalSection(&_xrefAccess);

    if (_drawcache)
        fz_freeglyphcache(_drawcache);
    free((void*)_fileName);
    free((void*)_decryptionKey);
}

bool PdfEngine::load(const TCHAR *fileName, WindowInfo *win, bool tryrepair)
{
    fz_error error;
    _windowInfo = win;
    assert(!_fileName);
    _fileName = tstr_dup(fileName);

    // File names ending in :<digits>:<digits> are interpreted as containing
    // embedded PDF documents (the digits are :<num>:<gen> of the embedded file stream)
    TCHAR *embedMarks = NULL;
    int colonCount = 0;
    for (TCHAR *c = (TCHAR *)_fileName + tstr_len(_fileName); c > _fileName; c--) {
        if (*c != ':')
            continue;
        if (!_istdigit(*(c + 1)))
            break;
        if (++colonCount % 2 == 0)
            embedMarks = c;
    }

    if (embedMarks)
        *embedMarks = '\0';
    int fd = _topen(_fileName, O_BINARY | O_RDONLY);
    if (embedMarks)
        *embedMarks = ':';

    if (-1 == fd)
        return false;
    fz_stream *file = fz_openfile(fd);
OpenEmbeddedFile:
    // TODO: not sure how to handle passwords
    error = pdf_openxrefwithstream(&_xref, file, NULL);
    fz_close(file);
    if (error || !_xref)
        return false;

    if (pdf_needspassword(_xref)) {
        if (!win)
            // win might not be given if called from pdfbench.cc
            return false;

        unsigned char digest[16 + 32] = { 0 };
        pdf_streamfingerprint(_xref->file, digest);

        bool okay = false, saveKey = false;
        for (int i = 0; !okay && i < 3; i++) {
            TCHAR *pwd = GetPasswordForFile(win, _fileName, _xref, digest, &saveKey);
            if (!pwd) {
                // password not given or encryption key has been remembered
                okay = saveKey;
                break;
            }

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

        if (saveKey) {
            memcpy(digest + 16, _xref->crypt->key, 32);
            _decryptionKey = mem_to_hexstr(digest, sizeof(digest));
        }
    }

    if (embedMarks && *embedMarks) {
        int num = _ttoi(embedMarks + 1); embedMarks = _tcschr(embedMarks + 1, ':');
        int gen = _ttoi(embedMarks + 1); embedMarks = _tcschr(embedMarks + 1, ':');
        if (!pdf_isstream(_xref, num, gen))
            return false;

        fz_buffer *buffer;
        error = pdf_loadstream(&buffer, _xref, num, gen);
        if (error)
            return false;
        file = fz_openbuffer(buffer);
        fz_dropbuffer(buffer);

        pdf_freexref(_xref);
        _xref = NULL;
        goto OpenEmbeddedFile;
    }

    error = pdf_loadpagetree(_xref);
    if (error)
        return false;

    EnterCriticalSection(&_xrefAccess);
    _pageCount = pdf_getpagecount(_xref);
    _outline = pdf_loadoutline(_xref);
    // silently ignore errors from pdf_loadoutline()
    // this information is not critical and checking the
    // error might prevent loading some pdfs that would
    // otherwise get displayed
    _attachments = pdf_loadattachments(_xref);
    LeaveCriticalSection(&_xrefAccess);

    _pages = (pdf_page **)calloc(_pageCount, sizeof(pdf_page *));
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
    PdfTocItem *node = NULL;

    if (_outline) {
        node = buildTocTree(_outline);
        if (_attachments)
            node->AddSibling(buildTocTree(_attachments));
    }
    else if (_attachments)
        node = buildTocTree(_attachments);

    return node;
}

int PdfEngine::findPageNo(fz_obj *dest)
{
    if (fz_isdict(dest)) {
        // The destination is linked from a Go-To action's D array
        fz_obj * D = fz_dictgets(dest, "D");
        if (D && fz_isarray(D))
            dest = D;
    }
    if (fz_isarray(dest))
        dest = fz_arrayget(dest, 0);
    if (fz_isint(dest))
        return fz_toint(dest) + 1;

    return pdf_findpageobject(_xref, dest);
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

    bool needsLinkification = false;

    EnterCriticalSection(&_pagesAccess);
    pdf_page *page = _pages[pageNo-1];
    if (!page) {
        EnterCriticalSection(&_xrefAccess);
        fz_obj * obj = pdf_getpageobject(_xref, pageNo);
        fz_error error = pdf_loadpage(&page, _xref, obj);
        if (!error) {
            _pages[pageNo-1] = page;
            needsLinkification = true;
        }
        LeaveCriticalSection(&_xrefAccess);
    }
    LeaveCriticalSection(&_pagesAccess);

    if (needsLinkification)
        linkifyPageText(page);
    return page;
}

void PdfEngine::dropPdfPage(int pageNo)
{
    assert(_pages);
    if (!_pages) return;
    EnterCriticalSection(&_pagesAccess);
    pdf_page *page = _pages[pageNo-1];
    assert(page);
    if (page) {
        pdf_freepage(page);
        _pages[pageNo-1] = NULL;
    }
    LeaveCriticalSection(&_pagesAccess);
}

int PdfEngine::pageRotation(int pageNo)
{
    assert(validPageNo(pageNo));
    fz_obj *page = pdf_getpageobject(_xref, pageNo);
    if (!page)
        return INVALID_ROTATION;
    return fz_toint(fz_dictgets(page, "Rotate"));
}

SizeD PdfEngine::pageSize(int pageNo)
{
    assert(validPageNo(pageNo));
    fz_obj *page = pdf_getpageobject(_xref, pageNo);
    fz_rect bbox;
    if (!page || pdf_getmediabox(&bbox, pdf_getpageobject(_xref, pageNo)) != fz_okay)
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

fz_matrix PdfEngine::viewctm(pdf_page *page, float zoom, int rotate)
{
    fz_matrix ctm = fz_identity;
    ctm = fz_concat(ctm, fz_translate(-page->mediabox.x0, -page->mediabox.y1));
    ctm = fz_concat(ctm, fz_scale(zoom, -zoom));
    ctm = fz_concat(ctm, fz_rotate(rotate + page->rotate));
    return ctm;
}

bool PdfEngine::renderPage(HDC hDC, pdf_page *page, RECT *screenRect, fz_matrix *ctm, double zoomReal, int rotation, fz_rect *pageRect)
{
    fz_matrix ctm2;
    if (!ctm) {
        float zoom = zoomReal / 100.0;
        if (!zoom)
            zoom = min(1.0 * (screenRect->right - screenRect->left) / (page->mediabox.x1 - page->mediabox.x0),
                       1.0 * (screenRect->bottom - screenRect->top) / (page->mediabox.y1 - page->mediabox.y0));
        ctm2 = viewctm(page, zoom, rotation);
        fz_bbox bbox = fz_roundrect(fz_transformrect(ctm2, pageRect ? *pageRect : page->mediabox));
        ctm2 = fz_concat(ctm2, fz_translate(screenRect->left - bbox.x0, screenRect->top - bbox.y0));
        ctm = &ctm2;
    }

    HRGN clip = CreateRectRgnIndirect(screenRect);
    SelectClipRgn(hDC, clip);

    HBRUSH bgBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
    FillRect(hDC, screenRect, bgBrush); // initialize white background
    DeleteObject(bgBrush);

    fz_device *dev = fz_newgdiplusdevice(hDC);
    EnterCriticalSection(&_xrefAccess);
    fz_error error = pdf_runpage(_xref, page, dev, *ctm);
    LeaveCriticalSection(&_xrefAccess);
    fz_freedevice(dev);

    SelectClipRgn(hDC, NULL);
    DeleteObject(clip);

    return fz_okay == error;
}

RenderedBitmap *PdfEngine::renderBitmap(
                           int pageNo, double zoomReal, int rotation,
                           fz_rect *pageRect,
                           BOOL (*abortCheckCbkA)(void *data),
                           void *abortCheckCbkDataA,
                           bool useGdi)
{
    pdf_page* page = getPdfPage(pageNo);
    if (!page)
        return NULL;
    zoomReal = zoomReal / 100.0;
    fz_matrix ctm = viewctm(page, zoomReal, rotation);
    if (!pageRect)
        pageRect = &page->mediabox;
    fz_bbox bbox = fz_roundrect(fz_transformrect(ctm, *pageRect));

    if (useGdi) {
        int w = bbox.x1 - bbox.x0, h = bbox.y1 - bbox.y0;
        ctm = fz_concat(ctm, fz_translate(-bbox.x0, -bbox.y0));

        // for now, don't render directly into a DC but produce an HBITMAP instead
        HDC hDC = GetDC(NULL);
        HDC hDCMem = CreateCompatibleDC(hDC);
        HBITMAP hbmp = CreateCompatibleBitmap(hDC, w, h);
        DeleteObject(SelectObject(hDCMem, hbmp));

        RECT rc = { 0, 0, w, h };
        bool success = renderPage(hDCMem, page, &rc, &ctm);
#if CONSERVE_MEMORY
        dropPdfPage(pageNo);
#endif
        DeleteDC(hDCMem);
        ReleaseDC(NULL, hDC);
        if (!success) {
            DeleteObject(hbmp);
            return NULL;
        }
        return new RenderedBitmap(hbmp, w, h);
    }

    // make sure not to request too large a pixmap, as MuPDF just aborts on OOM;
    // instead we get a 1*y sized pixmap and try to resize it manually and just
    // fail to render if we run out of memory.
    fz_pixmap *image = fz_newpixmap(fz_devicergb, bbox.x0, bbox.y0, 1, bbox.y1 - bbox.y0);
    image->w = bbox.x1 - bbox.x0;
    free(image->samples);
    image->samples = (unsigned char *)malloc(image->w * image->h * image->n);
    if (!image->samples)
        return NULL;

    fz_clearpixmap(image, 0xFF); // initialize white background
    if (!_drawcache)
        _drawcache = fz_newglyphcache();
    fz_device *dev = fz_newdrawdevice(_drawcache, image);
    EnterCriticalSection(&_xrefAccess);
    fz_error error = pdf_runpage(_xref, page, dev, ctm);
    LeaveCriticalSection(&_xrefAccess);
    fz_freedevice(dev);
#if CONSERVE_MEMORY
    dropPdfPage(pageNo);
#endif
    RenderedBitmap *bitmap = NULL;
    if (!error)
        bitmap = new RenderedBitmap(image);
    fz_droppixmap(image);
    return bitmap;
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
        pdf_page *page = _pages[i];
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
        pdf_page *page = _pages[pageNo];
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
            pdf_link *link = (pdf_link *)zmalloc(sizeof(pdf_link));
            link->kind = PDF_LURI;
            link->rect = bbox;
            link->dest = dest;
            link->next = page->links;
            page->links = link;
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
    EnterCriticalSection(&_xrefAccess);
    fz_error error = pdf_runpage(_xref, page, dev, fz_identity);
    LeaveCriticalSection(&_xrefAccess);
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
                *destRect++ = span->text[i].bbox;
        }
        if (!span->eol)
            continue;
#ifdef UNICODE
        lstrcpy(dest, lineSep);
        dest += lineSepLen;
#else
        dest += MultiByteToWideChar(CP_ACP, 0, lineSep, -1, dest, lineSepLen + 1);
#endif
        if (destRect) {
            ZeroMemory(destRect, lineSepLen * sizeof(fz_bbox));
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

fz_buffer *PdfEngine::getStreamData(int num, int gen)
{
    fz_stream *stream = NULL;
    fz_buffer *data = NULL;

    if (num)
        pdf_openstream(&stream, _xref, num, gen);
    else
        stream = fz_keepstream(_xref->file);

    if (stream) {
        fz_seek(stream, 0, 0);
        fz_readall(&data, stream, 1024);
        fz_close(stream);
    }

    return data;
}
