/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include "../mupdf/source/fitz/color-imp.h"
}

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/GuessFileType.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"
#include "utils/Timer.h"

#include "wingui/UIModels.h"

#include "AppColors.h"
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "EngineAll.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "SumatraConfig.h"
#include "Settings.h"
#include "GlobalPrefs.h"

#include "utils/Log.h"

// A5
static float layoutA5DxPt = 420.f;
static float layoutA5DyPt = 595.f;

// A4
static float layoutA4DxPt = 595.f;
static float layoutA4DyPt = 842.f;

static float layoutFontEm = 11.f;

// maximum size of a file that's entirely loaded into memory before parsed
// and displayed; larger files will be kept open while they're displayed
// so that their content can be loaded on demand in order to preserve memory
constexpr i64 kMaxMemoryFileSize = 32 * 1024 * 1024;

// in mupdf_load_system_font.c
extern "C" void drop_cached_fonts_for_ctx(fz_context*);
extern "C" void pdf_install_load_system_font_funcs(fz_context* ctx);

static AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp) {
    return (AnnotationType)tp;
}

Kind kindEngineMupdf = "enginePdf";

EngineMupdf* AsEngineMupdf(EngineBase* engine) {
    if (!engine || !IsOfKind(engine, kindEngineMupdf)) {
        return nullptr;
    }
    return (EngineMupdf*)engine;
}

class FitzAbortCookie : public AbortCookie {
  public:
    fz_cookie cookie;
    FitzAbortCookie() {
        memset(&cookie, 0, sizeof(cookie));
    }
    void Abort() override {
        cookie.abort = 1;
    }
};

// copy of fz_is_external_link without ctx
static bool IsExternalLink(const char* uri) {
    if (!uri) {
        return false;
    }
    while (*uri >= 'a' && *uri <= 'z') {
        ++uri;
    }
    return uri[0] == ':';
}

static char* FzGetURL(fz_link* link, fz_outline* outline) {
    if (link) {
        return link->uri;
    }
    return outline->uri;
}

struct PageDestinationMupdf : IPageDestination {
    fz_outline* outline = nullptr;
    fz_link* link = nullptr;

    char* value = nullptr;
    char* name = nullptr;

    PageDestinationMupdf(fz_link* l, fz_outline* o) {
        // exactly one must be provided
        kind = kindDestinationMupdf;
        link = l;
        outline = o;
    }
    ~PageDestinationMupdf() override {
        str::Free(value);
        str::Free(name);
    }

    char* GetValue() override;
    char* GetName() override;
};

char* PageDestinationMupdf ::GetValue() {
    if (value) {
        return value;
    }

    char* uri = FzGetURL(link, outline);
    if (uri && IsExternalLink(uri)) {
        value = str::Dup(uri);
    }
    return value;
}

char* PageDestinationMupdf ::GetName() {
    if (name) {
        return name;
    }
    if (outline && outline->title) {
        name = str::Dup(outline->title);
    }
    return name;
}

static NO_INLINE RectF FzGetRectF(fz_link* link, fz_outline* outline) {
    if (link) {
        return ToRectF(link->rect);
    }
    return {};
}

static int ResolveLink(fz_context* ctx, fz_document* doc, const char* uri, float* xp, float* yp) {
    if (!uri) {
        return -1;
    }
    int pageNo = -1;
    fz_location loc;

    fz_var(loc);
    fz_var(pageNo);
    fz_try(ctx) {
        loc = fz_resolve_link(ctx, doc, uri, xp, yp);
        pageNo = fz_page_number_from_location(ctx, doc, loc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "fz_resolve_link failed");
        pageNo = -1;
    }

    return pageNo + 1;
}

static int FzGetPageNo(fz_context* ctx, fz_document* doc, fz_link* link, fz_outline* outline) {
    float x, y;
    const char* uri = link ? link->uri : outline ? outline->uri : nullptr;
    int pageNo = ResolveLink(ctx, doc, uri, &x, &y);
    return pageNo;
}

static IPageDestination* NewPageDestinationMupdf(fz_context* ctx, fz_document* doc, fz_link* link,
                                                 fz_outline* outline) {
    CrashIf(link && outline);
    CrashIf(!link && !outline);
    char* uri = FzGetURL(link, outline);

    if (IsExternalUrl(uri)) {
        auto res = new PageDestinationURL(uri);
        res->rect = FzGetRectF(link, outline);
        return res;
    }

    if (str::StartsWithI(uri, "file://")) {
        TempStr path = CleanupFileURLTemp(uri);
        char* frag = str::FindChar(uri, '#');
        if (frag) {
            frag++;
        }
        auto res = new PageDestinationFile(path, frag);
        res->rect = FzGetRectF(link, outline);
        return res;
    }

    auto dest = new PageDestinationMupdf(link, outline);
    dest->rect = FzGetRectF(link, outline);
    dest->pageNo = FzGetPageNo(ctx, doc, link, outline);
    return dest;
}

static PageElementDestination* NewLinkDestination(int srcPageNo, fz_context* ctx, fz_document* doc, fz_link* link,
                                                  fz_outline* outline) {
    auto dest = NewPageDestinationMupdf(ctx, doc, link, outline);
    auto res = new PageElementDestination(dest);
    res->pageNo = srcPageNo;
    res->rect = dest->rect;
    return res;
}

struct LinkRectList {
    StrVec links;
    Vec<fz_rect> coords;
};

fz_rect ToFzRect(RectF rect) {
    fz_rect result = {(float)rect.x, (float)rect.y, (float)(rect.x + rect.dx), (float)(rect.y + rect.dy)};
    return result;
}

RectF ToRectF(fz_rect rect) {
    return RectF::FromXY(rect.x0, rect.y0, rect.x1, rect.y1);
}

static bool IsPointInRect(fz_rect rect, fz_point pt) {
    return ToRectF(rect).Contains(PointF(pt.x, pt.y));
}

fz_matrix FzCreateViewCtm(fz_rect mediabox, float zoom, int rotation) {
    fz_matrix ctm = fz_pre_scale(fz_rotate((float)rotation), zoom, zoom);

    CrashIf(0 != mediabox.x0 || 0 != mediabox.y0);
    rotation = (rotation + 360) % 360;
    if (90 == rotation) {
        ctm = fz_pre_translate(ctm, 0, -mediabox.y1);
    } else if (180 == rotation) {
        ctm = fz_pre_translate(ctm, -mediabox.x1, -mediabox.y1);
    } else if (270 == rotation) {
        ctm = fz_pre_translate(ctm, -mediabox.x1, 0);
    }

    CrashIf(fz_matrix_expansion(ctm) <= 0);
    if (fz_matrix_expansion(ctm) == 0) {
        return fz_identity;
    }

    return ctm;
}

// TODO: maybe make dpi a float as well
static float DpiScale(float x, int dpi) {
    CrashIf(dpi < 70.f);
    // TODO: maybe implement step scaling like mupdf
    float res = x * (float)dpi;
    res = res / 96.f;
    return res;
}

static float FzRectOverlap(fz_rect r1, fz_rect r2) {
    if (fz_is_empty_rect(r1)) {
        return 0.0f;
    }
    fz_rect isect = fz_intersect_rect(r1, r2);
    return (isect.x1 - isect.x0) * (isect.y1 - isect.y0) / ((r1.x1 - r1.x0) * (r1.y1 - r1.y0));
}

static float FzRectOverlap(fz_rect r1, RectF r2f) {
    if (fz_is_empty_rect(r1)) {
        return 0.0f;
    }
    fz_rect r2 = ToFzRect(r2f);
    fz_rect isect = fz_intersect_rect(r1, r2);
    return (isect.x1 - isect.x0) * (isect.y1 - isect.y0) / ((r1.x1 - r1.x0) * (r1.y1 - r1.y0));
}

static WCHAR* PdfToWstr(fz_context* ctx, pdf_obj* obj) {
    char* s = pdf_new_utf8_from_pdf_string_obj(ctx, obj);
    WCHAR* res = ToWstr(s);
    fz_free(ctx, s);
    return res;
}

static char* PdfToUtf8(fz_context* ctx, pdf_obj* obj) {
    char* s = pdf_new_utf8_from_pdf_string_obj(ctx, obj);
    char* res = str::Dup(s);
    fz_free(ctx, s);
    return res;
}

// some PDF documents contain control characters in outline titles or /Info properties
// we replace them with spaces and cleanup for display with NormalizeWSInPlace()
static WCHAR* PdfCleanStringInPlace(WCHAR* s) {
    if (!s) {
        return nullptr;
    }
    WCHAR* curr = s;
    while (*curr) {
        WCHAR c = *curr;
        if (c < 0x20) {
            *curr = ' ';
        }
        curr++;
    }
    str::NormalizeWSInPlace(s);
    return s;
}

struct istream_filter {
    IStream* stream;
    u8 buf[4096];
};

extern "C" int next_istream(fz_context* ctx, fz_stream* stm, size_t) {
    istream_filter* state = (istream_filter*)stm->state;
    ULONG cbRead = sizeof(state->buf);
    HRESULT res = state->stream->Read(state->buf, sizeof(state->buf), &cbRead);
    if (FAILED(res)) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "IStream read error: %x", res);
    }
    stm->rp = state->buf;
    stm->wp = stm->rp + cbRead;
    stm->pos += cbRead;

    return cbRead > 0 ? *stm->rp++ : EOF;
}

extern "C" void seek_istream(fz_context* ctx, fz_stream* stm, i64 offset, int whence) {
    istream_filter* state = (istream_filter*)stm->state;
    LARGE_INTEGER off;
    ULARGE_INTEGER n;
    off.QuadPart = offset;
    HRESULT res = state->stream->Seek(off, whence, &n);
    if (FAILED(res)) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "IStream seek error: %x", res);
    }
    if (n.HighPart != 0 || n.LowPart > INT_MAX) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "documents beyond 2GB aren't supported");
    }
    stm->pos = n.LowPart;
    stm->rp = stm->wp = state->buf;
}

extern "C" void drop_istream(fz_context* ctx, void* state_) {
    istream_filter* state = (istream_filter*)state_;
    state->stream->Release();
    fz_free(ctx, state);
}

static fz_stream* FzOpenIStream(fz_context* ctx, IStream* stream) {
    if (!stream) {
        return nullptr;
    }

    LARGE_INTEGER zero{};
    HRESULT res = stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    if (FAILED(res)) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "IStream seek error: %x", res);
    }

    istream_filter* state = fz_malloc_struct(ctx, istream_filter);
    state->stream = stream;
    stream->AddRef();

    fz_stream* stm = fz_new_stream(ctx, state, next_istream, drop_istream);
    stm->seek = seek_istream;
    return stm;
}

static void* FzMemdup(fz_context* ctx, void* p, size_t size) {
    void* res = fz_malloc_no_throw(ctx, size);
    if (!res) {
        return nullptr;
    }
    memcpy(res, p, size);
    return res;
}

static fz_stream* FzOpenFile2(fz_context* ctx, const char* path) {
    fz_stream* stm = nullptr;
    i64 fileSize = file::GetSize(path);
    // load small files entirely into memory so that they can be
    // overwritten even by programs that don't open files with FILE_SHARE_READ
    if (fileSize > 0 && fileSize < kMaxMemoryFileSize) {
        ByteSlice dataTmp = file::ReadFile(path);
        if (dataTmp.empty()) {
            // failed to read
            return nullptr;
        }

        // TODO: we copy so that the memory ends up in chunk allocated
        // by libmupdf so that it works across dll boundaries.
        // We can either use  fz_new_buffer_from_shared_data
        // and free the data on the side or create Allocator that
        // uses fz_malloc_no_throw and pass it to ReadFileWithAllocator
        size_t size = dataTmp.size();
        void* data = FzMemdup(ctx, (void*)dataTmp.data(), size);
        if (!data) {
            return nullptr;
        }
        dataTmp.Free();

        fz_buffer* buf = fz_new_buffer_from_data(ctx, (u8*)data, size);
        fz_var(buf);
        fz_try(ctx) {
            stm = fz_open_buffer(ctx, buf);
        }
        fz_always(ctx) {
            fz_drop_buffer(ctx, buf);
        }
        fz_catch(ctx) {
            stm = nullptr;
        }
        return stm;
    }

    WCHAR* pathW = ToWstrTemp(path);
    fz_try(ctx) {
        stm = fz_open_file_w(ctx, pathW);
    }
    fz_catch(ctx) {
        stm = nullptr;
    }
    return stm;
}

static void FzStreamFingerprint(fz_context* ctx, fz_stream* stm, u8 digest[16]) {
    i64 fileLen = -1;
    fz_buffer* buf = nullptr;

    fz_try(ctx) {
        fz_seek(ctx, stm, 0, 2);
        fileLen = fz_tell(ctx, stm);
        fz_seek(ctx, stm, 0, 0);
        buf = fz_read_all(ctx, stm, fileLen);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "couldn't read stream data, using a nullptr fingerprint instead");
        ZeroMemory(digest, 16);
        return;
    }
    CrashIf(nullptr == buf);
    u8* data;
    size_t size = fz_buffer_extract(ctx, buf, &data);
    CrashIf((size_t)fileLen != size);
    fz_drop_buffer(ctx, buf);

    fz_md5 md5;
    fz_md5_init(&md5);
    fz_md5_update(&md5, data, size);
    fz_md5_final(&md5, digest);
}

static ByteSlice FzExtractStreamData(fz_context* ctx, fz_stream* stream) {
    fz_seek(ctx, stream, 0, 2);
    i64 fileLen = fz_tell(ctx, stream);
    fz_seek(ctx, stream, 0, 0);

    fz_buffer* buf = fz_read_all(ctx, stream, fileLen);

    u8* data = nullptr;
    size_t size = fz_buffer_extract(ctx, buf, &data);
    CrashIf((size_t)fileLen != size);
    fz_drop_buffer(ctx, buf);
    if (!data || size == 0) {
        return {};
    }
    // this was allocated inside mupdf, make a copy that can be free()d
    u8* res = (u8*)memdup(data, size);
    fz_free(ctx, data);
    return {res, size};
}

static inline int WcharsPerRune(int rune) {
    if (rune & 0x1F0000) {
        return 2;
    }
    return 1;
}

static void AddChar(fz_stext_line* line, fz_stext_char* c, str::WStr& s, Vec<Rect>& rects) {
    fz_rect bbox = fz_rect_from_quad(c->quad);
    Rect r = ToRectF(bbox).Round();

    int n = WcharsPerRune(c->c);
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
        s.AppendChar(wc);
        rects.Append(r);
        return;
    }

    // non-printable or whitespace
    if (!str::IsWs(wc)) {
        s.AppendChar(L'?');
        rects.Append(r);
        return;
    }

    // collapse multiple whitespace characters into one
    WCHAR prev = s.LastChar();
    if (!str::IsWs(prev)) {
        s.AppendChar(L' ');
        rects.Append(r);
    }
}

static void AddLineSep(str::WStr& s, Vec<Rect>& rects, const WCHAR* lineSep, size_t lineSepLen) {
    if (lineSepLen == 0) {
        return;
    }
    // remove trailing spaces
    if (str::IsWs(s.LastChar())) {
        s.RemoveLast();
        rects.RemoveLast();
    }

    s.Append(lineSep);
    for (size_t i = 0; i < lineSepLen; i++) {
        rects.Append(Rect());
    }
}

static WCHAR* FzTextPageToStr(fz_stext_page* text, Rect** coordsOut) {
    const WCHAR* lineSep = L"\n";

    size_t lineSepLen = str::Len(lineSep);
    str::WStr content;
    // coordsOut is optional but we ask for it by default so we simplify the code
    // by always calculating it
    Vec<Rect> rects;

    fz_stext_block* block = text->first_block;
    while (block) {
        if (block->type != FZ_STEXT_BLOCK_TEXT) {
            block = block->next;
            continue;
        }
        fz_stext_line* line = block->u.t.first_line;
        while (line) {
            fz_stext_char* c = line->first_char;
            while (c) {
                AddChar(line, c, content, rects);
                c = c->next;
            }
            AddLineSep(content, rects, lineSep, lineSepLen);
            line = line->next;
        }

        block = block->next;
    }

    CrashIf(content.size() != rects.size());

    if (coordsOut) {
        *coordsOut = rects.StealData();
    }

    return content.StealData();
}

static bool LinkifyCheckMultiline(const WCHAR* pageText, const WCHAR* pos, Rect* coords) {
    // multiline links end in a non-alphanumeric character and continue on a line
    // that starts left and only slightly below where the current line ended
    // (and that doesn't start with http or a footnote numeral)
    return '\n' == *pos && pos > pageText && *(pos + 1) && !iswalnum(pos[-1]) && !str::IsWs(pos[1]) &&
           coords[pos - pageText + 1].BR().y > coords[pos - pageText - 1].y &&
           coords[pos - pageText + 1].y <= coords[pos - pageText - 1].BR().y + coords[pos - pageText - 1].dy * 0.35 &&
           coords[pos - pageText + 1].x < coords[pos - pageText - 1].BR().x &&
           coords[pos - pageText + 1].dy >= coords[pos - pageText - 1].dy * 0.85 &&
           coords[pos - pageText + 1].dy <= coords[pos - pageText - 1].dy * 1.2 && !str::StartsWith(pos + 1, L"http");
}

static bool EndsURL(WCHAR c) {
    if (c == 0 || str::IsWs(c)) {
        return true;
    }
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1313
    // 0xff0c is ","
    if (c == (WCHAR)0xff0c) {
        return true;
    }
    return false;
}

static const WCHAR* LinkifyFindEnd(const WCHAR* start, WCHAR prevChar) {
    const WCHAR* quote = nullptr;

    // look for the end of the URL (ends in a space preceded maybe by interpunctuation)
    const WCHAR* end = start;
    while (!EndsURL(*end)) {
        end++;
    }
    char prev = 0;
    if (end > start) {
        prev = end[-1];
    }
    if (',' == prev || '.' == prev || '?' == prev || '!' == prev) {
        end--;
    }

    prev = 0;
    if (end > start) {
        prev = end[-1];
    }
    // also ignore a closing parenthesis, if the URL doesn't contain any opening one
    if (')' == prev && (!str::FindChar(start, '(') || str::FindChar(start, '(') >= end)) {
        end--;
    }

    // cut the link at the first quotation mark, if it's also preceded by one
    if (('"' == prevChar || '\'' == prevChar) && (quote = str::FindChar(start, prevChar)) != nullptr && quote < end) {
        end = quote;
    }

    return end;
}

static const WCHAR* LinkifyMultilineText(LinkRectList* list, const WCHAR* pageText, const WCHAR* start,
                                         const WCHAR* next, Rect* coords) {
    int lastIx = list->coords.Size() - 1;
    char* uri = list->links.at(lastIx);
    const WCHAR* end = next;
    bool multiline = false;

    do {
        end = LinkifyFindEnd(next, start > pageText ? start[-1] : ' ');
        multiline = LinkifyCheckMultiline(pageText, end, coords);

        char* part = ToUtf8Temp(next, end - next);
        uri = str::JoinTemp(uri, part);
        Rect bbox = coords[next - pageText].Union(coords[end - pageText - 1]);
        list->coords.Append(ToFzRect(ToRectF(bbox)));

        next = end + 1;
    } while (multiline);

    // update the link URL for all partial links
    list->links.SetAt(lastIx, uri);
    for (int i = lastIx + 1; i < list->coords.Size(); i++) {
        list->links.Append(uri);
    }

    return end;
}

// cf. http://weblogs.mozillazine.org/gerv/archives/2011/05/html5_email_address_regexp.html
inline bool IsEmailUsernameChar(WCHAR c) {
    // explicitly excluding the '/' from the list, as it is more
    // often part of a URL or path than of an email address
    return iswalnum(c) || c && str::FindChar(L".!#$%&'*+=?^_`{|}~-", c);
}
inline bool IsEmailDomainChar(WCHAR c) {
    return iswalnum(c) || '-' == c;
}

static const WCHAR* LinkifyFindEmail(const WCHAR* pageText, const WCHAR* at) {
    const WCHAR* start;
    for (start = at; start > pageText && IsEmailUsernameChar(*(start - 1)); start--) {
        // do nothing
    }
    return start != at ? start : nullptr;
}

static const WCHAR* LinkifyEmailAddress(const WCHAR* start) {
    const WCHAR* end;
    for (end = start; IsEmailUsernameChar(*end); end++) {
        ;
    }
    if (end == start || *end != '@' || !IsEmailDomainChar(*(end + 1))) {
        return nullptr;
    }
    for (end++; IsEmailDomainChar(*end); end++) {
        ;
    }
    if ('.' != *end || !IsEmailDomainChar(*(end + 1))) {
        return nullptr;
    }
    do {
        for (end++; IsEmailDomainChar(*end); end++) {
            ;
        }
    } while ('.' == *end && IsEmailDomainChar(*(end + 1)));
    return end;
}

// caller needs to delete the result
// TODO: return Vec<IPageElement*> directly
static LinkRectList* LinkifyText(const WCHAR* pageText, Rect* coords) {
    LinkRectList* list = new LinkRectList;

    for (const WCHAR* start = pageText; *start; start++) {
        const WCHAR* end = nullptr;
        bool multiline = false;
        const WCHAR* protocol = nullptr;

        if ('@' == *start) {
            // potential email address without mailto:
            const WCHAR* email = LinkifyFindEmail(pageText, start);
            end = email ? LinkifyEmailAddress(email) : nullptr;
            protocol = L"mailto:";
            if (end != nullptr) {
                start = email;
            }
        } else if (start > pageText && ('/' == start[-1] || iswalnum(start[-1]))) {
            // hyperlinks must not be preceded by a slash (indicates a different protocol)
            // or an alphanumeric character (indicates part of a different protocol)
        } else if ('h' == *start && str::Parse(start, L"http%?s://")) {
            end = LinkifyFindEnd(start, start > pageText ? start[-1] : ' ');
            multiline = LinkifyCheckMultiline(pageText, end, coords);
        } else if ('w' == *start && str::StartsWith(start, L"www.")) {
            end = LinkifyFindEnd(start, start > pageText ? start[-1] : ' ');
            multiline = LinkifyCheckMultiline(pageText, end, coords);
            protocol = L"http://";
            // ignore www. links without a top-level domain
            if (end - start <= 4 || !multiline && (!wcschr(start + 5, '.') || wcschr(start + 5, '.') >= end)) {
                end = nullptr;
            }
        } else if ('m' == *start && str::StartsWith(start, L"mailto:")) {
            end = LinkifyEmailAddress(start + 7);
        }
        if (!end) {
            continue;
        }

        char* part = ToUtf8Temp(start, end - start);
        char* uri = part;
        if (protocol) {
            char* proto = ToUtf8Temp(protocol);
            uri = str::JoinTemp(proto, part);
        }
        list->links.Append(uri);
        Rect bbox = coords[start - pageText].Union(coords[end - pageText - 1]);
        list->coords.Append(ToFzRect(ToRectF(bbox)));
        if (multiline) {
            end = LinkifyMultilineText(list, pageText, start, end + 1, coords);
        }

        start = end;
    }

    return list;
}

// try to produce an 8-bit palette for saving some memory
static RenderedBitmap* TryRenderAsPaletteImage(fz_pixmap* pixmap) {
    int w = pixmap->w;
    int h = pixmap->h;
    int rows8 = ((w + 3) / 4) * 4;
    u8* bmpData = (u8*)calloc(rows8, h);
    if (!bmpData) {
        return nullptr;
    }

    ScopedMem<BITMAPINFO> bmi((BITMAPINFO*)calloc(1, sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD)));

    u8* dest = bmpData;
    u8* source = pixmap->samples;
    u32* palette = (u32*)bmi.Get()->bmiColors;
    u8 grayIdxs[256]{};

    int paletteSize = 0;
    RGBQUAD c;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            c.rgbRed = *source++;
            c.rgbGreen = *source++;
            c.rgbBlue = *source++;
            c.rgbReserved = 0;
            source++;

            /* find this color in the palette */
            int k;
            bool isGray = c.rgbRed == c.rgbGreen && c.rgbRed == c.rgbBlue;
            if (isGray) {
                k = grayIdxs[c.rgbRed] || palette[0] == *(u32*)&c ? grayIdxs[c.rgbRed] : paletteSize;
            } else {
                for (k = 0; k < paletteSize && palette[k] != *(u32*)&c; k++) {
                    ;
                }
            }
            /* add it to the palette if it isn't in there and if there's still space left */
            if (k == paletteSize) {
                if (++paletteSize > 256) {
                    free(bmpData);
                    return nullptr;
                }
                if (isGray) {
                    grayIdxs[c.rgbRed] = (BYTE)k;
                }
                palette[k] = *(u32*)&c;
            }
            /* 8-bit data consists of indices into the color palette */
            *dest++ = k;
        }
        dest += rows8 - w;
    }

    BITMAPINFOHEADER* bmih = &bmi.Get()->bmiHeader;
    bmih->biSize = sizeof(*bmih);
    bmih->biWidth = w;
    bmih->biHeight = -h;
    bmih->biPlanes = 1;
    bmih->biCompression = BI_RGB;
    bmih->biBitCount = 8;
    bmih->biSizeImage = h * rows8;
    bmih->biClrUsed = paletteSize;

    void* data = nullptr;
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bmih->biSizeImage, nullptr);
    HBITMAP hbmp = CreateDIBSection(nullptr, bmi, DIB_RGB_COLORS, &data, hMap, 0);
    if (!hbmp) {
        free(bmpData);
        return nullptr;
    }
    memcpy(data, bmpData, bmih->biSizeImage);
    free(bmpData);
    return new RenderedBitmap(hbmp, Size(w, h), hMap);
}

// had to create a copy of fz_convert_pixmap to ensure we always get the alpha
static fz_pixmap* FzConvertPixmap2(fz_context* ctx, fz_pixmap* pix, fz_colorspace* ds, fz_colorspace* prf,
                                   fz_default_colorspaces* default_cs, fz_color_params color_params, int keep_alpha) {
    fz_pixmap* cvt;

    if (!ds && !keep_alpha) {
        fz_throw(ctx, FZ_ERROR_GENERIC, "cannot both throw away and keep alpha");
    }

    cvt = fz_new_pixmap(ctx, ds, pix->w, pix->h, pix->seps, keep_alpha);

    cvt->xres = pix->xres;
    cvt->yres = pix->yres;
    cvt->x = pix->x;
    cvt->y = pix->y;
    if (pix->flags & FZ_PIXMAP_FLAG_INTERPOLATE) {
        cvt->flags |= FZ_PIXMAP_FLAG_INTERPOLATE;
    } else {
        cvt->flags &= ~FZ_PIXMAP_FLAG_INTERPOLATE;
    }

    fz_try(ctx) {
        fz_convert_pixmap_samples(ctx, pix, cvt, prf, default_cs, color_params, 1);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, cvt);
        fz_rethrow(ctx);
    }

    return cvt;
}

RenderedBitmap* NewRenderedFzPixmap(fz_context* ctx, fz_pixmap* pixmap) {
    if (pixmap->n == 4 && fz_colorspace_is_rgb(ctx, pixmap->colorspace)) {
        RenderedBitmap* res = TryRenderAsPaletteImage(pixmap);
        if (res) {
            return res;
        }
    }

    ScopedMem<BITMAPINFO> bmi((BITMAPINFO*)calloc(1, sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD)));

    fz_pixmap* bgrPixmap = nullptr;
    fz_colorspace* csdest = nullptr;
    fz_color_params cp;

    fz_var(bgrPixmap);
    fz_var(csdest);
    fz_var(cp);

    /* BGRA is a GDI compatible format */
    fz_try(ctx) {
        csdest = fz_device_bgr(ctx);
        cp = fz_default_color_params;
        bgrPixmap = FzConvertPixmap2(ctx, pixmap, csdest, nullptr, nullptr, cp, 1);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    if (!bgrPixmap || !bgrPixmap->samples) {
        return nullptr;
    }

    int w = bgrPixmap->w;
    int h = bgrPixmap->h;
    int n = bgrPixmap->n;
    int imgSize = bgrPixmap->stride * h;
    int bitsCount = n * 8;

    BITMAPINFOHEADER* bmih = &bmi.Get()->bmiHeader;
    bmih->biSize = sizeof(*bmih);
    bmih->biWidth = w;
    bmih->biHeight = -h;
    bmih->biPlanes = 1;
    bmih->biCompression = BI_RGB;
    bmih->biBitCount = bitsCount;
    bmih->biSizeImage = imgSize;
    bmih->biClrUsed = 0;

    void* data = nullptr;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD fl = PAGE_READWRITE;
    HANDLE hMap = CreateFileMappingW(hFile, nullptr, fl, 0, imgSize, nullptr);
    uint usage = DIB_RGB_COLORS;
    HBITMAP hbmp = CreateDIBSection(nullptr, bmi, usage, &data, hMap, 0);
    if (data) {
        u8* samples = bgrPixmap->samples;
        memcpy(data, samples, imgSize);
    }
    fz_drop_pixmap(ctx, bgrPixmap);
    if (!hbmp) {
        return nullptr;
    }
    // return a RenderedBitmap even if hbmp is nullptr so that callers can
    // distinguish rendering errors from GDI resource exhaustion
    // (and in the latter case retry using smaller target rectangles)
    return new RenderedBitmap(hbmp, Size(w, h), hMap);
}

static TocItem* NewTocItemWithDestination(TocItem* parent, char* title, IPageDestination* dest) {
    auto res = new TocItem(parent, title, 0);
    res->dest = dest;
    return res;
}

// TODO: could be optimized
static bool RectFullyContains(RectF r1, RectF r2) {
    return r1.Contains(r2.TL()) && r1.Contains(r2.BR());
}

// if an elements fully obscures another, remove it from the list
static bool RemoveHeWhoFullyContains(Vec<IPageElement*>& els) {
    int n = els.Size();
    CrashIf(n < 2);
    for (int i = 0; i < n; i++) {
        RectF r1 = els[i]->GetRect();
        for (int j = 0; j < n; j++) {
            if (j == i) {
                continue; // skip checking against self
            }
            auto r2 = els[j]->GetRect();
            if (RectFullyContains(r1, r2)) {
                // logfa("el %d fully obscures %d\n", i, j);
                els.RemoveAtFast(i);
                return true;
            }
        }
    }
    return false;
}

// if we have multiple elements at the same position, pick the one
// that is fully obscured by all other elements
// if not fully obscured, return the first one
static IPageElement* PickBestElement(Vec<IPageElement*>& els) {
    if (els.Size() == 0) {
        return nullptr;
    }

Encore:
    int n = els.Size();
    if (n == 1) {
        return els[0];
    }
    bool didRemove = RemoveHeWhoFullyContains(els);
    if (didRemove) {
        CrashIf(els.Size() != n - 1);
        goto Encore;
    }
    return els[0];
}

// don't delete the result
NO_INLINE static IPageElement* FzGetElementAtPos(FzPageInfo* pageInfo, PointF pt) {
    if (!pageInfo) {
        return nullptr;
    }
    Vec<IPageElement*> res;

    for (auto pel : pageInfo->links) {
        if (pel->GetRect().Contains(pt)) {
            res.Append(pel);
        }
    }

    for (auto* pel : pageInfo->autoLinks) {
        if (pel->GetRect().Contains(pt)) {
            res.Append(pel);
        }
    }

    for (auto* pel : pageInfo->comments) {
        if (pel->GetRect().Contains(pt)) {
            res.Append(pel);
        }
    }

    size_t imageIdx = 0;
    fz_point p = {(float)pt.x, (float)pt.y};
    for (auto& img : pageInfo->images) {
        fz_rect ir = img->rect;
        if (IsPointInRect(ir, p)) {
            res.Append(img->imageElement);
        }
        imageIdx++;
    }

    if (false) {
        int i = 0;
        for (auto&& el : res) {
            Rect r = el->GetRect().Round();
            logfa("el %d: pos: %d-%d, size: %d-%d, kind: %s\n", (int)i, r.x, r.y, r.dx, r.dy, el->GetKind());
            i++;
        }
    }
    return PickBestElement(res);
}

static void BuildElementsInfo(FzPageInfo* pageInfo) {
    if (!pageInfo || !pageInfo->elementsNeedRebuilding) {
        return;
    }
    pageInfo->elementsNeedRebuilding = false;
    auto& els = pageInfo->allElements;
    els.Reset();

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    size_t imageIdx = 0;
    for (auto& img : pageInfo->images) {
        auto image = img->imageElement;
        els.Append(image);
        imageIdx++;
    }

    for (auto& pel : pageInfo->links) {
        els.Append(pel);
    }

    for (auto& pel : pageInfo->autoLinks) {
        els.Append(pel);
    }

    for (auto& comment : pageInfo->comments) {
        els.Append(comment);
    }
    els.Reverse();
}

static void FzLinkifyPageText(FzPageInfo* pageInfo, fz_stext_page* stext) {
    if (!pageInfo || !stext) {
        return;
    }

    Rect* coords;
    WCHAR* pageText = FzTextPageToStr(stext, &coords);
    if (!pageText) {
        return;
    }

    LinkRectList* list = LinkifyText(pageText, coords);
    free(pageText);

    for (size_t i = 0; i < list->links.size(); i++) {
        fz_rect bbox = list->coords.at(i);
        bool overlaps = false;
        for (auto pel : pageInfo->links) {
            overlaps = FzRectOverlap(bbox, pel->GetRect()) >= 0.25f;
        }
        if (overlaps) {
            continue;
        }

        char* uri = list->links[i];
        if (!uri) {
            continue;
        }

        // TODO: those leak on xps
        auto dest = new PageDestinationURL(uri);
        auto pel = new PageElementDestination(dest);
        pel->rect = ToRectF(bbox);
        pageInfo->autoLinks.Append(pel);
    }
    delete list;
    free(coords);
}

static void FzFindImagePositions(fz_context* ctx, int pageNo, Vec<FitzPageImageInfo*>& images, fz_stext_page* stext) {
    if (!stext) {
        return;
    }
    fz_stext_block* block = stext->first_block;
    fz_image* image;
    while (block) {
        if (block->type != FZ_STEXT_BLOCK_IMAGE) {
            block = block->next;
            continue;
        }
        image = block->u.i.image;
        if (image->colorspace != nullptr) {
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/1480
            // fz_convert_pixmap_samples doesn't handle src without colorspace
            // TODO: this is probably not right
            FitzPageImageInfo* img = new FitzPageImageInfo{block->bbox, block->u.i.transform};
            auto pel = new PageElementImage();
            pel->pageNo = pageNo;
            pel->rect = ToRectF(block->bbox);
            pel->imageID = images.isize();
            img->imageElement = pel;
            images.Append(img);
        }
        block = block->next;
    }
}

static fz_image* FzFindImageAtIdx(fz_context* ctx, FzPageInfo* pageInfo, int idx) {
    fz_stext_options opts{};
    opts.flags = FZ_STEXT_PRESERVE_IMAGES;
    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, pageInfo->page, &opts);
    }
    fz_catch(ctx) {
    }
    if (!stext) {
        return nullptr;
    }
    // kind a hacky
    fz_stext_block* block = stext->first_block;
    while (block) {
        if (block->type != FZ_STEXT_BLOCK_IMAGE) {
            block = block->next;
            continue;
        }
        fz_image* image = block->u.i.image;
        if (image->colorspace != nullptr) {
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/1480
            // fz_convert_pixmap_samples doesn't handle src without colorspace
            // TODO: this is probably not right
            if (idx == 0) {
                // TODO: or maybe get pixmap here
                image = fz_keep_image(ctx, image);
                fz_drop_stext_page(ctx, stext);
                return image;
            }
            idx--;
        }
        block = block->next;
    }
    fz_drop_stext_page(ctx, stext);
    return nullptr;
}

static fz_link* FixupPageLinks(fz_link* root) {
    // Links in PDF documents are added from bottom-most to top-most,
    // i.e. links that appear later in the list should be preferred
    // to links appearing before. Since we search from the start of
    // the (single-linked) list, we have to reverse the order of links
    // (http://code.google.com/p/sumatrapdf/issues/detail?id=1303 )
    fz_link* new_root = nullptr;
    while (root) {
        fz_link* tmp = root->next;
        root->next = new_root;
        new_root = root;
        root = tmp;

        // there are PDFs that have x,y positions in reverse order, so fix them up
        fz_link* link = new_root;
        if (link->rect.x0 > link->rect.x1) {
            std::swap(link->rect.x0, link->rect.x1);
        }
        if (link->rect.y0 > link->rect.y1) {
            std::swap(link->rect.y0, link->rect.y1);
        }
        CrashIf(link->rect.x1 < link->rect.x0);
        CrashIf(link->rect.y1 < link->rect.y0);
    }
    return new_root;
}

pdf_obj* PdfCopyStrDict(fz_context* ctx, pdf_document* doc, pdf_obj* dict) {
    pdf_obj* copy = pdf_copy_dict(ctx, dict);
    for (int i = 0; i < pdf_dict_len(ctx, copy); i++) {
        pdf_obj* val = pdf_dict_get_val(ctx, copy, i);
        // resolve all indirect references
        if (pdf_is_indirect(ctx, val)) {
            auto s = pdf_to_str_buf(ctx, val);
            auto slen = pdf_to_str_len(ctx, val);
            pdf_obj* val2 = pdf_new_string(ctx, s, slen);
            pdf_dict_put(ctx, copy, pdf_dict_get_key(ctx, copy, i), val2);
            pdf_drop_obj(ctx, val2);
        }
    }
    return copy;
}

// Note: make sure to only call with ctxAccess
// PdfLoadAttachment && PdfLoadAttachments must traverse in the same order
static ByteSlice PdfLoadAttachment(fz_context* ctx, pdf_document* doc, int no) {
    pdf_obj* dict;
    fz_var(dict);
    ByteSlice res;

    fz_try(ctx) {
        dict = pdf_load_name_tree(ctx, doc, PDF_NAME(EmbeddedFiles));
        if (!dict) {
            break;
        }

        int n = pdf_dict_len(ctx, dict);
        for (int i = 0; i < n; i++) {
            pdf_obj* fs = pdf_dict_get_val(ctx, dict, i);

            if (!pdf_is_embedded_file(ctx, fs)) {
                continue;
            }
            if (no == i + 1) {
                fz_buffer* buf = pdf_load_embedded_file_contents(ctx, fs);
                res.d = (u8*)memdup(buf->data, buf->len);
                res.sz = buf->len;
                fz_drop_buffer(ctx, buf);
                i = n + 1; // exit for loop
            }
        }
    }
    fz_always(ctx) {
        pdf_drop_obj(ctx, dict);
    }
    fz_catch(ctx) {
        logfa("PdfLoadAttachment() failed\n");
    }
    return res;
}

// Note: make sure to only call with ctxAccess
static fz_outline* PdfLoadAttachments(fz_context* ctx, pdf_document* doc, const char* path) {
    fz_outline root{};
    pdf_obj* dict;

    fz_var(root);
    fz_var(dict);

    fz_try(ctx) {
        dict = pdf_load_name_tree(ctx, doc, PDF_NAME(EmbeddedFiles));
        if (!dict) {
            break;
        }

        fz_outline* curr = &root;
        for (int i = 0; i < pdf_dict_len(ctx, dict); i++) {
            pdf_obj* fs = pdf_dict_get_val(ctx, dict, i);

            if (!pdf_is_embedded_file(ctx, fs)) {
                continue;
            }
            pdf_embedded_file_params fileParams = {};
            pdf_get_embedded_file_params(ctx, fs, &fileParams);
            const char* nameStr = fileParams.filename;
            if (str::IsEmpty(nameStr)) {
                continue;
            }
            fz_outline* link = fz_new_outline(ctx);
            link->title = fz_strdup(ctx, nameStr);
            link->page.page = i + 1;
            link->uri = fz_strdup(ctx, nameStr);
            curr->next = link;
            curr = link;
        }
    }
    fz_always(ctx) {
        pdf_drop_obj(ctx, dict);
    }
    fz_catch(ctx) {
        logfa("PdfLoadAttachments() failed for '%s'\n", path);
    }
    return root.next;
}

struct PageLabelInfo {
    int startAt = 0;
    int countFrom = 0;
    const char* type = nullptr;
    pdf_obj* prefix = nullptr;
};

int CmpPageLabelInfo(const void* a, const void* b) {
    return ((PageLabelInfo*)a)->startAt - ((PageLabelInfo*)b)->startAt;
}

static char* FormatPageLabel(const char* type, int pageNo, const char* prefix) {
    if (str::Eq(type, "D")) {
        return str::Format("%s%d", prefix, pageNo);
    }
    if (str::EqI(type, "R")) {
        // roman numbering style
        AutoFreeStr number(str::FormatRomanNumeral(pageNo));
        if (*type == 'r') {
            str::ToLowerInPlace(number.Get());
        }
        return str::Format("%s%s", prefix, number.Get());
    }
    if (str::EqI(type, "A")) {
        // alphabetic numbering style (A..Z, AA..ZZ, AAA..ZZZ, ...)
        str::WStr number;
        number.AppendChar('A' + (pageNo - 1) % 26);
        for (int i = 0; i < (pageNo - 1) / 26; i++) {
            number.AppendChar(number.at(0));
        }
        if (*type == 'a') {
            str::ToLowerInPlace(number.Get());
        }
        return str::Format("%s%s", prefix, number.Get());
    }
    return str::Dup(prefix);
}

void BuildPageLabelRec(fz_context* ctx, pdf_obj* node, int pageCount, Vec<PageLabelInfo>& data) {
    pdf_obj* obj;
    if ((obj = pdf_dict_gets(ctx, node, "Kids")) != nullptr && !pdf_mark_obj(ctx, node)) {
        int n = pdf_array_len(ctx, obj);
        for (int i = 0; i < n; i++) {
            auto arr = pdf_array_get(ctx, obj, i);
            BuildPageLabelRec(ctx, arr, pageCount, data);
        }
        pdf_unmark_obj(ctx, node);
        return;
    }
    obj = pdf_dict_gets(ctx, node, "Nums");
    if (obj == nullptr) {
        return;
    }
    int n = pdf_array_len(ctx, obj);
    for (int i = 0; i < n; i += 2) {
        pdf_obj* info = pdf_array_get(ctx, obj, i + 1);
        PageLabelInfo pli;
        pli.startAt = pdf_to_int(ctx, pdf_array_get(ctx, obj, i)) + 1;
        if (pli.startAt < 1) {
            continue;
        }

        pli.type = pdf_to_name(ctx, pdf_dict_gets(ctx, info, "S"));
        pli.prefix = pdf_dict_gets(ctx, info, "P");
        pli.countFrom = pdf_to_int(ctx, pdf_dict_gets(ctx, info, "St"));
        if (pli.countFrom < 1) {
            pli.countFrom = 1;
        }
        data.Append(pli);
    }
}

// TODO: maybe remove the code completely
// bugs: 3225 and 353
// not sure if we should do it, it's unexpected behavior
static bool gEnsureUniqueLabels = false;

static void EnsureLabelsUnique(StrVec* labels) {
    if (!gEnsureUniqueLabels) {
        return;
    }
    // ensure that all page labels are unique (by appending a number to duplicates)
    StrVec dups(*labels);
    dups.Sort();
    int nDups = dups.Size();
    for (int i = 1; i < nDups; i++) {
        if (!str::Eq(dups.at(i), dups.at(i - 1))) {
            continue;
        }
        int idx = labels->Find(dups.at(i)), counter = 0;
        while ((idx = labels->Find(dups.at(i), idx + 1)) != -1) {
            AutoFreeStr unique;
            do {
                unique.Set(str::Format("%s.%d", dups.at(i), ++counter));
            } while (labels->Contains(unique));
            labels->SetAt(idx, unique.Get());
        }
        nDups = dups.Size();
        for (; i + 1 < nDups && str::Eq(dups.at(i), dups.at(i + 1)); i++) {
            // no-op
        }
    }
}

static StrVec* BuildPageLabelVec(fz_context* ctx, pdf_obj* root, int pageCount) {
    Vec<PageLabelInfo> data;
    BuildPageLabelRec(ctx, root, pageCount, data);
    data.Sort(CmpPageLabelInfo);

    size_t n = data.size();
    if (n == 0) {
        return nullptr;
    }

    PageLabelInfo& pli = data.at(0);
    if (n == 1 && pli.startAt == 1 && pli.countFrom == 1 && !pli.prefix && str::Eq(pli.type, "D")) {
        // this is the default case, no need for special treatment
        return nullptr;
    }

    StrVec* labels = new StrVec();
    for (int i = 0; i < pageCount; i++) {
        labels->Append("");
    }

    for (size_t i = 0; i < n; i++) {
        pli = data.at(i);
        if (pli.startAt > pageCount) {
            break;
        }
        int secLen = pageCount + 1 - pli.startAt;
        if (i < n - 1 && data.at(i + 1).startAt <= pageCount) {
            secLen = data.at(i + 1).startAt - pli.startAt;
        }
        AutoFreeStr prefix(PdfToUtf8(ctx, data.at(i).prefix));
        for (int j = 0; j < secLen; j++) {
            int idx = pli.startAt + j - 1;
            char* label = FormatPageLabel(pli.type, pli.countFrom + j, prefix);
            labels->SetAt(idx, label);
            str::Free(label);
        }
    }

    for (int idx = 0; (idx = labels->Find(nullptr, idx)) != -1; idx++) {
        labels->SetAt(idx, "");
    }

    EnsureLabelsUnique(labels);
    return labels;
}
struct PageTreeStackItem {
    pdf_obj* kids = nullptr;
    int i = -1;
    int len = 0;
    int next_page_no = 0;

    PageTreeStackItem() = default;

    explicit PageTreeStackItem(fz_context* ctx, pdf_obj* kids, int next_page_no = 0) {
        this->kids = kids;
        this->len = pdf_array_len(ctx, kids);
        this->next_page_no = next_page_no;
    }
};

static void fz_lock_context_cs(void* user, int lock) {
    EngineMupdf* e = (EngineMupdf*)user;
    EnterCriticalSection(&e->mutexes[lock]);
}

static void fz_unlock_context_cs(void* user, int lock) {
    EngineMupdf* e = (EngineMupdf*)user;
    LeaveCriticalSection(&e->mutexes[lock]);
}

static void fz_print_cb(void* user, const char* msg) {
    if (!str::EndsWith(msg, "\n")) {
        msg = str::JoinTemp(msg, "\n");
    }
    log(msg);
}

static void InstallFitzErrorCallbacks(fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, nullptr);
    fz_set_error_callback(ctx, fz_print_cb, nullptr);
}

EngineMupdf::EngineMupdf() {
    kind = kindEngineMupdf;
    defaultExt = str::Dup(".pdf");
    fileDPI = 72.0f;

    for (size_t i = 0; i < dimof(mutexes); i++) {
        InitializeCriticalSection(&mutexes[i]);
    }
    InitializeCriticalSection(&pagesAccess);
    ctxAccess = &mutexes[FZ_LOCK_ALLOC];

    fz_locks_ctx.user = this;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_DEFAULT);
    InstallFitzErrorCallbacks(ctx);

    pdf_install_load_system_font_funcs(ctx);
    fz_register_document_handlers(ctx);
}

EngineMupdf::~EngineMupdf() {
    EnterCriticalSection(&pagesAccess);

    // TODO: remove this lock and see what happens
    EnterCriticalSection(ctxAccess);

    for (FzPageInfo* pi : pages) {
        DeleteVecMembers(pi->links);
        DeleteVecMembers(pi->autoLinks);
        DeleteVecMembers(pi->comments);
        DeleteVecMembers(pi->images);
        if (pi->retainedLinks) {
            fz_drop_link(ctx, pi->retainedLinks);
        }
        if (pi->page) {
            fz_drop_page(ctx, pi->page);
        }
    }

    fz_drop_outline(ctx, outline);
    fz_drop_outline(ctx, attachments);

    if (pdfInfo) {
        pdf_drop_obj(ctx, pdfInfo);
    }

    if (pdfdoc) {
        pdf_drop_page_tree(ctx, pdfdoc);
    }

    fz_drop_document(ctx, _doc);
    drop_cached_fonts_for_ctx(ctx);
    fz_drop_context(ctx);

    delete pageLabels;
    delete tocTree;
    DeleteVecMembers(pages);

    for (size_t i = 0; i < dimof(mutexes); i++) {
        LeaveCriticalSection(&mutexes[i]);
        DeleteCriticalSection(&mutexes[i]);
    }
    LeaveCriticalSection(&pagesAccess);
    DeleteCriticalSection(&pagesAccess);
}

class PasswordCloner : public PasswordUI {
    u8* cryptKey = nullptr;

  public:
    explicit PasswordCloner(u8* cryptKey) {
        this->cryptKey = cryptKey;
    }

    char* GetPassword(const char*, u8*, u8 decryptionKeyOut[32], bool* saveKey) override {
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return nullptr;
    }
};

EngineBase* EngineMupdf::Clone() {
    ScopedCritSec scope(ctxAccess);
    if (!FilePath()) {
        // before port we could clone streams but it's no longer possible
        return nullptr;
    }

    // use this document's encryption key (if any) to load the clone
    PasswordCloner* pwdUI = nullptr;
    if (pdfdoc) {
        if (pdf_crypt_key(ctx, pdfdoc->crypt)) {
            pwdUI = new PasswordCloner(pdf_crypt_key(ctx, pdfdoc->crypt));
        }
    }

    EngineMupdf* clone = new EngineMupdf();
    bool ok = clone->Load(FilePath(), pwdUI);
    if (!ok) {
        delete clone;
        delete pwdUI;
        return nullptr;
    }
    delete pwdUI;

    if (!decryptionKey && pdfdoc && pdfdoc->crypt) {
        free(clone->decryptionKey);
        clone->decryptionKey = nullptr;
    }

    return clone;
}

// File names ending in :<digits> are interpreted as containing
// embedded PDF documents (the digits is stream number of the embedded file stream)
// the caller must free()
const char* ParseEmbeddedStreamNumber(const char* path, int* streamNoOut) {
    int streamNo = -1;
    char* path2 = str::Dup(path);
    char* streamNoStr = (char*)FindEmbeddedPdfFileStreamNo(path2);
    if (streamNoStr) {
        char* rest = (char*)str::Parse(streamNoStr, ":%d", &streamNo);
        // there shouldn't be any left unparsed data
        CrashIf(!rest);
        if (!rest) {
            streamNo = -1;
        }
        // replace ':' with 0 to create a filesystem path
        *streamNoStr = 0;
    }
    *streamNoOut = streamNo;
    return path2;
}

ByteSlice EngineMupdf::LoadStreamFromPDFFile(const char* filePath) {
    int streamNo = -1;
    AutoFreeStr fnCopy = ParseEmbeddedStreamNumber(filePath, &streamNo);
    if (streamNo < 0) {
        return {};
    }

    char* path = fnCopy.Get();
    bool ok = Load(path, nullptr);
    if (!ok) {
        return {};
    }

    if (!pdf_obj_num_is_stream(ctx, pdfdoc, streamNo)) {
        return {};
    }

    fz_buffer* buffer = nullptr;
    fz_var(buffer);
    fz_try(ctx) {
        buffer = pdf_load_stream_number(ctx, pdfdoc, streamNo);
    }
    fz_catch(ctx) {
        return {};
    }
    auto dataSize = buffer->len;
    if (dataSize == 0) {
        return {};
    }
    auto data = (u8*)memdup(buffer->data, dataSize);
    fz_drop_buffer(ctx, buffer);

    return {data, dataSize};
}

// <filePath> should end with embed marks, which is a stream number
// inside pdf file
ByteSlice LoadEmbeddedPDFFile(const char* filePath) {
    EngineMupdf* engine = new EngineMupdf();
    auto res = engine->LoadStreamFromPDFFile(filePath);
    delete engine;
    return res;
}

static ByteSlice TxtFileToHTML(const char* path) {
    ByteSlice fd = file::ReadFileWithAllocator(path, GetTempAllocator());
    if (fd.empty()) {
        return {};
    }

    InterlockedIncrement(&gAllowAllocFailure);
    defer {
        InterlockedDecrement(&gAllowAllocFailure);
    };

    TempStr data = (TempStr)fd.data();
    data = str::ReplaceTemp(data, "&", "&amp;");
    if (!data) {
        return {};
    }
    data = str::ReplaceTemp(data, ">", "&gt;");
    if (!data) {
        return {};
    }
    data = str::ReplaceTemp(data, "<", "&lt;");
    if (!data) {
        return {};
    }

    str::Str d;
    d.Append(R"(<html>
    <head>
<style>
    body {
        color: 0xff0000;
    }
    pre {
        white-space: pre-wrap;
    }
</style>
    </head>
<body>
    <pre>)");
    bool ok = d.Append(data);
    if (!ok) {
        return {};
    }
    d.Append(R"(</pre>
</body>
</html>)");
    size_t sz = d.size();
    return {(u8*)d.StealData(), sz};
}

static ByteSlice PalmDocToHTML(const char* path) {
    auto doc = PalmDoc::CreateFromFile(path);
    if (!doc) {
        return {};
    }
    ByteSlice html = doc->GetHtmlData();
    return html.Clone();
}

bool EngineMupdf::Load(const char* path, PasswordUI* pwdUI) {
    const char* pathA = path;
    CrashIf(FilePath() || _doc || !ctx);
    SetFilePath(path);

    auto ext = path::GetExtTemp(path);
    str::ReplaceWithCopy(&defaultExt, ext);

    int streamNo = -1;
    AutoFreeStr fnCopy = ParseEmbeddedStreamNumber(pathA, &streamNo);

    Kind kind = GuessFileTypeFromName(pathA);
    // show .txt, .xml and other text files as plain text
    // using html engine
    if (kind == kindFileTxt) {
        // synthesize a .html file from text file
        ByteSlice d = TxtFileToHTML(path);
        if (d.empty()) {
            return false;
        }
        fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (const u8*)d.data(), d.size());
        fz_stream* file = fz_open_buffer(ctx, buf);
        fz_drop_buffer(ctx, buf);
        str::Free(d);
        char* nameHint = str::JoinTemp(path, ".html");
        if (!LoadFromStream(file, nameHint, pwdUI)) {
            return false;
        }
        return FinishLoading();
    }

    if (str::EqI(ext, ".pdb")) {
        // synthesize a .html file from pdb file
        ByteSlice d = PalmDocToHTML(pathA);
        if (d.empty()) {
            return false;
        }
        fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (const u8*)d.data(), d.size());
        fz_stream* file = fz_open_buffer(ctx, buf);
        fz_drop_buffer(ctx, buf);
        str::Free(d);
        char* nameHint = str::Join(path, ".html");
        if (!LoadFromStream(file, nameHint, pwdUI)) {
            return false;
        }
        return FinishLoading();
    }

    fz_stream* file = nullptr;

    fz_var(file);
    fz_try(ctx) {
        file = FzOpenFile2(ctx, fnCopy);
    }
    fz_catch(ctx) {
        file = nullptr;
    }

    if (!LoadFromStream(file, FilePath(), pwdUI)) {
        return false;
    }

    if (streamNo < 0) {
        return FinishLoading();
    }

    pdfdoc = pdf_specifics(ctx, _doc);
    if (pdfdoc) {
        if (!pdf_obj_num_is_stream(ctx, pdfdoc, streamNo)) {
            return false;
        }

        fz_buffer* buffer = nullptr;
        fz_var(buffer);
        fz_try(ctx) {
            buffer = pdf_load_stream_number(ctx, pdfdoc, streamNo);
            file = fz_open_buffer(ctx, buffer);
        }
        fz_always(ctx) {
            fz_drop_buffer(ctx, buffer);
        }
        fz_catch(ctx) {
            return false;
        }
    }

    fz_drop_document(ctx, _doc);
    _doc = nullptr;

    if (!LoadFromStream(file, FilePath(), pwdUI)) {
        return false;
    }

    return FinishLoading();
}

#if 0
const char* custom_css = R"(
* {
    background-color: #f3f3f3;
    line-height: 1.3em;
}
@page{
    margin:2em 2em;    
}
)";
#endif

const char* custom_css = nullptr;

/*
line-height: 2.5em;
font-family: "Consolas";

    line-height: 2.5em;
    font-family: Consolas;

*/

// TODO: need to do stuff to support .txt etc.
bool EngineMupdf::Load(IStream* stream, const char* nameHint, PasswordUI* pwdUI) {
    CrashIf(FilePath() || _doc || !ctx);
    if (!ctx) {
        return false;
    }

    fz_stream* stm = nullptr;
    fz_var(stm);
    fz_try(ctx) {
        stm = FzOpenIStream(ctx, stream);
    }
    fz_catch(ctx) {
        stm = nullptr;
    }
    if (!stm) {
        return false;
    }
    if (!LoadFromStream(stm, nameHint, pwdUI)) {
        return false;
    }
    return FinishLoading();
}

bool EngineMupdf::LoadFromStream(fz_stream* stm, const char* nameHint, PasswordUI* pwdUI) {
    if (!stm) {
        return false;
    }

#if 0
    /* a heuristic. a layout page size for .epub is A5 but that makes a font size too
       large for non-epub files like .txt or .xml, so for those use larger A4 */
    float ldx = layoutA4DxPt;
    float ldy = layoutA4DyPt;
    const char* ext = path::GetExtTemp(nameHint);
    if (str::EqI(ext, ".epub")) {
        ldx = layoutA5DxPt;
        ldy = layoutA5DyPt;
    }
#endif

    float ldx = layoutA5DxPt;
    float ldy = layoutA5DyPt;
    float lfontDy = layoutFontEm;
    if (!str::EndsWithI(nameHint, ".epub")) {
        lfontDy = 8.f;
    }

    if (custom_css) {
        fz_set_user_css(ctx, custom_css);
    }

    float dx, dy, fontDy;
    _doc = nullptr;
    fz_var(dx);
    fz_var(dy);
    fz_var(fontDy);
    fz_try(ctx) {
        _doc = fz_open_document_with_stream(ctx, nameHint, stm);
        pdfdoc = pdf_specifics(ctx, _doc);
        dx = DpiScale(ldx, displayDPI);
        dy = DpiScale(ldy, displayDPI);
        fontDy = DpiScale(lfontDy, displayDPI);
        fz_layout_document(ctx, _doc, dx, dy, fontDy);
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, stm);
    }
    fz_catch(ctx) {
        _doc = nullptr;
    }
    if (!_doc) {
        return false;
    }

    docStream = stm;

    isPasswordProtected = fz_needs_password(ctx, _doc);
    if (!isPasswordProtected) {
        return true;
    }

    if (!pwdUI) {
        return false;
    }

    // TODO: make this work for non-PDF formats?
    u8 digest[16 + 32]{};
    if (pdfdoc) {
        FzStreamFingerprint(ctx, pdfdoc->file, digest);
    }

    bool ok = false;
    bool saveKey = false;
    while (!ok) {
        u8* decryptKey = nullptr;
        if (pdfdoc) {
            decryptKey = pdf_crypt_key(ctx, pdfdoc->crypt);
        }
        AutoFreeStr pwd(pwdUI->GetPassword(FilePath(), digest, decryptKey, &saveKey));
        if (!pwd) {
            // password not given or encryption key has been remembered
            ok = saveKey;
            break;
        }

        // MuPDF expects passwords to be UTF-8 encoded
        AutoFreeStr pwdA = str::Dup(pwd);
        ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
        // according to the spec (1.7 ExtensionLevel 3), the password
        // for crypt revisions 5 and above are in SASLprep normalization
        if (!ok) {
            // TODO: this is only part of SASLprep
            pwd.Set(NormalizeString(pwd, 5 /* NormalizationKC */));
            if (pwd) {
                pwdA = str::Dup(pwd);
                ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
            }
        }
        // older Acrobat versions seem to have considered passwords to be in codepage 1252
        // note: such passwords aren't portable when stored as Unicode text
        if (!ok && GetACP() != 1252) {
            AutoFreeStr pwd_ansi = str::Dup(pwd.Get());
            AutoFreeWstr pwd_cp1252(strconv::StrToWstr(pwd_ansi.Get(), 1252));
            pwdA = ToUtf8(pwd_cp1252);
            ok = fz_authenticate_password(ctx, _doc, pwdA.Get());
        }
    }

    if (pdfdoc && ok && saveKey) {
        memcpy(digest + 16, pdf_crypt_key(ctx, pdfdoc->crypt), 32);
        decryptionKey = _MemToHex(&digest);
    }

    return ok;
}

static PageLayout GetPreferredLayout(fz_context* ctx, fz_document* doc) {
    PageLayout layout(PageLayout::Type::Single);
    pdf_document* pdfdoc = pdf_specifics(ctx, doc);
    if (!pdfdoc) {
        return layout;
    }

    pdf_obj* root = nullptr;
    fz_var(root);
    fz_try(ctx) {
        root = pdf_dict_gets(ctx, pdf_trailer(ctx, pdfdoc), "Root");
    }
    fz_catch(ctx) {
        root = nullptr;
    }
    if (!root) {
        return layout;
    }

    const char* name = nullptr;
    fz_var(name);
    fz_try(ctx) {
        name = pdf_to_name(ctx, pdf_dict_gets(ctx, root, "PageLayout"));
        if (str::EndsWith(name, "Right")) {
            layout.type = PageLayout::Type::Book;
        } else if (str::StartsWith(name, "Two")) {
            layout.type = PageLayout::Type::Facing;
        }
    }
    fz_catch(ctx) {
    }

    pdf_obj* prefs = nullptr;
    const char* direction = nullptr;
    fz_var(prefs);
    fz_var(direction);
    fz_try(ctx) {
        prefs = pdf_dict_gets(ctx, root, "ViewerPreferences");
        direction = pdf_to_name(ctx, pdf_dict_gets(ctx, prefs, "Direction"));
        if (str::Eq(direction, "R2L")) {
            layout.r2l = true;
        }
    }
    fz_catch(ctx) {
    }

    return layout;
}

static bool IsLinearizedFile(EngineMupdf* e) {
    if (!e->pdfdoc) {
        return false;
    }

    ScopedCritSec scope(e->ctxAccess);
    int isLinear = 0;
    fz_try(e->ctx) {
        isLinear = pdf_doc_was_linearized(e->ctx, e->pdfdoc);
    }
    fz_catch(e->ctx) {
        isLinear = 0;
    }
    return isLinear;
}

static void FinishNonPDFLoading(EngineMupdf* e) {
    ScopedCritSec scope(e->ctxAccess);

    auto ctx = e->ctx;
    for (int i = 0; i < e->pageCount; i++) {
        fz_rect mbox{};
        fz_matrix page_ctm{};
        fz_page* page = nullptr;
        fz_var(page);
        fz_var(mbox);
        fz_try(ctx) {
            page = nullptr;
            page = fz_load_page(ctx, e->_doc, i);
            mbox = fz_bound_page(ctx, page);
        }
        fz_always(ctx) {
            fz_drop_page(ctx, page);
        }
        fz_catch(ctx) {
            mbox = {};
        }
        if (fz_is_empty_rect(mbox)) {
            fz_warn(ctx, "cannot find page size for page %d", i);
            mbox.x0 = 0;
            mbox.y0 = 0;
            mbox.x1 = 612;
            mbox.y1 = 792;
        }
        FzPageInfo* pageInfo = e->pages.at(i);
        pageInfo->mediabox = ToRectF(mbox);
        pageInfo->pageNo = i + 1;
    }

    fz_try(ctx) {
        e->outline = fz_load_outline(ctx, e->_doc);
    }
    fz_catch(ctx) {
        // ignore errors from pdf_load_outline()
        // this information is not critical and checking the
        // error might prevent loading some pdfs that would
        // otherwise get displayed
        fz_warn(ctx, "Couldn't load outline");
    }
}

bool EngineMupdf::FinishLoading() {
    pdfdoc = pdf_specifics(ctx, _doc);

    pageCount = 0;
    fz_var(pageCount);
    fz_try(ctx) {
        // this call might throw the first time
        pageCount = fz_count_pages(ctx, _doc);
    }
    fz_catch(ctx) {
        pageCount = 0;
    }
    if (pageCount == 0) {
        fz_warn(ctx, "document has no pages");
        return false;
    }

    preferredLayout = GetPreferredLayout(ctx, _doc);
    allowsPrinting = fz_has_permission(ctx, _doc, FZ_PERMISSION_PRINT);
    allowsCopyingText = fz_has_permission(ctx, _doc, FZ_PERMISSION_COPY);

    for (int i = 0; i < pageCount; i++) {
        auto pi = new FzPageInfo();
        pages.Append(pi);
    }
    if (!pdfdoc) {
        FinishNonPDFLoading(this);
        return true;
    }

    ScopedCritSec scope(ctxAccess);

    for (int pageNo = 0; pageNo < pageCount; pageNo++) {
        pdf_obj* pageref = nullptr;
        fz_rect mbox{};
        fz_matrix page_ctm{};
        fz_var(pageref);
        fz_var(mbox);
        fz_try(ctx) {
            // note: don't pdf_drop_obj() this
            pageref = pdf_lookup_page_obj(ctx, pdfdoc, pageNo);
            pdf_page_obj_transform(ctx, pageref, &mbox, &page_ctm);
            mbox = fz_transform_rect(mbox, page_ctm);
        }
        fz_catch(ctx) {
            mbox = {};
        }
        if (fz_is_empty_rect(mbox)) {
            logfa("cannot find page size for page %d", pageNo);
            mbox.x0 = 0;
            mbox.y0 = 0;
            mbox.x1 = 612;
            mbox.y1 = 792;
        }
        FzPageInfo* pageInfo = pages[pageNo];
        pageInfo->mediabox = ToRectF(mbox);
        pageInfo->pageNo = pageNo + 1;
    }

    fz_try(ctx) {
        outline = fz_load_outline(ctx, _doc);
    }
    fz_catch(ctx) {
        // ignore errors from pdf_load_outline()
        // this information is not critical and checking the
        // error might prevent loading some pdfs that would
        // otherwise get displayed
        logfa("Couldn't load outline for '%s'\n", FilePath());
    }

    attachments = PdfLoadAttachments(ctx, pdfdoc, FilePath());

    pdf_obj* origInfo = nullptr;
    fz_var(origInfo);
    fz_try(ctx) {
        // keep a copy of the Info dictionary, as accessing the original
        // isn't thread safe and we don't want to block for this when
        // displaying document properties
        origInfo = pdf_dict_gets(ctx, pdf_trailer(ctx, pdfdoc), "Info");

        if (origInfo) {
            pdfInfo = PdfCopyStrDict(ctx, pdfdoc, origInfo);
        }
        if (!pdfInfo) {
            pdfInfo = pdf_new_dict(ctx, pdfdoc, 4);
        }
        // also remember linearization and tagged states at this point
        if (IsLinearizedFile(this)) {
            pdf_dict_puts_drop(ctx, pdfInfo, "Linearized", PDF_TRUE);
        }
        pdf_obj* trailer = pdf_trailer(ctx, pdfdoc);
        pdf_obj* marked = pdf_dict_getp(ctx, trailer, "Root/MarkInfo/Marked");
        bool isMarked = pdf_to_bool(ctx, marked);
        if (isMarked) {
            pdf_dict_puts_drop(ctx, pdfInfo, "Marked", PDF_TRUE);
        }
        // also remember known output intents (PDF/X, etc.)
        pdf_obj* intents = pdf_dict_getp(ctx, trailer, "Root/OutputIntents");
        if (pdf_is_array(ctx, intents)) {
            int n = pdf_array_len(ctx, intents);
            pdf_obj* list = pdf_new_array(ctx, pdfdoc, n);
            for (int i = 0; i < n; i++) {
                pdf_obj* intent = pdf_dict_gets(ctx, pdf_array_get(ctx, intents, i), "S");
                if (pdf_is_name(ctx, intent) && !pdf_is_indirect(ctx, intent) &&
                    str::StartsWith(pdf_to_name(ctx, intent), "GTS_PDF")) {
                    pdf_array_push(ctx, list, intent);
                }
            }
            pdf_dict_puts_drop(ctx, pdfInfo, "OutputIntents", list);
        }
        // also note common unsupported features (such as XFA forms)
        pdf_obj* xfa = pdf_dict_getp(ctx, pdf_trailer(ctx, pdfdoc), "Root/AcroForm/XFA");
        if (pdf_is_array(ctx, xfa)) {
            pdf_dict_puts_drop(ctx, pdfInfo, "Unsupported_XFA", PDF_TRUE);
        }
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load document properties");
        pdf_drop_obj(ctx, pdfInfo);
        pdfInfo = nullptr;
    }

    pdf_obj* labels = nullptr;
    fz_var(labels);
    fz_try(ctx) {
        labels = pdf_dict_getp(ctx, pdf_trailer(ctx, pdfdoc), "Root/PageLabels");
        if (labels) {
            pageLabels = BuildPageLabelVec(ctx, labels, PageCount());
        }
    }
    fz_catch(ctx) {
        fz_warn(ctx, "Couldn't load page labels");
    }
    if (pageLabels) {
        hasPageLabels = true;
    }

    // TODO: support javascript
    CrashIf(pdf_js_supported(ctx, pdfdoc));

    return true;
}

static NO_INLINE IPageDestination* DestFromAttachment(EngineMupdf* engine, fz_outline* outline) {
    PageDestination* dest = new PageDestination();
    dest->kind = kindDestinationAttachment;
    // WCHAR* path = ToWstr(outline->uri);
    dest->name = str::Dup(outline->title);
    // page is really a stream number
    dest->value = str::Format("%s:%d", engine->FilePath(), outline->page.page);
    dest->pageNo = outline->page.page;
    return dest;
}

TocItem* EngineMupdf::BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment) {
    TocItem* root = nullptr;
    TocItem* curr = nullptr;

    while (outline) {
        char* name = nullptr;
        WCHAR* nameW = nullptr;
        if (outline->title) {
            // must convert to Unicode because PdfCleanString() doesn't work on utf8
            nameW = ToWstr(outline->title);
            PdfCleanStringInPlace(nameW);
            name = ToUtf8(nameW);
            str::Free(nameW);
        }
        if (!name) {
            name = str::Dup("");
        }

        int pageNo = FzGetPageNo(ctx, _doc, nullptr, outline);

        IPageDestination* dest = nullptr;
        if (isAttachment) {
            dest = DestFromAttachment(this, outline);
        } else {
            dest = NewPageDestinationMupdf(ctx, _doc, nullptr, outline);
        }
        TocItem* item = NewTocItemWithDestination(parent, name, dest);

        free(name);
        item->isOpenDefault = outline->is_open;
        item->id = ++idCounter;
        item->fontFlags = 0; // TODO: had outline->flags; but mupdf changed outline
        item->pageNo = pageNo;
        ReportIf(!isAttachment && !item->PageNumbersMatch());

        // TODO: had outline->n_color and outline->color but mupdf changed outline
        /*
        if (outline->n_color > 0) {
            item->color = ColorRefFromPdfFloat(ctx, outline->n_color, outline->color);
        }
        */

        if (outline->down) {
            item->child = BuildTocTree(item, outline->down, idCounter, isAttachment);
        }

        if (!root) {
            root = item;
            curr = item;
        } else {
            CrashIf(!curr);
            if (curr) {
                curr->next = item;
            }
            curr = item;
        }

        outline = outline->next;
    }

    return root;
}

// TODO: maybe build in FinishLoading
TocTree* EngineMupdf::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    if (outline == nullptr && attachments == nullptr) {
        return nullptr;
    }

    int idCounter = 0;

    ScopedCritSec cs(ctxAccess);

    TocItem* root = nullptr;
    TocItem* att = nullptr;
    if (outline) {
        root = BuildTocTree(nullptr, outline, idCounter, false);
    }
    if (!attachments) {
        goto MakeTree;
    }
    att = BuildTocTree(nullptr, attachments, idCounter, true);
    if (root) {
        root->AddSiblingAtEnd(att);
    } else {
        root = att;
    }
MakeTree:
    if (!root) {
        return nullptr;
    }
    TocItem* realRoot = new TocItem();
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

IPageDestination* EngineMupdf::GetNamedDest(const char* name) {
    if (!pdfdoc) {
        return nullptr;
    }

    ScopedCritSec scope1(&pagesAccess);
    ScopedCritSec scope2(ctxAccess);

    size_t nameLen = str::Len(name);
    pdf_obj* dest = nullptr;

    fz_var(dest);
    pdf_obj* nameobj = nullptr;
    fz_var(nameobj);
    fz_try(ctx) {
        nameobj = pdf_new_string(ctx, name, (int)nameLen);
        dest = pdf_lookup_dest(ctx, pdfdoc, nameobj);
        pdf_drop_obj(ctx, nameobj);
    }
    fz_catch(ctx) {
        dest = nullptr;
    }

    if (!dest) {
        return nullptr;
    }

    IPageDestination* pageDest = nullptr;
    char* uri = nullptr;

    fz_var(uri);
    fz_try(ctx) {
        uri = pdf_parse_link_dest(ctx, pdfdoc, dest);
    }
    fz_catch(ctx) {
        uri = nullptr;
    }

    if (!uri) {
        return nullptr;
    }

    float x, y, zoom = 0;
    int pageNo = ResolveLink(ctx, _doc, uri, &x, &y);

    RectF r{x, y, 0, 0};
    pageDest = NewSimpleDest(pageNo, r, zoom);
    fz_free(ctx, uri);
    return pageDest;
}

// return a page but only if is fully loaded
FzPageInfo* EngineMupdf::GetFzPageInfoFast(int pageNo) {
    ScopedCritSec scope(&pagesAccess);
    CrashIf(pageNo < 1 || pageNo > pageCount);
    FzPageInfo* pageInfo = pages[pageNo - 1];
    if (!pageInfo->page || !pageInfo->fullyLoaded) {
        return nullptr;
    }
    return pageInfo;
}

static IPageElement* NewFzComment(const char* comment, int pageNo, RectF rect) {
    auto res = new PageElementComment(comment);
    res->pageNo = pageNo;
    res->rect = rect;
    return res;
}

// must be called inside fz_try
static IPageElement* MakePdfCommentFromPdfAnnot(fz_context* ctx, int pageNo, pdf_annot* annot) {
    fz_rect rect = pdf_bound_annot(ctx, annot);
    auto tp = pdf_annot_type(ctx, annot);
    const char* contents = pdf_annot_contents(ctx, annot);
    const char* label = pdf_annot_field_label(ctx, annot);
    const char* s = contents;
    // TODO: use separate classes for comments and tooltips?
    if (str::IsEmpty(contents)) {
        s = label;
    }
    RectF rd = ToRectF(rect);
    return NewFzComment(s, pageNo, rd);
}

// must be called inside fz_try
static void RebuildCommentsFromAnnotationsInner(fz_context* ctx, pdf_annot* annot, int pageNo,
                                                Vec<IPageElement*>& comments) {
    auto tp = pdf_annot_type(ctx, annot);
    const char* contents = pdf_annot_contents(ctx, annot); // don't free
    if (str::Len(contents) > 128) {
        contents = str::DupTemp(contents, 128);
    }
    bool isContentsEmpty = str::IsEmpty(contents);
    const char* label = pdf_annot_field_label(ctx, annot); // don't free
    bool isLabelEmpty = str::IsEmpty(label);
    int flags = pdf_annot_field_flags(ctx, annot);
    bool isEmpty = isContentsEmpty && isLabelEmpty;

    // const char* tpStr = pdf_string_from_annot_type(ctx, tp);
    //  logf("MakePageElementCommentsFromAnnotations: annot %d '%s', contents: '%s', label: '%s'\n", tp, tpStr,
    //  contents, abel);

    if (PDF_ANNOT_FILE_ATTACHMENT == tp) {
        logf("found file attachment annotation\n");

        pdf_embedded_file_params fileParams = {};
        pdf_obj* fs = pdf_annot_filespec(ctx, annot);
        int num = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
        pdf_get_embedded_file_params(ctx, fs, &fileParams);
        const char* attname = fileParams.filename;
        fz_rect rect = pdf_bound_annot(ctx, annot);
        if (str::IsEmpty(attname) || fz_is_empty_rect(rect) || !pdf_is_embedded_file(ctx, fs)) {
            return;
        }

        logf("attachment: %s, num: %d\n", attname, num);

        auto dest = new PageDestination();
        // TODO: kindDestinationAttachment ?
        dest->kind = kindDestinationLaunchEmbedded;
        dest->value = str::Dup(attname);

        auto el = new PageElementDestination(dest);
        el->pageNo = pageNo;
        el->rect = ToRectF(rect);

        comments.Append(el);
        // TODO: need to implement https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
        // for saving the attachment to a file
        // TODO: expose /Contents in addition to the file path
        return;
    }

    if (!isEmpty && tp != PDF_ANNOT_FREE_TEXT) {
        auto comment = MakePdfCommentFromPdfAnnot(ctx, pageNo, annot);
        comments.Append(comment);
        return;
    }

    if (PDF_ANNOT_WIDGET == tp && !isLabelEmpty) {
        bool isReadOnly = flags & PDF_FIELD_IS_READ_ONLY;
        if (!isReadOnly) {
            auto comment = MakePdfCommentFromPdfAnnot(ctx, pageNo, annot);
            comments.Append(comment);
        }
    }
}

static void RebuildCommentsFromAnnotations(fz_context* ctx, FzPageInfo* pageInfo) {
    DeleteVecMembers(pageInfo->comments);

    // TODO: can use pageInof->annotations
    Vec<IPageElement*>& comments = pageInfo->comments;

    auto page = pageInfo->page;
    if (!page) {
        return;
    }
    auto pdfpage = pdf_page_from_fz_page(ctx, page);
    int pageNo = pageInfo->pageNo;

    pdf_annot* annot;
    for (annot = pdf_first_annot(ctx, pdfpage); annot; annot = pdf_next_annot(ctx, annot)) {
        fz_try(ctx) {
            RebuildCommentsFromAnnotationsInner(ctx, annot, pageNo, comments);
        }
        fz_catch(ctx) {
        }
    }

    // re-order list into top-to-bottom order (i.e. last-to-first)
    comments.Reverse();
}

// Maybe: handle FZ_ERROR_TRYLATER, which can happen when parsing from network.
// (I don't think we read from network now).
// Maybe: when loading fully, cache extracted text in FzPageInfo
// so that we don't have to re-do fz_new_stext_page_from_page() when doing search
FzPageInfo* EngineMupdf::GetFzPageInfo(int pageNo, bool loadQuick) {
    // TODO: minimize time spent under pagesAccess when fully loading
    ScopedCritSec scope(&pagesAccess);

    CrashIf(pageNo < 1 || pageNo > pageCount);
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = pages[pageIdx];

    ScopedCritSec ctxScope(ctxAccess);
    if (!pageInfo->page) {
        fz_try(ctx) {
            pageInfo->page = fz_load_page(ctx, _doc, pageIdx);
        }
        fz_catch(ctx) {
        }
    }

    fz_page* page = pageInfo->page;
    if (!page) {
        return nullptr;
    }

    // build annotations info on first access
    if (pdfdoc && pageInfo->annotations.isize() == 0) {
        fz_try(ctx) {
            pdf_page* pdfpage = pdf_page_from_fz_page(ctx, pageInfo->page);
            pdf_annot* annot = pdf_first_annot(ctx, pdfpage);
            while (annot) {
                Annotation* a = MakeAnnotationWrapper(this, annot, pageNo);
                if (a) {
                    pageInfo->annotations.Append(a);
                }
                annot = pdf_next_annot(ctx, annot);
            }
        }
        fz_catch(ctx) {
        }
        RebuildCommentsFromAnnotations(ctx, pageInfo);
    }

    if (loadQuick || pageInfo->fullyLoaded) {
        return pageInfo;
    }

    CrashIf(pageInfo->pageNo != pageNo);

    pageInfo->fullyLoaded = true;

    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_stext_options opts{};
    opts.flags = FZ_STEXT_PRESERVE_IMAGES;
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, page, &opts);
    }
    fz_catch(ctx) {
    }

    fz_link* link = fz_load_links(ctx, page);
    link = FixupPageLinks(link); // TOOD: is this necessary?
    pageInfo->retainedLinks = link;
    while (link) {
        auto pel = NewLinkDestination(pageNo, ctx, _doc, link, nullptr);
        pageInfo->links.Append(pel);
        link = link->next;
    }

    if (!stext) {
        return pageInfo;
    }

    FzLinkifyPageText(pageInfo, stext);
    FzFindImagePositions(ctx, pageNo, pageInfo->images, stext);
    fz_drop_stext_page(ctx, stext);
    return pageInfo;
}

RectF EngineMupdf::PageMediabox(int pageNo) {
    FzPageInfo* pi = pages[pageNo - 1];
    return pi->mediabox;
}

RectF EngineMupdf::PageContentBox(int pageNo, RenderTarget target) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo) {
        // maybe should return a dummy size. not sure how this
        // will play with layout. The page should fail to render
        // since the doc is broken and page is missing
        return RectF();
    }

    ScopedCritSec scope(ctxAccess);

    fz_cookie fzcookie{};
    fz_rect rect = fz_empty_rect;
    fz_device* dev = nullptr;
    fz_display_list* list = nullptr;

    fz_rect pagerect = fz_bound_page(ctx, pageInfo->page);

    fz_var(dev);
    fz_var(list);

    RectF mediabox = pageInfo->mediabox;

    fz_try(ctx) {
        list = fz_new_display_list_from_page(ctx, pageInfo->page);
        if (list) {
            dev = fz_new_bbox_device(ctx, &rect);
            fz_run_display_list(ctx, list, dev, fz_identity, pagerect, &fzcookie);
            fz_close_device(ctx, dev);
        }
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        if (list) {
            fz_drop_display_list(ctx, list);
        }
    }
    fz_catch(ctx) {
        list = nullptr;
    }

    if (!list) {
        return mediabox;
    }

    if (fz_is_infinite_rect(rect)) {
        return mediabox;
    }

    RectF rect2 = ToRectF(rect);
    return rect2.Intersect(mediabox);
}

RectF EngineMupdf::Transform(const RectF& rect, int pageNo, float zoom, int rotation, bool inverse) {
    if (zoom <= 0) {
        const char* name = FilePath();
        if (!name) {
            name = "";
        }
        logf("doc: %s, pageNo: %d, zoom: %.2f\n", name, pageNo, zoom);
    }
    ReportIf(zoom <= 0);
    if (zoom <= 0) {
        zoom = 1;
    }
    fz_matrix ctm = viewctm(pageNo, zoom, rotation);
    if (inverse) {
        ctm = fz_invert_matrix(ctm);
    }
    fz_rect rect2 = ToFzRect(rect);
    rect2 = fz_transform_rect(rect2, ctm);
    return ToRectF(rect2);
}

RenderedBitmap* EngineMupdf::RenderPage(RenderPageArgs& args) {
    auto pageNo = args.pageNo;

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo || !pageInfo->page) {
        return nullptr;
    }
    fz_page* page = pageInfo->page;

    fz_cookie* fzcookie = nullptr;
    FitzAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new FitzAbortCookie();
        *args.cookie_out = cookie;
        fzcookie = &cookie->cookie;
    }

    ScopedCritSec cs(ctxAccess);

    auto pageRect = args.pageRect;
    auto zoom = args.zoom;
    auto rotation = args.rotation;
    fz_rect pRect;
    if (pageRect) {
        pRect = ToFzRect(*pageRect);
    } else {
        // TODO(port): use pageInfo->mediabox?
        pRect = fz_bound_page(ctx, page);
    }
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_irect bbox = fz_round_rect(fz_transform_rect(pRect, ctm));

    fz_colorspace* csRgb = fz_device_rgb(ctx);
    fz_irect ibounds = bbox;

    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    RenderedBitmap* bitmap = nullptr;

    fz_var(dev);
    fz_var(pix);
    fz_var(bitmap);

    const char* usage = "View";
    switch (args.target) {
        case RenderTarget::Print:
            usage = "Print";
            break;
    }

    pdf_page* pdfpage = nullptr;
    fz_var(pdfpage);
    if (pdfdoc) {
        fz_try(ctx) {
            pdfpage = pdf_page_from_fz_page(ctx, page);
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            fz_clear_pixmap_with_value(ctx, pix, 0xff);
            // TODO: in printing different style. old code use pdf_run_page_with_usage(), with usage ="View"
            // or "Print". "Export" is not used
            dev = fz_new_draw_device(ctx, ctm, pix);
            pdf_run_page_with_usage(ctx, pdfpage, dev, fz_identity, usage, fzcookie);
            bitmap = NewRenderedFzPixmap(ctx, pix);
            fz_close_device(ctx, dev);
        }
        fz_always(ctx) {
            if (dev) {
                fz_drop_device(ctx, dev);
            }
            fz_drop_pixmap(ctx, pix);
        }
        fz_catch(ctx) {
            delete bitmap;
            return nullptr;
        }
    } else {
        fz_try(ctx) {
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            // TODO: to have uniform background needs to set custom css
            // background-color and clear pixmap with the same color
            fz_clear_pixmap_with_value(ctx, pix, 0xff);
            // fz_clear_pixmap(ctx, pix);
            // fz_fill_pixmap_with_color(ctx, pix, )
            dev = fz_new_draw_device(ctx, ctm, pix);
            fz_run_page_contents(ctx, page, dev, fz_identity, NULL);
            fz_close_device(ctx, dev);
            fz_drop_device(ctx, dev);
            bitmap = NewRenderedFzPixmap(ctx, pix);
        }
        fz_always(ctx) {
            fz_drop_pixmap(ctx, pix);
        }
        fz_catch(ctx) {
            delete bitmap;
            return nullptr;
        }
    }

    return bitmap;
}

// don't delete the result
IPageElement* EngineMupdf::GetElementAtPos(int pageNo, PointF pt) {
    FzPageInfo* pageInfo = GetFzPageInfoFast(pageNo);
    return FzGetElementAtPos(pageInfo, pt);
}

// TOOD: optimize by returning reference or pointer so that
// we don't have to re-create the Vec every time
Vec<IPageElement*> EngineMupdf::GetElements(int pageNo) {
    auto pageInfo = GetFzPageInfoFast(pageNo);
    if (!pageInfo) {
        return Vec<IPageElement*>();
    }

    BuildElementsInfo(pageInfo);
    return pageInfo->allElements;
}

void HandleLinkMupdf(EngineMupdf* e, IPageDestination* dest, ILinkHandler* linkHandler) {
    CrashIf(kindDestinationMupdf != dest->GetKind());
    PageDestinationMupdf* link = (PageDestinationMupdf*)dest;
    CrashIf(!(link->outline || link->link));
    const char* uri = link->outline ? link->outline->uri : nullptr;
    if (!link->outline) {
        uri = link->link->uri;
    }
    if (IsExternalLink(uri)) {
        linkHandler->LaunchURL(uri);
        return;
    }

    // those locks must be taken in this order
    // we need to lock pagesAccess because it might
    // be taken below
    ScopedCritSec csPages(&e->pagesAccess);
    ScopedCritSec cs(e->ctxAccess);

    float x, y;
    float zoom = 0.f; // TODO: used to have zoom but mupdf changed outline
    int pageNo = -1;
    fz_var(pageNo);
    fz_try(e->ctx) {
        fz_location loc = fz_resolve_link(e->ctx, e->_doc, uri, &x, &y);
        pageNo = fz_page_number_from_location(e->ctx, e->_doc, loc);
    }
    fz_catch(e->ctx) {
        logfa("HandleLinkMupdf: fz_resolve_link() for '%s' failed\n", uri);
    }
    if (pageNo < 0) {
        // TODO: more?
        // CrashIf(true);
        return;
    }

    RectF r(x, y, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
    auto ctrl = linkHandler->GetDocController();
    ctrl->ScrollTo(pageNo + 1, r, zoom);
}

bool EngineMupdf::HandleLink(IPageDestination* dest, ILinkHandler* linkHandler) {
    Kind k = dest->GetKind();
    if (k == kindDestinationMupdf) {
        HandleLinkMupdf(this, dest, linkHandler);
        return true;
    }
    linkHandler->GotoLink(dest);
    return true;
}

RenderedBitmap* EngineMupdf::GetImageForPageElement(IPageElement* ipel) {
    CrashIf(kindPageElementImage != ipel->GetKind());
    auto pel = (PageElementImage*)ipel;
    auto r = pel->rect;
    int pageNo = pel->pageNo;
    int imageID = pel->imageID;
    return GetPageImage(pageNo, r, imageID);
}

bool EngineMupdf::BenchLoadPage(int pageNo) {
    return GetFzPageInfo(pageNo, false) != nullptr;
}

fz_matrix EngineMupdf::viewctm(int pageNo, float zoom, int rotation) {
    const fz_rect tmpRc = ToFzRect(PageMediabox(pageNo));
    return FzCreateViewCtm(tmpRc, zoom, rotation);
}

fz_matrix EngineMupdf::viewctm(fz_page* page, float zoom, int rotation) const {
    fz_rect bounds;
    fz_var(bounds);
    fz_try(ctx) {
        bounds = fz_bound_page(ctx, page);
    }
    fz_catch(ctx) {
        bounds = {};
    }
    if (fz_is_empty_rect(bounds)) {
        bounds = {0, 0, 612, 792};
    }
    return FzCreateViewCtm(bounds, zoom, rotation);
}

RenderedBitmap* EngineMupdf::GetPageImage(int pageNo, RectF rect, int imageIdx) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo->page) {
        return nullptr;
    }
    const auto& images = pageInfo->images;
    bool outOfBounds = imageIdx >= images.isize();
    fz_rect imgRect = images.at(imageIdx)->rect;
    bool badRect = ToRectF(imgRect) != rect;
    CrashIf(outOfBounds);
    CrashIf(badRect);
    if (outOfBounds || badRect) {
        return nullptr;
    }

    ScopedCritSec scope(ctxAccess);

    fz_image* image = FzFindImageAtIdx(ctx, pageInfo, imageIdx);
    CrashIf(!image);
    if (!image) {
        return nullptr;
    }

    RenderedBitmap* bmp = nullptr;
    fz_pixmap* pixmap = nullptr;
    fz_var(pixmap);
    fz_var(bmp);

    fz_try(ctx) {
        // TODO(port): not sure if should provide subarea, w and h
        pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
        bmp = NewRenderedFzPixmap(ctx, pixmap);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx) {
        bmp = nullptr;
    }

    return bmp;
}

PageText EngineMupdf::ExtractPageText(int pageNo) {
    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);
    if (!pageInfo) {
        return {};
    }

    ScopedCritSec scope(ctxAccess);

    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_stext_options opts{};
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, pageInfo->page, &opts);
    }
    fz_catch(ctx) {
    }
    if (!stext) {
        return {};
    }
    PageText res;
    // TODO: convert to return PageText
    WCHAR* text = FzTextPageToStr(stext, &res.coords);
    fz_drop_stext_page(ctx, stext);
    res.text = text;
    res.len = (int)str::Len(text);
    return res;
}

static void pdf_extract_fonts(fz_context* ctx, pdf_obj* res, Vec<pdf_obj*>& fontList, Vec<pdf_obj*>& resList) {
    if (!res || pdf_mark_obj(ctx, res)) {
        return;
    }
    resList.Append(res);

    pdf_obj* fonts = pdf_dict_gets(ctx, res, "Font");
    for (int k = 0; k < pdf_dict_len(ctx, fonts); k++) {
        pdf_obj* font = pdf_resolve_indirect(ctx, pdf_dict_get_val(ctx, fonts, k));
        if (font && !fontList.Contains(font)) {
            fontList.Append(font);
        }
    }
    // also extract fonts for all XObjects (recursively)
    pdf_obj* xobjs = pdf_dict_gets(ctx, res, "XObject");
    for (int k = 0; k < pdf_dict_len(ctx, xobjs); k++) {
        pdf_obj* xobj = pdf_dict_get_val(ctx, xobjs, k);
        pdf_obj* xres = pdf_dict_gets(ctx, xobj, "Resources");
        pdf_extract_fonts(ctx, xres, fontList, resList);
    }
}

char* EngineMupdf::ExtractFontList() {
    Vec<pdf_obj*> fontList;
    Vec<pdf_obj*> resList;

    // collect all fonts from all page objects
    int nPages = PageCount();
    for (int i = 1; i <= nPages; i++) {
        auto pageInfo = GetFzPageInfo(i, false);
        if (!pageInfo) {
            continue;
        }
        fz_page* fzpage = pageInfo->page;
        if (!fzpage) {
            continue;
        }

        ScopedCritSec scope(ctxAccess);
        pdf_page* page = pdf_page_from_fz_page(ctx, fzpage);
        fz_try(ctx) {
            pdf_obj* resources = pdf_page_resources(ctx, page);
            pdf_extract_fonts(ctx, resources, fontList, resList);
            pdf_annot* annot;
            for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot)) {
                pdf_obj* o = pdf_annot_ap(ctx, annot);
                if (o) {
                    // TODO(port): not sure this is the right thing
                    resources = pdf_xobject_resources(ctx, o);
                    pdf_extract_fonts(ctx, resources, fontList, resList);
                }
            }
        }
        fz_catch(ctx) {
        }
    }

    // start ctxAccess scope here so that we don't also have to
    // ask for pagesAccess (as is required for GetFzPage)
    ScopedCritSec scope(ctxAccess);

    for (pdf_obj* res : resList) {
        pdf_unmark_obj(ctx, res);
    }

    StrVec fonts;
    for (size_t i = 0; i < fontList.size(); i++) {
        const char *name = nullptr, *type = nullptr, *encoding = nullptr;
        AutoFreeStr anonFontName;
        bool embedded = false;
        fz_try(ctx) {
            pdf_obj* font = fontList.at(i);
            pdf_obj* font2 = pdf_array_get(ctx, pdf_dict_gets(ctx, font, "DescendantFonts"), 0);
            if (!font2) {
                font2 = font;
            }

            name = pdf_to_name(ctx, pdf_dict_getsa(ctx, font2, "BaseFont", "Name"));
            bool needAnonName = str::IsEmpty(name);
            if (needAnonName && font2 != font) {
                name = pdf_to_name(ctx, pdf_dict_getsa(ctx, font, "BaseFont", "Name"));
                needAnonName = str::IsEmpty(name);
            }
            if (needAnonName) {
                anonFontName.Set(str::Format("<#%d>", pdf_obj_parent_num(ctx, font2)));
                name = anonFontName;
            }
            embedded = false;
            pdf_obj* desc = pdf_dict_gets(ctx, font2, "FontDescriptor");
            if (desc && (pdf_dict_gets(ctx, desc, "FontFile") || pdf_dict_getsa(ctx, desc, "FontFile2", "FontFile3"))) {
                embedded = true;
            }
            if (embedded && str::Len(name) > 7 && name[6] == '+') {
                name += 7;
            }

            type = pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Subtype"));
            if (font2 != font) {
                const char* type2 = pdf_to_name(ctx, pdf_dict_gets(ctx, font2, "Subtype"));
                if (str::Eq(type2, "CIDFontType0")) {
                    type = "Type1 (CID)";
                } else if (str::Eq(type2, "CIDFontType2")) {
                    type = "TrueType (CID)";
                }
            }
            if (str::Eq(type, "Type3")) {
                embedded = pdf_dict_gets(ctx, font2, "CharProcs") != nullptr;
            }

            encoding = pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Encoding"));
            if (str::Eq(encoding, "WinAnsiEncoding")) {
                encoding = "Ansi";
            } else if (str::Eq(encoding, "MacRomanEncoding")) {
                encoding = "Roman";
            } else if (str::Eq(encoding, "MacExpertEncoding")) {
                encoding = "Expert";
            }
        }
        fz_catch(ctx) {
            continue;
        }
        CrashIf(!name || !type || !encoding);

        str::Str info;
        if (name[0] < 0 && MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, name, -1, nullptr, 0)) {
            info.Append(strconv::ToMultiByte(name, 936, CP_UTF8));
        } else {
            info.Append(name);
        }
        if (!str::IsEmpty(encoding) || !str::IsEmpty(type) || embedded) {
            info.Append(" (");
            if (!str::IsEmpty(type)) {
                info.AppendFmt("%s; ", type);
            }
            if (!str::IsEmpty(encoding)) {
                info.AppendFmt("%s; ", encoding);
            }
            if (embedded) {
                info.Append("embedded; ");
            }
            info.RemoveAt(info.size() - 2, 2);
            info.Append(")");
        }

        if (info.IsEmpty()) {
            continue;
        }
        char* fontName = info.LendData();
        fonts.AppendIfNotExists(fontName);
    }
    if (fonts.size() == 0) {
        return nullptr;
    }

    fonts.SortNatural();
    char* res = Join(fonts, "\n");
    return res;
}

static const char* DocumentPropertyToMupdfMetadataKey(DocumentProperty prop) {
    switch (prop) {
        case DocumentProperty::Title:
            return FZ_META_INFO_TITLE;
        case DocumentProperty::Author:
            return FZ_META_INFO_AUTHOR;
        case DocumentProperty::Subject:
            return "info:Subject";
        case DocumentProperty::PdfProducer:
            return FZ_META_INFO_PRODUCER;
        case DocumentProperty::CreatorApp:
            return "info:Creator"; // not sure if the same meaning
        case DocumentProperty::CreationDate:
            return "info:CreationDate";
        case DocumentProperty::ModificationDate:
            return "info:ModDate";
    }
    return nullptr;
}

char* EngineMupdf::GetProperty(DocumentProperty prop) {
    const char* key = DocumentPropertyToMupdfMetadataKey(prop);
    if (key) {
        char buf[1024]{};
        int bufSize = (int)dimof(buf);
        int n = fz_lookup_metadata(ctx, _doc, key, buf, bufSize);
        if (n > 0) {
            if (n > bufSize) {
                // can be bigger if output truncated
                n = bufSize - 1;
                buf[bufSize - 1] = 0; // not sure if necessary
            }
            char* s = str::Dup(buf, (size_t)n - 1);
            return s;
        }
    }
    if (!pdfdoc) {
        return nullptr;
    }

    if (DocumentProperty::PdfVersion == prop) {
        int major = pdfdoc->version / 10, minor = pdfdoc->version % 10;
        pdf_crypt* crypt = pdfdoc->crypt;
        if (1 == major && 7 == minor && pdf_crypt_version(ctx, crypt) == 5) {
            if (pdf_crypt_revision(ctx, crypt) == 5) {
                return str::Format("%d.%d Adobe Extension Level %d", major, minor, 3);
            }
            if (pdf_crypt_revision(ctx, crypt) == 6) {
                return str::Format("%d.%d Adobe Extension Level %d", major, minor, 8);
            }
        }
        return str::Format("%d.%d", major, minor);
    }

    if (DocumentProperty::PdfFileStructure == prop) {
        StrVec fstruct;
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, pdfInfo, "Linearized"))) {
            fstruct.Append("linearized");
        }
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, pdfInfo, "Marked"))) {
            fstruct.Append("tagged");
        }
        if (pdf_dict_gets(ctx, pdfInfo, "OutputIntents")) {
            int n = pdf_array_len(ctx, pdf_dict_gets(ctx, pdfInfo, "OutputIntents"));
            for (int i = 0; i < n; i++) {
                pdf_obj* intent = pdf_array_get(ctx, pdf_dict_gets(ctx, pdfInfo, "OutputIntents"), i);
                CrashIf(!str::StartsWith(pdf_to_name(ctx, intent), "GTS_"));
                const char* s = pdf_to_name(ctx, intent) + 4;
                fstruct.Append(s);
            }
        }
        if (fstruct.Size() == 0) {
            return nullptr;
        }
        return Join(fstruct, ",");
    }

    if (DocumentProperty::UnsupportedFeatures == prop) {
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, pdfInfo, "Unsupported_XFA"))) {
            return str::Dup("XFA");
        }
        return nullptr;
    }

    if (DocumentProperty::FontList == prop) {
        return ExtractFontList();
    }

    static struct {
        DocumentProperty prop;
        const char* name;
    } pdfPropNames[] = {
        {DocumentProperty::Title, "Title"},
        {DocumentProperty::Author, "Author"},
        {DocumentProperty::Subject, "Subject"},
        {DocumentProperty::Copyright, "Copyright"},
        {DocumentProperty::CreationDate, "CreationDate"},
        {DocumentProperty::ModificationDate, "ModDate"},
        {DocumentProperty::CreatorApp, "Creator"},
        {DocumentProperty::PdfProducer, "Producer"},
    };
    for (int i = 0; i < dimof(pdfPropNames); i++) {
        if (pdfPropNames[i].prop == prop) {
            // _info is guaranteed not to contain any indirect references,
            // so no need for ctxAccess
            pdf_obj* obj = pdf_dict_gets(ctx, pdfInfo, pdfPropNames[i].name);
            if (!obj) {
                return nullptr;
            }
            WCHAR* s = PdfToWstr(ctx, obj);
            PdfCleanStringInPlace(s);
            char* res = ToUtf8(s);
            str::Free(s);
            return res;
        }
    }
    return nullptr;
};

ByteSlice EngineMupdf::GetFileData() {
    if (!pdfdoc) {
        return {};
    }

    ByteSlice res;
    ScopedCritSec scope(ctxAccess);

    fz_var(res);
    fz_try(ctx) {
        res = FzExtractStreamData(ctx, pdfdoc->file);
    }
    fz_catch(ctx) {
        res = {};
    }

    if (!res.empty()) {
        return res;
    }

    auto path = FilePath();
    if (!path) {
        return {};
    }
    return file::ReadFile(path);
}

bool EngineMupdf::SaveFileAs(const char* dstPath) {
    ByteSlice d = GetFileData();
    if (!d.empty()) {
        bool ok = file::WriteFile(dstPath, d);
        d.Free();
        return ok;
    }
    auto srcPath = FilePath();
    if (!srcPath) {
        return false;
    }
    bool ok = file::Copy(dstPath, srcPath, false);
    return ok;
}

const pdf_write_options pdf_default_write_options2 = {
    0,  /* do_incremental */
    0,  /* do_pretty */
    0,  /* do_ascii */
    0,  /* do_compress */
    0,  /* do_compress_images */
    0,  /* do_compress_fonts */
    0,  /* do_decompress */
    0,  /* do_garbage */
    0,  /* do_linear */
    0,  /* do_clean */
    0,  /* do_sanitize */
    0,  /* do_appearance */
    0,  /* do_encrypt */
    0,  /* dont_regenerate_id */
    ~0, /* permissions */
    "", /* opwd_utf8[128] */
    "", /* upwd_utf8[128] */
};

// re-save current pdf document using mupdf (as opposed to just saving the data)
// this is used after the PDF was modified by the user (e.g. by adding / changing
// annotations).
// if filePath is not given, we save under the same name
// TODO: if the file is locked, this might fail.
bool EngineMupdfSaveUpdated(EngineBase* engine, const char* path, std::function<void(const char*)> showErrorFunc) {
    CrashIf(!engine);
    if (!engine) {
        return false;
    }
    EngineMupdf* epdf = (EngineMupdf*)engine;
    if (!epdf->pdfdoc) {
        return false;
    }
    if (!EngineMupdfHasUnsavedAnnotations(engine)) {
        return false;
    }

    auto timeStart = TimeGet();
    const char* currPath = engine->FilePath();
    if (str::IsEmpty(path)) {
        path = currPath;
    }
    fz_context* ctx = epdf->ctx;

    pdf_write_options save_opts{};
    save_opts = pdf_default_write_options2;
    // TODO: if saving to a new file, don't do incremental and linearlize?
    // save_opts.do_linear = 1;
    save_opts.do_incremental = pdf_can_be_saved_incrementally(ctx, epdf->pdfdoc);
    save_opts.do_compress = 1;
    save_opts.do_compress_images = 1;
    save_opts.do_compress_fonts = 1;
    if (epdf->pdfdoc->redacted) {
        save_opts.do_garbage = 1;
    }

    bool ok = false;
    fz_var(ok);
    fz_try(ctx) {
        pdf_save_document(ctx, epdf->pdfdoc, path, &save_opts);
        ok = true;
        auto dur = TimeSinceInMs(timeStart);
        logf("Saved annotations to '%s' in  %.2f ms, incremental: %d\n", path, dur, save_opts.do_incremental);
    }
    fz_catch(ctx) {
        const char* mupdfErr = fz_caught_message(epdf->ctx);
        logf("Saving '%s' failed with: '%s'\n", path, mupdfErr);
        if (showErrorFunc) {
            showErrorFunc(mupdfErr);
        }
    }

    // TOOD: what if not ok?
    // note: this should be short-lived as we should re-load the file
    if (ok) {
        epdf->modifiedAnnotations = false;
    }
    return ok;
}

bool EngineMupdf::HasClipOptimizations(int pageNo) {
    if (!pdfdoc) {
        return false;
    }

    FzPageInfo* pageInfo = GetFzPageInfoFast(pageNo);
    if (!pageInfo || !pageInfo->page) {
        return false;
    }

    fz_rect mbox = ToFzRect(PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (auto& img : pageInfo->images) {
        fz_rect ir = img->rect;
        if (FzRectOverlap(mbox, ir) >= 0.9f) {
            return false;
        }
    }
    return true;
}

char* EngineMupdf::GetPageLabel(int pageNo) const {
    if (!pageLabels || pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabel(pageNo);
    }

    char* res = pageLabels->at(pageNo - 1);
    return str::Dup(res);
}

int EngineMupdf::GetPageByLabel(const char* label) const {
    if (!pdfdoc) {
        // non-pdf documents don't have labels so label is just a page number as string
        return EngineBase::GetPageByLabel(label);
    }
    int pageNo = 0;
    if (pageLabels) {
        pageNo = pageLabels->Find(label) + 1;
    }

    if (!pageNo) {
        return EngineBase::GetPageByLabel(label);
    }

    return pageNo;
}

bool IsEngineMupdfSupportedFileType(Kind kind) {
    if (kind == kindFilePDF) {
        return true;
    }
    if (kind == kindFileEpub) {
        return true;
    }
    if (kind == kindFileFb2) {
        return true;
    }
    if (kind == kindFileFb2z) {
        return true;
    }
    if (kind == kindFileHTML) {
        return true;
    }
    if (kind == kindFileSvg) {
        return true;
    }
    if (kind == kindFileXps) {
        return true;
    }
    if (kind == kindFileTxt) {
        return true;
    }
    if (kind == kindFilePalmDoc) {
        return true;
    }
    return false;
}

EngineBase* CreateEngineMupdfFromFile(const char* path, Kind kind, int displayDPI, PasswordUI* pwdUI) {
    if (str::IsEmpty(path)) {
        return nullptr;
    }
    if (kind == kindFileFb2z) {
        AutoDelete archive = OpenZipArchive(path, true);
        if (!archive) {
            return nullptr;
        }
        auto files = archive->GetFileInfos();
        if (files.size() != 1) {
            return nullptr;
        }
        ByteSlice d = archive->GetFileDataById(0);
        if (d.empty()) {
            return nullptr;
        }
        IStream* strm = CreateStreamFromData(d);
        d.Free();
        ScopedComPtr<IStream> stream(strm);
        if (!stream) {
            return nullptr;
        }
        EngineMupdf* engine = new EngineMupdf();
        if (displayDPI < 70) {
            displayDPI = 96;
        }
        engine->displayDPI = displayDPI;
        if (!engine->Load(stream, "foo.fb2", pwdUI)) {
            delete engine;
            return nullptr;
        }
        engine->SetFilePath(path);
        return engine;
    }
    EngineMupdf* engine = new EngineMupdf();
    if (displayDPI < 70) {
        displayDPI = 96;
    }
    engine->displayDPI = displayDPI;
    if (!engine->Load(path, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineMupdfFromStream(IStream* stream, const char* nameHint, PasswordUI* pwdUI) {
    EngineMupdf* engine = new EngineMupdf();
    if (!engine->Load(stream, nameHint, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineMupdfFromData(const ByteSlice& data, const char* nameHint, PasswordUI* pwdUI) {
    EngineMupdf* engine = new EngineMupdf();
    IStream* stream = CreateStreamFromData(data);
    if (!engine->Load(stream, nameHint, pwdUI)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

// it's fast because we only collect pointers from FzPageInfo
void EngineMupdfGetAnnotations(EngineBase* engine, Vec<Annotation*>& annotsOut) {
    annotsOut.Clear();

    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e->pdfdoc) {
        return;
    }
    ScopedCritSec scope(&e->pagesAccess);
    for (int i = 1; i <= e->pageCount; i++) {
        const auto& pi = e->GetFzPageInfo(i, true);
        annotsOut.Append(pi->annotations);
    }
}

bool EngineMupdfHasUnsavedAnnotations(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return false;
    }
#if 0
    // TODO: this fails in https://github.com/sumatrapdfreader/sumatrapdf/issues/3448
    // because pdf_has_unsaved_changes() returns true even though pdf_was_repaired()
    // returns false (even though the doc was modified in pdf_test_outline() becasue
    // "Bad or missing last pointer in outline tree, repairing"

    // pdf_has_unsaved_changes() also returns true if the file was auto-repaired
    // at loading time, which is not something we want, so only rely on it
    // when we know it wasn't repaired.
    if (!pdf_was_repaired(epdf->ctx, epdf->pdfdoc)) {
        return pdf_has_unsaved_changes(epdf->ctx, epdf->pdfdoc);
    }
#endif
    return epdf->modifiedAnnotations;
}

bool EngineMupdfSupportsAnnotations(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    return (epdf->pdfdoc != nullptr);
}

// caller must free
ByteSlice EngineMupdfLoadAttachment(EngineBase* engine, int attachmentNo) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return {};
    }

    ByteSlice res = PdfLoadAttachment(epdf->ctx, epdf->pdfdoc, attachmentNo);
    return res;
}

// if an elements fully obscures another, remove it from the list
static bool RemoveHeWhoFullyContains(Vec<Annotation*>& els) {
    int n = els.Size();
    CrashIf(n < 2);
    for (int i = 0; i < n; i++) {
        RectF r1 = els[i]->bounds;
        for (int j = 0; j < n; j++) {
            if (j == i) {
                continue; // skip checking against self
            }
            RectF r2 = els[j]->bounds;
            if (RectFullyContains(r1, r2)) {
                // logfa("el %d fully obscures %d\n", i, j);
                els.RemoveAtFast(i);
                return true;
            }
        }
    }
    return false;
}

Annotation* EngineMupdfGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, Annotation* preferredAnnot) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return nullptr;
    }
    FzPageInfo* pi = epdf->GetFzPageInfo(pageNo, true);
    if (!pi) {
        return nullptr;
    }

    ScopedCritSec cs(epdf->ctxAccess);
    Vec<Annotation*> els;
    for (auto& annot : pi->annotations) {
        auto& atp = annot->type;
        RectF bounds = annot->bounds;
        if (!bounds.Contains(pos)) {
            continue;
        }
        els.Append(annot);
    }
    if (els.Size() == 0) {
        return nullptr;
    }
    for (const auto& a : els) {
        if (a == preferredAnnot) {
            return preferredAnnot;
        }
    }

    // pick the best
Encore:
    int n = els.Size();
    if (n == 1) {
        return els[0];
    }
    bool didRemove = RemoveHeWhoFullyContains(els);
    if (didRemove) {
        CrashIf(els.Size() != n - 1);
        goto Encore;
    }
    return els[0];
}

// Note: this code is compiled in release mode even if debug build so
// DEBUG is not defined so we can't do #if defined(DEBUG) here
// so we use this runtime boolean instead
static bool gSkipAnnotatoinValidation = true;

// check that pageInfo->annotations has the same info as in mupdf
NO_INLINE void ValidateAnnotationsInSync(EngineMupdf* e, FzPageInfo* pageInfo) {
    if (gSkipAnnotatoinValidation) {
        return;
    }
    // TODO: write me
}

// in a function so that we can set a breakpoint or add logging
// to easily trace all places that modify annotations
NO_INLINE void MarkNotificationAsModified(EngineMupdf* e, Annotation* annot, AnnotationChange change) {
    e->modifiedAnnotations = true;
    if (!e->pdfdoc) {
        return;
    }
    int pageNo = annot->pageNo;
    CrashIf(pageNo < 1 || pageNo > e->pageCount);
    int pageIdx = pageNo - 1;

    // EngineMupdf is the ultimate source of truth for Annotation* list
    // all other places only get references to Annotation* created
    // inside EngineMupdf.
    // It would be easier to re-create Annotation* list after each change
    // to annotations inside mupdf but we don't want loose the identity
    // so on add /remove we update the list manually
    // on change we assume Annotation* lives inside EngineMupdf
    ScopedCritSec scope(&e->pagesAccess);
    FzPageInfo* pageInfo = e->pages[pageIdx];

    if (change == AnnotationChange::Remove) {
        int sizeBefore = pageInfo->annotations.isize();
        int removedPos = pageInfo->annotations.Remove(annot);
        CrashIf(removedPos < 0); // must exist
        int sizeNow = pageInfo->annotations.isize();
        CrashIf(sizeBefore != sizeNow + 1);
        ValidateAnnotationsInSync(e, pageInfo);
    } else if (change == AnnotationChange::Add) {
        int sizeBefore = pageInfo->annotations.isize();
        int pos = pageInfo->annotations.Find(annot);
        CrashIf(pos >= 0); // shouldn't exist
        pageInfo->annotations.Append(annot);
        int sizeNow = pageInfo->annotations.isize();
        CrashIf(sizeBefore != sizeNow - 1);
        ValidateAnnotationsInSync(e, pageInfo);
    } else {
        CrashIf(change != AnnotationChange::Modify);
    }
    RebuildCommentsFromAnnotations(e->ctx, pageInfo);
    pageInfo->elementsNeedRebuilding = true;
}

// creates Annotation wrapper around pdf_annot
Annotation* MakeAnnotationWrapper(EngineMupdf* engine, pdf_annot* annot, int pageNo) {
    CrashIf(pageNo < 1);
    CrashIf(!engine->pdfdoc);
    ScopedCritSec cs(engine->ctxAccess);

    AnnotationType typ = AnnotationType::Unknown;
    fz_rect bounds;

    fz_context* ctx = engine->ctx;
    fz_try(ctx) {
        auto tp = pdf_annot_type(ctx, annot);
        bounds = pdf_bound_annot(ctx, annot);
        typ = AnnotationTypeFromPdfAnnot(tp);
    }
    fz_catch(ctx) {
        // do nothing
    }

    if (typ == AnnotationType::Unknown) {
        // unsupported type or exception in fz_try
        return nullptr;
    }

    Annotation* res = new Annotation();
    res->engine = engine;
    res->pageNo = pageNo;
    res->pdfannot = annot;
    res->bounds = ToRectF(bounds);
    res->type = typ;
    return res;
}
