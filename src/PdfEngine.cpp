/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#pragma warning(disable: 4611) // interaction between '_setjmp' and C++ object destruction is non-portable

extern "C" {
#include <mupdf/fitz.h>
}

// utils
#include "BaseUtil.h"
#include "ArchUtil.h"
#include "FileUtil.h"
#include "HtmlParserLookup.h"
#include "HtmlPullParser.h"
#include "TrivialHtmlParser.h"
#include "WinUtil.h"
#include "ZipUtil.h"
// rendering engines
#include "BaseEngine.h"
#include "PdfEngine.h"

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

///// extensions to Fitz that are usable for both PDF and XPS /////

inline RectD fz_rect_to_RectD(fz_rect rect)
{
    return RectD::FromXY(rect.x0, rect.y0, rect.x1, rect.y1);
}

inline fz_rect fz_RectD_to_rect(RectD rect)
{
    fz_rect result = { (float)rect.x, (float)rect.y, (float)(rect.x + rect.dx), (float)(rect.y + rect.dy) };
    return result;
}

inline bool fz_is_pt_in_rect(fz_rect rect, fz_point pt)
{
    return fz_rect_to_RectD(rect).Contains(PointD(pt.x, pt.y));
}

inline float fz_calc_overlap(fz_rect r1, fz_rect r2)
{
    if (fz_is_empty_rect(&r1))
        return 0.0f;
    fz_rect isect = r1;
    fz_intersect_rect(&isect, &r2);
    return (isect.x1 - isect.x0) * (isect.y1 - isect.y0) / ((r1.x1 - r1.x0) * (r1.y1 - r1.y0));
}

static RenderedBitmap *new_rendered_fz_pixmap(fz_context *ctx, fz_pixmap *pixmap)
{
    int paletteSize = 0;
    bool hasPalette = false;

    int w = pixmap->w;
    int h = pixmap->h;
    int rows8 = ((w + 3) / 4) * 4;

    ScopedMem<BITMAPINFO> bmi((BITMAPINFO *)calloc(1, sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD)));

    // always try to produce an 8-bit palette for saving some memory
    unsigned char *bmpData = (unsigned char *)calloc(rows8, h);
    if (!bmpData)
        return nullptr;
    fz_pixmap *bgrPixmap = nullptr;
    if (bmpData && pixmap->n == 4 &&
        pixmap->colorspace == fz_device_rgb(ctx)) {
        unsigned char *dest = bmpData;
        unsigned char *source = pixmap->samples;
        uint32_t *palette = (uint32_t *)bmi.Get()->bmiColors;
        BYTE grayIdxs[256] = { 0 };

        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                RGBQUAD c;

                c.rgbRed = *source++;
                c.rgbGreen = *source++;
                c.rgbBlue = *source++;
                c.rgbReserved = 0;
                source++;

                /* find this color in the palette */
                int k;
                bool isGray = c.rgbRed == c.rgbGreen && c.rgbRed == c.rgbBlue;
                if (isGray) {
                    k = grayIdxs[c.rgbRed] || palette[0] == *(uint32_t *)&c ? grayIdxs[c.rgbRed] : paletteSize;
                }
                else {
                    for (k = 0; k < paletteSize && palette[k] != *(uint32_t *)&c; k++);
                }
                /* add it to the palette if it isn't in there and if there's still space left */
                if (k == paletteSize) {
                    if (++paletteSize > 256)
                        goto ProducingPaletteDone;
                    if (isGray)
                        grayIdxs[c.rgbRed] = (BYTE)k;
                    palette[k] = *(uint32_t *)&c;
                }
                /* 8-bit data consists of indices into the color palette */
                *dest++ = k;
            }
            dest += rows8 - w;
        }
ProducingPaletteDone:
        hasPalette = paletteSize <= 256;
    }
    if (!hasPalette) {
        free(bmpData);
        /* BGRA is a GDI compatible format */
        fz_try(ctx) {
            fz_irect bbox;
            fz_colorspace *colorspace = fz_device_bgr(ctx);
            bgrPixmap = fz_new_pixmap_with_bbox(ctx, colorspace, fz_pixmap_bbox(ctx, pixmap, &bbox));
            fz_convert_pixmap(ctx, bgrPixmap, pixmap);
        }
        fz_catch(ctx) {
            return nullptr;
        }
    }
    AssertCrash(hasPalette || bgrPixmap);

    BITMAPINFOHEADER *bmih = &bmi.Get()->bmiHeader;
    bmih->biSize = sizeof(*bmih);
    bmih->biWidth = w;
    bmih->biHeight = -h;
    bmih->biPlanes = 1;
    bmih->biCompression = BI_RGB;
    bmih->biBitCount = hasPalette ? 8 : 32;
    bmih->biSizeImage = h * (hasPalette ? rows8 : w * 4);
    bmih->biClrUsed = hasPalette ? paletteSize : 0;

    void *data = nullptr;
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bmih->biSizeImage, nullptr);
    HBITMAP hbmp = CreateDIBSection(nullptr, bmi, DIB_RGB_COLORS, &data, hMap, 0);
    if (hbmp)
        memcpy(data, hasPalette ? bmpData : bgrPixmap->samples, bmih->biSizeImage);

    if (hasPalette)
        free(bmpData);
    else
        fz_drop_pixmap(ctx, bgrPixmap);

    // return a RenderedBitmap even if hbmp is nullptr so that callers can
    // distinguish rendering errors from GDI resource exhaustion
    // (and in the latter case retry using smaller target rectangles)
    return new RenderedBitmap(hbmp, SizeI(w, h), hMap);
}

fz_stream *fz_open_file2(fz_context *ctx, const WCHAR *filePath)
{
    fz_stream *file = nullptr;
    int64 fileSize = file::GetSize(filePath);
    // load small files entirely into memory so that they can be
    // overwritten even by programs that don't open files with FILE_SHARE_READ
    if (fileSize > 0 && fileSize < MAX_MEMORY_FILE_SIZE) {
        fz_buffer *data = nullptr;
        fz_var(data);
        fz_try(ctx) {
            data = fz_new_buffer(ctx, (int)fileSize);
            data->len = (int)fileSize;
            if (file::ReadN(filePath, (char *)data->data, data->len))
                file = fz_open_buffer(ctx, data);
        }
        fz_catch(ctx) {
            file = nullptr;
        }
        fz_drop_buffer(ctx, data);
        if (file)
            return file;
    }

    fz_try(ctx) {
        file = fz_open_file_w(ctx, filePath);
    }
    fz_catch(ctx) {
        file = nullptr;
    }
    return file;
}

unsigned char *fz_extract_stream_data(fz_stream *stream, size_t *cbCount)
{
    fz_seek(stream, 0, 2);
    int fileLen = fz_tell(stream);
    fz_seek(stream, 0, 0);

    fz_buffer *buffer = fz_read_all(stream, fileLen);
    AssertCrash(fileLen == buffer->len);

    unsigned char *data = (unsigned char *)memdup(buffer->data, buffer->len);
    if (cbCount)
        *cbCount = buffer->len;

    fz_drop_buffer(stream->ctx, buffer);

    if (!data)
        fz_throw(stream->ctx, FZ_ERROR_GENERIC, "OOM in fz_extract_stream_data");
    return data;
}

void fz_stream_fingerprint(fz_stream *file, unsigned char digest[16])
{
    int fileLen = -1;
    fz_buffer *buffer = nullptr;

    fz_try(file->ctx) {
        fz_seek(file, 0, 2);
        fileLen = fz_tell(file);
        fz_seek(file, 0, 0);
        buffer = fz_read_all(file, fileLen);
    }
    fz_catch(file->ctx) {
        fz_warn(file->ctx, "couldn't read stream data, using a nullptr fingerprint instead");
        ZeroMemory(digest, 16);
        return;
    }
    CrashIf(nullptr == buffer);
    CrashIf(fileLen != buffer->len);

    fz_md5 md5;
    fz_md5_init(&md5);
    fz_md5_update(&md5, buffer->data, buffer->len);
    fz_md5_final(&md5, digest);

    fz_drop_buffer(file->ctx, buffer);
}

static inline int wchars_per_rune(int rune)
{
    if (rune & 0x1F0000)
        return 2;
    return 1;
}

static void AddChar(fz_text_span *span, fz_text_char *c, str::Str<WCHAR>& s, Vec<RectI>& rects) {
    fz_rect bbox;
    fz_text_char_bbox(&bbox, span, c - span->text);
    RectI r = fz_rect_to_RectD(bbox).Round();

    int n = wchars_per_rune(c->c);
    if (n == 2) {
        WCHAR tmp[2];
        tmp[0] = 0xD800 | ((c->c - 0x10000) >> 10) & 0x3FF;
        tmp[1] = 0xDC00 | (c->c - 0x10000) & 0x3FF;
        s.Append(tmp, 2);
        rects.Append(r);
        rects.Append(r);
        return;
    }
    WCHAR wc = c->c;
    bool isNonPrintable = (wc <= 32) || str::IsNonCharacter(wc);
    if (!isNonPrintable) {
        s.Append(wc);
        rects.Append(r);
        return;
    }

    // non-printable or whitespace
    if (!str::IsWs(wc)) {
        s.Append(L'?');
        rects.Append(r);
        return;
    }

    // collapse multiple whitespace characters into one
    WCHAR prev = s.LastChar();
    if (!str::IsWs(prev)) {
        s.Append(L' ');
        rects.Append(r);
    }
}

// if there's a span following this one, add space to separate them
static void AddSpaceAtSpanEnd(fz_text_span *span, str::Str<WCHAR>& s, Vec<RectI>& rects) {
    if (span->len == 0 || span->next == NULL) {
        return;
    }
    CrashIf(s.Count() == 0);
    CrashIf(rects.Count() == 0);
    if (s.LastChar() == ' ') {
        return;
    }
    // TODO: use a Tab instead? (this might be a table)
    s.Append(L' ');
    RectI prev = rects.Last();
    prev.x += prev.dx;
    prev.dx /= 2;
    rects.Append(prev);
}

static void AddLineSep(str::Str<WCHAR>& s, Vec<RectI>& rects, const WCHAR *lineSep, size_t lineSepLen) {
    if (lineSepLen == 0) {
        return;
    }
    // remove trailing spaces
    if (str::IsWs(s.LastChar())) {
        s.Pop();
        rects.Pop();
    }

    s.Append(lineSep);
    for (size_t i = 0; i < lineSepLen; i++) {
        rects.Append(RectI());
    }
}


static WCHAR *fz_text_page_to_str(fz_text_page *text, const WCHAR *lineSep, RectI **coordsOut)
{
    size_t lineSepLen = str::Len(lineSep);
    str::Str<WCHAR> content;
    // coordsOut is optional but we ask for it by default so we simplify the code
    // by always calculating it
    Vec<RectI> rects;

    for (fz_page_block *block = text->blocks; block < text->blocks + text->len; block++) {
        if (block->type != FZ_PAGE_BLOCK_TEXT)
            continue;
        for (fz_text_line *line = block->u.text->lines; line < block->u.text->lines + block->u.text->len; line++) {
            for (fz_text_span *span = line->first_span; span; span = span->next) {
                for (fz_text_char *c = span->text; c < span->text + span->len; c++) {
                    AddChar(span, c, content, rects);
                }
                AddSpaceAtSpanEnd(span, content, rects);
            }
            AddLineSep(content, rects, lineSep, lineSepLen);
        }
    }

    CrashIf(content.Count() != rects.Count());

    if (coordsOut) {
        *coordsOut = rects.StealData();
    }

    return content.StealData();
}

struct istream_filter {
    IStream *stream;
    unsigned char buf[4096];
};

extern "C" static int next_istream(fz_stream *stm, int max)
{
    UNUSED(max);
    istream_filter *state = (istream_filter *)stm->state;
    ULONG cbRead = sizeof(state->buf);
    HRESULT res = state->stream->Read(state->buf, sizeof(state->buf), &cbRead);
    if (FAILED(res))
        fz_throw(stm->ctx, FZ_ERROR_GENERIC, "IStream read error: %x", res);
    stm->rp = state->buf;
    stm->wp = stm->rp + cbRead;
    stm->pos += cbRead;

    return cbRead > 0 ? *stm->rp++ : EOF;
}

extern "C" static void seek_istream(fz_stream *stm, int offset, int whence)
{
    istream_filter *state = (istream_filter *)stm->state;
    LARGE_INTEGER off;
    ULARGE_INTEGER n;
    off.QuadPart = offset;
    HRESULT res = state->stream->Seek(off, whence, &n);
    if (FAILED(res))
        fz_throw(stm->ctx, FZ_ERROR_GENERIC, "IStream seek error: %x", res);
    if (n.HighPart != 0 || n.LowPart > INT_MAX)
        fz_throw(stm->ctx, FZ_ERROR_GENERIC, "documents beyond 2GB aren't supported");
    stm->pos = n.LowPart;
    stm->rp = stm->wp = state->buf;
}

extern "C" static void close_istream(fz_context *ctx, void *state_)
{
    istream_filter *state = (istream_filter *)state_;
    state->stream->Release();
    fz_free(ctx, state);
}

fz_stream *fz_open_istream(fz_context *ctx, IStream *stream);

extern "C" static fz_stream *reopen_istream(fz_context *ctx, fz_stream *stm)
{
    istream_filter *state = (istream_filter *)stm->state;
    ScopedComPtr<IStream> stream2;
    HRESULT res = state->stream->Clone(&stream2);
    if (E_NOTIMPL == res)
        fz_throw(ctx, FZ_ERROR_GENERIC, "IStream doesn't support cloning");
    if (FAILED(res))
        fz_throw(ctx, FZ_ERROR_GENERIC, "IStream clone error: %x", res);
    return fz_open_istream(ctx, stream2);
}

fz_stream *fz_open_istream(fz_context *ctx, IStream *stream)
{
    if (!stream)
        return nullptr;

    LARGE_INTEGER zero = { 0 };
    HRESULT res = stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    if (FAILED(res))
        fz_throw(ctx, FZ_ERROR_GENERIC, "IStream seek error: %x", res);

    istream_filter *state = fz_malloc_struct(ctx, istream_filter);
    state->stream = stream;
    stream->AddRef();

    fz_stream *stm = fz_new_stream(ctx, state, next_istream, close_istream, nullptr);
    stm->seek = seek_istream;
    stm->reopen = reopen_istream;
    return stm;
}

fz_matrix fz_create_view_ctm(const fz_rect *mediabox, float zoom, int rotation)
{
    fz_matrix ctm;
    fz_pre_scale(fz_rotate(&ctm, (float)rotation), zoom, zoom);

    AssertCrash(0 == mediabox->x0 && 0 == mediabox->y0);
    rotation = (rotation + 360) % 360;
    if (90 == rotation)
        fz_pre_translate(&ctm, 0, -mediabox->y1);
    else if (180 == rotation)
        fz_pre_translate(&ctm, -mediabox->x1, -mediabox->y1);
    else if (270 == rotation)
        fz_pre_translate(&ctm, -mediabox->x1, 0);

    AssertCrash(fz_matrix_expansion(&ctm) > 0);
    if (fz_matrix_expansion(&ctm) == 0)
        return fz_identity;

    return ctm;
}

struct LinkRectList {
    WStrVec links;
    Vec<fz_rect> coords;
};

static bool LinkifyCheckMultiline(const WCHAR *pageText, const WCHAR *pos, RectI *coords)
{
    // multiline links end in a non-alphanumeric character and continue on a line
    // that starts left and only slightly below where the current line ended
    // (and that doesn't start with http or a footnote numeral)
    return
        '\n' == *pos && pos > pageText && *(pos + 1) &&
        !iswalnum(pos[-1]) && !str::IsWs(pos[1]) &&
        coords[pos - pageText + 1].BR().y > coords[pos - pageText - 1].y &&
        coords[pos - pageText + 1].y <= coords[pos - pageText - 1].BR().y + coords[pos - pageText - 1].dy * 0.35 &&
        coords[pos - pageText + 1].x < coords[pos - pageText - 1].BR().x &&
        coords[pos - pageText + 1].dy >= coords[pos - pageText - 1].dy * 0.85 &&
        coords[pos - pageText + 1].dy <= coords[pos - pageText - 1].dy * 1.2 &&
        !str::StartsWith(pos + 1, L"http");
}

static const WCHAR *LinkifyFindEnd(const WCHAR *start, WCHAR prevChar)
{
    const WCHAR *end, *quote;

    // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
    for (end = start; *end && !str::IsWs(*end); end++);
    if (',' == end[-1] || '.' == end[-1] || '?' == end[-1] || '!' == end[-1])
        end--;
    // also ignore a closing parenthesis, if the URL doesn't contain any opening one
    if (')' == end[-1] && (!str::FindChar(start, '(') || str::FindChar(start, '(') >= end))
        end--;
    // cut the link at the first quotation mark, if it's also preceded by one
    if (('"' == prevChar || '\'' == prevChar) && (quote = str::FindChar(start, prevChar)) != nullptr && quote < end)
        end = quote;

    return end;
}

static const WCHAR *LinkifyMultilineText(LinkRectList *list, const WCHAR *pageText, const WCHAR *start, const WCHAR *next, RectI *coords)
{
    size_t lastIx = list->coords.Count() - 1;
    AutoFreeW uri(list->links.at(lastIx));
    const WCHAR *end = next;
    bool multiline = false;

    do {
        end = LinkifyFindEnd(next, start > pageText ? start[-1] : ' ');
        multiline = LinkifyCheckMultiline(pageText, end, coords);

        AutoFreeW part(str::DupN(next, end - next));
        uri.Set(str::Join(uri, part));
        RectI bbox = coords[next - pageText].Union(coords[end - pageText - 1]);
        list->coords.Append(fz_RectD_to_rect(bbox.Convert<double>()));

        next = end + 1;
    } while (multiline);

    // update the link URL for all partial links
    list->links.at(lastIx) = str::Dup(uri);
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

static const WCHAR *LinkifyFindEmail(const WCHAR *pageText, const WCHAR *at)
{
    const WCHAR *start;
    for (start = at; start > pageText && IsEmailUsernameChar(*(start - 1)); start--);
    return start != at ? start : nullptr;
}

static const WCHAR *LinkifyEmailAddress(const WCHAR *start)
{
    const WCHAR *end;
    for (end = start; IsEmailUsernameChar(*end); end++);
    if (end == start || *end != '@' || !IsEmailDomainChar(*(end + 1)))
        return nullptr;
    for (end++; IsEmailDomainChar(*end); end++);
    if ('.' != *end || !IsEmailDomainChar(*(end + 1)))
        return nullptr;
    do {
        for (end++; IsEmailDomainChar(*end); end++);
    } while ('.' == *end && IsEmailDomainChar(*(end + 1)));
    return end;
}

// caller needs to delete the result
static LinkRectList *LinkifyText(const WCHAR *pageText, RectI *coords)
{
    LinkRectList *list = new LinkRectList;

    for (const WCHAR *start = pageText; *start; start++) {
        const WCHAR *end = nullptr;
        bool multiline = false;
        const WCHAR *protocol = nullptr;

        if ('@' == *start) {
            // potential email address without mailto:
            const WCHAR *email = LinkifyFindEmail(pageText, start);
            end = email ? LinkifyEmailAddress(email) : nullptr;
            protocol = L"mailto:";
            if (end != nullptr)
                start = email;
        }
        else if (start > pageText && ('/' == start[-1] || iswalnum(start[-1]))) {
            // hyperlinks must not be preceded by a slash (indicates a different protocol)
            // or an alphanumeric character (indicates part of a different protocol)
        }
        else if ('h' == *start && str::Parse(start, L"http%?s://")) {
            end = LinkifyFindEnd(start, start > pageText ? start[-1] : ' ');
            multiline = LinkifyCheckMultiline(pageText, end, coords);
        }
        else if ('w' == *start && str::StartsWith(start, L"www.")) {
            end = LinkifyFindEnd(start, start > pageText ? start[-1] : ' ');
            multiline = LinkifyCheckMultiline(pageText, end, coords);
            protocol = L"http://";
            // ignore www. links without a top-level domain
            if (end - start <= 4 || !multiline && (!wcschr(start + 5, '.') || wcschr(start + 5, '.') >= end))
                end = nullptr;
        }
        else if ('m' == *start && str::StartsWith(start, L"mailto:")) {
            end = LinkifyEmailAddress(start + 7);
        }
        if (!end)
            continue;

        AutoFreeW part(str::DupN(start, end - start));
        WCHAR *uri = protocol ? str::Join(protocol, part) : part.StealData();
        list->links.Append(uri);
        RectI bbox = coords[start - pageText].Union(coords[end - pageText - 1]);
        list->coords.Append(fz_RectD_to_rect(bbox.Convert<double>()));
        if (multiline)
            end = LinkifyMultilineText(list, pageText, start, end + 1, coords);

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
    fz_link *new_root = nullptr;
    while (root) {
        fz_link *tmp = root->next;
        root->next = new_root;
        new_root = root;
        root = tmp;

        // there are PDFs that have x,y positions in reverse order, so fix them up
        fz_link *link = new_root;
        if (link->rect.x0 > link->rect.x1)
            std::swap(link->rect.x0, link->rect.x1);
        if (link->rect.y0 > link->rect.y1)
            std::swap(link->rect.y0, link->rect.y1);
        AssertCrash(link->rect.x1 >= link->rect.x0);
        AssertCrash(link->rect.y1 >= link->rect.y0);
    }
    return new_root;
}

class SimpleDest : public PageDestination {
    int pageNo;
    RectD rect;

public:
    SimpleDest(int pageNo, RectD rect) : pageNo(pageNo), rect(rect) { }

    PageDestType GetDestType() const override { return Dest_ScrollTo; }
    int GetDestPageNo() const override { return pageNo; }
    RectD GetDestRect() const override { return rect; }
};

struct FitzImagePos {
    fz_image *image;
    fz_rect rect;

    explicit FitzImagePos(fz_image *image=nullptr, fz_rect rect=fz_unit_rect) :
        image(image), rect(rect) { }
};

struct ListInspectionData {
    Vec<FitzImagePos> *images;
    size_t mem_estimate;

    explicit ListInspectionData(Vec<FitzImagePos>& images) : images(&images), mem_estimate(0) { }
};

extern "C" static void
fz_inspection_free(fz_device *dev)
{
    // images are extracted in bottom-to-top order, but for GetElements
    // we want to access them in top-to-bottom order (since images at
    // the bottom might not be visible at all)
    ((ListInspectionData *)dev->user)->images->Reverse();
}

static void fz_inspection_handle_path(fz_device *dev, fz_path *path)
{
    ((ListInspectionData *)dev->user)->mem_estimate += sizeof(fz_path) + path->cmd_cap + path->coord_cap * sizeof(float);
}

static void fz_inspection_handle_image(fz_device *dev, fz_image *image)
{
    int n = image->colorspace ? image->colorspace->n + 1 : 1;
    ((ListInspectionData *)dev->user)->mem_estimate += sizeof(fz_image) + image->w * image->h * n;
}

extern "C" static void
fz_inspection_fill_path(fz_device *dev, fz_path *path, int even_odd, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
    UNUSED(even_odd); UNUSED(ctm); UNUSED(colorspace); UNUSED(color); UNUSED(alpha);
    fz_inspection_handle_path(dev, path);
}

extern "C" static void
fz_inspection_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
    UNUSED(stroke); UNUSED(ctm); UNUSED(colorspace); UNUSED(color); UNUSED(alpha);
    fz_inspection_handle_path(dev, path);
}

extern "C" static void
fz_inspection_clip_path(fz_device *dev, fz_path *path, const fz_rect *rect, int even_odd, const fz_matrix *ctm)
{
    UNUSED(rect); UNUSED(even_odd); UNUSED(ctm);
    fz_inspection_handle_path(dev, path);
}

extern "C" static void
fz_inspection_clip_stroke_path(fz_device *dev, fz_path *path, const fz_rect *rect, fz_stroke_state *stroke, const fz_matrix *ctm)
{
    UNUSED(rect); UNUSED(stroke); UNUSED(ctm);
    fz_inspection_handle_path(dev, path);
}

extern "C" static void
fz_inspection_fill_shade(fz_device *dev, fz_shade *shade, const fz_matrix *ctm, float alpha)
{
    UNUSED(shade); UNUSED(ctm); UNUSED(alpha);
    ((ListInspectionData *)dev->user)->mem_estimate += sizeof(fz_shade);
}

extern "C" static void
fz_inspection_fill_image(fz_device *dev, fz_image *image, const fz_matrix *ctm, float alpha)
{
    UNUSED(alpha);
    fz_inspection_handle_image(dev, image);
    // extract rectangles for images a user might want to extract
    // TODO: try to better distinguish images a user might actually want to extract
    if (image->w < 16 || image->h < 16)
        return;
    fz_rect rect = fz_unit_rect;
    fz_transform_rect(&rect, ctm);
    if (!fz_is_empty_rect(&rect))
        ((ListInspectionData *)dev->user)->images->Append(FitzImagePos(image, rect));
}

extern "C" static void
fz_inspection_fill_image_mask(fz_device *dev, fz_image *image, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
    UNUSED(ctm); UNUSED(colorspace); UNUSED(color); UNUSED(alpha);
    fz_inspection_handle_image(dev, image);
}

extern "C" static void
fz_inspection_clip_image_mask(fz_device *dev, fz_image *image, const fz_rect *rect, const fz_matrix *ctm)
{
    UNUSED(rect); UNUSED(ctm);
    fz_inspection_handle_image(dev, image);
}

static fz_device *fz_new_inspection_device(fz_context *ctx, ListInspectionData *data)
{
    fz_device *dev = fz_new_device(ctx, data);
    dev->free_user = fz_inspection_free;

    dev->fill_path = fz_inspection_fill_path;
    dev->stroke_path = fz_inspection_stroke_path;
    dev->clip_path = fz_inspection_clip_path;
    dev->clip_stroke_path = fz_inspection_clip_stroke_path;

    dev->fill_shade = fz_inspection_fill_shade;
    dev->fill_image = fz_inspection_fill_image;
    dev->fill_image_mask = fz_inspection_fill_image_mask;
    dev->clip_image_mask = fz_inspection_clip_image_mask;

    return dev;
}

class FitzAbortCookie : public AbortCookie {
public:
    fz_cookie cookie;
    FitzAbortCookie() { memset(&cookie, 0, sizeof(cookie)); }
    void Abort() override { cookie.abort = 1; }
};

extern "C" static void
fz_lock_context_cs(void *user, int lock)
{
    UNUSED(lock);
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
    UNUSED(lock);
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)user;
    LeaveCriticalSection(cs);
}

static Vec<PageAnnotation> fz_get_user_page_annots(Vec<PageAnnotation>& userAnnots, int pageNo)
{
    Vec<PageAnnotation> result;
    for (size_t i = 0; i < userAnnots.Count(); i++) {
        PageAnnotation& annot = userAnnots.at(i);
        if (annot.pageNo != pageNo)
            continue;
        // include all annotations for pageNo that can be rendered by fz_run_user_annots
        switch (annot.type) {
        case Annot_Highlight: case Annot_Underline: case Annot_StrikeOut: case Annot_Squiggly:
            result.Append(annot);
            break;
        }
    }
    return result;
}

static void fz_run_user_page_annots(Vec<PageAnnotation>& pageAnnots, fz_device *dev, const fz_matrix *ctm, const fz_rect *cliprect, fz_cookie *cookie)
{
    for (size_t i = 0; i < pageAnnots.Count() && (!cookie || !cookie->abort); i++) {
        PageAnnotation& annot = pageAnnots.at(i);
        // skip annotation if it isn't visible
        fz_rect rect = fz_RectD_to_rect(annot.rect);
        fz_transform_rect(&rect, ctm);
        fz_rect isect = rect;
        if (cliprect && fz_is_empty_rect(fz_intersect_rect(&isect, cliprect)))
            continue;
        // prepare text highlighting path (cf. pdf_create_highlight_annot
        // and pdf_create_markup_annot in pdf_annot.c)
        fz_path *path = fz_new_path(dev->ctx);
        fz_stroke_state *stroke = nullptr;
        switch (annot.type) {
        case Annot_Highlight:
            fz_moveto(dev->ctx, path, annot.rect.TL().x, annot.rect.TL().y);
            fz_lineto(dev->ctx, path, annot.rect.BR().x, annot.rect.TL().y);
            fz_lineto(dev->ctx, path, annot.rect.BR().x, annot.rect.BR().y);
            fz_lineto(dev->ctx, path, annot.rect.TL().x, annot.rect.BR().y);
            fz_closepath(dev->ctx, path);
            break;
        case Annot_Underline:
            fz_moveto(dev->ctx, path, annot.rect.TL().x, annot.rect.BR().y - 0.25f);
            fz_lineto(dev->ctx, path, annot.rect.BR().x, annot.rect.BR().y - 0.25f);
            break;
        case Annot_StrikeOut:
            fz_moveto(dev->ctx, path, annot.rect.TL().x, annot.rect.TL().y + annot.rect.dy / 2);
            fz_lineto(dev->ctx, path, annot.rect.BR().x, annot.rect.TL().y + annot.rect.dy / 2);
            break;
        case Annot_Squiggly:
            fz_moveto(dev->ctx, path, annot.rect.TL().x + 1, annot.rect.BR().y);
            fz_lineto(dev->ctx, path, annot.rect.BR().x, annot.rect.BR().y);
            fz_moveto(dev->ctx, path, annot.rect.TL().x, annot.rect.BR().y - 0.5f);
            fz_lineto(dev->ctx, path, annot.rect.BR().x, annot.rect.BR().y - 0.5f);
            stroke = fz_new_stroke_state_with_dash_len(dev->ctx, 2);
            CrashIf(!stroke);
            stroke->linewidth = 0.5f;
            stroke->dash_list[stroke->dash_len++] = 1;
            stroke->dash_list[stroke->dash_len++] = 1;
            break;
        default:
            CrashIf(true);
        }
        fz_colorspace *cs = fz_device_rgb(dev->ctx);
        float color[3] = { annot.color.r / 255.f, annot.color.g / 255.f, annot.color.b / 255.f };
        if (Annot_Highlight == annot.type) {
            // render path with transparency effect
            fz_begin_group(dev, &rect, 0, 0, FZ_BLEND_MULTIPLY, 1.f);
            fz_fill_path(dev, path, 0, ctm, cs, color, annot.color.a / 255.f);
            fz_end_group(dev);
        }
        else {
            if (!stroke)
                stroke = fz_new_stroke_state(dev->ctx);
            fz_stroke_path(dev, path, stroke, ctm, cs, color, 1.0f);
            fz_drop_stroke_state(dev->ctx, stroke);
        }
        fz_free_path(dev->ctx, path);
    }
}

static void fz_run_page_transparency(Vec<PageAnnotation>& pageAnnots, fz_device *dev, const fz_rect *cliprect, bool endGroup, bool hasTransparency=false)
{
    if (hasTransparency || pageAnnots.Count() == 0)
        return;
    bool needsTransparency = false;
    for (size_t i = 0; i < pageAnnots.Count(); i++) {
        if (Annot_Highlight == pageAnnots.at(i).type) {
            needsTransparency = true;
            break;
        }
    }
    if (!needsTransparency)
        return;
    if (!endGroup)
        fz_begin_group(dev, cliprect ? cliprect : &fz_infinite_rect, 1, 0, 0, 1);
    else
        fz_end_group(dev);
}

///// PDF-specific extensions to Fitz/MuPDF /////

extern "C" {
#include <mupdf/pdf.h>
}

namespace str {
    namespace conv {

inline WCHAR *FromPdf(pdf_obj *obj)
{
    AutoFreeW str(AllocArray<WCHAR>(pdf_to_str_len(obj) + 1));
    pdf_to_ucs2_buf((unsigned short *)str.Get(), obj);
    return str.StealData();
}

    }
}

// some PDF documents contain control characters in outline titles or /Info properties
WCHAR *pdf_clean_string(WCHAR *string)
{
    for (WCHAR *c = string; *c; c++) {
        if (*c < 0x20 && *c != '\n' && *c != '\r' && *c != '\t')
            *c = ' ';
    }
    return string;
}

pdf_obj *pdf_copy_str_dict(pdf_document *doc, pdf_obj *dict)
{
    pdf_obj *copy = pdf_copy_dict(dict);
    for (int i = 0; i < pdf_dict_len(copy); i++) {
        pdf_obj *val = pdf_dict_get_val(copy, i);
        // resolve all indirect references
        if (pdf_is_indirect(val)) {
            pdf_obj *val2 = pdf_new_string(doc, pdf_to_str_buf(val), pdf_to_str_len(val));
            pdf_dict_put(copy, pdf_dict_get_key(copy, i), val2);
            pdf_drop_obj(val2);
        }
    }
    return copy;
}

// Note: make sure to only call with ctxAccess
fz_outline *pdf_loadattachments(pdf_document *doc)
{
    pdf_obj *dict = pdf_load_name_tree(doc, "EmbeddedFiles");
    if (!dict)
        return nullptr;

    fz_outline root = { 0 }, *node = &root;
    for (int i = 0; i < pdf_dict_len(dict); i++) {
        pdf_obj *name = pdf_dict_get_key(dict, i);
        pdf_obj *dest = pdf_dict_get_val(dict, i);
        pdf_obj *embedded = pdf_dict_getsa(pdf_dict_gets(dest, "EF"), "DOS", "F");
        if (!embedded)
            continue;

        node = node->next = (fz_outline *)fz_malloc_struct(doc->ctx, fz_outline);
        node->title = fz_strdup(doc->ctx, pdf_to_name(name));
        node->dest.kind = FZ_LINK_LAUNCH;
        node->dest.ld.launch.file_spec = pdf_file_spec_to_str(doc, dest);
        node->dest.ld.launch.new_window = 1;
        node->dest.ld.launch.embedded_num = pdf_to_num(embedded);
        node->dest.ld.launch.embedded_gen = pdf_to_gen(embedded);
        node->dest.ld.launch.is_uri = 0;
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
        AutoFreeW number(str::FormatRomanNumeral(pageNo));
        if (*type == 'r')
            str::ToLowerInPlace(number.Get());
        return str::Format(L"%s%s", prefix, number);
    }
    if (str::EqI(type, "A")) {
        // alphabetic numbering style (A..Z, AA..ZZ, AAA..ZZZ, ...)
        str::Str<WCHAR> number;
        number.Append('A' + (pageNo - 1) % 26);
        for (int i = 0; i < (pageNo - 1) / 26; i++)
            number.Append(number.at(0));
        if (*type == 'a')
            str::ToLowerInPlace(number.Get());
        return str::Format(L"%s%s", prefix, number.Get());
    }
    return str::Dup(prefix);
}

void BuildPageLabelRec(pdf_obj *node, int pageCount, Vec<PageLabelInfo>& data)
{
    pdf_obj *obj;
    if ((obj = pdf_dict_gets(node, "Kids")) != nullptr && !pdf_mark_obj(node)) {
        for (int i = 0; i < pdf_array_len(obj); i++)
            BuildPageLabelRec(pdf_array_get(obj, i), pageCount, data);
        pdf_unmark_obj(node);
    }
    else if ((obj = pdf_dict_gets(node, "Nums")) != nullptr) {
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
        return nullptr;

    if (data.Count() == 1 && data.at(0).startAt == 1 && data.at(0).countFrom == 1 &&
        !data.at(0).prefix && str::Eq(data.at(0).type, "D")) {
        // this is the default case, no need for special treatment
        return nullptr;
    }

    WStrVec *labels = new WStrVec();
    labels->AppendBlanks(pageCount);

    for (size_t i = 0; i < data.Count() && data.at(i).startAt <= pageCount; i++) {
        int secLen = pageCount + 1 - data.at(i).startAt;
        if (i < data.Count() - 1 && data.at(i + 1).startAt <= pageCount)
            secLen = data.at(i + 1).startAt - data.at(i).startAt;
        AutoFreeW prefix(str::conv::FromPdf(data.at(i).prefix));
        for (int j = 0; j < secLen; j++) {
            free(labels->at(data.at(i).startAt + j - 1));
            labels->at(data.at(i).startAt + j - 1) =
                FormatPageLabel(data.at(i).type, data.at(i).countFrom + j, prefix);
        }
    }

    for (int ix = 0; (ix = labels->Find(nullptr, ix)) != -1; ix++)
        labels->at(ix) = str::Dup(L"");

    // ensure that all page labels are unique (by appending a number to duplicates)
    WStrVec dups(*labels);
    dups.Sort();
    for (size_t i = 1; i < dups.Count(); i++) {
        if (!str::Eq(dups.at(i), dups.at(i - 1)))
            continue;
        int ix = labels->Find(dups.at(i)), counter = 0;
        while ((ix = labels->Find(dups.at(i), ix + 1)) != -1) {
            AutoFreeW unique;
            do {
                unique.Set(str::Format(L"%s.%d", dups.at(i), ++counter));
            } while (labels->Contains(unique));
            str::ReplacePtr(&labels->at(ix), unique);
        }
        for (; i + 1 < dups.Count() && str::Eq(dups.at(i), dups.at(i + 1)); i++);
    }

    return labels;
}

struct PageTreeStackItem {
    pdf_obj *kids;
    int i, len;
    int next_page_no;

    PageTreeStackItem() : kids(nullptr), i(-1), len(0), next_page_no(0) { }
    explicit PageTreeStackItem(pdf_obj *kids, int next_page_no=0) :
        kids(kids), i(-1), len(pdf_array_len(kids)), next_page_no(next_page_no) { }
};

static void
pdf_load_page_objs(pdf_document *doc, pdf_obj **page_objs)
{
    fz_context *ctx = doc->ctx;
    int page_no = 0;

    Vec<PageTreeStackItem> stack;
    PageTreeStackItem top(pdf_dict_getp(pdf_trailer(doc), "Root/Pages/Kids"));

    if (pdf_mark_obj(top.kids))
        fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in page tree");

    fz_try(ctx) {
        for (;;) {
            top.i++;
            if (top.i == top.len) {
                pdf_unmark_obj(top.kids);
                if (page_no < top.next_page_no)
                    page_no = top.next_page_no;
                if (stack.Size() == 0)
                    break;
                top = stack.Pop();
                continue;
            }

            pdf_obj *kid = pdf_array_get(top.kids, top.i);
            char *type = pdf_to_name(pdf_dict_gets(kid, "Type"));
            if (*type ? str::Eq(type, "Pages") : pdf_dict_gets(kid, "Kids") && !pdf_dict_gets(kid, "MediaBox")) {
                int count = pdf_to_int(pdf_dict_gets(kid, "Count"));
                if (count > 0) {
                    stack.Push(top);
                    top = PageTreeStackItem(pdf_dict_gets(kid, "Kids"), page_no + count);

                    if (pdf_mark_obj(top.kids))
                        fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in page tree");
                }
            }
            else {
                if (*type ? !str::Eq(type, "Page") : !pdf_dict_gets(kid, "MediaBox"))
                    fz_warn(ctx, "non-page object in page tree (%s)", type);
                if (page_no >= pdf_count_pages(doc))
                    fz_throw(ctx, FZ_ERROR_GENERIC, "found more /Page objects than anticipated");

                page_objs[page_no] = pdf_keep_obj(kid);
                page_no++;
            }
        }
    }
    fz_catch(ctx) {
        for (size_t i = 0; i < stack.Size(); i++) {
            pdf_unmark_obj(stack.at(i).kids);
        }
        pdf_unmark_obj(top.kids);
        fz_rethrow(ctx);
    }

    doc->page_objs = page_objs;
}

///// Above are extensions to Fitz and MuPDF, now follows PdfEngine /////

struct PdfPageRun {
    pdf_page *page;
    fz_display_list *list;
    size_t size_est;
    int refs;

    PdfPageRun(pdf_page *page, fz_display_list *list, ListInspectionData& data) :
        page(page), list(list), size_est(data.mem_estimate), refs(1) { }
};

class PdfTocItem;
class PdfLink;
class PdfImage;

class PdfEngineImpl : public BaseEngine {
    friend PdfLink;
    friend PdfImage;

public:
    PdfEngineImpl();
    virtual ~PdfEngineImpl();
    BaseEngine *Clone() override;

    int PageCount() const override {
        // make sure that _doc->page_count is initialized as soon as
        // _doc is defined, so that pdf_count_pages can't throw
        return _doc ? pdf_count_pages(_doc) : 0;
    }

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target=Target_View) override;

    RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=nullptr, /* if nullptr: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=nullptr) override;

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false) override;
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false) override;

    unsigned char *GetFileData(size_t *cbCount) override;
    bool SaveFileAs(const char *copyFileName, bool includeUserAnnots=false) override;
    virtual bool SaveFileAsPdf(const char *pdfFileName, bool includeUserAnnots=false) {
        return SaveFileAs(pdfFileName, includeUserAnnots);
    }
    WCHAR * ExtractPageText(int pageNo, const WCHAR *lineSep, RectI **coordsOut=nullptr,
                                    RenderTarget target=Target_View) override;
    bool HasClipOptimizations(int pageNo) override;
    PageLayoutType PreferredLayout() override;
    WCHAR *GetProperty(DocumentProperty prop) override;

    bool SupportsAnnotation(bool forSaving=false) const override;
    void UpdateUserAnnotations(Vec<PageAnnotation> *list) override;

    bool AllowsPrinting() const override {
        return pdf_has_permission(_doc, PDF_PERM_PRINT);
    }
    bool AllowsCopyingText() const override {
        return pdf_has_permission(_doc, PDF_PERM_COPY);
    }

    float GetFileDPI() const override { return 72.0f; }
    const WCHAR *GetDefaultFileExt() const override { return L".pdf"; }

    bool BenchLoadPage(int pageNo)  override { return GetPdfPage(pageNo) != nullptr; }

    Vec<PageElement *> *GetElements(int pageNo) override;
    PageElement *GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination *GetNamedDest(const WCHAR *name) override;
    bool HasTocTree() const override {
        return outline != nullptr || attachments != nullptr;
    }
    DocTocItem *GetTocTree() override;

    bool HasPageLabels() const override { return _pagelabels != nullptr; }
    WCHAR *GetPageLabel(int pageNo) const override;
    int GetPageByLabel(const WCHAR *label) const override;

    bool IsPasswordProtected() const override { return isProtected; }
    char *GetDecryptionKey() const override;

    static BaseEngine *CreateFromFile(const WCHAR *fileName, PasswordUI *pwdUI);
    static BaseEngine *CreateFromStream(IStream *stream, PasswordUI *pwdUI);

protected:
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
    pdf_obj **      _pageObjs;

    bool            Load(const WCHAR *fileName, PasswordUI *pwdUI=nullptr);
    bool            Load(IStream *stream, PasswordUI *pwdUI=nullptr);
    bool            Load(fz_stream *stm, PasswordUI *pwdUI=nullptr);
    bool            LoadFromStream(fz_stream *stm, PasswordUI *pwdUI=nullptr);
    bool            FinishLoading();

    pdf_page      * GetPdfPage(int pageNo, bool failIfBusy=false);
    int             GetPageNo(pdf_page *page);
    fz_matrix       viewctm(int pageNo, float zoom, int rotation) {
        const fz_rect tmpRc = fz_RectD_to_rect(PageMediabox(pageNo));
        return fz_create_view_ctm(&tmpRc, zoom, rotation);
    }
    fz_matrix       viewctm(pdf_page *page, float zoom, int rotation) {
        fz_rect r;
        return fz_create_view_ctm(pdf_bound_page(_doc, page, &r), zoom, rotation);
    }
    WCHAR         * ExtractPageText(pdf_page *page, const WCHAR *lineSep, RectI **coordsOut=nullptr,
                                    RenderTarget target=Target_View, bool cacheRun=false);

    Vec<PdfPageRun *> runCache; // ordered most recently used first
    PdfPageRun    * CreatePageRun(pdf_page *page, fz_display_list *list);
    PdfPageRun    * GetPageRun(pdf_page *page, bool tryOnly=false);
    bool            RunPage(pdf_page *page, fz_device *dev, const fz_matrix *ctm,
                            RenderTarget target=Target_View,
                            const fz_rect *cliprect=nullptr, bool cacheRun=true,
                            FitzAbortCookie *cookie=nullptr);
    void            DropPageRun(PdfPageRun *run, bool forceRemove=false);

    PdfTocItem    * BuildTocTree(fz_outline *entry, int& idCounter);
    void            LinkifyPageText(pdf_page *page);
    pdf_annot    ** ProcessPageAnnotations(pdf_page *page);
    RenderedBitmap *GetPageImage(int pageNo, RectD rect, size_t imageIx);
    WCHAR         * ExtractFontList();
    bool            IsLinearizedFile();

    bool            SaveEmbedded(LinkSaverUI& saveUI, int num, int gen);
    bool            SaveUserAnnots(const char *fileName);

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
        fz_rect rect=fz_empty_rect, int pageNo=-1, fz_point *pt=nullptr) :
        engine(engine), link(link), rect(fz_rect_to_RectD(rect)), pageNo(pageNo) {
        // cursor coordinates for IsMap URI links
        if (pt)
            this->pt = PointD(pt->x, pt->y);
    }

    // PageElement
    PageElementType GetType() const override { return Element_Link; }
    int GetPageNo() const override { return pageNo; }
    RectD GetRect() const override { return rect; }
    WCHAR *GetValue() const override;
    virtual PageDestination *AsLink() { return this; }

    // PageDestination
    PageDestType GetDestType() const override;
    int GetDestPageNo() const override;
    RectD GetDestRect() const override;
    WCHAR *GetDestValue() const override { return GetValue(); }
    WCHAR *GetDestName() const override;

    virtual bool SaveEmbedded(LinkSaverUI& saveUI);
};

class PdfComment : public PageElement {
    PageAnnotation annot;
    AutoFreeW content;

public:
    PdfComment(const WCHAR *content, RectD rect, int pageNo) :
        annot(Annot_None, pageNo, rect, PageAnnotation::Color()),
        content(str::Dup(content)) { }

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
    virtual WCHAR *GetValue() const { return nullptr; }

    virtual RenderedBitmap *GetImage() {
        return engine->GetPageImage(pageNo, rect, imageIx);
    }
};

PdfEngineImpl::PdfEngineImpl() : _doc(nullptr),
    _pages(nullptr), _pageObjs(nullptr), _mediaboxes(nullptr), _info(nullptr),
    outline(nullptr), attachments(nullptr), _pagelabels(nullptr),
    _decryptionKey(nullptr), isProtected(false),
    pageAnnots(nullptr), imageRects(nullptr)
{
    InitializeCriticalSection(&pagesAccess);
    InitializeCriticalSection(&ctxAccess);

    fz_locks_ctx.user = &ctxAccess;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, MAX_CONTEXT_MEMORY);

    if (ctx)
        pdf_install_load_system_font_funcs(ctx);
}

PdfEngineImpl::~PdfEngineImpl()
{
    EnterCriticalSection(&pagesAccess);
    EnterCriticalSection(&ctxAccess);

    if (_pages) {
        for (int i = 0; i < PageCount(); i++) {
            pdf_free_page(_doc, _pages[i]);
        }
        free(_pages);
    }
    if (_pageObjs) {
        for (int i = 0; i < PageCount(); i++) {
            pdf_drop_obj(_pageObjs[i]);
        }
        free(_pageObjs);
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

    while (runCache.Count() > 0) {
        AssertCrash(runCache.Last()->refs == 1);
        DropPageRun(runCache.Last(), true);
    }

    pdf_close_document(_doc);
    _doc = nullptr;
    fz_free_context(ctx);
    ctx = nullptr;

    free(_mediaboxes);
    delete _pagelabels;
    free(_decryptionKey);

    LeaveCriticalSection(&ctxAccess);
    DeleteCriticalSection(&ctxAccess);
    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

class PasswordCloner : public PasswordUI {
    unsigned char *cryptKey;

public:
    explicit PasswordCloner(unsigned char *cryptKey) : cryptKey(cryptKey) { }

    virtual WCHAR * GetPassword(const WCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) {
        UNUSED(fileName); UNUSED(fileDigest);
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return nullptr;
    }
};

BaseEngine *PdfEngineImpl::Clone()
{
    ScopedCritSec scope(&ctxAccess);

    // use this document's encryption key (if any) to load the clone
    PasswordCloner *pwdUI = nullptr;
    if (pdf_crypt_key(_doc))
        pwdUI = new PasswordCloner(pdf_crypt_key(_doc));

    PdfEngineImpl *clone = new PdfEngineImpl();
    bool ok = false;
    if (FileName()) {
        ok = clone->Load(FileName(), pwdUI);
    } else {
        ok = clone->Load(_doc->file, pwdUI);
    }
    if (!ok) {
        delete clone;
        delete pwdUI;
        return nullptr;
    }
    delete pwdUI;

    if (!_decryptionKey && _doc->crypt) {
        delete clone->_decryptionKey;
        clone->_decryptionKey = nullptr;
    }

    clone->UpdateUserAnnotations(&userAnnots);

    return clone;
}

static const WCHAR *findEmbedMarks(const WCHAR *fileName)
{
    const WCHAR *embedMarks = nullptr;

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
    AssertCrash(!FileName() && !_doc && ctx);
    SetFileName(fileName);
    if (!ctx)
        return false;

    fz_stream *file = nullptr;
    // File names ending in :<digits>:<digits> are interpreted as containing
    // embedded PDF documents (the digits are :<num>:<gen> of the embedded file stream)
    WCHAR *embedMarks = (WCHAR *)findEmbedMarks(fileName);
    if (embedMarks)
        *embedMarks = '\0';
    fz_try(ctx) {
        file = fz_open_file2(ctx, fileName);
    }
    fz_catch(ctx) {
        file = nullptr;
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
    CrashIf(!embedMarks);
    if (!embedMarks || !pdf_is_stream(_doc, num, gen))
        return false;

    fz_buffer *buffer = nullptr;
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
    _doc = nullptr;

    goto OpenEmbeddedFile;
}

bool PdfEngineImpl::Load(IStream *stream, PasswordUI *pwdUI)
{
    AssertCrash(!FileName() && !_doc && ctx);
    if (!ctx)
        return false;

    fz_stream *stm = nullptr;
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
    AssertCrash(!FileName() && !_doc && ctx);
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
    while (!ok) {
        AutoFreeW pwd(pwdUI->GetPassword(FileName(), digest, pdf_crypt_key(_doc), &saveKey));
        if (!pwd) {
            // password not given or encryption key has been remembered
            ok = saveKey;
            break;
        }

        // MuPDF expects passwords to be UTF-8 encoded
        AutoFree pwd_utf8(str::conv::ToUtf8(pwd));
        ok = pwd_utf8 && pdf_authenticate_password(_doc, pwd_utf8);
        // according to the spec (1.7 ExtensionLevel 3), the password
        // for crypt revisions 5 and above are in SASLprep normalization
        if (!ok) {
            // TODO: this is only part of SASLprep
            pwd.Set(NormalizeString(pwd, 5 /* NormalizationKC */));
            if (pwd) {
                pwd_utf8.Set(str::conv::ToUtf8(pwd));
                ok = pwd_utf8 && pdf_authenticate_password(_doc, pwd_utf8);
            }
        }
        // older Acrobat versions seem to have considered passwords to be in codepage 1252
        // note: such passwords aren't portable when stored as Unicode text
        if (!ok && GetACP() != 1252) {
            AutoFree pwd_ansi(str::conv::ToAnsi(pwd));
            AutoFreeW pwd_cp1252(str::conv::FromCodePage(pwd_ansi, 1252));
            pwd_utf8.Set(str::conv::ToUtf8(pwd_cp1252));
            ok = pwd_utf8 && pdf_authenticate_password(_doc, pwd_utf8);
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
        // this call might throw the first time
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
    _pageObjs = AllocArray<pdf_obj *>(PageCount());
    _mediaboxes = AllocArray<RectD>(PageCount());
    pageAnnots = AllocArray<pdf_annot **>(PageCount());
    imageRects = AllocArray<fz_rect *>(PageCount());

    if (!_pages || !_pageObjs || !_mediaboxes || !pageAnnots || !imageRects)
        return false;

    ScopedCritSec scope(&ctxAccess);

    fz_try(ctx) {
        pdf_load_page_objs(_doc, _pageObjs);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load all page objects");
    }
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
        _info = pdf_dict_gets(pdf_trailer(_doc), "Info");
        if (_info)
            _info = pdf_copy_str_dict(_doc, _info);
        if (!_info)
            _info = pdf_new_dict(_doc, 4);
        // also remember linearization and tagged states at this point
        if (IsLinearizedFile())
            pdf_dict_puts_drop(_info, "Linearized", pdf_new_bool(_doc, 1));
        if (pdf_to_bool(pdf_dict_getp(pdf_trailer(_doc), "Root/MarkInfo/Marked")))
            pdf_dict_puts_drop(_info, "Marked", pdf_new_bool(_doc, 1));
        // also remember known output intents (PDF/X, etc.)
        pdf_obj *intents = pdf_dict_getp(pdf_trailer(_doc), "Root/OutputIntents");
        if (pdf_is_array(intents)) {
            pdf_obj *list = pdf_new_array(_doc, pdf_array_len(intents));
            for (int i = 0; i < pdf_array_len(intents); i++) {
                pdf_obj *intent = pdf_dict_gets(pdf_array_get(intents, i), "S");
                if (pdf_is_name(intent) && !pdf_is_indirect(intent) && str::StartsWith(pdf_to_name(intent), "GTS_PDF"))
                    pdf_array_push(list, intent);
            }
            pdf_dict_puts_drop(_info, "OutputIntents", list);
        }
        // also note common unsupported features (such as XFA forms)
        pdf_obj *xfa = pdf_dict_getp(pdf_trailer(_doc), "Root/AcroForm/XFA");
        if (pdf_is_array(xfa))
            pdf_dict_puts_drop(_info, "Unsupported_XFA", pdf_new_bool(_doc, 1));
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load document properties");
        pdf_drop_obj(_info);
        _info = nullptr;
    }
    fz_try(ctx) {
        pdf_obj *pagelabels = pdf_dict_getp(pdf_trailer(_doc), "Root/PageLabels");
        if (pagelabels)
            _pagelabels = BuildPageLabelVec(pagelabels, PageCount());
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load page labels");
    }

    AssertCrash(!pdf_js_supported(_doc));

    return true;
}

PdfTocItem *PdfEngineImpl::BuildTocTree(fz_outline *entry, int& idCounter)
{
    PdfTocItem *node = nullptr;

    for (; entry; entry = entry->next) {
        WCHAR *name = entry->title ? pdf_clean_string(str::conv::FromUtf8(entry->title)) : str::Dup(L"");
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
    PdfTocItem *node = nullptr;
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
    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(&ctxAccess);

    AutoFree name_utf8(str::conv::ToUtf8(name));
    pdf_obj *dest = nullptr;
    fz_try(ctx) {
        pdf_obj *nameobj = pdf_new_string(_doc, name_utf8, (int)str::Len(name_utf8));
        dest = pdf_lookup_dest(_doc, nameobj);
        pdf_drop_obj(nameobj);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    PageDestination *pageDest = nullptr;
    fz_link_dest ld = { FZ_LINK_NONE, 0 };
    fz_try(ctx) {
        ld = pdf_parse_link_dest(_doc, FZ_LINK_GOTO, dest);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    if (FZ_LINK_GOTO == ld.kind && ld.ld.gotor.page != -1) {
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
        return nullptr;
    if (failIfBusy)
        return _pages[pageNo-1];

    ScopedCritSec scope(&pagesAccess);

    pdf_page *page = _pages[pageNo-1];
    if (!page) {
        ScopedCritSec ctxScope(&ctxAccess);
        fz_var(page);
        fz_try(ctx) {
            page = pdf_load_page_by_obj(_doc, pageNo - 1, _pageObjs[pageNo-1]);
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
    fz_device *dev = nullptr;

    fz_var(dev);
    fz_try(ctx) {
        dev = fz_new_inspection_device(ctx, &data);
        fz_run_display_list(list, dev, &fz_identity, nullptr, nullptr);
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
                rects[i] = positions.at(i).rect;
            }
            imageRects[pageNo-1] = rects;
        }
    }

    return new PdfPageRun(page, list, data);
}

PdfPageRun *PdfEngineImpl::GetPageRun(pdf_page *page, bool tryOnly)
{
    PdfPageRun *result = nullptr;

    ScopedCritSec scope(&pagesAccess);

    for (size_t i = 0; i < runCache.Count(); i++) {
        if (runCache.at(i)->page == page) {
            result = runCache.at(i);
            break;
        }
    }
    if (!result && !tryOnly) {
        size_t mem = 0;
        for (size_t i = 0; i < runCache.Count(); i++) {
            // drop page runs that take up too much memory due to huge images
            // (except for the very recently used ones)
            if (i >= 2 && mem + runCache.at(i)->size_est >= MAX_PAGE_RUN_MEMORY)
                DropPageRun(runCache.at(i--), true);
            else
                mem += runCache.at(i)->size_est;
        }
        if (runCache.Count() >= MAX_PAGE_RUN_CACHE) {
            AssertCrash(runCache.Count() == MAX_PAGE_RUN_CACHE);
            DropPageRun(runCache.Last(), true);
        }

        ScopedCritSec scope2(&ctxAccess);

        fz_display_list *list = nullptr;
        fz_device *dev = nullptr;
        fz_var(list);
        fz_var(dev);
        fz_try(ctx) {
            list = fz_new_display_list(ctx);
            dev = fz_new_list_device(ctx, list);
            pdf_run_page(_doc, page, dev, &fz_identity, nullptr);
        }
        fz_catch(ctx) {
            fz_drop_display_list(ctx, list);
            list = nullptr;
        }
        fz_free_device(dev);

        if (list) {
            result = CreatePageRun(page, list);
            runCache.InsertAt(0, result);
        }
    }
    else if (result && result != runCache.at(0)) {
        // keep the list Most Recently Used first
        runCache.Remove(result);
        runCache.InsertAt(0, result);
    }

    if (result)
        result->refs++;
    return result;
}

bool PdfEngineImpl::RunPage(pdf_page *page, fz_device *dev, const fz_matrix *ctm, RenderTarget target, const fz_rect *cliprect, bool cacheRun, FitzAbortCookie *cookie)
{
    bool ok = true;

    PdfPageRun *run;
    if (Target_View == target && (run = GetPageRun(page, !cacheRun)) != nullptr) {
        EnterCriticalSection(&ctxAccess);
        Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, GetPageNo(page));
        fz_try(ctx) {
            fz_rect pagerect;
            fz_begin_page(dev, pdf_bound_page(_doc, page, &pagerect), ctm);
            fz_run_page_transparency(pageAnnots, dev, cliprect, false, page->transparency);
            fz_run_display_list(run->list, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_run_page_transparency(pageAnnots, dev, cliprect, true, page->transparency);
            fz_run_user_page_annots(pageAnnots, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_end_page(dev);
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
            fz_rect pagerect;
            fz_begin_page(dev, pdf_bound_page(_doc, page, &pagerect), ctm);
            fz_run_page_transparency(pageAnnots, dev, cliprect, false, page->transparency);
            pdf_run_page_with_usage(_doc, page, dev, ctm, targetName, cookie ? &cookie->cookie : nullptr);
            fz_run_page_transparency(pageAnnots, dev, cliprect, true, page->transparency);
            fz_run_user_page_annots(pageAnnots, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_end_page(dev);
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
    ScopedCritSec scope(&pagesAccess);
    run->refs--;

    if (0 == run->refs || forceRemove)
        runCache.Remove(run);

    if (0 == run->refs) {
        ScopedCritSec ctxScope(&ctxAccess);
        fz_drop_display_list(ctx, run->list);
        delete run;
    }
}

RectD PdfEngineImpl::PageMediabox(int pageNo)
{
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    if (!_mediaboxes[pageNo-1].IsEmpty())
        return _mediaboxes[pageNo-1];

    pdf_obj *page = _pageObjs[pageNo - 1];
    if (!page)
        return RectD();

    ScopedCritSec scope(&ctxAccess);

    // cf. pdf-page.c's pdf_load_page
    fz_rect mbox = fz_empty_rect, cbox = fz_empty_rect;
    int rotate = 0;
    float userunit = 1.0;
    fz_try(ctx) {
        pdf_to_rect(ctx, pdf_lookup_inherited_page_item(_doc, page, "MediaBox"), &mbox);
        pdf_to_rect(ctx, pdf_lookup_inherited_page_item(_doc, page, "CropBox"), &cbox);
        rotate = pdf_to_int(pdf_lookup_inherited_page_item(_doc, page, "Rotate"));
        pdf_obj *obj = pdf_dict_gets(page, "UserUnit");
        if (pdf_is_real(obj))
            userunit = pdf_to_real(obj);
    }
    fz_catch(ctx) { }
    if (fz_is_empty_rect(&mbox)) {
        fz_warn(ctx, "cannot find page size for page %d", pageNo);
        mbox.x0 = 0; mbox.y0 = 0;
        mbox.x1 = 612; mbox.y1 = 792;
    }
    if (!fz_is_empty_rect(&cbox)) {
        fz_intersect_rect(&mbox, &cbox);
        if (fz_is_empty_rect(&mbox))
            return RectD();
    }
    if ((rotate % 90) != 0)
        rotate = 0;

    // cf. pdf-page.c's pdf_bound_page
    fz_matrix ctm;
    fz_transform_rect(&mbox, fz_rotate(&ctm, (float)rotate));

    _mediaboxes[pageNo-1] = RectD(0, 0, (mbox.x1 - mbox.x0) * userunit, (mbox.y1 - mbox.y0) * userunit);
    return _mediaboxes[pageNo-1];
}

RectD PdfEngineImpl::PageContentBox(int pageNo, RenderTarget target)
{
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    pdf_page *page = GetPdfPage(pageNo);
    if (!page)
        return RectD();

    fz_rect rect = fz_empty_rect;
    fz_device *dev = nullptr;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &rect);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return RectD();
    }
    LeaveCriticalSection(&ctxAccess);

    fz_rect pagerect;
    pdf_bound_page(_doc, page, &pagerect);
    bool ok = RunPage(page, dev, &fz_identity, target, &pagerect, false);
    if (!ok)
        return PageMediabox(pageNo);
    if (fz_is_infinite_rect(&rect))
        return PageMediabox(pageNo);

    RectD rect2 = fz_rect_to_RectD(rect);
    return rect2.Intersect(PageMediabox(pageNo));
}

PointD PdfEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        fz_invert_matrix(&ctm, &ctm);
    fz_point pt2 = { (float)pt.x, (float)pt.y };
    fz_transform_point(&pt2, &ctm);
    return PointD(pt2.x, pt2.y);
}

RectD PdfEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        fz_invert_matrix(&ctm, &ctm);
    fz_rect rect2 = fz_RectD_to_rect(rect);
    fz_transform_rect(&rect2, &ctm);
    return fz_rect_to_RectD(rect2);
}

RenderedBitmap *PdfEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    pdf_page* page = GetPdfPage(pageNo);
    if (!page || !pdf_is_dict(page->me))
        return nullptr;

    fz_rect pRect;
    if (pageRect)
        pRect = fz_RectD_to_rect(*pageRect);
    else
        pdf_bound_page(_doc, page, &pRect);
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_rect r = pRect;
    fz_irect bbox;
    fz_round_rect(&bbox, fz_transform_rect(&r, &ctm));

    fz_pixmap *image = nullptr;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        fz_colorspace *colorspace = fz_device_rgb(ctx);
        image = fz_new_pixmap_with_bbox(ctx, colorspace, &bbox);
        fz_clear_pixmap_with_value(ctx, image, 0xFF); // initialize white background
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }

    fz_device *dev = nullptr;
    fz_try(ctx) {
        dev = fz_new_draw_device(ctx, image);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, image);
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    FitzAbortCookie *cookie = nullptr;
    if (cookie_out)
        *cookie_out = cookie = new FitzAbortCookie();
    fz_rect cliprect;
    bool ok = RunPage(page, dev, &ctm, target, fz_rect_from_irect(&cliprect, &bbox), true, cookie);

    ScopedCritSec scope(&ctxAccess);

    RenderedBitmap *bitmap = nullptr;
    if (ok)
        bitmap = new_rendered_fz_pixmap(ctx, image);
    fz_drop_pixmap(ctx, image);
    return bitmap;
}

PageElement *PdfEngineImpl::GetElementAtPos(int pageNo, PointD pt)
{
    pdf_page *page = GetPdfPage(pageNo, true);
    if (!page)
        return nullptr;

    fz_point p = { (float)pt.x, (float)pt.y };
    for (fz_link *link = page->links; link; link = link->next) {
        if (link->dest.kind != FZ_LINK_NONE && fz_is_pt_in_rect(link->rect, p))
            return new PdfLink(this, &link->dest, link->rect, pageNo, &p);
    }

    if (pageAnnots[pageNo-1]) {
        for (size_t i = 0; pageAnnots[pageNo-1][i]; i++) {
            pdf_annot *annot = pageAnnots[pageNo-1][i];
            fz_rect rect = annot->rect;
            fz_transform_rect(&rect, &page->ctm);
            if (fz_is_pt_in_rect(rect, p)) {
                ScopedCritSec scope(&ctxAccess);

                AutoFreeW contents(str::conv::FromPdf(pdf_dict_gets(annot->obj, "Contents")));
                // TODO: use separate classes for comments and tooltips?
                if (str::IsEmpty(contents.Get()) && FZ_ANNOT_WIDGET == annot->annot_type)
                    contents.Set(str::conv::FromPdf(pdf_dict_gets(annot->obj, "TU")));
                return new PdfComment(contents, fz_rect_to_RectD(rect), pageNo);
            }
        }
    }

    if (imageRects[pageNo-1]) {
        for (size_t i = 0; !fz_is_empty_rect(&imageRects[pageNo-1][i]); i++)
            if (fz_is_pt_in_rect(imageRects[pageNo-1][i], p))
                return new PdfImage(this, pageNo, imageRects[pageNo-1][i], i);
    }

    return nullptr;
}

Vec<PageElement *> *PdfEngineImpl::GetElements(int pageNo)
{
    pdf_page *page = GetPdfPage(pageNo, true);
    if (!page)
        return nullptr;

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    Vec<PageElement *> *els = new Vec<PageElement *>();
    if (!els)
        return nullptr;

    if (imageRects[pageNo-1]) {
        for (size_t i = 0; !fz_is_empty_rect(&imageRects[pageNo-1][i]); i++)
            els->Append(new PdfImage(this, pageNo, imageRects[pageNo-1][i], i));
    }

    if (pageAnnots[pageNo-1]) {
        ScopedCritSec scope(&ctxAccess);

        for (size_t i = 0; pageAnnots[pageNo-1][i]; i++) {
            pdf_annot *annot = pageAnnots[pageNo-1][i];
            fz_rect rect = annot->rect;
            fz_transform_rect(&rect, &page->ctm);
            AutoFreeW contents(str::conv::FromPdf(pdf_dict_gets(annot->obj, "Contents")));
            if (str::IsEmpty(contents.Get()) && FZ_ANNOT_WIDGET == annot->annot_type)
                contents.Set(str::conv::FromPdf(pdf_dict_gets(annot->obj, "TU")));
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
    AssertCrash(!page->links || page->links->refs == 1);

    RectI *coords;
    AutoFreeW pageText(ExtractPageText(page, L"\n", &coords, Target_View, true));
    if (!pageText)
        return;

    LinkRectList *list = LinkifyText(pageText, coords);
    for (size_t i = 0; i < list->links.Count(); i++) {
        bool overlaps = false;
        for (fz_link *next = page->links; next && !overlaps; next = next->next)
            overlaps = fz_calc_overlap(list->coords.at(i), next->rect) >= 0.25f;
        if (!overlaps) {
            AutoFree uri(str::conv::ToUtf8(list->links.at(i)));
            if (!uri) continue;
            fz_link_dest ld = { FZ_LINK_URI, 0 };
            ld.ld.uri.uri = fz_strdup(ctx, uri);
            // add links in top-to-bottom order (i.e. last-to-first)
            fz_link *link = fz_new_link(ctx, &list->coords.at(i), ld);
            CrashIf(!link); // TODO: if fz_new_link throws, there are memory leaks
            link->next = page->links;
            page->links = link;
        }
    }

    delete list;
    free(coords);
}

pdf_annot **PdfEngineImpl::ProcessPageAnnotations(pdf_page *page)
{
    Vec<pdf_annot *> annots;

    for (pdf_annot *annot = page->annots; annot; annot = annot->next) {
        if (FZ_ANNOT_FILEATTACHMENT == annot->annot_type) {
            pdf_obj *file = pdf_dict_gets(annot->obj, "FS");
            pdf_obj *embedded = pdf_dict_getsa(pdf_dict_gets(file, "EF"), "DOS", "F");
            fz_rect rect;
            pdf_to_rect(ctx, pdf_dict_gets(annot->obj, "Rect"), &rect);
            if (file && embedded && !fz_is_empty_rect(&rect)) {
                fz_link_dest ld;
                ld.kind = FZ_LINK_LAUNCH;
                ld.ld.launch.file_spec = pdf_file_spec_to_str(_doc, file);
                ld.ld.launch.new_window = 1;
                ld.ld.launch.embedded_num = pdf_to_num(embedded);
                ld.ld.launch.embedded_gen = pdf_to_gen(embedded);
                ld.ld.launch.is_uri = 0;
                fz_transform_rect(&rect, &page->ctm);
                // add links in top-to-bottom order (i.e. last-to-first)
                fz_link *link = fz_new_link(ctx, &rect, ld);
                link->next = page->links;
                page->links = link;
                // TODO: expose /Contents in addition to the file path
            }
            else if (!str::IsEmpty(pdf_to_str_buf(pdf_dict_gets(annot->obj, "Contents")))) {
                annots.Append(annot);
            }
        }
        else if (!str::IsEmpty(pdf_to_str_buf(pdf_dict_gets(annot->obj, "Contents"))) && annot->annot_type != FZ_ANNOT_FREETEXT) {
            annots.Append(annot);
        }
        else if (FZ_ANNOT_WIDGET == annot->annot_type && !str::IsEmpty(pdf_to_str_buf(pdf_dict_gets(annot->obj, "TU")))) {
            if (!(pdf_to_int(pdf_dict_gets(annot->obj, "Ff")) & Ff_ReadOnly))
                annots.Append(annot);
        }
    }

    if (annots.Count() == 0)
        return nullptr;

    // re-order list into top-to-bottom order (i.e. last-to-first)
    annots.Reverse();
    // add sentinel value
    annots.Append(nullptr);
    return annots.StealData();
}

RenderedBitmap *PdfEngineImpl::GetPageImage(int pageNo, RectD rect, size_t imageIx)
{
    pdf_page *page = GetPdfPage(pageNo);
    if (!page)
        return nullptr;

    Vec<FitzImagePos> positions;
    ListInspectionData data(positions);
    fz_device *dev = nullptr;

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_inspection_device(ctx, &data);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    RunPage(page, dev, &fz_identity);

    if (imageIx >= positions.Count() || fz_rect_to_RectD(positions.at(imageIx).rect) != rect) {
        AssertCrash(0);
        return nullptr;
    }

    ScopedCritSec scope(&ctxAccess);

    fz_pixmap *pixmap = nullptr;
    fz_try(ctx) {
        fz_image *image = positions.at(imageIx).image;
        pixmap = fz_new_pixmap_from_image(ctx, image, image->w, image->h);
    }
    fz_catch(ctx) {
        return nullptr;
    }
    RenderedBitmap *bmp = new_rendered_fz_pixmap(ctx, pixmap);
    fz_drop_pixmap(ctx, pixmap);

    return bmp;
}

WCHAR *PdfEngineImpl::ExtractPageText(pdf_page *page, const WCHAR *lineSep, RectI **coordsOut, RenderTarget target, bool cacheRun)
{
    if (!page)
        return nullptr;

    fz_text_sheet *sheet = nullptr;
    fz_text_page *text = nullptr;
    fz_device *dev = nullptr;
    fz_var(sheet);
    fz_var(text);

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        sheet = fz_new_text_sheet(ctx);
        text = fz_new_text_page(ctx);
        dev = fz_new_text_device(ctx, sheet, text);
    }
    fz_catch(ctx) {
        fz_free_text_page(ctx, text);
        fz_free_text_sheet(ctx, sheet);
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    if (!cacheRun)
        fz_enable_device_hints(dev, FZ_NO_CACHE);

    // use an infinite rectangle as bounds (instead of pdf_bound_page) to ensure that
    // the extracted text is consistent between cached runs using a list device and
    // fresh runs (otherwise the list device omits text outside the mediabox bounds)
    bool ok = RunPage(page, dev, &fz_identity, target, nullptr, cacheRun);

    ScopedCritSec scope(&ctxAccess);

    WCHAR *content = nullptr;
    if (ok)
        content = fz_text_page_to_str(text, lineSep, coordsOut);
    fz_free_text_page(ctx, text);
    fz_free_text_sheet(ctx, sheet);

    return content;
}

WCHAR *PdfEngineImpl::ExtractPageText(int pageNo, const WCHAR *lineSep, RectI **coordsOut, RenderTarget target)
{
    pdf_page *page = GetPdfPage(pageNo, true);
    if (page)
        return ExtractPageText(page, lineSep, coordsOut, target);

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        page = pdf_load_page_by_obj(_doc, pageNo - 1, _pageObjs[pageNo-1]);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    WCHAR *result = ExtractPageText(page, lineSep, coordsOut, target);

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
    if (num < 0 || num >= pdf_xref_len(_doc))
        return false;
    // check whether it's a linearization dictionary
    fz_try(_doc->ctx) {
        pdf_cache_object(_doc, num, 0);
    }
    fz_catch(_doc->ctx) {
        return false;
    }
    pdf_obj *obj = pdf_get_xref_entry(_doc, num)->obj;
    if (!pdf_is_dict(obj))
        return false;
    // /Linearized format must be version 1.0
    if (pdf_to_real(pdf_dict_gets(obj, "Linearized")) != 1.0f)
        return false;
    // /L must be the exact file size
    if (pdf_to_int(pdf_dict_gets(obj, "L")) != _doc->file_size)
        return false;
    // /O must be the object number of the first page
    if (pdf_to_int(pdf_dict_gets(obj, "O")) != pdf_to_num(_pageObjs[0]))
        return false;
    // /N must be the total number of pages
    if (pdf_to_int(pdf_dict_gets(obj, "N")) != PageCount())
        return false;
    // /H must be an array and /E and /T must be integers
    return pdf_is_array(pdf_dict_gets(obj, "H")) &&
           pdf_is_int(pdf_dict_gets(obj, "E")) &&
           pdf_is_int(pdf_dict_gets(obj, "T"));
}

static void pdf_extract_fonts(pdf_obj *res, Vec<pdf_obj *>& fontList, Vec<pdf_obj *>& resList)
{
    if (!res || pdf_mark_obj(res))
        return;
    resList.Append(res);

    pdf_obj *fonts = pdf_dict_gets(res, "Font");
    for (int k = 0; k < pdf_dict_len(fonts); k++) {
        pdf_obj *font = pdf_resolve_indirect(pdf_dict_get_val(fonts, k));
        if (font && !fontList.Contains(font))
            fontList.Append(font);
    }
    // also extract fonts for all XObjects (recursively)
    pdf_obj *xobjs = pdf_dict_gets(res, "XObject");
    for (int k = 0; k < pdf_dict_len(xobjs); k++) {
        pdf_obj *xobj = pdf_dict_get_val(xobjs, k);
        pdf_obj *xres = pdf_dict_gets(xobj, "Resources");
        pdf_extract_fonts(xres, fontList, resList);
    }
}

WCHAR *PdfEngineImpl::ExtractFontList()
{
    Vec<pdf_obj *> fontList;
    Vec<pdf_obj *> resList;

    // collect all fonts from all page objects
    for (int i = 1; i <= PageCount(); i++) {
        pdf_page *page = GetPdfPage(i);
        if (page) {
            ScopedCritSec scope(&ctxAccess);
            fz_try(ctx) {
                pdf_extract_fonts(page->resources, fontList, resList);
                for (pdf_annot *annot = page->annots; annot; annot = annot->next) {
                    if (annot->ap)
                        pdf_extract_fonts(annot->ap->resources, fontList, resList);
                }
            }
            fz_catch(ctx) { }
        }
    }

    // start ctxAccess scope here so that we don't also have to
    // ask for pagesAccess (as is required for GetPdfPage)
    ScopedCritSec scope(&ctxAccess);

    for (pdf_obj *res : resList) {
        pdf_unmark_obj(res);
    }

    WStrVec fonts;
    for (size_t i = 0; i < fontList.Count(); i++) {
        const char *name = nullptr, *type = nullptr, *encoding = nullptr;
        AutoFree anonFontName;
        bool embedded = false;
        fz_try(ctx) {
            pdf_obj *font = fontList.at(i);
            pdf_obj *font2 = pdf_array_get(pdf_dict_gets(font, "DescendantFonts"), 0);
            if (!font2)
                font2 = font;

            name = pdf_to_name(pdf_dict_getsa(font2, "BaseFont", "Name"));
            bool needAnonName = str::IsEmpty(name);
            if (needAnonName && font2 != font) {
                name = pdf_to_name(pdf_dict_getsa(font, "BaseFont", "Name"));
                needAnonName = str::IsEmpty(name);
            }
            if (needAnonName) {
                anonFontName.Set(str::Format("<#%d>", pdf_obj_parent_num(font2)));
                name = anonFontName;
            }
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
            if (str::Eq(type, "Type3"))
                embedded = pdf_dict_gets(font2, "CharProcs") != nullptr;

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
        if (name[0] < 0 && MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, name, -1, nullptr, 0))
            info.Append(AutoFree(str::ToMultiByte(name, 936, CP_UTF8)));
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

        AutoFreeW fontInfo(str::conv::FromUtf8(info.LendData()));
        if (fontInfo && !fonts.Contains(fontInfo))
            fonts.Append(fontInfo.StealData());
    }
    if (fonts.Count() == 0)
        return nullptr;

    fonts.SortNatural();
    return fonts.Join(L"\n");
}

WCHAR *PdfEngineImpl::GetProperty(DocumentProperty prop)
{
    if (!_doc)
        return nullptr;

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
        return fstruct.Count() > 0 ? fstruct.Join(L",") : nullptr;
    }

    if (Prop_UnsupportedFeatures == prop) {
        if (pdf_to_bool(pdf_dict_gets(_info, "Unsupported_XFA")))
            return str::Dup(L"XFA");
        return nullptr;
    }

    if (Prop_FontList == prop)
        return ExtractFontList();

    static struct {
        DocumentProperty prop;
        const char *name;
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
            return obj ? pdf_clean_string(str::conv::FromPdf(obj)) : nullptr;
        }
    }
    return nullptr;
};

bool PdfEngineImpl::SupportsAnnotation(bool forSaving) const
{
    if (forSaving) {
        // TODO: support updating of documents where pages aren't all numbered objects?
        for (int i = 0; i < PageCount(); i++) {
            if (pdf_to_num(_pageObjs[i]) == 0)
                return false;
        }
    }
    return true;
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
        return nullptr;
    return str::Dup(_decryptionKey);
}

PageLayoutType PdfEngineImpl::PreferredLayout()
{
    PageLayoutType layout = Layout_Single;

    ScopedCritSec scope(&ctxAccess);
    pdf_obj *root = nullptr;
    fz_try(ctx) {
        root = pdf_dict_gets(pdf_trailer(_doc), "Root");
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
    unsigned char *data = nullptr;
    ScopedCritSec scope(&ctxAccess);
    fz_try(ctx) {
        data = fz_extract_stream_data(_doc->file, cbCount);
    }
    fz_catch(ctx) {
        data = nullptr;
        if (FileName()) {
            data = (unsigned char *) file::ReadAll(FileName(), cbCount);
        }
    }
    return data;
}

bool PdfEngineImpl::SaveFileAs(const char *copyFileName, bool includeUserAnnots)
{
    size_t dataLen;
    AutoFreeW dstPath(str::conv::FromUtf8(copyFileName));
    ScopedMem<unsigned char> data(GetFileData(&dataLen));
    if (data) {
        bool ok = file::WriteAll(dstPath, data.Get(), dataLen);
        if (ok)
            return !includeUserAnnots || SaveUserAnnots(copyFileName);
    }
    if (!FileName())
        return false;
    bool ok = CopyFileW(FileName(), dstPath, FALSE);
    if (!ok)
        return false;
    // TODO: try to recover when SaveUserAnnots fails?
    return !includeUserAnnots || SaveUserAnnots(copyFileName);
}

static bool pdf_file_update_add_annotation(pdf_document *doc, pdf_page *page, pdf_obj *page_obj, PageAnnotation& annot, pdf_obj *annots)
{
    static const char *obj_dict = "<<\
    /Type /Annot /Subtype /%s\
    /Rect [%f %f %f %f]\
    /C [%f %f %f]\
    /F %d\
    /P %d %d R\
    /QuadPoints %s\
    /AP << >>\
>>";
    static const char *obj_quad_tpl = "[%f %f %f %f %f %f %f %f]";
    static const char *ap_dict = "<< /Type /XObject /Subtype /Form /BBox [0 0 %f %f] /Resources << /ExtGState << /GS << /Type /ExtGState /ca %.f /AIS false /BM /Multiply >> >> /ProcSet [/PDF] >> >>";
    static const char *ap_highlight = "q /DeviceRGB cs /GS gs %f %f %f rg 0 0 %f %f re f Q\n";
    static const char *ap_underline = "q /DeviceRGB CS %f %f %f RG 1 w [] 0 d 0 0.5 m %f 0.5 l S Q\n";
    static const char *ap_strikeout = "q /DeviceRGB CS %f %f %f RG 1 w [] 0 d 0 %f m %f %f l S Q\n";
    static const char *ap_squiggly = "q /DeviceRGB CS %f %f %f RG 0.5 w [1] 1.5 d 0 0.25 m %f 0.25 l S [1] 0.5 d 0 0.75 m %f 0.75 l S Q\n";

    fz_context *ctx = doc->ctx;
    pdf_obj *annot_obj = nullptr, *ap_obj = nullptr;
    fz_buffer *ap_buf = nullptr;

    fz_var(annot_obj);
    fz_var(ap_obj);
    fz_var(ap_buf);

    const char *subtype = Annot_Highlight == annot.type ? "Highlight" :
                          Annot_Underline == annot.type ? "Underline" :
                          Annot_StrikeOut == annot.type ? "StrikeOut" :
                          Annot_Squiggly  == annot.type ? "Squiggly"  : nullptr;
    CrashIf(!subtype);
    int rotation = (page->rotate + 360) % 360;
    CrashIf((rotation % 90) != 0);
    // convert the annotation's rectangle back to raw user space
    fz_rect r = fz_RectD_to_rect(annot.rect);
    fz_matrix invctm;
    fz_transform_rect(&r, fz_invert_matrix(&invctm, &page->ctm));
    double dx = r.x1 - r.x0, dy = r.y1 - r.y0;
    if ((rotation % 180) == 90)
        std::swap(dx, dy);
    float rgb[3] = { annot.color.r / 255.f, annot.color.g / 255.f, annot.color.b / 255.f };
    // rotate the QuadPoints to match the page
    AutoFree quad_tpl;
    if (0 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x0, r.y1, r.x1, r.y1, r.x0, r.y0, r.x1, r.y0));
    else if (90 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x0, r.y0, r.x0, r.y1, r.x1, r.y0, r.x1, r.y1));
    else if (180 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x1, r.y0, r.x0, r.y0, r.x1, r.y1, r.x0, r.y1));
    else // if (270 == rotation)
        quad_tpl.Set(str::Format(obj_quad_tpl, r.x1, r.y1, r.x1, r.y0, r.x0, r.y1, r.x0, r.y0));
    AutoFree annot_tpl(str::Format(obj_dict, subtype,
        r.x0, r.y0, r.x1, r.y1, rgb[0], rgb[1], rgb[2], //Rect and Color
        F_Print, pdf_to_num(page_obj), pdf_to_gen(page_obj), //F and P
        quad_tpl.Get()));
    AutoFree annot_ap_dict(str::Format(ap_dict, dx, dy, annot.color.a / 255.f));
    AutoFree annot_ap_stream;

    fz_try(ctx) {
        annot_obj = pdf_new_obj_from_str(doc, annot_tpl);
        // append the annotation to the file
        pdf_array_push_drop(annots, pdf_new_ref(doc, annot_obj));
    }
    fz_catch(ctx) {
        pdf_drop_obj(annot_obj);
        return false;
    }

    if (doc->crypt) {
        // since we don't encrypt the appearance stream, for encrypted documents
        // the readers will have to synthesize an appearance stream themselves
        pdf_drop_obj(annot_obj);
        return true;
    }

    fz_try(ctx) {
        // create the appearance stream (unencrypted) and append it to the file
        ap_obj = pdf_new_obj_from_str(doc, annot_ap_dict);
        switch (annot.type) {
        case Annot_Highlight:
            annot_ap_stream.Set(str::Format(ap_highlight, rgb[0], rgb[1], rgb[2], dx, dy));
            break;
        case Annot_Underline:
            annot_ap_stream.Set(str::Format(ap_underline, rgb[0], rgb[1], rgb[2], dx));
            break;
        case Annot_StrikeOut:
            annot_ap_stream.Set(str::Format(ap_strikeout, rgb[0], rgb[1], rgb[2], dy / 2, dx, dy / 2));
            break;
        case Annot_Squiggly:
            annot_ap_stream.Set(str::Format(ap_squiggly, rgb[0], rgb[1], rgb[2], dx, dx));
            break;
        }
        if (annot.type != Annot_Highlight)
            pdf_dict_dels(pdf_dict_gets(ap_obj, "Resources"), "ExtGState");
        if (rotation) {
            fz_matrix rot;
            pdf_dict_puts_drop(ap_obj, "Matrix", pdf_new_matrix(doc, fz_rotate(&rot, rotation)));
        }
        ap_buf = fz_new_buffer(ctx, (int)str::Len(annot_ap_stream));
        memcpy(ap_buf->data, annot_ap_stream, (ap_buf->len = (int)str::Len(annot_ap_stream)));
        pdf_dict_puts_drop(ap_obj, "Length", pdf_new_int(doc, ap_buf->len));
        // append the appearance stream to the file
        int num = pdf_create_object(doc);
        pdf_update_object(doc, num, ap_obj);
        pdf_update_stream(doc, num, ap_buf);
        pdf_dict_puts_drop(pdf_dict_gets(annot_obj, "AP"), "N", pdf_new_indirect(doc, num, 0));
    }
    fz_always(ctx) {
        pdf_drop_obj(ap_obj);
        fz_drop_buffer(ctx, ap_buf);
        pdf_drop_obj(annot_obj);
    }
    fz_catch(ctx) {
        return false;
    }

    return true;
}

bool PdfEngineImpl::SaveUserAnnots(const char *pathUtf8)
{
    if (!userAnnots.Count())
        return true;

    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(&ctxAccess);

    bool ok = true;
    Vec<PageAnnotation> pageAnnots;

    fz_try(ctx) {
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            pdf_page *page = GetPdfPage(pageNo);
            // TODO: this will skip annotations for broken documents
            if (!page || !pdf_to_num(_pageObjs[pageNo - 1])) {
                ok = false;
                break;
            }
            pageAnnots = fz_get_user_page_annots(userAnnots, pageNo);
            if (pageAnnots.Count() == 0)
                continue;
            // get the page's /Annots array for appending
            pdf_obj *annots = pdf_dict_gets(_pageObjs[pageNo - 1], "Annots");
            if (!pdf_is_array(annots)) {
                pdf_dict_puts_drop(_pageObjs[pageNo - 1], "Annots", pdf_new_array(_doc, (int)pageAnnots.Count()));
                annots = pdf_dict_gets(_pageObjs[pageNo - 1], "Annots");
            }
            if (!pdf_is_indirect(annots)) {
                // make /Annots indirect for the current /Page
                pdf_dict_puts_drop(_pageObjs[pageNo - 1], "Annots", pdf_new_ref(_doc, annots));
            }
            // append all annotations for the current page
            for (size_t i = 0; i < pageAnnots.Count(); i++) {
                ok &= pdf_file_update_add_annotation(_doc, page, _pageObjs[pageNo - 1], pageAnnots.at(i), annots);
            }
        }
        if (ok) {
            fz_write_options opts = { 0 };
            opts.do_incremental = 1;
            pdf_write_document(_doc, const_cast<char*>(pathUtf8), &opts);
        }
    }
    fz_catch(ctx) {
        ok = false;
    }
    return ok;
}

bool PdfEngineImpl::SaveEmbedded(LinkSaverUI& saveUI, int num, int gen)
{
    ScopedCritSec scope(&ctxAccess);

    fz_buffer *data = nullptr;
    fz_try(ctx) {
        data = pdf_load_stream(_doc, num, gen);
    }
    fz_catch(ctx) {
        return false;
    }
    CrashIf(nullptr == data);
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
    for (int i = 0; !fz_is_empty_rect(&imageRects[pageNo-1][i]); i++)
        if (fz_calc_overlap(mbox, imageRects[pageNo-1][i]) >= 0.9f)
            return false;
    return true;
}

WCHAR *PdfEngineImpl::GetPageLabel(int pageNo) const
{
    if (!_pagelabels || pageNo < 1 || PageCount() < pageNo)
        return BaseEngine::GetPageLabel(pageNo);

    return str::Dup(_pagelabels->at(pageNo - 1));
}

int PdfEngineImpl::GetPageByLabel(const WCHAR *label) const
{
    int pageNo = _pagelabels ? _pagelabels->Find(label) + 1 : 0;
    if (!pageNo)
        return BaseEngine::GetPageByLabel(label);

    return pageNo;
}

static bool IsRelativeURI(const WCHAR *uri)
{
    const WCHAR *c = uri;
    while (*c && *c != ':' && *c != '/' && *c != '?' && *c != '#') {
        c++;
    }
    return *c != ':';
}

WCHAR *PdfLink::GetValue() const
{
    if (!link || !engine)
        return nullptr;
    if (link->kind != FZ_LINK_URI && link->kind != FZ_LINK_LAUNCH &&
        link->kind != FZ_LINK_GOTOR)
        return nullptr;

    ScopedCritSec scope(&engine->ctxAccess);

    WCHAR *path = nullptr;

    switch (link->kind) {
    case FZ_LINK_URI:
        path = str::conv::FromUtf8(link->ld.uri.uri);
        if (IsRelativeURI(path)) {
            AutoFreeW base;
            fz_try(engine->ctx) {
                pdf_obj *obj = pdf_dict_gets(pdf_trailer(engine->_doc), "Root");
                obj = pdf_dict_gets(pdf_dict_gets(obj, "URI"), "Base");
                if (obj)
                    base.Set(str::conv::FromPdf(obj));
            }
            fz_catch(engine->ctx) { }
            if (!str::IsEmpty(base.Get())) {
                AutoFreeW uri(str::Join(base, path));
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
            AutoFreeW uri(str::Format(L"%s?%d,%d", path, x, y));
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
        if (link->ld.launch.is_uri)
            return Dest_LaunchURL;
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
    if (link && FZ_LINK_GOTOR == link->kind && !link->ld.gotor.dest)
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
    fz_point lt = link->ld.gotor.lt, rb = link->ld.gotor.rb;
    fz_transform_point(&lt, &page->ctm);
    fz_transform_point(&rb, &page->ctm);

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
    else if ((link->ld.gotor.flags & (fz_link_flag_fit_h | fz_link_flag_fit_v)) == fz_link_flag_fit_h &&
        (link->ld.gotor.flags & fz_link_flag_t_valid)) {
        // /FitH or /FitBH link
        result.y = lt.y;
    }
    // all other link types only affect the zoom level, which we intentionally leave alone
    return result;
}

WCHAR *PdfLink::GetDestName() const
{
    if (!link || FZ_LINK_GOTOR != link->kind || !link->ld.gotor.dest)
        return nullptr;
    return str::conv::FromUtf8(link->ld.gotor.dest);
}

bool PdfLink::SaveEmbedded(LinkSaverUI& saveUI)
{
    ScopedCritSec scope(&engine->ctxAccess);
    return engine->SaveEmbedded(saveUI, link->ld.launch.embedded_num, link->ld.launch.embedded_gen);
}

BaseEngine *PdfEngineImpl::CreateFromFile(const WCHAR *fileName, PasswordUI *pwdUI)
{
    PdfEngineImpl *engine = new PdfEngineImpl();
    if (!engine || !fileName || !engine->Load(fileName, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

BaseEngine *PdfEngineImpl::CreateFromStream(IStream *stream, PasswordUI *pwdUI)
{
    PdfEngineImpl *engine = new PdfEngineImpl();
    if (!engine->Load(stream, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

namespace PdfEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        char header[1024] = { 0 };
        file::ReadN(fileName, header, sizeof(header));

        for (int i = 0; i < sizeof(header) - 4; i++) {
            if (str::EqN(header + i, "%PDF", 4))
                return true;
        }
        return false;
    }

    return str::EndsWithI(fileName, L".pdf") || findEmbedMarks(fileName);
}

BaseEngine *CreateFromFile(const WCHAR *fileName, PasswordUI *pwdUI)
{
    return PdfEngineImpl::CreateFromFile(fileName, pwdUI);
}

BaseEngine *CreateFromStream(IStream *stream, PasswordUI *pwdUI)
{
    return PdfEngineImpl::CreateFromStream(stream, pwdUI);
}

}

///// XPS-specific extensions to Fitz/MuXPS /////

extern "C" {
#include <mupdf/xps.h>
}

// TODO: use http://schemas.openxps.org/oxps/v1.0 as well once NS actually matters
#define NS_XPS_MICROSOFT "http://schemas.microsoft.com/xps/2005/06"

fz_rect
xps_bound_page_quick(xps_document *doc, int number)
{
    xps_page *page = doc->first_page;
    for (int n = 0; n < number && page; n++)
        page = page->next;

    xps_part *part = page ? xps_read_part(doc, page->name) : nullptr;
    if (!part)
        return fz_empty_rect;

    const char *data = (const char *)part->data;
    size_t data_size = part->size;

    AutoFree dataUtf8;
    if (str::StartsWith(data, UTF16BE_BOM)) {
        for (int i = 0; i + 1 < part->size; i += 2) {
            std::swap(part->data[i], part->data[i+1]);
        }
    }
    if (str::StartsWith(data, UTF16_BOM)) {
        dataUtf8.Set(str::conv::ToUtf8((const WCHAR *)(part->data + 2), (part->size - 2) / 2));
        data = dataUtf8;
        data_size = str::Len(dataUtf8);
    }
    else if (str::StartsWith(data, UTF8_BOM)) {
        data += 3;
        data_size -= 3;
    }

    HtmlPullParser p(data, data_size);
    HtmlToken *tok = p.Next();
    fz_rect bounds = fz_empty_rect;
    if (tok && tok->IsStartTag() && tok->NameIsNS("FixedPage", NS_XPS_MICROSOFT)) {
        AttrInfo *attr = tok->GetAttrByNameNS("Width", NS_XPS_MICROSOFT);
        if (attr)
            bounds.x1 = fz_atof(attr->val) * 72.0f / 96.0f;
        attr = tok->GetAttrByNameNS("Height", NS_XPS_MICROSOFT);
        if (attr)
            bounds.y1 = fz_atof(attr->val) * 72.0f / 96.0f;
    }

    xps_free_part(doc, part);

    return bounds;
}

class xps_doc_props {
public:
    AutoFreeW title;
    AutoFreeW author;
    AutoFreeW subject;
    AutoFreeW creation_date;
    AutoFreeW modification_date;
};

static fz_xml *
xps_open_and_parse(xps_document *doc, char *path)
{
    fz_xml *root = nullptr;
    xps_part *part = xps_read_part(doc, path);

    fz_try(doc->ctx) {
        root = fz_parse_xml(doc->ctx, part->data, part->size, 0);
    }
    fz_always(doc->ctx) {
        xps_free_part(doc, part);
    }
    fz_catch(doc->ctx) {
        fz_rethrow(doc->ctx);
    }

    return root;
}

static WCHAR *
xps_get_core_prop(fz_context *ctx, fz_xml *item)
{
    fz_xml *text = fz_xml_down(item);

    if (!text)
        return nullptr;
    if (!fz_xml_text(text) || fz_xml_next(text)) {
        fz_warn(ctx, "non-text content for property %s", fz_xml_tag(item));
        return nullptr;
    }

    char *start, *end;
    for (start = fz_xml_text(text); str::IsWs(*start); start++);
    for (end = start + strlen(start); end > start && str::IsWs(*(end - 1)); end--);

    return str::conv::FromHtmlUtf8(start, end - start);
}

#define REL_CORE_PROPERTIES \
    "http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties"

xps_doc_props *
xps_extract_doc_props(xps_document *doc)
{
    fz_xml *root = xps_open_and_parse(doc, "/_rels/.rels");

    if (!fz_xml_is_tag(root, "Relationships")) {
        fz_free_xml(doc->ctx, root);
        fz_throw(doc->ctx, FZ_ERROR_GENERIC, "couldn't parse part '/_rels/.rels'");
    }

    bool has_correct_root = false;
    for (fz_xml *item = fz_xml_down(root); item; item = fz_xml_next(item)) {
        if (fz_xml_is_tag(item, "Relationship") &&
            str::Eq(fz_xml_att(item, "Type"), REL_CORE_PROPERTIES) && fz_xml_att(item, "Target")) {
            char path[1024];
            xps_resolve_url(path, "", fz_xml_att(item, "Target"), nelem(path));
            fz_free_xml(doc->ctx, root);
            root = xps_open_and_parse(doc, path);
            has_correct_root = true;
            break;
        }
    }

    if (!has_correct_root) {
        fz_free_xml(doc->ctx, root);
        return nullptr;
    }

    xps_doc_props *props = new xps_doc_props();

    for (fz_xml *item = fz_xml_down(root); item; item = fz_xml_next(item)) {
        if (fz_xml_is_tag(item, /*"dc:"*/"title") && !props->title)
            props->title.Set(xps_get_core_prop(doc->ctx, item));
        else if (fz_xml_is_tag(item, /*"dc:"*/"creator") && !props->author)
            props->author.Set(xps_get_core_prop(doc->ctx, item));
        else if (fz_xml_is_tag(item, /*"dc:"*/"subject") && !props->subject)
            props->subject.Set(xps_get_core_prop(doc->ctx, item));
        else if (fz_xml_is_tag(item, /*"dcterms:"*/"created") && !props->creation_date)
            props->creation_date.Set(xps_get_core_prop(doc->ctx, item));
        else if (fz_xml_is_tag(item, /*"dcterms:"*/"modified") && !props->modification_date)
            props->modification_date.Set(xps_get_core_prop(doc->ctx, item));
    }
    fz_free_xml(doc->ctx, root);

    return props;
}

///// XpsEngine is also based on Fitz and shares quite some code with PdfEngine /////

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

class XpsEngineImpl : public BaseEngine {
    friend XpsImage;

public:
    XpsEngineImpl();
    virtual ~XpsEngineImpl();
    BaseEngine *Clone() override;

    int PageCount() const override {
        return _doc ? xps_count_pages(_doc) : 0;
    }

    RectD PageMediabox(int pageNo) override;
    RectD PageContentBox(int pageNo, RenderTarget target=Target_View) override;

    RenderedBitmap *RenderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=nullptr, /* if nullptr: defaults to the page's mediabox */
                         RenderTarget target=Target_View, AbortCookie **cookie_out=nullptr) override;

    PointD Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse=false) override;
    RectD Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse=false) override;

    unsigned char *GetFileData(size_t *cbCount) override;
    bool SaveFileAs(const char *copyFileName, bool includeUserAnnots=false) override;
    WCHAR * ExtractPageText(int pageNo, const WCHAR *lineSep, RectI **coordsOut=nullptr,
                                    RenderTarget target=Target_View) override {
        UNUSED(target);
        return ExtractPageText(GetXpsPage(pageNo), lineSep, coordsOut);
    }
    bool HasClipOptimizations(int pageNo) override;
    WCHAR *GetProperty(DocumentProperty prop) override;

    bool SupportsAnnotation(bool forSaving=false) const override;
    void UpdateUserAnnotations(Vec<PageAnnotation> *list) override;

    float GetFileDPI() const override { return 72.0f; }
    const WCHAR *GetDefaultFileExt() const override { return L".xps"; }

    bool BenchLoadPage(int pageNo) override { return GetXpsPage(pageNo) != nullptr; }

    Vec<PageElement *> *GetElements(int pageNo) override;
    PageElement *GetElementAtPos(int pageNo, PointD pt) override;

    PageDestination *GetNamedDest(const WCHAR *name) override;
    bool HasTocTree() const override { return _outline != nullptr; }
    DocTocItem *GetTocTree() override;

    fz_rect FindDestRect(const char *target);

    static BaseEngine *CreateFromFile(const WCHAR *fileName);
    static BaseEngine *CreateFromStream(IStream *stream);

protected:
    // make sure to never ask for _pagesAccess in an ctxAccess
    // protected critical section in order to avoid deadlocks
    CRITICAL_SECTION ctxAccess;
    fz_context *    ctx;
    fz_locks_context fz_locks_ctx;
    xps_document *  _doc;
    fz_stream *     _docStream;

    CRITICAL_SECTION _pagesAccess;
    xps_page **     _pages;

    bool            Load(const WCHAR *fileName);
    bool            Load(IStream *stream);
    bool            Load(fz_stream *stm);
    bool            LoadFromStream(fz_stream *stm);

    xps_page      * GetXpsPage(int pageNo, bool failIfBusy=false);
    int             GetPageNo(xps_page *page);
    fz_matrix       viewctm(int pageNo, float zoom, int rotation) {
        const fz_rect tmpRect = fz_RectD_to_rect(PageMediabox(pageNo));
        return fz_create_view_ctm(&tmpRect, zoom, rotation);
    }
    fz_matrix       viewctm(xps_page *page, float zoom, int rotation) {
        fz_rect r;
        return fz_create_view_ctm(xps_bound_page(_doc, page, &r), zoom, rotation);
    }
    WCHAR         * ExtractPageText(xps_page *page, const WCHAR *lineSep,
                                    RectI **coordsOut=nullptr, bool cacheRun=false);

    Vec<XpsPageRun *> runCache; // ordered most recently used first
    XpsPageRun    * CreatePageRun(xps_page *page, fz_display_list *list);
    XpsPageRun    * GetPageRun(xps_page *page, bool tryOnly=false);
    bool            RunPage(xps_page *page, fz_device *dev, const fz_matrix *ctm,
                            const fz_rect *cliprect=nullptr, bool cacheRun=true,
                            FitzAbortCookie *cookie=nullptr);
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
    XpsLink() : engine(nullptr), link(nullptr), pageNo(-1) { }
    XpsLink(XpsEngineImpl *engine, fz_link_dest *link, fz_rect rect=fz_empty_rect, int pageNo=-1) :
        engine(engine), link(link), rect(fz_rect_to_RectD(rect)), pageNo(pageNo) { }

    PageElementType GetType() const  override { return Element_Link; }
    int GetPageNo() const  override { return pageNo; }
    RectD GetRect() const  override { return rect; }
    WCHAR *GetValue() const  override {
        if (link && FZ_LINK_URI == link->kind)
            return str::conv::FromUtf8(link->ld.uri.uri);
        return nullptr;
    }
    virtual PageDestination *AsLink() { return this; }

    PageDestType GetDestType() const override {
        if (!link)
            return Dest_None;
        if (FZ_LINK_GOTO == link->kind)
            return Dest_ScrollTo;
        if (FZ_LINK_URI == link->kind)
            return Dest_LaunchURL;
        return Dest_None;
    }
    int GetDestPageNo() const override {
        if (!link || link->kind != FZ_LINK_GOTO)
            return 0;
        return link->ld.gotor.page + 1;
    }
    RectD GetDestRect() const override {
        if (!engine || !link || link->kind != FZ_LINK_GOTO || !link->ld.gotor.dest)
            return RectD(DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
        return fz_rect_to_RectD(engine->FindDestRect(link->ld.gotor.dest));
    }
    WCHAR *GetDestValue() const override { return GetValue(); }
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
    virtual WCHAR *GetValue() const { return nullptr; }

    virtual RenderedBitmap *GetImage() {
        return engine->GetPageImage(pageNo, rect, imageIx);
    }
};

XpsEngineImpl::XpsEngineImpl() : _doc(nullptr), _docStream(nullptr), _pages(nullptr),
    _mediaboxes(nullptr), _outline(nullptr), _info(nullptr), imageRects(nullptr)
{
    InitializeCriticalSection(&_pagesAccess);
    InitializeCriticalSection(&ctxAccess);

    fz_locks_ctx.user = &ctxAccess;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, MAX_CONTEXT_MEMORY);
}

XpsEngineImpl::~XpsEngineImpl()
{
    EnterCriticalSection(&_pagesAccess);
    EnterCriticalSection(&ctxAccess);

    if (_pages) {
        // xps_pages are freed by xps_close_document -> xps_free_page_list
        AssertCrash(_doc);
        free(_pages);
    }

    fz_free_outline(ctx, _outline);
    delete _info;

    if (imageRects) {
        for (int i = 0; i < PageCount(); i++)
            free(imageRects[i]);
        free(imageRects);
    }

    while (runCache.Count() > 0) {
        AssertCrash(runCache.Last()->refs == 1);
        DropPageRun(runCache.Last(), true);
    }

    xps_close_document(_doc);
    _doc = nullptr;
    fz_close(_docStream);
    _docStream = nullptr;
    fz_free_context(ctx);
    ctx = nullptr;

    free(_mediaboxes);

    LeaveCriticalSection(&ctxAccess);
    DeleteCriticalSection(&ctxAccess);
    LeaveCriticalSection(&_pagesAccess);
    DeleteCriticalSection(&_pagesAccess);
}

BaseEngine *XpsEngineImpl::Clone()
{
    ScopedCritSec scope(&ctxAccess);

    XpsEngineImpl *clone = new XpsEngineImpl();
    bool ok;
    if (FileName()) {
        ok = clone->Load(FileName());
    } else {
        ok = clone->Load(_docStream);
    }
    if (!ok) {
        delete clone;
        return nullptr;
    }

    clone->UpdateUserAnnotations(&userAnnots);

    return clone;
}

bool XpsEngineImpl::Load(const WCHAR *fileName)
{
    AssertCrash(!FileName() && !_doc && !_docStream && ctx);
    SetFileName(fileName);
    if (!ctx)
        return false;

    if (dir::Exists(fileName)) {
        // load uncompressed documents as a recompressed ZIP stream
        ScopedComPtr<IStream> zipStream(OpenDirAsZipStream(fileName, true));
        if (!zipStream)
            return false;
        return Load(zipStream);
    }

    fz_stream *stm = nullptr;
    fz_try(ctx) {
        stm = fz_open_file2(ctx, fileName);
    }
    fz_catch(ctx) {
        return false;
    }
    return LoadFromStream(stm);
}

bool XpsEngineImpl::Load(IStream *stream)
{
    AssertCrash(!_doc && !_docStream && ctx);
    if (!ctx)
        return false;

    fz_stream *stm = nullptr;
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
    AssertCrash(!FileName() && !_doc && !_docStream && ctx);
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

    _docStream = stm;
    fz_try(ctx) {
        _doc = xps_open_document_with_stream(ctx, stm);
    }
    fz_catch(ctx) {
        return false;
    }

    if (PageCount() == 0) {
        fz_warn(ctx, "document has no pages");
        return false;
    }

    _pages = AllocArray<xps_page *>(PageCount());
    _mediaboxes = AllocArray<RectD>(PageCount());
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
        return nullptr;
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
            AssertCrash(page->links_resolved);
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
    fz_device *dev = nullptr;

    fz_var(dev);
    fz_try(ctx) {
        dev = fz_new_inspection_device(ctx, &data);
        fz_run_display_list(list, dev, &fz_identity, nullptr, nullptr);
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
                rects[i] = positions.at(i).rect;
            }
            imageRects[pageNo-1] = rects;
        }
    }

    return new XpsPageRun(page, list, data);
}

XpsPageRun *XpsEngineImpl::GetPageRun(xps_page *page, bool tryOnly)
{
    ScopedCritSec scope(&_pagesAccess);

    XpsPageRun *result = nullptr;

    for (size_t i = 0; i < runCache.Count(); i++) {
        if (runCache.at(i)->page == page) {
            result = runCache.at(i);
            break;
        }
    }
    if (!result && !tryOnly) {
        size_t mem = 0;
        for (size_t i = 0; i < runCache.Count(); i++) {
            // drop page runs that take up too much memory due to huge images
            // (except for the very recently used ones)
            if (i >= 2 && mem + runCache.at(i)->size_est >= MAX_PAGE_RUN_MEMORY)
                DropPageRun(runCache.at(i--), true);
            else
                mem += runCache.at(i)->size_est;
        }
        if (runCache.Count() >= MAX_PAGE_RUN_CACHE) {
            AssertCrash(runCache.Count() == MAX_PAGE_RUN_CACHE);
            DropPageRun(runCache.Last(), true);
        }

        ScopedCritSec ctxScope(&ctxAccess);

        fz_display_list *list = nullptr;
        fz_device *dev = nullptr;
        fz_var(list);
        fz_var(dev);
        fz_try(ctx) {
            list = fz_new_display_list(ctx);
            dev = fz_new_list_device(ctx, list);
            xps_run_page(_doc, page, dev, &fz_identity, nullptr);
        }
        fz_catch(ctx) {
            fz_drop_display_list(ctx, list);
            list = nullptr;
        }
        fz_free_device(dev);

        if (list) {
            result = CreatePageRun(page, list);
            runCache.InsertAt(0, result);
        }
    }
    else if (result && result != runCache.at(0)) {
        // keep the list Most Recently Used first
        runCache.Remove(result);
        runCache.InsertAt(0, result);
    }

    if (result)
        result->refs++;
    return result;
}

bool XpsEngineImpl::RunPage(xps_page *page, fz_device *dev, const fz_matrix *ctm, const fz_rect *cliprect, bool cacheRun, FitzAbortCookie *cookie)
{
    bool ok = true;

    XpsPageRun *run = GetPageRun(page, !cacheRun);
    if (run) {
        EnterCriticalSection(&ctxAccess);
        Vec<PageAnnotation> pageAnnots = fz_get_user_page_annots(userAnnots, GetPageNo(page));
        fz_try(ctx) {
            fz_rect pagerect;
            fz_begin_page(dev, xps_bound_page(_doc, page, &pagerect), ctm);
            fz_run_page_transparency(pageAnnots, dev, cliprect, false);
            fz_run_display_list(run->list, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_run_page_transparency(pageAnnots, dev, cliprect, true);
            fz_run_user_page_annots(pageAnnots, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_end_page(dev);
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
            fz_rect pagerect;
            fz_begin_page(dev, xps_bound_page(_doc, page, &pagerect), ctm);
            fz_run_page_transparency(pageAnnots, dev, cliprect, false);
            xps_run_page(_doc, page, dev, ctm, cookie ? &cookie->cookie : nullptr);
            fz_run_page_transparency(pageAnnots, dev, cliprect, true);
            fz_run_user_page_annots(pageAnnots, dev, ctm, cliprect, cookie ? &cookie->cookie : nullptr);
            fz_end_page(dev);
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

    if (0 == run->refs || forceRemove)
        runCache.Remove(run);

    if (0 == run->refs) {
        ScopedCritSec ctxScope(&ctxAccess);
        fz_drop_display_list(ctx, run->list);
        delete run;
    }
}

RectD XpsEngineImpl::PageMediabox(int pageNo)
{
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    if (!_mediaboxes)
        return RectD();

    RectD mbox = _mediaboxes[pageNo-1];
    if (!mbox.IsEmpty())
        return mbox;

    xps_page *page = GetXpsPage(pageNo, true);
    if (!page) {
        ScopedCritSec scope(&ctxAccess);
        fz_try(ctx) {
            mbox = fz_rect_to_RectD(xps_bound_page_quick(_doc, pageNo-1));
        }
        fz_catch(ctx) { }
        if (!mbox.IsEmpty()) {
            _mediaboxes[pageNo-1] = mbox;
            return _mediaboxes[pageNo-1];
        }
    }
    if (!page && (page = GetXpsPage(pageNo)) == nullptr)
        return RectD();

    fz_rect pagerect;
    _mediaboxes[pageNo-1] = fz_rect_to_RectD(*xps_bound_page(_doc, page, &pagerect));
    return _mediaboxes[pageNo-1];
}

RectD XpsEngineImpl::PageContentBox(int pageNo, RenderTarget target)
{
    UNUSED(target);
    AssertCrash(1 <= pageNo && pageNo <= PageCount());
    xps_page *page = GetXpsPage(pageNo);
    if (!page)
        return RectD();

    fz_rect rect = fz_empty_rect;
    fz_device *dev = nullptr;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &rect);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return RectD();
    }
    LeaveCriticalSection(&ctxAccess);

    fz_rect pagerect;
    xps_bound_page(_doc, page, &pagerect);
    bool ok = RunPage(page, dev, &fz_identity, &pagerect, false);
    if (!ok)
        return PageMediabox(pageNo);
    if (fz_is_infinite_rect(&rect))
        return PageMediabox(pageNo);

    RectD rect2 = fz_rect_to_RectD(rect);
    return rect2.Intersect(PageMediabox(pageNo));
}

PointD XpsEngineImpl::Transform(PointD pt, int pageNo, float zoom, int rotation, bool inverse)
{
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        fz_invert_matrix(&ctm, &ctm);
    fz_point pt2 = { (float)pt.x, (float)pt.y };
    fz_transform_point(&pt2, &ctm);
    return PointD(pt2.x, pt2.y);
}

RectD XpsEngineImpl::Transform(RectD rect, int pageNo, float zoom, int rotation, bool inverse)
{
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse)
        fz_invert_matrix(&ctm, &ctm);
    fz_rect rect2 = fz_RectD_to_rect(rect);
    fz_transform_rect(&rect2, &ctm);
    return fz_rect_to_RectD(rect2);
}

RenderedBitmap *XpsEngineImpl::RenderBitmap(int pageNo, float zoom, int rotation, RectD *pageRect, RenderTarget target, AbortCookie **cookie_out)
{
    UNUSED(target);
    xps_page* page = GetXpsPage(pageNo);
    if (!page)
        return nullptr;

    fz_rect pRect;
    if (pageRect)
        pRect = fz_RectD_to_rect(*pageRect);
    else
        xps_bound_page(_doc, page, &pRect);
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_rect r = pRect;
    fz_irect bbox;
    fz_round_rect(&bbox, fz_transform_rect(&r, &ctm));

    fz_pixmap *image = nullptr;
    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        fz_colorspace *colorspace = fz_device_rgb(ctx);
        image = fz_new_pixmap_with_bbox(ctx, colorspace, &bbox);
        fz_clear_pixmap_with_value(ctx, image, 0xFF); // initialize white background
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }

    fz_device *dev = nullptr;
    fz_try(ctx) {
        dev = fz_new_draw_device(ctx, image);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, image);
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    FitzAbortCookie *cookie = nullptr;
    if (cookie_out)
        *cookie_out = cookie = new FitzAbortCookie();
    fz_rect cliprect;
    bool ok = RunPage(page, dev, &ctm, fz_rect_from_irect(&cliprect, &bbox), true, cookie);

    ScopedCritSec scope(&ctxAccess);

    RenderedBitmap *bitmap = nullptr;
    if (ok)
        bitmap = new_rendered_fz_pixmap(ctx, image);
    fz_drop_pixmap(ctx, image);
    return bitmap;
}

WCHAR *XpsEngineImpl::ExtractPageText(xps_page *page, const WCHAR *lineSep, RectI **coordsOut, bool cacheRun)
{
    if (!page)
        return nullptr;

    fz_text_sheet *sheet = nullptr;
    fz_text_page *text = nullptr;
    fz_device *dev = nullptr;
    fz_var(sheet);
    fz_var(text);

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        sheet = fz_new_text_sheet(ctx);
        text = fz_new_text_page(ctx);
        dev = fz_new_text_device(ctx, sheet, text);
    }
    fz_catch(ctx) {
        fz_free_text_page(ctx, text);
        fz_free_text_sheet(ctx, sheet);
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    if (!cacheRun)
        fz_enable_device_hints(dev, FZ_NO_CACHE);

    // use an infinite rectangle as bounds (instead of a mediabox) to ensure that
    // the extracted text is consistent between cached runs using a list device and
    // fresh runs (otherwise the list device omits text outside the mediabox bounds)
    RunPage(page, dev, &fz_identity, nullptr, cacheRun);

    ScopedCritSec scope(&ctxAccess);

    WCHAR *content = fz_text_page_to_str(text, lineSep, coordsOut);
    fz_free_text_page(ctx, text);
    fz_free_text_sheet(ctx, sheet);

    return content;
}

unsigned char *XpsEngineImpl::GetFileData(size_t *cbCount)
{
    unsigned char *data = nullptr;
    ScopedCritSec scope(&ctxAccess);
    fz_try(ctx) {
        data = fz_extract_stream_data(_docStream, cbCount);
    }
    fz_catch(ctx) {
        data = nullptr;
        if (FileName()) {
            data = (unsigned char *) file::ReadAll(FileName(), cbCount);
        }
    }
    return data;
}

bool XpsEngineImpl::SaveFileAs(const char *copyFileName, bool includeUserAnnots)
{
    UNUSED(includeUserAnnots);
    size_t dataLen;
    AutoFreeW dstPath(str::conv::FromUtf8(copyFileName));
    ScopedMem<unsigned char> data(GetFileData(&dataLen));
    if (data) {
        bool ok = file::WriteAll(dstPath, data.Get(), dataLen);
        if (ok)
            return true;
    }
    if (!FileName())
        return false;
    return CopyFileW(FileName(), dstPath, FALSE);
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
        AutoFreeW path(str::conv::FromUtf8(font->name));
        AutoFreeW name(str::conv::FromUtf8(font->font->name));
        fonts.Append(str::Format(L"%s (%s)", name.Get(), path::GetBaseName(path)));
    }
    if (fonts.Count() == 0)
        return nullptr;

    fonts.SortNatural();
    return fonts.Join(L"\n");
}

WCHAR *XpsEngineImpl::GetProperty(DocumentProperty prop)
{
    if (Prop_FontList == prop)
        return ExtractFontList();
    if (!_info)
        return nullptr;

    switch (prop) {
    case Prop_Title: return str::Dup(_info->title);
    case Prop_Author: return str::Dup(_info->author);
    case Prop_Subject: return str::Dup(_info->subject);
    case Prop_CreationDate: return str::Dup(_info->creation_date);
    case Prop_ModificationDate: return str::Dup(_info->modification_date);
    default: return nullptr;
    }
};

bool XpsEngineImpl::SupportsAnnotation(bool forSaving) const
{
    return !forSaving; // for now
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
        return nullptr;

    fz_point p = { (float)pt.x, (float)pt.y };
    for (fz_link *link = page->links; link; link = link->next)
        if (fz_is_pt_in_rect(link->rect, p))
            return new XpsLink(this, &link->dest, link->rect, pageNo);

    if (imageRects[pageNo-1]) {
        for (int i = 0; !fz_is_empty_rect(&imageRects[pageNo-1][i]); i++)
            if (fz_is_pt_in_rect(imageRects[pageNo-1][i], p))
                return new XpsImage(this, pageNo, imageRects[pageNo-1][i], i);
    }

    return nullptr;
}

Vec<PageElement *> *XpsEngineImpl::GetElements(int pageNo)
{
    xps_page *page = GetXpsPage(pageNo, true);
    if (!page)
        return nullptr;

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    Vec<PageElement *> *els = new Vec<PageElement *>();
    if (!els)
        return nullptr;

    if (imageRects[pageNo-1]) {
        for (int i = 0; !fz_is_empty_rect(&imageRects[pageNo-1][i]); i++) {
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
    UNUSED(pageNo);
    // make MuXPS extract all links and named destinations from the page
    AssertCrash(!GetPageRun(page, true));
    XpsPageRun *run = GetPageRun(page);
    AssertCrash(!run == !page->links_resolved);
    if (run)
        DropPageRun(run);
    else
        page->links_resolved = 1;
    AssertCrash(!page->links || page->links->refs == 1);

    RectI *coords;
    AutoFreeW pageText(ExtractPageText(page, L"\n", &coords, true));
    if (!pageText)
        return;

    LinkRectList *list = LinkifyText(pageText, coords);
    for (size_t i = 0; i < list->links.Count(); i++) {
        bool overlaps = false;
        for (fz_link *next = page->links; next && !overlaps; next = next->next)
            overlaps = fz_calc_overlap(list->coords.at(i), next->rect) >= 0.25f;
        if (!overlaps) {
            AutoFree uri(str::conv::ToUtf8(list->links.at(i)));
            if (!uri) continue;
            fz_link_dest ld = { FZ_LINK_URI, 0 };
            ld.ld.uri.uri = fz_strdup(ctx, uri);
            // add links in top-to-bottom order (i.e. last-to-first)
            fz_link *link = fz_new_link(ctx, &list->coords.at(i), ld);
            CrashIf(!link); // TODO: if fz_new_link throws, there are memory leaks
            link->next = page->links;
            page->links = link;
        }
    }

    delete list;
    free(coords);
}

RenderedBitmap *XpsEngineImpl::GetPageImage(int pageNo, RectD rect, size_t imageIx)
{
    xps_page *page = GetXpsPage(pageNo);
    if (!page)
        return nullptr;

    Vec<FitzImagePos> positions;
    ListInspectionData data(positions);
    fz_device *dev = nullptr;

    EnterCriticalSection(&ctxAccess);
    fz_try(ctx) {
        dev = fz_new_inspection_device(ctx, &data);
    }
    fz_catch(ctx) {
        LeaveCriticalSection(&ctxAccess);
        return nullptr;
    }
    LeaveCriticalSection(&ctxAccess);

    RunPage(page, dev, &fz_identity);

    if (imageIx >= positions.Count() || fz_rect_to_RectD(positions.at(imageIx).rect) != rect) {
        AssertCrash(0);
        return nullptr;
    }

    ScopedCritSec scope(&ctxAccess);

    fz_pixmap *pixmap = nullptr;
    fz_try(ctx) {
        fz_image *image = positions.at(imageIx).image;
        pixmap = fz_new_pixmap_from_image(ctx, image, image->w, image->h);
    }
    fz_catch(ctx) {
        return nullptr;
    }
    RenderedBitmap *bmp = new_rendered_fz_pixmap(ctx, pixmap);
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
    if (fz_is_empty_rect(&found->rect)) {
        // ensure that the target rectangle could have been
        // updated through LinkifyPageText -> xps_extract_anchor_info
        GetXpsPage(found->page + 1);
    }
    return found->rect;
}

PageDestination *XpsEngineImpl::GetNamedDest(const WCHAR *name)
{
    AutoFree name_utf8(str::conv::ToUtf8(name));
    if (!str::StartsWith(name_utf8.Get(), "#"))
        name_utf8.Set(str::Join("#", name_utf8));

    for (xps_target *dest = _doc->target; dest; dest = dest->next)
        if (str::EndsWithI(dest->name, name_utf8))
            return new SimpleDest(dest->page + 1, fz_rect_to_RectD(dest->rect));

    return nullptr;
}

XpsTocItem *XpsEngineImpl::BuildTocTree(fz_outline *entry, int& idCounter)
{
    XpsTocItem *node = nullptr;

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
        return nullptr;

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
    for (int i = 0; !fz_is_empty_rect(&imageRects[pageNo-1][i]); i++)
        if (fz_calc_overlap(mbox, imageRects[pageNo-1][i]) >= 0.9f)
            return false;
    return true;
}

BaseEngine *XpsEngineImpl::CreateFromFile(const WCHAR *fileName)
{
    XpsEngineImpl *engine = new XpsEngineImpl();
    if (!engine || !fileName || !engine->Load(fileName)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

BaseEngine *XpsEngineImpl::CreateFromStream(IStream *stream)
{
    XpsEngineImpl *engine = new XpsEngineImpl();
    if (!engine->Load(stream)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

namespace XpsEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff)
{
    if (sniff) {
        if (dir::Exists(fileName)) {
            // allow opening uncompressed XPS files as well
            AutoFreeW relsPath(path::Join(fileName, L"_rels\\.rels"));
            return file::Exists(relsPath) || dir::Exists(relsPath);
        }
        ZipFile zip(fileName, true);
        return zip.GetFileIndex(L"_rels/.rels") != (size_t)-1 ||
               zip.GetFileIndex(L"_rels/.rels/[0].piece") != (size_t)-1 ||
               zip.GetFileIndex(L"_rels/.rels/[0].last.piece") != (size_t)-1;
    }

    return str::EndsWithI(fileName, L".xps") || str::EndsWithI(fileName, L".oxps");
}

BaseEngine *CreateFromFile(const WCHAR *fileName)
{
    return XpsEngineImpl::CreateFromFile(fileName);
}

BaseEngine *CreateFromStream(IStream *stream)
{
    return XpsEngineImpl::CreateFromStream(stream);
}

}
