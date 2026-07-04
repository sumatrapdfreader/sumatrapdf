/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/pdf.h>
#include <mupdf/helpers/pkcs7-windows.h>
#include "../mupdf/source/fitz/color-imp.h"
}

#include "base/Base.h"
#include "base/Archive.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Win.h"
#include "base/Timer.h"

#include "wingui/UIModels.h"

#include "Annotation.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "EngineAll.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "Settings.h"

#include "base/Log.h"

// A5
static float layoutA5DxPt = 420.F;
static float layoutA5DyPt = 595.F;

// A4
static float layoutA4DxPt = 595.F;
static float layoutA4DyPt = 842.F;

static float layoutFontEm = 11.F;

// in mupdf_load_system_font.c
extern "C" void install_load_windows_font_funcs(fz_context* ctx);

static AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp) {
    return (AnnotationType)tp;
}

Kind kindEngineMupdf = "enginePdf";

// Whether to enable mupdf's JavaScript engine for newly loaded PDFs (form-field
// calculate/validate/format). Set by the app from the DisableJavaScript pref;
// PdfPreview/PdfFilter don't link GlobalPrefs, so they keep the default.
static bool gDisableFormJavaScript = false;
void EngineMupdfSetDisableJavaScript(bool disable) {
    gDisableFormJavaScript = disable;
}

// Whether a PDF may load an image from an external sibling file (issue #3731).
// Set by the app from the AllowExternalImages pref; off by default (and in the
// PdfPreview/PdfFilter DLLs, which don't link GlobalPrefs).
static bool gAllowExternalImages = false;
void EngineMupdfSetAllowExternalImages(bool allow) {
    gAllowExternalImages = allow;
}

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
        // Unknown progress avoids MuPDF pre-counting annotations; the cookie is only used for aborting.
        cookie.progress_max = (size_t)-1;
    }
    void Abort() override { cookie.abort = 1; }
    void* GetData() override { return (void*)&cookie; }
};

// copy of fz_is_external_link without ctx
static bool IsExternalLink(Str uri) {
    if (!uri) {
        return false;
    }
    int i = 0;
    while (i < uri.len && uri.s[i] >= 'a' && uri.s[i] <= 'z') {
        ++i;
    }
    return i < uri.len && uri.s[i] == ':';
}

static Str FzGetURL(fz_link* link, fz_outline* outline) {
    if (link) {
        return Str(link->uri);
    }
    if (outline) {
        return Str(outline->uri);
    }
    return {};
}

struct PageDestinationMupdf : IPageDestination {
    fz_outline* outline = nullptr;
    fz_link* link = nullptr;

    Str value;
    Str name;

    // anchor (x, y) on the destination page resolved from the link URI;
    // -1 means "not resolved" (e.g. external URL or file launch).
    float destX = -1.f;
    float destY = -1.f;
    // /XYZ zoom level requested by the link (1.0 = 100%). 0 means
    // "not specified" — caller should use document default.
    float destZoom = 0.f;

    PageDestinationMupdf(fz_link* l, fz_outline* o) {
        // exactly one must be provided
        kind = kindDestinationMupdf;
        link = l;
        outline = o;
    }

    RectF GetRect2() override {
        if (outline) {
            // needed for -named-dest called from LinkHandler::ScrollTo
            RectF r{outline->x, outline->y, 0, 0};
            return r;
        }
        return rect;
    }

    RectF GetDestPoint2() override {
        if (outline) {
            return RectF{outline->x, outline->y, 0, 0};
        }
        if (destY >= 0.f) {
            return RectF{destX, destY, 0, 0};
        }
        return {};
    }

    float GetZoom2() override { return destZoom; }

    ~PageDestinationMupdf() override {
        str::Free(value);
        str::Free(name);
    }

    Str GetValue2() override;
    Str GetName2() override;
};

Str PageDestinationMupdf::GetValue2() {
    if (value) {
        return value;
    }

    Str uri = FzGetURL(link, outline);
    if (uri && IsExternalLink(uri)) {
        value = str::Dup(uri.s);
        url::DecodeInPlace(value);
    }
    return value;
}

Str PageDestinationMupdf::GetName2() {
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

static int ResolveLink(fz_context* ctx, fz_document* doc, Str uri, float* xp, float* yp, float* zoomp = nullptr) {
    if (!uri) {
        return -1;
    }
    int pageNo = -1;
    fz_link_dest ldest{};

    fz_var(ldest);
    fz_var(pageNo);
    fz_try(ctx) {
        ldest = fz_resolve_link_dest(ctx, doc, CStrTemp(uri));
        pageNo = fz_page_number_from_location(ctx, doc, ldest.loc);
    }
    fz_catch(ctx) {
        fz_warn(ctx, "fz_resolve_link_dest failed");
        fz_report_error(ctx);
        pageNo = -1;
    }
    if (pageNo < 0) {
        return -1;
    }
    if (xp) {
        *xp = isnan(ldest.x) ? 0.f : ldest.x;
    }
    if (yp) {
        *yp = isnan(ldest.y) ? 0.f : ldest.y;
    }
    if (zoomp) {
        float z = isnan(ldest.zoom) ? 0.f : ldest.zoom;
        // mupdf reports zoom as percentage (100 = 100%); we use 1.0 as 100%.
        *zoomp = z / 100.f;
    }
    return pageNo + 1;
}

static int FzGetPageNo(fz_context* ctx, fz_document* doc, fz_link* link, fz_outline* outline) {
    float x, y;
    Str uri = FzGetURL(link, outline);
    int pageNo = ResolveLink(ctx, doc, uri, &x, &y);
    return pageNo;
}

// MuPDF html/md link URIs for relative hrefs are built with an empty base file,
// so e.g. [other](other.md) becomes "/other.md". Treat those like the HTML
// ebook engine: launch a local file Sumatra can open.
static void SkipLeadingPathSeparators(Str& path) {
    while (!str::IsEmpty(path) && (path.s[0] == '/' || path.s[0] == '\\')) {
        path.s++;
        path.len--;
    }
}

static bool IsMupdfLocalFileLink(Str uri, TempStr* pathOut, Str* fragmentOut) {
    if (!uri || uri.s[0] == '#') {
        return false;
    }
    if (str::StartsWith(uri, "file:") || IsExternalUrl(uri) || IsExternalLink(uri)) {
        return false;
    }

    TempStr path = str::DupTemp(uri);
    Str pathStr = path;
    Str fragment = str::SliceFromChar(pathStr, '#');
    if (fragment) {
        pathStr = Str(pathStr.s, (int)(fragment.s - pathStr.s));
        fragment = Str(fragment.s + 1);
    }
    // MuPDF uses unix paths; strip a leading slash from relative URIs.
    SkipLeadingPathSeparators(pathStr);
    if (!pathStr) {
        return false;
    }
    path = str::ReplaceTemp(pathStr, StrL("/"), StrL("\\"));

    Kind kind = GuessFileTypeFromName(path);
    if (!IsEngineMupdfSupportedFileType(kind)) {
        return false;
    }
    *pathOut = path;
    *fragmentOut = fragment;
    return true;
}

static IPageDestination* NewPageDestinationMupdf(fz_context* ctx, fz_document* doc, fz_link* link,
                                                 fz_outline* outline) {
    ReportIf(link && outline);
    ReportIf(!link && !outline);
    Str uri = FzGetURL(link, outline);
    Str maybePath = uri;

    if (str::Skip(maybePath, "file:")) {
        // decode: file:path%20to_file.pdf#page=1

        // this is to handle file:// and
        // file:/// (which I assume is a mistake in PDF)
        str::Skip(maybePath, "/");
        str::Skip(maybePath, "/");
        str::Skip(maybePath, "/");

        TempStr path = str::DupTemp(maybePath);
        Str pathStr = path;
        Str destStr = str::SliceFromChar(pathStr, '#');
        if (destStr) {
            pathStr = Str(pathStr.s, (int)(destStr.s - pathStr.s));
            destStr = Str(destStr.s + 1);
        }
        // mupdf url-encodes paths so we un-decode them
        TempStr pathNul = str::DupTemp(pathStr);
        fz_urldecode(pathNul.s);
        fz_cleanname(pathNul.s);

        // mupdf does unix path, we want windows
        path = str::ReplaceTemp(pathNul, StrL("/"), StrL("\\"));
        if (destStr) {
            TempStr destNul = str::DupTemp(destStr);
            fz_urldecode(destNul.s);
            destStr = destNul;
        }

        logf("NewPageDestinationMupdf: path='%s', dest='%s'\n", path, destStr);
        if (len(path) == 0) {
            // degenerate bare "file:" uri (seen in broken PDFs)
            return nullptr;
        }
        auto res = new PageDestinationFile(path, destStr);
        res->rect = FzGetRectF(link, outline);
        return res;
    }

    if (IsExternalUrl(uri)) {
        auto res = new PageDestinationURL(uri);
        res->rect = FzGetRectF(link, outline);
        return res;
    }

    // Try to resolve the URI as an internal document location first. EPUB
    // chapter links (e.g. "OEBPS/ch1.htm") point inside the same document and
    // must navigate internally, not launch an external file. Only when the URI
    // doesn't resolve internally do we treat a relative href to a supported file
    // as a sibling file to launch (e.g. markdown "[other](other.md)").
    float x = 0, y = 0, z = 0;
    int pageNo = ResolveLink(ctx, doc, uri, &x, &y, &z);

    if (pageNo <= 0) {
        TempStr localPath;
        Str localFragment;
        if (IsMupdfLocalFileLink(uri, &localPath, &localFragment)) {
            auto res = new PageDestinationFile(localPath, localFragment);
            res->rect = FzGetRectF(link, outline);
            return res;
        }
    }

    auto dest = new PageDestinationMupdf(link, outline);
    dest->rect = FzGetRectF(link, outline);
    dest->pageNo = pageNo;
    if (pageNo > 0) {
        dest->destX = x;
        dest->destY = y;
        dest->destZoom = z;
    }
    // when not resolved destX / destY keep their -1 sentinel
    return dest;
}

static PageElementDestination* NewLinkDestination(int srcPageNo, fz_context* ctx, fz_document* doc, fz_link* link,
                                                  fz_outline* outline) {
    auto dest = NewPageDestinationMupdf(ctx, doc, link, outline);
    if (!dest) {
        return nullptr;
    }
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

    // TODO: this is happening quite often so don't report it
    // not sure if it indicates an actual issue
    // ReportIf(0 != mediabox.x0 || 0 != mediabox.y0);
    rotation = (rotation + 360) % 360;
    if (90 == rotation) {
        ctm = fz_pre_translate(ctm, 0, -mediabox.y1);
    } else if (180 == rotation) {
        ctm = fz_pre_translate(ctm, -mediabox.x1, -mediabox.y1);
    } else if (270 == rotation) {
        ctm = fz_pre_translate(ctm, -mediabox.x1, 0);
    }

    ReportIf(fz_matrix_expansion(ctm) <= 0);
    if (fz_matrix_expansion(ctm) == 0) {
        return fz_identity;
    }

    return ctm;
}

// TODO: maybe make dpi a float as well
static float DpiScale(float x, int dpi) {
    ReportIf(dpi < 70.F);
    // TODO: maybe implement step scaling like mupdf
    float res = x * (float)dpi;
    res = res / 96.F;
    return res;
}

static float FzRectOverlap(fz_rect r1, fz_rect r2) {
    if (fz_is_empty_rect(r1)) {
        return 0.0F;
    }
    fz_rect isect = fz_intersect_rect(r1, r2);
    return (isect.x1 - isect.x0) * (isect.y1 - isect.y0) / ((r1.x1 - r1.x0) * (r1.y1 - r1.y0));
}

static float FzRectOverlap(fz_rect r1, RectF r2f) {
    if (fz_is_empty_rect(r1)) {
        return 0.0F;
    }
    fz_rect r2 = ToFzRect(r2f);
    fz_rect isect = fz_intersect_rect(r1, r2);
    return (isect.x1 - isect.x0) * (isect.y1 - isect.y0) / ((r1.x1 - r1.x0) * (r1.y1 - r1.y0));
}

static TempWStr PdfToWStrTemp(fz_context* ctx, pdf_obj* obj) {
    char* s = pdf_new_utf8_from_pdf_string_obj(ctx, obj);
    TempWStr res = ToWStrTemp(s);
    fz_free(ctx, s);
    return res;
}

static TempStr PdfToUtf8Temp(fz_context* ctx, pdf_obj* obj) {
    char* s = pdf_new_utf8_from_pdf_string_obj(ctx, obj);
    TempStr res = str::DupTemp(s);
    fz_free(ctx, s);
    return res;
}

// some PDF documents contain control characters in outline titles or /Info properties
// we replace them with spaces and cleanup for display with NormalizeWSInPlace()
static void PdfCleanStringInPlace(WStr& ws) {
    if (!ws) {
        return;
    }
    for (int i = 0; i < ws.len; i++) {
        WCHAR c = ws.s[i];
        if (c < 0x20) {
            ws.s[i] = ' ';
        } else if (c == 0xfffd) {
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/4965
            // TODO: was there mupdf change that caused this?
            ws.s[i] = 0;
            ws.len = i;
            break;
        }
    }
    wstr::NormalizeWSInPlace(ws);
    ws.len = len(ws);
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

static fz_stream* FzStreamFromData(fz_context* ctx, const u8* data, int size) {
    fz_stream* stm = nullptr;
    // TODO: we copy so that the memory ends up in chunk allocated
    // by libmupdf so that it works across dll boundaries.
    // We can either use  fz_new_buffer_from_shared_data
    // and free the data on the side or create Allocator that
    // uses fz_malloc_no_throw and pass it to ReadFileWithArena
    void* dataCopy = FzMemdup(ctx, (void*)data, size);
    if (!dataCopy) {
        return nullptr;
    }

    fz_buffer* buf = fz_new_buffer_from_data(ctx, (u8*)dataCopy, size);
    fz_var(buf);
    fz_try(ctx) {
        stm = fz_open_buffer(ctx, buf);
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        stm = nullptr;
        fz_report_error(ctx);
    }
    return stm;
}

// maximum size of a file that's entirely loaded into memory before parsed
// and displayed; larger files will be kept open while they're displayed
// so that their content can be loaded on demand in order to preserve memory
constexpr i64 kMaxMemoryFileSize = 32 * 1024 * 1024;

static fz_stream* FzReadFileIfSmall(fz_context* ctx, Str path) {
    fz_stream* stm = nullptr;
    i64 fileSize = file::GetSize(path);
    // load small files entirely into memory so that they can be
    // overwritten even by programs that don't open files with FILE_SHARE_READ
    bool isSmallFile = fileSize > 0 && fileSize < kMaxMemoryFileSize;
    if (!isSmallFile) {
        return nullptr;
    }

    Str d = file::ReadFile(path);
    if (str::IsEmpty(d)) {
        // failed to read
        return nullptr;
    }

    stm = FzStreamFromData(ctx, (u8*)d.s, len(d));
    str::Free(d);
    return stm;
}

/*
https://github.com/sumatrapdfreader/sumatrapdf/issues/4514
Some PDF files have garbage at the beginning, before the %PDF- marker
Sometimes removing this garbage fixes the file for mupdf
*/
static fz_stream* FzReadMaybeFixPDF(fz_context* ctx, Str path) {
    fz_stream* stm;
    // fast fail: read enough to check if this is PDF file with garbage
    char buf[1024];
    size_t bufSize = dimof(buf);
    int n = file::ReadN(path, (u8*)buf, bufSize);
    if (n < 1024) {
        return nullptr;
    }
    n = str::IndexOf(Str(buf, n), StrL("%PDF-"));
    if (n <= 0) {
        // not PDF or no garbage at the beginning
        return nullptr;
    }

    Str d = file::ReadFile(path);
    if (str::IsEmpty(d)) {
        // failed to read
        return nullptr;
    }

    // strip garbage
    const u8* data = (u8*)d.s + n;
    int size = len(d) - n;
    stm = FzStreamFromData(ctx, data, size);
    str::Free(d);
    return stm;
}

static fz_stream* FzOpenOrReadFile(fz_context* ctx, Str path) {
    fz_stream* stm = FzReadFileIfSmall(ctx, path);
    if (stm) {
        return stm;
    }
    WCHAR* pathW = CWStrTemp(path);
    fz_try(ctx) {
        stm = fz_open_file_w(ctx, pathW);
    }
    fz_catch(ctx) {
        stm = nullptr;
        fz_report_error(ctx);
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
        fz_report_error(ctx);
        return;
    }
    ReportIf(nullptr == buf);
    u8* data;
    size_t size = fz_buffer_extract(ctx, buf, &data);
    ReportIf((size_t)fileLen != size);
    fz_drop_buffer(ctx, buf);

    fz_md5 md5;
    fz_md5_init(&md5);
    fz_md5_update(&md5, data, size);
    fz_md5_final(&md5, digest);
}

static Str FzExtractStreamData(fz_context* ctx, fz_stream* stream) {
    fz_seek(ctx, stream, 0, 2);
    i64 fileLen = fz_tell(ctx, stream);
    fz_seek(ctx, stream, 0, 0);

    fz_buffer* buf = fz_read_all(ctx, stream, fileLen);

    u8* data = nullptr;
    size_t size = fz_buffer_extract(ctx, buf, &data);
    ReportIf((size_t)fileLen != size);
    fz_drop_buffer(ctx, buf);
    if (!data || size == 0) {
        return {};
    }
    // this was allocated inside mupdf, make a copy that can be free()d
    Str res = str::Dup(Str((char*)data, (int)size));
    fz_free(ctx, data);
    return res;
}

struct SeenGlyph {
    int rune;
    Rect r;
};

static bool HasSeenGlyph(const Vec<SeenGlyph>& seen, int rune, const Rect& r) {
    // A "duplicate" glyph is one drawn on top of an earlier one (e.g. faux-bold
    // double-strike or an overprinted shadow); its box overlaps the earlier one
    // almost entirely. Two *adjacent* identical letters (e.g. the "ll" in
    // "Yellow") sit side by side and barely overlap, so they must NOT be treated
    // as duplicates. Comparing coordinates with a fixed +-1px tolerance can't
    // tell them apart once the glyph advance rounds to <=1px (small fonts),
    // which dropped a letter on copy (issue #5766). Require the boxes to overlap
    // by more than half the smaller glyph instead.
    i64 area = (i64)r.dx * (i64)r.dy;
    if (area <= 0) {
        return false;
    }
    for (const SeenGlyph& glyph : seen) {
        if (glyph.rune != rune) {
            continue;
        }
        Rect inter = glyph.r.Intersect(r);
        if (inter.IsEmpty()) {
            continue;
        }
        i64 interArea = (i64)inter.dx * (i64)inter.dy;
        i64 seenArea = (i64)glyph.r.dx * (i64)glyph.r.dy;
        i64 minArea = std::min(area, seenArea);
        if (minArea > 0 && interArea * 2 > minArea) {
            return true;
        }
    }
    return false;
}

static void AddSeenGlyph(Vec<SeenGlyph>& seen, int rune, const Rect& r) {
    seen.Append({rune, r});
}

static void AddCharUtf8(fz_stext_line*, fz_stext_char* c, str::Builder& s, Vec<Rect>& rects, Vec<SeenGlyph>& seen) {
    fz_rect bbox = fz_rect_from_quad(c->quad);
    Rect r = ToRectF(bbox).Round();
    int rune = c->c;
    if (HasSeenGlyph(seen, rune, r)) {
        return;
    }

    bool isWhitespace = rune > 0 && rune <= 0x7f && str::IsWs((char)rune);
    bool isNonPrintable = rune <= 32 || (rune <= 0xffff && wstr::IsNonCharacter((WCHAR)rune));
    if (isNonPrintable && !isWhitespace) {
        s.AppendChar('?');
        rects.Append(r);
        AddSeenGlyph(seen, rune, r);
        return;
    }
    if (isWhitespace) {
        // collapse multiple whitespace characters into one
        char prev = s.IsEmpty() ? 0 : s.LastChar();
        if (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r') {
            return;
        }
        s.AppendChar(' ');
        rects.Append(r);
        AddSeenGlyph(seen, rune, r);
        return;
    }
    char buf[4];
    int n = fz_runetochar(buf, rune);
    s.Append(Str(buf, n));
    rects.Append(r);
    AddSeenGlyph(seen, rune, r);
}

static void AddLineSepUtf8(str::Builder& s, Vec<Rect>& rects, Str lineSep) {
    size_t lineSepLen = (size_t)lineSep.len;
    if (lineSepLen == 0) {
        return;
    }
    // remove trailing space
    if (!s.IsEmpty() && s.LastChar() == ' ') {
        s.RemoveLast();
        rects.RemoveLast();
    }
    s.Append(lineSep);
    for (size_t i = 0; i < lineSepLen; i++) {
        rects.Append(Rect());
    }
}

static Str FzTextPageToUtf8(fz_stext_page* text, Rect** coordsOut) {
    Str lineSep = StrL("\n");
    str::Builder content;
    Vec<Rect> rects;
    Vec<SeenGlyph> seen;

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
                AddCharUtf8(line, c, content, rects, seen);
                c = c->next;
            }
            AddLineSepUtf8(content, rects, lineSep);
            line = line->next;
        }
        block = block->next;
    }

    ReportIf(Utf8CodepointCount(ToStr(content)) != len(rects));

    if (coordsOut) {
        if (len(rects) > 0) {
            *coordsOut = rects.Take();
        } else {
            *coordsOut = nullptr;
        }
    }
    return content.TakeStr();
}

static fz_stext_options NewTextPageOptions(int flags = 0) {
    fz_stext_options opts{};
    // Use glyph outline bounds so text selection rectangles match visible text
    // instead of the looser line-height boxes from default MuPDF extraction.
    opts.flags = flags | FZ_STEXT_ACCURATE_BBOXES;
    return opts;
}

struct Utf8PageText {
    Str text;
    int len = 0;
    int* codepoints = nullptr;
    int* byteOffsets = nullptr;
};

static Utf8PageText MakeUtf8PageTextTemp(Str text) {
    Utf8PageText res;
    res.text = text;
    int maxCodepoints = text ? text.len : 0;
    res.codepoints = AllocArrayTemp<int>(maxCodepoints + 1);
    res.byteOffsets = AllocArrayTemp<int>(maxCodepoints + 1);
    int byteIdx = 0;
    while (text && byteIdx < text.len) {
        int n = 0;
        res.byteOffsets[res.len] = byteIdx;
        res.codepoints[res.len] = Utf8CodepointAtByte(text, byteIdx, &n);
        byteIdx += n > 0 ? n : 1;
        res.len++;
    }
    res.byteOffsets[res.len] = text ? text.len : 0;
    return res;
}

static int RuneAt(Utf8PageText pageText, int off) {
    if (off < 0 || off >= pageText.len) {
        return 0;
    }
    return pageText.codepoints[off];
}

static Str SliceByRuneOff(Utf8PageText pageText, int startOff, int endOff) {
    startOff = limitValue(startOff, 0, pageText.len);
    endOff = limitValue(endOff, startOff, pageText.len);
    int startByte = pageText.byteOffsets[startOff];
    int endByte = pageText.byteOffsets[endOff];
    return Str(pageText.text.s + startByte, endByte - startByte);
}

static bool StartsWithAscii(Utf8PageText pageText, int off, const char* s) {
    for (; *s; s++, off++) {
        if (RuneAt(pageText, off) != (u8)*s) {
            return false;
        }
    }
    return true;
}

static bool ContainsAsciiChar(Str chars, int c) {
    if (c < 0 || c > 0x7f) {
        return false;
    }
    for (int i = 0; i < chars.len; i++) {
        if ((u8)chars.s[i] == c) {
            return true;
        }
    }
    return false;
}

static int IndexOfRune(Utf8PageText pageText, int startOff, int endOff, int c) {
    for (int i = startOff; i < endOff; i++) {
        if (RuneAt(pageText, i) == c) {
            return i;
        }
    }
    return -1;
}

static bool IsAlphaNumRune(int c) {
    return c >= 0 && c <= 0xffff && iswalnum((wint_t)c);
}

static bool IsWhitespaceRune(int c) {
    if (c >= 0 && c <= 0x7f) {
        return str::IsWs((char)c);
    }
    return c <= 0xffff && iswspace((wint_t)c);
}

static bool LinkifyCheckMultiline(Utf8PageText pageText, int posOff, Rect* coords) {
    int pageLen = pageText.len;
    // multiline links end in a non-alphanumeric character and continue on a line
    // that starts left and only slightly below where the current line ended
    // (and that doesn't start with http or a footnote numeral)
    return posOff > 0 && posOff < pageLen && '\n' == RuneAt(pageText, posOff) && (posOff + 1) < pageLen &&
           !IsAlphaNumRune(RuneAt(pageText, posOff - 1)) && !IsWhitespaceRune(RuneAt(pageText, posOff + 1)) &&
           coords[posOff + 1].BR().y > coords[posOff - 1].y &&
           coords[posOff + 1].y <= coords[posOff - 1].BR().y + coords[posOff - 1].dy * 0.35 &&
           coords[posOff + 1].x < coords[posOff - 1].BR().x && coords[posOff + 1].dy >= coords[posOff - 1].dy * 0.85 &&
           coords[posOff + 1].dy <= coords[posOff - 1].dy * 1.2 && !StartsWithAscii(pageText, posOff + 1, "http");
}

static bool EndsURL(int c) {
    if (c == 0 || IsWhitespaceRune(c)) {
        return true;
    }
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1313
    // 0xff0c is ","
    if (c == 0xff0c) {
        return true;
    }
    return false;
}

// Trim trailing punctuation that likely belongs to surrounding sentence text, not
// the link. `trimChars` lists chars to strip; when trimRepeat is false, at most
// one char is removed. When trimCloseParen is true, a trailing ')' is also
// stripped unless the span contains an opening '(' before it.
static int LinkifyTrimTrailingPunctOff(int startOff, int endOff, Str trimChars, bool trimRepeat, bool trimCloseParen,
                                       Utf8PageText pageText) {
    for (;;) {
        if (endOff <= startOff) {
            break;
        }
        int c = RuneAt(pageText, endOff - 1);
        if (ContainsAsciiChar(trimChars, c)) {
            endOff--;
            if (!trimRepeat) {
                break;
            }
            continue;
        }
        if (trimCloseParen && ')' == c) {
            if (IndexOfRune(pageText, startOff, endOff, '(') < 0) {
                endOff--;
                if (!trimRepeat) {
                    break;
                }
                continue;
            }
        }
        break;
    }
    return endOff;
}

static int LinkifyFindEndOff(int startOff, int prevChar, Utf8PageText pageText) {
    int pageEnd = pageText.len;
    int endOff = startOff;
    while (endOff < pageEnd && !EndsURL(RuneAt(pageText, endOff))) {
        endOff++;
    }
    endOff = LinkifyTrimTrailingPunctOff(startOff, endOff, StrL(",.?!"), false, true, pageText);

    // cut the link at the first quotation mark, if it's also preceded by one
    if ('"' == prevChar || '\'' == prevChar) {
        int quoteOff = IndexOfRune(pageText, startOff, endOff, prevChar);
        if (quoteOff >= 0) {
            endOff = quoteOff;
        }
    }

    return endOff;
}

static int LinkifyMultilineText(LinkRectList* list, Utf8PageText pageText, int startOff, int nextOff, Rect* coords) {
    int lastIx = len(list->coords) - 1;
    TempStr uri = list->links.At(lastIx);
    int endOff = nextOff;
    bool multiline = false;

    do {
        int prevChar = startOff > 0 ? RuneAt(pageText, startOff - 1) : ' ';
        endOff = LinkifyFindEndOff(nextOff, prevChar, pageText);
        multiline = LinkifyCheckMultiline(pageText, endOff, coords);

        Str part = SliceByRuneOff(pageText, nextOff, endOff);
        uri = str::JoinTemp(uri, part);
        Rect bbox = coords[nextOff].Union(coords[endOff - 1]);
        list->coords.Append(ToFzRect(ToRectF(bbox)));

        nextOff = endOff + 1;
    } while (multiline);

    // update the link URL for all partial links
    list->links.SetAt(lastIx, uri);
    for (int i = lastIx + 1; i < len(list->coords); i++) {
        list->links.Append(uri);
    }

    return endOff;
}

// cf. http://weblogs.mozillazine.org/gerv/archives/2011/05/html5_email_address_regexp.html
inline bool IsEmailUsernameChar(int c) {
    // explicitly excluding the '/' from the list, as it is more
    // often part of a URL or path than of an email address
    return IsAlphaNumRune(c) || ContainsAsciiChar(StrL(".!#$%&'*+=?^_`{|}~-"), c);
}
inline bool IsEmailDomainChar(int c) {
    return IsAlphaNumRune(c) || '-' == c;
}

static int LinkifyFindEmailOff(Utf8PageText pageText, int atOff) {
    int startOff = atOff;
    while (startOff > 0 && IsEmailUsernameChar(RuneAt(pageText, startOff - 1))) {
        startOff--;
    }
    return startOff != atOff ? startOff : -1;
}

static int LinkifyEmailAddressOff(int startOff, Utf8PageText pageText) {
    int pageEnd = pageText.len;
    int endOff = startOff;
    while (endOff < pageEnd && IsEmailUsernameChar(RuneAt(pageText, endOff))) {
        endOff++;
    }
    if (endOff == startOff || endOff >= pageEnd || RuneAt(pageText, endOff) != '@' || (endOff + 1) >= pageEnd ||
        !IsEmailDomainChar(RuneAt(pageText, endOff + 1))) {
        return -1;
    }
    for (endOff++; endOff < pageEnd && IsEmailDomainChar(RuneAt(pageText, endOff)); endOff++) {
        ;
    }
    if (endOff >= pageEnd || '.' != RuneAt(pageText, endOff) || (endOff + 1) >= pageEnd ||
        !IsEmailDomainChar(RuneAt(pageText, endOff + 1))) {
        return -1;
    }
    do {
        for (endOff++; endOff < pageEnd && IsEmailDomainChar(RuneAt(pageText, endOff)); endOff++) {
            ;
        }
    } while (endOff < pageEnd && '.' == RuneAt(pageText, endOff) && (endOff + 1) < pageEnd &&
             IsEmailDomainChar(RuneAt(pageText, endOff + 1)));
    return endOff;
}

// Detect a printed DOI ("10." + 4-9 digit registrant + "/" + non-empty suffix),
// e.g. "10.1109/WICSA.2015.29". `start` must point at the leading '1'. Returns
// the end ptr (exclusive) past the suffix, or nullptr if `start` is not a DOI.
// The suffix runs to the first EndsURL() terminator (whitespace, fullwidth
// comma) or quote/angle bracket; trailing sentence punctuation is trimmed.
static int LinkifyFindDoiEndOff(int startOff, Utf8PageText pageText) {
    if (!StartsWithAscii(pageText, startOff, "10.")) {
        return -1;
    }
    int p = startOff + 3;
    int regStart = p;
    int pageEnd = pageText.len;
    while (p < pageEnd && isdigit(RuneAt(pageText, p))) {
        p++;
    }
    int regLen = p - regStart;
    if (regLen < 4 || regLen > 9 || p >= pageEnd || RuneAt(pageText, p) != '/') {
        return -1;
    }
    p++; // skip '/'
    int suffixStart = p;
    while (p < pageEnd && !EndsURL(RuneAt(pageText, p)) && RuneAt(pageText, p) != '"' && RuneAt(pageText, p) != '<' &&
           RuneAt(pageText, p) != '>') {
        p++;
    }
    if (p == suffixStart) {
        return -1;
    }
    p = LinkifyTrimTrailingPunctOff(suffixStart, p, StrL(".,;:!)]}'"), true, false, pageText);
    if (p == suffixStart) {
        return -1;
    }
    return p;
}

// caller needs to delete the result
// TODO: return Vec<IPageElement*> directly
static LinkRectList* LinkifyText(Utf8PageText pageText, Rect* coords) {
    LinkRectList* list = new LinkRectList;
    int pageEnd = pageText.len;

    for (int startOff = 0; startOff < pageEnd;) {
        int endOff = -1;
        bool multiline = false;
        Str protocol;

        int startChar = RuneAt(pageText, startOff);
        if ('@' == startChar) {
            // potential email address without mailto:
            int emailOff = LinkifyFindEmailOff(pageText, startOff);
            endOff = emailOff >= 0 ? LinkifyEmailAddressOff(emailOff, pageText) : -1;
            protocol = StrL("mailto:");
            if (endOff >= 0) {
                startOff = emailOff;
            }
        } else if (startOff > 0 &&
                   (RuneAt(pageText, startOff - 1) == '/' || IsAlphaNumRune(RuneAt(pageText, startOff - 1)))) {
            // hyperlinks must not be preceded by a slash (indicates a different protocol)
            // or an alphanumeric character (indicates part of a different protocol)
        } else if ('h' == startChar && (StartsWithAscii(pageText, startOff, "http://") ||
                                        StartsWithAscii(pageText, startOff, "https://"))) {
            int prevChar = startOff > 0 ? RuneAt(pageText, startOff - 1) : ' ';
            endOff = LinkifyFindEndOff(startOff, prevChar, pageText);
            multiline = LinkifyCheckMultiline(pageText, endOff, coords);
        } else if ('w' == startChar && StartsWithAscii(pageText, startOff, "www.")) {
            int prevChar = startOff > 0 ? RuneAt(pageText, startOff - 1) : ' ';
            endOff = LinkifyFindEndOff(startOff, prevChar, pageText);
            multiline = LinkifyCheckMultiline(pageText, endOff, coords);
            protocol = StrL("http://");
            // ignore www. links without a top-level domain
            int dotOff = IndexOfRune(pageText, startOff + 5, endOff, '.');
            if (endOff - startOff <= 4 || (!multiline && dotOff < 0)) {
                endOff = -1;
            }
        } else if ('m' == startChar && StartsWithAscii(pageText, startOff, "mailto:")) {
            endOff = LinkifyEmailAddressOff(startOff + 7, pageText);
        } else if ('1' == startChar) {
            endOff = LinkifyFindDoiEndOff(startOff, pageText);
            if (endOff >= 0) {
                // a plain-text DOI ("10.1109/...") -> https://doi.org/<doi>
                protocol = StrL("https://doi.org/");
            }
        }
        if (endOff < 0) {
            startOff++;
            continue;
        }

        Str part = SliceByRuneOff(pageText, startOff, endOff);
        Str uri = part;
        if (protocol) {
            uri = str::JoinTemp(protocol, part);
        }
        list->links.Append(uri);
        Rect bbox = coords[startOff].Union(coords[endOff - 1]);
        list->coords.Append(ToFzRect(ToRectF(bbox)));
        if (multiline) {
            endOff = LinkifyMultilineText(list, pageText, startOff, endOff + 1, coords);
        }

        startOff = endOff;
    }

    return list;
}

// try to produce an 8-bit palette for saving some memory
static RenderedBitmap* TryRenderAsPaletteImage(fz_pixmap* pixmap) {
    int w = pixmap->w;
    int h = pixmap->h;
    int stride = ((w + 3) / 4) * 4;

    size_t sz = sizeof(BITMAPINFO) + (255 * sizeof(RGBQUAD));
    ScopedMem<BITMAPINFO> bmi((BITMAPINFO*)calloc(1, sz));
    if (!bmi.Get()) {
        return nullptr;
    }
    BITMAPINFOHEADER* bmih = &bmi.Get()->bmiHeader;
    bmih->biSize = sizeof(*bmih);
    bmih->biWidth = w;
    bmih->biHeight = -h;
    bmih->biPlanes = 1;
    bmih->biCompression = BI_RGB;
    bmih->biBitCount = 8;
    bmih->biSizeImage = h * stride;
    bmih->biClrUsed = 256;

    void* data = nullptr;
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bmih->biSizeImage, nullptr);
    HBITMAP hbmp = CreateDIBSection(nullptr, bmi, DIB_RGB_COLORS, &data, hMap, 0);
    if (!hbmp) {
        if (hMap) {
            CloseHandle(hMap);
        }
        return nullptr;
    }

    u32* palette = (u32*)bmi.Get()->bmiColors;

    // open-addressed hash table for color -> palette index lookup.
    // key is RGB in source byte order (R | G<<8 | B<<16); empty slot = -1.
    // kHashSize = 1,024 slots (4x the 256 max palette entries -> load factor <= 25%).
    // hashIdx is 1,024 * 2 = 2,048 bytes; hashKey is 1,024 * 4 = 4,096 bytes; 6 KB total on the stack.
    constexpr int kHashBits = 10;
    constexpr int kHashSize = 1 << kHashBits;
    constexpr u32 kHashMask = kHashSize - 1;
    i16 hashIdx[kHashSize];
    u32 hashKey[kHashSize];
    memset(hashIdx, 0xFF, sizeof(hashIdx));

    u8* dest = (u8*)data;
    u8* source = pixmap->samples;
    int paletteSize = 0;
    int padding = stride - w;
    // sentinel that can't equal any masked pixel (alpha bits would be 0)
    u32 lastPx = 0xFFFFFFFFu;
    int lastIdx = 0;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            u32 px = *(u32*)source & 0x00FFFFFFu;
            source += 4;

            if (px == lastPx) {
                *dest++ = (u8)lastIdx;
                continue;
            }

            u32 slot = (px * 2654435761u) >> (32 - kHashBits);
            int k;
            for (;;) {
                int idx = hashIdx[slot];
                if (idx < 0) {
                    if (paletteSize >= 256) {
                        DeleteObject(hbmp);
                        if (hMap) {
                            CloseHandle(hMap);
                        }
                        return {};
                    }
                    k = paletteSize++;
                    hashKey[slot] = px;
                    hashIdx[slot] = (i16)k;
                    // palette is BGR0 (RGBQUAD layout); source is RGBA, so swap R and B
                    palette[k] = ((px & 0xFFu) << 16) | (px & 0xFF00u) | ((px >> 16) & 0xFFu);
                    break;
                }
                if (hashKey[slot] == px) {
                    k = idx;
                    break;
                }
                slot = (slot + 1) & kHashMask;
            }
            lastPx = px;
            lastIdx = k;
            *dest++ = (u8)k;
        }
        dest += padding;
    }

    bmih->biClrUsed = paletteSize;
    // CreateDIBSection snapshotted the (empty) color table at call time, so push the
    // palette we just built into the DIB now via SetDIBColorTable.
    HDC hdc = CreateCompatibleDC(nullptr);
    if (hdc) {
        HGDIOBJ oldBmp = SelectObject(hdc, hbmp);
        SetDIBColorTable(hdc, 0, paletteSize, (RGBQUAD*)palette);
        SelectObject(hdc, oldBmp);
        DeleteDC(hdc);
    }
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

static RenderedBitmap* NewRenderedFzPixmap(fz_context* ctx, fz_pixmap* pixmap) {
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
        fz_report_error(ctx);
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
        if (hMap) {
            CloseHandle(hMap);
        }
        return nullptr;
    }
    // return a RenderedBitmap even if hbmp is nullptr so that callers can
    // distinguish rendering errors from GDI resource exhaustion
    // (and in the latter case retry using smaller target rectangles)
    return new RenderedBitmap(hbmp, Size(w, h), hMap);
}

static TocItem* NewTocItemWithDestination(TocItem* parent, Str title, IPageDestination* dest) {
    auto res = new TocItem(parent, title, 0);
    res->dest = dest;
    return res;
}

// TODO: could be optimized
static bool RectFullyContains(RectF r1, RectF r2) {
    // if same size, we don't consider it that one covers another
    if (r1 == r2) {
        return false;
    }
    return r1.Contains(r2.TL()) && r1.Contains(r2.BR());
}

// if an elements fully obscures another, remove it from the list
static bool RemoveHeWhoFullyContains(Vec<IPageElement*>& els) {
    int n = len(els);
    ReportIf(n < 2);
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
    int n = len(els);
    if (n == 0) {
        return nullptr;
    }
    if (n == 1) {
        return els[0];
    }

    // for https://github.com/sumatrapdfreader/sumatrapdf/issues/5200
    // priority for destinations (e.g. links) over images
    for (IPageElement* el : els) {
        if (el->GetKind() == kindPageElementDest) {
            return el;
        }
    }
Encore:
    bool didRemove = RemoveHeWhoFullyContains(els);
    if (didRemove) {
        ReportIf(len(els) != n - 1);
        n = len(els);
        if (n == 1) {
            return els[0];
        }
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

    fz_point p = {(float)pt.x, (float)pt.y};
    for (auto& img : pageInfo->images) {
        fz_rect ir = img->rect;
        if (IsPointInRect(ir, p)) {
            res.Append(img->imageElement);
        }
    }

    if (false) {
        int i = 0;
        for (auto&& el : res) {
            Rect r = el->GetRect().Round();
            logfa("el %d: pos: %d-%d, size: %d-%d, kind: %s\n", (int)i, r.x, r.y, r.dx, r.dy, Str(el->GetKind()));
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

    int total = len(pageInfo->images) + len(pageInfo->links) + len(pageInfo->autoLinks) + len(pageInfo->comments);
    els.Clear();
    els.EnsureCap(total);

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    for (auto& img : pageInfo->images) {
        els.Append(img->imageElement);
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
    Str pageTextUtf8 = FzTextPageToUtf8(stext, &coords);
    if (!pageTextUtf8) {
        // even for empty text FzTextPageToUtf8 allocates coords via Vec::Take
        free(coords);
        str::Free(pageTextUtf8);
        return;
    }

    Utf8PageText pageText = MakeUtf8PageTextTemp(pageTextUtf8);
    LinkRectList* list = LinkifyText(pageText, coords);
    str::Free(pageTextUtf8);

    for (int i = 0; i < len(list->links); i++) {
        fz_rect bbox = list->coords.at(i);
        bool overlaps = false;
        for (auto pel : pageInfo->links) {
            overlaps = FzRectOverlap(bbox, pel->GetRect()) >= 0.25f;
        }
        if (overlaps) {
            continue;
        }

        TempStr uri = list->links.At(i);
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
            pel->imageID = len(images);
            img->imageElement = pel;
            images.Append(img);
        }
        block = block->next;
    }
}

static fz_image* FzFindImageAtIdx(fz_context* ctx, FzPageInfo* pageInfo, int idx) {
    fz_stext_options opts = NewTextPageOptions(FZ_STEXT_PRESERVE_IMAGES);
    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page(ctx, pageInfo->page, &opts);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
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
    // (https://code.google.com/archive/p/sumatrapdf/issues/1303)
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
        ReportIf(link->rect.x1 < link->rect.x0);
        ReportIf(link->rect.y1 < link->rect.y0);
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

// Note: make sure to only call with docLock
// PdfLoadAttachment && PdfLoadAttachments must traverse in the same order
static Str PdfLoadAttachment(fz_context* ctx, pdf_document* doc, int no) {
    pdf_obj* dict;
    fz_var(dict);
    Str res;

    fz_try(ctx) {
        dict = pdf_load_name_tree(ctx, doc, PDF_NAME(EmbeddedFiles));
        if (!dict) {
            break;
        }

        int n = pdf_dict_len(ctx, dict);
        for (int i = 0; i < n; i++) {
            pdf_obj* fs = pdf_dict_get_val(ctx, dict, i);

            // https://github.com/sumatrapdfreader/sumatrapdf/issues/1666
            if (false && !pdf_is_embedded_file(ctx, fs)) {
                continue;
            }
            if (no == i + 1) {
                fz_buffer* buf = pdf_load_embedded_file_contents(ctx, fs);
                res = str::Dup(Str((char*)buf->data, (int)buf->len));
                fz_drop_buffer(ctx, buf);
                i = n + 1; // exit for loop
            }
        }
    }
    fz_always(ctx) {
        pdf_drop_obj(ctx, dict);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logfa("PdfLoadAttachment() failed\n");
    }
    return res;
}

// load embedded file data from a file attachment annotation by PDF object number
static Str PdfLoadAnnotationAttachment(fz_context* ctx, pdf_document* doc, int objNum) {
    Str res;
    fz_try(ctx) {
        pdf_obj* obj = pdf_new_indirect(ctx, doc, objNum, 0);
        pdf_obj* fs = pdf_dict_get(ctx, obj, PDF_NAME(FS));
        if (!fs) {
            pdf_drop_obj(ctx, obj);
            break;
        }
        fz_buffer* buf = pdf_load_embedded_file_contents(ctx, fs);
        if (buf) {
            res = str::Dup(Str((char*)buf->data, (int)buf->len));
            fz_drop_buffer(ctx, buf);
        }
        pdf_drop_obj(ctx, obj);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logfa("PdfLoadAnnotationAttachment(objNum=%d) failed\n", objNum);
    }
    return res;
}

// Note: make sure to only call with docLock
static fz_outline* PdfLoadAttachments(fz_context* ctx, pdf_document* doc, Str path) {
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

            // https://github.com/sumatrapdfreader/sumatrapdf/issues/1666
            if (false && !pdf_is_embedded_file(ctx, fs)) {
                continue;
            }
            pdf_filespec_params fileParams = {};
            pdf_get_filespec_params(ctx, fs, &fileParams);
            const char* nameStr = fileParams.filename;
            if (str::IsEmpty(nameStr) || (fileParams.size < 0)) {
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
        fz_report_error(ctx);
        logfa("PdfLoadAttachments() failed for '%s'\n", path);
    }
    return root.next;
}

struct PageLabelInfo {
    int startAt = 0;
    int countFrom = 0;
    Str type;
    pdf_obj* prefix = nullptr;
};

int CmpPageLabelInfo(const void* a, const void* b) {
    return ((PageLabelInfo*)a)->startAt - ((PageLabelInfo*)b)->startAt;
}

static TempStr FormatPageLabelTemp(Str type, int pageNo, Str prefix) {
    if (str::Eq(type, "D")) {
        return fmt("%s%d", prefix, pageNo);
    }
    if (str::EqI(type, "R")) {
        // roman numbering style
        TempStr number = str::FormatRomanNumeralTemp(pageNo);
        if (!str::IsEmpty(type) && type.s[0] == 'r') {
            str::ToLowerInPlace(number);
        }
        return fmt("%s%s", prefix, number);
    }
    if (str::EqI(type, "A")) {
        // alphabetic numbering style (A..Z, AA..ZZ, AAA..ZZZ, ...)
        str::Builder number;
        number.AppendChar('A' + (pageNo - 1) % 26);
        for (int i = 0; i < (pageNo - 1) / 26; i++) {
            number.AppendChar(number[0]);
        }
        if (!str::IsEmpty(type) && type.s[0] == 'a') {
            str::ToLowerInPlace(ToStr(number));
        }
        return fmt("%s%s", prefix, ToStr(number));
    }
    return str::DupTemp(prefix);
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

static StrVec* BuildPageLabelVec(fz_context* ctx, pdf_obj* root, int pageCount) {
    Vec<PageLabelInfo> data;
    BuildPageLabelRec(ctx, root, pageCount, data);
    data.Sort(CmpPageLabelInfo);

    int n = len(data);
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

    for (int i = 0; i < n; i++) {
        pli = data.at(i);
        if (pli.startAt > pageCount) {
            break;
        }
        int secLen = pageCount + 1 - pli.startAt;
        if (i < n - 1 && data.at(i + 1).startAt <= pageCount) {
            secLen = data.at(i + 1).startAt - pli.startAt;
        }
        TempStr prefix = PdfToUtf8Temp(ctx, data.at(i).prefix);
        for (int j = 0; j < secLen; j++) {
            int idx = pli.startAt + j - 1;
            TempStr label = FormatPageLabelTemp(pli.type, pli.countFrom + j, prefix);
            labels->SetAt(idx, label);
        }
    }

    for (int idx = 0; (idx = labels->Find(nullptr, idx)) != -1; idx++) {
        labels->SetAt(idx, "");
    }

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
    EnterCriticalSection(&e->fz_locks[lock]);
}

static void fz_unlock_context_cs(void* user, int lock) {
    EngineMupdf* e = (EngineMupdf*)user;
    LeaveCriticalSection(&e->fz_locks[lock]);
}

static void fz_print_cb(void* user, const char* msg) {
    Str msgStr = Str(msg);
    static AtomicBool seenMsg = 0;
    if (str::Contains(msgStr, StrL("generic error: couldn't find system font"))) {
        // this floods the log in some files
        // it shows a font name like this:
        // generic error: couldn't find system font 'AngsanaUPC-Bold'
        // generic error: couldn't find system font 'AngsanaUPC'
        // we only show the first missed font. Could use StrVec() to log every
        // missing font
        if (AtomicBoolGet(&seenMsg)) {
            return;
        }
        AtomicBoolSet(&seenMsg, true);
    }
    if (!str::EndsWith(msgStr, "\n")) {
        msgStr = str::JoinTemp(msgStr, StrL("\n"));
    }
    log(msgStr);
    EngineMupdf* engine = (EngineMupdf*)user;
    if (engine && !str::Contains(msgStr, StrL("unknown epub version"))) {
        // epub 3.0 is rendered fine, so don't treat the version warning as an error
        engine->errors.Append(msgStr);
    }
}

static void InstallFitzErrorCallbacks(EngineMupdf* engine, fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, (void*)engine);
    fz_set_error_callback(ctx, fz_print_cb, (void*)engine);
}

struct ContextThreadID {
    EngineMupdf* engine = nullptr;
    fz_context* ctx = nullptr;
    DWORD threadID = 0;
};

static Vec<ContextThreadID>* gPerThreadContexts;
static CRITICAL_SECTION gPerThreadContextsCs;
static AtomicInt gEngineCount = 0;

static void InitializeEngineMupdf() {
    auto n = AtomicIntInc(&gEngineCount);
    if (n != 1) return;
    ReportIf(gPerThreadContexts);
    InitializeCriticalSection(&gPerThreadContextsCs);
    gPerThreadContexts = new Vec<ContextThreadID>();
}

static void DeInitializeEngineMupdf() {
    auto n = AtomicIntDec(&gEngineCount);
    if (n > 0) return;
    ReportIf(n < 0);
    DeleteCriticalSection(&gPerThreadContextsCs);
    delete gPerThreadContexts;
    gPerThreadContexts = nullptr;
}

fz_context* GetOrClonePerThreadContext(EngineMupdf* engine, fz_context* ctx) {
    DWORD threadID = GetCurrentThreadId();
    {
        ScopedCritSec cs(&gPerThreadContextsCs);
        for (auto& el : *gPerThreadContexts) {
            if (el.engine == engine && el.threadID == threadID) {
                return el.ctx;
            }
        }
    }
    // clone context without holding gPerThreadContextsCs to avoid deadlock
    // with threads that hold fz_locks (e.g. docLock) and then call Ctx()
    // safe because only current thread can create a context for its own threadID
    auto newCtx = fz_clone_context(ctx);
    if (!newCtx) {
        // OOM or unexpected clone failure: fall back to the engine's main context
        // rather than caching/returning nullptr (which would crash mupdf callers).
        return ctx;
    }
    {
        ScopedCritSec cs(&gPerThreadContextsCs);
        ContextThreadID el{engine, newCtx, threadID};
        gPerThreadContexts->Append(el);
    }
    return newCtx;
}

void ReleasePerThreadContext(EngineMupdf* engine) {
    DWORD threadID = GetCurrentThreadId();
    fz_context* ctxToDrop = nullptr;
    {
        ScopedCritSec cs(&gPerThreadContextsCs);
        auto n = len(*gPerThreadContexts);
        for (int i = 0; i < n; i++) {
            auto& el = gPerThreadContexts->at(i);
            if (el.engine == engine && el.threadID == threadID) {
                ctxToDrop = el.ctx;
                gPerThreadContexts->RemoveAtFast(i);
                break;
            }
        }
    }
    if (ctxToDrop) {
        fz_drop_context(ctxToDrop);
    }
}

// Release all per-thread contexts for a given engine (called from destructor)
static void ReleaseAllPerThreadContexts(EngineMupdf* engine) {
    Vec<fz_context*> ctxsToDrop;
    {
        ScopedCritSec cs(&gPerThreadContextsCs);
        for (int i = len(*gPerThreadContexts) - 1; i >= 0; i--) {
            auto& el = gPerThreadContexts->at(i);
            if (el.engine == engine) {
                ctxsToDrop.Append(el.ctx);
                gPerThreadContexts->RemoveAtFast(i);
            }
        }
    }
    for (auto ctx : ctxsToDrop) {
        fz_drop_context(ctx);
    }
}

EngineMupdf::EngineMupdf() {
    InitializeEngineMupdf();
    kind = kindEngineMupdf;
    defaultExt = str::Dup(StrL(".pdf"));
    fileDPI = 72.0f;

    // pages Vec + its FzPageInfo elements live for the lifetime of the
    // engine, so bump-allocate them out of EngineBase::arena
    pages.allocator = arena;

    for (size_t i = 0; i < dimof(fz_locks); i++) {
        InitializeCriticalSection(&fz_locks[i]);
    }
    InitializeCriticalSection(&pagesLock);
    InitializeCriticalSection(&renderLock);
    InitializeCriticalSection(&docLock);

    fz_locks_ctx.user = this;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    _ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_DEFAULT);
    if (!_ctx) {
        // can happen when out of memory. Load() will fail
        log("EngineMupdf: fz_new_context() failed\n");
        return;
    }
    InstallFitzErrorCallbacks(this, _ctx);

    install_load_windows_font_funcs(_ctx);
    fz_register_document_handlers(_ctx);
}

fz_context* EngineMupdf::Ctx() const {
    if (!_ctx) {
        // fz_new_context() failed in the constructor, likely OOM
        return nullptr;
    }
    return GetOrClonePerThreadContext(const_cast<EngineMupdf*>(this), _ctx);
}

EngineMupdf::~EngineMupdf() {
    EnterCriticalSection(&pagesLock);

    auto ctx = _ctx;
    for (FzPageInfo* pi : pages) {
        DeleteVecMembers(pi->links);
        DeleteVecMembers(pi->autoLinks);
        DeleteVecMembers(pi->comments);
        DeleteVecMembers(pi->images);
        DeleteVecMembers(pi->annotations);
        DeleteVecMembers(pi->widgets);
        if (pi->retainedLinks) {
            fz_drop_link(ctx, pi->retainedLinks);
        }
        if (pi->displayList) {
            fz_drop_display_list(ctx, pi->displayList);
        }
        if (pi->page) {
            fz_drop_page(ctx, pi->page);
        }
        // storage is arena-owned; run the destructor in place so the inner
        // Vec<>s free their heap-allocated els buffers, then leave the
        // memory to the arena.
        pi->~FzPageInfo();
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
    // Drop per-thread clones only after the document (and any JS tied to _ctx) is gone.
    ReleaseAllPerThreadContexts(this);
    if (ctx) {
        fz_purge_glyph_cache(ctx);
    }
    fz_drop_context(ctx);

    str::Free(pdfPassword);
    delete pageLabels;
    delete tocTree;

    for (size_t i = 0; i < dimof(fz_locks); i++) {
        DeleteCriticalSection(&fz_locks[i]);
    }
    LeaveCriticalSection(&pagesLock);
    DeleteCriticalSection(&pagesLock);
    DeleteCriticalSection(&renderLock);
    DeleteCriticalSection(&docLock);

    DeInitializeEngineMupdf();
}

class PasswordCloner : public PasswordUI {
    u8* cryptKey = nullptr;

  public:
    explicit PasswordCloner(u8* cryptKey) { this->cryptKey = cryptKey; }

    Str GetPassword(Str, u8*, u8 decryptionKeyOut[32], bool* saveKey) override {
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return {};
    }
};

EngineBase* EngineMupdf::Clone() {
    ScopedCritSec scope(&docLock);
    if (!FilePath()) {
        // before port we could clone streams but it's no longer possible
        logf("EngineMupdf::Clone() failed: no file path\n");
        return nullptr;
    }
    auto ctx = Ctx();
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
        logf("EngineMupdf::Clone() failed: Load('%s') failed\n", FilePath());
        delete clone;
        delete pwdUI;
        return nullptr;
    }
    delete pwdUI;

    clone->disableAntiAlias = disableAntiAlias;
    clone->disableAutoLinks = disableAutoLinks;

    if (!decryptionKey.s && pdfdoc && pdfdoc->crypt) {
        clone->decryptionKey = Str();
    }

    return clone;
}

// File names ending in :<digits> are interpreted as containing
// embedded PDF documents (the digits is stream number of the embedded file stream)
TempStr ParseEmbeddedStreamNumber(Str path, int* streamNoOut) {
    int streamNo = -1;
    Str path2 = str::DupTemp(path);
    Str streamNoStr = ParseEmbeddedPdfName(path2).streamNoStr;
    if (streamNoStr) {
        Str rest = str::Parse(streamNoStr, ":%d", &streamNo);
        bool hasAttachmentName = rest && str::StartsWith(rest, ":attachname=");
        // there shouldn't be any left unparsed data except attachment name metadata
        ReportIf(!rest.s || (rest.s[0] && !hasAttachmentName));
        if (!rest.s || (rest.s[0] && !hasAttachmentName)) {
            streamNo = -1;
        }
        if (hasAttachmentName) {
            path2 = Str(path2.s, (int)(rest.s - path2.s));
        }
        // truncate at ':' to create a filesystem path
        path2 = Str(path2.s, (int)(streamNoStr.s - path2.s));
    }
    *streamNoOut = streamNo;
    return path2;
}

Str EngineMupdf::LoadStreamFromPDFFile(Str filePath) {
    auto ctx = Ctx();
    int streamNo = -1;
    TempStr fnCopy = ParseEmbeddedStreamNumber(filePath, &streamNo);
    if (streamNo < 0) {
        return {};
    }

    bool ok = Load(fnCopy, nullptr);
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
        fz_report_error(ctx);
        return {};
    }
    auto dataSize = buffer->len;
    if (dataSize == 0) {
        return {};
    }
    Str res = str::Dup(Str((char*)buffer->data, (int)dataSize));
    fz_drop_buffer(ctx, buffer);

    return res;
}

// <filePath> should end with embed marks, which is a stream number
// inside pdf file
Str LoadEmbeddedPDFFile(Str filePath) {
    EngineMupdf* engine = new EngineMupdf();
    auto res = engine->LoadStreamFromPDFFile(filePath);
    SafeEngineRelease(&engine);
    return res;
}

static Str TxtFileToHTML(Str path) {
    Str fd = file::ReadFileWithArena(path, GetTempArena());
    if (str::IsEmpty(fd)) {
        return {};
    }

    InterlockedIncrement(&gAllowAllocFailure);
    defer {
        InterlockedDecrement(&gAllowAllocFailure);
    };

    TempStr data = fd;
    data = str::ReplaceTemp(data, StrL("&"), StrL("&amp;"));
    if (!data) {
        return {};
    }
    data = str::ReplaceTemp(data, StrL(">"), StrL("&gt;"));
    if (!data) {
        return {};
    }
    data = str::ReplaceTemp(data, StrL("<"), StrL("&lt;"));
    if (!data) {
        return {};
    }

    str::Builder d;
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
    return d.TakeStr();
}

static Str PalmDocToHTML(Str path) {
    auto doc = PalmDoc::CreateFromFile(path);
    if (!doc) {
        return {};
    }
    // GetHtmlData() is a view into doc, dup before deleting it
    Str html = str::Dup(doc->GetHtmlData());
    delete doc;
    return html;
}

bool EngineMupdf::Load(Str path, PasswordUI* pwdUI) {
    bool ok;
    auto ctx = Ctx();
    ReportIf(FilePath() || _doc);
    if (!ctx) {
        return false;
    }
    SetFilePath(path);

    auto ext = path::GetExtTemp(path);
    SetDefaultExt(defaultExt, ext);

    int streamNo = -1;
    TempStr fnCopy = ParseEmbeddedStreamNumber(path, &streamNo);

    Kind kind = GuessFileTypeFromName(path);
    // show .txt, .xml and other text files as plain text
    // using html engine
    if (kind == kindFileTxt) {
        // synthesize a .html file from text file
        Str d = TxtFileToHTML(path);
        if (str::IsEmpty(d)) {
            return false;
        }
        fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (const u8*)d.s, (size_t)d.len);
        fz_stream* file = fz_open_buffer(ctx, buf);
        fz_drop_buffer(ctx, buf);
        str::Free(d);
        TempStr nameHint = str::JoinTemp(path, StrL(".html"));
        if (!LoadFromStream(file, nameHint, pwdUI)) {
            return false;
        }
        return FinishLoading();
    }

    if (str::EqI(ext, ".pdb")) {
        // synthesize a .html file from pdb file
        Str d = PalmDocToHTML(path);
        if (str::IsEmpty(d)) {
            return false;
        }
        fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (const u8*)d.s, d.len);
        fz_stream* file = fz_open_buffer(ctx, buf);
        fz_drop_buffer(ctx, buf);
        str::Free(d);
        TempStr nameHint = str::JoinTemp(path, StrL(".html"));
        if (!LoadFromStream(file, nameHint, pwdUI)) {
            return false;
        }
        return FinishLoading();
    }

    fz_stream* file = FzOpenOrReadFile(ctx, fnCopy);
    ok = LoadFromStream(file, FilePath(), pwdUI);
    if (!ok) {
        return false;
    }

    if (streamNo < 0) {
        ok = FinishLoading();
        if (ok) {
            return true;
        }
        fz_drop_document(ctx, _doc);
        _doc = nullptr;
        file = FzReadMaybeFixPDF(ctx, FilePath());
        if (!file) {
            return false;
        }
        ok = LoadFromStream(file, FilePath(), pwdUI);
        if (!ok) {
            return false;
        }
        return FinishLoading();
    }

    // load a stream from inside a pdf document
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
            fz_report_error(ctx);
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

// TODO: need to do stuff to support .txt etc.
bool EngineMupdf::Load(IStream* stream, Str nameHint, PasswordUI* pwdUI) {
    auto ctx = Ctx();
    ReportIf(FilePath() || _doc);
    if (!ctx) {
        return false;
    }

    fz_stream* stm = nullptr;
    fz_var(stm);
    fz_try(ctx) {
        stm = FzOpenIStream(ctx, stream);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
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

// is implemented in SumatraPDF.exe, PdfFilter and PdfPreview
// TODO: allow setting per
extern EBookUI* GetEBookUI();

// stm is either freed or retained via _doc
bool EngineMupdf::LoadFromStream(fz_stream* stm, Str nameHint, PasswordUI* pwdUI) {
    if (!stm) {
        return false;
    }
    // a 3rd-party DLL might have unmasked fp exceptions on this thread, which
    // would crash mupdf on benign NaN comparisons e.g. in pdf_resolve_link_dest()
    MaskFpExceptions();
    auto ctx = Ctx();

#if 0
    /* a heuristic. a layout page size for .epub is A5 but that makes a font size too
       large for non-epub files like .txt or .xml, so for those use larger A4 */
    float ldx = layoutA4DxPt;
    float ldy = layoutA4DyPt;
    TempStr ext = path::GetExtTemp(nameHint);
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

    // mupdf 1.28 replaced the global fz_set_user_css / fz_set_use_document_css
    // with per-document styling via fz_style_document (applied after the
    // document is opened, before fz_layout_document)
    TempStr userCss;
    int usePublisherCss = 1; // use the document's own (publisher) CSS by default
    auto eBookUI = GetEBookUI();
    if (eBookUI) {
        // accept any reasonable font size; the old upper bound of 30 made
        // larger sizes silently revert to the default (#2276). 256 is just a
        // sanity cap to reject garbage values.
        if (eBookUI->fontSize > 6 && eBookUI->fontSize < 256) {
            lfontDy = eBookUI->fontSize;
        }
        if (eBookUI->layoutDx > 100) {
            ldx = eBookUI->layoutDx;
        }
        if (eBookUI->layoutDy > 100) {
            ldy = eBookUI->layoutDy;
        }
        if (eBookUI->customCSS) {
            userCss = eBookUI->customCSS;
        }
        usePublisherCss = eBookUI->ignoreDocumentCSS ? 0 : 1;
    }
    const char* userCssZ = userCss ? userCss.s : nullptr;

    float dx, dy, fontDy;
    _doc = nullptr;
    fz_archive* dir = nullptr;
    fz_var(dx);
    fz_var(dy);
    fz_var(fontDy);
    fz_var(dir);
    Kind kind = GuessFileTypeFromName(nameHint);
    const char* nameHintZ = CStrTemp(nameHint);
    if (kind == kindFileMarkdown) {
        TempStr parentDir = path::GetDirTemp(nameHint);
        if (!str::IsEmpty(parentDir)) {
            fz_try(ctx) {
                dir = fz_open_directory(ctx, CStrTemp(parentDir));
            }
            fz_catch(ctx) {
                dir = nullptr;
                fz_report_error(ctx);
            }
        }
    }
    fz_try(ctx) {
        if (dir) {
            _doc = fz_open_document_with_stream_and_dir(ctx, nameHintZ, stm, dir);
        } else {
            _doc = fz_open_document_with_stream(ctx, nameHintZ, stm);
        }
        // per-document CSS styling (replaces the global fz_set_user_css /
        // fz_set_use_document_css); must be set before fz_layout_document
        fz_style_document(ctx, _doc, usePublisherCss, userCssZ);
        pdfdoc = pdf_specifics(ctx, _doc);
        dx = DpiScale(ldx, displayDPI);
        dy = DpiScale(ldy, displayDPI);
        fontDy = DpiScale(lfontDy, displayDPI);
        fz_layout_document(ctx, _doc, dx, dy, fontDy);
    }
    fz_always(ctx) {
        fz_drop_stream(ctx, stm);
        fz_drop_archive(ctx, dir);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        _doc = nullptr;
    }
    if (!_doc) {
        return false;
    }

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
        Str pwd = pwdUI->GetPassword(FilePath(), digest, decryptKey, &saveKey);
        if (!pwd) {
            // password not given or encryption key has been remembered
            ok = saveKey;
            break;
        }

        // MuPDF expects passwords to be UTF-8 encoded
        TempStr pwdA = pwd;
        ok = fz_authenticate_password(ctx, _doc, pwdA.s);
        // according to the spec (1.7 ExtensionLevel 3), the password
        // for crypt revisions 5 and above are in SASLprep normalization
        if (!ok) {
            // TODO: this is only part of SASLprep
            Str normalized = NormalizeString(pwd, 5 /* NormalizationKC */);
            str::Free(pwd);
            pwd = normalized;
            if (pwd) {
                pwdA = pwd;
                ok = fz_authenticate_password(ctx, _doc, pwdA.s);
            }
        }
        // older Acrobat versions seem to have considered passwords to be in codepage 1252
        // note: such passwords aren't portable when stored as Unicode text
        if (!ok && GetACP() != 1252) {
            TempStr pwd_ansi = pwd;
            TempWStr pwdCp1252 = strconv::StrCPToWStrTemp(pwd_ansi, 1252);
            pwdA = ToUtf8Temp(pwdCp1252);
            ok = fz_authenticate_password(ctx, _doc, pwdA.s);
        }
        if (ok) {
            str::ReplaceWithCopy(&pdfPassword, pwdA);
        }
        str::Free(pwd);
    }

    if (pdfdoc && ok && saveKey) {
        memcpy(digest + 16, pdf_crypt_key(ctx, pdfdoc->crypt), 32);
        TempStr hex = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));
        decryptionKey = str::Dup(arena, hex);
    }
    // TODO: if !ok,
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
        fz_report_error(ctx);
        root = nullptr;
    }
    if (!root) {
        return layout;
    }

    const char* name = nullptr;
    fz_var(name);
    fz_try(ctx) {
        name = pdf_to_name(ctx, pdf_dict_gets(ctx, root, "PageLayout"));
        if (str::EndsWith(Str(name), "Right")) {
            layout.type = PageLayout::Type::Book;
        } else if (str::StartsWith(Str(name), "Two")) {
            layout.type = PageLayout::Type::Facing;
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }

    pdf_obj* prefs = nullptr;
    const char* direction = nullptr;
    fz_var(prefs);
    fz_var(direction);
    fz_try(ctx) {
        prefs = pdf_dict_gets(ctx, root, "ViewerPreferences");
        direction = pdf_to_name(ctx, pdf_dict_gets(ctx, prefs, "Direction"));
        if (str::Eq(Str(direction), "R2L")) {
            layout.r2l = true;
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }

    return layout;
}

bool GetPdfViewerPrintPrefs(EngineBase* engineBase, PdfViewerPrintPrefs& prefs) {
    EngineMupdf* engine = AsEngineMupdf(engineBase);
    if (!engine || !engine->pdfdoc) {
        return false;
    }
    fz_context* ctx = engine->Ctx();
    if (!ctx) {
        return false;
    }
    pdf_document* pdfdoc = engine->pdfdoc;

    ScopedCritSec cs(&engine->docLock);

    pdf_obj* vprefs = nullptr;
    fz_var(vprefs);
    fz_try(ctx) {
        pdf_obj* root = pdf_dict_gets(ctx, pdf_trailer(ctx, pdfdoc), "Root");
        vprefs = pdf_dict_gets(ctx, root, "ViewerPreferences");
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        vprefs = nullptr;
    }
    if (!vprefs) {
        return false;
    }

    bool found = false;
    fz_try(ctx) {
        pdf_obj* o = pdf_dict_gets(ctx, vprefs, "PickTrayByPDFSize");
        if (pdf_is_bool(ctx, o)) {
            prefs.hasPickTrayByPdfSize = true;
            prefs.pickTrayByPdfSize = pdf_to_bool(ctx, o) != 0;
            found = true;
        }
        o = pdf_dict_gets(ctx, vprefs, "NumCopies");
        if (pdf_is_int(ctx, o)) {
            prefs.hasNumCopies = true;
            prefs.numCopies = pdf_to_int(ctx, o);
            found = true;
        }
        const char* dup = pdf_to_name(ctx, pdf_dict_gets(ctx, vprefs, "Duplex"));
        if (str::Eq(Str(dup), "Simplex")) {
            prefs.hasDuplex = true;
            prefs.duplex = PdfDuplexPref::Simplex;
            found = true;
        } else if (str::Eq(Str(dup), "DuplexFlipShortEdge")) {
            prefs.hasDuplex = true;
            prefs.duplex = PdfDuplexPref::FlipShortEdge;
            found = true;
        } else if (str::Eq(Str(dup), "DuplexFlipLongEdge")) {
            prefs.hasDuplex = true;
            prefs.duplex = PdfDuplexPref::FlipLongEdge;
            found = true;
        }
        const char* ps = pdf_to_name(ctx, pdf_dict_gets(ctx, vprefs, "PrintScaling"));
        if (str::Eq(Str(ps), "None")) {
            prefs.hasPrintScaling = true;
            prefs.printScalingNone = true;
            found = true;
        } else if (str::Eq(Str(ps), "AppDefault")) {
            prefs.hasPrintScaling = true;
            prefs.printScalingNone = false;
            found = true;
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return found;
}

static bool IsLinearizedFile(EngineMupdf* e) {
    if (!e->pdfdoc) {
        return false;
    }
    auto ctx = e->Ctx();

    ScopedCritSec scope(&e->docLock);
    int isLinear = 0;
    fz_try(ctx) {
        isLinear = pdf_doc_was_linearized(ctx, e->pdfdoc);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        isLinear = 0;
    }
    return isLinear;
}

static void FinishNonPDFLoading(EngineMupdf* e) {
    ScopedCritSec scope(&e->docLock);

    auto ctx = e->Ctx();
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
            fz_report_error(ctx);
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
        fz_report_error(ctx);
        fz_warn(ctx, "Couldn't load outline");
    }
}

// Resolve an external-file image stream (issue #3731): the PDF's /F entry
// names a file that must sit next to the PDF. Gated by the AllowExternalImages
// setting and restricted to sibling files (no path separators / drive specs)
// for security -- Acrobat denies these by default too.
static fz_buffer* EngineMupdfLoadExternalStream(fz_context* ctx, const char* filespec, void* opaque) {
    if (!gAllowExternalImages) {
        return nullptr;
    }
    EngineMupdf* e = (EngineMupdf*)opaque;
    Str pdfPath = e ? e->FilePath() : Str{};
    Str spec = Str(filespec);
    if (!pdfPath || !spec) {
        return nullptr;
    }
    // sibling-only: reject anything with a path separator or drive spec so the
    // PDF can only pull a file from its own directory
    if (str::ContainsChar(spec, '/') || str::ContainsChar(spec, '\\') || str::ContainsChar(spec, ':')) {
        return nullptr;
    }
    TempStr full = path::JoinTemp(path::GetDirTemp(pdfPath), spec);
    if (!file::Exists(full)) {
        return nullptr;
    }
    Str data = file::ReadFile(full);
    if (len(data) == 0) {
        return nullptr;
    }
    fz_buffer* buf = nullptr;
    fz_try(ctx) {
        buf = fz_new_buffer_from_copied_data(ctx, (u8*)data.s, (size_t)data.len);
    }
    fz_catch(ctx) {
        buf = nullptr;
    }
    str::Free(data);
    return buf;
}

bool EngineMupdf::FinishLoading() {
    auto ctx = Ctx();
    pdfdoc = pdf_specifics(ctx, _doc);
    if (pdfdoc) {
        // allow loading external-file image streams from next to the PDF (#3731)
        pdf_set_load_external_stream_fn(ctx, pdfdoc, EngineMupdfLoadExternalStream, this);
    }

    pageCount = 0;
    fz_var(pageCount);
    fz_try(ctx) {
        // this call might throw the first time
        pageCount = fz_count_pages(ctx, _doc);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
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
        auto pi = New<FzPageInfo>(arena);
        pages.Append(pi);
    }
    if (!pdfdoc) {
        FinishNonPDFLoading(this);
        return true;
    }

    ScopedCritSec scope(&docLock);

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
            fz_report_error(ctx);
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
        fz_report_error(ctx);
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
        fz_report_error(ctx);
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
        fz_report_error(ctx);
        fz_warn(ctx, "Couldn't load page labels");
    }
    if (pageLabels) {
        hasPageLabels = true;
    }

    // enable mupdf's JavaScript engine so form-field calculate / validate /
    // format actions run (e.g. auto-summed totals on a fillable form). mujs is
    // sandboxed to the PDF/form API -- it has no file or network access. Can be
    // turned off with the DisableJavaScript advanced setting.
    // Use the engine's main context (_ctx), not a per-thread clone: MuJS stores
    // the fz_context passed here as its allocator and fz_drop_document() must
    // tear it down with the same context (see ~EngineMupdf).
    if (!gDisableFormJavaScript) {
        fz_try(_ctx) {
            pdf_enable_js(_ctx, pdfdoc);
        }
        fz_catch(_ctx) {
            fz_report_error(_ctx);
            fz_warn(_ctx, "Couldn't enable form JavaScript");
        }
    }

    return true;
}

static NO_INLINE IPageDestination* DestFromAttachment(EngineMupdf* engine, fz_outline* outline) {
    PageDestination* dest = new PageDestination();
    dest->kind = kindDestinationAttachment;
    // WCHAR* path = ToWStr(outline->uri);
    dest->name = str::Dup(outline->title);
    // page is really a stream number
    Str title = outline->title ? Str(outline->title) : StrL("");
    TempStr nameHex = str::MemToHexTemp(title);
    dest->value = str::Dup(fmt("%s:%d:attachname=%s", engine->FilePath(), outline->page.page, nameHex));
    dest->pageNo = outline->page.page;
    return dest;
}

TocItem* EngineMupdf::BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment) {
    TocItem* root = nullptr;
    TocItem* curr = nullptr;

    auto ctx = Ctx();
    while (outline) {
        TempStr name;
        if (outline->title) {
            // must convert to Unicode because PdfCleanString() doesn't work on utf8
            TempWStr nameW = ToWStrTemp(Str(outline->title));
            PdfCleanStringInPlace(nameW);
            name = ToUtf8Temp(nameW);
        }

        int pageNo = FzGetPageNo(ctx, _doc, nullptr, outline);

        IPageDestination* dest = nullptr;
        if (isAttachment) {
            dest = DestFromAttachment(this, outline);
        } else {
            dest = NewPageDestinationMupdf(ctx, _doc, nullptr, outline);
        }
        TocItem* item = NewTocItemWithDestination(parent, name, dest);
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
            ReportIf(!curr);
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

    ScopedCritSec cs(&docLock);

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

IPageDestination* EngineMupdf::GetNamedDest(Str name) {
    if (!pdfdoc) {
        return nullptr;
    }
    auto ctx = Ctx();
    IPageDestination* pageDest = nullptr;
    ScopedCritSec scope2(&docLock);
    TempStr uri = str::JoinTemp(StrL("#nameddest="), name);
    float x, y, zoom = 0;
    int pageNo = ResolveLink(ctx, _doc, uri, &x, &y);
    if (pageNo < 0) {
        return nullptr;
    }

    RectF r{x, y, 0, 0};
    pageDest = NewSimpleDest(pageNo, r, zoom);
    return pageDest;
}

#if 0
IPageDestination* EngineMupdf::GetNamedDest(Str name) {
    if (!pdfdoc) {
        return nullptr;
    }

    ScopedCritSec scope1(&pagesLock);
    ScopedCritSec scope2(&docLock);

    int nameLen = len(name);
    pdf_obj* dest = nullptr;

    fz_var(dest);
    pdf_obj* nameobj = nullptr;
    fz_var(nameobj);
    fz_try(ctx) {
        nameobj = pdf_new_string(ctx, name, nameLen);
        dest = pdf_lookup_dest(ctx, pdfdoc, nameobj);
        pdf_drop_obj(ctx, nameobj);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
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
        fz_report_error(ctx);
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
#endif

// return a page but only if is fully loaded
FzPageInfo* EngineMupdf::GetFzPageInfoFast(int pageNo) {
    ScopedCritSec scope(&pagesLock);
    ReportIf(pageNo < 1 || pageNo > pageCount);
    FzPageInfo* pageInfo = pages[pageNo - 1];
    if (!pageInfo->page || !pageInfo->fullyLoaded) {
        return nullptr;
    }
    return pageInfo;
}

static IPageElement* NewFzComment(Str comment, int pageNo, RectF rect) {
    auto res = new PageElementComment(comment);
    res->pageNo = pageNo;
    res->rect = rect;
    return res;
}

// must be called inside fz_try
static IPageElement* MakePdfCommentFromPdfAnnot(fz_context* ctx, int pageNo, pdf_annot* annot) {
    fz_rect rect = pdf_bound_annot(ctx, annot);
    Str contents = pdf_annot_contents(ctx, annot);
    Str label = pdf_annot_field_label(ctx, annot);
    Str s = contents;
    // TODO: use separate classes for comments and tooltips?
    if (!contents) {
        s = label;
    }
    RectF rd = ToRectF(rect);
    return NewFzComment(s, pageNo, rd);
}

// must be called inside fz_try
static void RebuildCommentsFromAnnotationsInner(fz_context* ctx, pdf_annot* annot, int pageNo,
                                                Vec<IPageElement*>& comments) {
    auto tp = pdf_annot_type(ctx, annot);
    Str contents = pdf_annot_contents(ctx, annot); // don't free
    if (contents.len > 128) {
        contents = str::DupTemp(Str(contents.s, 128));
    }
    bool isContentsEmpty = !contents;
    Str label = pdf_annot_field_label(ctx, annot); // don't free
    bool isLabelEmpty = !label;
    int flags = pdf_annot_field_flags(ctx, annot);
    bool isEmpty = isContentsEmpty && isLabelEmpty;

    // const char* tpStr = pdf_string_from_annot_type(ctx, tp);
    //  logf("MakePageElementCommentsFromAnnotations: annot %d '%s', contents: '%s', label: '%s'\n", tp, tpStr,
    //  contents, abel);

    if (PDF_ANNOT_FILE_ATTACHMENT == tp) {
        logf("found file attachment annotation\n");

        pdf_filespec_params fileParams = {};
        pdf_obj* fs = pdf_annot_filespec(ctx, annot);
        int num = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
        pdf_get_filespec_params(ctx, fs, &fileParams);
        const char* attname = fileParams.filename;
        fz_rect rect = pdf_bound_annot(ctx, annot);
        if (str::IsEmpty(attname) || fz_is_empty_rect(rect) || !pdf_is_embedded_file(ctx, fs)) {
            return;
        }

        logf("attachment: %s, num: %d\n", Str(attname), num);

        auto dest = new PageDestination();
        dest->kind = kindDestinationLaunchEmbedded;
        dest->value = str::Dup(attname);
        dest->embedObjNum = num;

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
            fz_report_error(ctx);
        }
    }

    // re-order list into top-to-bottom order (i.e. last-to-first)
    comments.Reverse();
}

// like GetFzPageInfo() but fails if we can't acquire locks
// prevents blocking main thread due to render thread keeping the lock
// https://github.com/sumatrapdfreader/sumatrapdf/issues/4145
// https://github.com/sumatrapdfreader/sumatrapdf/issues/4187
FzPageInfo* EngineMupdf::GetFzPageInfoCanFail(int pageNo) {
#if 0
    return GetFzPageInfo(pageNo, true);
#else
    FzPageInfo* res = nullptr;
    if (!TryEnterCriticalSection(&pagesLock)) {
        return nullptr;
    }
    if (TryEnterCriticalSection(&docLock)) {
        // CRITICAL_SECTION locking is recursive
        res = GetFzPageInfo(pageNo, true);
        LeaveCriticalSection(&docLock);
    }
    LeaveCriticalSection(&pagesLock);
    return res;
#endif
}

/* SumatraPDF */
fz_stext_page* fz_new_stext_page_from_page2(fz_context* ctx, fz_page* page, const fz_stext_options* options,
                                            fz_cookie* cookie) {
    fz_stext_page* text;
    fz_device* dev = NULL;

    fz_var(dev);

    if (page == NULL) return NULL;

    text = fz_new_stext_page(ctx, fz_bound_page(ctx, page));
    fz_try(ctx) {
        dev = fz_new_stext_device(ctx, text, options);
        fz_run_page_contents(ctx, page, dev, fz_identity, cookie);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
    }
    fz_catch(ctx) {
        fz_drop_stext_page(ctx, text);
        fz_rethrow(ctx);
    }

    return text;
}

// Like fz_new_stext_page_from_page() but runs the *whole* page - contents plus
// annotations and form-field widgets - instead of only the page contents. This
// makes free-text annotations and form-field values part of the extracted text
// so they can be selected and searched, matching Acrobat (and SumatraPDF <=3.1).
// mupdf's fz_new_stext_page_from_page() runs only fz_run_page_contents() (issue #1649).
static fz_stext_page* fz_new_stext_page_from_whole_page(fz_context* ctx, fz_page* page,
                                                        const fz_stext_options* options) {
    if (page == nullptr) {
        return nullptr;
    }
    fz_stext_page* text = fz_new_stext_page(ctx, fz_bound_page(ctx, page));
    fz_device* dev = nullptr;
    fz_var(dev);
    fz_try(ctx) {
        dev = fz_new_stext_device(ctx, text, options);
        fz_run_page(ctx, page, dev, fz_identity, nullptr);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
    }
    fz_catch(ctx) {
        fz_drop_stext_page(ctx, text);
        fz_rethrow(ctx);
    }
    return text;
}

// Maybe: handle FZ_ERROR_TRYLATER, which can happen when parsing from network.
// (I don't think we read from network now).
// Maybe: when loading fully, cache extracted text in FzPageInfo
// so that we don't have to re-do fz_new_stext_page_from_page() when doing search
FzPageInfo* EngineMupdf::GetFzPageInfo(int pageNo, bool loadQuick, fz_cookie* cookie) {
    auto ctx = Ctx();
    // TODO: minimize time spent under pagesLock when fully loading
    ScopedCritSec scope(&pagesLock);

    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) {
        return nullptr;
    }
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = pages[pageIdx];
    if (!pageInfo) {
        return nullptr;
    }

    // page-running operations on this specific page run under per-page lock.
    // pagesLock (held above) serializes concurrent fz_load_page on _doc.
    ScopedCritSec ctxScope(&renderLock);
    if (!pageInfo->page) {
        fz_try(ctx) {
            pageInfo->page = fz_load_page(ctx, _doc, pageIdx);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }

    fz_page* page = pageInfo->page;
    if (!page) {
        return nullptr;
    }

    // build annotations + widgets info on first access
    if (pdfdoc && !pageInfo->annotsLoaded) {
        pageInfo->annotsLoaded = true;
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
            // form fields (widgets) are a separate mupdf list; keep them in their
            // own list so they're hit-testable for form filling but don't show up
            // as annotations (comments, edit-annotations panel)
            pdf_annot* widget = pdf_first_widget(ctx, pdfpage);
            while (widget) {
                Annotation* a = MakeAnnotationWrapper(this, widget, pageNo);
                if (a) {
                    pageInfo->widgets.Append(a);
                }
                widget = pdf_next_widget(ctx, widget);
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
        RebuildCommentsFromAnnotations(ctx, pageInfo);
    }

    if (loadQuick || pageInfo->fullyLoaded) {
        return pageInfo;
    }

    ReportIf(pageInfo->pageNo != pageNo);

    pageInfo->fullyLoaded = true;

    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_stext_options opts = NewTextPageOptions(FZ_STEXT_PRESERVE_IMAGES);
    fz_try(ctx) {
        stext = fz_new_stext_page_from_page2(ctx, page, &opts, cookie);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }

    fz_link* link = fz_load_links(ctx, page);
    link = FixupPageLinks(link); // TOOD: is this necessary?
    pageInfo->retainedLinks = link;
    while (link) {
        auto pel = NewLinkDestination(pageNo, ctx, _doc, link, nullptr);
        if (pel) {
            pageInfo->links.Append(pel);
        }
        link = link->next;
    }

    if (!stext) {
        return pageInfo;
    }

    if (!disableAutoLinks) {
        FzLinkifyPageText(pageInfo, stext);
    }
    FzFindImagePositions(ctx, pageNo, pageInfo->images, stext);
    fz_drop_stext_page(ctx, stext);
    return pageInfo;
}

RectF EngineMupdf::PageMediabox(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) return {};
    FzPageInfo* pi = pages[pageNo - 1];
    return pi->mediabox;
}

// returns a kept reference to the cached "View" display list for the page,
// building+caching it on first call. Caller must fz_drop_display_list when done.
// must be called with pi->renderLock held (this both protects pi->displayList
// and serializes the page-running done by fz_new_display_list_from_page).
static fz_display_list* GetOrBuildPageDisplayList(FzPageInfo* pi, fz_context* ctx) {
    if (!pi->displayList) {
        fz_display_list* list = nullptr;
        fz_try(ctx) {
            list = fz_new_display_list_from_page(ctx, pi->page);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            list = nullptr;
        }
        pi->displayList = list;
    }
    if (!pi->displayList) {
        return nullptr;
    }
    return fz_keep_display_list(ctx, pi->displayList);
}

RectF EngineMupdf::PageContentBox(int pageNo, RenderTarget target) {
    auto ctx = Ctx();

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo) {
        // maybe should return a dummy size. not sure how this
        // will play with layout. The page should fail to render
        // since the doc is broken and page is missing
        return RectF();
    }

    RectF mediabox = pageInfo->mediabox;

    fz_rect pagerect;
    fz_display_list* keptList = nullptr;
    {
        // Hold per-page lock briefly: page bounds + (re-)acquire cached display list.
        ScopedCritSec scope(&renderLock);
        pagerect = fz_bound_page(ctx, pageInfo->page);
        keptList = GetOrBuildPageDisplayList(pageInfo, ctx);
    }
    if (!keptList) {
        return mediabox;
    }

    // Lock-free: bbox-device run on a display list is concurrency-safe.
    fz_cookie fzcookie{};
    fz_rect rect = fz_empty_rect;
    fz_device* dev = nullptr;
    fz_var(dev);
    fz_try(ctx) {
        dev = fz_new_bbox_device(ctx, &rect);
        fz_run_display_list(ctx, keptList, dev, fz_identity, pagerect, &fzcookie);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_display_list(ctx, keptList);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
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
        Str name = FilePath();
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

Pixmap* EngineMupdf::RenderPage(RenderPageArgs& args) {
    auto ctx = Ctx();
    auto pageNo = args.pageNo;

    fz_cookie* fzcookie = nullptr;
    FitzAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new FitzAbortCookie();
        *args.cookie_out = cookie;
        fzcookie = (fz_cookie*)cookie->GetData();
    }

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false, fzcookie);
    if (!pageInfo || !pageInfo->page) {
        return nullptr;
    }
    fz_page* page = pageInfo->page;

    // AA level is per-thread-context state since Ctx() clones; no lock needed.
    if (disableAntiAlias) {
        fz_set_aa_level(ctx, 0);
    } else {
        // 8 seems to be the default
        fz_set_aa_level(ctx, 8);
    }

    auto pageRect = args.pageRect;
    auto zoom = args.zoom;
    auto rotation = args.rotation;

    // The "View" rendering (no Print, no hideAnnotations) is what
    // fz_new_display_list_from_page produces; safe to cache and re-run lock-free.
    bool useCache = (args.target == RenderTarget::View) && !hideAnnotations;

    fz_rect pRect;
    fz_matrix ctm;
    fz_irect ibounds;
    fz_display_list* keptList = nullptr;

    {
        // Hold per-page lock while we touch the page (bounds, optional list build).
        ScopedCritSec cs(&renderLock);

        if (pageRect) {
            pRect = ToFzRect(*pageRect);
        } else {
            // TODO(port): use pageInfo->mediabox?
            pRect = fz_bound_page(ctx, page);
        }
        ctm = viewctm(page, zoom, rotation);
        ibounds = fz_round_rect(fz_transform_rect(pRect, ctm));

        if (useCache) {
            keptList = GetOrBuildPageDisplayList(pageInfo, ctx);
        }
    }

    fz_colorspace* csRgb = fz_device_rgb(ctx);
    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    RenderedBitmap* bitmap = nullptr;

    fz_var(dev);
    fz_var(pix);
    fz_var(bitmap);

    if (keptList) {
        // Display-list replay still decodes shared images (JBIG2 etc.) under
        // the hood, and mupdf's image store races on concurrent decode of the
        // same image -- crashes seen in template_image_compose_opt with use-
        // after-free. Hold renderLock to serialize.
        ScopedCritSec rls(&renderLock);
        fz_try(ctx) {
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            fz_clear_pixmap_with_value(ctx, pix, 0xff);
            dev = fz_new_draw_device(ctx, ctm, pix);
            fz_run_display_list(ctx, keptList, dev, fz_identity, pRect, fzcookie);
            fz_close_device(ctx, dev);
            bitmap = NewRenderedFzPixmap(ctx, pix);
        }
        fz_always(ctx) {
            if (dev) {
                fz_drop_device(ctx, dev);
            }
            if (pix) {
                fz_drop_pixmap(ctx, pix);
            }
            fz_drop_display_list(ctx, keptList);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            delete bitmap;
            return {};
        }
        return PixmapFromRenderedBitmap(bitmap);
    }

    // Fallback: Print or hideAnnotations (each needs different content/usage,
    // not what the cached display list captured), or display-list construction
    // failed. Run the page directly under per-page lock.
    ScopedCritSec cs(&renderLock);

    Str usage = "View";
    switch (args.target) {
        case RenderTarget::Print:
            usage = "Print";
            break;
    }
    const char* usageZ = CStrTemp(usage);

    pdf_page* pdfpage = nullptr;
    fz_var(pdfpage);
    if (pdfdoc) {
        fz_try(ctx) {
            pdfpage = pdf_page_from_fz_page(ctx, page);
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            fz_clear_pixmap_with_value(ctx, pix, 0xff);
            dev = fz_new_draw_device(ctx, ctm, pix);
            if (hideAnnotations) {
                pdf_run_page_contents_with_usage(ctx, pdfpage, dev, fz_identity, usageZ, fzcookie);
                pdf_run_page_widgets_with_usage(ctx, pdfpage, dev, fz_identity, usageZ, fzcookie);
            } else {
                pdf_run_page_with_usage(ctx, pdfpage, dev, fz_identity, usageZ, fzcookie);
            }
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
            fz_report_error(ctx);
            delete bitmap;
            return {};
        }
    } else {
        fz_try(ctx) {
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            fz_clear_pixmap_with_value(ctx, pix, 0xff);
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
            fz_report_error(ctx);
            delete bitmap;
            return {};
        }
    }

    return PixmapFromRenderedBitmap(bitmap);
}

// don't delete the result
IPageElement* EngineMupdf::GetElementAtPos(int pageNo, PointF pt) {
    FzPageInfo* pageInfo = GetFzPageInfoCanFail(pageNo);
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
    ReportIf(kindDestinationMupdf != dest->GetKind());
    PageDestinationMupdf* link = (PageDestinationMupdf*)dest;
    ReportIf(!(link->outline || link->link));
    Str uri = link->outline ? Str(link->outline->uri) : Str{};
    if (!link->outline) {
        uri = Str(link->link->uri);
    }
    if (!uri) {
        return;
    }
    if (IsExternalLink(uri)) {
        linkHandler->LaunchURL(uri);
        return;
    }

    // those locks must be taken in this order
    // we need to lock pagesLock because it might
    // be taken below
    ScopedCritSec csPages(&e->pagesLock);
    ScopedCritSec cs(&e->docLock);

    int pageNo = -1;
    fz_link_dest ldest{};
    auto ctx = e->Ctx();
    fz_var(pageNo);
    fz_try(ctx) {
        ldest = fz_resolve_link_dest(ctx, e->_doc, CStrTemp(uri));
        pageNo = fz_page_number_from_location(ctx, e->_doc, ldest.loc);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logfa("HandleLinkMupdf: fz_resolve_link() for '%s' failed\n", uri);
    }
    if (pageNo < 0) {
        TempStr localPath;
        Str localFragment;
        if (IsMupdfLocalFileLink(uri, &localPath, &localFragment)) {
            PageDestinationFile fileDest(localPath, localFragment);
            linkHandler->GotoLink(&fileDest);
        }
        return;
    }

    // TODO: handle ldest.type like FZ_LINK_DEST_FIT_H ?
    float x = isnan(ldest.x) ? DEST_USE_DEFAULT : ldest.x;
    float y = isnan(ldest.y) ? DEST_USE_DEFAULT : ldest.y;
    float zoom = isnan(ldest.zoom) ? 0.f : ldest.zoom;
    zoom = zoom / 100; // mupdf uses 100 as 100% zoom, we use 1
    float w = isnan(ldest.w) ? DEST_USE_DEFAULT : ldest.w;
    float h = isnan(ldest.h) ? DEST_USE_DEFAULT : ldest.h;

    RectF r(x, y, w, h);
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
    ReportIf(kindPageElementImage != ipel->GetKind());
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
    auto ctx = Ctx();

    fz_rect bounds;
    fz_var(bounds);
    fz_try(ctx) {
        bounds = fz_bound_page(ctx, page);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        bounds = {};
    }
    if (fz_is_empty_rect(bounds)) {
        bounds = {0, 0, 612, 792};
    }
    return FzCreateViewCtm(bounds, zoom, rotation);
}

RenderedBitmap* EngineMupdf::GetPageImage(int pageNo, RectF rect, int imageIdx) {
    auto ctx = Ctx();

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo->page) {
        return nullptr;
    }
    const auto& images = pageInfo->images;
    bool outOfBounds = imageIdx >= len(images);
    fz_rect imgRect = images.at(imageIdx)->rect;
    bool badRect = ToRectF(imgRect) != rect;
    ReportIf(outOfBounds);
    ReportIf(badRect);
    if (outOfBounds || badRect) {
        return nullptr;
    }

    ScopedCritSec scope(&docLock);

    fz_image* image = FzFindImageAtIdx(ctx, pageInfo, imageIdx);
    // can happen when the file becomes unreadable (e.g. network drive read errors)
    if (!image) {
        return nullptr;
    }

    RenderedBitmap* bmp = nullptr;
    fz_pixmap* pixmap = nullptr;
    fz_pixmap* mask = nullptr;
    fz_var(pixmap);
    fz_var(mask);
    fz_var(bmp);

    fz_try(ctx) {
        // TODO(port): not sure if should provide subarea, w and h
        pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
        // Match `extract -r`: normalize embedded images to RGB before creating
        // a Windows bitmap for copy/save operations.
        if (pixmap && pixmap->colorspace && !fz_colorspace_is_rgb(ctx, pixmap->colorspace)) {
            fz_pixmap* rgb =
                fz_convert_pixmap(ctx, pixmap, fz_device_rgb(ctx), nullptr, nullptr, fz_default_color_params, 1);
            fz_drop_pixmap(ctx, pixmap);
            pixmap = rgb;
        }
        // The image's visible content can live entirely in its soft mask: the
        // base color image is then solid black and the copy is a black box
        // (issue #1682). The /SMask isn't baked into the color pixmap (it's
        // applied by the interpreter at draw time), so composite it here -- over
        // a white background, matching how the image looks on the (white) page.
        if (image->mask && pixmap && fz_colorspace_is_rgb(ctx, pixmap->colorspace)) {
            mask = fz_get_pixmap_from_image(ctx, image->mask, nullptr, nullptr, nullptr, nullptr);
            if (mask && mask->n == 1) {
                int bw = pixmap->w, bh = pixmap->h, bn = pixmap->n;
                int mw = mask->w, mh = mask->h, mn = mask->n;
                u8* bp = pixmap->samples;
                u8* mp = mask->samples;
                for (int y = 0; y < bh; y++) {
                    int my = (mh == bh) ? y : (int)((i64)y * mh / bh);
                    for (int x = 0; x < bw; x++) {
                        int mx = (mw == bw) ? x : (int)((i64)x * mw / bw);
                        int a = mp[(size_t)my * mask->stride + (size_t)mx * mn]; // smask = alpha
                        u8* px = bp + (size_t)y * pixmap->stride + (size_t)x * bn;
                        for (int k = 0; k < 3; k++) {
                            px[k] = (u8)((px[k] * a + 255 * (255 - a)) / 255);
                        }
                    }
                }
            }
        }
        bmp = NewRenderedFzPixmap(ctx, pixmap);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, mask);
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        bmp = nullptr;
    }

    return bmp;
}

PageText EngineMupdf::ExtractPageText(int pageNo) {
    auto ctx = Ctx();

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, true);
    if (!pageInfo) {
        return {};
    }

    ScopedCritSec scope(&renderLock);

    fz_stext_page* stext = nullptr;
    fz_var(stext);
    fz_stext_options opts = NewTextPageOptions();
    fz_try(ctx) {
        stext = fz_new_stext_page_from_whole_page(ctx, pageInfo->page, &opts);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    if (!stext) {
        return {};
    }
    PageText res;
    res.text = FzTextPageToUtf8(stext, &res.coords);
    fz_drop_stext_page(ctx, stext);
    res.len = res.text.len;
    res.nCodepoints = Utf8CodepointCount(res.text);
    return res;
}

void EngineMupdf::ReleaseTextExtractionThreadContext() {
    ReleasePerThreadContext(this);
}

static void pdf_extract_fonts(fz_context* ctx, pdf_obj* res, Vec<pdf_obj*>& fontList, Vec<pdf_obj*>& resList) {
    // dedupe/cycle-protect via resList, not pdf_mark_obj: marks mutate shared
    // pdf_obj flags, which races with other threads using marks (and would
    // leave objects marked while locks are dropped between pages)
    if (!res || resList.Contains(res)) {
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

TempStr EngineMupdf::ExtractFontListTemp() {
    if (!pdfdoc) {
        return {};
    }

    Vec<pdf_obj*> fontList;
    Vec<pdf_obj*> resList;

    auto ctx = Ctx();

    // collect all fonts from all page objects.
    // this runs on a background thread (GetFontsThread) so it must not
    // starve the UI thread: walk raw pdf objects via pdf_lookup_page_obj
    // instead of GetFzPageInfo, which would load and parse every page
    // while holding pagesLock for the duration.
    // renderLock is needed too, not just docLock: we read the same objects
    // the renderer reads when building display lists (page /Resources etc.)
    // and pdf object reads can mutate (pdf_dict_get sorts large dicts,
    // pdf_resolve_indirect lazy-loads), so unserialized overlap with a
    // render makes pages fail with render errors. both locks are released
    // between pages so the UI thread can interleave
    int nPages = PageCount();
    for (int i = 0; i < nPages; i++) {
        ScopedCritSec renderScope(&renderLock);
        ScopedCritSec perPageScope(&docLock);
        fz_try(ctx) {
            pdf_obj* pageObj = pdf_lookup_page_obj(ctx, pdfdoc, i);
            pdf_obj* resources = pdf_dict_gets(ctx, pageObj, "Resources");
            pdf_extract_fonts(ctx, resources, fontList, resList);
            // fonts used by annotation appearance streams
            pdf_obj* annots = pdf_dict_gets(ctx, pageObj, "Annots");
            int nAnnots = pdf_array_len(ctx, annots);
            for (int k = 0; k < nAnnots; k++) {
                pdf_obj* annot = pdf_array_get(ctx, annots, k);
                pdf_obj* ap = pdf_dict_gets(ctx, pdf_dict_gets(ctx, annot, "AP"), "N");
                if (!ap) {
                    continue;
                }
                if (pdf_is_stream(ctx, ap)) {
                    pdf_extract_fonts(ctx, pdf_dict_gets(ctx, ap, "Resources"), fontList, resList);
                } else {
                    // appearance state sub-dictionary
                    for (int j = 0; j < pdf_dict_len(ctx, ap); j++) {
                        pdf_obj* state = pdf_dict_get_val(ctx, ap, j);
                        pdf_extract_fonts(ctx, pdf_dict_gets(ctx, state, "Resources"), fontList, resList);
                    }
                }
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }

    // font dicts are also read by the renderer when loading fonts, so
    // serialize with renders here as well
    ScopedCritSec renderScope(&renderLock);
    ScopedCritSec scope(&docLock);

    StrVec fonts;
    for (int i = 0; i < len(fontList); i++) {
        Str name, type, encoding;
        bool embedded = false;
        fz_try(ctx) {
            pdf_obj* font = fontList.at(i);
            pdf_obj* font2 = pdf_array_get(ctx, pdf_dict_gets(ctx, font, "DescendantFonts"), 0);
            if (!font2) {
                font2 = font;
            }

            name = Str(pdf_to_name(ctx, pdf_dict_getsa(ctx, font2, "BaseFont", "Name")));
            bool needAnonName = !name;
            if (needAnonName && font2 != font) {
                name = Str(pdf_to_name(ctx, pdf_dict_getsa(ctx, font, "BaseFont", "Name")));
                needAnonName = !name;
            }
            if (needAnonName) {
                name = fmt("<#%d>", pdf_obj_parent_num(ctx, font2));
            }
            embedded = false;
            pdf_obj* desc = pdf_dict_gets(ctx, font2, "FontDescriptor");
            if (desc && (pdf_dict_gets(ctx, desc, "FontFile") || pdf_dict_getsa(ctx, desc, "FontFile2", "FontFile3"))) {
                embedded = true;
            }
            if (embedded && name.len > 7 && name.s[6] == '+') {
                name = Str(name.s + 7);
            }

            type = Str(pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Subtype")));
            if (font2 != font) {
                Str type2 = Str(pdf_to_name(ctx, pdf_dict_gets(ctx, font2, "Subtype")));
                if (str::Eq(type2, "CIDFontType0")) {
                    type = "Type1 (CID)";
                } else if (str::Eq(type2, "CIDFontType2")) {
                    type = "TrueType (CID)";
                }
            }
            if (str::Eq(type, "Type3")) {
                embedded = pdf_dict_gets(ctx, font2, "CharProcs") != nullptr;
            }

            encoding = Str(pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Encoding")));
            if (str::Eq(encoding, "WinAnsiEncoding")) {
                encoding = "Ansi";
            } else if (str::Eq(encoding, "MacRomanEncoding")) {
                encoding = "Roman";
            } else if (str::Eq(encoding, "MacExpertEncoding")) {
                encoding = "Expert";
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            continue;
        }
        // check pointers, not Str's bool operator: empty type/encoding are
        // legitimate (e.g. a font with no Encoding) and handled below
        ReportIf(!name.s || !type.s || !encoding.s);

        str::Builder info;
        if (name.s[0] < 0 && MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, name.s, -1, nullptr, 0)) {
            TempStr s = strconv::ToMultiByteTemp(name, 936, CP_UTF8);
            info.Append(s);
        } else {
            info.Append(name);
        }
        if (!str::IsEmpty(encoding) || !str::IsEmpty(type) || embedded) {
            info.Append(" (");
            if (!str::IsEmpty(type)) {
                info.Append(fmt("%s; ", type));
            }
            if (!str::IsEmpty(encoding)) {
                info.Append(fmt("%s; ", encoding));
            }
            if (embedded) {
                info.Append("embedded; ");
            }
            info.RemoveAt(len(info) - 2, 2);
            info.Append(")");
        }

        if (info.IsEmpty()) {
            continue;
        }
        AppendIfNotExists(&fonts, ToStr(info));
    }
    if (len(fonts) == 0) {
        return {};
    }

    SortNatural(&fonts);
    return JoinTemp(&fonts, "\n");
}

// clang-format off
static const Str mupdfPropsMap[] = {
    kPropTitle, Str(FZ_META_INFO_TITLE),
    kPropAuthor, Str(FZ_META_INFO_AUTHOR),
    kPropSubject, StrL("info:Subject"),
    kPropPdfProducer, Str(FZ_META_INFO_PRODUCER),
    kPropCreatorApp, StrL("info:Creator"), // not sure if the same meaning
    kPropCreationDate, StrL("info:CreationDate"),
    kPropModificationDate, StrL("info:ModDate"),
    Str(),
};
// clang-format on

TempStr EngineMupdf::GetPropertyTemp(Str name) {
    auto ctx = Ctx();
    ScopedCritSec ctxScope(&docLock);

    Str key = GetMatchingString(mupdfPropsMap, name);
    if (key) {
        char buf[1024]{};
        int bufSize = (int)dimof(buf);
        int n = fz_lookup_metadata(ctx, _doc, CStrTemp(key), buf, bufSize);
        if (n > 0) {
            if (n > bufSize) {
                // can be bigger if output truncated
                n = bufSize - 1;
                buf[bufSize - 1] = 0; // not sure if necessary
            }
            return str::DupTemp(Str(buf, (int)((size_t)n - 1)));
        }
    }
    if (!pdfdoc) {
        return {};
    }

    if (str::Eq(kPropPdfVersion, name)) {
        int major = pdfdoc->version / 10, minor = pdfdoc->version % 10;
        pdf_crypt* crypt = pdfdoc->crypt;
        if (1 == major && 7 == minor && pdf_crypt_version(ctx, crypt) == 5) {
            if (pdf_crypt_revision(ctx, crypt) == 5) {
                return fmt("%d.%d Adobe Extension Level %d", major, minor, 3);
            }
            if (pdf_crypt_revision(ctx, crypt) == 6) {
                return fmt("%d.%d Adobe Extension Level %d", major, minor, 8);
            }
        }
        return fmt("%d.%d", major, minor);
    }

    if (str::Eq(kPropPdfFileStructure, name)) {
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
                ReportIf(!str::StartsWith(pdf_to_name(ctx, intent), "GTS_"));
                const char* intentName = pdf_to_name(ctx, intent);
                fstruct.Append(Str(intentName + 4));
            }
        }
        if (len(fstruct) == 0) {
            return {};
        }
        return JoinTemp(&fstruct, ",");
    }

    if (str::Eq(kPropUnsupportedFeatures, name)) {
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, pdfInfo, "Unsupported_XFA"))) {
            return "XFA";
        }
        return {};
    }

    if (str::Eq(kPropFontList, name)) {
        return ExtractFontListTemp();
    }

    static const Str pdfPropNames[] = {
        kPropTitle,
        StrL("Title"),
        kPropAuthor,
        StrL("Author"),
        kPropSubject,
        StrL("Subject"),
        kPropCopyright,
        StrL("Copyright"),
        kPropCreationDate,
        StrL("CreationDate"),
        kPropModificationDate,
        StrL("ModDate"),
        kPropCreatorApp,
        StrL("Creator"),
        kPropPdfProducer,
        StrL("Producer"),
        Str(),
    };
    Str pdfPropName = GetMatchingString(pdfPropNames, name);
    if (!pdfPropName) {
        return {};
    }

    // _info is guaranteed not to contain any indirect references,
    // so no need for docLock
    pdf_obj* obj = pdf_dict_gets(ctx, pdfInfo, pdfPropName.s);
    if (!obj) {
        return {};
    }
    TempWStr ws = PdfToWStrTemp(ctx, obj);
    PdfCleanStringInPlace(ws);
    TempStr res = ToUtf8Temp(ws);
    return res;
};

static TempStr LookupMetadataTemp(fz_context* ctx, fz_document* doc, Str key) {
    char buf[1024]{};
    int n = fz_lookup_metadata(ctx, doc, CStrTemp(key), buf, (int)dimof(buf));
    if (n <= 0) {
        return {};
    }
    if (n > (int)dimof(buf)) {
        n = (int)dimof(buf) - 1;
        buf[n] = 0;
    }
    return str::DupTemp(Str(buf, (int)((size_t)n - 1)));
}

static void AppendSigDictText(fz_context* ctx, str::Builder& s, pdf_obj* sigDict, Str label, pdf_obj* key) {
    const char* val = nullptr;
    fz_try(ctx) {
        pdf_obj* obj = pdf_dict_get(ctx, sigDict, key);
        if (obj) {
            val = pdf_to_text_string(ctx, obj);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        val = nullptr;
    }
    if (val && *val) {
        s.Append(fmt("  %s: %s\n", label, Str(val)));
    }
}

static void AppendSigDictDate(fz_context* ctx, str::Builder& s, pdf_obj* sigDict, Str label, pdf_obj* key) {
    int64_t secs = 0;
    fz_try(ctx) {
        pdf_obj* obj = pdf_dict_get(ctx, sigDict, key);
        if (obj) {
            secs = pdf_to_date(ctx, obj);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        secs = 0;
    }
    if (secs <= 0) {
        return;
    }
    time_t t = (time_t)secs;
    struct tm tm;
    gmtime_s(&tm, &t);
    char buf[64];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", &tm);
    s.Append(fmt("  %s: %s\n", label, Str(buf)));
}

static void AppendSignatureInfo(fz_context* ctx, str::Builder& s, pdf_pkcs7_verifier* verifier, pdf_document* pdfdoc,
                                pdf_annot* widget, int sigNo, int pageNo) {
    if (!s.IsEmpty()) {
        s.AppendChar('\n');
    }
    s.Append(fmt("Signature %d (page %d):\n", sigNo, pageNo));
    pdf_obj* sigObj = pdf_annot_obj(ctx, widget);
    if (!pdf_signature_is_signed(ctx, pdfdoc, sigObj)) {
        s.Append("  not signed\n");
        return;
    }

    pdf_pkcs7_distinguished_name* dn = nullptr;
    char* name = nullptr;
    fz_var(dn);
    fz_var(name);
    fz_try(ctx) {
        dn = pdf_signature_get_widget_signatory(ctx, verifier, widget);
        if (dn) {
            name = pdf_signature_format_distinguished_name(ctx, dn);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    s.Append(fmt("  signer: %s\n", Str(name ? name : "(unknown)")));
    fz_free(ctx, name);
    pdf_signature_drop_distinguished_name(ctx, dn);

    // optional metadata the signer put in the /V dictionary (PDF 32000-1
    // §12.8.1). These are plain PDF text strings, so pdf_to_text_string
    // already hands us well-formed UTF-8 -- no mojibake risk.
    pdf_obj* vDict = pdf_dict_get(ctx, sigObj, PDF_NAME(V));
    if (!vDict) {
        vDict = sigObj;
    }
    AppendSigDictDate(ctx, s, vDict, "signing time", PDF_NAME(M));
    AppendSigDictText(ctx, s, vDict, "reason", PDF_NAME(Reason));
    AppendSigDictText(ctx, s, vDict, "location", PDF_NAME(Location));
    AppendSigDictText(ctx, s, vDict, "contact", PDF_NAME(ContactInfo));

    pdf_signature_error certErr = PDF_SIGNATURE_ERROR_UNKNOWN;
    fz_try(ctx) {
        certErr = pdf_check_widget_certificate(ctx, verifier, widget);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    s.Append(fmt("  certificate: %s\n", Str(pdf_signature_error_description(certErr))));

    pdf_signature_error digErr = PDF_SIGNATURE_ERROR_UNKNOWN;
    int edits = 0;
    fz_try(ctx) {
        digErr = pdf_check_widget_digest(ctx, verifier, widget);
        edits = pdf_signature_incremental_change_since_signing(ctx, pdfdoc, sigObj);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    if (digErr) {
        s.Append(fmt("  digest: %s\n", Str(pdf_signature_error_description(digErr))));
    } else if (edits) {
        s.Append("  document edited after signing\n");
    } else {
        s.Append("  document unchanged since signing\n");
    }
}

void EngineMupdf::GetProperties(StrVec& keyValOut) {
    EngineBase::GetProperties(keyValOut);

    auto ctx = Ctx();
    ScopedCritSec ctxScope(&docLock);

    TempStr val = LookupMetadataTemp(ctx, _doc, "info:Keywords");
    if (val) {
        AddProp(keyValOut, kPropKeywords, val);
    }

    val = LookupMetadataTemp(ctx, _doc, "encryption");
    if (val) {
        AddProp(keyValOut, kPropEncryption, val);
    }

    // pdf signatures (signed form widgets). Walks each page's widget set;
    // for each signature widget, pulls signer DN + cert/digest verdict via
    // the Windows CryptoAPI pdf_pkcs7_verifier.
    if (pdfdoc && pdf_count_signatures(ctx, pdfdoc) > 0) {
        str::Builder sigs;
        pdf_pkcs7_verifier* verifier = nullptr;
        pdf_page* page = nullptr;
        fz_var(verifier);
        fz_var(page);
        fz_try(ctx) {
            verifier = pkcs7_windows_new_verifier(ctx);
            int totalPages = pdf_count_pages(ctx, pdfdoc);
            int sigNo = 0;
            for (int pageNo = 0; pageNo < totalPages; pageNo++) {
                page = pdf_load_page(ctx, pdfdoc, pageNo);
                for (pdf_annot* w = pdf_first_widget(ctx, page); w; w = pdf_next_widget(ctx, w)) {
                    if (pdf_widget_type(ctx, w) != PDF_WIDGET_TYPE_SIGNATURE) {
                        continue;
                    }
                    ++sigNo;
                    AppendSignatureInfo(ctx, sigs, verifier, pdfdoc, w, sigNo, pageNo + 1);
                }
                fz_drop_page(ctx, (fz_page*)page);
                page = nullptr;
            }
        }
        fz_always(ctx) {
            fz_drop_page(ctx, (fz_page*)page);
            pdf_drop_verifier(ctx, verifier);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
        if (!sigs.IsEmpty()) {
            AddProp(keyValOut, kPropSignatures, ToStr(sigs));
        }
    }

    // for epub files, list all files in the archive
    Str path = FilePath();
    if (path && str::EndsWithI(path, ".epub")) {
        ArchiveExtractProgressCb emptyCb;
        MultiFormatArchive* zip = OpenArchiveFromFile(path, /*eagerLoad=*/false, emptyCb);
        if (zip) {
            str::Builder filesStr;
            auto& fileInfos = zip->GetFileInfos();
            int n = len(fileInfos);
            for (int i = 0; i < n; i++) {
                auto* fi = fileInfos[i];
                if (str::IsEmpty(fi->name)) {
                    continue;
                }
                filesStr.AppendChar('\n');
                filesStr.Append(fi->name);
            }
            AddProp(keyValOut, kPropFiles, ToStr(filesStr));
            delete zip;
        }
    }
}

Str EngineMupdf::GetFileData() {
    auto ctx = Ctx();

    if (!pdfdoc) {
        return {};
    }

    Str res;
    ScopedCritSec scope(&docLock);

    fz_var(res);
    fz_try(ctx) {
        res = FzExtractStreamData(ctx, pdfdoc->file);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        res = {};
    }

    if (!str::IsEmpty(res)) {
        return res;
    }

    auto path = FilePath();
    if (!path) {
        return {};
    }
    return file::ReadFile(path);
}

bool EngineMupdf::SaveFileAs(Str dstPath) {
    Str d = GetFileData();
    if (!str::IsEmpty(d)) {
        bool ok = file::WriteFile(dstPath, d);
        str::Free(d);
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

bool EngineMupdfIsEncrypted(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return false;
    }
    return epdf->pdfdoc->crypt != nullptr;
}

Str EngineMupdfGetPassword(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf) {
        return nullptr;
    }
    return epdf->pdfPassword;
}

// re-save current pdf document using mupdf (as opposed to just saving the data)
// this is used after the PDF was modified by the user (e.g. by adding / changing
// annotations).
// if filePath is not given, we save under the same name
// TODO: if the file is locked, this might fail.
bool EngineMupdfSaveUpdated(EngineBase* engine, Str path, const ShowErrorCb& showErrorFunc) {
    ReportIf(!engine);
    if (!engine) {
        return false;
    }
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return false;
    }
    if (!EngineMupdfHasUnsavedAnnotations(engine)) {
        return false;
    }

    auto timeStart = TimeGet();
    Str currPath = engine->FilePath();
    if (str::IsEmpty(path)) {
        path = currPath;
    }
    auto ctx = epdf->Ctx();
    ScopedCritSec scope(&epdf->docLock);

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
        pdf_save_document(ctx, epdf->pdfdoc, CStrTemp(path), &save_opts);
        ok = true;
        auto dur = TimeSinceInMs(timeStart);
        logf("Saved annotations to '%s' in  %.2f ms, incremental: %d\n", path, dur, save_opts.do_incremental);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        const char* mupdfErr = fz_caught_message(ctx);
        logf("Saving '%s' failed with: '%s'\n", path, Str(mupdfErr));
        if (showErrorFunc.IsValid()) {
            showErrorFunc.Call(Str(mupdfErr));
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

TempStr EngineMupdf::GetPageLabeTemp(int pageNo) const {
    if (!pageLabels || pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabeTemp(pageNo);
    }

    return pageLabels->At(pageNo - 1);
}

int EngineMupdf::GetPageByLabel(Str label) const {
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
    if (kind == kindFileMarkdown) {
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

EngineBase* CreateEngineMupdfFromFile(Str path, Kind kind, int displayDPI, PasswordUI* pwdUI) {
    if (str::IsEmpty(path)) {
        return nullptr;
    }
    if (kind == kindFileFb2z) {
        AutoDelete archive = OpenArchiveFromFile(path, /*eagerLoad=*/true, gArchiveProgressCb);
        if (!archive) {
            return {};
        }
        auto files = archive->GetFileInfos();
        if (len(files) != 1) {
            return {};
        }
        auto* fi = archive->GetFileDataById(0);
        if (!fi || !fi->data) {
            return {};
        }
        Str d = Str((char*)(fi->data), (int)(fi->fileSizeUncompressed));
        IStream* strm = CreateStreamFromData(d);
        ScopedComPtr<IStream> stream(strm);
        if (!stream) {
            return {};
        }
        EngineMupdf* engine = new EngineMupdf();
        if (displayDPI < 70) {
            displayDPI = 96;
        }
        engine->displayDPI = displayDPI;
        if (!engine->Load(stream, "foo.fb2", pwdUI)) {
            SafeEngineRelease(&engine);
            return {};
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
        SafeEngineRelease(&engine);
        return nullptr;
    }
    TempStr ext = GetExtForKindTemp(kind);
    if (ext) {
        SetDefaultExt(engine->defaultExt, ext);
    }
    return engine;
}

EngineBase* CreateEngineMupdfFromStream(IStream* stream, Str nameHint, PasswordUI* pwdUI) {
    EngineMupdf* engine = new EngineMupdf();
    if (!engine->Load(stream, nameHint, pwdUI)) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

EngineBase* CreateEngineMupdfFromData(Str data, Str nameHint, PasswordUI* pwdUI) {
    EngineMupdf* engine = new EngineMupdf();
    IStream* stream = CreateStreamFromData(data);
    if (!engine->Load(stream, nameHint, pwdUI)) {
        SafeEngineRelease(&engine);
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
    ScopedCritSec scope(&e->pagesLock);
    for (int i = 1; i <= e->pageCount; i++) {
        FzPageInfo* pi = e->GetFzPageInfo(i, false);
        if (!pi) {
            continue;
        }
        annotsOut.Append(pi->annotations);
    }
}

bool EngineMupdfHasUnsavedAnnotations(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return false;
    }
    return epdf->modifiedAnnotations;
}

bool EngineMupdfSupportsAnnotations(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf) {
        return false;
    }
    return (epdf->pdfdoc != nullptr);
}

// caller must free
Str EngineMupdfLoadAttachment(EngineBase* engine, int attachmentNo) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return {};
    }

    Str res = PdfLoadAttachment(epdf->Ctx(), epdf->pdfdoc, attachmentNo);
    return res;
}

Str EngineMupdfLoadAnnotAttachment(EngineBase* engine, int objNum) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return {};
    }
    ScopedCritSec scope(&epdf->docLock);
    return PdfLoadAnnotationAttachment(epdf->Ctx(), epdf->pdfdoc, objNum);
}

// if an elements fully obscures another, remove it from the list
Annotation* EngineMupdfGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, Annotation* preferredAnnot) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return nullptr;
    }
    FzPageInfo* pi = epdf->GetFzPageInfoCanFail(pageNo);
    if (!pi) {
        return nullptr;
    }

    ScopedCritSec cs(&epdf->docLock);
    Vec<Annotation*> els;
    for (auto& annot : pi->annotations) {
        auto& atp = annot->type;
        RectF bounds = annot->bounds;
        if (!bounds.Contains(pos)) {
            continue;
        }
        els.Append(annot);
    }
    if (len(els) == 0) {
        return nullptr;
    }
    for (const auto& a : els) {
        if (a == preferredAnnot) {
            return preferredAnnot;
        }
    }

    // pick the annotation with the smallest rect: if the click lands inside
    // a big highlight that also wraps a smaller annotation, the smaller one
    // is almost always what the user meant
    Annotation* best = els[0];
    RectF br = best->bounds;
    float bestArea = br.dx * br.dy;
    for (int i = 1; i < len(els); i++) {
        RectF r = els[i]->bounds;
        float area = r.dx * r.dy;
        if (area < bestArea) {
            best = els[i];
            bestArea = area;
        }
    }
    return best;
}

// Like EngineMupdfGetAnnotationAtPos but for form fields (widgets), which live
// in their own list. Returns the smallest widget containing pos, or null.
Annotation* EngineMupdfGetWidgetAtPos(EngineBase* engine, int pageNo, PointF pos) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc) {
        return nullptr;
    }
    FzPageInfo* pi = epdf->GetFzPageInfoCanFail(pageNo);
    if (!pi) {
        return nullptr;
    }
    ScopedCritSec cs(&epdf->docLock);
    Annotation* best = nullptr;
    float bestArea = 0;
    for (auto& w : pi->widgets) {
        RectF bounds = w->bounds;
        if (!bounds.Contains(pos)) {
            continue;
        }
        float area = bounds.dx * bounds.dy;
        if (!best || area < bestArea) {
            best = w;
            bestArea = area;
        }
    }
    return best;
}

// Next/previous editable (text/choice, non-read-only) widget on the same page
// as `cur`, wrapping around. Used for Tab / Shift+Tab navigation. Returns null
// if there's no other editable field. Does not hold docLock (calls helpers that
// take it), so it must not be called while docLock is held.
Annotation* EngineMupdfGetAdjacentWidget(EngineBase* engine, Annotation* cur, bool forward) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf->pdfdoc || !cur) {
        return nullptr;
    }
    FzPageInfo* pi = epdf->GetFzPageInfoCanFail(cur->pageNo);
    if (!pi) {
        return nullptr;
    }
    Vec<Annotation*>& ws = pi->widgets;
    int n = len(ws);
    int idx = ws.Find(cur);
    if (n == 0 || idx < 0) {
        return nullptr;
    }
    // read type/flags via mupdf directly (this file is also compiled into
    // PdfPreview/PdfFilter, which don't link Annotation.cpp's GetWidget*)
    auto ctx = epdf->Ctx();
    ScopedCritSec cs(&epdf->docLock);
    for (int step = 1; step <= n; step++) {
        int j = forward ? (idx + step) % n : (idx - step + n) % n;
        Annotation* w = ws[j];
        if (w == cur) {
            break;
        }
        int wt = PDF_WIDGET_TYPE_UNKNOWN;
        int flags = 0;
        fz_try(ctx) {
            wt = pdf_widget_type(ctx, w->pdfannot);
            flags = pdf_annot_field_flags(ctx, w->pdfannot);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
        bool editable =
            (wt == PDF_WIDGET_TYPE_TEXT) || (wt == PDF_WIDGET_TYPE_COMBOBOX) || (wt == PDF_WIDGET_TYPE_LISTBOX);
        if (editable && !(flags & PDF_FIELD_IS_READ_ONLY)) {
            return w;
        }
    }
    return nullptr;
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
    ReportIf(pageNo < 1 || pageNo > e->pageCount);
    int pageIdx = pageNo - 1;

    // EngineMupdf is the ultimate source of truth for Annotation* list
    // all other places only get references to Annotation* created
    // inside EngineMupdf.
    // It would be easier to re-create Annotation* list after each change
    // to annotations inside mupdf but we don't want loose the identity
    // so on add /remove we update the list manually
    // on change we assume Annotation* lives inside EngineMupdf
    ScopedCritSec scope(&e->pagesLock);
    FzPageInfo* pageInfo = e->pages[pageIdx];

    if (change == AnnotationChange::Remove) {
        int sizeBefore = len(pageInfo->annotations);
        int removedPos = pageInfo->annotations.Remove(annot);
        ReportIf(removedPos < 0); // must exist
        int sizeNow = len(pageInfo->annotations);
        ReportIf(sizeBefore != sizeNow + 1);
        ValidateAnnotationsInSync(e, pageInfo);
    } else if (change == AnnotationChange::Add) {
        int sizeBefore = len(pageInfo->annotations);
        int pos = pageInfo->annotations.Find(annot);
        ReportIf(pos >= 0); // shouldn't exist
        pageInfo->annotations.Append(annot);
        int sizeNow = len(pageInfo->annotations);
        ReportIf(sizeBefore != sizeNow - 1);
        ValidateAnnotationsInSync(e, pageInfo);
    } else {
        ReportIf(change != AnnotationChange::Modify);
    }
    {
        auto ctx = e->Ctx();
        ScopedCritSec ctxScope(&e->docLock);
        RebuildCommentsFromAnnotations(ctx, pageInfo);
    }
    pageInfo->elementsNeedRebuilding = true;

    // cached display list captured the old annotations; drop it so the next
    // render rebuilds with the new state.
    {
        auto ctx = e->Ctx();
        ScopedCritSec rl(&e->renderLock);
        if (pageInfo->displayList) {
            fz_drop_display_list(ctx, pageInfo->displayList);
            pageInfo->displayList = nullptr;
        }
    }
}

// creates Annotation wrapper around pdf_annot
Annotation* MakeAnnotationWrapper(EngineMupdf* engine, pdf_annot* annot, int pageNo) {
    ReportIf(pageNo < 1);
    ReportIf(!engine->pdfdoc);
    ScopedCritSec cs(&engine->docLock);

    AnnotationType typ = AnnotationType::Unknown;
    fz_rect bounds;

    fz_context* ctx = engine->Ctx();
    fz_try(ctx) {
        auto tp = pdf_annot_type(ctx, annot);
        bounds = pdf_bound_annot(ctx, annot);
        typ = AnnotationTypeFromPdfAnnot(tp);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
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

extern "C" fz_buffer* pdfinfo_to_buffer(fz_context* ctx, const char* filename);

static void outline_to_buffer_rec(fz_context* ctx, fz_output* out, fz_outline* outline, int level) {
    while (outline) {
        for (int i = 0; i < level; i++) {
            fz_write_byte(ctx, out, '\t');
        }
        fz_write_printf(ctx, out, "%s\t%s\n", outline->title ? outline->title : "", outline->uri ? outline->uri : "");
        if (outline->down) {
            outline_to_buffer_rec(ctx, out, outline->down, level + 1);
        }
        outline = outline->next;
    }
}

TempStr EngineMupdfGetPdfOutline(Str path) {
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!ctx) {
        return {};
    }
    fz_register_document_handlers(ctx);
    TempStr res = nullptr;
    fz_document* doc = nullptr;
    fz_outline* outline = nullptr;
    fz_buffer* buf = nullptr;
    fz_output* out = nullptr;
    fz_try(ctx) {
        doc = fz_open_document(ctx, CStrTemp(path));
        outline = fz_load_outline(ctx, doc);
        if (!outline) {
            res = str::DupTemp("(no outline)");
        } else {
            buf = fz_new_buffer(ctx, 1024);
            out = fz_new_output_with_buffer(ctx, buf);
            outline_to_buffer_rec(ctx, out, outline, 0);
            fz_close_output(ctx, out);
            unsigned char* data;
            size_t n = fz_buffer_storage(ctx, buf, &data);
            res = str::DupTemp(Str((char*)(data), (int)(n)));
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    fz_drop_output(ctx, out);
    fz_drop_buffer(ctx, buf);
    fz_drop_outline(ctx, outline);
    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
    return res;
}

TempStr EngineMupdfGetPdfInfo(Str path) {
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (!ctx) {
        return {};
    }
    fz_register_document_handlers(ctx);
    TempStr res = nullptr;
    fz_buffer* buf = nullptr;
    fz_try(ctx) {
        buf = pdfinfo_to_buffer(ctx, path.s);
        unsigned char* data;
        size_t n = fz_buffer_storage(ctx, buf, &data);
        res = str::DupTemp(Str((char*)(data), (int)(n)));
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    fz_drop_buffer(ctx, buf);
    fz_drop_context(ctx);
    return res;
}
