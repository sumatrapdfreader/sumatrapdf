/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PdfEngine.h"
#include "FileUtil.h"

// maximum size of a file that's entirely loaded into memory before parsed
// and displayed; larger files will be kept open while they're displayed
// so that their content can be loaded on demand in order to preserve memory
#define MAX_MEMORY_FILE_SIZE (10 * 1024 * 1024)

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

    mediabox->x0 = (float)MIN(bbox.x0, bbox.x1);
    mediabox->y0 = (float)MIN(bbox.y0, bbox.y1);
    mediabox->x1 = (float)MAX(bbox.x0, bbox.x1);
    mediabox->y1 = (float)MAX(bbox.y0, bbox.y1);

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

HBITMAP fz_pixtobitmap(HDC hDC, fz_pixmap *pixmap, bool paletted)
{
    int paletteSize = 0;
    bool hasPalette = false;
    unsigned char *bmpData = NULL;
    
    int w = pixmap->w;
    int h = pixmap->h;
    int rows8 = ((w + 3) / 4) * 4;
    
    /* abgr is a GDI compatible format */
    fz_pixmap *bgrPixmap = fz_newpixmap_no_abort(fz_getstaticcolorspace("DeviceBGR"), pixmap->x, pixmap->y, w, h);
    if (!bgrPixmap)
        return NULL;
    fz_convertpixmap(pixmap, bgrPixmap);
    
    assert(bgrPixmap->n == 4);
    
    BITMAPINFO *bmi = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    
    if (paletted)
    {
        unsigned char *dest = bmpData = (unsigned char *)calloc(rows8, h);
        unsigned char *source = bgrPixmap->samples;
        
        for (int j = 0; j < h; j++)
        {
            for (int i = 0; i < w; i++)
            {
                RGBQUAD c = { 0 };
                
                c.rgbBlue = *source++;
                c.rgbGreen = *source++;
                c.rgbRed = *source++;
                source++;
                
                /* find this color in the palette */
                int k;
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
    
    HBITMAP hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT,
        hasPalette ? bmpData : bgrPixmap->samples, bmi, DIB_RGB_COLORS);
    
    fz_droppixmap(bgrPixmap);
    free(bmi);
    free(bmpData);
    
    return hbmp;
}

pdf_link *pdf_newlink(fz_obj *dest, pdf_linkkind kind)
{
    pdf_link *link = (pdf_link *)fz_malloc(sizeof(pdf_link));

    ZeroMemory(link, sizeof(pdf_link));
    link->dest = dest;
    link->kind = kind;

    return link;
}

pdf_outline *pdf_loadattachments(pdf_xref *xref)
{
    fz_obj *dict = pdf_loadnametree(xref, "EmbeddedFiles");
    if (!dict)
        return NULL;

    pdf_outline root = { 0 }, *node = &root;
    for (int i = 0; i < fz_dictlen(dict); i++) {
        node = node->next = (pdf_outline *)fz_malloc(sizeof(pdf_outline));
        ZeroMemory(node, sizeof(pdf_outline));

        fz_obj *name = fz_dictgetkey(dict, i);
        fz_obj *dest = fz_dictgetval(dict, i);
        fz_obj *type = fz_dictgets(dest, "Type");

        node->title = fz_strdup(fz_toname(name));
        if (fz_isname(type) && Str::Eq(fz_toname(type), "Filespec"))
            node->link = pdf_newlink(fz_keepobj(dest), PDF_LLAUNCH);
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

bool fz_isptinrect(fz_rect rect, fz_point pt)
{
    return MIN(rect.x0, rect.x1) <= pt.x && pt.x < MAX(rect.x0, rect.x1) &&
           MIN(rect.y0, rect.y1) <= pt.y && pt.y < MAX(rect.y0, rect.y1);
}

#define fz_sizeofrect(rect) (((rect).x1 - (rect).x0) * ((rect).y1 - (rect).y0))

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
                bmpData[i] = (unsigned char)(bmpData[i] * alpha + (alpha > 0 ? 0 : 255));
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
    free(_decryptionKey);

    LeaveCriticalSection(&_xrefAccess);
    DeleteCriticalSection(&_xrefAccess);
    LeaveCriticalSection(&_pagesAccess);
    DeleteCriticalSection(&_pagesAccess);
}

PdfEngine *PdfEngine::clone()
{
    // use this document's encryption key (if any) to load the clone
    char *key = NULL;
    if (_xref->crypt)
        key = _MemToHex(&_xref->crypt->key);
    TCHAR *password = key ? Str::Conv::FromAnsi(key) : NULL;
    free(key);

    PdfEngine *clone = PdfEngine::CreateFromStream(_xref->file, password);
    free(password);

    if (clone && _fileName)
        clone->_fileName = Str::Dup(_fileName);

    return clone;
}

bool PdfEngine::load(const TCHAR *fileName, PasswordUI *pwdUI)
{
    assert(!_fileName && !_xref);
    _fileName = Str::Dup(fileName);
    if (!_fileName)
        return false;
    fileName = NULL; // use _fileName instead

    // File names ending in :<digits>:<digits> are interpreted as containing
    // embedded PDF documents (the digits are :<num>:<gen> of the embedded file stream)
    TCHAR *embedMarks = NULL;
    int colonCount = 0;
    for (TCHAR *c = (TCHAR *)_fileName + Str::Len(_fileName); c > _fileName; c--) {
        if (*c != ':')
            continue;
        if (!ChrIsDigit(*(c + 1)))
            break;
        if (++colonCount % 2 == 0)
            embedMarks = c;
    }

    char *fileData = NULL;
    fz_stream *file = NULL;

    if (embedMarks)
        *embedMarks = '\0';
    size_t fileSize = File::GetSize(_fileName);
    // load small files entirely into memory so that they can be
    // overwritten even by programs that don't open files with FILE_SHARE_READ
    if (fileSize < MAX_MEMORY_FILE_SIZE)
        fileData = File::ReadAll(_fileName, &fileSize);
    if (fileData) {
        fz_buffer *data = fz_newbuffer((int)fileSize);
        if (data) {
            memcpy(data->data, fileData, data->len = (int)fileSize);
            file = fz_openbuffer(data);
            fz_dropbuffer(data);
        }
        free(fileData);
    }
    else {
#ifdef UNICODE
        file = fz_openfile2W(_fileName);
#else
        file = fz_openfile2(_fileName);
#endif
    }
    if (embedMarks)
        *embedMarks = ':';

    if (!file)
        return false;

OpenEmbeddedFile:
    // don't pass in a password so that _xref isn't thrown away if it was wrong
    fz_error error = pdf_openxrefwithstream(&_xref, file, NULL);
    fz_close(file);
    if (error || !_xref)
        return false;

    if (pdf_needspassword(_xref)) {
        if (!pwdUI)
            return false;

        unsigned char digest[16 + 32] = { 0 };
        pdf_streamfingerprint(_xref->file, digest);

        bool okay = false, saveKey = false;
        for (int i = 0; !okay && i < 3; i++) {
            ScopedMem<TCHAR> pwd(pwdUI->GetPassword(_fileName, digest, _xref->crypt->key, &saveKey));
            if (!pwd) {
                // password not given or encryption key has been remembered
                okay = saveKey;
                break;
            }

            char *pwd_doc = Str::Conv::ToPDF(pwd);
            okay = pwd_doc && pdf_authenticatepassword(_xref, pwd_doc);
            fz_free(pwd_doc);
            // try the UTF-8 password, if the PDFDocEncoding one doesn't work
            if (!okay) {
                ScopedMem<char> pwd_utf8(Str::Conv::ToUtf8(pwd));
                okay = pwd_utf8 && pdf_authenticatepassword(_xref, pwd_utf8);
            }
            // fall back to an ANSI-encoded password as a last measure
            if (!okay) {
                ScopedMem<char> pwd_ansi(Str::Conv::ToAnsi(pwd));
                okay = pwd_ansi && pdf_authenticatepassword(_xref, pwd_ansi);
            }
        }
        if (!okay)
            return false;

        if (saveKey) {
            memcpy(digest + 16, _xref->crypt->key, 32);
            _decryptionKey = _MemToHex(&digest);
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

    return finishLoading();
}

bool PdfEngine::load(fz_stream *stm, TCHAR *password)
{
    assert(!_fileName && !_xref);

    // don't pass in a password so that _xref isn't thrown away if it was wrong
    fz_error error = pdf_openxrefwithstream(&_xref, stm, NULL);
    if (error || !_xref)
        return false;

    if (pdf_needspassword(_xref)) {
        if (!password)
            return false;

        char *pwd_doc = Str::Conv::ToPDF(password);
        bool okay = pwd_doc && pdf_authenticatepassword(_xref, pwd_doc);
        fz_free(pwd_doc);
        // try the UTF-8 password, if the PDFDocEncoding one doesn't work
        if (!okay) {
            ScopedMem<char> pwd_utf8(Str::Conv::ToUtf8(password));
            okay = pwd_utf8 && pdf_authenticatepassword(_xref, pwd_utf8);
        }
        // fall back to an ANSI-encoded password as a last measure
        if (!okay) {
            ScopedMem<char> pwd_ansi(Str::Conv::ToAnsi(password));
            okay = pwd_ansi && pdf_authenticatepassword(_xref, pwd_ansi);
        }
        // finally, try using the password as hex-encoded encryption key
        if (!okay && Str::Len(password) == 64) {
            ScopedMem<char> pwd_hex(Str::Conv::ToAnsi(password));
            okay = _HexToMem(pwd_hex, &_xref->crypt->key);
        }

        if (!okay)
            return false;
    }

    return finishLoading();
}

bool PdfEngine::finishLoading()
{
    fz_error error = pdf_loadpagetree(_xref);
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
        _info = fz_copydict(pdf_resolveindirect(_info));
    LeaveCriticalSection(&_xrefAccess);

    _pages = SAZA(pdf_page *, _pageCount);
    return _pageCount > 0;
}

PdfTocItem *PdfEngine::buildTocTree(pdf_outline *entry, int *idCounter)
{
    TCHAR *name = entry->title ? Str::Conv::FromUtf8(entry->title) : Str::Dup(_T(""));
    PdfTocItem *node = new PdfTocItem(name, entry->link);
    node->open = entry->count >= 0;
    node->id = ++(*idCounter);

    if (entry->link && PDF_LGOTO == entry->link->kind)
        node->pageNo = findPageNo(entry->link->dest);
    if (entry->child)
        node->child = buildTocTree(entry->child, idCounter);
    if (entry->next)
        node->next = buildTocTree(entry->next, idCounter);

    return node;
}

PdfTocItem *PdfEngine::getTocTree()
{
    PdfTocItem *node = NULL;
    int idCounter = 0;

    if (_outline) {
        node = buildTocTree(_outline, &idCounter);
        if (_attachments)
            node->AddSibling(buildTocTree(_attachments, &idCounter));
    }
    else if (_attachments)
        node = buildTocTree(_attachments, &idCounter);

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
        fz_freedevice(dev);
        if (error)
            fz_freedisplaylist(list);
        LeaveCriticalSection(&_xrefAccess);

        if (!error) {
            PdfPageRun newRun = { page, list, 1 };
            result = _runCache[i] = (PdfPageRun *)_memdup(&newRun);
        }
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
            EnterCriticalSection(&_xrefAccess);
            fz_freedisplaylist(run->list);
            LeaveCriticalSection(&_xrefAccess);
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
    ctm = fz_concat(ctm, fz_rotate((float)rotate));
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

bool PdfEngine::renderPage(HDC hDC, pdf_page *page, RectI *screenRect, fz_matrix *ctm, float zoom, int rotation, fz_rect *pageRect, RenderTarget target)
{
    if (!page)
        return false;

    fz_matrix ctm2;
    if (!pageRect)
        pageRect = &page->mediabox;
    if (!ctm) {
        if (!zoom)
            zoom = min(1.0f * screenRect->dx / (page->mediabox.x1 - page->mediabox.x0),
                       1.0f * screenRect->dy / (page->mediabox.y1 - page->mediabox.y0));
        ctm2 = viewctm(page, zoom, rotation);
        fz_bbox bbox = fz_roundrect(fz_transformrect(ctm2, *pageRect));
        ctm2 = fz_concat(ctm2, fz_translate((float)screenRect->x - bbox.x0, (float)screenRect->y - bbox.y0));
        ctm = &ctm2;
    }

    HBRUSH bgBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
    FillRect(hDC, &screenRect->ToRECT(), bgBrush); // initialize white background
    DeleteObject(bgBrush);

    fz_bbox clipBox = { screenRect->x, screenRect->y, screenRect->x + screenRect->dx, screenRect->y + screenRect->dy };
    fz_error error = runPage(page, fz_newgdiplusdevice(hDC, clipBox), *ctm, target, *pageRect);

    return fz_okay == error;
}

RenderedBitmap *PdfEngine::renderBitmap(
                           int pageNo, float zoom, int rotation,
                           fz_rect *pageRect, RenderTarget target,
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
        ctm = fz_concat(ctm, fz_translate((float)-bbox.x0, (float)-bbox.y0));

        // for now, don't render directly into a DC but produce an HBITMAP instead
        HDC hDC = GetDC(NULL);
        HDC hDCMem = CreateCompatibleDC(hDC);
        HBITMAP hbmp = CreateCompatibleBitmap(hDC, w, h);
        DeleteObject(SelectObject(hDCMem, hbmp));

        RectI rc(0, 0, w, h);
        bool success = renderPage(hDCMem, page, &rc, &ctm, 0, 0, pageRect, target);
        DeleteDC(hDCMem);
        ReleaseDC(NULL, hDC);
        if (!success) {
            DeleteObject(hbmp);
            return NULL;
        }
        return new RenderedBitmap(hbmp, w, h);
    }

    fz_pixmap *image = fz_newpixmap_no_abort(fz_getstaticcolorspace("DeviceRGB"),
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

pdf_link *PdfEngine::getLinkAtPosition(int pageNo, float x, float y)
{
    pdf_page *page = getPdfPage(pageNo, true);
    if (!page)
        return NULL;

    for (pdf_link *link = page->links; link; link = link->next) {
        fz_point pt = { x, y };
        if (fz_isptinrect(link->rect, pt))
            return link;
    }

    return NULL;
}

pdf_annot *PdfEngine::getCommentAtPosition(int pageNo, float x, float y)
{
    pdf_page *page = getPdfPage(pageNo, true);
    if (!page)
        return NULL;

    for (pdf_annot *annot = page->annots; annot; annot = annot->next) {
        fz_point pt = { x, y };
        if (fz_isptinrect(annot->rect, pt) &&
            Str::Eq(fz_toname(fz_dictgets(annot->obj, "Subtype")), "Text") &&
            !Str::IsEmpty(fz_tostrbuf(fz_dictgets(annot->obj, "Contents")))) {
            return annot;
        }
    }

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

    pdf_link *linkPtr = *links = SAZA(pdf_link, count);
    for (pdf_link *link = page->links; link; link = link->next)
        *linkPtr++ = *link;

    return count;
}

static pdf_link *getLastLink(pdf_link *head)
{
    if (head)
        while (head->next)
            head = head->next;

    return head;
}

static bool isMultilineLink(TCHAR *pageText, TCHAR *pos, fz_bbox *coords)
{
    // multiline links end in a non-alphanumeric character and continue on a line
    // that starts left and only slightly below where the current line ended
    // (and that doesn't start with http itself)
    return
        '\n' == *pos && pos > pageText && !_istalnum(pos[-1]) && !_istspace(pos[1]) &&
        coords[pos - pageText + 1].y1 > coords[pos - pageText - 1].y0 &&
        coords[pos - pageText + 1].y0 <= coords[pos - pageText - 1].y1 &&
        coords[pos - pageText + 1].x0 < coords[pos - pageText - 1].x1 &&
        !Str::StartsWith(pos + 1, _T("http"));
}

static TCHAR *findLinkEnd(TCHAR *start)
{
    TCHAR *end;

    // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
    for (end = start; *end && !_istspace(*end); end++);
    if (',' == end[-1] || '.' == end[-1])
        end--;
    // also ignore a closing parenthesis, if the URL doesn't contain any opening one
    if (')' == end[-1] && (!_tcschr(start, '(') || _tcschr(start, '(') > end))
        end--;

    return end;
}

static TCHAR *parseMultilineLink(pdf_page *page, TCHAR *pageText, TCHAR *start, fz_bbox *coords)
{
    pdf_link *firstLink = getLastLink(page->links);
    char *uri = Str::Dup(fz_tostrbuf(firstLink->dest));
    TCHAR *end = start;
    bool multiline = false;

    do {
        end = findLinkEnd(start);
        multiline = isMultilineLink(pageText, end, coords);
        *end = 0;

        // add a new link for this line
        fz_bbox bbox = fz_unionbbox(coords[start - pageText], coords[end - pageText - 1]);
        ScopedMem<char> uriPart(Str::Conv::ToUtf8(start));
        char *newUri = Str::Join(uri, uriPart);
        free(uri);
        uri = newUri;

        pdf_link *link = pdf_newlink(NULL, PDF_LURI);
        link->rect = fz_bboxtorect(bbox);
        getLastLink(firstLink)->next = link;

        start = end + 1;
    } while (multiline);

    // update the link URL for all partial links
    fz_dropobj(firstLink->dest);
    firstLink->dest = fz_newstring(uri, (int)strlen(uri));
    for (pdf_link *link = firstLink->next; link; link = link->next)
        link->dest = fz_keepobj(firstLink->dest);
    free(uri);

    return end;
}

static void FixupPageLinks(pdf_page *page)
{
    // Links in PDF documents are added from bottom-most to top-most,
    // i.e. links that appear later in the list should be preferred
    // to links appearing before. Since we search from the start of
    // the (single-linked) list, we have to reverse the order of links
    // (cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1303 )
    pdf_link *newTop = NULL;
    while (page->links) {
        pdf_link *tmp = page->links->next;
        page->links->next = newTop;
        newTop = page->links;
        page->links = tmp;

        // there are PDFs that have x,y positions in reverse order, so fix them up
        pdf_link *link = newTop;
        if (link->rect.x0 > link->rect.x1)
            swap(link->rect.x0, link->rect.x1);
        if (link->rect.y0 > link->rect.y1)
            swap(link->rect.y0, link->rect.y1);
        assert(link->rect.x1 >= link->rect.x0);
        assert(link->rect.y1 >= link->rect.y0);
    }
    page->links = newTop;
}

void PdfEngine::linkifyPageText(pdf_page *page)
{
    FixupPageLinks(page);

    fz_bbox *coords;
    TCHAR *pageText = ExtractPageText(page, _T("\n"), &coords, Target_View, true);
    if (!pageText)
        return;

    for (TCHAR *start = pageText; *start; start++) {
        // look for words starting with "http://", "https://" or "www."
        if (('h' != *start || !Str::StartsWith(start, _T("http://")) &&
                              !Str::StartsWith(start, _T("https://"))) &&
            ('w' != *start || !Str::StartsWith(start, _T("www."))) ||
            (start > pageText && (_istalnum(start[-1]) || '/' == start[-1])))
            continue;

        TCHAR *end = findLinkEnd(start);
        bool multiline = isMultilineLink(pageText, end, coords);
        *end = 0;

        // make sure that no other link is associated with this area
        fz_bbox bbox = fz_unionbbox(coords[start - pageText], coords[end - pageText - 1]);
        for (pdf_link *link = page->links; link && *start; link = link->next) {
            fz_bbox isect = fz_intersectbbox(bbox, fz_roundrect(link->rect));
            if (!fz_isemptybbox(isect) && fz_sizeofrect(isect) >= 0.25 * fz_sizeofrect(link->rect))
                start = end;
        }

        // add the link, if it's a new one (ignoring www. links without a toplevel domain)
        if (*start && (Str::StartsWith(start, _T("http")) || _tcschr(start + 5, '.') != NULL)) {
            char *uri = Str::Conv::ToUtf8(start);
            char *httpUri = Str::StartsWith(uri, "http") ? uri : Str::Join("http://", uri);
            fz_obj *dest = fz_newstring(httpUri, (int)strlen(httpUri));
            pdf_link *link = pdf_newlink(dest, PDF_LURI);
            link->rect = fz_bboxtorect(bbox);
            if (page->links)
                getLastLink(page->links)->next = link;
            else
                page->links = link;
            if (httpUri != uri)
                free(httpUri);
            free(uri);

            if (multiline)
                end = parseMultilineLink(page, pageText, end + 1, coords);
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

    WCHAR *content = NULL;

    fz_textspan *text = fz_newtextspan();
    // use an infinite rectangle as bounds (instead of page->mediabox) to ensure that
    // the extracted text is consistent between cached runs using a list device and
    // fresh runs (otherwise the list device omits text outside the mediabox bounds)
    fz_error error = runPage(page, fz_newtextdevice(text), fz_identity, target, fz_infiniterect, cacheRun);
    if (fz_okay != error)
        goto CleanUp;

    int lineSepLen = Str::Len(lineSep);
    size_t textLen = 0;
    for (fz_textspan *span = text; span; span = span->next)
        textLen += span->len + lineSepLen;

    content = SAZA(WCHAR, textLen + 1);
    if (!content)
        goto CleanUp;
    fz_bbox *destRect = NULL;
    if (coords_out)
        destRect = *coords_out = SAZA(fz_bbox, textLen);

    WCHAR *dest = content;
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

CleanUp:
    EnterCriticalSection(&_xrefAccess);
    fz_freetextspan(text);
    LeaveCriticalSection(&_xrefAccess);

    return Str::Conv::FromWStrQ(content);
}

TCHAR *PdfEngine::ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out, RenderTarget target)
{
    // TODO: move fz_bbox** to RectI** conversion into ExtractPageText(pdf_page, ...)
    fz_bbox *coords = NULL;
    fz_bbox **coords_tmp = coords_out ? &coords : NULL;
    TCHAR *result = NULL;

    pdf_page *page = getPdfPage(pageNo, true);
    if (page) {
        result = ExtractPageText(page, lineSep, coords_tmp, target);
        goto ConvertCoords;
    }

    EnterCriticalSection(&_xrefAccess);
    fz_error error = pdf_loadpage(&page, _xref, pdf_getpageobject(_xref, pageNo));
    LeaveCriticalSection(&_xrefAccess);
    if (error)
        return NULL;

    result = ExtractPageText(page, lineSep, coords_tmp, target);
    pdf_freepage(page);

ConvertCoords:
    if (coords) {
        size_t len = Str::Len(result);
        RectI *destRect = *coords_out = new RectI[len];
        for (size_t i = 0; i < len; i++)
            destRect[i] = RectI::FromXY(coords[i].x0, coords[i].y0, coords[i].x1, coords[i].y1);
        free(coords);
    }

    return result;
}

TCHAR *PdfEngine::getPdfInfo(char *key) const
{
    fz_obj *obj = fz_dictgets(_info, key);
    if (!obj)
        return NULL;

    WCHAR *ucs2 = (WCHAR *)pdf_toucs2(obj);
    TCHAR *tstr = Str::Conv::FromWStr(ucs2);
    fz_free(ucs2);

    return tstr;
};

// returns the version in the format Mmmee (Major, minor, extensionlevel)
int PdfEngine::getPdfVersion() const
{
    if (!_xref)
        return -1;

    int version = (_xref->version / 10) * 10000 + (_xref->version % 10) * 100;
    // Crypt version 5 indicates PDF 1.7 Adobe Extension Level 3
    if (10700 == version && _xref->crypt && 5 == _xref->crypt->v)
        version += 3;

    return version;
}

char *PdfEngine::getPageLayoutName()
{
    EnterCriticalSection(&_xrefAccess);
    fz_obj *root = fz_dictgets(_xref->trailer, "Root");
    char *name = fz_toname(fz_dictgets(root, "PageLayout"));
    LeaveCriticalSection(&_xrefAccess);
    return name;
}

bool PdfEngine::isDocumentDirectionR2L()
{
    EnterCriticalSection(&_xrefAccess);
    fz_obj *root = fz_dictgets(_xref->trailer, "Root");
    fz_obj *prefs = fz_dictgets(root, "ViewerPreferences");
    char *direction = fz_toname(fz_dictgets(prefs, "Direction"));
    LeaveCriticalSection(&_xrefAccess);
    return Str::Eq(direction, "R2L");
}

fz_buffer *PdfEngine::getStreamData(int num, int gen)
{
    fz_stream *stream = NULL;
    fz_buffer *data = NULL;

    if (num) {
        fz_error error = pdf_loadstream(&data, _xref, num, gen);
        if (error != fz_okay)
            return NULL;
        return data;
    }

    stream = fz_keepstream(_xref->file);
    if (!stream)
        return NULL;

    fz_seek(stream, 0, 2);
    int len = fz_tell(stream);
    fz_seek(stream, 0, 0);
    fz_readall(&data, stream, len);
    fz_close(stream);

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

PdfEngine *PdfEngine::CreateFromFileName(const TCHAR *fileName, PasswordUI *pwdUI)
{
    PdfEngine *engine = new PdfEngine();
    if (!engine || !fileName || !engine->load(fileName, pwdUI)) {
        delete engine;
        return NULL;
    }
    return engine;
}

PdfEngine *PdfEngine::CreateFromStream(fz_stream *stm, TCHAR *password)
{
    PdfEngine *engine = new PdfEngine();
    if (!engine || !stm || !engine->load(stm, password)) {
        delete engine;
        return NULL;
    }
    return engine;
}
