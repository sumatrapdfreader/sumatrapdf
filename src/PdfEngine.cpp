/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
__pragma(warning(push))
#include <fitz.h>
__pragma(warning(pop))
}

#include "PdfEngine.h"
#include "StrUtil.h"
#include "FileUtil.h"

// maximum size of a file that's entirely loaded into memory before parsed
// and displayed; larger files will be kept open while they're displayed
// so that their content can be loaded on demand in order to preserve memory
#define MAX_MEMORY_FILE_SIZE (10 * 1024 * 1024)

// number of page content trees to cache for quicker rendering
#define MAX_PAGE_RUN_CACHE  8

inline fz_rect fz_bbox_to_rect(fz_bbox bbox)
{
    fz_rect result = { (float)bbox.x0, (float)bbox.y0, (float)bbox.x1, (float)bbox.y1 };
    return result;
}

bool fz_is_pt_in_rect(fz_rect rect, fz_point pt)
{
    return MIN(rect.x0, rect.x1) <= pt.x && pt.x < MAX(rect.x0, rect.x1) &&
           MIN(rect.y0, rect.y1) <= pt.y && pt.y < MAX(rect.y0, rect.y1);
}

#define fz_sizeofrect(rect) (((rect).x1 - (rect).x0) * ((rect).y1 - (rect).y0))

inline RectD fz_rect_to_RectD(fz_rect rect)
{
    return RectD::FromXY(rect.x0, rect.y0, rect.x1, rect.y1);
}

inline fz_rect fz_RectD_to_rect(RectD rect)
{
    fz_rect result = { (float)rect.x, (float)rect.y, (float)(rect.x + rect.dx), (float)(rect.y + rect.dy) };
    return result;
}

inline RectI fz_bbox_to_RectI(fz_bbox bbox)
{
    return RectI::FromXY(bbox.x0, bbox.y0, bbox.x1, bbox.y1);
}

inline fz_bbox fz_RectI_to_bbox(RectI bbox)
{
    fz_bbox result = { bbox.x, bbox.y, bbox.x + bbox.dx, bbox.y + bbox.dy };
    return result;
}

class RenderedFitzBitmap : public RenderedBitmap {
public:
    RenderedFitzBitmap(fz_pixmap *pixmap, HDC hDC);
};

RenderedFitzBitmap::RenderedFitzBitmap(fz_pixmap *pixmap, HDC hDC) :
    RenderedBitmap(NULL, pixmap->w, pixmap->h)
{
    int paletteSize = 0;
    bool hasPalette = false;
    unsigned char *bmpData = NULL;
    
    int w = pixmap->w;
    int h = pixmap->h;
    int rows8 = ((w + 3) / 4) * 4;
    
    /* abgr is a GDI compatible format */
    fz_pixmap *bgrPixmap = fz_new_pixmap_with_limit(fz_find_device_colorspace("DeviceBGR"), w, h);
    if (!bgrPixmap)
        return;
    bgrPixmap->x = pixmap->x; bgrPixmap->y = pixmap->y;
    fz_convert_pixmap(pixmap, bgrPixmap);
    
    assert(bgrPixmap->n == 4);
    
    BITMAPINFO *bmi = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    
    // always try to produce an 8-bit palette for saving some memory
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
    
    _hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT,
        hasPalette ? bmpData : bgrPixmap->samples, bmi, DIB_RGB_COLORS);
    
    fz_drop_pixmap(bgrPixmap);
    free(bmi);
    free(bmpData);
}

fz_stream *fz_open_file2(const TCHAR *filePath)
{
    fz_stream *file = NULL;

    size_t fileSize = File::GetSize(filePath);
    // load small files entirely into memory so that they can be
    // overwritten even by programs that don't open files with FILE_SHARE_READ
    if (fileSize < MAX_MEMORY_FILE_SIZE) {
        fz_buffer *data = fz_new_buffer((int)fileSize);
        if (data) {
            if (File::ReadAll(filePath, (char *)data->data, (data->len = fileSize)))
                file = fz_open_buffer(data);
            fz_drop_buffer(data);
        }
    }
    if (!file) {
#ifdef UNICODE
        file = fz_open_file_w(filePath);
#else
        file = fz_open_file(filePath);
#endif
    }

    return file;
}

unsigned char *fz_extract_stream_data(fz_stream *stream, size_t *cbCount)
{
    fz_seek(stream, 0, 2);
    int fileLen = fz_tell(stream);

    fz_buffer *buffer;
    fz_seek(stream, 0, 0);
    fz_read_all(&buffer, stream, fileLen);
    assert(fileLen == buffer->len);

    unsigned char *data = (unsigned char *)malloc(buffer->len);
    if (data) {
        memcpy(data, buffer->data, buffer->len);
        if (cbCount)
            *cbCount = buffer->len;
    }

    fz_drop_buffer(buffer);

    return data;
}

void fz_stream_fingerprint(fz_stream *file, unsigned char *digest)
{
    fz_seek(file, 0, 2);
    int fileLen = fz_tell(file);

    fz_buffer *buffer;
    fz_seek(file, 0, 0);
    fz_read_all(&buffer, file, fileLen);
    assert(fileLen == buffer->len);

    fz_md5 md5;
    fz_md5_init(&md5);
    fz_md5_update(&md5, buffer->data, buffer->len);
    fz_md5_final(&md5, digest);

    fz_drop_buffer(buffer);
}

WCHAR *fz_span_to_wchar(fz_text_span *text, TCHAR *lineSep, RectI **coords_out=NULL)
{
    int lineSepLen = Str::Len(lineSep);
    size_t textLen = 0;
    for (fz_text_span *span = text; span; span = span->next)
        textLen += span->len + lineSepLen;

    WCHAR *content = SAZA(WCHAR, textLen + 1);
    if (!content)
        return NULL;

    RectI *destRect = NULL;
    if (coords_out)
        destRect = *coords_out = new RectI[textLen];

    WCHAR *dest = content;
    for (fz_text_span *span = text; span; span = span->next) {
        for (int i = 0; i < span->len; i++) {
            *dest = span->text[i].c;
            if (*dest < 32)
                *dest = '?';
            dest++;
            if (destRect)
                *destRect++ = fz_bbox_to_RectI(span->text[i].bbox);
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

    return content;
}

extern "C" static int read_istream(fz_stream *stm, unsigned char *buf, int len)
{
    ULONG cbRead = len;
    HRESULT res = ((IStream *)stm->state)->Read(buf, len, &cbRead);
    if (FAILED(res))
        return fz_throw("read error: %s", res);
    return (int)cbRead;
}

extern "C" static void seek_istream(fz_stream *stm, int offset, int whence)
{
    LARGE_INTEGER off;
    ULARGE_INTEGER n;
    off.QuadPart = offset;
    HRESULT res = ((IStream *)stm->state)->Seek(off, whence, &n);
    if (FAILED(res))
        fz_warn("cannot seek: %s", res);
    stm->pos = (int)n.QuadPart;
    stm->rp = stm->wp = stm->bp;
}

extern "C" static void close_istream(fz_stream *stm)
{
    ((IStream *)stm->state)->Release();
}

fz_stream *fz_open_istream(IStream *stream)
{
    stream->AddRef();

    fz_stream *stm = fz_new_stream(stream, read_istream, close_istream);
    stm->seek = seek_istream;
    return stm;
}

// Ensure that fz_accelerate is called before using Fitz the first time.
class FitzAccelerator { public: FitzAccelerator() { fz_accelerate(); } };
FitzAccelerator _globalAccelerator;

extern "C" {
#include <mupdf.h>
}

namespace Str {
    namespace Conv {

inline TCHAR *FromPdf(fz_obj *obj)
{
    WCHAR *ucs2 = (WCHAR *)pdf_to_ucs2(obj);
    TCHAR *tstr = FromWStr(ucs2);
    fz_free(ucs2);
    return tstr;
}

// Caller needs to fz_free the result
inline char *ToPDF(TCHAR *tstr)
{
    ScopedMem<WCHAR> wstr(ToWStr(tstr));
    return pdf_from_ucs2((unsigned short *)wstr.Get());
}

    }
}

pdf_link *pdf_new_link(fz_obj *dest, pdf_link_kind kind)
{
    pdf_link *link = (pdf_link *)fz_malloc(sizeof(pdf_link));

    ZeroMemory(link, sizeof(pdf_link));
    link->dest = dest;
    link->kind = kind;

    return link;
}

pdf_outline *pdf_loadattachments(pdf_xref *xref)
{
    fz_obj *dict = pdf_load_name_tree(xref, "EmbeddedFiles");
    if (!dict)
        return NULL;

    pdf_outline root = { 0 }, *node = &root;
    for (int i = 0; i < fz_dict_len(dict); i++) {
        node = node->next = (pdf_outline *)fz_malloc(sizeof(pdf_outline));
        ZeroMemory(node, sizeof(pdf_outline));

        fz_obj *name = fz_dict_get_key(dict, i);
        fz_obj *dest = fz_dict_get_val(dict, i);
        fz_obj *type = fz_dict_gets(dest, "Type");

        node->title = fz_strdup(fz_to_name(name));
        if (fz_is_name(type) && Str::Eq(fz_to_name(type), "Filespec"))
            node->link = pdf_new_link(fz_keep_obj(dest), PDF_LINK_LAUNCH);
    }
    fz_drop_obj(dict);

    return root.next;
}

// adapted from pdf_page.c's pdf_load_page_info
fz_error pdf_get_mediabox(fz_rect *mediabox, fz_obj *page)
{
    fz_obj *obj = fz_dict_gets(page, "MediaBox");
    fz_bbox bbox = fz_round_rect(pdf_to_rect(obj));
    if (fz_is_empty_rect(pdf_to_rect(obj)))
    {
        fz_warn("cannot find page bounds, guessing page bounds.");
        bbox.x0 = 0;
        bbox.y0 = 0;
        bbox.x1 = 612;
        bbox.y1 = 792;
    }

    obj = fz_dict_gets(page, "CropBox");
    if (fz_is_array(obj))
    {
        fz_bbox cropbox = fz_round_rect(pdf_to_rect(obj));
        bbox = fz_intersect_bbox(bbox, cropbox);
    }

    mediabox->x0 = (float)MIN(bbox.x0, bbox.x1);
    mediabox->y0 = (float)MIN(bbox.y0, bbox.y1);
    mediabox->x1 = (float)MAX(bbox.x0, bbox.x1);
    mediabox->y1 = (float)MAX(bbox.y0, bbox.y1);

    if (mediabox->x1 - mediabox->x0 < 1 || mediabox->y1 - mediabox->y0 < 1)
        return fz_throw("invalid page size");

    return fz_okay;
}

class PdfLink : public PageDestination {
    pdf_link *link;
    int pageNo;

public:
    PdfLink() : link(NULL), pageNo(-1) { }
    PdfLink(pdf_link *link, int pageNo=-1) : link(link), pageNo(pageNo) { }

    virtual RectD GetRect() const;
    virtual TCHAR *GetValue() const;
    virtual int GetPageNo() const { return pageNo; }
    virtual PageDestination *AsLink() { return this; }

    virtual const char *GetType() const;
    virtual fz_obj *dest() const;
};

class CPdfTocItem : public PdfTocItem {
    PdfLink link;

public:
    CPdfTocItem(TCHAR *title, PdfLink link) : PdfTocItem(title), link(link) { }

    void AddSibling(PdfTocItem *sibling)
    {
        PdfTocItem *item;
        for (item = this; item->next; item = item->next);
        item->next = sibling;
    }

    virtual PageDestination *GetLink() { return &link; }
};

RectD PdfLink::GetRect() const
{
    return link ? fz_rect_to_RectD(link->rect) : RectD();
}

TCHAR *PdfLink::GetValue() const
{
    TCHAR *path = NULL;
    fz_obj *obj;

    switch (link ? link->kind : -1) {
        case PDF_LINK_URI:
            path = Str::Conv::FromPdf(link->dest);
            break;
        case PDF_LINK_LAUNCH:
            obj = fz_dict_gets(link->dest, "Type");
            if (!fz_is_name(obj) || !Str::Eq(fz_to_name(obj), "Filespec"))
                break;
            obj = fz_dict_gets(link->dest, "UF"); 
            if (!fz_is_string(obj))
                obj = fz_dict_gets(link->dest, "F"); 

            if (fz_is_string(obj)) {
                path = Str::Conv::FromPdf(obj);
                Str::TransChars(path, _T("/"), _T("\\"));
            }
            break;
        case PDF_LINK_ACTION:
            obj = fz_dict_gets(link->dest, "S");
            if (!fz_is_name(obj))
                break;
            if (Str::Eq(fz_to_name(obj), "GoToR")) {
                obj = fz_dict_gets(link->dest, "F");
                // Note: this might not be per standard but is required to fix Nissan_Manual_370Z.pdf
                // from http://fofou.appspot.com/sumatrapdf/topic?id=2018365
                if (fz_is_dict(obj))
                    obj = fz_dict_gets(obj, "F");
                if (fz_is_string(obj)) {
                    path = Str::Conv::FromPdf(obj);
                    Str::TransChars(path, _T("/"), _T("\\"));
                }
            }
            break;
    }

    return path;
}

const char *PdfLink::GetType() const
{
    switch (link ? link->kind : -1) {
    case PDF_LINK_URI: return "LaunchURL";
    case PDF_LINK_GOTO: return "ScrollTo";
    case PDF_LINK_NAMED: return fz_to_name(link->dest);
    case PDF_LINK_LAUNCH:
        if (fz_dict_gets(link->dest, "EF"))
            return "LaunchEmbedded";
        return "LaunchFile";
    case PDF_LINK_ACTION:
        if (Str::Eq(fz_to_name(fz_dict_gets(link->dest, "S")), "GoToR") &&
            fz_dict_gets(link->dest, "D")) {
            return "ScrollToEx";
        }
        // fall through (unsupported action)
    default:
        return NULL;
    }
}

fz_obj *PdfLink::dest() const
{
    if (!link)
        return NULL;
    if (PDF_LINK_ACTION == link->kind && Str::Eq(GetType(), "ScrollToEx"))
        return fz_dict_gets(link->dest, "D");
    if (PDF_LINK_LAUNCH == link->kind && Str::Eq(GetType(), "LaunchEmbedded"))
        return fz_dict_getsa(fz_dict_gets(link->dest, "EF"), "UF", "F");
    return link->dest;
}

class PdfComment : public PageElement {
    pdf_annot *annot;
    int pageNo;

public:
    PdfComment(pdf_annot *annot, int pageNo=-1) : annot(annot), pageNo(pageNo) { }

    virtual RectD GetRect() const {
        return fz_rect_to_RectD(annot->rect);
    }
    virtual TCHAR *GetValue() const {
        return Str::Conv::FromUtf8(fz_to_str_buf(fz_dict_gets(annot->obj, "Contents")));
    }
    virtual int GetPageNo() const {
        return pageNo;
    }
};

///// Above are extensions to Fitz and MuPDF, now follows PdfEngine /////

struct PdfPageRun {
    pdf_page *page;
    fz_display_list *list;
    int refs;
};

class CPdfEngine : public PdfEngine {
public:
    CPdfEngine();
    virtual ~CPdfEngine();
    virtual CPdfEngine *Clone();

    virtual const TCHAR *FileName() const { return _fileName; };
    virtual int PageCount() const {
        return _xref ? pdf_count_pages(_xref) : 0;
    }

    virtual int PageRotation(int pageNo);
    virtual RectD PageMediabox(int pageNo);
    virtual RectI PageContentBox(int pageNo, RenderTarget target=Target_View);

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false);
    virtual bool RenderPage(HDC hDC, int pageNo, RectI screenRect,
                         float zoom=0, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View) {
        return renderPage(hDC, getPdfPage(pageNo), screenRect, NULL, zoom, rotation, pageRect, target);
    }

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool HasTextContent() { return true; }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View);
    virtual bool IsImagePage(int pageNo);
    virtual PageLayoutType PreferredLayout();
    virtual TCHAR *GetProperty(char *name);

    virtual bool IsPrintingAllowed() { return hasPermission(PDF_PERM_PRINT); }
    virtual bool IsCopyingTextAllowed() { return hasPermission(PDF_PERM_COPY); }

    virtual float GetFileDPI() const { return 72.0f; }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".pdf"); }

    virtual bool BenchLoadPage(int pageNo) { return getPdfPage(pageNo) != NULL; }

    // TODO: move any of the following into BaseEngine?

    virtual Vec<PageElement *> *GetElements(int pageNo);
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt);

    virtual int FindPageNo(fz_obj *dest);
    virtual fz_obj *GetNamedDest(const TCHAR *name);
    virtual bool HasToCTree() const {
        return _outline != NULL || _attachments != NULL;
    }
    virtual PdfTocItem *GetToCTree();
    virtual bool SaveEmbedded(fz_obj *obj, LinkSaverUI& saveUI);

    virtual char *GetDecryptionKey() const;
    virtual void RunGC();

protected:
    const TCHAR *_fileName;
    char *_decryptionKey;

    // make sure to never ask for _pagesAccess in an _xrefAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION _xrefAccess;
    pdf_xref *      _xref;

    CRITICAL_SECTION _pagesAccess;
    pdf_page **     _pages;

    virtual bool    load(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    virtual bool    load(IStream *stream, PasswordUI *pwdUI=NULL);
    bool            load(fz_stream *stm, PasswordUI *pwdUI=NULL);
    bool            load_from_stream(fz_stream *stm, PasswordUI *pwdUI=NULL);
    bool            finishLoading();
    pdf_page      * getPdfPage(int pageNo, bool failIfBusy=false);
    fz_matrix       viewctm(int pageNo, float zoom, int rotate);
    fz_matrix       viewctm(pdf_page *page, float zoom, int rotate);
    bool            renderPage(HDC hDC, pdf_page *page, RectI screenRect,
                               fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                               RectD *pageRect=NULL, RenderTarget target=Target_View);
    TCHAR         * ExtractPageText(pdf_page *page, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View, bool cacheRun=false);

    PdfPageRun    * _runCache[MAX_PAGE_RUN_CACHE];
    PdfPageRun    * getPageRun(pdf_page *page, bool tryOnly=false);
    fz_error        runPage(pdf_page *page, fz_device *dev, fz_matrix ctm,
                            RenderTarget target=Target_View,
                            fz_bbox clipbox=fz_infinite_bbox, bool cacheRun=true);
    void            dropPageRun(PdfPageRun *run, bool forceRemove=false);

    CPdfTocItem   * buildTocTree(pdf_outline *entry, int *idCounter);
    void            linkifyPageText(pdf_page *page);
    bool            hasPermission(int permission);

    pdf_outline   * _outline;
    pdf_outline   * _attachments;
    fz_obj        * _info;
    fz_glyph_cache* _drawcache;
};

CPdfEngine::CPdfEngine() : 
        _fileName(NULL)
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

CPdfEngine::~CPdfEngine()
{
    EnterCriticalSection(&_pagesAccess);
    EnterCriticalSection(&_xrefAccess);

    if (_pages) {
        for (int i = 0; i < PageCount(); i++) {
            if (_pages[i])
                pdf_free_page(_pages[i]);
        }
        free(_pages);
    }

    if (_outline)
        pdf_free_outline(_outline);
    if (_attachments)
        pdf_free_outline(_attachments);
    if (_info)
        fz_drop_obj(_info);

    if (_xref) {
        pdf_free_xref(_xref);
        _xref = NULL;
    }

    if (_drawcache)
        fz_free_glyph_cache(_drawcache);
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

class PasswordCloner : public PasswordUI {
    unsigned char *cryptKey;

public:
    PasswordCloner(unsigned char *cryptKey) : cryptKey(cryptKey) { }

    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey)
    {
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return NULL;
    }
};

CPdfEngine *CPdfEngine::Clone()
{
    CPdfEngine *clone = new CPdfEngine();
    if (!clone)
        return NULL;

    // use this document's encryption key (if any) to load the clone
    PasswordCloner *pwdUI = NULL;
    if (_xref->crypt)
        pwdUI = new PasswordCloner(pdf_get_crypt_key(_xref));
    bool ok = clone->load(_xref->file, pwdUI);
    delete pwdUI;

    if (!ok) {
        delete clone;
        return NULL;
    }

    if (_fileName)
        clone->_fileName = Str::Dup(_fileName);
    if (!_decryptionKey && _xref->crypt) {
        delete clone->_decryptionKey;
        clone->_decryptionKey = NULL;
    }

    return clone;
}

bool CPdfEngine::load(const TCHAR *fileName, PasswordUI *pwdUI)
{
    assert(!_fileName && !_xref);
    _fileName = Str::Dup(fileName);
    if (!_fileName)
        return false;

    // File names ending in :<digits>:<digits> are interpreted as containing
    // embedded PDF documents (the digits are :<num>:<gen> of the embedded file stream)
    TCHAR *embedMarks = NULL;
    int colonCount = 0;
    for (TCHAR *c = (TCHAR *)_fileName + Str::Len(_fileName) - 1; c > _fileName; c--) {
        if (*c == ':') {
            if (!ChrIsDigit(*(c + 1)))
                break;
            if (++colonCount % 2 == 0)
                embedMarks = c;
        }
        else if (!ChrIsDigit(*c))
            break;
    }

    if (embedMarks)
        *embedMarks = '\0';
    fz_stream *file = fz_open_file2(_fileName);
    if (embedMarks)
        *embedMarks = ':';

OpenEmbeddedFile:
    if (!load_from_stream(file, pwdUI))
        return false;

    if (Str::IsEmpty(embedMarks))
        return finishLoading();

    int num, gen;
    embedMarks = (TCHAR *)Str::Parse(embedMarks, _T(":%d:%d"), &num, &gen);
    assert(embedMarks);
    if (!embedMarks || !pdf_is_stream(_xref, num, gen))
        return false;

    fz_buffer *buffer;
    fz_error error = pdf_load_stream(&buffer, _xref, num, gen);
    if (error)
        return false;

    file = fz_open_buffer(buffer);
    fz_drop_buffer(buffer);
    pdf_free_xref(_xref);
    _xref = NULL;

    goto OpenEmbeddedFile;
}

bool CPdfEngine::load(IStream *stream, PasswordUI *pwdUI)
{
    assert(!_fileName && !_xref);
    if (!load_from_stream(fz_open_istream(stream), pwdUI))
        return false;
    return finishLoading();
}

bool CPdfEngine::load(fz_stream *stm, PasswordUI *pwdUI)
{
    assert(!_fileName && !_xref);
    if (!load_from_stream(fz_keep_stream(stm), pwdUI))
        return false;
    return finishLoading();
}

bool CPdfEngine::load_from_stream(fz_stream *stm, PasswordUI *pwdUI)
{
    if (!stm)
        return false;

    // don't pass in a password so that _xref isn't thrown away if it was wrong
    fz_error error = pdf_open_xref_with_stream(&_xref, stm, NULL);
    fz_close(stm);
    if (error || !_xref)
        return false;

    if (pdf_needs_password(_xref)) {
        if (!pwdUI)
            return false;

        unsigned char digest[16 + 32] = { 0 };
        fz_stream_fingerprint(_xref->file, digest);

        bool okay = false, saveKey = false;
        for (int i = 0; !okay && i < 3; i++) {
            ScopedMem<TCHAR> pwd(pwdUI->GetPassword(_fileName, digest, pdf_get_crypt_key(_xref), &saveKey));
            if (!pwd) {
                // password not given or encryption key has been remembered
                okay = saveKey;
                break;
            }

            char *pwd_doc = Str::Conv::ToPDF(pwd);
            okay = pwd_doc && pdf_authenticate_password(_xref, pwd_doc);
            fz_free(pwd_doc);
            // try the UTF-8 password, if the PDFDocEncoding one doesn't work
            if (!okay) {
                ScopedMem<char> pwd_utf8(Str::Conv::ToUtf8(pwd));
                okay = pwd_utf8 && pdf_authenticate_password(_xref, pwd_utf8);
            }
            // fall back to an ANSI-encoded password as a last measure
            if (!okay) {
                ScopedMem<char> pwd_ansi(Str::Conv::ToAnsi(pwd));
                okay = pwd_ansi && pdf_authenticate_password(_xref, pwd_ansi);
            }
        }
        if (!okay)
            return false;

        if (saveKey) {
            memcpy(digest + 16, pdf_get_crypt_key(_xref), 32);
            _decryptionKey = _MemToHex(&digest);
        }
    }

    return true;
}

bool CPdfEngine::finishLoading()
{
    fz_error error = pdf_load_page_tree(_xref);
    if (error)
        return false;

    EnterCriticalSection(&_xrefAccess);
    _outline = pdf_load_outline(_xref);
    // silently ignore errors from pdf_loadoutline()
    // this information is not critical and checking the
    // error might prevent loading some pdfs that would
    // otherwise get displayed
    _attachments = pdf_loadattachments(_xref);
    // keep a copy of the Info dictionary, as accessing the original
    // isn't thread safe and we don't want to block for this when
    // displaying document properties
    _info = fz_dict_gets(_xref->trailer, "Info");
    if (_info)
        _info = fz_copy_dict(pdf_resolve_indirect(_info));
    LeaveCriticalSection(&_xrefAccess);

    _pages = SAZA(pdf_page *, PageCount());

    return PageCount() > 0;
}

CPdfTocItem *CPdfEngine::buildTocTree(pdf_outline *entry, int *idCounter)
{
    TCHAR *name = entry->title ? Str::Conv::FromUtf8(entry->title) : Str::Dup(_T(""));
    CPdfTocItem *node = new CPdfTocItem(name, PdfLink(entry->link));
    node->open = entry->count >= 0;
    node->id = ++(*idCounter);

    if (entry->link && PDF_LINK_GOTO == entry->link->kind)
        node->pageNo = FindPageNo(entry->link->dest);
    if (entry->child)
        node->child = buildTocTree(entry->child, idCounter);
    if (entry->next)
        node->next = buildTocTree(entry->next, idCounter);

    return node;
}

PdfTocItem *CPdfEngine::GetToCTree()
{
    CPdfTocItem *node = NULL;
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

int CPdfEngine::FindPageNo(fz_obj *dest)
{
    if (fz_is_dict(dest)) {
        // The destination is linked from a Go-To action's D array
        fz_obj * D = fz_dict_gets(dest, "D");
        if (D && fz_is_array(D))
            dest = D;
    }
    if (fz_is_array(dest))
        dest = fz_array_get(dest, 0);
    if (fz_is_int(dest))
        return fz_to_int(dest) + 1;

    return pdf_find_page_number(_xref, dest) + 1;
}

fz_obj *CPdfEngine::GetNamedDest(const TCHAR *name)
{
    ScopedMem<char> name_utf8(Str::Conv::ToUtf8(name));
    fz_obj *nameobj = fz_new_string((char *)name_utf8, (int)strlen(name_utf8));
    fz_obj *dest = pdf_lookup_dest(_xref, nameobj);
    fz_drop_obj(nameobj);

    // names refer to either an array or a dictionary with an array /D
    if (fz_is_dict(dest))
        dest = fz_dict_gets(dest, "D");
    if (fz_is_array(dest))
        return dest;
    return NULL;
}

pdf_page *CPdfEngine::getPdfPage(int pageNo, bool failIfBusy)
{
    if (!_pages)
        return NULL;
    if (failIfBusy)
        return _pages[pageNo-1];

    EnterCriticalSection(&_pagesAccess);

    pdf_page *page = _pages[pageNo-1];
    if (!page) {
        EnterCriticalSection(&_xrefAccess);
        fz_error error = pdf_load_page(&page, _xref, pageNo - 1);
        LeaveCriticalSection(&_xrefAccess);
        if (!error) {
            linkifyPageText(page);
            _pages[pageNo-1] = page;
        }
    }

    LeaveCriticalSection(&_pagesAccess);

    return page;
}

PdfPageRun *CPdfEngine::getPageRun(pdf_page *page, bool tryOnly)
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

        fz_display_list *list = fz_new_display_list();

        fz_device *dev = fz_new_list_device(list);
        EnterCriticalSection(&_xrefAccess);
        fz_error error = pdf_run_page(_xref, page, dev, fz_identity);
        fz_free_device(dev);
        if (error)
            fz_free_display_list(list);
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

fz_error CPdfEngine::runPage(pdf_page *page, fz_device *dev, fz_matrix ctm, RenderTarget target, fz_bbox clipbox, bool cacheRun)
{
    fz_error error = fz_okay;
    PdfPageRun *run;

    if (Target_View == target && (run = getPageRun(page, !cacheRun))) {
        EnterCriticalSection(&_xrefAccess);
        fz_execute_display_list(run->list, dev, ctm, clipbox);
        LeaveCriticalSection(&_xrefAccess);
        dropPageRun(run);
    }
    else {
        char *targetName = target == Target_Print ? "Print" :
                           target == Target_Export ? "Export" : "View";
        EnterCriticalSection(&_xrefAccess);
        error = pdf_run_page_with_usage(_xref, page, dev, ctm, targetName);
        LeaveCriticalSection(&_xrefAccess);
    }
    fz_free_device(dev);

    return error;
}

void CPdfEngine::dropPageRun(PdfPageRun *run, bool forceRemove)
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
            fz_free_display_list(run->list);
            LeaveCriticalSection(&_xrefAccess);
            free(run);
        }
    }

    LeaveCriticalSection(&_pagesAccess);
}

int CPdfEngine::PageRotation(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    fz_obj *page = _xref->page_objs[pageNo-1];
    if (!page)
        return 0;
    int rotation = fz_to_int(fz_dict_gets(page, "Rotate"));
    if ((rotation % 90) != 0)
        return 0;
    return rotation;
}

RectD CPdfEngine::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    fz_rect mediabox;
    if (pdf_get_mediabox(&mediabox, _xref->page_objs[pageNo-1]) != fz_okay)
        return RectD();
    return fz_rect_to_RectD(mediabox);
}

RectI CPdfEngine::PageContentBox(int pageNo, RenderTarget target)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    pdf_page *page = getPdfPage(pageNo);
    if (!page)
        return RectI();

    fz_bbox bbox;
    fz_error error = runPage(page, fz_new_bbox_device(&bbox), fz_identity, target, fz_round_rect(page->mediabox), false);
    if (error != fz_okay)
        return PageMediabox(pageNo).Round();
    if (fz_is_infinite_bbox(bbox))
        return PageMediabox(pageNo).Round();

    RectI bbox2 = fz_bbox_to_RectI(bbox);
    return bbox2.Intersect(PageMediabox(pageNo).Round());
}

bool CPdfEngine::hasPermission(int permission)
{
    return (bool)pdf_has_permission(_xref, permission);
}

fz_matrix CPdfEngine::viewctm(pdf_page *page, float zoom, int rotate)
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

fz_matrix CPdfEngine::viewctm(int pageNo, float zoom, int rotate)
{
    pdf_page partialPage;
    fz_obj *page = _xref->page_objs[pageNo-1];

    if (!page || pdf_get_mediabox(&partialPage.mediabox, page) != fz_okay)
        return fz_identity;
    partialPage.rotate = fz_to_int(fz_dict_gets(page, "Rotate"));

    return viewctm(&partialPage, zoom, rotate);
}

PointD CPdfEngine::Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse)
{
    fz_point pt2 = { (float)pt.x, (float)pt.y };
    fz_matrix ctm = viewctm(pageNo, zoom, rotate);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    pt2 = fz_transform_point(ctm, pt2);
    return PointD(pt2.x, pt2.y);
}

RectD CPdfEngine::Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse)
{
    fz_rect rect2 = fz_RectD_to_rect(rect);
    fz_matrix ctm = viewctm(pageNo, zoom, rotate);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    rect2 = fz_transform_rect(ctm, rect2);
    return fz_rect_to_RectD(rect2);
}

bool CPdfEngine::renderPage(HDC hDC, pdf_page *page, RectI screenRect, fz_matrix *ctm, float zoom, int rotation, RectD *pageRect, RenderTarget target)
{
    if (!page)
        return false;

    fz_matrix ctm2;
    if (!ctm) {
        if (!zoom)
            zoom = min(1.0f * screenRect.dx / (page->mediabox.x1 - page->mediabox.x0),
                       1.0f * screenRect.dy / (page->mediabox.y1 - page->mediabox.y0));
        ctm2 = viewctm(page, zoom, rotation);
        fz_rect pRect = pageRect ? fz_RectD_to_rect(*pageRect) : page->mediabox;
        fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm2, pRect));
        ctm2 = fz_concat(ctm2, fz_translate((float)screenRect.x - bbox.x0, (float)screenRect.y - bbox.y0));
        ctm = &ctm2;
    }

    HBRUSH bgBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
    FillRect(hDC, &screenRect.ToRECT(), bgBrush); // initialize white background
    DeleteObject(bgBrush);

    fz_bbox clipbox = fz_RectI_to_bbox(screenRect);
    fz_error error = runPage(page, fz_new_gdiplus_device(hDC, clipbox), *ctm, target, clipbox);

    return fz_okay == error;
}

RenderedBitmap *CPdfEngine::RenderBitmap(
                           int pageNo, float zoom, int rotation,
                           RectD *pageRect, RenderTarget target,
                           bool useGdi)
{
    pdf_page* page = getPdfPage(pageNo);
    if (!page)
        return NULL;

    fz_rect pRect = pageRect ? fz_RectD_to_rect(*pageRect) : page->mediabox;
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm, pRect));

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
        RectD pageRect2 = fz_rect_to_RectD(pRect);
        bool ok = renderPage(hDCMem, page, rc, &ctm, 0, 0, &pageRect2, target);
        DeleteDC(hDCMem);
        ReleaseDC(NULL, hDC);
        if (!ok) {
            DeleteObject(hbmp);
            return NULL;
        }
        return new RenderedBitmap(hbmp, w, h);
    }

    fz_pixmap *image = fz_new_pixmap_with_limit(fz_find_device_colorspace("DeviceRGB"),
        bbox.x1 - bbox.x0, bbox.y1 - bbox.y0);
    if (!image)
        return NULL;
    image->x = bbox.x0; image->y = bbox.y0;

    fz_clear_pixmap_with_color(image, 255); // initialize white background
    if (!_drawcache)
        _drawcache = fz_new_glyph_cache();

    fz_error error = runPage(page, fz_new_draw_device(_drawcache, image), ctm, target, bbox);
    RenderedBitmap *bitmap = NULL;
    if (!error) {
        HDC hDC = GetDC(NULL);
        bitmap = new RenderedFitzBitmap(image, hDC);
        ReleaseDC(NULL, hDC);
    }
    fz_drop_pixmap(image);
    return bitmap;
}

PageElement *CPdfEngine::GetElementAtPos(int pageNo, PointD pt)
{
    pdf_page *page = getPdfPage(pageNo, true);
    if (!page)
        return NULL;

    fz_point p = { (float)pt.x, (float)pt.y };
    for (pdf_link *link = page->links; link; link = link->next)
        if (fz_is_pt_in_rect(link->rect, p))
            return new PdfLink(link, pageNo);

    for (pdf_annot *annot = page->annots; annot; annot = annot->next)
        if (fz_is_pt_in_rect(annot->rect, p) &&
            Str::Eq(fz_to_name(fz_dict_gets(annot->obj, "Subtype")), "Text") &&
            !Str::IsEmpty(fz_to_str_buf(fz_dict_gets(annot->obj, "Contents")))) {
            return new PdfComment(annot, pageNo);
        }

    return NULL;
}

Vec<PageElement *> *CPdfEngine::GetElements(int pageNo)
{
    Vec<PageElement *> *els = new Vec<PageElement *>();

    pdf_page *page = getPdfPage(pageNo, true);
    if (!page)
        return els;

    for (pdf_link *link = page->links; link; link = link->next)
        els->Append(new PdfLink(link, pageNo));

    for (pdf_annot *annot = page->annots; annot; annot = annot->next)
        if (Str::Eq(fz_to_name(fz_dict_gets(annot->obj, "Subtype")), "Text") &&
            !Str::IsEmpty(fz_to_str_buf(fz_dict_gets(annot->obj, "Contents")))) {
            els->Append(new PdfComment(annot, pageNo));
        }

    return els;
}

static pdf_link *getLastLink(pdf_link *head)
{
    if (head)
        while (head->next)
            head = head->next;

    return head;
}

static bool isMultilineLink(TCHAR *pageText, TCHAR *pos, RectI *coords)
{
    // multiline links end in a non-alphanumeric character and continue on a line
    // that starts left and only slightly below where the current line ended
    // (and that doesn't start with http itself)
    return
        '\n' == *pos && pos > pageText && !_istalnum(pos[-1]) && !_istspace(pos[1]) &&
        coords[pos - pageText + 1].BR().y > coords[pos - pageText - 1].y &&
        coords[pos - pageText + 1].y <= coords[pos - pageText - 1].BR().y &&
        coords[pos - pageText + 1].x < coords[pos - pageText - 1].BR().x &&
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

static TCHAR *parseMultilineLink(pdf_page *page, TCHAR *pageText, TCHAR *start, RectI *coords)
{
    pdf_link *firstLink = getLastLink(page->links);
    char *uri = Str::Dup(fz_to_str_buf(firstLink->dest));
    TCHAR *end = start;
    bool multiline = false;

    do {
        end = findLinkEnd(start);
        multiline = isMultilineLink(pageText, end, coords);
        *end = 0;

        // add a new link for this line
        RectI bbox = coords[start - pageText].Union(coords[end - pageText - 1]);
        ScopedMem<char> uriPart(Str::Conv::ToUtf8(start));
        char *newUri = Str::Join(uri, uriPart);
        free(uri);
        uri = newUri;

        pdf_link *link = pdf_new_link(NULL, PDF_LINK_URI);
        link->rect = fz_RectD_to_rect(bbox.Convert<double>());
        getLastLink(firstLink)->next = link;

        start = end + 1;
    } while (multiline);

    // update the link URL for all partial links
    fz_drop_obj(firstLink->dest);
    firstLink->dest = fz_new_string(uri, (int)strlen(uri));
    for (pdf_link *link = firstLink->next; link; link = link->next)
        link->dest = fz_keep_obj(firstLink->dest);
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

void CPdfEngine::linkifyPageText(pdf_page *page)
{
    FixupPageLinks(page);

    RectI *coords;
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
        fz_bbox bbox = fz_RectI_to_bbox(coords[start - pageText].Union(coords[end - pageText - 1]));
        for (pdf_link *link = page->links; link && *start; link = link->next) {
            fz_bbox isect = fz_intersect_bbox(bbox, fz_round_rect(link->rect));
            if (!fz_is_empty_bbox(isect) && fz_sizeofrect(isect) >= 0.25 * fz_sizeofrect(link->rect))
                start = end;
        }

        // add the link, if it's a new one (ignoring www. links without a toplevel domain)
        if (*start && (Str::StartsWith(start, _T("http")) || _tcschr(start + 5, '.') != NULL)) {
            char *uri = Str::Conv::ToUtf8(start);
            char *httpUri = Str::StartsWith(uri, "http") ? uri : Str::Join("http://", uri);
            fz_obj *dest = fz_new_string(httpUri, (int)strlen(httpUri));
            pdf_link *link = pdf_new_link(dest, PDF_LINK_URI);
            link->rect = fz_bbox_to_rect(bbox);
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

TCHAR *CPdfEngine::ExtractPageText(pdf_page *page, TCHAR *lineSep, RectI **coords_out, RenderTarget target, bool cacheRun)
{
    if (!page)
        return NULL;

    fz_text_span *text = fz_new_text_span();
    // use an infinite rectangle as bounds (instead of page->mediabox) to ensure that
    // the extracted text is consistent between cached runs using a list device and
    // fresh runs (otherwise the list device omits text outside the mediabox bounds)
    fz_error error = runPage(page, fz_new_text_device(text), fz_identity, target, fz_infinite_bbox, cacheRun);

    WCHAR *content = NULL;
    if (!error)
        content = fz_span_to_wchar(text, lineSep, coords_out);

    EnterCriticalSection(&_xrefAccess);
    fz_free_text_span(text);
    LeaveCriticalSection(&_xrefAccess);

    return Str::Conv::FromWStrQ(content);
}

TCHAR *CPdfEngine::ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out, RenderTarget target)
{
    pdf_page *page = getPdfPage(pageNo, true);
    if (page)
        return ExtractPageText(page, lineSep, coords_out, target);

    EnterCriticalSection(&_xrefAccess);
    fz_error error = pdf_load_page(&page, _xref, pageNo - 1);
    LeaveCriticalSection(&_xrefAccess);
    if (error)
        return NULL;

    TCHAR *result = ExtractPageText(page, lineSep, coords_out, target);
    pdf_free_page(page);

    return result;
}

TCHAR *CPdfEngine::GetProperty(char *name)
{
    if (!_xref)
        return NULL;

    if (Str::Eq(name, "PdfVersion")) {
        int major = _xref->version / 10, minor = _xref->version % 10;
        if (1 == major && 7 == minor && 5 == pdf_get_crypt_revision(_xref))
            return Str::Format(_T("%d.%d Adobe Extension Level %d"), major, minor, 3);
        return Str::Format(_T("%d.%d"), major, minor);
    }

    fz_obj *obj = fz_dict_gets(_info, name);
    if (!obj)
        return NULL;

    WCHAR *ucs2 = (WCHAR *)pdf_to_ucs2(obj);
    TCHAR *tstr = Str::Conv::FromWStr(ucs2);
    fz_free(ucs2);

    return tstr;
};

char *CPdfEngine::GetDecryptionKey() const
{
    if (!_decryptionKey)
        return NULL;
    return Str::Dup(_decryptionKey);
}

PageLayoutType CPdfEngine::PreferredLayout()
{
    PageLayoutType layout = Layout_Single;

    ScopedCritSec scope(&_xrefAccess);
    fz_obj *root = fz_dict_gets(_xref->trailer, "Root");

    char *name = fz_to_name(fz_dict_gets(root, "PageLayout"));
    if (Str::EndsWith(name, "Right"))
        layout = Layout_Book;
    else if (Str::StartsWith(name, "Two"))
        layout = Layout_Facing;

    fz_obj *prefs = fz_dict_gets(root, "ViewerPreferences");
    char *direction = fz_to_name(fz_dict_gets(prefs, "Direction"));
    if (Str::Eq(direction, "R2L"))
        layout = (PageLayoutType)(layout | Layout_R2L);

    return layout;
}

unsigned char *CPdfEngine::GetFileData(size_t *cbCount)
{
    ScopedCritSec scope(&_xrefAccess);
    return fz_extract_stream_data(_xref->file, cbCount);
}

bool CPdfEngine::SaveEmbedded(fz_obj *obj, LinkSaverUI& saveUI)
{
    fz_buffer *data = NULL;
    fz_error error = pdf_load_stream(&data, _xref, fz_to_num(obj), fz_to_gen(obj));
    if (error != fz_okay)
        return false;
    bool result = saveUI.SaveEmbedded(data->data, data->len);
    fz_drop_buffer(data);
    return result;
}

bool CPdfEngine::IsImagePage(int pageNo)
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

    bool hasSingleImage = fz_list_is_single_image(run->list);
    dropPageRun(run);

    return hasSingleImage;
}

void CPdfEngine::RunGC()
{
    EnterCriticalSection(&_xrefAccess);
    if (_xref && _xref->store)
        pdf_age_store(_xref->store, 3);
    LeaveCriticalSection(&_xrefAccess);
}

PdfEngine *PdfEngine::CreateFromFileName(const TCHAR *fileName, PasswordUI *pwdUI)
{
    PdfEngine *engine = new CPdfEngine();
    if (!engine || !fileName || !engine->load(fileName, pwdUI)) {
        delete engine;
        return NULL;
    }
    return engine;
}

PdfEngine *PdfEngine::CreateFromStream(IStream *stream, PasswordUI *pwdUI)
{
    PdfEngine *engine = new CPdfEngine();
    if (!engine || !stream || !engine->load(stream, pwdUI)) {
        delete engine;
        return NULL;
    }
    return engine;
}

///// XpsEngine is also based on Fitz and shares quite some code with PdfEngine /////

extern "C" {
#include <muxps.h>
}

struct XpsPageRun {
    xps_page *page;
    fz_display_list *list;
    int refs;
};

class CXpsEngine : public XpsEngine {
public:
    CXpsEngine();
    virtual ~CXpsEngine();
    virtual CXpsEngine *Clone();

    virtual const TCHAR *FileName() const { return _fileName; };
    virtual int PageCount() const {
        return _ctx ? xps_count_pages(_ctx) : 0;
    }

    virtual RectD PageMediabox(int pageNo);
    virtual RectI PageContentBox(int pageNo, RenderTarget target=Target_View);

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false);
    virtual bool RenderPage(HDC hDC, int pageNo, RectI screenRect,
                         float zoom=0, int rotation=0,
                         RectD *pageRect=NULL, RenderTarget target=Target_View) {
        return renderPage(hDC, getXpsPage(pageNo), screenRect, NULL, zoom, rotation, pageRect);
    }

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool HasTextContent() { return true; }
    virtual TCHAR * ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View);
    virtual bool IsImagePage(int pageNo) { return false; }
    virtual TCHAR *GetProperty(char *name) { return NULL; }

    virtual float GetFileDPI() const { return 96.0f; }
    virtual const TCHAR *GetDefaultFileExt() const { return _T(".xps"); }

    virtual bool BenchLoadPage(int pageNo) { return getXpsPage(pageNo) != NULL; }

protected:
    const TCHAR *_fileName;

    // make sure to never ask for _pagesAccess in an _ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION _ctxAccess;
    xps_context *    _ctx;

    CRITICAL_SECTION _pagesAccess;
    xps_page **     _pages;

    virtual bool    load(const TCHAR *fileName);
    virtual bool    load(IStream *stream);
    bool            load(fz_stream *stm);
    bool            load_from_stream(fz_stream *stm);

    xps_page      * getXpsPage(int pageNo, bool failIfBusy=false);
    fz_matrix       viewctm(int pageNo, float zoom, int rotate) {
        return viewctm(getXpsPage(pageNo), zoom, rotate);
    }
    fz_matrix       viewctm(xps_page *page, float zoom, int rotate);
    bool            renderPage(HDC hDC, xps_page *page, RectI screenRect,
                               fz_matrix *ctm=NULL, float zoom=0, int rotation=0,
                               RectD *pageRect=NULL);
    TCHAR         * ExtractPageText(xps_page *page, TCHAR *lineSep,
                                    RectI **coords_out=NULL, bool cacheRun=false);

    XpsPageRun    * _runCache[MAX_PAGE_RUN_CACHE];
    XpsPageRun    * getPageRun(xps_page *page, bool tryOnly=false);
    void            runPage(xps_page *page, fz_device *dev, fz_matrix ctm,
                            fz_bbox clipbox=fz_infinite_bbox, bool cacheRun=true);
    void            dropPageRun(XpsPageRun *run, bool forceRemove=false);

    fz_glyph_cache* _drawcache;
};

static void
xps_run_page(xps_context *ctx, xps_page *page, fz_device *dev, fz_matrix ctm)
{
    ctx->dev = dev;
    xps_parse_fixed_page(ctx, ctm, page);
    ctx->dev = NULL;
}

CXpsEngine::CXpsEngine() : _fileName(NULL), _ctx(NULL), _pages(NULL), _drawcache(NULL)
{
    InitializeCriticalSection(&_pagesAccess);
    InitializeCriticalSection(&_ctxAccess);
    ZeroMemory(&_runCache, sizeof(_runCache));
}

CXpsEngine::~CXpsEngine()
{
    EnterCriticalSection(&_pagesAccess);
    EnterCriticalSection(&_ctxAccess);

    if (_pages) {
        for (int i=0; i < PageCount(); i++) {
            if (_pages[i])
                xps_free_page(_ctx, _pages[i]);
        }
        free(_pages);
    }

    if (_ctx) {
        xps_free_context(_ctx);
        _ctx = NULL;
    }

    if (_drawcache)
        fz_free_glyph_cache(_drawcache);
    while (_runCache[0]) {
        assert(_runCache[0]->refs == 1);
        dropPageRun(_runCache[0], true);
    }
    free((void*)_fileName);

    LeaveCriticalSection(&_ctxAccess);
    DeleteCriticalSection(&_ctxAccess);
    LeaveCriticalSection(&_pagesAccess);
    DeleteCriticalSection(&_pagesAccess);
}

CXpsEngine *CXpsEngine::Clone()
{
    CXpsEngine *clone = new CXpsEngine();
    if (!clone || !clone->load(_ctx->file)) {
        delete clone;
        return NULL;
    }

    if (_fileName)
        clone->_fileName = Str::Dup(_fileName);

    return clone;
}

bool CXpsEngine::load(const TCHAR *fileName)
{
    assert(!_fileName && !_ctx);
    _fileName = Str::Dup(fileName);
    if (!_fileName)
        return false;
    return load_from_stream(fz_open_file2(_fileName));
}

bool CXpsEngine::load(IStream *stream)
{
    assert(!_fileName && !_ctx);
    return load_from_stream(fz_open_istream(stream));
}

bool CXpsEngine::load(fz_stream *stm)
{
    assert(!_fileName && !_ctx);
    return load_from_stream(fz_keep_stream(stm));
}

bool CXpsEngine::load_from_stream(fz_stream *stm)
{
    if (!stm)
        return false;

    xps_open_stream(&_ctx, stm);
    fz_close(stm);
    if (!_ctx)
        return false;

    _pages = SAZA(xps_page *, PageCount());
    // TODO: extract document properties from the "/docProps/core.xml" part

    return true;
}

xps_page *CXpsEngine::getXpsPage(int pageNo, bool failIfBusy)
{
    if (!_pages)
        return NULL;
    if (failIfBusy)
        return _pages[pageNo-1];

    EnterCriticalSection(&_pagesAccess);

    xps_page *page = _pages[pageNo-1];
    if (!page) {
        ScopedCritSec scope(&_ctxAccess);
        int error = xps_load_page(&page, _ctx, pageNo - 1);
        if (!error)
            _pages[pageNo-1] = page;
    }

    LeaveCriticalSection(&_pagesAccess);

    return page;
}

XpsPageRun *CXpsEngine::getPageRun(xps_page *page, bool tryOnly)
{
    ScopedCritSec scope(&_pagesAccess);

    XpsPageRun *result = NULL;
    int i;

    for (i = 0; i < MAX_PAGE_RUN_CACHE && _runCache[i] && !result; i++)
        if (_runCache[i]->page == page)
            result = _runCache[i];
    if (!result && !tryOnly) {
        if (MAX_PAGE_RUN_CACHE == i) {
            dropPageRun(_runCache[0], true);
            i--;
        }

        fz_display_list *list = fz_new_display_list();

        fz_device *dev = fz_new_list_device(list);
        EnterCriticalSection(&_ctxAccess);
        xps_run_page(_ctx, page, dev, fz_identity);
        fz_free_device(dev);
        LeaveCriticalSection(&_ctxAccess);

        XpsPageRun newRun = { page, list, 1 };
        result = _runCache[i] = (XpsPageRun *)_memdup(&newRun);
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
    return result;
}

void CXpsEngine::runPage(xps_page *page, fz_device *dev, fz_matrix ctm, fz_bbox clipbox, bool cacheRun)
{
    XpsPageRun *run;

    if ((run = getPageRun(page, !cacheRun))) {
        EnterCriticalSection(&_ctxAccess);
        fz_execute_display_list(run->list, dev, ctm, clipbox);
        LeaveCriticalSection(&_ctxAccess);
        dropPageRun(run);
    }
    else {
        ScopedCritSec scope(&_ctxAccess);
        xps_run_page(_ctx, page, dev, ctm);
    }
    fz_free_device(dev);
}

void CXpsEngine::dropPageRun(XpsPageRun *run, bool forceRemove)
{
    ScopedCritSec scope(&_pagesAccess);
    run->refs--;

    if (0 == run->refs || forceRemove) {
        int i;
        for (i = 0; i < MAX_PAGE_RUN_CACHE && _runCache[i] != run; i++);
        if (i < MAX_PAGE_RUN_CACHE) {
            memmove(&_runCache[i], &_runCache[i+1], (MAX_PAGE_RUN_CACHE - i - 1) * sizeof(XpsPageRun *));
            _runCache[MAX_PAGE_RUN_CACHE-1] = NULL;
        }
        if (0 == run->refs) {
            ScopedCritSec scope(&_ctxAccess);
            fz_free_display_list(run->list);
            free(run);
        }
    }
}

RectD CXpsEngine::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    xps_page *page = getXpsPage(pageNo);
    if (!page)
        return RectD();

    return RectD(0, 0, page->width, page->height);
}

RectI CXpsEngine::PageContentBox(int pageNo, RenderTarget target)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    xps_page *page = getXpsPage(pageNo);
    if (!page)
        return RectI();

    fz_bbox bbox;
    runPage(page, fz_new_bbox_device(&bbox), fz_identity);
    if (fz_is_infinite_bbox(bbox))
        return PageMediabox(pageNo).Round();

    RectI bbox2 = fz_bbox_to_RectI(bbox);
    return bbox2.Intersect(PageMediabox(pageNo).Round());
}

fz_matrix CXpsEngine::viewctm(xps_page *page, float zoom, int rotate)
{
    fz_matrix ctm = fz_identity;
    if (!page)
        return ctm;

    rotate = rotate % 360;
    if (rotate < 0) rotate = rotate + 360;
    if (90 == rotate)
        ctm = fz_concat(ctm, fz_translate(0, (float)-page->height));
    else if (180 == rotate)
        ctm = fz_concat(ctm, fz_translate((float)-page->width, (float)-page->height));
    else if (270 == rotate)
        ctm = fz_concat(ctm, fz_translate((float)-page->width, 0));
    else // if (0 == rotate)
        ctm = fz_concat(ctm, fz_translate(0, 0));

    ctm = fz_concat(ctm, fz_scale(zoom, zoom));
    ctm = fz_concat(ctm, fz_rotate((float)rotate));
    return ctm;
}

PointD CXpsEngine::Transform(PointD pt, int pageNo, float zoom, int rotate, bool inverse)
{
    fz_point pt2 = { (float)pt.x, (float)pt.y };
    fz_matrix ctm = viewctm(pageNo, zoom, rotate);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    pt2 = fz_transform_point(ctm, pt2);
    return PointD(pt2.x, pt2.y);
}

RectD CXpsEngine::Transform(RectD rect, int pageNo, float zoom, int rotate, bool inverse)
{
    fz_rect rect2 = fz_RectD_to_rect(rect);
    fz_matrix ctm = viewctm(pageNo, zoom, rotate);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    rect2 = fz_transform_rect(ctm, rect2);
    return fz_rect_to_RectD(rect2);
}

bool CXpsEngine::renderPage(HDC hDC, xps_page *page, RectI screenRect, fz_matrix *ctm, float zoom, int rotation, RectD *pageRect)
{
    if (!page)
        return false;

    fz_matrix ctm2;
    if (!ctm) {
        fz_bbox mediabox = { 0, 0, page->width, page->height };
        if (!zoom)
            zoom = min(1.0f * screenRect.dx / (mediabox.x1 - mediabox.x0),
                       1.0f * screenRect.dy / (mediabox.y1 - mediabox.y0));
        ctm2 = viewctm(page, zoom, rotation);
        fz_rect pRect = pageRect ? fz_RectD_to_rect(*pageRect) : fz_bbox_to_rect(mediabox);
        fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm2, pRect));
        ctm2 = fz_concat(ctm2, fz_translate((float)screenRect.x - bbox.x0, (float)screenRect.y - bbox.y0));
        ctm = &ctm2;
    }

    HBRUSH bgBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
    FillRect(hDC, &screenRect.ToRECT(), bgBrush); // initialize white background
    DeleteObject(bgBrush);

    fz_bbox clipbox = fz_RectI_to_bbox(screenRect);
    runPage(page, fz_new_gdiplus_device(hDC, clipbox), *ctm, clipbox);

    return true;
}

RenderedBitmap *CXpsEngine::RenderBitmap(
                           int pageNo, float zoom, int rotation,
                           RectD *pageRect, RenderTarget target,
                           bool useGdi)
{
    xps_page* page = getXpsPage(pageNo);
    if (!page)
        return NULL;

    fz_bbox mediabox = { 0, 0, page->width, page->height };
    fz_rect pRect = pageRect ? fz_RectD_to_rect(*pageRect) : fz_bbox_to_rect(mediabox);
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm, pRect));

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
        bool ok = renderPage(hDCMem, page, rc, &ctm, 0, 0, pageRect);
        DeleteDC(hDCMem);
        ReleaseDC(NULL, hDC);
        if (!ok) {
            DeleteObject(hbmp);
            return NULL;
        }
        return new RenderedBitmap(hbmp, w, h);
    }

    fz_pixmap *image = fz_new_pixmap_with_limit(fz_find_device_colorspace("DeviceRGB"),
        bbox.x1 - bbox.x0, bbox.y1 - bbox.y0);
    if (!image)
        return NULL;
    image->x = bbox.x0; image->y = bbox.y0;

    fz_clear_pixmap_with_color(image, 255); // initialize white background
    if (!_drawcache)
        _drawcache = fz_new_glyph_cache();

    runPage(page, fz_new_draw_device(_drawcache, image), ctm, bbox);
    RenderedBitmap *bitmap = NULL;
    HDC hDC = GetDC(NULL);
    bitmap = new RenderedFitzBitmap(image, hDC);
    ReleaseDC(NULL, hDC);
    fz_drop_pixmap(image);
    return bitmap;
}

TCHAR *CXpsEngine::ExtractPageText(xps_page *page, TCHAR *lineSep, RectI **coords_out, bool cacheRun)
{
    if (!page)
        return NULL;

    fz_text_span *text = fz_new_text_span();
    // use an infinite rectangle as bounds (instead of a mediabox) to ensure that
    // the extracted text is consistent between cached runs using a list device and
    // fresh runs (otherwise the list device omits text outside the mediabox bounds)
    runPage(page, fz_new_text_device(text), fz_identity, fz_infinite_bbox, cacheRun);

    WCHAR *content = fz_span_to_wchar(text, lineSep, coords_out);

    EnterCriticalSection(&_ctxAccess);
    fz_free_text_span(text);
    LeaveCriticalSection(&_ctxAccess);

    return Str::Conv::FromWStrQ(content);
}

TCHAR *CXpsEngine::ExtractPageText(int pageNo, TCHAR *lineSep, RectI **coords_out, RenderTarget target)
{
    xps_page *page = getXpsPage(pageNo, true);
    if (page)
        return ExtractPageText(page, lineSep, coords_out);

    EnterCriticalSection(&_ctxAccess);
    int error = xps_load_page(&page, _ctx, pageNo - 1);
    LeaveCriticalSection(&_ctxAccess);
    if (error)
        return NULL;

    TCHAR *result = ExtractPageText(page, lineSep, coords_out);

    EnterCriticalSection(&_ctxAccess);
    xps_free_page(_ctx, page);
    LeaveCriticalSection(&_ctxAccess);

    return result;
}

unsigned char *CXpsEngine::GetFileData(size_t *cbCount)
{
    ScopedCritSec scope(&_ctxAccess);
    return fz_extract_stream_data(_ctx->file, cbCount);
}

XpsEngine *XpsEngine::CreateFromFileName(const TCHAR *fileName)
{
    XpsEngine *engine = new CXpsEngine();
    if (!engine || !fileName || !engine->load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

XpsEngine *XpsEngine::CreateFromStream(IStream *stream)
{
    XpsEngine *engine = new CXpsEngine();
    if (!engine || !stream || !engine->load(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}
