/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */
#include "PdfEngine.h"

// in SumatraPDF.cpp
TCHAR *GetPasswordForFile(WindowInfo *win, const TCHAR *fileName, pdf_xref *xref, unsigned char *decryptionKey, bool *saveKey);

// adapted from pdf_page.c's pdf_loadpageinfo
fz_error pdf_getmediabox(fz_rect *mediabox, fz_obj *page)
{
    fz_obj *obj = fz_dictgets(page, "MediaBox");
    fz_bbox bbox = fz_roundrect(pdf_torect(obj));
    if (fz_isemptyrect(pdf_torect(obj)))
    {
        fz_warn("cannot find page bounds, guessing page bounds.");
        bbox.x0 = 0;
        bbox.y0 = 0;
        bbox.x1 = 612;
        bbox.y1 = 792;
    }

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

fz_error pdf_runpagefortarget(pdf_xref *xref, pdf_page *page, fz_device *dev, fz_matrix ctm, RenderTarget target)
{
    fz_obj *targetName = fz_newname(
        target == Target_View ? "View" :
        target == Target_Print ? "Print" :
        target == Target_Export ? "Export" :
        "<unknown>"
    );
    fz_dictputs(xref->trailer, "_MuPDF_OCG_Usage", targetName);

    fz_error error = pdf_runpage(xref, page, dev, ctm);

    fz_dictdels(xref->trailer, "_MuPDF_OCG_Usage");
    fz_dropobj(targetName);

    return error;
}

fz_pixmap *fz_newpixmap_nullonoom(fz_colorspace *colorspace, int x, int y, int w, int h)
{
    // allocate the memory needed for the pixmap ourselves, as MuPDF just aborts on OOM
    unsigned char *samples = (unsigned char *)malloc(w * h * (colorspace->n + 1));
    if (!samples)
        return NULL;

    fz_pixmap *image = fz_newpixmapwithdata(colorspace, x, y, w, h, samples);
    // tell MuPDF to free the memory when discarding the pixmap
    image->freesamples = 1;

    return image;
}

HBITMAP fz_pixtobitmap(HDC hDC, fz_pixmap *pixmap, BOOL paletted)
{
    int w, h, rows8;
    int paletteSize = 0;
    BOOL hasPalette = FALSE;
    int i, j, k;
    BITMAPINFO *bmi;
    HBITMAP hbmp = NULL;
    unsigned char *bmpData = NULL, *source, *dest;
    fz_pixmap *bgrPixmap;
    
    w = pixmap->w;
    h = pixmap->h;
    
    /* abgr is a GDI compatible format */
    bgrPixmap = fz_newpixmap_nullonoom(fz_devicebgr, pixmap->x, pixmap->y, w, h);
    if (!bgrPixmap)
        return NULL;
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
        hasPalette = paletteSize < 256;
    }
    
    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = w;
    bmi->bmiHeader.biHeight = -h;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biBitCount = hasPalette ? 8 : 32;
    bmi->bmiHeader.biSizeImage = h * (hasPalette ? rows8 : w * 4);
    bmi->bmiHeader.biClrUsed = hasPalette ? paletteSize : 0;
    
    if (!hasPalette)
    {
        VOID *dibData;
        hbmp = CreateDIBSection(hDC, bmi, DIB_RGB_COLORS, &dibData, NULL, 0);
        memcpy(dibData, pixmap->samples, bmi->bmiHeader.biSizeImage);
        GdiFlush();
    }
    else
        hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT, bmpData, bmi, DIB_RGB_COLORS);
    
    fz_droppixmap(bgrPixmap);
    free(bmi);
    if (bmpData)
        free(bmpData);
    
    return hbmp;
}

pdf_outline *pdf_loadattachments(pdf_xref *xref)
{
    fz_obj *dict = pdf_loadnametree(xref, "EmbeddedFiles");
    if (!dict)
        return NULL;

    pdf_outline root = { 0 }, *node = &root;
    for (int i = 0; i < fz_dictlen(dict); i++) {
        node = node->next = (pdf_outline *)zmalloc(sizeof(pdf_outline));

        fz_obj *name = fz_dictgetkey(dict, i);
        fz_obj *dest = fz_dictgetval(dict, i);
        fz_obj *type = fz_dictgets(dest, "Type");

        node->title = strdup(fz_toname(name));
        if (fz_isname(type) && !strcmp(fz_toname(type), "Filespec")) {
            node->link = (pdf_link *)zmalloc(sizeof(pdf_link));
            node->link->kind = PDF_LLAUNCH;
            node->link->dest = fz_keepobj(dest);
        }
    }
    fz_dropobj(dict);

    return root.next;
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

bool fz_isptinrect(fz_rect rect, double x, double y)
{
    return MIN(rect.x0, rect.x1) <= x && MAX(rect.x0, rect.x1) >= x &&
           MIN(rect.y0, rect.y1) <= y && MAX(rect.y0, rect.y1) >= y;
}

RenderedBitmap::RenderedBitmap(fz_pixmap *pixmap, HDC hDC) :
    _hbmp(fz_pixtobitmap(hDC, pixmap, TRUE)),
    _width(pixmap->w), _height(pixmap->h), outOfDate(false) { }

void RenderedBitmap::stretchDIBits(HDC hdc, int leftMargin, int topMargin, int pageDx, int pageDy) {
    HDC bmpDC = CreateCompatibleDC(hdc);
    HGDIOBJ oldBmp = SelectObject(bmpDC, _hbmp);
    SetStretchBltMode(hdc, HALFTONE);
    StretchBlt(hdc, leftMargin, topMargin, pageDx, pageDy,
        bmpDC, 0, 0, _width, _height, SRCCOPY);
    SelectObject(bmpDC, oldBmp);
    DeleteDC(bmpDC);
}

void RenderedBitmap::grayOut(float alpha) {
    HDC hDC = GetDC(NULL);
    BITMAPINFO bmi = { 0 };

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biHeight = _height;
    bmi.bmiHeader.biWidth = _width;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    unsigned char *bmpData = (unsigned char *)malloc(_width * _height * 4);
    if (GetDIBits(hDC, _hbmp, 0, _height, bmpData, &bmi, DIB_RGB_COLORS)) {
        int dataLen = _width * _height * 4;
        for (int i = 0; i < dataLen; i++)
            if ((i + 1) % 4) // don't affect the alpha channel
                bmpData[i] = bmpData[i] * alpha + (alpha > 0 ? 0 : 255);
        SetDIBits(hDC, _hbmp, 0, _height, bmpData, &bmi, DIB_RGB_COLORS);
    }

    free(bmpData);
    ReleaseDC(NULL, hDC);
}

PdfEngine::PdfEngine() : 
        _fileName(NULL)
        , _pageCount(INVALID_PAGE_NO) 
        , _xref(NULL)
        , _outline(NULL)
        , _attachments(NULL)
        , _info(NULL)
        , _pages(NULL)
        , _drawcache(NULL)
        , _decryptionKey(NULL)
{
    InitializeCriticalSection(&_pagesAccess);
    InitializeCriticalSection(&_xrefAccess);
    ZeroMemory(&_runCache, sizeof(_runCache));
}

PdfEngine::~PdfEngine()
{
    EnterCriticalSection(&_pagesAccess);
    EnterCriticalSection(&_xrefAccess);

    if (_pages) {
        for (int i=0; i < _pageCount; i++) {
            if (_pages[i])
                pdf_freepage(_pages[i]);
        }
        free(_pages);
    }

    if (_outline)
        pdf_freeoutline(_outline);
    if (_attachments)
        pdf_freeoutline(_attachments);
    if (_info)
        fz_dropobj(_info);

    if (_xref) {
        pdf_freexref(_xref);
        _xref = NULL;
    }

    if (_drawcache)
        fz_freeglyphcache(_drawcache);
    while (_runCache[0]) {
        assert(_runCache[0]->refs == 1);
        dropPageRun(_runCache[0], true);
    }
    free((void*)_fileName);
    free((void*)_decryptionKey);

    LeaveCriticalSection(&_xrefAccess);
    DeleteCriticalSection(&_xrefAccess);
    LeaveCriticalSection(&_pagesAccess);
    DeleteCriticalSection(&_pagesAccess);
}

bool PdfEngine::load(const TCHAR *fileName, WindowInfo *win, bool tryrepair)
{
    fz_error error;
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
    // don't pass in a password so that _xref isn't thrown away if it was wrong
    error = pdf_openxrefwithstream(&_xref, file, NULL);
    fz_close(file);
    if (error || !_xref)
        return false;

    if (pdf_needspassword(_xref)) {
        assert(win);
        if (!win)
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
    // keep a copy of the Info dictionary, as accessing the original
    // isn't thread safe and we don't want to block for this when
    // displaying document properties
    _info = fz_dictgets(_xref->trailer, "Info");
    if (_info)
        _info = fz_copydict(_info);
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
    fz_obj *nameobj = fz_newstring((char*)name, (int)strlen(name));
    fz_obj *dest = pdf_lookupdest(_xref, nameobj);
    fz_dropobj(nameobj);

    return dest;
}

pdf_page *PdfEngine::getPdfPage(int pageNo, bool failIfBusy)
{
    if (!_pages)
        return NULL;
    if (failIfBusy)
        return _pages[pageNo-1];

    EnterCriticalSection(&_pagesAccess);

    pdf_page *page = _pages[pageNo-1];
    if (!page) {
        EnterCriticalSection(&_xrefAccess);
        fz_obj * obj = pdf_getpageobject(_xref, pageNo);
        fz_error error = pdf_loadpage(&page, _xref, obj);
        LeaveCriticalSection(&_xrefAccess);
        if (!error) {
            linkifyPageText(page);
            _pages[pageNo-1] = page;
        }
    }

    LeaveCriticalSection(&_pagesAccess);

    return page;
}

PdfPageRun *PdfEngine::getPageRun(pdf_page *page, bool tryOnly)
{
    PdfPageRun *result = NULL;
    int i;

    EnterCriticalSection(&_pagesAccess);
    for (i = 0; i < MAX_PAGE_RUN_CACHE && _runCache[i] && !result; i++)
        if (_runCache[i]->page == page)
            result = _runCache[i];
    if (!result && !tryOnly) {
        if (MAX_PAGE_RUN_CACHE == i) {
            dropPageRun(_runCache[0], true);
            i--;
        }

        fz_displaylist *list = fz_newdisplaylist();
        fz_device *dev = fz_newlistdevice(list);
        EnterCriticalSection(&_xrefAccess);
        fz_error error = pdf_runpagefortarget(_xref, page, dev, fz_identity, Target_View);
        LeaveCriticalSection(&_xrefAccess);
        fz_freedevice(dev);

        if (fz_okay == error) {
            PdfPageRun newRun = { page, list, 1 };
            result = _runCache[i] = (PdfPageRun *)_memdup(&newRun);
        }
        else
            fz_freedisplaylist(list);
    }
    else {
        // keep the list Least Recently Used first
        for (; i < MAX_PAGE_RUN_CACHE && _runCache[i]; i++) {
            _runCache[i-1] = _runCache[i];
            _runCache[i] = result;
        }
    }

    if (result)
        result->refs++;
    LeaveCriticalSection(&_pagesAccess);
    return result;
}

fz_error PdfEngine::runPage(pdf_page *page, fz_device *dev, fz_matrix ctm, RenderTarget target, fz_rect bounds, bool cacheRun)
{
    fz_error error = fz_okay;
    PdfPageRun *run;

    if (Target_View == target && (run = getPageRun(page, !cacheRun))) {
        EnterCriticalSection(&_xrefAccess);
        fz_executedisplaylist2(run->list, dev, ctm, fz_roundrect(bounds));
        LeaveCriticalSection(&_xrefAccess);
        dropPageRun(run);
    }
    else {
        EnterCriticalSection(&_xrefAccess);
        error = pdf_runpagefortarget(_xref, page, dev, ctm, target);
        LeaveCriticalSection(&_xrefAccess);
    }
    fz_freedevice(dev);

    return error;
}

void PdfEngine::dropPageRun(PdfPageRun *run, bool forceRemove)
{
    EnterCriticalSection(&_pagesAccess);
    run->refs--;

    if (0 == run->refs || forceRemove) {
        int i;
        for (i = 0; i < MAX_PAGE_RUN_CACHE && _runCache[i] != run; i++);
        if (i < MAX_PAGE_RUN_CACHE) {
            memmove(&_runCache[i], &_runCache[i+1], (MAX_PAGE_RUN_CACHE - i - 1) * sizeof(PdfPageRun *));
            _runCache[MAX_PAGE_RUN_CACHE-1] = NULL;
        }
        if (0 == run->refs) {
            fz_freedisplaylist(run->list);
            free(run);
        }
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

fz_rect PdfEngine::pageMediabox(int pageNo)
{
    fz_rect mediabox;
    if (pdf_getmediabox(&mediabox, pdf_getpageobject(_xref, pageNo)) != fz_okay)
        return fz_emptyrect;
    return mediabox;
}

SizeD PdfEngine::pageSize(int pageNo)
{
    assert(validPageNo(pageNo));
    fz_rect bbox = pageMediabox(pageNo);
    return SizeD(fabs(bbox.x1 - bbox.x0), fabs(bbox.y1 - bbox.y0));
}

fz_bbox PdfEngine::pageContentBox(int pageNo, RenderTarget target)
{
    assert(validPageNo(pageNo));
    pdf_page *page = getPdfPage(pageNo);
    if (!page)
        return fz_emptybbox;

    fz_bbox bbox;
    fz_error error = runPage(page, fz_newbboxdevice(&bbox), fz_identity, target, page->mediabox, false);
    if (error != fz_okay)
        return fz_emptybbox;

    return fz_intersectbbox(bbox, fz_roundrect(pageMediabox(pageNo)));
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

    rotate = (rotate + page->rotate) % 360;
    if (rotate < 0) rotate = rotate + 360;
    if (90 == rotate)
        ctm = fz_concat(ctm, fz_translate(-page->mediabox.x0, -page->mediabox.y0));
    else if (180 == rotate)
        ctm = fz_concat(ctm, fz_translate(-page->mediabox.x1, -page->mediabox.y0));
    else if (270 == rotate)
        ctm = fz_concat(ctm, fz_translate(-page->mediabox.x1, -page->mediabox.y1));
    else // if (0 == rotate)
        ctm = fz_concat(ctm, fz_translate(-page->mediabox.x0, -page->mediabox.y1));

    ctm = fz_concat(ctm, fz_scale(zoom, -zoom));
    ctm = fz_concat(ctm, fz_rotate(rotate));
    return ctm;
}

fz_matrix PdfEngine::viewctm(int pageNo, float zoom, int rotate)
{
    pdf_page partialPage;
    fz_obj *page = pdf_getpageobject(_xref, pageNo);

    if (!page || pdf_getmediabox(&partialPage.mediabox, page) != fz_okay)
        return fz_identity;
    partialPage.rotate = fz_toint(fz_dictgets(page, "Rotate"));

    return viewctm(&partialPage, zoom, rotate);
}

bool PdfEngine::renderPage(HDC hDC, pdf_page *page, RECT *screenRect, fz_matrix *ctm, float zoom, int rotation, fz_rect *pageRect, RenderTarget target)
{
    if (!page)
        return false;

    fz_matrix ctm2;
    if (!pageRect)
        pageRect = &page->mediabox;
    if (!ctm) {
        if (!zoom)
            zoom = min(1.0 * (screenRect->right - screenRect->left) / (page->mediabox.x1 - page->mediabox.x0),
                       1.0 * (screenRect->bottom - screenRect->top) / (page->mediabox.y1 - page->mediabox.y0));
        ctm2 = viewctm(page, zoom, rotation);
        fz_bbox bbox = fz_roundrect(fz_transformrect(ctm2, *pageRect));
        ctm2 = fz_concat(ctm2, fz_translate(screenRect->left - bbox.x0, screenRect->top - bbox.y0));
        ctm = &ctm2;
    }

    HBRUSH bgBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
    FillRect(hDC, screenRect, bgBrush); // initialize white background
    DeleteObject(bgBrush);

    fz_bbox clipBox = { screenRect->left, screenRect->top, screenRect->right, screenRect->bottom };
    fz_error error = runPage(page, fz_newgdiplusdevice(hDC, clipBox), *ctm, target, *pageRect);

    return fz_okay == error;
}

RenderedBitmap *PdfEngine::renderBitmap(
                           int pageNo, float zoom, int rotation,
                           fz_rect *pageRect,
                           BOOL (*abortCheckCbkA)(void *data),
                           void *abortCheckCbkDataA,
                           RenderTarget target,
                           bool useGdi)
{
    pdf_page* page = getPdfPage(pageNo);
    if (!page)
        return NULL;
    fz_matrix ctm = viewctm(page, zoom, rotation);
    if (!pageRect)
        pageRect = &page->mediabox;
    fz_bbox bbox = fz_roundrect(fz_transformrect(ctm, *pageRect));

    // GDI+ seems to render quicker and more reliable at high zoom levels
    if (zoom > 40.0)
        useGdi = true;

    if (useGdi) {
        int w = bbox.x1 - bbox.x0, h = bbox.y1 - bbox.y0;
        ctm = fz_concat(ctm, fz_translate(-bbox.x0, -bbox.y0));

        // for now, don't render directly into a DC but produce an HBITMAP instead
        HDC hDC = GetDC(NULL);
        HDC hDCMem = CreateCompatibleDC(hDC);
        HBITMAP hbmp = CreateCompatibleBitmap(hDC, w, h);
        DeleteObject(SelectObject(hDCMem, hbmp));

        RECT rc = { 0, 0, w, h };
        bool success = renderPage(hDCMem, page, &rc, &ctm, 0, 0, pageRect, target);
        DeleteDC(hDCMem);
        ReleaseDC(NULL, hDC);
        if (!success) {
            DeleteObject(hbmp);
            return NULL;
        }
        return new RenderedBitmap(hbmp, w, h);
    }

    fz_pixmap *image = fz_newpixmap_nullonoom(fz_devicergb,
        bbox.x0, bbox.y0, bbox.x1 - bbox.x0, bbox.y1 - bbox.y0);
    if (!image)
        return NULL;

    fz_clearpixmapwithcolor(image, 255); // initialize white background
    if (!_drawcache)
        _drawcache = fz_newglyphcache();

    fz_error error = runPage(page, fz_newdrawdevice(_drawcache, image), ctm, target, *pageRect);
    RenderedBitmap *bitmap = NULL;
    if (!error) {
        HDC hDC = GetDC(NULL);
        bitmap = new RenderedBitmap(image, hDC);
        ReleaseDC(NULL, hDC);
    }
    fz_droppixmap(image);
    return bitmap;
}

pdf_link *PdfEngine::getLinkAtPosition(int pageNo, double x, double y)
{
    pdf_page *page = getPdfPage(pageNo, true);
    if (!page)
        return NULL;

    for (pdf_link *link = page->links; link; link = link->next)
        if (fz_isptinrect(link->rect, x, y))
            return link;

    return NULL;
}

int PdfEngine::getPdfLinks(int pageNo, pdf_link **links)
{
    pdf_page *page = getPdfPage(pageNo, true);
    if (!page)
        return -1;

    int count = 0;
    for (pdf_link *link = page->links; link; link = link->next)
        count++;

    pdf_link *linkPtr = *links = (pdf_link *)calloc(count, sizeof(pdf_link));
    for (pdf_link *link = page->links; link; link = link->next)
        *linkPtr++ = *link;

    return count;
}

void PdfEngine::linkifyPageText(pdf_page *page)
{
    fz_bbox *coords;
    TCHAR *pageText = ExtractPageText(page, _T(" "), &coords, Target_View, true);
    if (!pageText)
        return;

    // there are PDFs that have x,y positions in reverse order, so fix them up
    for (pdf_link *link = page->links; link; link = link->next) {
        if (link->rect.x0 > link->rect.x1) {
            double tmp = link->rect.x0;
            link->rect.x0 = link->rect.x1;
            link->rect.x1 = tmp;
        }
        assert(link->rect.x1 >= link->rect.x0);

        if (link->rect.y0 > link->rect.y1) {
            double tmp = link->rect.y0;
            link->rect.y0 = link->rect.y1;
            link->rect.y1 = tmp;
        }
        assert(link->rect.y1 >= link->rect.y0);
    }

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
            if (!fz_isemptyrect(isect))
                start = end;
        }

        // add the link, if it's a new one (ignoring www. links without a toplevel domain)
        if (*start && (tstr_startswith(start, _T("http")) || _tcschr(start + 5, '.') != NULL)) {
            char *uri = tstr_to_utf8(start);
            char *httpUri = str_startswith(uri, "http") ? uri : str_cat("http://", uri);
            fz_obj *dest = fz_newstring(httpUri, (int)strlen(httpUri));
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

TCHAR *PdfEngine::ExtractPageText(pdf_page *page, TCHAR *lineSep, fz_bbox **coords_out, RenderTarget target, bool cacheRun)
{
    if (!page)
        return NULL;

    fz_textspan *text = fz_newtextspan();
    fz_error error = runPage(page, fz_newtextdevice(text), fz_identity, target, page->mediabox, cacheRun);
    if (fz_okay != error) {
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

bool PdfEngine::isImagePage(int pageNo)
{
    pdf_page *page = getPdfPage(pageNo, true);
    // pages containing a single image usually contain about 50
    // characters worth of instructions, so don't bother checking
    // more instruction-heavy pages
    if (!page || !page->contents || page->contents->len > 100)
        return false;

    PdfPageRun *run = getPageRun(page);
    if (!run)
        return false;

    bool hasSingleImage = run->list->first && !run->list->first->next &&
                          run->list->first->cmd == FZ_CMDFILLIMAGE;
    dropPageRun(run);

    return hasSingleImage;
}

void PdfEngine::ageStore()
{
    EnterCriticalSection(&_xrefAccess);
    if (_xref && _xref->store)
        pdf_agestore(_xref->store, 3);
    LeaveCriticalSection(&_xrefAccess);
}
