/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <fitz-internal.h>
}

#include "BaseUtil.h"
#include "PdfEngine.h"

#include "FileUtil.h"
#include "ZipUtil.h"

// maximum size of a file that's entirely loaded into memory before parsed
// and displayed; larger files will be kept open while they're displayed
// so that their content can be loaded on demand in order to preserve memory
#define MAX_MEMORY_FILE_SIZE (10 * 1024 * 1024)

// number of page content trees to cache for quicker rendering
#define MAX_PAGE_RUN_CACHE  8
// maximum estimated memory requirement allowed for the run cache of one document
#define MAX_PAGE_RUN_MEMORY (40 * 1024 * 1024)

// maximum amount of memory that MuPDF should use per fz_context store
#define MAX_CONTEXT_MEMORY  (256 * 1024 * 1024)

// when set, always uses GDI+ for rendering (else GDI+ is only used for
// zoom levels above 4000% and for rendering directly into an HDC)
static bool gDebugGdiPlusDevice = false;

void DebugGdiPlusDevice(bool enable)
{
    gDebugGdiPlusDevice = enable;
}

void CalcMD5Digest(unsigned char *data, size_t byteCount, unsigned char digest[16])
{
    fz_md5 md5;
    fz_md5_init(&md5);
#ifdef _WIN64
    for (; byteCount > UINT_MAX; data += UINT_MAX, byteCount -= UINT_MAX)
        fz_md5_update(&md5, data, UINT_MAX);
#endif
    fz_md5_update(&md5, data, (unsigned int)byteCount);
    fz_md5_final(&md5, digest);
}

///// extensions to Fitz that are usable for both PDF and XPS /////

inline fz_rect fz_bbox_to_rect(fz_bbox bbox)
{
    fz_rect result = { (float)bbox.x0, (float)bbox.y0, (float)bbox.x1, (float)bbox.y1 };
    return result;
}

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

inline bool fz_is_pt_in_rect(fz_rect rect, fz_point pt)
{
    return fz_rect_to_RectD(rect).Contains(PointD(pt.x, pt.y));
}

inline float fz_calc_overlap(fz_rect r1, fz_rect r2)
{
    if (r1.x0 == r1.x1 || r1.y0 == r1.y1)
        return 0.0f;
    fz_rect isect = fz_intersect_rect(r1, r2);
    return (isect.x1 - isect.x0) * (isect.y1 - isect.y0) / ((r1.x1 - r1.x0) * (r1.y1 - r1.y0));
}

class RenderedFitzBitmap : public RenderedBitmap {
public:
    RenderedFitzBitmap(fz_context *ctx, fz_pixmap *pixmap);
};

RenderedFitzBitmap::RenderedFitzBitmap(fz_context *ctx, fz_pixmap *pixmap) :
    RenderedBitmap(NULL, SizeI(pixmap->w, pixmap->h))
{
    int paletteSize = 0;
    bool hasPalette = false;

    int w = pixmap->w;
    int h = pixmap->h;
    int rows8 = ((w + 3) / 4) * 4;

    BITMAPINFO *bmi = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));

    // always try to produce an 8-bit palette for saving some memory
    unsigned char *bmpData = (unsigned char *)calloc(rows8, h);
    if (!bmpData) {
        free(bmi);
        return;
    }
    fz_pixmap *bgrPixmap = NULL;
    if (bmpData && pixmap->n == 4 &&
        pixmap->colorspace == fz_find_device_colorspace(ctx, "DeviceRGB"))
    {
        unsigned char *dest = bmpData;
        unsigned char *source = pixmap->samples;

        for (int j = 0; j < h; j++)
        {
            for (int i = 0; i < w; i++)
            {
                RGBQUAD c = { 0 };

                c.rgbRed = *source++;
                c.rgbGreen = *source++;
                c.rgbBlue = *source++;
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
    if (!hasPalette) {
        free(bmpData);
        /* BGRA is a GDI compatible format */
        fz_try(ctx) {
            fz_colorspace *colorspace = fz_find_device_colorspace(ctx, "DeviceBGR");
            bgrPixmap = fz_new_pixmap_with_bbox(ctx, colorspace, fz_pixmap_bbox(ctx, pixmap));
            fz_convert_pixmap(ctx, bgrPixmap, pixmap);
        }
        fz_catch(ctx) {
            free(bmi);
            return;
        }
    }
    AssertCrash(hasPalette || bgrPixmap);

    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = w;
    bmi->bmiHeader.biHeight = -h;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biBitCount = hasPalette ? 8 : 32;
    bmi->bmiHeader.biSizeImage = h * (hasPalette ? rows8 : w * 4);
    bmi->bmiHeader.biClrUsed = hasPalette ? paletteSize : 0;

    HDC hDC = GetDC(NULL);
    hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT,
        hasPalette ? bmpData : bgrPixmap->samples, bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hDC);

    if (hasPalette)
        free(bmpData);
    else
        fz_drop_pixmap(ctx, bgrPixmap);
    free(bmi);
}

fz_stream *fz_open_file2(fz_context *ctx, const WCHAR *filePath)
{
    fz_stream *file = NULL;
    size_t fileSize = file::GetSize(filePath);
    // load small files entirely into memory so that they can be
    // overwritten even by programs that don't open files with FILE_SHARE_READ
    if (fileSize < MAX_MEMORY_FILE_SIZE) {
        fz_buffer *data = NULL;
        fz_var(data);
        fz_try(ctx) {
            data = fz_new_buffer(ctx, (int)fileSize);
            if (file::ReadAll(filePath, (char *)data->data, (data->len = (int)fileSize)))
                file = fz_open_buffer(ctx, data);
        }
        fz_catch(ctx) {
            file = NULL;
        }
        fz_drop_buffer(ctx, data);
        if (file)
            return file;
    }

    fz_try(ctx) {
        file = fz_open_file_w(ctx, filePath);
    }
    fz_catch(ctx) {
        file = NULL;
    }
    return file;
}

unsigned char *fz_extract_stream_data(fz_stream *stream, size_t *cbCount)
{
    fz_seek(stream, 0, 2);
    int fileLen = fz_tell(stream);
    fz_seek(stream, 0, 0);

    fz_buffer *buffer = fz_read_all(stream, fileLen);
    assert(fileLen == buffer->len);

    unsigned char *data = (unsigned char *)memdup(buffer->data, buffer->len);
    if (cbCount)
        *cbCount = buffer->len;

    fz_drop_buffer(stream->ctx, buffer);

    if (!data)
        fz_throw(stream->ctx, "OOM in fz_extract_stream_data");
    return data;
}

void fz_stream_fingerprint(fz_stream *file, unsigned char digest[16])
{
    int fileLen;
    fz_buffer *buffer = NULL;

    fz_try(file->ctx) {
        fz_seek(file, 0, 2);
        fileLen = fz_tell(file);
        fz_seek(file, 0, 0);
        buffer = fz_read_all(file, fileLen);
    }
    fz_catch(file->ctx) {
        fz_warn(file->ctx, "couldn't read stream data, using a NULL fingerprint instead");
        ZeroMemory(digest, 16);
        return;
    }
    CrashIf(NULL == buffer);
    CrashIf(fileLen != buffer->len);

    CalcMD5Digest(buffer->data, buffer->len, digest);

    fz_drop_buffer(file->ctx, buffer);
}

WCHAR *fz_text_page_to_str(fz_text_page *text, WCHAR *lineSep, RectI **coords_out=NULL)
{
    size_t lineSepLen = str::Len(lineSep);
    size_t textLen = 0;
    for (fz_text_block *block = text->blocks; block < text->blocks + text->len; block++) {
        for (fz_text_line *line = block->lines; line < block->lines + block->len; line++) {
            for (fz_text_span *span = line->spans; span < line->spans + line->len; span++) {
                textLen += span->len;
            }
            textLen += lineSepLen;
        }
    }

    WCHAR *content = AllocArray<WCHAR>(textLen + 1);
    if (!content)
        return NULL;

    RectI *destRect = NULL;
    if (coords_out) {
        destRect = *coords_out = new RectI[textLen];
        if (!*coords_out) {
            free(content);
            return NULL;
        }
    }

    WCHAR *dest = content;
    for (fz_text_block *block = text->blocks; block < text->blocks + text->len; block++) {
        for (fz_text_line *line = block->lines; line < block->lines + block->len; line++) {
            for (fz_text_span *span = line->spans; span < line->spans + line->len; span++) {
                for (int i = 0; i < span->len; i++) {
                    *dest = span->text[i].c;
                    if (*dest <= 32) {
                        if (!str::IsWs(*dest))
                            *dest = '?';
                        // collapse multiple whitespace characters into one
                        else if (dest > content && !str::IsWs(dest[-1]))
                            *dest = ' ';
                        else
                            continue;
                    }
                    dest++;
                    if (destRect)
                        *destRect++ = fz_rect_to_RectD(span->text[i].bbox).Round();
                }
            }
            // remove trailing spaces
            if (lineSepLen > 0 && dest > content && str::IsWs(dest[-1])) {
                dest--;
                if (destRect)
                    destRect--;
            }
            lstrcpy(dest, lineSep);
            dest += lineSepLen;
            if (destRect) {
                ZeroMemory(destRect, lineSepLen * sizeof(fz_bbox));
                destRect += lineSepLen;
            }
        }
    }

    return content;
}

extern "C" static int read_istream(fz_stream *stm, unsigned char *buf, int len)
{
    ULONG cbRead = len;
    HRESULT res = ((IStream *)stm->state)->Read(buf, len, &cbRead);
    if (FAILED(res))
        fz_throw(stm->ctx, "IStream read error: %x", res);
    return (int)cbRead;
}

extern "C" static void seek_istream(fz_stream *stm, int offset, int whence)
{
    LARGE_INTEGER off;
    ULARGE_INTEGER n;
    off.QuadPart = offset;
    HRESULT res = ((IStream *)stm->state)->Seek(off, whence, &n);
    if (FAILED(res))
        fz_throw(stm->ctx, "IStream seek error: %x", res);
    if (n.HighPart != 0 || n.LowPart > INT_MAX)
        fz_throw(stm->ctx, "documents beyond 2GB aren't supported");
    stm->pos = n.LowPart;
    stm->rp = stm->wp = stm->bp;
}

extern "C" static void close_istream(fz_context *ctx, void *state)
{
    ((IStream *)state)->Release();
}

fz_stream *fz_open_istream(fz_context *ctx, IStream *stream);

extern "C" static fz_stream *reopen_istream(fz_context *ctx, fz_stream *stm)
{
    ScopedComPtr<IStream> stream2;
    HRESULT res = ((IStream *)stm->state)->Clone(&stream2);
    if (E_NOTIMPL == res)
        fz_throw(ctx, "IStream doesn't support cloning");
    if (FAILED(res))
        fz_throw(ctx, "IStream clone error: %x", res);
    return fz_open_istream(ctx, stream2);
}

fz_stream *fz_open_istream(fz_context *ctx, IStream *stream)
{
    if (!stream)
        return NULL;

    LARGE_INTEGER zero = { 0 };
    HRESULT res = stream->Seek(zero, STREAM_SEEK_SET, NULL);
    if (FAILED(res))
        fz_throw(ctx, "IStream seek error: %x", res);
    stream->AddRef();

    fz_stream *stm = fz_new_stream(ctx, stream, read_istream, close_istream);
    stm->seek = seek_istream;
    stm->reopen = reopen_istream;
    return stm;
}

fz_matrix fz_create_view_ctm(fz_rect mediabox, float zoom, int rotation)
{
    fz_matrix ctm = fz_identity;

    assert(0 == mediabox.x0 && 0 == mediabox.y0);
    rotation = (rotation + 360) % 360;
    if (90 == rotation)
        ctm = fz_concat(ctm, fz_translate(0, -mediabox.y1));
    else if (180 == rotation)
        ctm = fz_concat(ctm, fz_translate(-mediabox.x1, -mediabox.y1));
    else if (270 == rotation)
        ctm = fz_concat(ctm, fz_translate(-mediabox.x1, 0));

    ctm = fz_concat(ctm, fz_scale(zoom, zoom));
    ctm = fz_concat(ctm, fz_rotate((float)rotation));

    assert(fz_matrix_expansion(ctm) > 0);
    if (fz_matrix_expansion(ctm) == 0)
        return fz_identity;

    return ctm;
}

struct LinkRectList {
    WStrVec links;
    Vec<fz_rect> coords;
};

static bool LinkifyCheckMultiline(WCHAR *pageText, WCHAR *pos, RectI *coords)
{
    // multiline links end in a non-alphanumeric character and continue on a line
    // that starts left and only slightly below where the current line ended
    // (and that doesn't start with http itself)
    return
        '\n' == *pos && pos > pageText && *(pos + 1) &&
        !iswalnum(pos[-1]) && !iswspace(pos[1]) &&
        coords[pos - pageText + 1].BR().y > coords[pos - pageText - 1].y &&
        coords[pos - pageText + 1].y <= coords[pos - pageText - 1].BR().y &&
        coords[pos - pageText + 1].x < coords[pos - pageText - 1].BR().x &&
        !str::StartsWith(pos + 1, L"http");
}

static WCHAR *LinkifyFindEnd(WCHAR *start, bool atStart)
{
    WCHAR *end;

    // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
    for (end = start; *end && !iswspace(*end); end++);
    if (',' == end[-1] || '.' == end[-1] || '?' == end[-1] || '!' == end[-1])
        end--;
    // also ignore a closing parenthesis, if the URL doesn't contain any opening one
    if (')' == end[-1] && (!str::FindChar(start, '(') || str::FindChar(start, '(') >= end))
        end--;
    // cut the link at the first double quote if it's also preceded by one
    if (!atStart && start[-1] == '"' && str::FindChar(start, '"') && str::FindChar(start, '"') < end)
        end = (WCHAR *)str::FindChar(start, '"');

    return end;
}

static WCHAR *LinkifyMultilineText(LinkRectList *list, WCHAR *pageText, WCHAR *start, RectI *coords)
{
    size_t lastIx = list->coords.Count() - 1;
    ScopedMem<WCHAR> uri(list->links.At(lastIx));
    WCHAR *end = start;
    bool multiline = false;

    do {
        end = LinkifyFindEnd(start, start == pageText);
        multiline = LinkifyCheckMultiline(pageText, end, coords);
        *end = 0;

        uri.Set(str::Join(uri, start));
        RectI bbox = coords[start - pageText].Union(coords[end - pageText - 1]);
        list->coords.Append(fz_RectD_to_rect(bbox.Convert<double>()));

        start = end + 1;
    } while (multiline);

    // update the link URL for all partial links
    list->links.At(lastIx) = str::Dup(uri);
    for (size_t i = lastIx + 1; i < list->coords.Count(); i++)
        list->links.Append(str::Dup(uri));

    return end;
}

// cf. http://weblogs.mozillazine.org/gerv/archives/2011/05/html5_email_address_regexp.html
inline bool IsEmailUsernameChar(WCHAR c)
{
    // explicitly excluding the '/' from the list, as it is more
    // often part of a URL or path than of an email address
    return iswalnum(c) || c && str::FindChar(L".!#$%&'*+=?^_`{|}~-", c);
}
inline bool IsEmailDomainChar(WCHAR c)
{
    return iswalnum(c) || '-' == c;
}

static WCHAR *LinkifyFindEmail(WCHAR *pageText, WCHAR *at)
{
    WCHAR *start;
    for (start = at; start > pageText && IsEmailUsernameChar(*(start - 1)); start--);
    return start != at ? start : NULL;
}

static WCHAR *LinkifyEmailAddress(WCHAR *start)
{
    WCHAR *end;
    for (end = start; IsEmailUsernameChar(*end); end++);
    if (end == start || *end != '@' || !IsEmailDomainChar(*(end + 1)))
        return NULL;
    for (end++; IsEmailDomainChar(*end); end++);
    if ('.' != *end || !IsEmailDomainChar(*(end + 1)))
        return NULL;
    do {
        for (end++; IsEmailDomainChar(*end); end++);
    } while ('.' == *end && IsEmailDomainChar(*(end + 1)));
    return end;
}

// caller needs to delete the result
static LinkRectList *LinkifyText(WCHAR *pageText, RectI *coords)
{
    LinkRectList *list = new LinkRectList;

    for (WCHAR *start = pageText; *start; start++) {
        WCHAR *end = NULL;
        bool multiline = false;
        const WCHAR *protocol = NULL;

        if ('@' == *start) {
            // potential email address without mailto:
            WCHAR *email = LinkifyFindEmail(pageText, start);
            end = email ? LinkifyEmailAddress(email) : NULL;
            protocol = L"mailto:";
            if (end != NULL)
                start = email;
        }
        else if (start > pageText && ('/' == start[-1] || iswalnum(start[-1]))) {
            // hyperlinks must not be preceded by a slash (indicates a different protocol)
            // or an alphanumeric character (indicates part of a different protocol)
        }
        else if ('h' == *start && str::Parse(start, L"http%?s://")) {
            end = LinkifyFindEnd(start, start == pageText);
            multiline = LinkifyCheckMultiline(pageText, end, coords);
        }
        else if ('w' == *start && str::StartsWith(start, L"www.")) {
            end = LinkifyFindEnd(start, start == pageText);
            multiline = LinkifyCheckMultiline(pageText, end, coords);
            protocol = L"http://";
            // ignore www. links without a top-level domain
            if (end - start <= 4 || !multiline && (!wcschr(start + 5, '.') || wcschr(start + 5, '.') >= end))
                end = NULL;
        }
        else if ('m' == *start && str::StartsWith(start, L"mailto:")) {
            end = LinkifyEmailAddress(start + 7);
        }
        if (!end)
            continue;

        *end = 0;
        WCHAR *uri = protocol ? str::Join(protocol, start) : str::Dup(start);
        list->links.Append(uri);
        RectI bbox = coords[start - pageText].Union(coords[end - pageText - 1]);
        list->coords.Append(fz_RectD_to_rect(bbox.Convert<double>()));
        if (multiline)
            end = LinkifyMultilineText(list, pageText, end + 1, coords);

        start = end;
    }

    return list;
}

static fz_link *FixupPageLinks(fz_link *root)
{
    // Links in PDF documents are added from bottom-most to top-most,
    // i.e. links that appear later in the list should be preferred
    // to links appearing before. Since we search from the start of
    // the (single-linked) list, we have to reverse the order of links
    // (cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1303 )
    fz_link *new_root = NULL;
    while (root) {
        fz_link *tmp = root->next;
        root->next = new_root;
        new_root = root;
        root = tmp;

        // there are PDFs that have x,y positions in reverse order, so fix them up
        fz_link *link = new_root;
        if (link->rect.x0 > link->rect.x1)
            swap(link->rect.x0, link->rect.x1);
        if (link->rect.y0 > link->rect.y1)
            swap(link->rect.y0, link->rect.y1);
        assert(link->rect.x1 >= link->rect.x0);
        assert(link->rect.y1 >= link->rect.y0);
    }
    return new_root;
}

class SimpleDest : public PageDestination {
    int pageNo;
    RectD rect;

public:
    SimpleDest(int pageNo, RectD rect) : pageNo(pageNo), rect(rect) { }

    virtual PageDestType GetDestType() const { return Dest_ScrollTo; }
    virtual int GetDestPageNo() const { return pageNo; }
    virtual RectD GetDestRect() const { return rect; }
};

struct FitzImagePos {
    fz_image *image;
    fz_rect rect;

    FitzImagePos(fz_image *image=NULL, fz_rect rect=fz_unit_rect) :
        image(image), rect(rect) { }
};

struct ListInspectionData {
    Vec<FitzImagePos> *images;
    bool req_blending;
    bool req_t3_fonts;
    size_t mem_estimate;
    size_t path_len;
    size_t clip_path_len;

    ListInspectionData(Vec<FitzImagePos>& images) : images(&images),
        req_blending(false), req_t3_fonts(false), mem_estimate(0),
        path_len(0), clip_path_len(0) { }
};

extern "C" static void
fz_inspection_free(fz_device *dev)
{
    // images are extracted in bottom-to-top order, but for GetElements
    // we want to access them in top-to-bottom order (since images at
    // the bottom might not be visible at all)
    ((ListInspectionData *)dev->user)->images->Reverse();
}

static void fz_inspection_handle_path(fz_device *dev, fz_path *path, bool clipping=false)
{
    ListInspectionData *data = (ListInspectionData *)dev->user;
    if (!clipping)
        data->path_len += path->len;
    else
        data->clip_path_len += path->len;
    data->mem_estimate += path->cap * sizeof(fz_path_item);
}

static void fz_inspection_handle_text(fz_device *dev, fz_text *text)
{
    ((ListInspectionData *)dev->user)->req_t3_fonts = text->font->t3procs != NULL;
}

static void fz_inspection_handle_image(fz_device *dev, fz_image *image)
{
    int n = image->colorspace ? image->colorspace->n + 1 : 1;
    ((ListInspectionData *)dev->user)->mem_estimate += image->w * image->h * n;
}

extern "C" static void
fz_inspection_fill_path(fz_device *dev, fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha)
{
    fz_inspection_handle_path(dev, path);
}

extern "C" static void
fz_inspection_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha)
{
    fz_inspection_handle_path(dev, path);
}

extern "C" static void
fz_inspection_clip_path(fz_device *dev, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm)
{
    fz_inspection_handle_path(dev, path, true);
}

extern "C" static void
fz_inspection_clip_stroke_path(fz_device *dev, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
    fz_inspection_handle_path(dev, path, true);
}

extern "C" static void
fz_inspection_fill_text(fz_device *dev, fz_text *text, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha)
{
    fz_inspection_handle_text(dev, text);
}

extern "C" static void
fz_inspection_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha)
{
    fz_inspection_handle_text(dev, text);
}

extern "C" static void
fz_inspection_clip_text(fz_device *dev, fz_text *text, fz_matrix ctm, int accumulate)
{
    fz_inspection_handle_text(dev, text);
}

extern "C" static void
fz_inspection_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
    fz_inspection_handle_text(dev, text);
}

extern "C" static void
fz_inspection_fill_shade(fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha)
{
    ((ListInspectionData *)dev->user)->mem_estimate += sizeof(fz_shade);
}

extern "C" static void
fz_inspection_fill_image(fz_device *dev, fz_image *image, fz_matrix ctm, float alpha)
{
    fz_inspection_handle_image(dev, image);
    // extract rectangles for images a user might want to extract
    // TODO: try to better distinguish images a user might actually want to extract
    if (image->w < 16 || image->h < 16)
        return;
    fz_rect rect = fz_transform_rect(ctm, fz_unit_rect);
    if (!fz_is_empty_rect(rect))
        ((ListInspectionData *)dev->user)->images->Append(FitzImagePos(image, rect));
}

extern "C" static void
fz_inspection_fill_image_mask(fz_device *dev, fz_image *image, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha)
{
    fz_inspection_handle_image(dev, image);
}

extern "C" static void
fz_inspection_clip_image_mask(fz_device *dev, fz_image *image, fz_rect *rect, fz_matrix ctm)
{
    fz_inspection_handle_image(dev, image);
}

extern "C" static void
fz_inspection_begin_group(fz_device *dev, fz_rect rect, int isolated, int knockout, int blendmode, float alpha)
{
    if (blendmode != FZ_BLEND_NORMAL || alpha != 1.0f || !isolated || knockout)
        ((ListInspectionData *)dev->user)->req_blending = true;
}

static fz_device *fz_new_inspection_device(fz_context *ctx, ListInspectionData *data)
{
    fz_device *dev = fz_new_device(ctx, data);
    dev->free_user = fz_inspection_free;

    dev->fill_path = fz_inspection_fill_path;
    dev->stroke_path = fz_inspection_stroke_path;
    dev->clip_path = fz_inspection_clip_path;
    dev->clip_stroke_path = fz_inspection_clip_stroke_path;

    dev->fill_text = fz_inspection_fill_text;
    dev->stroke_text = fz_inspection_stroke_text;
    dev->clip_text = fz_inspection_clip_text;
    dev->clip_stroke_text = fz_inspection_clip_stroke_text;

    dev->fill_shade = fz_inspection_fill_shade;
    dev->fill_image = fz_inspection_fill_image;
    dev->fill_image_mask = fz_inspection_fill_image_mask;
    dev->clip_image_mask = fz_inspection_clip_image_mask;

    dev->begin_group = fz_inspection_begin_group;
    return dev;
}

class FitzAbortCookie : public AbortCookie {
public:
    fz_cookie cookie;
    FitzAbortCookie() { memset(&cookie, 0, sizeof(cookie)); }
    virtual void Abort() { cookie.abort = 1; }
};

extern "C" static void
fz_lock_context_cs(void *user, int lock)
{
    // we use a single critical section for all locks,
    // since that critical section (ctxAccess) should
    // be guarding all fz_context access anyway and
    // thus already be in place (in debug builds we
    // crash if that assertion doesn't hold)
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)user;
    BOOL ok = TryEnterCriticalSection(cs);
    if (!ok) {
        CrashIf(true);
        EnterCriticalSection(cs);
    }
}

extern "C" static void
fz_unlock_context_cs(void *user, int lock)
{
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)user;
    LeaveCriticalSection(cs);
}

static Vec<PageAnnotation> fz_get_user_page_annots(Vec<PageAnnotation>& userAnnots, int pageNo)
{
    Vec<PageAnnotation> result;
    for (size_t i = 0; i < userAnnots.Count(); i++) {
        PageAnnotation& annot = userAnnots.At(i);
        // include all annotations for pageNo that can be rendered by fz_run_user_annots
        if (pageNo == annot.pageNo && Annot_Highlight == annot.type)
            result.Append(annot);
    }
    return result;
}

static void fz_run_user_page_annots(Vec<PageAnnotation>& pageAnnots, fz_device *dev, fz_matrix ctm, fz_bbox clipbox, fz_cookie *cookie)
{
    for (size_t i = 0; i < pageAnnots.Count() && (!cookie || !cookie->abort); i++) {
        PageAnnotation& annot = pageAnnots.At(i);
        CrashIf(Annot_Highlight != annot.type);
        // skip annotation if it isn't visible
        fz_rect rect = fz_RectD_to_rect(annot.rect);
        rect = fz_transform_rect(ctm, rect);
        fz_bbox bbox = fz_bbox_covering_rect(rect);
        if (fz_is_empty_bbox(fz_intersect_bbox(bbox, clipbox)))
            continue;
        // prepare text highlighting path (cf. pdf_create_highlight_annot in pdf_annot.c)
        fz_path *path = fz_new_path(dev->ctx);
        fz_moveto(dev->ctx, path, annot.rect.TL().x, annot.rect.TL().y);
        fz_lineto(dev->ctx, path, annot.rect.BR().x, annot.rect.TL().y);
        fz_lineto(dev->ctx, path, annot.rect.BR().x, annot.rect.BR().y);
        fz_lineto(dev->ctx, path, annot.rect.TL().x, annot.rect.BR().y);
        fz_closepath(dev->ctx, path);
        fz_colorspace *cs = fz_find_device_colorspace(dev->ctx, "DeviceRGB");
        float color[3] = { 0.8863f, 0.7686f, 0.8863f };
        // render path with transparency effect
        fz_begin_group(dev, rect, 0, 0, FZ_BLEND_MULTIPLY, 1.f);
        fz_fill_path(dev, path, 0, ctm, cs, color, 0.8f);
        fz_end_group(dev);
        fz_free_path(dev->ctx, path);
    }
}

static void fz_run_page_transparency(Vec<PageAnnotation>& pageAnnots, fz_device *dev, fz_bbox clipbox, bool endGroup, bool hasTransparency=false)
{
    if (hasTransparency || pageAnnots.Count() == 0)
        return;
    if (!endGroup)
        fz_begin_group(dev, fz_bbox_to_rect(clipbox), 1, 0, 0, 1);
    else
        fz_end_group(dev);
}

///// PDF-specific extensions to Fitz/MuPDF /////

extern "C" {
#include <mupdf-internal.h>
}

namespace str {
    namespace conv {

inline WCHAR *FromPdf(pdf_obj *obj)
{
    ScopedMem<WCHAR> str(AllocArray<WCHAR>(pdf_to_str_len(obj) + 1));
    pdf_to_ucs2_buf((unsigned short *)str.Get(), obj);
    return str.StealData();
}

    }
}

pdf_obj *pdf_copy_str_dict(fz_context *ctx, pdf_obj *dict)
{
    pdf_obj *copy = pdf_copy_dict(ctx, dict);
    for (int i = 0; i < pdf_dict_len(copy); i++) {
        pdf_obj *val = pdf_dict_get_val(copy, i);
        // resolve all indirect references
        if (pdf_is_indirect(val)) {
            pdf_obj *val2 = pdf_new_string(ctx, pdf_to_str_buf(val), pdf_to_str_len(val));
            pdf_dict_put(copy, pdf_dict_get_key(copy, i), val2);
            pdf_drop_obj(val2);
        }
    }
    return copy;
}

// Note: make sure to only call with ctxAccess
fz_outline *pdf_loadattachments(pdf_document *xref)
{
    pdf_obj *dict = pdf_load_name_tree(xref, "EmbeddedFiles");
    if (!dict)
        return NULL;

    fz_outline root = { 0 }, *node = &root;
    for (int i = 0; i < pdf_dict_len(dict); i++) {
        pdf_obj *name = pdf_dict_get_key(dict, i);
        pdf_obj *dest = pdf_dict_get_val(dict, i);
        pdf_obj *embedded = pdf_dict_getsa(pdf_dict_gets(dest, "EF"), "DOS", "F");
        if (!embedded)
            continue;

        node = node->next = (fz_outline *)fz_malloc_struct(xref->ctx, fz_outline);
        ZeroMemory(node, sizeof(fz_outline));
        node->title = fz_strdup(xref->ctx, pdf_to_name(name));
        node->dest.kind = FZ_LINK_LAUNCH;
        node->dest.ld.launch.file_spec = pdf_file_spec_to_str(xref, dest);
        node->dest.ld.launch.new_window = 1;
        node->dest.ld.launch.embedded_num = pdf_to_num(embedded);
        node->dest.ld.launch.embedded_gen = pdf_to_gen(embedded);
    }
    pdf_drop_obj(dict);

    return root.next;
}

struct PageLabelInfo {
    int startAt, countFrom;
    const char *type;
    pdf_obj *prefix;
};

int CmpPageLabelInfo(const void *a, const void *b)
{
    return ((PageLabelInfo *)a)->startAt - ((PageLabelInfo *)b)->startAt;
}

WCHAR *FormatPageLabel(const char *type, int pageNo, const WCHAR *prefix)
{
    if (str::Eq(type, "D"))
        return str::Format(L"%s%d", prefix, pageNo);
    if (str::EqI(type, "R")) {
        // roman numbering style
        ScopedMem<WCHAR> number(str::FormatRomanNumeral(pageNo));
        if (*type == 'r')
            str::ToLower(number.Get());
        return str::Format(L"%s%s", prefix, number);
    }
    if (str::EqI(type, "A")) {
        // alphabetic numbering style (A..Z, AA..ZZ, AAA..ZZZ, ...)
        str::Str<WCHAR> number;
        number.Append('A' + (pageNo - 1) % 26);
        for (int i = 0; i < (pageNo - 1) / 26; i++)
            number.Append(number.At(0));
        if (*type == 'a')
            str::ToLower(number.Get());
        return str::Format(L"%s%s", prefix, number.Get());
    }
    return str::Dup(prefix);
}

void BuildPageLabelRec(pdf_obj *node, int pageCount, Vec<PageLabelInfo>& data)
{
    pdf_obj *obj;
    if ((obj = pdf_dict_gets(node, "Kids")) && !pdf_obj_mark(node)) {
        for (int i = 0; i < pdf_array_len(obj); i++)
            BuildPageLabelRec(pdf_array_get(obj, i), pageCount, data);
        pdf_obj_unmark(node);
    }
    else if ((obj = pdf_dict_gets(node, "Nums"))) {
        for (int i = 0; i < pdf_array_len(obj); i += 2) {
            pdf_obj *info = pdf_array_get(obj, i + 1);
            PageLabelInfo pli;
            pli.startAt = pdf_to_int(pdf_array_get(obj, i)) + 1;
            if (pli.startAt < 1)
                continue;

            pli.type = pdf_to_name(pdf_dict_gets(info, "S"));
            pli.prefix = pdf_dict_gets(info, "P");
            pli.countFrom = pdf_to_int(pdf_dict_gets(info, "St"));
            if (pli.countFrom < 1)
                pli.countFrom = 1;
            data.Append(pli);
        }
    }
}

WStrVec *BuildPageLabelVec(pdf_obj *root, int pageCount)
{
    Vec<PageLabelInfo> data;
    BuildPageLabelRec(root, pageCount, data);
    data.Sort(CmpPageLabelInfo);

    if (data.Count() == 0)
        return NULL;

    if (data.Count() == 1 && data.At(0).startAt == 1 && data.At(0).countFrom == 1 &&
        !data.At(0).prefix && str::Eq(data.At(0).type, "D")) {
        // this is the default case, no need for special treatment
        return NULL;
    }

    WStrVec *labels = new WStrVec();
    labels->AppendBlanks(pageCount);

    for (size_t i = 0; i < data.Count() && data.At(i).startAt <= pageCount; i++) {
        int secLen = pageCount + 1 - data.At(i).startAt;
        if (i < data.Count() - 1 && data.At(i + 1).startAt <= pageCount)
            secLen = data.At(i + 1).startAt - data.At(i).startAt;
        ScopedMem<WCHAR> prefix(str::conv::FromPdf(data.At(i).prefix));
        for (int j = 0; j < secLen; j++) {
            free(labels->At(data.At(i).startAt + j - 1));
            labels->At(data.At(i).startAt + j - 1) =
                FormatPageLabel(data.At(i).type, data.At(i).countFrom + j, prefix);
        }
    }

    for (int ix = 0; (ix = labels->Find(NULL, ix)) != -1; ix++)
        labels->At(ix) = str::Dup(L"");

    // ensure that all page labels are unique (by appending a number to duplicates)
    WStrVec dups(*labels);
    dups.Sort();
    for (size_t i = 1; i < dups.Count(); i++) {
        if (!str::Eq(dups.At(i), dups.At(i - 1)))
            continue;
        int ix = labels->Find(dups.At(i)), counter = 0;
        while ((ix = labels->Find(dups.At(i), ix + 1)) != -1) {
            ScopedMem<WCHAR> unique;
            do {
                unique.Set(str::Format(L"%s.%d", dups.At(i), ++counter));
            } while (labels->Find(unique) != -1);
            str::ReplacePtr(&labels->At(ix), unique);
        }
        for (; i + 1 < dups.Count() && str::Eq(dups.At(i), dups.At(i + 1)); i++);
    }

    return labels;
}

///// Above are extensions to Fitz and MuPDF, now follows PdfEngine /////

struct PdfPageRun {
    pdf_page *page;
    fz_display_list *list;
    size_t size_est;
    bool req_blending;
    bool req_t3_fonts;
    size_t path_len;
    size_t clip_path_len;
    int refs;

    PdfPageRun(pdf_page *page, fz_display_list *list, ListInspectionData& data) :
        page(page), list(list), size_est(data.mem_estimate),
        req_blending(data.req_blending), req_t3_fonts(data.req_t3_fonts),
        path_len(data.path_len), clip_path_len(data.clip_path_len), refs(1) { }
};

class PdfTocItem;
class PdfLink;
class PdfImage;

class PdfEngineImpl : public PdfEngine {
    friend PdfEngine;
    friend PdfLink;
    friend PdfImage;

public:
    PdfEngineImpl();
    virtual ~PdfEngineImpl();
    virtual PdfEngineImpl *Clone();

    virtual const WCHAR *FileName() const { return _fileName; };
    virtual int PageCount() const {
        // make sure that pdf_load_page_tree is called as soon as
        // _doc is defined, so that pdf_count_pages can't throw
        return _doc ? pdf_count_pages(_doc) : 0;
    }

    virtual RectD PageMediabox(int pageNo);
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View);

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=NULL);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, RenderTarget target=Target_View, AbortCookie **cookie_out=NULL) {
        return RenderPage(hDC, GetPdfPage(pageNo), screenRect, NULL, zoom, rotation, pageRect, target, cookie_out);
    }

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool SaveFileAs(const WCHAR *copyFileName);
    virtual WCHAR * ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View);
    virtual bool HasClipOptimizations(int pageNo);
    virtual PageLayoutType PreferredLayout();
    virtual WCHAR *GetProperty(DocumentProperty prop);

    virtual bool SupportsAnnotation(PageAnnotType type, bool forSaving=false) const;
    virtual void UpdateUserAnnotations(Vec<PageAnnotation> *list);

    virtual bool AllowsPrinting() const {
        return pdf_has_permission(_doc, PDF_PERM_PRINT);
    }
    virtual bool AllowsCopyingText() const {
        return pdf_has_permission(_doc, PDF_PERM_COPY);
    }

    virtual float GetFileDPI() const { return 72.0f; }
    virtual const WCHAR *GetDefaultFileExt() const { return L".pdf"; }

    virtual bool BenchLoadPage(int pageNo) { return GetPdfPage(pageNo) != NULL; }

    virtual Vec<PageElement *> *GetElements(int pageNo);
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt);

    virtual PageDestination *GetNamedDest(const WCHAR *name);
    virtual bool HasTocTree() const {
        return outline != NULL || attachments != NULL;
    }
    virtual DocTocItem *GetTocTree();

    virtual bool HasPageLabels() { return _pagelabels != NULL; }
    virtual WCHAR *GetPageLabel(int pageNo);
    virtual int GetPageByLabel(const WCHAR *label);

    virtual bool IsPasswordProtected() const { return isProtected; }
    virtual char *GetDecryptionKey() const;

protected:
    WCHAR *_fileName;
    char *_decryptionKey;
    bool isProtected;

    // make sure to never ask for pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION ctxAccess;
    fz_context *    ctx;
    fz_locks_context fz_locks_ctx;
    pdf_document *  _doc;

    CRITICAL_SECTION pagesAccess;
    pdf_page **     _pages;

    virtual bool    Load(const WCHAR *fileName, PasswordUI *pwdUI=NULL);
    virtual bool    Load(IStream *stream, PasswordUI *pwdUI=NULL);
    bool            Load(fz_stream *stm, PasswordUI *pwdUI=NULL);
    bool            LoadFromStream(fz_stream *stm, PasswordUI *pwdUI=NULL);
    bool            FinishLoading();
    pdf_page      * GetPdfPage(int pageNo, bool failIfBusy=false);
    int             GetPageNo(pdf_page *page);
    fz_matrix       viewctm(int pageNo, float zoom, int rotation) {
        return fz_create_view_ctm(fz_RectD_to_rect(PageMediabox(pageNo)), zoom, rotation);
    }
    fz_matrix       viewctm(pdf_page *page, float zoom, int rotation) {
        return fz_create_view_ctm(pdf_bound_page(_doc, page), zoom, rotation);
    }
    bool            RenderPage(HDC hDC, pdf_page *page, RectI screenRect,
                               fz_matrix *ctm, float zoom, int rotation,
                               RectD *pageRect, RenderTarget target, AbortCookie **cookie_out);
    bool            PreferGdiPlusDevice(pdf_page *page, float zoom, fz_rect clip);
    WCHAR         * ExtractPageText(pdf_page *page, WCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View, bool cacheRun=false);

    Vec<PdfPageRun*>runCache; // ordered most recently used first
    PdfPageRun    * CreatePageRun(pdf_page *page, fz_display_list *list);
    PdfPageRun    * GetPageRun(pdf_page *page, bool tryOnly=false);
    bool            RunPage(pdf_page *page, fz_device *dev, fz_matrix ctm,
                            RenderTarget target=Target_View,
                            fz_bbox clipbox=fz_infinite_bbox, bool cacheRun=true,
                            FitzAbortCookie *cookie=NULL);
    void            DropPageRun(PdfPageRun *run, bool forceRemove=false);

    PdfTocItem    * BuildTocTree(fz_outline *entry, int& idCounter);
    void            LinkifyPageText(pdf_page *page);
    pdf_annot    ** ProcessPageAnnotations(pdf_page *page);
    RenderedBitmap *GetPageImage(int pageNo, RectD rect, size_t imageIx);
    WCHAR         * ExtractFontList();
    bool            IsLinearizedFile();

    bool            SaveEmbedded(LinkSaverUI& saveUI, int num, int gen);
    bool            SaveUserAnnots(const WCHAR *fileName);

    RectD         * _mediaboxes;
    fz_outline    * outline;
    fz_outline    * attachments;
    pdf_obj       * _info;
    WStrVec       * _pagelabels;
    pdf_annot   *** pageAnnots;
    fz_rect      ** imageRects;

    Vec<PageAnnotation> userAnnots;
};

class PdfLink : public PageElement, public PageDestination {
    PdfEngineImpl *engine;
    fz_link_dest *link; // owned by an fz_link or fz_outline
    RectD rect;
    int pageNo;
    PointD pt;

public:
    PdfLink(PdfEngineImpl *engine, fz_link_dest *link,
        fz_rect rect=fz_empty_rect, int pageNo=-1, fz_point *pt=NULL) :
        engine(engine), link(link), rect(fz_rect_to_RectD(rect)), pageNo(pageNo) {
        // cursor coordinates for IsMap URI links
        if (pt)
            this->pt = PointD(pt->x, pt->y);
    }

    virtual PageElementType GetType() const { return Element_Link; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return rect; }
    virtual WCHAR *GetValue() const;
    virtual PageDestination *AsLink() { return this; }

    virtual PageDestType GetDestType() const;
    virtual int GetDestPageNo() const;
    virtual RectD GetDestRect() const;
    virtual WCHAR *GetDestValue() const { return GetValue(); }
    virtual WCHAR *GetDestName() const;

    virtual bool SaveEmbedded(LinkSaverUI& saveUI);
};

class PdfComment : public PageElement {
    PageAnnotation annot;
    ScopedMem<WCHAR> content;

public:
    PdfComment(const WCHAR *content, RectD rect, int pageNo) :
        annot(Annot_Comment, pageNo, rect), content(str::Dup(content)) { }

    virtual PageElementType GetType() const { return Element_Comment; }
    virtual int GetPageNo() const { return annot.pageNo; }
    virtual RectD GetRect() const { return annot.rect; }
    virtual WCHAR *GetValue() const { return str::Dup(content); }
};

class PdfTocItem : public DocTocItem {
    PdfLink link;

public:
    PdfTocItem(WCHAR *title, PdfLink link) : DocTocItem(title), link(link) { }

    virtual PageDestination *GetLink() { return &link; }
};

class PdfImage : public PageElement {
    PdfEngineImpl *engine;
    int pageNo;
    RectD rect;
    size_t imageIx;

public:
    PdfImage(PdfEngineImpl *engine, int pageNo, fz_rect rect, size_t imageIx) :
        engine(engine), pageNo(pageNo), rect(fz_rect_to_RectD(rect)), imageIx(imageIx) { }

    virtual PageElementType GetType() const { return Element_Image; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return rect; }
    virtual WCHAR *GetValue() const { return NULL; }

    virtual RenderedBitmap *GetImage() {
        return engine->GetPageImage(pageNo, rect, imageIx);
    }
};

PdfEngineImpl::PdfEngineImpl() : _fileName(NULL), _doc(NULL),
    _pages(NULL), _mediaboxes(NULL), _info(NULL),
    outline(NULL), attachments(NULL), _pagelabels(NULL),
    _decryptionKey(NULL), isProtected(false),
    pageAnnots(NULL), imageRects(NULL)
{
    InitializeCriticalSection(&pagesAccess);
    InitializeCriticalSection(&ctxAccess);

    fz_locks_ctx.user = &ctxAccess;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(NULL, &fz_locks_ctx, MAX_CONTEXT_MEMORY);

    AssertCrash(!fz_javascript_supported() && !pdf_js_supported());
}

PdfEngineImpl::~PdfEngineImpl()
{
    EnterCriticalSection(&pagesAccess);
    EnterCriticalSection(&ctxAccess);

    if (_pages) {
        for (int i = 0; i < PageCount(); i++) {
            if (_pages[i])
                pdf_free_page(_doc, _pages[i]);
        }
        free(_pages);
    }

    fz_free_outline(ctx, outline);
    fz_free_outline(ctx, attachments);
    pdf_drop_obj(_info);

    if (pageAnnots) {
        for (int i = 0; i < PageCount(); i++) {
            free(pageAnnots[i]);
        }
        free(pageAnnots);
    }
    if (imageRects) {
        for (int i = 0; i < PageCount(); i++) {
            free(imageRects[i]);
        }
        free(imageRects);
    }

    pdf_close_document(_doc);
    _doc = NULL;

    while (runCache.Count() > 0) {
        assert(runCache.Last()->refs == 1);
        DropPageRun(runCache.Last(), true);
    }

    delete[] _mediaboxes;
    delete _pagelabels;
    free(_fileName);
    free(_decryptionKey);

    fz_free_context(ctx);

    LeaveCriticalSection(&ctxAccess);
    DeleteCriticalSection(&ctxAccess);
    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

class PasswordCloner : public PasswordUI {
    unsigned char *cryptKey;

public:
    PasswordCloner(unsigned char *cryptKey) : cryptKey(cryptKey) { }

    virtual WCHAR * GetPassword(const WCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey)
    {
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return NULL;
    }
};

PdfEngineImpl *PdfEngineImpl::Clone()
{
    ScopedCritSec scope(&ctxAccess);

    // use this document's encryption key (if any) to load the clone
    PasswordCloner *pwdUI = NULL;
    if (pdf_crypt_key(_doc))
        pwdUI = new PasswordCloner(pdf_crypt_key(_doc));

    PdfEngineImpl *clone = new PdfEngineImpl();
    if (!clone || !(_fileName ? clone->Load(_fileName, pwdUI) : clone->Load(_doc->file, pwdUI))) {
        delete clone;
        delete pwdUI;
        return NULL;
    }
    delete pwdUI;

    if (!_decryptionKey && _doc->crypt) {
        delete clone->_decryptionKey;
        clone->_decryptionKey = NULL;
    }

    clone->UpdateUserAnnotations(&userAnnots);

    return clone;
}

static const WCHAR *findEmbedMarks(const WCHAR *fileName)
{
    const WCHAR *embedMarks = NULL;

    int colonCount = 0;
    for (const WCHAR *c = fileName + str::Len(fileName) - 1; c > fileName; c--) {
        if (*c == ':') {
            if (!str::IsDigit(*(c + 1)))
                break;
            if (++colonCount % 2 == 0)
                embedMarks = c;
        }
        else if (!str::IsDigit(*c))
            break;
    }

    return embedMarks;
}

bool PdfEngineImpl::Load(const WCHAR *fileName, PasswordUI *pwdUI)
{
    assert(!_fileName && !_doc && ctx);
    _fileName = str::Dup(fileName);
    if (!_fileName || !ctx)
        return false;

    fz_stream *file;
    // File names ending in :<digits>:<digits> are interpreted as containing
    // embedded PDF documents (the digits are :<num>:<gen> of the embedded file stream)
    WCHAR *embedMarks = (WCHAR *)findEmbedMarks(_fileName);
    if (embedMarks)
        *embedMarks = '\0';
    fz_try(ctx) {
        file = fz_open_file2(ctx, _fileName);
    }
    fz_catch(ctx) {
        file = NULL;
    }
    if (embedMarks)
        *embedMarks = ':';

OpenEmbeddedFile:
    if (!LoadFromStream(file, pwdUI))
        return false;

    if (str::IsEmpty(embedMarks))
        return FinishLoading();

    int num, gen;
    embedMarks = (WCHAR *)str::Parse(embedMarks, L":%d:%d", &num, &gen);
    assert(embedMarks);
    if (!embedMarks || !pdf_is_stream(_doc, num, gen))
        return false;

    fz_buffer *buffer = NULL;
    fz_var(buffer);
    fz_try(ctx) {
        buffer = pdf_load_stream(_doc, num, gen);
        file = fz_open_buffer(ctx, buffer);
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx) {
        return false;
    }

    pdf_close_document(_doc);
    _doc = NULL;

    goto OpenEmbeddedFile;
}

bool PdfEngineImpl::Load(IStream *stream, PasswordUI *pwdUI)
{
    assert(!_fileName && !_doc && ctx);
    if (!ctx)
        return false;

    fz_stream *stm = NULL;
    fz_try(ctx) {
        stm = fz_open_istream(ctx, stream);
    }
    fz_catch(ctx) {
        return false;
    }
    if (!LoadFromStream(stm, pwdUI))
        return false;
    return FinishLoading();
}

bool PdfEngineImpl::Load(fz_stream *stm, PasswordUI *pwdUI)
{
    assert(!_fileName && !_doc && ctx);
    if (!ctx)
        return false;

    fz_try(ctx) {
        stm = fz_clone_stream(ctx, stm);
    }
    fz_catch(ctx) {
        return false;
    }
    if (!LoadFromStream(stm, pwdUI))
        return false;
    return FinishLoading();
}

bool PdfEngineImpl::LoadFromStream(fz_stream *stm, PasswordUI *pwdUI)
{
    if (!stm)
        return false;

    fz_try(ctx) {
        _doc = pdf_open_document_with_stream(ctx, stm);
    }
    fz_always(ctx) {
        fz_close(stm);
    }
    fz_catch(ctx) {
        return false;
    }

    isProtected = pdf_needs_password(_doc);
    if (!isProtected)
        return true;

    if (!pwdUI)
        return false;

    unsigned char digest[16 + 32] = { 0 };
    fz_stream_fingerprint(_doc->file, digest);

    bool ok = false, saveKey = false;
    for (int i = 0; !ok && i < 3; i++) {
        ScopedMem<WCHAR> pwd(pwdUI->GetPassword(_fileName, digest, pdf_crypt_key(_doc), &saveKey));
        if (!pwd) {
            // password not given or encryption key has been remembered
            ok = saveKey;
            break;
        }

        ScopedMem<WCHAR> wstr(str::Dup(pwd));
        fz_try(ctx) {
            char *pwd_doc = pdf_from_ucs2(_doc, (unsigned short *)wstr.Get());
            ok = pwd_doc && pdf_authenticate_password(_doc, pwd_doc);
            fz_free(ctx, pwd_doc);
        }
        fz_catch(ctx) { }
        // try the UTF-8 password, if the PDFDocEncoding one doesn't work
        if (!ok) {
            ScopedMem<char> pwd_utf8(str::conv::ToUtf8(pwd));
            ok = pwd_utf8 && pdf_authenticate_password(_doc, pwd_utf8);
        }
        // fall back to an ANSI-encoded password as a last measure
        if (!ok) {
            ScopedMem<char> pwd_ansi(str::conv::ToAnsi(pwd));
            ok = pwd_ansi && pdf_authenticate_password(_doc, pwd_ansi);
        }
    }

    if (ok && saveKey) {
        memcpy(digest + 16, pdf_crypt_key(_doc), 32);
        _decryptionKey = _MemToHex(&digest);
    }

    return ok;
}

bool PdfEngineImpl::FinishLoading()
{
    fz_try(ctx) {
        // this calls pdf_load_page_tree(xref) which may throw
        pdf_count_pages(_doc);
    }
    fz_catch(ctx) {
        return false;
    }
    if (PageCount() == 0) {
        fz_warn(ctx, "document has no pages");
        return false;
    }

    _pages = AllocArray<pdf_page *>(PageCount());
    _mediaboxes = new RectD[PageCount()];
    pageAnnots = AllocArray<pdf_annot **>(PageCount());
    imageRects = AllocArray<fz_rect *>(PageCount());

    if (!_pages || !_mediaboxes || !pageAnnots || !imageRects)
        return false;

    ScopedCritSec scope(&ctxAccess);

    fz_try(ctx) {
        outline = pdf_load_outline(_doc);
    }
    fz_catch(ctx) {
        // ignore errors from pdf_load_outline()
        // this information is not critical and checking the
        // error might prevent loading some pdfs that would
        // otherwise get displayed
        fz_warn(ctx, "Couldn't load outline");
    }
    fz_try(ctx) {
        attachments = pdf_loadattachments(_doc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load attachments");
    }
    fz_try(ctx) {
        // keep a copy of the Info dictionary, as accessing the original
        // isn't thread safe and we don't want to block for this when
        // displaying document properties
        _info = pdf_dict_gets(_doc->trailer, "Info");
        if (_info)
            _info = pdf_copy_str_dict(ctx, _info);
        if (!_info)
            _info = pdf_new_dict(ctx, 4);
        // also remember linearization and tagged states at this point
        if (IsLinearizedFile())
            pdf_dict_puts_drop(_info, "Linearized", pdf_new_bool(ctx, 1));
        if (pdf_to_bool(pdf_dict_getp(_doc->trailer, "Root/MarkInfo/Marked")))
            pdf_dict_puts_drop(_info, "Marked", pdf_new_bool(ctx, 1));
        // also remember known output intents (PDF/X, etc.)
        pdf_obj *intents = pdf_dict_getp(_doc->trailer, "Root/OutputIntents");
        if (pdf_is_array(intents)) {
            pdf_obj *list = pdf_new_array(ctx, pdf_array_len(intents));
            for (int i = 0; i < pdf_array_len(intents); i++) {
                pdf_obj *intent = pdf_dict_gets(pdf_array_get(intents, i), "S");
                if (pdf_is_name(intent) && !pdf_is_indirect(intent) && str::StartsWith(pdf_to_name(intent), "GTS_PDF"))
                    pdf_array_push(list, intent);
            }
            pdf_dict_puts_drop(_info, "OutputIntents", list);
        }
        else
            pdf_dict_dels(_info, "OutputIntents");
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load document properties");
        pdf_drop_obj(_info);
        _info = NULL;
    }
    fz_try(ctx) {
        pdf_obj *pagelabels = pdf_dict_getp(_doc->trailer, "Root/PageLabels");
        if (pagelabels)
            _pagelabels = BuildPageLabelVec(pagelabels, PageCount());
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load page labels");
    }

    return true;
}

PdfTocItem *PdfEngineImpl::BuildTocTree(fz_outline *entry, int& idCounter)
{
    PdfTocItem *node = NULL;

    for (; entry; entry = entry->next) {
        WCHAR *name = entry->title ? str::conv::FromUtf8(entry->title) : str::Dup(L"");
        PdfTocItem *item = new PdfTocItem(name, PdfLink(this, &entry->dest));
        item->open = entry->is_open;
        item->id = ++idCounter;

        if (entry->dest.kind == FZ_LINK_GOTO)
            item->pageNo = entry->dest.ld.gotor.page + 1;
        if (entry->down)
            item->child = BuildTocTree(entry->down, idCounter);

        if (!node)
            node = item;
        else
            node->AddSibling(item);
    }

    return node;
}

DocTocItem *PdfEngineImpl::GetTocTree()
{
    PdfTocItem *node = NULL;
    int idCounter = 0;

    if (outline) {
        node = BuildTocTree(outline, idCounter);
        if (attachments)
            node->AddSibling(BuildTocTree(attachments, idCounter));
    } else if (attachments)
        node = BuildTocTree(attachments, idCounter);

    return node;
}

PageDestination *PdfEngineImpl::GetNamedDest(const WCHAR *name)
{
    ScopedCritSec scope(&ctxAccess);

    ScopedMem<char> name_utf8(str::conv::ToUtf8(name));
    pdf_obj *dest = NULL;
    fz_try(ctx) {
        pdf_obj *nameobj = pdf_new_string(ctx, (char *)name_utf8, (int)str::Len(name_utf8));
        dest = pdf_lookup_dest(_doc, nameobj);
        pdf_drop_obj(nameobj);
    }
    fz_catch(ctx) {
        return NULL;
    }

    PageDestination *pageDest = NULL;
    fz_link_dest ld = { FZ_LINK_NONE, 0 };
    fz_try(ctx) {
        ld = pdf_parse_link_dest(_doc, dest);
    }
    fz_catch(ctx) {
        return NULL;
    }

    if (FZ_LINK_GOTO == ld.kind) {
        // create a SimpleDest because we have to
        // free the fz_link_dest before returning
        PdfLink tmp(this, &ld);
        pageDest = new SimpleDest(tmp.GetDestPageNo(), tmp.GetDestRect());
    }
    fz_free_link_dest(ctx, &ld);

    return pageDest;
}

pdf_page *PdfEngineImpl::GetPdfPage(int pageNo, bool failIfBusy)
{
    if (!_pages)
        return NULL;
    if (failIfBusy)
        return _pages[pageNo-1];

    ScopedCritSec scope(&pagesAccess);

    pdf_page *page = _pages[pageNo-1];
    if (!page) {
        ScopedCritSec ctxScope(&ctxAccess);
        fz_var(page);
        fz_try(ctx) {
            page = pdf_load_page(_doc, pageNo - 1);
            _pages[pageNo-1] = page;
            LinkifyPageText(page);
            pageAnnots[pageNo-1] = ProcessPageAnnotations(page);
        }
        fz_catch(ctx) { }
    }

    return page;
}

int PdfEngineImpl::GetPageNo(pdf_page *page)
{
    for (int i = 0; i < PageCount(); i++)
        if (page == _pages[i])
            return i + 1;
    return 0;
}

PdfPageRun *PdfEngineImpl::CreatePageRun(pdf_page *page, fz_display_list *list)
{
    Vec<FitzImagePos> positions;
    ListInspectionData data(positions);
    fz_device *dev = NULL;

    fz_var(dev);
    fz_try(ctx) {
        dev = fz_new_inspection_device(ctx, &data);
        fz_run_display_list(list, dev, fz_identity, fz_infinite_bbox, NULL);
    }
    fz_catch(ctx) { }
    fz_free_device(dev);

    // save the image rectangles for this page
    int pageNo = GetPageNo(page);
    if (!imageRects[pageNo-1] && positions.Count() > 0) {
        // the list of page image rectangles is terminated with a null-rectangle
        fz_rect *rects = AllocArray<fz_rect>(positions.Count() + 1);
        if (rects) {
            for (size_t i = 0; i < positions.Count(); i++) {
                rects[i] = positions.At(i).rect;
            }
            imageRects[pageNo-1] = rects;
        }
    }

    return new PdfPageRun(page, list, data);
}

PdfPageRun *PdfEngineImpl::GetPageRun(pdf_page *page, bool tryOnly)
{
    PdfPageRun *result = NULL;

    ScopedCritSec scope(&pagesAccess);

    for (size_t i = 0; i < runCache.Count(); i++) {
        if (runCache.At(i)->page == page) {
            result = runCache.At(i);
            break;
        }
    }
    if (!result && !tryOnly) {
        size_t mem = 0;
        for (size_t i = 0; i < runCache.Count(); i++) {
            // drop page runs that take up too much memory due to huge images
            // (except for the very recently used ones)
            if (i >= 2 && mem + runCache.At(i)->size_est >= MAX_PAGE_RUN_MEMORY)
                DropPageRun(runCache.At(i--), true);
            else
                mem += runCache.At(i)->size_est;
        }
        if (runCache.Count() >= MAX_PAGE_RUN_CACHE) {
            assert(runCache.Count() == MAX_PAGE_RUN_CACHE);
            DropPageRun(runCache.Last(), true);
        }

        ScopedCritSec scope2(&ctxAccess);

        fz_display_list *list = NULL;
        fz_device *dev = NULL;
        fz_var(list);
        fz_var(dev);
        fz_try(ctx) {
            list = fz_new_display_list(ctx);
            dev = fz_new_list_device(ctx, list);
            pdf_run_page(_doc, page, dev, fz_identity, NULL);
        }
        fz_catch(ctx) {
            fz_free_display_list(ctx, list);
            list = NULL;
        }
        fz_free_device(dev);

        if (list) {
            result = CreatePageRun(page, list);
            runCache.InsertAt(0, result);
        }
    }
    else if (result && result != runCache.At(0)) {
        // keep the list Most Recently Used first
        runCache.Remove(result);
        runCache.InsertAt(0, result);
    }

    if (result)
        result->refs++;
    return result;
}

bool PdfEngineImpl::RunPage(pdf_page *page, fz_device *dev, fz_matrix ctm, RenderTarget target, fz_bbox clipbox, bool cacheRun, FitzAbortCookie *cookie)
{
    bool ok = true;

    PdfPageRun *run;
    if (Target_View == target && (run = GetPageRun(page, !cacheRun))) {
        EnterCriticalSection(&ctxAccess);
        Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, GetPageNo(page));
        fz_try(ctx) {
            fz_run_page_transparency(pageAnnots, dev, clipbox, false, page->transparency);
            fz_run_display_list(run->list, dev, ctm, clipbox, cookie ? &cookie->cookie : NULL);
            fz_run_page_transparency(pageAnnots, dev, clipbox, true, page->transparency);
            fz_run_user_page_annots(pageAnnots, dev, ctm, clipbox, cookie ? &cookie->cookie : NULL);
        }
        fz_catch(ctx) {
            ok = false;
        }
        LeaveCriticalSection(&ctxAccess);
        DropPageRun(run);
    }
    else {
        ScopedCritSec scope(&ctxAccess);
        char *targetName = target == Target_Print ? "Print" :
                           target == Target_Export ? "Export" : "View";
        Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, GetPageNo(page));
        fz_try(ctx) {
            fz_run_page_transparency(pageAnnots, dev, clipbox, false, page->transparency);
            pdf_run_page_with_usage(_doc, page, dev, ctm, targetName, cookie ? &cookie->cookie : NULL);
            fz_run_page_transparency(pageAnnots, dev, clipbox, true, page->transparency);
            fz_run_user_page_annots(pageAnnots, dev, ctm, clipbox, cookie ? &cookie->cookie : NULL);
        }
        fz_catch(ctx) {
            ok = false;
        }
    }

    EnterCriticalSection(&ctxAccess);
    fz_free_device(dev);
    LeaveCriticalSection(&ctxAccess);

    return ok && !(cookie && cookie->cookie.abort);
}

void PdfEngineImpl::DropPageRun(PdfPageRun *run, bool forceRemove)
{
    EnterCriticalSection(&pagesAccess);
    run->refs--;

    if (0 == run->refs || forceRemove) {
        runCache.Remove(run);
        if (0 == run->refs) {
            EnterCriticalSection(&ctxAccess);
            fz_free_display_list(ctx, run->list);
            LeaveCriticalSection(&ctxAccess);
            delete run;
        }
    }

    LeaveCriticalSection(&pagesAccess);
}

RectD PdfEngineImpl::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (!_mediaboxes[pageNo-1].IsEmpty())
        return _mediaboxes[pageNo-1];

    pdf_obj *page = _doc->page_objs[pageNo-1];
    if (!page)
        return RectD();

    ScopedCritSec scope(&ctxAccess);

    // cf. pdf_page.c's pdf_load_page
    fz_rect mbox = fz_empty_rect, cbox = fz_empty_rect;
    int rotate = 0;
    float userunit = 1.0;
    fz_try(ctx) {
        mbox = pdf_to_rect(ctx, pdf_dict_gets(page, "MediaBox"));
        cbox = pdf_to_rect(ctx, pdf_dict_gets(page, "CropBox"));
        rotate = pdf_to_int(pdf_dict_gets(page, "Rotate"));
        pdf_obj *obj = pdf_dict_gets(page, "UserUnit");
        if (pdf_is_real(obj))
            userunit = pdf_to_real(obj);
    }
    fz_catch(ctx) { }
    if (fz_is_empty_rect(mbox)) {
        fz_warn(ctx, "cannot find page size for page %d", pageNo);
        mbox.x0 = 0; mbox.y0 = 0;
        mbox.x1 = 612; mbox.y1 = 792;
    }
    if (!fz_is_empty_rect(cbox)) {
        mbox = fz_intersect_rect(mbox, cbox);
        if (fz_is_empty_rect(mbox))
            return RectD();
    }
    if ((rotate % 90) != 0)
        rotate = 0;

    // cf. pdf_page.c's pdf_bound_page
    mbox = fz_transform_rect(fz_rotate((float)rotate), mbox);

    _mediaboxes[pageNo-1] = RectD(0, 0, (mbox.x1 - mbox.x0) * userunit, (mbox.y1 - mbox.y0) * userunit);
    return _mediaboxes[pageNo-1];
}

RectD PdfEngineImpl::PageContentBox(int pageNo, RenderTarget target)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    pdf_page *page = GetPdfPage(pageNo);
    if (!page)
        return RectD();

    fz_bbox bbox = { 0 };
    fz_device *dev = NULL;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &bbox);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return RectD();
    }
    LeaveCriticalSection(&ctxAccess);

    fz_bbox mediabox = fz_round_rect(pdf_bound_page(_doc, page));
    bool ok = RunPage(page, dev, fz_identity, target, mediabox, false);
    if (!ok)
        return PageMediabox(pageNo);
    if (fz_is_infinite_bbox(bbox))
        return PageMediabox(pageNo);

    RectD bbox2 = fz_bbox_to_RectI(bbox).Convert<double>();
    return bbox2.Intersect(PageMediabox(pageNo));
}

PointD PdfEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    fz_point pt2 = { (float)pt.x, (float)pt.y };
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    pt2 = fz_transform_point(ctm, pt2);
    return PointD(pt2.x, pt2.y);
}

RectD PdfEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    fz_rect rect2 = fz_RectD_to_rect(rect);
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    rect2 = fz_transform_rect(ctm, rect2);
    return fz_rect_to_RectD(rect2);
}

bool PdfEngineImpl::RenderPage(HDC hDC, pdf_page *page, RectI screenRect, fz_matrix *ctm, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    if (!page)
        return false;

    fz_matrix ctm2;
    if (!ctm) {
        ctm2 = viewctm(page, zoom, rotation);
        fz_rect pRect = pageRect ? fz_RectD_to_rect(*pageRect) : pdf_bound_page(_doc, page);
        fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm2, pRect));
        ctm2 = fz_concat(ctm2, fz_translate((float)screenRect.x - bbox.x0, (float)screenRect.y - bbox.y0));
    }
    else
        ctm2 = *ctm;

    HBRUSH bgBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
    FillRect(hDC, &screenRect.ToRECT(), bgBrush); // initialize white background
    DeleteObject(bgBrush);

    fz_bbox clipbox = fz_RectI_to_bbox(screenRect);
    if (pageRect) {
        fz_bbox pageclip = fz_round_rect(fz_transform_rect(ctm2, fz_RectD_to_rect(*pageRect)));
        clipbox = fz_intersect_bbox(clipbox, pageclip);
    }
    fz_device *dev = NULL;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_gdiplus_device(ctx, hDC, clipbox);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return false;
    }
    LeaveCriticalSection(&ctxAccess);

    FitzAbortCookie *cookie = NULL;
    if (cookie_out)
        *cookie_out = cookie = new FitzAbortCookie();
    return RunPage(page, dev, ctm2, target, clipbox, true, cookie);
}

// various heuristics for deciding when to use dev_gdiplus instead of fitz/draw
bool PdfEngineImpl::PreferGdiPlusDevice(pdf_page *page, float zoom, fz_rect clip)
{
    PdfPageRun *run = GetPageRun(page);
    if (!run)
        return false;

    bool result = false;
    // dev_gdiplus seems significantly slower at clipping than fitz/draw
    if (run->clip_path_len > 50000)
        result = false;
    // dev_gdiplus seems to render quicker and more reliably at high zoom levels
    else if (zoom > 40.0f)
        result = true;
    // dev_gdiplus' Type 3 fonts look worse than bad transparency at lower zoom levels
    else if (run->req_t3_fonts)
        result = false;
    // fitz/draw currently isn't able to correctly/quickly render some
    // transparency groups while dev_gdiplus gets most of them right
    else if (run->req_blending)
        result = true;
    // dev_gdiplus seems significantly faster at rendering large (amounts of) paths
    // (only required when tiling, at lower zoom levels lines look slightly worse)
    else if (run->path_len > 100000) {
        fz_bbox clipBox = fz_round_rect(clip);
        fz_bbox pageBox = fz_round_rect(pdf_bound_page(_doc, page));
        result = clipBox.x0 > pageBox.x0 || clipBox.x1 < pageBox.x1 ||
                 clipBox.y0 > pageBox.y0 || clipBox.y1 < pageBox.y1;
    }
    DropPageRun(run);
    return result;
}

RenderedBitmap *PdfEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    pdf_page* page = GetPdfPage(pageNo);
    if (!page)
        return NULL;

    fz_rect pRect = pageRect ? fz_RectD_to_rect(*pageRect) : pdf_bound_page(_doc, page);
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm, pRect));

    if (PreferGdiPlusDevice(page, zoom, pRect) != gDebugGdiPlusDevice) {
        int w = bbox.x1 - bbox.x0, h = bbox.y1 - bbox.y0;
        ctm = fz_concat(ctm, fz_translate((float)-bbox.x0, (float)-bbox.y0));

        // for now, don't render directly into a DC but produce an HBITMAP instead
        HDC hDC = GetDC(NULL);
        HDC hDCMem = CreateCompatibleDC(hDC);
        HBITMAP hbmp = CreateCompatibleBitmap(hDC, w, h);
        DeleteObject(SelectObject(hDCMem, hbmp));

        RectI rc(0, 0, w, h);
        RectD pageRect2 = fz_rect_to_RectD(pRect);
        bool ok = RenderPage(hDCMem, page, rc, &ctm, 0, 0, &pageRect2, target, cookie_out);
        DeleteDC(hDCMem);
        ReleaseDC(NULL, hDC);
        if (!ok) {
            DeleteObject(hbmp);
            return NULL;
        }
        return new RenderedBitmap(hbmp, SizeI(w, h));
    }

    fz_pixmap *image = NULL;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        fz_colorspace *colorspace = fz_find_device_colorspace(ctx, "DeviceRGB");
        image = fz_new_pixmap_with_bbox(ctx, colorspace, bbox);
        fz_clear_pixmap_with_value(ctx, image, 0xFF); // initialize white background
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }

    fz_device *dev = NULL;
    fz_try(ctx) {
        dev = fz_new_draw_device(ctx, image);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, image);
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }
    LeaveCriticalSection(&ctxAccess);

    FitzAbortCookie *cookie = NULL;
    if (cookie_out)
        *cookie_out = cookie = new FitzAbortCookie();
    bool ok = RunPage(page, dev, ctm, target, bbox, true, cookie);

    ScopedCritSec scope(&ctxAccess);

    RenderedBitmap *bitmap = NULL;
    if (ok)
        bitmap = new RenderedFitzBitmap(ctx, image);
    fz_drop_pixmap(ctx, image);
    return bitmap;
}

PageElement *PdfEngineImpl::GetElementAtPos(int pageNo, PointD pt)
{
    pdf_page *page = GetPdfPage(pageNo, true);
    if (!page)
        return NULL;

    fz_point p = { (float)pt.x, (float)pt.y };
    for (fz_link *link = page->links; link; link = link->next) {
        if (link->dest.kind != FZ_LINK_NONE && fz_is_pt_in_rect(link->rect, p))
            return new PdfLink(this, &link->dest, link->rect, pageNo, &p);
    }

    if (pageAnnots[pageNo-1]) {
        for (size_t i = 0; pageAnnots[pageNo-1][i]; i++) {
            pdf_annot *annot = pageAnnots[pageNo-1][i];
            fz_rect rect = fz_transform_rect(page->ctm, annot->rect);
            if (fz_is_pt_in_rect(rect, p)) {
                ScopedCritSec scope(&ctxAccess);

                ScopedMem<WCHAR> contents(str::conv::FromPdf(pdf_dict_gets(annot->obj, "Contents")));
                return new PdfComment(contents, fz_rect_to_RectD(rect), pageNo);
            }
        }
    }

    if (imageRects[pageNo-1]) {
        for (size_t i = 0; !fz_is_empty_rect(imageRects[pageNo-1][i]); i++)
            if (fz_is_pt_in_rect(imageRects[pageNo-1][i], p))
                return new PdfImage(this, pageNo, imageRects[pageNo-1][i], i);
    }

    return NULL;
}

Vec<PageElement *> *PdfEngineImpl::GetElements(int pageNo)
{
    pdf_page *page = GetPdfPage(pageNo, true);
    if (!page)
        return NULL;

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    Vec<PageElement *> *els = new Vec<PageElement *>();
    if (!els)
        return NULL;

    if (imageRects[pageNo-1]) {
        for (size_t i = 0; !fz_is_empty_rect(imageRects[pageNo-1][i]); i++)
            els->Append(new PdfImage(this, pageNo, imageRects[pageNo-1][i], i));
    }

    if (pageAnnots[pageNo-1]) {
        ScopedCritSec scope(&ctxAccess);

        for (size_t i = 0; pageAnnots[pageNo-1][i]; i++) {
            pdf_annot *annot = pageAnnots[pageNo-1][i];
            fz_rect rect = fz_transform_rect(page->ctm, annot->rect);
            ScopedMem<WCHAR> contents(str::conv::FromPdf(pdf_dict_gets(annot->obj, "Contents")));
            els->Append(new PdfComment(contents, fz_rect_to_RectD(rect), pageNo));
        }
    }

    for (fz_link *link = page->links; link; link = link->next) {
        if (link->dest.kind != FZ_LINK_NONE) {
            els->Append(new PdfLink(this, &link->dest, link->rect, pageNo));
        }
    }

    els->Reverse();
    return els;
}

void PdfEngineImpl::LinkifyPageText(pdf_page *page)
{
    page->links = FixupPageLinks(page->links);
    assert(!page->links || page->links->refs == 1);

    RectI *coords;
    WCHAR *pageText = ExtractPageText(page, L"\n", &coords, Target_View, true);
    if (!pageText) {
        return;
    }

    LinkRectList *list = LinkifyText(pageText, coords);
    for (size_t i = 0; i < list->links.Count(); i++) {
        bool overlaps = false;
        for (fz_link *next = page->links; next && !overlaps; next = next->next)
            overlaps = fz_calc_overlap(list->coords.At(i), next->rect) >= 0.25f;
        if (!overlaps) {
            ScopedMem<char> uri(str::conv::ToUtf8(list->links.At(i)));
            if (!uri) continue;
            fz_link_dest ld = { FZ_LINK_URI, 0 };
            ld.ld.uri.uri = fz_strdup(ctx, uri);
            // add links in top-to-bottom order (i.e. last-to-first)
            fz_link *link = fz_new_link(ctx, list->coords.At(i), ld);
            link->next = page->links;
            page->links = link;
        }
    }

    delete list;
    delete[] coords;
    free(pageText);
}

pdf_annot **PdfEngineImpl::ProcessPageAnnotations(pdf_page *page)
{
    Vec<pdf_annot *> annots;

    for (pdf_annot *annot = page->annots; annot; annot = annot->next) {
        if (FZ_WIDGET_TYPE_FILE == annot->type) {
            pdf_obj *file = pdf_dict_gets(annot->obj, "FS");
            fz_rect rect = pdf_to_rect(ctx, pdf_dict_gets(annot->obj, "Rect"));
            pdf_obj *embedded = pdf_dict_getsa(pdf_dict_gets(file, "EF"), "DOS", "F");
            if (file && embedded && !fz_is_empty_rect(rect)) {
                fz_link_dest ld;
                ld.kind = FZ_LINK_LAUNCH;
                ld.ld.launch.file_spec = pdf_file_spec_to_str(_doc, file);
                ld.ld.launch.new_window = 1;
                ld.ld.launch.embedded_num = pdf_to_num(embedded);
                ld.ld.launch.embedded_gen = pdf_to_gen(embedded);
                rect = fz_transform_rect(page->ctm, rect);
                // add links in top-to-bottom order (i.e. last-to-first)
                fz_link *link = fz_new_link(ctx, rect, ld);
                link->next = page->links;
                page->links = link;
                // TODO: expose /Contents in addition to the file path
            }
            else if (!str::IsEmpty(pdf_to_str_buf(pdf_dict_gets(annot->obj, "Contents")))) {
                annots.Append(annot);
            }
        }
        else if (!str::IsEmpty(pdf_to_str_buf(pdf_dict_gets(annot->obj, "Contents"))) && annot->type != FZ_WIDGET_TYPE_FREETEXT) {
            annots.Append(annot);
        }
    }

    if (annots.Count() == 0)
        return NULL;

    // re-order list into top-to-bottom order (i.e. last-to-first)
    annots.Reverse();
    // add sentinel value
    annots.Append(NULL);
    return annots.StealData();
}

RenderedBitmap *PdfEngineImpl::GetPageImage(int pageNo, RectD rect, size_t imageIx)
{
    pdf_page *page = GetPdfPage(pageNo);
    if (!page)
        return NULL;

    Vec<FitzImagePos> positions;
    ListInspectionData data(positions);
    fz_device *dev = NULL;

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_inspection_device(ctx, &data);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }
    LeaveCriticalSection(&ctxAccess);

    RunPage(page, dev, fz_identity);

    if (imageIx >= positions.Count() || fz_rect_to_RectD(positions.At(imageIx).rect) != rect) {
        assert(0);
        return NULL;
    }

    ScopedCritSec scope(&ctxAccess);

    fz_pixmap *pixmap = NULL;
    fz_try(ctx) {
        fz_image *image = positions.At(imageIx).image;
        pixmap = fz_image_to_pixmap(ctx, image, image->w, image->h);
    }
    fz_catch(ctx) {
        return NULL;
    }
    RenderedFitzBitmap *bmp = new RenderedFitzBitmap(ctx, pixmap);
    fz_drop_pixmap(ctx, pixmap);

    return bmp;
}

WCHAR *PdfEngineImpl::ExtractPageText(pdf_page *page, WCHAR *lineSep, RectI **coords_out, RenderTarget target, bool cacheRun)
{
    if (!page)
        return NULL;

    fz_text_sheet *sheet = NULL;
    fz_text_page *text = NULL;
    fz_device *dev = NULL;
    fz_var(sheet);
    fz_var(text);

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        sheet = fz_new_text_sheet(ctx);
        text = fz_new_text_page(ctx, pdf_bound_page(_doc, page));
        dev = fz_new_text_device(ctx, sheet, text);
    }
    fz_catch(ctx) {
        fz_free_text_page(ctx, text);
        fz_free_text_sheet(ctx, sheet);
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }
    LeaveCriticalSection(&ctxAccess);

    // use an infinite rectangle as bounds (instead of pdf_bound_page) to ensure that
    // the extracted text is consistent between cached runs using a list device and
    // fresh runs (otherwise the list device omits text outside the mediabox bounds)
    bool ok = RunPage(page, dev, fz_identity, target, fz_infinite_bbox, cacheRun);

    ScopedCritSec scope(&ctxAccess);

    WCHAR *content = NULL;
    if (ok)
        content = fz_text_page_to_str(text, lineSep, coords_out);
    fz_free_text_page(ctx, text);
    fz_free_text_sheet(ctx, sheet);

    return content;
}

WCHAR *PdfEngineImpl::ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out, RenderTarget target)
{
    pdf_page *page = GetPdfPage(pageNo, true);
    if (page)
        return ExtractPageText(page, lineSep, coords_out, target);

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        page = pdf_load_page(_doc, pageNo - 1);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }
    LeaveCriticalSection(&ctxAccess);

    WCHAR *result = ExtractPageText(page, lineSep, coords_out, target);

    EnterCriticalSection(&ctxAccess);
    pdf_free_page(_doc, page);
    LeaveCriticalSection(&ctxAccess);

    return result;
}

bool PdfEngineImpl::IsLinearizedFile()
{
    ScopedCritSec scope(&ctxAccess);
    // determine the object number of the very first object in the file
    fz_seek(_doc->file, 0, 0);
    int tok = pdf_lex(_doc->file, &_doc->lexbuf.base);
    if (tok != PDF_TOK_INT)
        return false;
    int num = _doc->lexbuf.base.i;
    // check whether it's a linearization dictionary
    fz_try(_doc->ctx) {
        pdf_cache_object(_doc, num, 0);
    }
    fz_catch(_doc->ctx) {
        return false;
    }
    pdf_obj *obj = _doc->table[num].obj;
    if (!pdf_is_dict(obj))
        return false;
    // /Linearized format must be version 1.0
    if (pdf_to_real(pdf_dict_gets(obj, "Linearized")) != 1.0f)
        return false;
    // /L must be the exact file size
    if (pdf_to_int(pdf_dict_gets(obj, "L")) != _doc->file_size)
        return false;
    // /O must be the object number of the first page
    if (pdf_to_int(pdf_dict_gets(obj, "O")) != pdf_to_num(_doc->page_refs[0]))
        return false;
    // /N must be the total number of pages
    if (pdf_to_int(pdf_dict_gets(obj, "N")) != PageCount())
        return false;
    // /H must be an array and /E and /T must be integers
    return pdf_is_array(pdf_dict_gets(obj, "H")) &&
           pdf_is_int(pdf_dict_gets(obj, "E")) &&
           pdf_is_int(pdf_dict_gets(obj, "T"));
}

static void pdf_extract_fonts(pdf_obj *res, Vec<pdf_obj *>& fontList)
{
    pdf_obj *fonts = pdf_dict_gets(res, "Font");
    for (int k = 0; k < pdf_dict_len(fonts); k++) {
        pdf_obj *font = pdf_resolve_indirect(pdf_dict_get_val(fonts, k));
        if (font && fontList.Find(font) == -1)
            fontList.Append(font);
    }
    // also extract fonts for all XObjects (recursively)
    pdf_obj *xobjs = pdf_dict_gets(res, "XObject");
    for (int k = 0; k < pdf_dict_len(xobjs); k++) {
        pdf_obj *xobj = pdf_dict_get_val(xobjs, k);
        pdf_obj *xres = pdf_dict_gets(xobj, "Resources");
        if (xobj && xres && !pdf_obj_mark(xobj)) {
            pdf_extract_fonts(xres, fontList);
            pdf_obj_unmark(xobj);
        }
    }
}

WCHAR *PdfEngineImpl::ExtractFontList()
{
    Vec<pdf_obj *> fontList;

    // collect all fonts from all page objects
    for (int i = 1; i <= PageCount(); i++) {
        pdf_page *page = GetPdfPage(i);
        if (page) {
            ScopedCritSec scope(&ctxAccess);
            fz_try(ctx) {
                pdf_extract_fonts(page->resources, fontList);
                for (pdf_annot *annot = page->annots; annot; annot = annot->next) {
                    if (annot->ap)
                        pdf_extract_fonts(annot->ap->resources, fontList);
                }
            }
            fz_catch(ctx) { }
        }
    }

    ScopedCritSec scope(&ctxAccess);

    WStrVec fonts;
    for (size_t i = 0; i < fontList.Count(); i++) {
        const char *name = NULL, *type = NULL, *encoding = NULL;
        bool embedded = false;
        fz_try(ctx) {
            pdf_obj *font = fontList.At(i);
            pdf_obj *font2 = pdf_array_get(pdf_dict_gets(font, "DescendantFonts"), 0);
            if (!font2)
                font2 = font;

            name = pdf_to_name(pdf_dict_getsa(font2, "BaseFont", "Name"));
            if (str::IsEmpty(name))
                fz_throw(ctx, "ignoring font with empty name");
            embedded = false;
            pdf_obj *desc = pdf_dict_gets(font2, "FontDescriptor");
            if (desc && (pdf_dict_gets(desc, "FontFile") || pdf_dict_getsa(desc, "FontFile2", "FontFile3")))
                embedded = true;
            if (embedded && str::Len(name) > 7 && name[6] == '+')
                name += 7;

            type = pdf_to_name(pdf_dict_gets(font, "Subtype"));
            if (font2 != font) {
                const char *type2 = pdf_to_name(pdf_dict_gets(font2, "Subtype"));
                if (str::Eq(type2, "CIDFontType0"))
                    type = "Type1 (CID)";
                else if (str::Eq(type2, "CIDFontType2"))
                    type = "TrueType (CID)";
            }

            encoding = pdf_to_name(pdf_dict_gets(font, "Encoding"));
            if (str::Eq(encoding, "WinAnsiEncoding"))
                encoding = "Ansi";
            else if (str::Eq(encoding, "MacRomanEncoding"))
                encoding = "Roman";
            else if (str::Eq(encoding, "MacExpertEncoding"))
                encoding = "Expert";
        }
        fz_catch(ctx) {
            continue;
        }
        CrashIf(!name || !type || !encoding);

        str::Str<char> info;
        if (name[0] < 0 && MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, name, -1, NULL, 0))
            info.Append(ScopedMem<char>(str::ToMultiByte(name, 936, CP_UTF8)));
        else
            info.Append(name);
        if (!str::IsEmpty(encoding) || !str::IsEmpty(type) || embedded) {
            info.Append(" (");
            if (!str::IsEmpty(type))
                info.AppendFmt("%s; ", type);
            if (!str::IsEmpty(encoding))
                info.AppendFmt("%s; ", encoding);
            if (embedded)
                info.Append("embedded; ");
            info.RemoveAt(info.Count() - 2, 2);
            info.Append(")");
        }

        ScopedMem<WCHAR> fontInfo(str::conv::FromUtf8(info.LendData()));
        if (fontInfo && fonts.Find(fontInfo) == -1)
            fonts.Append(fontInfo.StealData());
    }
    if (fonts.Count() == 0)
        return NULL;

    fonts.SortNatural();
    return fonts.Join(L"\n");
}

WCHAR *PdfEngineImpl::GetProperty(DocumentProperty prop)
{
    if (!_doc)
        return NULL;

    if (Prop_PdfVersion == prop) {
        int major = _doc->version / 10, minor = _doc->version % 10;
        if (1 == major && 7 == minor && pdf_crypt_version(_doc) == 5) {
            if (pdf_crypt_revision(_doc) == 5)
                return str::Format(L"%d.%d Adobe Extension Level %d", major, minor, 3);
            if (pdf_crypt_revision(_doc) == 6)
                return str::Format(L"%d.%d Adobe Extension Level %d", major, minor, 8);
        }
        return str::Format(L"%d.%d", major, minor);
    }

    if (Prop_PdfFileStructure == prop) {
        WStrVec fstruct;
        if (pdf_to_bool(pdf_dict_gets(_info, "Linearized")))
            fstruct.Append(str::Dup(L"linearized"));
        if (pdf_to_bool(pdf_dict_gets(_info, "Marked")))
            fstruct.Append(str::Dup(L"tagged"));
        if (pdf_dict_gets(_info, "OutputIntents")) {
            for (int i = 0; i < pdf_array_len(pdf_dict_gets(_info, "OutputIntents")); i++) {
                pdf_obj *intent = pdf_array_get(pdf_dict_gets(_info, "OutputIntents"), i);
                CrashIf(!str::StartsWith(pdf_to_name(intent), "GTS_"));
                fstruct.Append(str::conv::FromUtf8(pdf_to_name(intent) + 4));
            }
        }
        return fstruct.Count() > 0 ? fstruct.Join(L",") : NULL;
    }

    if (Prop_FontList == prop)
        return ExtractFontList();

    static struct {
        DocumentProperty prop;
        char *name;
    } pdfPropNames[] = {
        { Prop_Title, "Title" }, { Prop_Author, "Author" },
        { Prop_Subject, "Subject" }, { Prop_Copyright, "Copyright" },
        { Prop_CreationDate, "CreationDate" }, { Prop_ModificationDate, "ModDate" },
        { Prop_CreatorApp, "Creator" }, { Prop_PdfProducer, "Producer" },
    };
    for (int i = 0; i < dimof(pdfPropNames); i++) {
        if (pdfPropNames[i].prop == prop) {
            // _info is guaranteed not to contain any indirect references,
            // so no need for ctxAccess
            pdf_obj *obj = pdf_dict_gets(_info, pdfPropNames[i].name);
            return obj ? str::conv::FromPdf(obj) : NULL;
        }
    }
    return NULL;
};

bool PdfEngineImpl::SupportsAnnotation(PageAnnotType type, bool forSaving) const
{
    return Annot_Highlight == type;
}

void PdfEngineImpl::UpdateUserAnnotations(Vec<PageAnnotation> *list)
{
    // TODO: use a new critical section to avoid blocking the UI thread
    ScopedCritSec scope(&ctxAccess);
    if (list)
        userAnnots = *list;
    else
        userAnnots.Reset();
}

char *PdfEngineImpl::GetDecryptionKey() const
{
    if (!_decryptionKey)
        return NULL;
    return str::Dup(_decryptionKey);
}

PageLayoutType PdfEngineImpl::PreferredLayout()
{
    PageLayoutType layout = Layout_Single;

    ScopedCritSec scope(&ctxAccess);
    pdf_obj *root = NULL;
    fz_try(ctx) {
        root = pdf_dict_gets(_doc->trailer, "Root");
    }
    fz_catch(ctx) {
        return layout;
    }

    fz_try(ctx) {
        char *name = pdf_to_name(pdf_dict_gets(root, "PageLayout"));
        if (str::EndsWith(name, "Right"))
            layout = Layout_Book;
        else if (str::StartsWith(name, "Two"))
            layout = Layout_Facing;
    }
    fz_catch(ctx) { }

    fz_try(ctx) {
        pdf_obj *prefs = pdf_dict_gets(root, "ViewerPreferences");
        char *direction = pdf_to_name(pdf_dict_gets(prefs, "Direction"));
        if (str::Eq(direction, "R2L"))
            layout = (PageLayoutType)(layout | Layout_R2L);
    }
    fz_catch(ctx) { }

    return layout;
}

unsigned char *PdfEngineImpl::GetFileData(size_t *cbCount)
{
    unsigned char *data = NULL;
    ScopedCritSec scope(&ctxAccess);
    fz_try(ctx) {
        data = fz_extract_stream_data(_doc->file, cbCount);
    }
    fz_catch(ctx) {
        return _fileName ? (unsigned char *)file::ReadAll(_fileName, cbCount) : NULL;
    }
    return data;
}

bool PdfEngineImpl::SaveFileAs(const WCHAR *copyFileName)
{
    size_t dataLen;
    ScopedMem<unsigned char> data(GetFileData(&dataLen));
    if (data) {
        bool ok = file::WriteAll(copyFileName, data.Get(), dataLen);
        if (ok)
            return SaveUserAnnots(copyFileName);
    }
    if (!_fileName)
        return false;
    bool ok = CopyFile(_fileName, copyFileName, FALSE);
    if (!ok)
        return false;
    return SaveUserAnnots(copyFileName);
}

bool PdfEngineImpl::SaveUserAnnots(const WCHAR *fileName)
{
    if (!userAnnots.Count())
        return true;

    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(&ctxAccess);

    static const char *ap_dict = "<< /Type /XObject /Subtype /Form /BBox [0 0 1 1] /Resources << /ExtGState << /GS << /Type /ExtGState /ca 0.8 /AIS false /BM /Multiply >> >> /ProcSet [/PDF] >> >>";
    static const char *ap_stream = "q /GS gs 0.886275 0.768627 0.886275 rg 0 0 1 1 re f Q";
    static const char *annot_dict = "<< /Type /Annot /Subtype /Highlight /C [0.886275 0.768627 0.886275] /AP << >> >>";

    bool ok = true;
    pdf_obj *obj = NULL, *obj2 = NULL, *page = NULL;
    fz_buffer *buf = NULL;
    pdf_file_update_list *list = NULL;
    int next_num = _doc->len;

    fz_var(obj);
    fz_var(obj2);
    fz_var(page);
    fz_var(buf);
    fz_var(list);

    fz_try(ctx) {
        list = pdf_file_update_start_w(ctx, fileName, next_num + PageCount() + userAnnots.Count() + 1);
        // append appearance stream for all highlights (required e.g. for Chrome's built-in viewer)
        int ap_num = next_num++;
        obj = pdf_new_obj_from_str(ctx, ap_dict);
        buf = fz_new_buffer(ctx, (int)str::Len(ap_stream));
        memcpy(buf->data, ap_stream, (buf->len = (int)str::Len(ap_stream)));
        pdf_file_update_append(list, obj, ap_num, 0, buf);
        pdf_drop_obj(obj);
        obj = NULL;
        fz_drop_buffer(ctx, buf);
        buf = NULL;
        // prepare the annotation template object
        obj = pdf_new_obj_from_str(ctx, annot_dict);
        pdf_dict_puts_drop(pdf_dict_gets(obj, "AP"), "N", pdf_new_indirect(ctx, ap_num, 0, NULL));
        // append annotations per page
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            // TODO: this will skip annotations for broken documents
            if (!GetPdfPage(pageNo) || !pdf_to_num(_doc->page_refs[pageNo-1])) {
                ok = false;
                break;
            }
            Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, pageNo);
            if (pageAnnots.Count() == 0)
                continue;
            // get the page's /Annots array for appending
            page = pdf_copy_dict(ctx, _doc->page_objs[pageNo-1]);
            if (!pdf_is_array(pdf_dict_gets(page, "Annots")))
                pdf_dict_puts_drop(page, "Annots", pdf_new_array(ctx, pageAnnots.Count()));
            pdf_obj *annots = pdf_dict_gets(page, "Annots");
            // append all annotations for the current page
            for (size_t i = 0; i < pageAnnots.Count(); i++) {
                PageAnnotation& annot = pageAnnots.At(i);
                CrashIf(annot.type != Annot_Highlight);
                // update the annotation's /Rect (converted to raw user space)
                fz_rect r = fz_RectD_to_rect(annot.rect);
                r = fz_transform_rect(fz_invert_matrix(GetPdfPage(pageNo)->ctm), r);
                pdf_dict_puts_drop(obj, "Rect", pdf_new_rect(ctx, &r));
                // all /QuadPoints must lie within /Rect
                ScopedMem<char> quadpoints(str::Format("[%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f]",
                                                       r.x0, r.y1, r.x1, r.y1, r.x0, r.y0, r.x1, r.y0));
                pdf_dict_puts_drop(obj, "QuadPoints", pdf_new_obj_from_str(ctx, quadpoints));
                // add a reference back to the page
                pdf_dict_puts_drop(obj, "P", pdf_new_indirect(ctx, pdf_to_num(_doc->page_refs[pageNo-1]), pdf_to_gen(_doc->page_refs[pageNo-1]), NULL));
                // append a reference to the annotation to the page's /Annots entry
                obj2 = pdf_new_indirect(ctx, next_num, 0, NULL);
                pdf_array_push(annots, obj2);
                pdf_drop_obj(obj2);
                obj2 = NULL;
                // finally write the annotation to the file
                pdf_file_update_append(list, obj, next_num++, 0, NULL);
            }
            // write the page object with the update /Annots array back to the file
            pdf_file_update_append(list, page, pdf_to_num(_doc->page_refs[pageNo-1]), pdf_to_gen(_doc->page_refs[pageNo-1]), NULL);
            pdf_drop_obj(page);
            page = NULL;
        }
    }
    fz_always(ctx) {
        pdf_drop_obj(obj);
        pdf_drop_obj(obj2);
        pdf_drop_obj(page);
        fz_drop_buffer(ctx, buf);
        if (list) {
            // write xref, trailer and startxref entries and clean up
            fz_try(ctx) {
                pdf_file_update_end(list, _doc->trailer, _doc->startxref);
            }
            fz_catch(ctx) { }
        }
    }
    fz_catch(ctx) {
        return false;
    }
    return ok;
}

bool PdfEngineImpl::SaveEmbedded(LinkSaverUI& saveUI, int num, int gen)
{
    ScopedCritSec scope(&ctxAccess);

    fz_buffer *data = NULL;
    fz_try(ctx) {
        data = pdf_load_stream(_doc, num, gen);
    }
    fz_catch(ctx) {
        return false;
    }
    CrashIf(NULL == data);
    bool result = saveUI.SaveEmbedded(data->data, data->len);
    fz_drop_buffer(ctx, data);
    return result;
}

bool PdfEngineImpl::HasClipOptimizations(int pageNo)
{
    pdf_page *page = GetPdfPage(pageNo, true);
    // GetPdfPage extracts imageRects for us
    if (!page || !imageRects[pageNo-1])
        return true;

    fz_rect mbox = fz_RectD_to_rect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (int i = 0; !fz_is_empty_rect(imageRects[pageNo-1][i]); i++)
        if (fz_calc_overlap(mbox, imageRects[pageNo-1][i]) >= 0.9f)
            return false;
    return true;
}

WCHAR *PdfEngineImpl::GetPageLabel(int pageNo)
{
    if (!_pagelabels || pageNo < 1 || PageCount() < pageNo)
        return BaseEngine::GetPageLabel(pageNo);

    return str::Dup(_pagelabels->At(pageNo - 1));
}

int PdfEngineImpl::GetPageByLabel(const WCHAR *label)
{
    int pageNo = _pagelabels ? _pagelabels->Find(label) + 1 : 0;
    if (!pageNo)
        return BaseEngine::GetPageByLabel(label);

    return pageNo;
}

static bool IsRelativeURI(const WCHAR *uri)
{
    const WCHAR *colon = str::FindChar(uri, ':');
    const WCHAR *slash = str::FindChar(uri, '/');

    return !colon || (slash && colon > slash);
}

WCHAR *PdfLink::GetValue() const
{
    if (!link || !engine)
        return NULL;
    if (link->kind != FZ_LINK_URI && link->kind != FZ_LINK_LAUNCH &&
        link->kind != FZ_LINK_GOTOR)
        return NULL;

    ScopedCritSec scope(&engine->ctxAccess);

    WCHAR *path = NULL;

    switch (link->kind) {
    case FZ_LINK_URI:
        path = str::conv::FromUtf8(link->ld.uri.uri);
        if (IsRelativeURI(path)) {
            ScopedMem<WCHAR> base;
            fz_try(engine->ctx) {
                pdf_obj *obj = pdf_dict_gets(engine->_doc->trailer, "Root");
                obj = pdf_dict_gets(pdf_dict_gets(obj, "URI"), "Base");
                if (obj)
                    base.Set(str::conv::FromPdf(obj));
            }
            fz_catch(engine->ctx) { }
            if (!str::IsEmpty(base.Get())) {
                ScopedMem<WCHAR> uri(str::Join(base, path));
                free(path);
                path = uri.StealData();
            }
        }
        if (link->ld.uri.is_map) {
            int x = 0, y = 0;
            if (rect.Contains(pt)) {
                x = (int)(pt.x - rect.x + 0.5);
                y = (int)(pt.y - rect.y + 0.5);
            }
            ScopedMem<WCHAR> uri(str::Format(L"%s?%d,%d", path, x, y));
            free(path);
            path = uri.StealData();
        }
        break;
    case FZ_LINK_LAUNCH:
        // note: we (intentionally) don't support the /Win specific Launch parameters
        if (link->ld.launch.file_spec)
            path = str::conv::FromUtf8(link->ld.launch.file_spec);
        if (path && link->ld.launch.embedded_num && str::EndsWithI(path, L".pdf")) {
            free(path);
            path = str::Format(L"%s:%d:%d", engine->FileName(),
                link->ld.launch.embedded_num, link->ld.launch.embedded_gen);
        }
        break;
    case FZ_LINK_GOTOR:
        if (link->ld.gotor.file_spec)
            path = str::conv::FromUtf8(link->ld.gotor.file_spec);
        break;
    }

    return path;
}

static PageDestType DestTypeFromName(const char *name)
{
    // named actions are converted either to Dest_Name or Dest_NameDialog
#define HandleType(type) if (str::Eq(name, #type)) return Dest_ ## type
#define HandleTypeDialog(type) if (str::Eq(name, #type)) return Dest_ ## type ## Dialog
    // predefined named actions
    HandleType(NextPage);
    HandleType(PrevPage);
    HandleType(FirstPage);
    HandleType(LastPage);
    // Adobe Reader extensions to the spec
    // cf. http://www.tug.org/applications/hyperref/manual.html
    HandleTypeDialog(Find);
    HandleType(FullScreen);
    HandleType(GoBack);
    HandleType(GoForward);
    HandleTypeDialog(GoToPage);
    HandleTypeDialog(Print);
    HandleTypeDialog(SaveAs);
    HandleTypeDialog(ZoomTo);
#undef HandleType
#undef HandleTypeDialog
    // named action that we don't support (or invalid action name)
    return Dest_None;
}

PageDestType PdfLink::GetDestType() const
{
    if (!link)
        return Dest_None;

    switch (link->kind) {
    case FZ_LINK_GOTO:
        return Dest_ScrollTo;
    case FZ_LINK_URI:
        return Dest_LaunchURL;
    case FZ_LINK_NAMED:
        return DestTypeFromName(link->ld.named.named);
    case FZ_LINK_LAUNCH:
        if (link->ld.launch.embedded_num)
            return Dest_LaunchEmbedded;
        return Dest_LaunchFile;
    case FZ_LINK_GOTOR:
        return Dest_LaunchFile;
    default:
        return Dest_None; // unsupported action
    }
}

int PdfLink::GetDestPageNo() const
{
    if (link && FZ_LINK_GOTO == link->kind)
        return link->ld.gotor.page + 1;
    if (link && FZ_LINK_GOTOR == link->kind && !link->ld.gotor.rname)
        return link->ld.gotor.page + 1;
    return 0;
}

RectD PdfLink::GetDestRect() const
{
    RectD result(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    if (!link || FZ_LINK_GOTO != link->kind && FZ_LINK_GOTOR != link->kind)
        return result;
    if (link->ld.gotor.page < 0 || link->ld.gotor.page >= engine->PageCount())
        return result;

    pdf_page *page = engine->GetPdfPage(link->ld.gotor.page + 1);
    if (!page)
        return result;
    fz_point lt = fz_transform_point(page->ctm, link->ld.gotor.lt);
    fz_point rb = fz_transform_point(page->ctm, link->ld.gotor.rb);

    if ((link->ld.gotor.flags & fz_link_flag_r_is_zoom)) {
        // /XYZ link, undefined values for the coordinates mean: keep the current position
        if ((link->ld.gotor.flags & fz_link_flag_l_valid))
            result.x = lt.x;
        if ((link->ld.gotor.flags & fz_link_flag_t_valid))
            result.y = lt.y;
        result.dx = result.dy = 0;
    }
    else if ((link->ld.gotor.flags & (fz_link_flag_fit_h | fz_link_flag_fit_v)) == (fz_link_flag_fit_h | fz_link_flag_fit_v) &&
        (link->ld.gotor.flags & (fz_link_flag_l_valid | fz_link_flag_t_valid | fz_link_flag_r_valid | fz_link_flag_b_valid))) {
        // /FitR link
        result = RectD::FromXY(lt.x, lt.y, rb.x, rb.y);
        // an empty destination rectangle would imply an /XYZ-type link to callers
        if (result.IsEmpty())
            result.dx = result.dy = 0.1;
    }
    else if ((link->ld.gotor.flags & (fz_link_flag_fit_h | fz_link_flag_fit_v)) == fz_link_flag_fit_h) {
        // /FitH or /FitBH link
        result.y = lt.y;
    }
    // all other link types only affect the zoom level, which we intentionally leave alone
    return result;
}

WCHAR *PdfLink::GetDestName() const
{
    if (!link || FZ_LINK_GOTOR != link->kind || !link->ld.gotor.rname)
        return NULL;
    return str::conv::FromUtf8(link->ld.gotor.rname);
}

bool PdfLink::SaveEmbedded(LinkSaverUI& saveUI)
{
    ScopedCritSec scope(&engine->ctxAccess);
    return engine->SaveEmbedded(saveUI, link->ld.launch.embedded_num, link->ld.launch.embedded_gen);
}

bool PdfEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        char header[1024];
        ZeroMemory(header, sizeof(header));
        file::ReadAll(fileName, header, sizeof(header));

        for (int i = 0; i < sizeof(header) - 4; i++)
            if (str::EqN(header + i, "%PDF", 4))
                return true;
        return false;
    }

    return str::EndsWithI(fileName, L".pdf") || findEmbedMarks(fileName);
}

PdfEngine *PdfEngine::CreateFromFile(const WCHAR *fileName, PasswordUI *pwdUI)
{
    PdfEngineImpl *engine = new PdfEngineImpl();
    if (!engine || !fileName || !engine->Load(fileName, pwdUI)) {
        delete engine;
        return NULL;
    }
    return engine;
}

PdfEngine *PdfEngine::CreateFromStream(IStream *stream, PasswordUI *pwdUI)
{
    PdfEngineImpl *engine = new PdfEngineImpl();
    if (!engine->Load(stream, pwdUI)) {
        delete engine;
        return NULL;
    }
    return engine;
}

///// XpsEngine is also based on Fitz and shares quite some code with PdfEngine /////

extern "C" {
#include <muxps-internal.h>
}

struct XpsPageRun {
    xps_page *page;
    fz_display_list *list;
    size_t size_est;
    int refs;

    XpsPageRun(xps_page *page, fz_display_list *list, ListInspectionData& data) :
        page(page), list(list), size_est(data.mem_estimate), refs(1) { }
};

class XpsTocItem;
class XpsImage;

class XpsEngineImpl : public XpsEngine {
    friend XpsEngine;
    friend XpsImage;

public:
    XpsEngineImpl();
    virtual ~XpsEngineImpl();
    virtual XpsEngineImpl *Clone();

    virtual const WCHAR *FileName() const { return _fileName; };
    virtual int PageCount() const {
        return _doc ? xps_count_pages(_doc) : 0;
    }

    virtual RectD PageMediabox(int pageNo);
    virtual RectD PageContentBox(int pageNo, RenderTarget target=Target_View);

    virtual RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=NULL);
    virtual bool RenderPage(HDC hDC, RectI screenRect, int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, RenderTarget target=Target_View, AbortCookie **cookie_out=NULL) {
        return RenderPage(hDC, GetXpsPage(pageNo), screenRect, NULL, zoom, rotation, pageRect, cookie_out);
    }

    virtual PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false);
    virtual RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false);

    virtual unsigned char *GetFileData(size_t *cbCount);
    virtual bool SaveFileAs(const WCHAR *copyFileName);
    virtual WCHAR * ExtractPageText(int pageNo, WCHAR *lineSep, RectI **coords_out=NULL,
                                    RenderTarget target=Target_View) {
        return ExtractPageText(GetXpsPage(pageNo), lineSep, coords_out);
    }
    virtual bool HasClipOptimizations(int pageNo);
    virtual WCHAR *GetProperty(DocumentProperty prop);

    virtual bool SupportsAnnotation(PageAnnotType type, bool forSaving=false) const;
    virtual void UpdateUserAnnotations(Vec<PageAnnotation> *list);

    virtual float GetFileDPI() const { return 72.0f; }
    virtual const WCHAR *GetDefaultFileExt() const { return L".xps"; }

    virtual bool BenchLoadPage(int pageNo) { return GetXpsPage(pageNo) != NULL; }

    virtual Vec<PageElement *> *GetElements(int pageNo);
    virtual PageElement *GetElementAtPos(int pageNo, PointD pt);

    virtual PageDestination *GetNamedDest(const WCHAR *name);
    virtual bool HasTocTree() const { return _outline != NULL; }
    virtual DocTocItem *GetTocTree();

    fz_rect FindDestRect(const char *target);

protected:
    WCHAR *_fileName;

    // make sure to never ask for _pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION ctxAccess;
    fz_context *    ctx;
    fz_locks_context fz_locks_ctx;
    xps_document *  _doc;

    CRITICAL_SECTION _pagesAccess;
    xps_page **     _pages;

    virtual bool    Load(const WCHAR *fileName);
    virtual bool    Load(IStream *stream);
    bool            Load(fz_stream *stm);
    bool            LoadFromStream(fz_stream *stm);

    xps_page      * GetXpsPage(int pageNo, bool failIfBusy=false);
    int             GetPageNo(xps_page *page);
    fz_matrix       viewctm(int pageNo, float zoom, int rotation) {
        return fz_create_view_ctm(fz_RectD_to_rect(PageMediabox(pageNo)), zoom, rotation);
    }
    fz_matrix       viewctm(xps_page *page, float zoom, int rotation) {
        return fz_create_view_ctm(xps_bound_page(_doc, page), zoom, rotation);
    }
    bool            RenderPage(HDC hDC, xps_page *page, RectI screenRect,
                               fz_matrix *ctm, float zoom, int rotation,
                               RectD *pageRect, AbortCookie **cookie_out);
    WCHAR         * ExtractPageText(xps_page *page, WCHAR *lineSep,
                                    RectI **coords_out=NULL, bool cacheRun=false);

    Vec<XpsPageRun*>runCache; // ordered most recently used first
    XpsPageRun    * CreatePageRun(xps_page *page, fz_display_list *list);
    XpsPageRun    * GetPageRun(xps_page *page, bool tryOnly=false);
    bool            RunPage(xps_page *page, fz_device *dev, fz_matrix ctm,
                            fz_bbox clipbox=fz_infinite_bbox, bool cacheRun=true,
                            FitzAbortCookie *cookie=NULL);
    void            DropPageRun(XpsPageRun *run, bool forceRemove=false);

    XpsTocItem    * BuildTocTree(fz_outline *entry, int& idCounter);
    void            LinkifyPageText(xps_page *page, int pageNo);
    RenderedBitmap *GetPageImage(int pageNo, RectD rect, size_t imageIx);
    WCHAR         * ExtractFontList();

    RectD         * _mediaboxes;
    fz_outline    * _outline;
    xps_doc_props * _info;
    fz_rect      ** imageRects;

    Vec<PageAnnotation> userAnnots;
};

class XpsLink : public PageElement, public PageDestination {
    XpsEngineImpl *engine;
    fz_link_dest *link; // owned by a fz_link or fz_outline
    RectD rect;
    int pageNo;

public:
    XpsLink() : engine(NULL), link(NULL), pageNo(-1) { }
    XpsLink(XpsEngineImpl *engine, fz_link_dest *link, fz_rect rect=fz_empty_rect, int pageNo=-1) :
        engine(engine), link(link), rect(fz_rect_to_RectD(rect)), pageNo(pageNo) { }

    virtual PageElementType GetType() const { return Element_Link; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return rect; }
    virtual WCHAR *GetValue() const {
        if (link && FZ_LINK_URI == link->kind)
            return str::conv::FromUtf8(link->ld.uri.uri);
        return NULL;
    }
    virtual PageDestination *AsLink() { return this; }

    virtual PageDestType GetDestType() const {
        if (!link)
            return Dest_None;
        if (FZ_LINK_GOTO == link->kind)
            return Dest_ScrollTo;
        if (FZ_LINK_URI == link->kind)
            return Dest_LaunchURL;
        return Dest_None;
    }
    virtual int GetDestPageNo() const {
        if (!link || link->kind != FZ_LINK_GOTO)
            return 0;
        return link->ld.gotor.page + 1;
    }
    virtual RectD GetDestRect() const {
        if (!engine || !link || link->kind != FZ_LINK_GOTO || !link->ld.gotor.rname)
            return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
        return fz_rect_to_RectD(engine->FindDestRect(link->ld.gotor.rname));
    }
    virtual WCHAR *GetDestValue() const { return GetValue(); }
};

class XpsTocItem : public DocTocItem {
    XpsLink link;

public:
    XpsTocItem(WCHAR *title, XpsLink link) : DocTocItem(title), link(link) { }

    virtual PageDestination *GetLink() { return &link; }
};

class XpsImage : public PageElement {
    XpsEngineImpl *engine;
    int pageNo;
    RectD rect;
    size_t imageIx;

public:
    XpsImage(XpsEngineImpl *engine, int pageNo, fz_rect rect, size_t imageIx) :
        engine(engine), pageNo(pageNo), rect(fz_rect_to_RectD(rect)), imageIx(imageIx) { }

    virtual PageElementType GetType() const { return Element_Image; }
    virtual int GetPageNo() const { return pageNo; }
    virtual RectD GetRect() const { return rect; }
    virtual WCHAR *GetValue() const { return NULL; }

    virtual RenderedBitmap *GetImage() {
        return engine->GetPageImage(pageNo, rect, imageIx);
    }
};

XpsEngineImpl::XpsEngineImpl() : _fileName(NULL), _doc(NULL), _pages(NULL), _mediaboxes(NULL),
    _outline(NULL), _info(NULL), imageRects(NULL)
{
    InitializeCriticalSection(&_pagesAccess);
    InitializeCriticalSection(&ctxAccess);

    fz_locks_ctx.user = &ctxAccess;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(NULL, &fz_locks_ctx, MAX_CONTEXT_MEMORY);
}

XpsEngineImpl::~XpsEngineImpl()
{
    EnterCriticalSection(&_pagesAccess);
    EnterCriticalSection(&ctxAccess);

    if (_pages) {
        // xps_pages are freed by xps_close_document -> xps_free_page_list
        assert(_doc);
        free(_pages);
    }
    if (_outline)
        fz_free_outline(ctx, _outline);
    if (_mediaboxes)
        delete[] _mediaboxes;
    if (imageRects) {
        for (int i = 0; i < PageCount(); i++)
            free(imageRects[i]);
        free(imageRects);
    }

    if (_doc) {
        xps_close_document(_doc);
        _doc = NULL;
    }
    if (_info)
        xps_free_doc_props(ctx, _info);

    while (runCache.Count() > 0) {
        assert(runCache.Last()->refs == 1);
        DropPageRun(runCache.Last(), true);
    }

    free(_fileName);

    fz_free_context(ctx);

    LeaveCriticalSection(&ctxAccess);
    DeleteCriticalSection(&ctxAccess);
    LeaveCriticalSection(&_pagesAccess);
    DeleteCriticalSection(&_pagesAccess);
}

XpsEngineImpl *XpsEngineImpl::Clone()
{
    ScopedCritSec scope(&ctxAccess);

    XpsEngineImpl *clone = new XpsEngineImpl();
    if (!clone || !(_fileName ? clone->Load(_fileName) : clone->Load(_doc->file))) {
        delete clone;
        return NULL;
    }

    clone->UpdateUserAnnotations(&userAnnots);

    return clone;
}

bool XpsEngineImpl::Load(const WCHAR *fileName)
{
    assert(!_fileName && !_doc && ctx);
    _fileName = str::Dup(fileName);
    if (!_fileName || !ctx)
        return false;

    fz_stream *stm;
    fz_try(ctx) {
        stm = fz_open_file2(ctx, _fileName);
    }
    fz_catch(ctx) {
        return false;
    }
    return LoadFromStream(stm);
}

bool XpsEngineImpl::Load(IStream *stream)
{
    assert(!_fileName && !_doc && ctx);
    if (!ctx)
        return false;

    fz_stream *stm = NULL;
    fz_try(ctx) {
        stm = fz_open_istream(ctx, stream);
    }
    fz_catch(ctx) {
        return false;
    }
    return LoadFromStream(stm);
}

bool XpsEngineImpl::Load(fz_stream *stm)
{
    assert(!_fileName && !_doc && ctx);
    if (!ctx)
        return false;

    fz_try(ctx) {
        stm = fz_clone_stream(ctx, stm);
    }
    fz_catch(ctx) {
        return false;
    }
    return LoadFromStream(stm);
}

bool XpsEngineImpl::LoadFromStream(fz_stream *stm)
{
    if (!stm)
        return false;

    fz_try(ctx) {
        _doc = xps_open_document_with_stream(ctx, stm);
    }
    fz_always(ctx) {
        fz_close(stm);
    }
    fz_catch(ctx) {
        return false;
    }

    if (PageCount() == 0) {
        fz_warn(ctx, "document has no pages");
        return false;
    }

    _pages = AllocArray<xps_page *>(PageCount());
    _mediaboxes = new RectD[PageCount()];
    imageRects = AllocArray<fz_rect *>(PageCount());

    if (!_pages || !_mediaboxes || !imageRects)
        return false;

    fz_try(ctx) {
        _outline = xps_load_outline(_doc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load outline");
    }
    fz_try(ctx) {
        _info = xps_extract_doc_props(_doc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load document properties");
    }

    return true;
}

xps_page *XpsEngineImpl::GetXpsPage(int pageNo, bool failIfBusy)
{
    if (!_pages)
        return NULL;
    if (failIfBusy)
        return _pages[pageNo-1];

    ScopedCritSec scope(&_pagesAccess);

    xps_page *page = _pages[pageNo-1];
    if (!page) {
        ScopedCritSec ctxScope(&ctxAccess);
        fz_var(page);
        fz_try(ctx) {
            // caution: two calls to xps_load_page return the
            // same xps_page object (without reference counting)
            page = xps_load_page(_doc, pageNo - 1);
            _pages[pageNo-1] = page;
            LinkifyPageText(page, pageNo);
            assert(page->links_resolved);
        }
        fz_catch(ctx) { }
    }

    return page;
}

int XpsEngineImpl::GetPageNo(xps_page *page)
{
    for (int i = 0; i < PageCount(); i++)
        if (page == _pages[i])
            return i + 1;
    return 0;
}

XpsPageRun *XpsEngineImpl::CreatePageRun(xps_page *page, fz_display_list *list)
{
    Vec<FitzImagePos> positions;
    ListInspectionData data(positions);
    fz_device *dev = NULL;

    fz_var(dev);
    fz_try(ctx) {
        dev = fz_new_inspection_device(ctx, &data);
        fz_run_display_list(list, dev, fz_identity, fz_infinite_bbox, NULL);
    }
    fz_catch(ctx) { }
    fz_free_device(dev);

    // save the image rectangles for this page
    int pageNo = GetPageNo(page);
    if (!imageRects[pageNo-1] && positions.Count() > 0) {
        // the list of page image rectangles is terminated with a null-rectangle
        fz_rect *rects = AllocArray<fz_rect>(positions.Count() + 1);
        if (rects) {
            for (size_t i = 0; i < positions.Count(); i++) {
                rects[i] = positions.At(i).rect;
            }
            imageRects[pageNo-1] = rects;
        }
    }

    return new XpsPageRun(page, list, data);
}

XpsPageRun *XpsEngineImpl::GetPageRun(xps_page *page, bool tryOnly)
{
    ScopedCritSec scope(&_pagesAccess);

    XpsPageRun *result = NULL;

    for (size_t i = 0; i < runCache.Count(); i++) {
        if (runCache.At(i)->page == page) {
            result = runCache.At(i);
            break;
        }
    }
    if (!result && !tryOnly) {
        size_t mem = 0;
        for (size_t i = 0; i < runCache.Count(); i++) {
            // drop page runs that take up too much memory due to huge images
            // (except for the very recently used ones)
            if (i >= 2 && mem + runCache.At(i)->size_est >= MAX_PAGE_RUN_MEMORY)
                DropPageRun(runCache.At(i--), true);
            else
                mem += runCache.At(i)->size_est;
        }
        if (runCache.Count() >= MAX_PAGE_RUN_CACHE) {
            assert(runCache.Count() == MAX_PAGE_RUN_CACHE);
            DropPageRun(runCache.Last(), true);
        }

        ScopedCritSec ctxScope(&ctxAccess);

        fz_display_list *list = NULL;
        fz_device *dev = NULL;
        fz_var(list);
        fz_var(dev);
        fz_try(ctx) {
            list = fz_new_display_list(ctx);
            dev = fz_new_list_device(ctx, list);
            xps_run_page(_doc, page, dev, fz_identity, NULL);
        }
        fz_catch(ctx) {
            fz_free_display_list(ctx, list);
            list = NULL;
        }
        fz_free_device(dev);

        if (list) {
            result = CreatePageRun(page, list);
            runCache.InsertAt(0, result);
        }
    }
    else if (result && result != runCache.At(0)) {
        // keep the list Most Recently Used first
        runCache.Remove(result);
        runCache.InsertAt(0, result);
    }

    if (result)
        result->refs++;
    return result;
}

bool XpsEngineImpl::RunPage(xps_page *page, fz_device *dev, fz_matrix ctm, fz_bbox clipbox, bool cacheRun, FitzAbortCookie *cookie)
{
    bool ok = true;

    XpsPageRun *run = GetPageRun(page, !cacheRun);
    if (run) {
        EnterCriticalSection(&ctxAccess);
        Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, GetPageNo(page));
        fz_try(ctx) {
            fz_run_page_transparency(pageAnnots, dev, clipbox, false);
            fz_run_display_list(run->list, dev, ctm, clipbox, cookie ? &cookie->cookie : NULL);
            fz_run_page_transparency(pageAnnots, dev, clipbox, true);
            fz_run_user_page_annots(pageAnnots, dev, ctm, clipbox, cookie ? &cookie->cookie : NULL);
        }
        fz_catch(ctx) {
            ok = false;
        }
        LeaveCriticalSection(&ctxAccess);
        DropPageRun(run);
    }
    else {
        ScopedCritSec scope(&ctxAccess);
        Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, GetPageNo(page));
        fz_try(ctx) {
            fz_run_page_transparency(pageAnnots, dev, clipbox, false);
            xps_run_page(_doc, page, dev, ctm, cookie ? &cookie->cookie : NULL);
            fz_run_page_transparency(pageAnnots, dev, clipbox, true);
            fz_run_user_page_annots(pageAnnots, dev, ctm, clipbox, cookie ? &cookie->cookie : NULL);
        }
        fz_catch(ctx) {
            ok = false;
        }
    }

    EnterCriticalSection(&ctxAccess);
    fz_free_device(dev);
    LeaveCriticalSection(&ctxAccess);

    return ok && !(cookie && cookie->cookie.abort);
}

void XpsEngineImpl::DropPageRun(XpsPageRun *run, bool forceRemove)
{
    ScopedCritSec scope(&_pagesAccess);
    run->refs--;

    if (0 == run->refs || forceRemove) {
        runCache.Remove(run);
        if (0 == run->refs) {
            ScopedCritSec ctxScope(&ctxAccess);
            fz_free_display_list(ctx, run->list);
            delete run;
        }
    }
}

RectD XpsEngineImpl::PageMediabox(int pageNo)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    if (!_mediaboxes)
        return RectD();

    RectD mbox = _mediaboxes[pageNo-1];
    if (!mbox.IsEmpty())
        return mbox;

    xps_page *page = GetXpsPage(pageNo, true);
    if (!page) {
        ScopedCritSec scope(&ctxAccess);
        fz_try(ctx) {
            mbox = fz_rect_to_RectD(xps_bound_page_quick_and_dirty(_doc, pageNo-1));
        }
        fz_catch(ctx) { }
        if (!mbox.IsEmpty()) {
            _mediaboxes[pageNo-1] = mbox;
            return _mediaboxes[pageNo-1];
        }
    }
    if (!page && !(page = GetXpsPage(pageNo)))
        return RectD();

    _mediaboxes[pageNo-1] = fz_rect_to_RectD(xps_bound_page(_doc, page));
    return _mediaboxes[pageNo-1];
}

RectD XpsEngineImpl::PageContentBox(int pageNo, RenderTarget target)
{
    assert(1 <= pageNo && pageNo <= PageCount());
    xps_page *page = GetXpsPage(pageNo);
    if (!page)
        return RectD();

    fz_bbox bbox = { 0 };
    fz_device *dev = NULL;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &bbox);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return RectD();
    }
    LeaveCriticalSection(&ctxAccess);

    fz_bbox mediabox = fz_round_rect(xps_bound_page(_doc, page));
    bool ok = RunPage(page, dev, fz_identity, mediabox, false);
    if (!ok)
        return PageMediabox(pageNo);
    if (fz_is_infinite_bbox(bbox))
        return PageMediabox(pageNo);

    RectD bbox2 = fz_bbox_to_RectI(bbox).Convert<double>();
    return bbox2.Intersect(PageMediabox(pageNo));
}

PointD XpsEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    fz_point pt2 = { (float)pt.x, (float)pt.y };
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    pt2 = fz_transform_point(ctm, pt2);
    return PointD(pt2.x, pt2.y);
}

RectD XpsEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    fz_rect rect2 = fz_RectD_to_rect(rect);
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        ctm = fz_invert_matrix(ctm);
    rect2 = fz_transform_rect(ctm, rect2);
    return fz_rect_to_RectD(rect2);
}

bool XpsEngineImpl::RenderPage(HDC hDC, xps_page *page, RectI screenRect, fz_matrix *ctm, float zoom, int rotation, RectD *pageRect, AbortCookie **cookie_out)
{
    if (!page)
        return false;

    fz_matrix ctm2;
    if (!ctm) {
        ctm2 = viewctm(page, zoom, rotation);
        fz_rect pRect = pageRect ? fz_RectD_to_rect(*pageRect) : xps_bound_page(_doc, page);
        fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm2, pRect));
        ctm2 = fz_concat(ctm2, fz_translate((float)screenRect.x - bbox.x0, (float)screenRect.y - bbox.y0));
    }
    else
        ctm2 = *ctm;

    HBRUSH bgBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
    FillRect(hDC, &screenRect.ToRECT(), bgBrush); // initialize white background
    DeleteObject(bgBrush);

    fz_bbox clipbox = fz_RectI_to_bbox(screenRect);
    if (pageRect) {
        fz_bbox pageclip = fz_round_rect(fz_transform_rect(ctm2, fz_RectD_to_rect(*pageRect)));
        clipbox = fz_intersect_bbox(clipbox, pageclip);
    }
    fz_device *dev = NULL;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_gdiplus_device(ctx, hDC, clipbox);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return false;
    }
    LeaveCriticalSection(&ctxAccess);

    FitzAbortCookie *cookie = NULL;
    if (cookie_out)
        *cookie_out = cookie = new FitzAbortCookie();
    return RunPage(page, dev, ctm2, clipbox, true, cookie);
}

RenderedBitmap *XpsEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    xps_page* page = GetXpsPage(pageNo);
    if (!page)
        return NULL;

    fz_rect pRect = pageRect ? fz_RectD_to_rect(*pageRect) : xps_bound_page(_doc, page);
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm, pRect));

    // GDI+ seems to render quicker and more reliably at high zoom levels
    if ((zoom > 40.0) != gDebugGdiPlusDevice) {
        int w = bbox.x1 - bbox.x0, h = bbox.y1 - bbox.y0;
        ctm = fz_concat(ctm, fz_translate((float)-bbox.x0, (float)-bbox.y0));

        // for now, don't render directly into a DC but produce an HBITMAP instead
        HDC hDC = GetDC(NULL);
        HDC hDCMem = CreateCompatibleDC(hDC);
        HBITMAP hbmp = CreateCompatibleBitmap(hDC, w, h);
        DeleteObject(SelectObject(hDCMem, hbmp));

        RectI rc(0, 0, w, h);
        bool ok = RenderPage(hDCMem, page, rc, &ctm, 0, 0, pageRect, cookie_out);
        DeleteDC(hDCMem);
        ReleaseDC(NULL, hDC);
        if (!ok) {
            DeleteObject(hbmp);
            return NULL;
        }
        return new RenderedBitmap(hbmp, SizeI(w, h));
    }

    fz_pixmap *image = NULL;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        fz_colorspace *colorspace = fz_find_device_colorspace(ctx, "DeviceRGB");
        image = fz_new_pixmap_with_bbox(ctx, colorspace, bbox);
        fz_clear_pixmap_with_value(ctx, image, 0xFF); // initialize white background
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }

    fz_device *dev = NULL;
    fz_try(ctx) {
        dev = fz_new_draw_device(ctx, image);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, image);
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }
    LeaveCriticalSection(&ctxAccess);

    FitzAbortCookie *cookie = NULL;
    if (cookie_out)
        *cookie_out = cookie = new FitzAbortCookie();
    bool ok = RunPage(page, dev, ctm, bbox, true, cookie);

    ScopedCritSec scope(&ctxAccess);

    RenderedBitmap *bitmap = NULL;
    if (ok)
        bitmap = new RenderedFitzBitmap(ctx, image);
    fz_drop_pixmap(ctx, image);
    return bitmap;
}

WCHAR *XpsEngineImpl::ExtractPageText(xps_page *page, WCHAR *lineSep, RectI **coords_out, bool cacheRun)
{
    if (!page)
        return NULL;

    fz_text_sheet *sheet = NULL;
    fz_text_page *text = NULL;
    fz_device *dev = NULL;
    fz_var(sheet);
    fz_var(text);

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        sheet = fz_new_text_sheet(ctx);
        text = fz_new_text_page(ctx, xps_bound_page(_doc, page));
        dev = fz_new_text_device(ctx, sheet, text);
    }
    fz_catch(ctx) {
        fz_free_text_page(ctx, text);
        fz_free_text_sheet(ctx, sheet);
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }
    LeaveCriticalSection(&ctxAccess);

    // use an infinite rectangle as bounds (instead of a mediabox) to ensure that
    // the extracted text is consistent between cached runs using a list device and
    // fresh runs (otherwise the list device omits text outside the mediabox bounds)
    RunPage(page, dev, fz_identity, fz_infinite_bbox, cacheRun);

    ScopedCritSec scope(&ctxAccess);

    WCHAR *content = fz_text_page_to_str(text, lineSep, coords_out);
    fz_free_text_page(ctx, text);
    fz_free_text_sheet(ctx, sheet);

    return content;
}

unsigned char *XpsEngineImpl::GetFileData(size_t *cbCount)
{
    unsigned char *data = NULL;
    ScopedCritSec scope(&ctxAccess);
    fz_try(ctx) {
        data = fz_extract_stream_data(_doc->file, cbCount);
    }
    fz_catch(ctx) {
        return _fileName ? (unsigned char *)file::ReadAll(_fileName, cbCount) : NULL;
    }
    return data;
}

bool XpsEngineImpl::SaveFileAs(const WCHAR *copyFileName)
{
    size_t dataLen;
    ScopedMem<unsigned char> data(GetFileData(&dataLen));
    if (data) {
        bool ok = file::WriteAll(copyFileName, data.Get(), dataLen);
        if (ok)
            return true;
    }
    if (!_fileName)
        return false;
    return CopyFile(_fileName, copyFileName, FALSE);
}

WCHAR *XpsEngineImpl::ExtractFontList()
{
    // load and parse all pages
    for (int i = 1; i <= PageCount(); i++)
        GetXpsPage(i);

    ScopedCritSec scope(&ctxAccess);

    // collect a list of all included fonts
    WStrVec fonts;
    for (xps_font_cache *font = _doc->font_table; font; font = font->next) {
        ScopedMem<WCHAR> path(str::conv::FromUtf8(font->name));
        ScopedMem<WCHAR> name(str::conv::FromUtf8(font->font->name));
        fonts.Append(str::Format(L"%s (%s)", name, path::GetBaseName(path)));
    }
    if (fonts.Count() == 0)
        return NULL;

    fonts.SortNatural();
    return fonts.Join(L"\n");
}

WCHAR *XpsEngineImpl::GetProperty(DocumentProperty prop)
{
    if (Prop_FontList == prop)
        return ExtractFontList();
    if (!_info)
        return NULL;

    char *value = NULL;
    switch (prop) {
    case Prop_Title: value = _info->title; break;
    case Prop_Author: value = _info->author; break;
    case Prop_Subject: value = _info->subject; break;
    case Prop_CreationDate: value = _info->creation_date; break;
    case Prop_ModificationDate: value = _info->modification_date; break;
    }
    return value ? str::conv::FromUtf8(value) : NULL;
};

bool XpsEngineImpl::SupportsAnnotation(PageAnnotType type, bool forSaving) const
{
    if (forSaving)
        return false; // for now
    return Annot_Highlight == type;
}

void XpsEngineImpl::UpdateUserAnnotations(Vec<PageAnnotation> *list)
{
    // TODO: use a new critical section to avoid blocking the UI thread
    ScopedCritSec scope(&ctxAccess);
    if (list)
        userAnnots = *list;
    else
        userAnnots.Reset();
}

PageElement *XpsEngineImpl::GetElementAtPos(int pageNo, PointD pt)
{
    xps_page *page = GetXpsPage(pageNo, true);
    if (!page)
        return NULL;

    fz_point p = { (float)pt.x, (float)pt.y };
    for (fz_link *link = page->links; link; link = link->next)
        if (fz_is_pt_in_rect(link->rect, p))
            return new XpsLink(this, &link->dest, link->rect, pageNo);

    if (imageRects[pageNo-1]) {
        for (int i = 0; !fz_is_empty_rect(imageRects[pageNo-1][i]); i++)
            if (fz_is_pt_in_rect(imageRects[pageNo-1][i], p))
                return new XpsImage(this, pageNo, imageRects[pageNo-1][i], i);
    }

    return NULL;
}

Vec<PageElement *> *XpsEngineImpl::GetElements(int pageNo)
{
    xps_page *page = GetXpsPage(pageNo, true);
    if (!page)
        return NULL;

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    Vec<PageElement *> *els = new Vec<PageElement *>();
    if (!els)
        return NULL;

    if (imageRects[pageNo-1]) {
        for (int i = 0; !fz_is_empty_rect(imageRects[pageNo-1][i]); i++) {
            els->Append(new XpsImage(this, pageNo, imageRects[pageNo-1][i], i));
        }
    }

    for (fz_link *link = page->links; link; link = link->next) {
        els->Append(new XpsLink(this, &link->dest, link->rect, pageNo));
    }

    els->Reverse();
    return els;
}

void XpsEngineImpl::LinkifyPageText(xps_page *page, int pageNo)
{
    // make MuXPS extract all links and named destinations from the page
    assert(!GetPageRun(page, true));
    XpsPageRun *run = GetPageRun(page);
    assert(!run == !page->links_resolved);
    if (run)
        DropPageRun(run);
    else
        page->links_resolved = 1;
    assert(!page->links || page->links->refs == 1);

    RectI *coords;
    WCHAR *pageText = ExtractPageText(page, L"\n", &coords, true);
    if (!pageText)
        return;

    LinkRectList *list = LinkifyText(pageText, coords);
    for (size_t i = 0; i < list->links.Count(); i++) {
        bool overlaps = false;
        for (fz_link *next = page->links; next && !overlaps; next = next->next)
            overlaps = fz_calc_overlap(list->coords.At(i), next->rect) >= 0.25f;
        if (!overlaps) {
            ScopedMem<char> uri(str::conv::ToUtf8(list->links.At(i)));
            if (!uri) continue;
            fz_link_dest ld = { FZ_LINK_URI, 0 };
            ld.ld.uri.uri = fz_strdup(ctx, uri);
            // add links in top-to-bottom order (i.e. last-to-first)
            fz_link *link = fz_new_link(ctx, list->coords.At(i), ld);
            link->next = page->links;
            page->links = link;
        }
    }

    delete list;
    delete[] coords;
    free(pageText);
}

RenderedBitmap *XpsEngineImpl::GetPageImage(int pageNo, RectD rect, size_t imageIx)
{
    xps_page *page = GetXpsPage(pageNo);
    if (!page)
        return NULL;

    Vec<FitzImagePos> positions;
    ListInspectionData data(positions);
    fz_device *dev = NULL;

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_inspection_device(ctx, &data);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return NULL;
    }
    LeaveCriticalSection(&ctxAccess);

    RunPage(page, dev, fz_identity);

    if (imageIx >= positions.Count() || fz_rect_to_RectD(positions.At(imageIx).rect) != rect) {
        assert(0);
        return NULL;
    }

    ScopedCritSec scope(&ctxAccess);

    fz_pixmap *pixmap = NULL;
    fz_try(ctx) {
        fz_image *image = positions.At(imageIx).image;
        pixmap = fz_image_to_pixmap(ctx, image, image->w, image->h);
    }
    fz_catch(ctx) {
        return NULL;
    }
    RenderedFitzBitmap *bmp = new RenderedFitzBitmap(ctx, pixmap);
    fz_drop_pixmap(ctx, pixmap);

    return bmp;
}

fz_rect XpsEngineImpl::FindDestRect(const char *target)
{
    if (str::IsEmpty(target))
        return fz_empty_rect;

    xps_target *found = xps_lookup_link_target_obj(_doc, (char *)target);
    if (!found)
        return fz_empty_rect;
    if (fz_is_empty_rect(found->rect)) {
        // ensure that the target rectangle could have been
        // updated through LinkifyPageText -> xps_extract_anchor_info
        GetXpsPage(found->page + 1);
    }
    return found->rect;
}

PageDestination *XpsEngineImpl::GetNamedDest(const WCHAR *name)
{
    ScopedMem<char> name_utf8(str::conv::ToUtf8(name));
    if (!str::StartsWith(name_utf8.Get(), "#"))
        name_utf8.Set(str::Join("#", name_utf8));

    for (xps_target *dest = _doc->target; dest; dest = dest->next)
        if (str::EndsWithI(dest->name, name_utf8))
            return new SimpleDest(dest->page + 1, fz_rect_to_RectD(dest->rect));

    return NULL;
}

XpsTocItem *XpsEngineImpl::BuildTocTree(fz_outline *entry, int& idCounter)
{
    XpsTocItem *node = NULL;

    for (; entry; entry = entry->next) {
        WCHAR *name = entry->title ? str::conv::FromUtf8(entry->title) : str::Dup(L"");
        XpsTocItem *item = new XpsTocItem(name, XpsLink(this, &entry->dest));
        item->id = ++idCounter;
        item->open = entry->is_open;

        if (FZ_LINK_GOTO == entry->dest.kind)
            item->pageNo = entry->dest.ld.gotor.page + 1;
        if (entry->down)
            item->child = BuildTocTree(entry->down, idCounter);

        if (!node)
            node = item;
        else
            node->AddSibling(item);
    }

    return node;
}

DocTocItem *XpsEngineImpl::GetTocTree()
{
    if (!HasTocTree())
        return NULL;

    int idCounter = 0;
    return BuildTocTree(_outline, idCounter);
}

bool XpsEngineImpl::HasClipOptimizations(int pageNo)
{
    xps_page *page = GetXpsPage(pageNo, true);
    // GetXpsPage extracts imageRects for us
    if (!page || !imageRects[pageNo-1])
        return true;

    fz_rect mbox = fz_RectD_to_rect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (int i = 0; !fz_is_empty_rect(imageRects[pageNo-1][i]); i++)
        if (fz_calc_overlap(mbox, imageRects[pageNo-1][i]) >= 0.9f)
            return false;
    return true;
}

bool XpsEngine::IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        ZipFile zip(fileName);
        return zip.GetFileIndex(L"_rels/.rels") != (size_t)-1 ||
               zip.GetFileIndex(L"_rels/.rels/[0].piece") != (size_t)-1 ||
               zip.GetFileIndex(L"_rels/.rels/[0].last.piece") != (size_t)-1;
    }

    return str::EndsWithI(fileName, L".xps");
}

XpsEngine *XpsEngine::CreateFromFile(const WCHAR *fileName)
{
    XpsEngineImpl *engine = new XpsEngineImpl();
    if (!engine || !fileName || !engine->Load(fileName)) {
        delete engine;
        return NULL;
    }
    return engine;
}

XpsEngine *XpsEngine::CreateFromStream(IStream *stream)
{
    XpsEngineImpl *engine = new XpsEngineImpl();
    if (!engine->Load(stream)) {
        delete engine;
        return NULL;
    }
    return engine;
}
