/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Archive.h"
#if OS_WIN
#include "base/ScopedWin.h"
#endif
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "base/Timer.h"
#include "base/UITask.h"

extern "C" {
#include <mupdf/pdf.h>
#if OS_WIN
#include <mupdf/helpers/pkcs7-windows.h>
#endif
#include "../ext/mupdf/source/fitz/color-imp.h"
}

#include "Annotation.h"
#include "DocProperties.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "PdfCadEnhanceDevice.h"
#include "PdfDarkMode.h"
#include "PdfDarkModeInternal.h"
#include "EngineAll.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "Settings.h"
#include "EngineMupdf.h"

// A5
static float layoutA5DxPt = 420.F;
static float layoutA5DyPt = 595.F;

// A4
static float layoutA4DxPt = 595.F;
static float layoutA4DyPt = 842.F;

static float layoutFontEm = 11.F;

// Shape of the window a reflowable ebook should be laid out for, as
// height / width. A reflow document is laid out once, into a page of a fixed
// size, and Fit Width then only scales that page -- so with the A5 default a
// width-fitted page is ~1.4x taller than any landscape window and has to be
// scrolled through before turning it (issue #3472). Deriving the layout height
// from the window's shape makes one page one screen. 0 keeps the fixed page.
static thread_local float gEbookLayoutAspect = 0.F;
// layout height for ebooks loaded after this call, as a fraction of the layout
// width; 0 restores the fixed A5 page. EBookUI.LayoutDy overrides it
float EngineMupdfSetEbookLayoutAspect(float dyOverDx) {
    if (dyOverDx < 0.05f || dyOverDx > 20.f) {
        dyOverDx = 0.f;
    }
    float prev = gEbookLayoutAspect;
    gEbookLayoutAspect = dyOverDx;
    return prev;
}

// in mupdf_load_system_font.c
#if OS_WIN
extern "C" void install_load_windows_font_funcs(fz_context* ctx);
#endif

static AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp) {
    return (AnnotationType)tp;
}

Kind kindEngineMupdf = "enginePdf";

// Whether to enable mupdf's JavaScript engine for newly loaded PDFs (form-field
// calculate/validate/format). Set by the app from the DisableJavaScript pref;
// PdfPreview/PdfFilter don't link Settings, so they keep the default.
static bool gDisableFormJavaScript = false;
// disable mupdf's JavaScript engine for PDFs loaded after this call
void EngineMupdfSetDisableJavaScript(bool disable) {
    gDisableFormJavaScript = disable;
}

// Whether a PDF may load an image from an external sibling file (issue #3731).
// Set by the app from the AllowExternalImages pref; off by default (and in the
// PdfPreview/PdfFilter DLLs, which don't link Settings).
static bool gAllowExternalImages = false;
// allow PDFs to load images from an external sibling file (#3731), for PDFs
// loaded after this call; set from gSettings->allowExternalImages
void EngineMupdfSetAllowExternalImages(bool allow) {
    gAllowExternalImages = allow;
}

EngineMupdf* AsEngineMupdf(EngineBase* engine) {
    if (!engine || !IsOfKind(engine, kindEngineMupdf)) {
        return nullptr;
    }
    return (EngineMupdf*)engine;
}

bool EngineMupdfHeadingTocPending(EngineBase* engine) {
    EngineMupdf* e = AsEngineMupdf(engine);
    return e && e->HeadingTocPending();
}

void EngineMupdfStartHeadingToc(EngineBase* engine, const Func0& onDone) {
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e) {
        return;
    }
    e->headingTocDoneCb = onDone;
    e->StartHeadingTocIfNeeded();
}

void EngineMupdfCancelHeadingToc(EngineBase* engine) {
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e) {
        return;
    }
    AtomicIntSet(&e->headingTocCancel, 1);
    e->headingTocDoneCb = {};
}

void EngineMupdfCancelLoadAllAnnotations(EngineBase* engine) {
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e) {
        return;
    }
    AtomicIntSet(&e->annotLoadCancel, 1);
    e->annotLoadDoneCb = {};
}

// lets the UI ask without pulling in mupdf headers (declared in EngineAll.h)
Str EngineEbookFontUnavailable(EngineBase* engine) {
    EngineMupdf* e = AsEngineMupdf(engine);
    return e ? e->ebookFontUnavailable : Str{};
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

    // destination on the target page, resolved from the link URI.
    // Valid after hasResolvedCoords; x/y/w/h may be kDestUseDefault when the
    // PDF destination left a coordinate unspecified (null / Fit).
    // IPageDestination::rect stays the source annotation box (issue #5944).
    float destX = 0.f;
    float destY = 0.f;
    float destW = kDestUseDefault;
    float destH = kDestUseDefault;
    bool hasResolvedCoords = false;
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
        // Prefer URI-resolved coords (page-level /Fit and /XYZ nulls become
        // kDestUseDefault). outline->x/y are often 0 and would scroll to the
        // bottom of the page in PDF space. FitR keeps width/height on destW/H;
        // `rect` is the source annotation and must not be used as the dest
        // (issue #5944).
        if (hasResolvedCoords) {
            return RectF{destX, destY, destW, destH};
        }
        if (outline) {
            RectF r{outline->x, outline->y, 0, 0};
            return r;
        }
        return rect;
    }

    RectF GetDestPoint2() override {
        if (hasResolvedCoords) {
            return RectF{destX, destY, 0, 0};
        }
        if (outline) {
            return RectF{outline->x, outline->y, 0, 0};
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
        value = str::Dup(url::DecodeTemp(uri));
    }
    return value;
}

Str PageDestinationMupdf::GetName2() {
    if (name) {
        return name;
    }
    if (outline && outline->title) {
        name = str::Dup(Str(outline->title));
    }
    return name;
}

static NO_INLINE RectF FzGetRectF(fz_link* link) {
    if (link) {
        return ToRectF(link->rect);
    }
    return {};
}

// Map a MuPDF/Adobe link destination to Sumatra rect + zoom for ScrollTo.
// zoomOut: 0 = leave zoom; >0 absolute fraction (1 = 100%); negative =
// virtual modes (kZoomFitPage / FitWidth / FitContent). (issue #5828)
static void DestFromFzLinkDest(const fz_link_dest& ldest, RectF* rectOut, float* zoomOut) {
    float x = isnan(ldest.x) ? kDestUseDefault : ldest.x;
    float y = isnan(ldest.y) ? kDestUseDefault : ldest.y;
    float w = isnan(ldest.w) ? kDestUseDefault : ldest.w;
    float h = isnan(ldest.h) ? kDestUseDefault : ldest.h;
    float zoom = 0.f;

    switch (ldest.type) {
        case FZ_LINK_DEST_XYZ:
            w = h = kDestUseDefault;
            // mupdf reports zoom as percentage (100 = 100%); we use 1.0 as 100%.
            if (!isnan(ldest.zoom) && ldest.zoom > 0) {
                zoom = ldest.zoom / 100.f;
            }
            break;
        case FZ_LINK_DEST_FIT:
            zoom = kZoomFitPage;
            x = y = w = h = kDestUseDefault;
            break;
        case FZ_LINK_DEST_FIT_H:
            // Fit page width; optional top (y)
            zoom = kZoomFitWidth;
            x = w = h = kDestUseDefault;
            break;
        case FZ_LINK_DEST_FIT_V:
            // Fit page height (no dedicated mode → Fit Page); optional left (x)
            zoom = kZoomFitPage;
            y = w = h = kDestUseDefault;
            break;
        case FZ_LINK_DEST_FIT_B:
            zoom = kZoomFitContent;
            x = y = w = h = kDestUseDefault;
            break;
        case FZ_LINK_DEST_FIT_BH:
            // Fit content width; optional top (y)
            zoom = kZoomFitContent;
            x = w = h = kDestUseDefault;
            break;
        case FZ_LINK_DEST_FIT_BV:
            // Fit content height; optional left (x)
            zoom = kZoomFitContent;
            y = w = h = kDestUseDefault;
            break;
        case FZ_LINK_DEST_FIT_R:
            // rectangle in x,y,w,h — scroll/zoom handled by ScrollTo FitR path
            break;
        default:
            w = h = kDestUseDefault;
            break;
    }
    if (rectOut) {
        *rectOut = RectF{x, y, w, h};
    }
    if (zoomOut) {
        *zoomOut = zoom;
    }
}

static int ResolveLink(fz_context* ctx, fz_document* doc, Str uri, float* xp, float* yp, float* zoomp = nullptr,
                       RectF* rectp = nullptr) {
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
    // Match HandleLinkMupdf: unspecified PDF coords are NaN and must stay
    // kDestUseDefault so ScrollTo lands on the page top, not user-space (0,0)
    // (bottom of the page in PDF coords) which can make continuous view report
    // the next page as current (#2799 / page-level outline destinations).
    RectF rect;
    float zoom = 0.f;
    DestFromFzLinkDest(ldest, &rect, &zoom);
    if (xp) {
        *xp = rect.x;
    }
    if (yp) {
        *yp = rect.y;
    }
    if (zoomp) {
        *zoomp = zoom;
    }
    if (rectp) {
        *rectp = rect;
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
    while (len(path) > 0 && (path.s[0] == '/' || path.s[0] == '\\')) {
        path.s++;
        path.len--;
    }
}

static bool IsMupdfLocalFileLink(Str uri, TempStr* pathOut, Str* fragmentOut) {
    if (!uri || uri.s[0] == '#') {
        return false;
    }
    if (str::StartsWith(uri, StrL("file:")) || IsExternalUrl(uri) || IsExternalLink(uri)) {
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

    FileType kind = GuessFileTypeFromName(path);
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

    if (str::TrimPrefix(maybePath, StrL("file:"))) {
        // decode: file:path%20to_file.pdf#page=1

        // this is to handle file:// and
        // file:/// (which I assume is a mistake in PDF)
        str::TrimPrefix(maybePath, StrL("/"));
        str::TrimPrefix(maybePath, StrL("/"));
        str::TrimPrefix(maybePath, StrL("/"));

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

        path = path::ToOSTemp(pathNul);
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
        auto* res = new PageDestinationFile(path, destStr);
        res->rect = FzGetRectF(link);
        return res;
    }

    if (IsExternalUrl(uri)) {
        auto* res = new PageDestinationURL(uri);
        res->rect = FzGetRectF(link);
        return res;
    }

    // Try to resolve the URI as an internal document location first. EPUB
    // chapter links (e.g. "OEBPS/ch1.htm") point inside the same document and
    // must navigate internally, not launch an external file. Only when the URI
    // doesn't resolve internally do we treat a relative href to a supported file
    // as a sibling file to launch (e.g. markdown "[other](other.md)").
    float x = 0, y = 0, z = 0;
    RectF destRect{};
    int pageNo = ResolveLink(ctx, doc, uri, &x, &y, &z, &destRect);

    if (pageNo <= 0) {
        TempStr localPath;
        Str localFragment;
        if (IsMupdfLocalFileLink(uri, &localPath, &localFragment)) {
            auto* res = new PageDestinationFile(localPath, localFragment);
            res->rect = FzGetRectF(link);
            return res;
        }
    }

    auto* dest = new PageDestinationMupdf(link, outline);
    dest->rect = FzGetRectF(link);
    dest->pageNo = pageNo;
    if (pageNo > 0) {
        dest->destX = destRect.x;
        dest->destY = destRect.y;
        dest->destW = destRect.dx;
        dest->destH = destRect.dy;
        dest->destZoom = z;
        dest->hasResolvedCoords = true;
    }
    return dest;
}

// A link annotation can carry a human-readable description of where it goes in
// its /Contents, which is what a viewer has to show for a link that has no URL
// to show instead. mupdf's fz_link doesn't hand out the annotation object it
// came from, so find it in the page's /Annots by rect (issue #1724).
static Str PdfLinkContents(fz_context* ctx, pdf_document* pdfdoc, pdf_page* pdfpage, int pageNo, fz_rect linkRect) {
    if (!pdfdoc || !pdfpage) {
        return {};
    }
    Str res;
    fz_try(ctx) {
        fz_rect mediabox;
        fz_matrix ctm;
        pdf_page_transform(ctx, pdfpage, &mediabox, &ctm);
        pdf_obj* pageObj = pdf_lookup_page_obj(ctx, pdfdoc, pageNo - 1);
        pdf_obj* annots = pdf_dict_get(ctx, pageObj, PDF_NAME(Annots));
        int n = pdf_array_len(ctx, annots);
        for (int i = 0; i < n; i++) {
            pdf_obj* annot = pdf_array_get(ctx, annots, i);
            pdf_obj* subtype = pdf_dict_get(ctx, annot, PDF_NAME(Subtype));
            if (!pdf_name_eq(ctx, subtype, PDF_NAME(Link))) {
                continue;
            }
            // /Rect is in page space, fz_link::rect has the page's transform
            // applied, so compare in the same space
            fz_rect r = pdf_dict_get_rect(ctx, annot, PDF_NAME(Rect));
            r = fz_transform_rect(r, ctm);
            constexpr float kMaxDiff = 1.f;
            bool same = fabsf(r.x0 - linkRect.x0) < kMaxDiff && fabsf(r.y0 - linkRect.y0) < kMaxDiff &&
                        fabsf(r.x1 - linkRect.x1) < kMaxDiff && fabsf(r.y1 - linkRect.y1) < kMaxDiff;
            if (!same) {
                continue;
            }
            const char* s = pdf_dict_get_text_string(ctx, annot, PDF_NAME(Contents));
            if (s && *s) {
                res = str::Dup(Str(s));
            }
            break;
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return res;
}

static PageElementDestination* NewLinkDestination(int srcPageNo, fz_context* ctx, fz_document* doc, fz_link* link,
                                                  fz_outline* outline) {
    auto* dest = NewPageDestinationMupdf(ctx, doc, link, outline);
    if (!dest) {
        return nullptr;
    }
    auto* res = new PageElementDestination(dest);
    res->pageNo = srcPageNo;
    res->rect = dest->rect;
    return res;
}

struct LinkRectList {
    StrVec links;
    Vec<fz_rect> coords;
};

fz_rect ToFzRect(RectF rect) {
    fz_rect result = {rect.x, rect.y, rect.x + rect.dx, rect.y + rect.dy};
    return result;
}

RectF ToRectF(fz_rect rect) {
    return RectF::FromXY(rect.x0, rect.y0, rect.x1, rect.y1);
}

static bool IsPointInRect(fz_rect rect, fz_point pt) {
    return ToRectF(rect).Contains(PointF(pt.x, pt.y));
}

static fz_matrix FzCreateViewCtm(fz_rect mediabox, float zoom, int rotation) {
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
    TempWStr res = ToWStrTemp(Str(s));
    fz_free(ctx, s);
    return res;
}

static TempStr PdfToUtf8Temp(fz_context* ctx, pdf_obj* obj) {
    char* s = pdf_new_utf8_from_pdf_string_obj(ctx, obj);
    TempStr res = str::DupTemp(Str(s));
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
    // by libsumatrapdf so that it works across dll boundaries.
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
constexpr i64 kMaxMemoryFileSize = 32LL * 1024 * 1024;

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
    if (len(d) == 0) {
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
    if (len(d) == 0) {
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
    fz_stream* stm = nullptr;
    // OneNote/Outlook cache files: always load fully so we drop the original
    // handle even when the copy-on-open path could not run (issue #4705).
    if (path::IsEphemeralHostFile(path)) {
        Str d = file::ReadFile(path);
        if (len(d) > 0) {
            stm = FzStreamFromData(ctx, (u8*)d.s, len(d));
        }
        str::Free(d);
        if (stm) {
            return stm;
        }
    } else {
        stm = FzReadFileIfSmall(ctx, path);
        if (stm) {
            return stm;
        }
    }
#if OS_WIN
    WCHAR* pathW = CWStrTemp(path);
    fz_try(ctx) {
        stm = fz_open_file_w(ctx, pathW);
    }
#else
    char* pathZ = CStrTemp(path);
    fz_try(ctx) {
        stm = fz_open_file(ctx, pathZ);
    }
#endif
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
    fz_free(ctx, data);
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
    RectF r;
};

static bool HasSeenGlyph(const Vec<SeenGlyph>& seen, int rune, const RectF& r) {
    // A "duplicate" glyph is one drawn on top of an earlier one (e.g. faux-bold
    // double-strike or an overprinted shadow); its box overlaps the earlier one
    // almost entirely. Two *adjacent* identical letters (e.g. the "ll" in
    // "Yellow") sit side by side and barely overlap, so they must NOT be treated
    // as duplicates. Comparing integer-rounded boxes can't tell them apart
    // once the glyph is ~1px wide (small CAD net names, issue #5968):
    // RectF::Round() expands outward, so "II" / "22" share most of a 1–2px
    // box and the second letter was dropped. Compare the float boxes instead
    // (#5766 still holds: adjacent "ll" barely overlap in float space).
    float area = r.dx * r.dy;
    if (area <= 0) {
        return false;
    }
    for (const SeenGlyph& glyph : seen) {
        if (glyph.rune != rune) {
            continue;
        }
        RectF inter = glyph.r.Intersect(r);
        if (inter.IsEmpty()) {
            continue;
        }
        float interArea = inter.dx * inter.dy;
        float seenArea = glyph.r.dx * glyph.r.dy;
        float minArea = std::min(area, seenArea);
        if (minArea > 0 && interArea * 2 > minArea) {
            return true;
        }
    }
    return false;
}

static void AddSeenGlyph(Vec<SeenGlyph>& seen, int rune, const RectF& r) {
    VecAppend(seen, {rune, r});
}

// True Unicode scalar (not surrogate, not out of range). fz_runetochar will still
// encode surrogates as 3-byte sequences, but those are illegal UTF-8; Utf8CodepointCount
// then counts each byte as its own codepoint while we only append one rect, tripping
// ReportIf in FzTextPageToUtf8 (debug report 8bdec9f53000001, PIC datasheet PDF).
static bool IsUnicodeScalar(int rune) {
    unsigned int c = (unsigned int)rune;
    if (c > 0x10FFFF) {
        return false;
    }
    // UTF-16 surrogates are not valid Unicode scalar values
    if (c >= 0xD800 && c <= 0xDFFF) {
        return false;
    }
    return true;
}

// True if this space is only tracking/justification between syllables, not a
// real word break. PDFs that place space operators (or MuPDF synthetic spaces)
// between every syllable produce "Kro nik, im mün" on copy (#5627).
//
// Measure from the space's own origin to the next letter, not from the previous
// letter's font-box right edge. Without FZ_STEXT_ACCURATE_BBOXES the quad is
// font-height, so italic shear of the full ascender can push prev.quad.x1 past
// the next origin and a real ~0.25em word space looks negative (bug-5627
// "Helicobacter pylori" after matching 3.6.1 selection boxes).
static bool IsTrackingSpace(const fz_stext_char* space, const fz_stext_char* nextNonSpace) {
    if (!space || !nextNonSpace) {
        return false;
    }
    float gap = nextNonSpace->origin.x - space->origin.x;
    float size = space->size;
    if (size <= 0) {
        size = nextNonSpace->size;
    }
    if (size <= 0) {
        size = 1.f;
    }
    // Measured advance/size across tracking (#5627), ordinary body text, and
    // condensed headers (#5871 CompTIA / MyriadPro-BoldCond):
    //   tracking syllables: ~0.00-0.02em (often near zero)
    //   condensed real word spaces: ~0.16em
    //   body / TJ-synthesized word spaces: ~0.20-0.30em (#5868)
    // 0.1em sits between tracking and the tightest real word spaces. A higher
    // threshold (0.2em) correctly kills #5627 tracking but also deletes spaces
    // in condensed titles ("CompTIAA+Certification…") — #5871.
    //
    // Don't treat synthetic (MuPDF-inserted) spaces as more suspicious than
    // real ones: a PDF that positions words with TJ offsets instead of space
    // glyphs -- groff/troff output, for one -- gets *every* word space
    // synthesized, at a perfectly normal 0.25-0.3em gap (#5868).
    return gap < size * 0.1f;
}

static void AddCharUtf8(fz_stext_line* /*line*/, fz_stext_char* c, str::Builder& s, Vec<Rect>& rects,
                        Vec<SeenGlyph>& seen) {
    fz_rect bbox = fz_rect_from_quad(c->quad);
    RectF rf = ToRectF(bbox);
    Rect r = rf.Round();
    int rune = c->c;
    if (HasSeenGlyph(seen, rune, rf)) {
        return;
    }

    bool isWhitespace = rune > 0 && rune <= 0x7f && str::IsWs((char)rune);
    bool isNonPrintable = rune <= 32 || (rune <= 0xffff && wstr::IsNonCharacter((WCHAR)rune));
    // Invalid scalars (surrogates / out of range) must not go through fz_runetochar:
    // that produces illegal UTF-8 that Utf8CodepointCount splits into multiple units.
    if (!IsUnicodeScalar(rune) || (isNonPrintable && !isWhitespace)) {
        s.AppendChar('?');
        VecAppend(rects, r);
        AddSeenGlyph(seen, rune, rf);
        return;
    }
    if (isWhitespace) {
        // collapse multiple whitespace characters into one
        char prev = len(s) == 0 ? 0 : s.LastChar();
        if (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r') {
            return;
        }
        s.AppendChar(' ');
        VecAppend(rects, r);
        AddSeenGlyph(seen, rune, rf);
        return;
    }
    char buf[4];
    int n = fz_runetochar(buf, rune);
    // One Unicode scalar → one UTF-8 sequence → one rect (codepoint-aligned coords)
    if (n <= 0 || !s.Append(Str(buf, n))) {
        return;
    }
    VecAppend(rects, r);
    AddSeenGlyph(seen, rune, rf);
}

static void AddLineSepUtf8(str::Builder& s, Vec<Rect>& rects, Str lineSep) {
    size_t lineSepLen = (size_t)lineSep.len;
    if (lineSepLen == 0) {
        return;
    }
    // remove trailing space
    if (len(s) > 0 && s.LastChar() == ' ') {
        s.RemoveLast();
        VecRemoveLast(rects);
    }
    s.Append(lineSep);
    for (size_t i = 0; i < lineSepLen; i++) {
        VecAppend(rects, Rect());
    }
}

// Prefer font size over tight glyph bboxes (FZ_STEXT_ACCURATE_BBOXES makes
// line->bbox height jump between lines with/without descenders).
static float StextLineHeight(const fz_stext_line* line) {
    if (line->first_char && line->first_char->size > 0.5f) {
        return line->first_char->size;
    }
    float h = line->bbox.y1 - line->bbox.y0;
    if (h > 0.5f) {
        return h;
    }
    return 10.f;
}

// True when nextLine is a soft wrap of the same paragraph (reflow/ebook copy
// should join with a space, not a newline — #5793).
// FB2/HTML reflow lines typically share a ~0.25–0.35em gap; paragraph spacing
// is larger (~0.5em+). Mid-line soft wraps also nearly fill the block width.
static bool IsSoftLineBreak(const fz_stext_line* line, const fz_stext_line* nextLine, const fz_stext_block* block) {
    if (!line || !nextLine || !block) {
        return false;
    }
    float h = StextLineHeight(line);
    float gap = nextLine->bbox.y0 - line->bbox.y1;
    // Same-paragraph wraps sit close together; larger gaps are paragraph
    // spacing. Negative gap = overlapping/tight lines still soft.
    // Threshold ~0.55h: observed soft gaps ~2.3–2.7 on 8–10pt body text,
    // paragraph gaps ~4+ (see FB2 #5793 samples).
    if (gap > h * 0.55f) {
        return false;
    }
    // Soft wraps usually fill most of the block width; a short line is the
    // end of a paragraph (or a title), so keep a hard newline after it.
    float blockW = block->bbox.x1 - block->bbox.x0;
    float lineW = line->bbox.x1 - line->bbox.x0;
    if (blockW > 1.f && lineW < blockW * 0.82f) {
        return false;
    }
    // New paragraphs often start with a larger left indent (text-indent).
    float dx = nextLine->bbox.x0 - line->bbox.x0;
    if (dx > h * 0.4f) {
        return false;
    }
    return true;
}

// Unicode hyphens MuPDF treats as dehyphenation candidates (fz_is_unicode_hyphen).
static bool IsUnicodeHyphenRune(int c) {
    return c == '-' || c == 0xAD || c == 0x2010 || c == 0x2011;
}

// Drop a trailing hyphen used for line wrapping before joining the next line
// so "some-\\nthing" becomes "something" (#5793, #1189). Handles multi-byte
// UTF-8 hyphens (U+00AD soft hyphen, U+2010/U+2011), not just ASCII '-'.
static void MaybeDropTrailingSoftHyphen(str::Builder& s, Vec<Rect>& rects) {
    if (len(s) == 0 || len(rects) == 0) {
        return;
    }
    Str text = ToStr(s);
    int byteIdx = text.len;
    int c = Utf8CodepointPrev(text, byteIdx);
    if (!IsUnicodeHyphenRune(c)) {
        return;
    }
    // Truncate builder to before the last codepoint.
    int dropBytes = text.len - byteIdx;
    while (dropBytes-- > 0) {
        s.RemoveLast();
    }
    VecRemoveLast(rects);
}

static void AppendStextTextBlock(const fz_stext_block* block, str::Builder& content, Vec<Rect>& rects,
                                 Vec<SeenGlyph>& seen, Str hardLineSep, Str softLineSep) {
    fz_stext_line* line = block->u.t.first_line;
    while (line) {
        // Walk each line so tracking spaces can be dropped (issue #5627:
        // Turkish PDFs that space every syllable).
        fz_stext_char* c = line->first_char;
        while (c) {
            int rune = c->c;
            // Soft hyphens (U+00AD) are never typed by the user; drop them
            // so search for "softhyphen" matches text that contains SHY
            // mid-word (issue #1189). End-of-line hard hyphens are dropped
            // when joining via MaybeDropTrailingSoftHyphen below.
            if (rune == 0xAD) {
                c = c->next;
                continue;
            }
            bool isWs = rune > 0 && rune <= 0x7f && str::IsWs((char)rune);
            if (isWs) {
                fz_stext_char* nextNonSpace = c->next;
                while (nextNonSpace) {
                    int nr = nextNonSpace->c;
                    bool nWs = nr > 0 && nr <= 0x7f && str::IsWs((char)nr);
                    if (!nWs) {
                        break;
                    }
                    nextNonSpace = nextNonSpace->next;
                }
                if (IsTrackingSpace(c, nextNonSpace)) {
                    c = c->next;
                    continue;
                }
            }
            AddCharUtf8(line, c, content, rects, seen);
            c = c->next;
        }
        // Soft-join reflow lines within a paragraph for better copy (#5793);
        // keep a hard newline between paragraphs / blocks.
        // Dehyphenation (#1189): when MuPDF marked the line JOINED (EOL
        // hyphen + DEHYPHENATE), or we soft-break after a trailing hyphen,
        // drop the hyphen and join with no separator so "hyphen-\\nated"
        // becomes searchable as "hyphenated". Plain soft wraps still get a
        // space.
        fz_stext_line* nextLine = line->next;
        bool dehyphen = nextLine && (line->flags & FZ_STEXT_LINE_FLAGS_JOINED) != 0;
        bool soft = nextLine && IsSoftLineBreak(line, nextLine, block);
        if (dehyphen || soft) {
            bool hadHyphen = false;
            {
                Str cur = ToStr(content);
                if (cur && cur.len > 0) {
                    int bi = cur.len;
                    int last = Utf8CodepointPrev(cur, bi);
                    hadHyphen = IsUnicodeHyphenRune(last);
                }
            }
            if (dehyphen || hadHyphen) {
                MaybeDropTrailingSoftHyphen(content, rects);
                // no separator: dehyphenated word continues on next line
            } else {
                AddLineSepUtf8(content, rects, softLineSep);
            }
        } else {
            AddLineSepUtf8(content, rects, hardLineSep);
        }
        // each line has independent glyph positions; reset duplicate detection
        VecReset(seen);
        line = line->next;
    }
}

static void AppendStextBlocks(fz_stext_block* block, str::Builder& content, Vec<Rect>& rects, Vec<SeenGlyph>& seen,
                              Str hardLineSep, Str softLineSep) {
    while (block) {
        if (block->type == FZ_STEXT_BLOCK_TEXT) {
            AppendStextTextBlock(block, content, rects, seen, hardLineSep, softLineSep);
        } else if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down) {
            // Tagged-PDF structure nodes; MuPDF's do_as_text walks these too.
            // They only appear when FZ_STEXT_COLLECT_STRUCTURE is set (#4859).
            AppendStextBlocks(block->u.s.down->first_block, content, rects, seen, hardLineSep, softLineSep);
        }
        block = block->next;
    }
}

static Str FzTextPageToUtf8(fz_stext_page* text, Rect** coordsOut) {
    Str hardLineSep = StrL("\n");
    Str softLineSep = StrL(" ");
    str::Builder content;
    Vec<Rect> rects;
    Vec<SeenGlyph> seen;

    AppendStextBlocks(text->first_block, content, rects, seen, hardLineSep, softLineSep);

    ReportIf(Utf8CodepointCount(ToStr(content)) != len(rects));

    if (coordsOut) {
        if (len(rects) > 0) {
            *coordsOut = VecTake(rects);
        } else {
            *coordsOut = nullptr;
        }
    }
    return content.TakeStr();
}

static fz_stext_options NewTextPageOptions(int flags = 0) {
    fz_stext_options opts{};
    // Font ascender/descender boxes, like 3.6.1: FZ_STEXT_ACCURATE_BBOXES uses
    // tight glyph ink, so a word such as "compass" (no d/h/l) has almost no
    // padding above the letters. DEHYPHENATE: mark end-of-line hyphens as
    // soft joins so FzTextPageToUtf8 can drop them and stitch
    // "hyphen-\\nated" into "hyphenated" for search (issue #1189).
    opts.flags = flags | FZ_STEXT_DEHYPHENATE;
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

// True when the glyph at posOff is a newline that continues a URL started at
// startOff. Wrapped URLs end in a non-alphanumeric, resume slightly below and
// to the left of the previous line's end, and do not start a new column
// (issue #2239: a two-column table glued "Languagehat" onto archive.org/).
static bool LinkifyCheckMultiline(Utf8PageText pageText, int startOff, int posOff, Rect* coords) {
    int pageLen = pageText.len;
    if (startOff < 0 || startOff >= posOff || posOff <= 0 || posOff >= pageLen || (posOff + 1) >= pageLen) {
        return false;
    }
    if ('\n' != RuneAt(pageText, posOff)) {
        return false;
    }
    if (IsAlphaNumRune(RuneAt(pageText, posOff - 1)) || IsWhitespaceRune(RuneAt(pageText, posOff + 1))) {
        return false;
    }
    if (StartsWithAscii(pageText, posOff + 1, "http")) {
        return false;
    }
    Rect next = coords[posOff + 1];
    Rect last = coords[posOff - 1];
    Rect first = coords[startOff];
    // stext glyph boxes use page space with y growing down.
    if (next.BR().y <= last.y) {
        return false;
    }
    if (next.y > last.BR().y + last.dy * 1.5f) {
        return false;
    }
    if (next.x >= last.BR().x) {
        return false;
    }
    // Continuation stays near the URL's left edge. The next row of a
    // left-hand column starts much further left than that.
    float slack = last.dy * 1.5f;
    if (first.dx > 0) {
        slack = std::max(slack, (float)first.dx * 3);
    }
    if (next.x < first.x - slack) {
        return false;
    }
    if (next.dy < last.dy * 0.85f || next.dy > last.dy * 1.2f) {
        return false;
    }
    return true;
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
    TempStr uri = list->links[lastIx];
    int endOff = nextOff;
    bool multiline = false;

    do {
        int prevChar = startOff > 0 ? RuneAt(pageText, startOff - 1) : ' ';
        endOff = LinkifyFindEndOff(nextOff, prevChar, pageText);
        multiline = LinkifyCheckMultiline(pageText, startOff, endOff, coords);

        Str part = SliceByRuneOff(pageText, nextOff, endOff);
        uri = str::JoinTemp(uri, part);
        Rect bbox = coords[nextOff].Union(coords[endOff - 1]);
        VecAppend(list->coords, ToFzRect(ToRectF(bbox)));

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
static inline bool IsEmailUsernameChar(int c) {
    // explicitly excluding the '/' from the list, as it is more
    // often part of a URL or path than of an email address
    return IsAlphaNumRune(c) || ContainsAsciiChar(StrL(".!#$%&'*+=?^_`{|}~-"), c);
}
static inline bool IsEmailDomainChar(int c) {
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
            multiline = LinkifyCheckMultiline(pageText, startOff, endOff, coords);
        } else if ('w' == startChar && StartsWithAscii(pageText, startOff, "www.")) {
            int prevChar = startOff > 0 ? RuneAt(pageText, startOff - 1) : ' ';
            endOff = LinkifyFindEndOff(startOff, prevChar, pageText);
            multiline = LinkifyCheckMultiline(pageText, startOff, endOff, coords);
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
        VecAppend(list->coords, ToFzRect(ToRectF(bbox)));
        if (multiline) {
            endOff = LinkifyMultilineText(list, pageText, startOff, endOff + 1, coords);
        }

        startOff = endOff;
    }

    return list;
}

// try to produce an 8-bit palette for saving some memory
#if OS_WIN
static RenderedBitmap* TryRenderAsPaletteImage(fz_pixmap* pixmap) {
    int w = pixmap->w;
    int h = pixmap->h;
    int stride = ((w + 3) / 4) * 4;

    size_t sz = sizeof(BITMAPINFO) + (255 * sizeof(RGBQUAD));
    auto* bmi = (BITMAPINFO*)AllocArrayTemp<u8>((int)sz);
    if (!bmi) {
        return nullptr;
    }
    BITMAPINFOHEADER* bmih = &bmi->bmiHeader;
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

    u32* palette = (u32*)bmi->bmiColors;

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
#endif

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

#if OS_WIN
// preserveAlpha: palettizing drops the alpha channel, so skip it when the
// caller needs transparent holes to composite over the canvas (issue #1809).
static RenderedBitmap* NewRenderedFzPixmap(fz_context* ctx, fz_pixmap* pixmap, bool preserveAlpha = false) {
    if (!pixmap) {
        return nullptr;
    }
    if (!preserveAlpha && pixmap->n == 4 && fz_colorspace_is_rgb(ctx, pixmap->colorspace)) {
        RenderedBitmap* res = TryRenderAsPaletteImage(pixmap);
        if (res) {
            return res;
        }
    }

    auto* bmi = (BITMAPINFO*)AllocArrayTemp<u8>(sizeofi(BITMAPINFO) + (255 * sizeofi(RGBQUAD)));

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
        if (bgrPixmap) {
            fz_drop_pixmap(ctx, bgrPixmap);
        }
        return nullptr;
    }

    int w = bgrPixmap->w;
    int h = bgrPixmap->h;
    int n = bgrPixmap->n;
    int imgSize = (int)bgrPixmap->stride * h;
    int bitsCount = n * 8;

    BITMAPINFOHEADER* bmih = &bmi->bmiHeader;
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
#endif

static Pixmap* NewPixmapFromFzPixmap(fz_context* ctx, fz_pixmap* pixmap, bool preserveAlpha = false) {
#if OS_WIN
    return PixmapFromRenderedBitmap(NewRenderedFzPixmap(ctx, pixmap, preserveAlpha));
#else
    fz_pixmap* bgrPixmap = nullptr;
    fz_var(bgrPixmap);

    fz_try(ctx) {
        bgrPixmap = FzConvertPixmap2(ctx, pixmap, fz_device_bgr(ctx), nullptr, nullptr, fz_default_color_params, 1);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return nullptr;
    }
    if (!bgrPixmap || !bgrPixmap->samples) {
        if (bgrPixmap) {
            fz_drop_pixmap(ctx, bgrPixmap);
        }
        return nullptr;
    }

    Pixmap* res = AllocPixmap(bgrPixmap->w, bgrPixmap->h, PixmapFormat::BGRA8, false);
    if (res) {
        res->xres = (float)bgrPixmap->xres;
        res->yres = (float)bgrPixmap->yres;
        u8* dst = res->data;
        u8* src = bgrPixmap->samples;
        size_t rowBytes = (size_t)bgrPixmap->w * 4;
        for (int y = 0; y < bgrPixmap->h; y++) {
            memcpy(dst, src, rowBytes);
            dst += res->stride;
            src += bgrPixmap->stride;
        }
    }
    fz_drop_pixmap(ctx, bgrPixmap);
    return res;
#endif
}

static TocItem* NewTocItemWithDestination(TocItem* parent, Str title, IPageDestination* dest) {
    auto* res = AllocTocItem(nullptr, title, 0);
    res->parent = parent;
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
                // logf("el %d fully obscures %d\n", i, j);
                VecRemoveAtFast(els, i);
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

    for (auto* pel : pageInfo->links) {
        if (pel->GetRect().Contains(pt)) {
            VecAppend(res, pel);
        }
    }

    for (auto* pel : pageInfo->autoLinks) {
        if (pel->GetRect().Contains(pt)) {
            VecAppend(res, pel);
        }
    }

    for (auto* pel : pageInfo->comments) {
        if (pel->GetRect().Contains(pt)) {
            VecAppend(res, pel);
        }
    }

    fz_point p = {pt.x, pt.y};
    for (auto& img : pageInfo->images) {
        fz_rect ir = img->rect;
        if (IsPointInRect(ir, p)) {
            VecAppend(res, img->imageElement);
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
    VecClear(els);
    VecReserve(els, total);

    // since all elements lists are in last-to-first order, append
    // item types in inverse order and reverse the whole list at the end
    for (auto& img : pageInfo->images) {
        VecAppend(els, img->imageElement);
    }
    for (auto& pel : pageInfo->links) {
        VecAppend(els, pel);
    }
    for (auto& pel : pageInfo->autoLinks) {
        VecAppend(els, pel);
    }
    for (auto& comment : pageInfo->comments) {
        VecAppend(els, comment);
    }
    VecReverse(els);
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
        fz_rect bbox = list->coords[i];
        bool overlaps = false;
        for (auto* pel : pageInfo->links) {
            if (FzRectOverlap(bbox, pel->GetRect()) >= 0.25f) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            continue;
        }

        TempStr uri = list->links[i];
        if (!uri) {
            continue;
        }

        // TODO: those leak on xps
        auto* dest = new PageDestinationURL(uri);
        auto* pel = new PageElementDestination(dest);
        pel->rect = ToRectF(bbox);
        VecAppend(pageInfo->autoLinks, pel);
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
            img->image = fz_keep_image(ctx, image);
            auto* pel = new PageElementImage();
            pel->pageNo = pageNo;
            pel->rect = ToRectF(block->bbox);
            pel->imageID = len(images);
            img->imageElement = pel;
            VecAppend(images, img);
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

// --- dark-mode image preservation helpers (ported from dengxibo/sumatrapdf-plus) ---

static void FzAppendPageImageRect(fz_context* ctx, Vec<FitzPageImageInfo*>& images, int pageNo, fz_rect bbox,
                                  fz_image* image) {
    if (fz_is_empty_rect(bbox) || fz_is_infinite_rect(bbox)) {
        return;
    }
    RectF rf = ToRectF(bbox);
    if (rf.IsEmpty()) {
        return;
    }
    for (FitzPageImageInfo* existing : images) {
        if (FzRectOverlap(existing->rect, rf) > 0.85f) {
            if (ctx && image && !existing->image) {
                existing->image = fz_keep_image(ctx, image);
            }
            return;
        }
    }
    FitzPageImageInfo* img = new FitzPageImageInfo{bbox, fz_identity};
    if (ctx && image) {
        img->image = fz_keep_image(ctx, image);
    }
    auto* pel = new PageElementImage();
    pel->pageNo = pageNo;
    pel->rect = rf;
    pel->imageID = len(images);
    img->imageElement = pel;
    VecAppend(images, img);
}

// A pass-through-free device that records the page rects of drawn images,
// including content-stream images that text extraction doesn't see. Tracks
// the clip stack so recorded rects are clipped to what's actually visible.
constexpr int kImgCollectStackSize = 96;

typedef struct {
    fz_device super;
    Vec<FitzPageImageInfo*>* images;
    int pageNo;
    int top;
    fz_rect stack[kImgCollectStackSize];
} fz_image_collect_device;

static void fz_img_collect_add(fz_context* ctx, fz_device* dev, fz_rect rect, bool clip, fz_image* image) {
    fz_image_collect_device* d = (fz_image_collect_device*)dev;
    if (d->top > 0 && d->top <= kImgCollectStackSize) {
        rect = fz_intersect_rect(rect, d->stack[d->top - 1]);
    }
    if (!clip && !fz_is_empty_rect(rect)) {
        FzAppendPageImageRect(ctx, *d->images, d->pageNo, rect, image);
    }
    if (clip && ++d->top <= kImgCollectStackSize) {
        d->stack[d->top - 1] = rect;
    }
}

static void fz_img_collect_fill_image(fz_context* ctx, fz_device* dev, fz_image* image, fz_matrix ctm, float /*alpha*/,
                                      fz_color_params /*colorParams*/) {
    fz_img_collect_add(ctx, dev, fz_transform_rect(fz_unit_rect, ctm), false, image);
}

// Image masks are knockouts / stencil shapes, not photos, so dark-mode preserve
// must not treat them as artwork (#5806). Ignoring them is the whole job: this
// used to call fz_img_collect_add() with clip=true, which pushed a clip that
// nothing ever pops - fill_image_mask is not a clipping operation and has no
// matching pop_clip, unlike clip_image_mask below. Every mask on a page then
// narrowed the rect of every image drawn after it (often below the preserve
// min size, so a full-page image stopped being preserved and got recolored)
// and left d->top too high for the rest of the page (#5887).
static void fz_img_collect_fill_image_mask(fz_context* /*ctx*/, fz_device* /*dev*/, fz_image* /*img*/,
                                           fz_matrix /*ctm*/, fz_colorspace* /*cs*/, const float* /*color*/,
                                           float /*alpha*/, fz_color_params /*colorParams*/) {}

static void fz_img_collect_clip_path(fz_context* ctx, fz_device* dev, const fz_path* path, int /*evenOdd*/,
                                     fz_matrix ctm, fz_rect /*scissor*/) {
    fz_img_collect_add(ctx, dev, fz_bound_path(ctx, path, nullptr, ctm), true, nullptr);
}

static void fz_img_collect_clip_stroke_path(fz_context* ctx, fz_device* dev, const fz_path* path,
                                            const fz_stroke_state* stroke, fz_matrix ctm, fz_rect /*scissor*/) {
    fz_img_collect_add(ctx, dev, fz_bound_path(ctx, path, stroke, ctm), true, nullptr);
}

static void fz_img_collect_clip_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm,
                                     fz_rect /*scissor*/) {
    fz_img_collect_add(ctx, dev, fz_bound_text(ctx, text, nullptr, ctm), true, nullptr);
}

static void fz_img_collect_clip_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text,
                                            const fz_stroke_state* stroke, fz_matrix ctm, fz_rect /*scissor*/) {
    fz_img_collect_add(ctx, dev, fz_bound_text(ctx, text, stroke, ctm), true, nullptr);
}

static void fz_img_collect_clip_image_mask(fz_context* ctx, fz_device* dev, fz_image* /*img*/, fz_matrix ctm,
                                           fz_rect /*scissor*/) {
    fz_img_collect_add(ctx, dev, fz_transform_rect(fz_unit_rect, ctm), true, nullptr);
}

static void fz_img_collect_pop_clip(fz_context* /*ctx*/, fz_device* dev) {
    fz_image_collect_device* d = (fz_image_collect_device*)dev;
    if (d->top > 0) {
        d->top--;
    }
}

static void fz_img_collect_begin_mask(fz_context* ctx, fz_device* dev, fz_rect rect, int /*luminosity*/,
                                      fz_colorspace* /*cs*/, const float* /*bc*/, fz_color_params /*colorParams*/) {
    fz_img_collect_add(ctx, dev, rect, true, nullptr);
}

static void fz_img_collect_end_mask(fz_context* ctx, fz_device* dev, fz_function* /*fn*/) {
    fz_img_collect_pop_clip(ctx, dev);
}

static void fz_img_collect_begin_group(fz_context* ctx, fz_device* dev, fz_rect rect, fz_colorspace* /*cs*/,
                                       int /*isolated*/, int /*knockout*/, int /*blendmode*/, float /*alpha*/) {
    fz_img_collect_add(ctx, dev, rect, true, nullptr);
}

static void fz_img_collect_end_group(fz_context* ctx, fz_device* dev) {
    fz_img_collect_pop_clip(ctx, dev);
}

static int fz_img_collect_begin_tile(fz_context* ctx, fz_device* dev, fz_rect area, fz_rect /*view*/, float /*xstep*/,
                                     float /*ystep*/, fz_matrix ctm, int /*id*/, int /*docId*/) {
    fz_img_collect_add(ctx, dev, fz_transform_rect(area, ctm), false, nullptr);
    return 0;
}

static void fz_img_collect_end_tile(fz_context* /*ctx*/, fz_device* /*dev*/) {}

static fz_device* FzNewImageCollectDevice(fz_context* ctx, Vec<FitzPageImageInfo*>* images, int pageNo) {
    fz_image_collect_device* dev = fz_new_derived_device(ctx, fz_image_collect_device);
    dev->super.fill_image = fz_img_collect_fill_image;
    dev->super.fill_image_mask = fz_img_collect_fill_image_mask;
    dev->super.clip_path = fz_img_collect_clip_path;
    dev->super.clip_stroke_path = fz_img_collect_clip_stroke_path;
    dev->super.clip_text = fz_img_collect_clip_text;
    dev->super.clip_stroke_text = fz_img_collect_clip_stroke_text;
    dev->super.clip_image_mask = fz_img_collect_clip_image_mask;
    dev->super.pop_clip = fz_img_collect_pop_clip;
    dev->super.begin_mask = fz_img_collect_begin_mask;
    dev->super.end_mask = fz_img_collect_end_mask;
    dev->super.begin_group = fz_img_collect_begin_group;
    dev->super.end_group = fz_img_collect_end_group;
    dev->super.begin_tile = fz_img_collect_begin_tile;
    dev->super.end_tile = fz_img_collect_end_tile;
    dev->images = images;
    dev->pageNo = pageNo;
    dev->top = 0;
    return &dev->super;
}

static void FzCollectImagesFromPageContent(fz_context* ctx, int pageNo, FzPageInfo* pageInfo, fz_page* page,
                                           fz_cookie* cookie) {
    fz_device* dev = nullptr;
    fz_var(dev);
    fz_try(ctx) {
        dev = FzNewImageCollectDevice(ctx, &pageInfo->images, pageNo);
        fz_run_page(ctx, page, dev, fz_identity, cookie);
    }
    fz_always(ctx) {
        if (dev) {
            fz_drop_device(ctx, dev);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
}

static fz_image* FzFindImageByRect(fz_context* ctx, FzPageInfo* pageInfo, fz_rect target) {
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
    float bestOverlap = 0.f;
    fz_image* best = nullptr;
    fz_stext_block* block = stext->first_block;
    while (block) {
        if (block->type != FZ_STEXT_BLOCK_IMAGE) {
            block = block->next;
            continue;
        }
        fz_image* image = block->u.i.image;
        if (!image) {
            block = block->next;
            continue;
        }
        float overlap = FzRectOverlap(block->bbox, target);
        if (overlap > bestOverlap) {
            if (best) {
                fz_drop_image(ctx, best);
            }
            bestOverlap = overlap;
            best = fz_keep_image(ctx, image);
        }
        block = block->next;
    }
    fz_drop_stext_page(ctx, stext);
    if (bestOverlap < 0.45f) {
        if (best) {
            fz_drop_image(ctx, best);
            best = nullptr;
        }
    }
    return best;
}

static fz_image* FzGetKeptPageImage(fz_context* ctx, FzPageInfo* pageInfo, int idx) {
    if (!pageInfo || idx < 0 || idx >= len(pageInfo->images)) {
        return nullptr;
    }
    FitzPageImageInfo* info = pageInfo->images[idx];
    if (info->image) {
        return fz_keep_image(ctx, info->image);
    }
    return FzFindImageByRect(ctx, pageInfo, info->rect);
}

// --- end dark-mode image preservation helpers ---

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

// mupdf's own conversion from a PDF action / destination object to a link uri
// (pdf-link.c). Not in a public header, so declare it here, like the other
// mupdf internals this file reaches into.
extern "C" char* pdf_parse_link_action(fz_context* ctx, pdf_document* doc, pdf_obj* action, int pagenum);
extern "C" char* pdf_parse_link_dest(fz_context* ctx, pdf_document* doc, pdf_obj* dest);

// mupdf only makes links out of /Link annotations (pdf_load_link() rejects
// every other subtype). A PDF can just as well navigate from a pushbutton form
// field: a /Widget annotation with a GoTo or URI action, which is how documents
// produced by Acrobat draw "back to where you came from" arrows. Those are
// followed by Acrobat and by browsers, and by SumatraPDF up to 3.1.2. Give them
// the same fz_links a /Link annotation gets, so they hover, click, and follow
// like any other link (fixes #1914).
static fz_link* MakePushButtonWidgetLinks(fz_context* ctx, pdf_document* doc, pdf_page* pdfpage, int pageNo) {
    fz_link* head = nullptr;
    fz_link* tail = nullptr;
    for (pdf_annot* w = pdf_first_widget(ctx, pdfpage); w; w = pdf_next_widget(ctx, w)) {
        char* uri = nullptr;
        fz_rect rect{};
        fz_var(uri);
        fz_var(rect);
        fz_try(ctx) {
            if (pdf_widget_type(ctx, w) == PDF_WIDGET_TYPE_BUTTON) {
                // same places pdf_load_link() looks, in the same order
                pdf_obj* obj = pdf_annot_obj(ctx, w);
                pdf_obj* dest = pdf_dict_get(ctx, obj, PDF_NAME(Dest));
                if (dest) {
                    uri = pdf_parse_link_dest(ctx, doc, dest);
                } else {
                    pdf_obj* action = pdf_dict_get(ctx, obj, PDF_NAME(A));
                    if (!action) {
                        action = pdf_dict_geta(ctx, pdf_dict_get(ctx, obj, PDF_NAME(AA)), PDF_NAME(U), PDF_NAME(D));
                    }
                    // returns null for actions that aren't navigation
                    // (JavaScript, ResetForm, ...), which stay non-links
                    uri = pdf_parse_link_action(ctx, doc, action, pageNo - 1);
                }
                if (uri) {
                    rect = pdf_bound_annot(ctx, w);
                }
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            uri = nullptr;
        }
        if (!uri) {
            continue;
        }

        fz_link* link = nullptr;
        fz_try(ctx) {
            link = pdf_new_link(ctx, pdfpage, rect, uri, pdf_annot_obj(ctx, w));
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            link = nullptr;
        }
        fz_free(ctx, uri);
        if (!link) {
            continue;
        }
        if (!tail) {
            head = tail = link;
        } else {
            tail->next = link;
            tail = link;
        }
    }
    return head;
}

static void SkipJsWs(const char*& p, const char* end) {
    while (p < end && str::IsWs(*p)) {
        p++;
    }
}

static bool IsJsIdentStart(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$';
}

static bool IsJsIdentChar(char c) {
    return IsJsIdentStart(c) || (c >= '0' && c <= '9');
}

static int JsHexNibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool IsJsReservedCallName(Str ident) {
    return str::Eq(ident, StrL("function")) || str::Eq(ident, StrL("if")) || str::Eq(ident, StrL("for")) ||
           str::Eq(ident, StrL("while")) || str::Eq(ident, StrL("switch")) || str::Eq(ident, StrL("catch")) ||
           str::Eq(ident, StrL("with")) || str::Eq(ident, StrL("return")) || str::Eq(ident, StrL("typeof")) ||
           str::Eq(ident, StrL("void")) || str::Eq(ident, StrL("delete")) || str::Eq(ident, StrL("new")) ||
           str::Eq(ident, StrL("throw")) || str::Eq(ident, StrL("else")) || str::Eq(ident, StrL("do")) ||
           str::Eq(ident, StrL("try"));
}

// Decode one JS '...' or "..." string at p. Advances p past the closing quote.
static bool ParseJsQuotedString(const char*& p, const char* end, Str* out) {
    *out = {};
    if (p >= end || (*p != '"' && *p != '\'')) {
        return false;
    }
    char quote = *p++;
    str::Builder b;
    while (p < end && *p != quote) {
        char c = *p++;
        if (c != '\\') {
            b.AppendChar(c);
            continue;
        }
        if (p >= end) {
            break;
        }
        char e = *p++;
        switch (e) {
            case 'n':
                b.AppendChar('\n');
                break;
            case 'r':
                b.AppendChar('\r');
                break;
            case 't':
                b.AppendChar('\t');
                break;
            case 'b':
                b.AppendChar('\b');
                break;
            case 'f':
                b.AppendChar('\f');
                break;
            case 'v':
                b.AppendChar('\v');
                break;
            case '0':
                b.AppendChar('\0');
                break;
            case '\\':
            case '\'':
            case '"':
                b.AppendChar(e);
                break;
            case 'x': {
                if (p + 2 > end) {
                    b.AppendChar(e);
                    break;
                }
                int h1 = JsHexNibble(p[0]);
                int h2 = JsHexNibble(p[1]);
                if (h1 < 0 || h2 < 0) {
                    b.AppendChar(e);
                    break;
                }
                p += 2;
                b.AppendChar((char)((h1 << 4) | h2));
                break;
            }
            case 'u': {
                if (p + 4 > end) {
                    b.AppendChar(e);
                    break;
                }
                int cp = 0;
                bool ok = true;
                for (int i = 0; i < 4; i++) {
                    int h = JsHexNibble(p[i]);
                    if (h < 0) {
                        ok = false;
                        break;
                    }
                    cp = (cp << 4) | h;
                }
                if (!ok) {
                    b.AppendChar(e);
                    break;
                }
                p += 4;
                char utf8[4];
                int off = 0;
                str::Utf8Encode(utf8, off, cp);
                b.Append(Str(utf8, off));
                break;
            }
            default:
                b.AppendChar(e);
                break;
        }
    }
    if (p >= end || *p != quote) {
        return false;
    }
    p++;
    *out = b.TakeStr();
    return true;
}

static bool SkipJsNested(const char*& p, const char* end, char open, char close) {
    if (p >= end || *p != open) {
        return false;
    }
    int depth = 1;
    p++;
    while (p < end && depth > 0) {
        if (*p == '"' || *p == '\'') {
            Str dummy;
            if (!ParseJsQuotedString(p, end, &dummy)) {
                str::Free(dummy);
                return false;
            }
            str::Free(dummy);
            continue;
        }
        if (*p == open) {
            depth++;
        } else if (*p == close) {
            depth--;
        }
        p++;
    }
    return depth == 0;
}

// Collect the quoted arguments of app.popUpMenu(...) / app.popUpMenuEx(...).
static bool ParseJsPopUpMenuItems(Str js, StrVec& items) {
    int idx = str::IndexOf(js, StrL("popUpMenu"));
    if (idx < 0) {
        return false;
    }
    const char* p = js.s + idx + 9; // strlen("popUpMenu")
    const char* end = js.s + len(js);
    if (p + 2 <= end && p[0] == 'E' && p[1] == 'x') {
        p += 2;
    }
    SkipJsWs(p, end);
    if (p >= end || *p != '(') {
        return false;
    }
    p++;
    while (p < end) {
        SkipJsWs(p, end);
        if (p >= end) {
            break;
        }
        if (*p == ')') {
            break;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '[') {
            if (!SkipJsNested(p, end, '[', ']')) {
                break;
            }
            continue;
        }
        if (*p == '"' || *p == '\'') {
            Str item;
            if (!ParseJsQuotedString(p, end, &item)) {
                str::Free(item);
                break;
            }
            items.Append(item);
            str::Free(item);
            continue;
        }
        p++;
    }
    return len(items) > 0;
}

// First identifier that is followed by '(', skipping JS keywords.
static Str ExtractJsCallName(Str js) {
    if (!js) {
        return {};
    }
    const char* p = js.s;
    const char* end = js.s + len(js);
    while (p < end) {
        SkipJsWs(p, end);
        if (p >= end) {
            break;
        }
        if (!IsJsIdentStart(*p)) {
            p++;
            continue;
        }
        const char* start = p;
        p++;
        while (p < end && IsJsIdentChar(*p)) {
            p++;
        }
        Str ident{start, (int)(p - start)};
        SkipJsWs(p, end);
        if (p < end && *p == '(' && !IsJsReservedCallName(ident)) {
            return ident;
        }
    }
    return {};
}

static char* LookupNamedJavaScript(fz_context* ctx, pdf_document* doc, Str name) {
    if (!ctx || !doc || !name) {
        return nullptr;
    }
    pdf_obj* needle = nullptr;
    char* js = nullptr;
    fz_var(needle);
    fz_var(js);
    fz_try(ctx) {
        needle = pdf_new_string(ctx, name.s, (size_t)len(name));
        pdf_obj* found = pdf_lookup_name(ctx, doc, PDF_NAME(JavaScript), needle);
        if (found) {
            if (pdf_is_dict(ctx, found)) {
                found = pdf_dict_get(ctx, found, PDF_NAME(JS));
            }
            if (found) {
                js = pdf_load_stream_or_string_as_utf8(ctx, found);
            }
        }
    }
    fz_always(ctx) {
        pdf_drop_obj(ctx, needle);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        js = nullptr;
    }
    return js;
}

// Altium schematic PDFs (and similar) put component metadata on /Link annots
// whose action is Acrobat JavaScript: either app.popUpMenu("...") inline, or a
// call to a named ShowCompProps_* function in catalog /Names /JavaScript.
// MuPDF does not turn JavaScript actions into fz_links (pdf_parse_link_action
// returns null), and pdf_first_annot skips /Link annots, so walk /Annots.
// Parse the menu strings without executing JS (issue #1198).
static void AppendJsMenuLinks(fz_context* ctx, pdf_document* doc, pdf_page* pdfpage, int pageNo,
                              Vec<PageElementDestination*>& links) {
    if (!ctx || !doc || !pdfpage) {
        return;
    }
    pdf_obj* annots = nullptr;
    int n = 0;
    fz_matrix ctm{};
    fz_try(ctx) {
        fz_rect mediabox;
        pdf_page_transform(ctx, pdfpage, &mediabox, &ctm);
        pdf_obj* pageObj = pdf_lookup_page_obj(ctx, doc, pageNo - 1);
        annots = pdf_dict_get(ctx, pageObj, PDF_NAME(Annots));
        n = pdf_array_len(ctx, annots);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return;
    }

    for (int i = 0; i < n; i++) {
        char* js = nullptr;
        fz_rect rect{};
        fz_var(js);
        fz_try(ctx) {
            pdf_obj* annot = pdf_array_get(ctx, annots, i);
            pdf_obj* subtype = pdf_dict_get(ctx, annot, PDF_NAME(Subtype));
            if (pdf_name_eq(ctx, subtype, PDF_NAME(Link))) {
                pdf_obj* action = pdf_dict_get(ctx, annot, PDF_NAME(A));
                pdf_obj* s = pdf_dict_get(ctx, action, PDF_NAME(S));
                if (pdf_name_eq(ctx, s, PDF_NAME(JavaScript))) {
                    pdf_obj* jsObj = pdf_dict_get(ctx, action, PDF_NAME(JS));
                    if (jsObj) {
                        js = pdf_load_stream_or_string_as_utf8(ctx, jsObj);
                        rect = pdf_dict_get_rect(ctx, annot, PDF_NAME(Rect));
                        rect = fz_transform_rect(rect, ctm);
                    }
                }
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            js = nullptr;
        }
        if (!js) {
            continue;
        }
        StrVec items;
        ParseJsPopUpMenuItems(Str(js), items);
        if (len(items) == 0) {
            Str name = ExtractJsCallName(Str(js));
            if (name) {
                char* namedJs = LookupNamedJavaScript(ctx, doc, name);
                if (namedJs) {
                    ParseJsPopUpMenuItems(Str(namedJs), items);
                    fz_free(ctx, namedJs);
                }
            }
        }
        fz_free(ctx, js);
        if (len(items) == 0) {
            continue;
        }
        auto* dest = new PageDestinationJsMenu();
        dest->items = items;
        dest->rect = ToRectF(rect);
        auto* pel = new PageElementDestination(dest);
        pel->pageNo = pageNo;
        pel->rect = dest->rect;
        VecAppend(links, pel);
    }
}

static pdf_obj* PdfCopyStrDict(fz_context* ctx, pdf_document* /*doc*/, pdf_obj* dict) {
    pdf_obj* copy = pdf_copy_dict(ctx, dict);
    for (int i = 0; i < pdf_dict_len(ctx, copy); i++) {
        pdf_obj* val = pdf_dict_get_val(ctx, copy, i);
        // resolve all indirect references
        if (pdf_is_indirect(ctx, val)) {
            auto* s = pdf_to_str_buf(ctx, val);
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
            // the `false &&` disable is deliberate; silence /analyze C6237
#pragma warning(suppress : 6237)
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
        logf("PdfLoadAttachment() failed\n");
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
        logf("PdfLoadAnnotationAttachment(objNum=%d) failed\n", objNum);
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
            // the `false &&` disable is deliberate; silence /analyze C6237
#pragma warning(suppress : 6237)
            if (false && !pdf_is_embedded_file(ctx, fs)) {
                continue;
            }
            pdf_filespec_params fileParams = {};
            pdf_get_filespec_params(ctx, fs, &fileParams);
            const char* nameStr = fileParams.filename;
            if (len(nameStr) == 0 || (fileParams.size < 0)) {
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
        logf("PdfLoadAttachments() failed for '%s'\n", path);
    }
    return root.next;
}

struct PageLabelInfo {
    int startAt = 0;
    int countFrom = 0;
    Str type;
    pdf_obj* prefix = nullptr;
};

static int CmpPageLabelInfo(const PageLabelInfo* a, const PageLabelInfo* b) {
    return a->startAt - b->startAt;
}

// Some PDFs (often scanned ebooks) assign a separate PageLabels entry to
// almost every page with only a custom prefix and no numbering style (/S).
// These aren't meaningful page numbers and break the toolbar display.
static bool IsPerPagePrefixOnlyLabels(Vec<PageLabelInfo>& data, int pageCount) {
    int n = len(data);
    if (n < 16 || pageCount <= 0 || n < pageCount / 4) {
        return false;
    }
    int prefixOnly = 0;
    for (int i = 0; i < n; i++) {
        PageLabelInfo& pli = data[i];
        if (len(pli.type) == 0 && pli.prefix) {
            prefixOnly++;
        }
    }
    return prefixOnly * 4 >= n * 3;
}

// Scanned ebooks converted from .pdg image collections leak internal image
// file names into PageLabels; treat those as no labels at all.
static bool PageLabelsContainInternalPdgNames(StrVec* labels, int pageCount) {
    int n = labels->size;
    int samples = std::min(n, pageCount);
    samples = std::min(samples, 32);
    for (int i = 0; i < samples; i++) {
        Str label = labels->At(i);
        if (str::ContainsI(label, StrL(".pdg"))) {
            return true;
        }
    }
    return false;
}

static TempStr FormatPageLabelTemp(Str type, int pageNo, Str prefix) {
    if (str::Eq(type, StrL("D"))) {
        return fmt("%s%d", prefix, pageNo);
    }
    if (str::EqI(type, StrL("R"))) {
        // roman numbering style
        TempStr number = str::FormatRomanNumeralTemp(pageNo);
        if (len(type) > 0 && type.s[0] == 'r') {
            str::ToLowerInPlace(number);
        }
        return fmt("%s%s", prefix, number);
    }
    if (str::EqI(type, StrL("A"))) {
        // alphabetic numbering style (A..Z, AA..ZZ, AAA..ZZZ, ...)
        str::Builder number;
        number.AppendChar((char)('A' + ((pageNo - 1) % 26)));
        for (int i = 0; i < (pageNo - 1) / 26; i++) {
            number.AppendChar(number[0]);
        }
        if (len(type) > 0 && type.s[0] == 'a') {
            str::ToLowerInPlace(ToStr(number));
        }
        return fmt("%s%s", prefix, ToStr(number));
    }
    return str::DupTemp(prefix);
}

static void BuildPageLabelRec(fz_context* ctx, pdf_obj* node, int pageCount, Vec<PageLabelInfo>& data, int depth) {
    if (depth >= 64) {
        return;
    }
    pdf_obj* obj = pdf_dict_gets(ctx, node, "Kids");
    if (obj != nullptr && !pdf_mark_obj(ctx, node)) {
        int n = pdf_array_len(ctx, obj);
        for (int i = 0; i < n; i++) {
            auto* arr = pdf_array_get(ctx, obj, i);
            BuildPageLabelRec(ctx, arr, pageCount, data, depth + 1);
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
        // the key is attacker-controlled: INT_MAX + 1 in int is signed
        // overflow, undefined behavior (issue #5952)
        i64 startAt = (i64)pdf_to_int(ctx, pdf_array_get(ctx, obj, i)) + 1;
        if (startAt < 1 || startAt > (i64)pageCount) {
            continue;
        }
        pli.startAt = (int)startAt;

        pli.type = Str(pdf_to_name(ctx, pdf_dict_gets(ctx, info, "S")));
        pli.prefix = pdf_dict_gets(ctx, info, "P");
        pli.countFrom = pdf_to_int(ctx, pdf_dict_gets(ctx, info, "St"));
        // BuildPageLabelVec computes countFrom + j for j < pageCount: keep the
        // sum representable for an attacker-controlled /St near INT_MAX
        pli.countFrom = limitValue(pli.countFrom, 1, INT_MAX - pageCount);
        VecAppend(data, pli);
    }
}

static StrVec* BuildPageLabelVec(fz_context* ctx, pdf_obj* root, int pageCount) {
    Vec<PageLabelInfo> data;
    BuildPageLabelRec(ctx, root, pageCount, data, 0);
    VecSort(data, CmpPageLabelInfo);

    int n = len(data);
    if (n == 0) {
        return nullptr;
    }

    PageLabelInfo& pli = data[0];
    if (n == 1 && pli.startAt == 1 && pli.countFrom == 1 && !pli.prefix && str::Eq(pli.type, StrL("D"))) {
        // this is the default case, no need for special treatment
        return nullptr;
    }
    if (IsPerPagePrefixOnlyLabels(data, pageCount)) {
        return nullptr;
    }

    StrVec* labels = new StrVec();
    for (int i = 0; i < pageCount; i++) {
        labels->Append(StrL(""));
    }

    for (int i = 0; i < n; i++) {
        pli = data[i];
        if (pli.startAt > pageCount) {
            break;
        }
        int secLen = pageCount + 1 - pli.startAt;
        if (i < n - 1 && data[i + 1].startAt <= pageCount) {
            secLen = data[i + 1].startAt - pli.startAt;
        }
        TempStr prefix = PdfToUtf8Temp(ctx, data[i].prefix);
        for (int j = 0; j < secLen; j++) {
            int idx = pli.startAt + j - 1;
            TempStr label = FormatPageLabelTemp(pli.type, pli.countFrom + j, prefix);
            labels->SetAt(idx, label);
        }
    }

    for (int idx = 0; (idx = labels->Find(Str(), idx)) != -1; idx++) {
        labels->SetAt(idx, StrL(""));
    }

    if (PageLabelsContainInternalPdgNames(labels, pageCount)) {
        delete labels;
        return nullptr;
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
    e->fz_locks[lock].Lock();
}

static void fz_unlock_context_cs(void* user, int lock) {
    EngineMupdf* e = (EngineMupdf*)user;
    e->fz_locks[lock].Unlock();
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
    if (!str::EndsWith(msgStr, StrL("\n"))) {
        msgStr = str::JoinTemp(msgStr, StrL("\n"));
    }
    log(msgStr);
    EngineMupdf* engine = (EngineMupdf*)user;
    if (!engine) {
        return;
    }
    // logged above, not shown as "Errors in document": epub 3.0 is rendered
    // fine, a missing system font falls back to a default (issue #6027),
    // Movie (etc.) appearance streams are a MuPDF limitation, not a PDF defect,
    // and hyphen tables are compiled out (issue #6109)
    if (str::Contains(msgStr, StrL("unknown epub version")) ||
        str::Contains(msgStr, StrL("couldn't find system font")) ||
        str::Contains(msgStr, StrL("cannot create appearance stream")) ||
        str::Contains(msgStr, StrL("no hyphenation table"))) {
        return;
    }
    engine->AppendError(msgStr);
}

static void InstallFitzErrorCallbacks(EngineMupdf* engine, fz_context* ctx) {
    fz_set_warning_callback(ctx, fz_print_cb, (void*)engine);
    fz_set_error_callback(ctx, fz_print_cb, (void*)engine);
}

struct ContextThreadID {
    EngineMupdf* engine = nullptr;
    fz_context* ctx = nullptr;
    ThreadId threadID = 0;
};

static Vec<ContextThreadID>* gPerThreadContexts;
static Mutex gPerThreadContextsCs;
static AtomicInt gEngineCount = 0;

static void InitializeEngineMupdf() {
    auto n = AtomicIntInc(&gEngineCount);
    if (n != 1) return;
    ReportIf(gPerThreadContexts);
    gPerThreadContexts = new Vec<ContextThreadID>();
}

static void DeInitializeEngineMupdf() {
    auto n = AtomicIntDec(&gEngineCount);
    if (n > 0) return;
    ReportIf(n < 0);
    delete gPerThreadContexts;
    gPerThreadContexts = nullptr;
}

static fz_context* GetOrClonePerThreadContext(EngineMupdf* engine, fz_context* ctx) {
    ThreadId threadID = GetCurrentThreadId();
    {
        ScopedMutex cs(&gPerThreadContextsCs);
        for (auto& el : *gPerThreadContexts) {
            if (el.engine == engine && el.threadID == threadID) {
                return el.ctx;
            }
        }
    }
    // clone context without holding gPerThreadContextsCs to avoid deadlock
    // with threads that hold fz_locks (e.g. docLock) and then call Ctx()
    // safe because only current thread can create a context for its own threadID
    auto* newCtx = fz_clone_context(ctx);
    if (!newCtx) {
        // OOM or unexpected clone failure: fall back to the engine's main context
        // rather than caching/returning nullptr (which would crash mupdf callers).
        return ctx;
    }
    {
        ScopedMutex cs(&gPerThreadContextsCs);
        ContextThreadID el{engine, newCtx, threadID};
        VecAppend(*gPerThreadContexts, el);
    }
    return newCtx;
}

static void ReleasePerThreadContext(EngineMupdf* engine) {
    ThreadId threadID = GetCurrentThreadId();
    fz_context* ctxToDrop = nullptr;
    {
        ScopedMutex cs(&gPerThreadContextsCs);
        auto n = len(*gPerThreadContexts);
        for (int i = 0; i < n; i++) {
            auto& el = (*gPerThreadContexts)[i];
            if (el.engine == engine && el.threadID == threadID) {
                ctxToDrop = el.ctx;
                VecRemoveAtFast(*gPerThreadContexts, i);
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
        ScopedMutex cs(&gPerThreadContextsCs);
        for (int i = len(*gPerThreadContexts) - 1; i >= 0; i--) {
            auto& el = (*gPerThreadContexts)[i];
            if (el.engine == engine) {
                VecAppend(ctxsToDrop, el.ctx);
                VecRemoveAtFast(*gPerThreadContexts, i);
            }
        }
    }
    for (auto* ctx : ctxsToDrop) {
        fz_drop_context(ctx);
    }
}

EngineMupdf::EngineMupdf() {
    InitializeEngineMupdf();
    kind = kindEngineMupdf;
    defaultExt = str::Dup(StrL(".pdf"));
    fileDPI = 72.0f;
    darkModeEngineCache = PdfDarkModeEngineCacheCreate();

    fz_locks_ctx.user = this;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    _ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_DEFAULT);
    if (!_ctx) {
        // can happen when out of memory. Load() will fail
        log(StrL("EngineMupdf: fz_new_context() failed\n"));
        return;
    }
    InstallFitzErrorCallbacks(this, _ctx);

#if OS_WIN
    install_load_windows_font_funcs(_ctx);
#endif
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
    pagesLock.Lock();
    str::Free(ebookFontUnavailable);
    str::Free(ebookUserCss);

    auto* ctx = _ctx;
    if (darkModeEngineCache) {
        PdfDarkModeEngineCacheFree(ctx, darkModeEngineCache);
        darkModeEngineCache = nullptr;
    }
    for (FzPageInfo* pi : pages) {
        DeleteVecMembers(pi->links);
        DeleteVecMembers(pi->autoLinks);
        DeleteVecMembers(pi->comments);
        for (FitzPageImageInfo* img : pi->images) {
            if (img && img->image) {
                fz_drop_image(ctx, img->image);
                img->image = nullptr;
            }
        }
        DeleteVecMembers(pi->images);
        DeleteVecMembers(pi->annotations);
        DeleteVecMembers(pi->widgets);
        if (pi->retainedLinks) {
            fz_drop_link(ctx, pi->retainedLinks);
        }
        if (pi->displayList) {
            fz_drop_display_list(ctx, pi->displayList);
        }
        PdfDarkModeInvalidatePage(ctx, pi);
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
    FreeTocItemRec(nullptr, pendingHeadingToc);
    pendingHeadingToc = nullptr;
    delete tocTree;

    pagesLock.Unlock();

    DeInitializeEngineMupdf();
}

class PasswordCloner : public PasswordUI {
    u8* cryptKey = nullptr;

  public:
    explicit PasswordCloner(u8* cryptKey) { this->cryptKey = cryptKey; }

    Str GetPassword(Str /*path*/, u8* /*fileDigest*/, u8 decryptionKeyOut[32], bool* saveKey) override {
        memcpy(decryptionKeyOut, cryptKey, 32);
        *saveKey = true;
        return {};
    }
};

EngineBase* EngineMupdf::Clone() {
    // use this document's encryption key (if any) to load the clone
    u8 cryptKey[32]{};
    bool hasCryptKey = false;
    PasswordCloner* pwdUI = nullptr;
    {
        ScopedRecursiveMutex scope(&docLock);
        auto* ctx = Ctx();
        if (pdfdoc) {
            u8* key = pdf_crypt_key(ctx, pdfdoc->crypt);
            if (key) {
                memcpy(cryptKey, key, sizeof(cryptKey));
                hasCryptKey = true;
            }
        }
    }
    if (hasCryptKey) {
        pwdUI = new PasswordCloner(cryptKey);
    }

    // prefer re-loading from the file: it streams large documents on demand
    // rather than copying them, and is the cheapest path when the file is present.
    EngineMupdf* clone = nullptr;
    if (FilePath()) {
        clone = new EngineMupdf();
        if (!clone->Load(FilePath(), pwdUI)) {
            delete clone;
            clone = nullptr;
        }
    }
    // The file may have been moved or deleted after opening. mupdf can no longer
    // re-open a file stream, but documents loaded fully into memory (those under
    // kMaxMemoryFileSize) still hold their bytes, so clone from those instead of
    // the missing file. This is what let e.g. printing a moved/deleted PDF keep
    // working before the port (issue #5790); GetFileData() is PDF-only.
    if (!clone) {
        Str data = GetFileData();
        if (data) {
            clone = (EngineMupdf*)CreateEngineMupdfFromData(data, FilePath(), pwdUI);
        }
        str::Free(data);
    }
    if (!clone) {
        logf("EngineMupdf::Clone() failed for '%s'\n", FilePath() ? FilePath() : StrL("(null)"));
        delete pwdUI;
        return nullptr;
    }
    delete pwdUI;

    {
        ScopedRecursiveMutex scope(&docLock);
        clone->disableAntiAlias = disableAntiAlias;
        clone->disableAutoLinks = disableAutoLinks;
        clone->cadDetectDone = cadDetectDone;
        clone->cadDetectEnable = cadDetectEnable;
        clone->cadDetectScore = cadDetectScore;
        clone->cadRasterDominant = cadRasterDominant;
        clone->cadHairlineVector = cadHairlineVector;
        clone->cadEnhanceOverride = cadEnhanceOverride;

        if (!decryptionKey.s && pdfdoc && pdfdoc->crypt) {
            clone->decryptionKey = Str();
        }
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
        bool hasAttachmentName = rest && str::StartsWith(rest, StrL(":attachname="));
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
    auto* ctx = Ctx();
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
    if (len(fd) == 0) {
        return {};
    }

    AtomicIntInc(&gAllowAllocFailure);
    AutoCall decAllowAlloc(AtomicIntDec, &gAllowAllocFailure);

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
    d.Append(StrL(R"(<html>
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
    <pre>)"));
    bool ok = d.Append(data);
    if (!ok) {
        return {};
    }
    d.Append(StrL(R"(</pre>
</body>
</html>)"));
    return d.TakeStr();
}

static Str PalmDocToHTML(Str path) {
    auto* doc = PalmDoc::CreateFromFile(path);
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
    auto* ctx = Ctx();
    ReportIf(FilePath() || _doc);
    if (!ctx) {
        return false;
    }
    SetFilePath(path);

    auto ext = path::GetExtTemp(path);
    SetDefaultExt(defaultExt, ext);

    int streamNo = -1;
    TempStr fnCopy = ParseEmbeddedStreamNumber(path, &streamNo);

    FileType kind = GuessFileTypeFromName(path);
    // show .txt, .xml and other text files as plain text
    // using html engine
    if (kind == FileType::Txt) {
        // synthesize a .html file from text file
        Str d = TxtFileToHTML(path);
        if (len(d) == 0) {
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

    if (str::EqI(ext, StrL(".pdb"))) {
        // synthesize a .html file from pdb file
        Str d = PalmDocToHTML(path);
        if (len(d) == 0) {
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

// is implemented in SumatraPDF.exe, PdfFilter and PdfPreview
// TODO: allow setting per
extern EBookUI* GetEBookUI();
// per-document overrides (FileStates -> EBookUI), null unless the document has them
extern FileEBookUI* GetFileEBookUI(Str filePath);

static TempStr EbookLineSpacingCssTemp(float lineSpacing) {
    if (!(lineSpacing >= 0.5f && lineSpacing <= 5.f)) {
        return {};
    }
    return fmt("body, body * { line-height: %g !important; }\n", lineSpacing);
}

// user CSS with !important beats the publisher's font-family (issue #3138) and,
// because it comes from the user stylesheet, also inline style="font-family:..."
// attributes (issue #4600).
// the element list has to be this long because font-family only reaches text
// whose own element matches: publishers routinely put the font on a <span> or
// another inline element. pre, code, kbd, samp and tt are left out on purpose,
// so code stays monospace
static TempStr EbookFontFamilyCssTemp(Str fontName) {
    if (!fontName) {
        return {};
    }
    return fmt(
        "body, p, div, span, a, em, strong, b, i, u, s, small, big, sub, sup, "
        "li, ol, ul, dl, dt, dd, td, th, caption, table, "
        "h1, h2, h3, h4, h5, h6, blockquote, q, cite, "
        "section, article, aside, header, footer, nav, main, figure, figcaption, "
        "label, center, font { font-family: \"%s\" !important; }\n",
        fontName);
}

// mupdf lays reflowable text into the page minus the @page margins (its own
// default is "@page{margin:3em 2em}"), so that rule is the lever for how much
// white space surrounds the text. The setting is in points like LayoutDx and
// FontSize, so it gets the same DPI scaling: mupdf reads a CSS pt as one of the
// units the page size and font are already expressed in.
// 1, 2 or 4 values, meaning what they do in CSS (all / vertical horizontal /
// top right bottom left); anything else is ignored rather than half-applied
static TempStr EbookMarginCssTemp(const Vec<float>* margin, int displayDPI) {
    int n = margin ? len(*margin) : 0;
    if (n != 1 && n != 2 && n != 4) {
        return {};
    }
    TempStr vals;
    for (int i = 0; i < n; i++) {
        float v = (*margin)[i];
        if (!(v >= 0 && v <= 200)) {
            return {};
        }
        TempStr one = fmt("%gpt", DpiScale(v, displayDPI));
        vals = vals ? str::JoinTemp(vals, StrL(" "), one) : one;
    }
    return fmt("@page { margin: %s !important; }\n", vals);
}

// the user CSS we generate from the ebook settings: what the Ebook Settings
// dialog shows in its preview, so the two can't drift apart. The built-in
// img { height: auto } fix (#5805) is not part of it -- that one is ours, not
// something the user configured
TempStr EbookGeneratedCssTemp(Str fontName, const Vec<float>* margin, float lineSpacing, int displayDPI) {
    TempStr res = EbookFontFamilyCssTemp(EbookFontNameFromSetting(fontName));
    TempStr parts[] = {EbookMarginCssTemp(margin, displayDPI), EbookLineSpacingCssTemp(lineSpacing)};
    for (TempStr part : parts) {
        if (!part) {
            continue;
        }
        res = res ? str::JoinTemp(res, part) : part;
    }
    return res;
}

#ifdef DEBUG
bool EngineMupdf_UnitTestEbookLineSpacingCss() {
    return !EbookLineSpacingCssTemp(0) && !EbookLineSpacingCssTemp(0.49f) && !EbookLineSpacingCssTemp(5.01f) &&
           str::Eq(EbookLineSpacingCssTemp(1.5f), StrL("body, body * { line-height: 1.5 !important; }\n"));
}

bool EngineMupdf_UnitTestEbookFontFamilyCss() {
    if (EbookFontFamilyCssTemp({})) {
        return false;
    }
    TempStr css = EbookFontFamilyCssTemp(StrL("Segoe UI"));
    if (!str::Contains(css, StrL("font-family: \"Segoe UI\" !important;"))) {
        return false;
    }
    // the elements publishers actually hang fonts off, and no monospace ones
    return str::Contains(css, StrL("span,")) && str::Contains(css, StrL("div,")) && str::Contains(css, StrL("li,")) &&
           !str::Contains(css, StrL("pre,")) && !str::Contains(css, StrL("code,"));
}
#endif

// the reflow settings that actually reach mupdf, after a document's own
// FileStates -> EBookUI block (if any) has overridden the global EBookUI
// section. A per-document field is "unset" when it's empty or 0, so a document
// can override just the font and inherit the rest (issue #4600)
struct EBookUISettings {
    Str fontName;
    float fontSize;
    const Vec<float>* margin; // not owned; empty or null means unset
    float lineSpacing;
    float layoutDx;
    float layoutDy;
    bool ignoreDocumentCSS;
    Str customCSS;
};

static EBookUISettings MergeEBookUI(const EBookUI* global, const FileEBookUI* perFile) {
    EBookUISettings res{};
    res.fontName = global->fontName;
    res.fontSize = global->fontSize;
    res.margin = global->margin;
    res.lineSpacing = global->lineSpacing;
    res.layoutDx = global->layoutDx;
    res.layoutDy = global->layoutDy;
    res.ignoreDocumentCSS = global->ignoreDocumentCSS;
    res.customCSS = global->customCSS;
    if (!perFile) {
        return res;
    }
    if (perFile->fontName) {
        res.fontName = perFile->fontName;
    }
    if (perFile->fontSize > 0) {
        res.fontSize = perFile->fontSize;
    }
    if (perFile->margin && len(*perFile->margin) > 0) {
        res.margin = perFile->margin;
    }
    if (perFile->lineSpacing > 0) {
        res.lineSpacing = perFile->lineSpacing;
    }
    if (perFile->layoutDx > 0) {
        res.layoutDx = perFile->layoutDx;
    }
    if (perFile->layoutDy > 0) {
        res.layoutDy = perFile->layoutDy;
    }
    // a tri-state: empty inherits, "true" / "false" (or "yes" / "1") override
    // in both directions, so one document can keep the publisher's CSS even
    // when the global setting ignores it
    if (perFile->ignoreDocumentCSS) {
        res.ignoreDocumentCSS = str::EqI(perFile->ignoreDocumentCSS, StrL("true")) ||
                                str::EqI(perFile->ignoreDocumentCSS, StrL("yes")) ||
                                str::Eq(perFile->ignoreDocumentCSS, StrL("1"));
    }
    if (perFile->customCSS) {
        res.customCSS = perFile->customCSS;
    }
    return res;
}

#ifdef DEBUG
bool EngineMupdf_UnitTestEbookMarginCss() {
    Vec<float> m;
    // nothing set, and 3 values (not a CSS margin), leave mupdf's default alone
    if (EbookMarginCssTemp(nullptr, 96) || EbookMarginCssTemp(&m, 96)) {
        return false;
    }
    VecAppend(m, 1);
    VecAppend(m, 2);
    VecAppend(m, 3);
    if (EbookMarginCssTemp(&m, 96)) {
        return false;
    }
    // 0 is a real value: no margin at all
    VecReset(m);
    VecAppend(m, 0);
    if (!str::Eq(EbookMarginCssTemp(&m, 96), StrL("@page { margin: 0pt !important; }\n"))) {
        return false;
    }
    // one value for all four sides, and points, so it scales with the display
    VecReset(m);
    VecAppend(m, 24);
    if (!str::Eq(EbookMarginCssTemp(&m, 96), StrL("@page { margin: 24pt !important; }\n"))) {
        return false;
    }
    if (!str::Eq(EbookMarginCssTemp(&m, 192), StrL("@page { margin: 48pt !important; }\n"))) {
        return false;
    }
    // two and four values pass through in CSS order
    VecReset(m);
    VecAppend(m, 36);
    VecAppend(m, 24);
    if (!str::Eq(EbookMarginCssTemp(&m, 96), StrL("@page { margin: 36pt 24pt !important; }\n"))) {
        return false;
    }
    VecReset(m);
    VecAppend(m, 1);
    VecAppend(m, 2);
    VecAppend(m, 3);
    VecAppend(m, 4);
    if (!str::Eq(EbookMarginCssTemp(&m, 96), StrL("@page { margin: 1pt 2pt 3pt 4pt !important; }\n"))) {
        return false;
    }
    // out of range is ignored rather than clamped
    VecReset(m);
    VecAppend(m, -1);
    if (EbookMarginCssTemp(&m, 96)) {
        return false;
    }
    VecReset(m);
    VecAppend(m, 200.1f);
    return !EbookMarginCssTemp(&m, 96);
}

bool EngineMupdf_UnitTestMergeEBookUI() {
    EBookUI g{};
    g.fontName = StrL("Georgia");
    g.fontSize = 10;
    Vec<float> gMargin;
    VecAppend(gMargin, 24);
    g.margin = &gMargin;
    g.lineSpacing = 1.5f;
    g.layoutDx = 400;
    g.layoutDy = 600;
    g.ignoreDocumentCSS = true;
    g.customCSS = StrL("p { color: red }");

    // no per-document block: the global values, unchanged
    EBookUISettings s = MergeEBookUI(&g, nullptr);
    if (!str::Eq(s.fontName, StrL("Georgia")) || s.fontSize != 10 || !s.ignoreDocumentCSS) {
        return false;
    }
    // an empty block inherits everything
    FileEBookUI f{};
    s = MergeEBookUI(&g, &f);
    if (!str::Eq(s.fontName, StrL("Georgia")) || s.fontSize != 10 || s.margin != &gMargin || s.lineSpacing != 1.5f ||
        s.layoutDx != 400 || s.layoutDy != 600 || !s.ignoreDocumentCSS ||
        !str::Eq(s.customCSS, StrL("p { color: red }"))) {
        return false;
    }
    // set fields win, the rest still inherits
    f.fontName = StrL("Segoe UI");
    f.fontSize = 14;
    Vec<float> fMargin; // this document has no margin at all, the global one has 24pt
    VecAppend(fMargin, 0);
    f.margin = &fMargin;
    f.ignoreDocumentCSS = StrL("false");
    s = MergeEBookUI(&g, &f);
    if (!str::Eq(s.fontName, StrL("Segoe UI")) || s.fontSize != 14 || s.margin != &fMargin || s.lineSpacing != 1.5f) {
        return false;
    }
    // the tri-state can turn the global true back off
    return !s.ignoreDocumentCSS;
}
#endif

// can mupdf turn this font-family into a font? mirrors fz_load_html_font():
// a builtin (base-14 and friends), a system font, or a CSS generic family.
// used to tell the user their EBookUI.FontName didn't take (issue #4600)
static bool EbookFontIsAvailable(fz_context* ctx, Str fontName) {
    const char* name = CStrTemp(fontName);
    if (str::EqI(Str(name), StrL("serif")) || str::EqI(Str(name), StrL("sans-serif")) ||
        str::EqI(Str(name), StrL("monospace"))) {
        return true;
    }
    int size = 0;
    if (fz_lookup_builtin_font(ctx, name, 0, 0, &size)) {
        return true;
    }
    fz_font* font = nullptr;
    fz_try(ctx) {
        font = fz_load_system_font(ctx, name, 0, 0, 0);
    }
    fz_catch(ctx) {
        fz_ignore_error(ctx);
        font = nullptr;
    }
    if (!font) {
        return false;
    }
    fz_drop_font(ctx, font);
    return true;
}

// stm is either freed or retained via _doc
// TODO(port): fz_stream can no-longer be re-opened (fz_clone_stream)
// bool Load(fz_stream* stm, PasswordUI* pwdUI = nullptr);
bool EngineMupdf::LoadFromStream(fz_stream* stm, Str nameHint, PasswordUI* pwdUI) {
    if (!stm) {
        return false;
    }
    // a 3rd-party DLL might have unmasked fp exceptions on this thread, which
    // would crash mupdf on benign NaN comparisons e.g. in pdf_resolve_link_dest()
#if OS_WIN
    MaskFpExceptions();
#endif
    auto* ctx = Ctx();

#if 0
    /* a heuristic. a layout page size for .epub is A5 but that makes a font size too
       large for non-epub files like .txt or .xml, so for those use larger A4 */
    float ldx = layoutA4DxPt;
    float ldy = layoutA4DyPt;
    TempStr ext = path::GetExtTemp(nameHint);
    if (str::EqI(ext, StrL(".epub"))) {
        ldx = layoutA5DxPt;
        ldy = layoutA5DyPt;
    }
#endif

    float ldx = layoutA5DxPt;
    float ldy = layoutA5DyPt;
    float lfontDy = layoutFontEm;
    if (!str::EndsWithI(nameHint, StrL(".epub"))) {
        lfontDy = 8.f;
    }

    // mupdf 1.28 replaced the global fz_set_user_css / fz_set_use_document_css
    // with per-document styling via fz_style_document (applied after the
    // document is opened, before fz_layout_document)
    //
    // Default user CSS: many EPUBs (and other reflow docs) set
    //   img { width: 100%; height: 100%; }
    // which collapses images in MuPDF's reflow layout when the containing
    // block has no fixed height — images vanish or sit on top of text (#5805).
    // Force auto height and a width cap; applied after publisher CSS so it
    // overrides with !important. User CustomCSS is appended after this.
    TempStr userCss = StrL("img { height: auto !important; max-width: 100% !important; }\n");
    int usePublisherCss = 1; // use the document's own (publisher) CSS by default
    Str requestedFontName;   // the font name, checked for existence after layout
    auto* eBookUI = GetEBookUI();
    if (eBookUI) {
        // FileStates -> EBookUI overrides the global section for this document
        EBookUISettings s = MergeEBookUI(eBookUI, GetFileEBookUI(FilePath()));
        // accept any reasonable font size; the old upper bound of 30 made
        // larger sizes silently revert to the default (#2276). 256 is just a
        // sanity cap to reject garbage values.
        if (s.fontSize > 6 && s.fontSize < 256) {
            lfontDy = s.fontSize;
        }
        if (s.layoutDx > 100) {
            ldx = s.layoutDx;
        }
        if (s.layoutDy > 100) {
            ldy = s.layoutDy;
        } else if (gEbookLayoutAspect > 0) {
            // after any LayoutDx override, so the user still sets line length
            ldy = limitValue(ldx * gEbookLayoutAspect, 150.f, 5000.f);
        }
        requestedFontName = EbookFontNameFromSetting(s.fontName);
        TempStr generated = EbookGeneratedCssTemp(s.fontName, s.margin, s.lineSpacing, displayDPI);
        if (generated) {
            userCss = str::JoinTemp(generated, userCss);
        }
        if (s.customCSS) {
            userCss = str::JoinTemp(userCss, s.customCSS);
        }
        usePublisherCss = s.ignoreDocumentCSS ? 0 : 1;
    }
    str::ReplaceWithCopy(&ebookUserCss, userCss);
    ebookPublisherCss = usePublisherCss;
    TempStr themeCss = ReflowDocumentThemeCssTemp();
    if (themeCss) {
        userCss = str::JoinTemp(userCss, themeCss);
    }
    const char* userCssZ = CStrTemp(userCss);

    float dx, dy, fontDy;
    _doc = nullptr;
    fz_archive* dir = nullptr;
    fz_var(dx);
    fz_var(dy);
    fz_var(fontDy);
    fz_var(dir);
    // a synthesized name for the stream (e.g. "<path>.html"), never a directory
    FileType kind = GuessFileTypeFromName(nameHint, true);
    const char* nameHintZ = CStrTemp(nameHint);
    if (kind == FileType::Markdown) {
        TempStr parentDir = path::GetDirTemp(nameHint);
        if (len(parentDir) > 0) {
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
        ebookLayoutW = dx;
        ebookLayoutH = dy;
        ebookLayoutEm = fontDy;
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

    isReflowable = fz_is_document_reflowable(ctx, _doc) != 0;

    // EBookUI.FontName only affects reflowable documents, and a name we can't
    // resolve silently renders in the default font. Note it for the UI (#4600).
    // After fz_layout_document, so a font that was used is already cached.
    if (requestedFontName && isReflowable) {
        if (!EbookFontIsAvailable(ctx, requestedFontName)) {
            ebookFontUnavailable = str::Dup(requestedFontName);
        }
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
#if OS_WIN
        if (!ok) {
            // TODO: this is only part of SASLprep
            TempStr normalized = NormalizeString(pwd, 5 /* NormalizationKC */);
            pwdA = normalized;
            if (pwdA) {
                ok = fz_authenticate_password(ctx, _doc, pwdA.s);
            }
        }
#endif
        // older Acrobat versions seem to have considered passwords to be in codepage 1252
        // note: such passwords aren't portable when stored as Unicode text
#if OS_WIN
        if (!ok && GetACP() != 1252) {
            TempStr pwd_ansi = pwdA;
            TempWStr pwdCp1252 = strconv::StrCPToWStrTemp(pwd_ansi, 1252);
            pwdA = ToUtf8Temp(pwdCp1252);
            ok = fz_authenticate_password(ctx, _doc, pwdA.s);
        }
#endif
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

// Catalog /PageLayout (facing vs book) and /ViewerPreferences /Direction
// (R2L vs L2R). Direction is a document-stated wish: r2lDeclared keeps a
// remembered manga-mode "off" from silently overriding it (issue #2022).
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
        if (str::EndsWith(Str(name), StrL("Right"))) {
            layout.type = PageLayout::Type::Book;
        } else if (str::StartsWith(Str(name), StrL("Two"))) {
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
        if (str::Eq(Str(direction), StrL("R2L"))) {
            layout.r2l = true;
            layout.r2lDeclared = true;
        } else if (str::Eq(Str(direction), StrL("L2R"))) {
            layout.r2lDeclared = true;
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }

    return layout;
}

// reads the four print-related /ViewerPreferences entries from a PDF document.
// returns false for non-PDF engines or when none of the entries are present.
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

    ScopedRecursiveMutex cs(&engine->docLock);

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
        if (str::Eq(Str(dup), StrL("Simplex"))) {
            prefs.hasDuplex = true;
            prefs.duplex = PdfDuplexPref::Simplex;
            found = true;
        } else if (str::Eq(Str(dup), StrL("DuplexFlipShortEdge"))) {
            prefs.hasDuplex = true;
            prefs.duplex = PdfDuplexPref::FlipShortEdge;
            found = true;
        } else if (str::Eq(Str(dup), StrL("DuplexFlipLongEdge"))) {
            prefs.hasDuplex = true;
            prefs.duplex = PdfDuplexPref::FlipLongEdge;
            found = true;
        }
        const char* ps = pdf_to_name(ctx, pdf_dict_gets(ctx, vprefs, "PrintScaling"));
        if (str::Eq(Str(ps), StrL("None"))) {
            prefs.hasPrintScaling = true;
            prefs.printScalingNone = true;
            found = true;
        } else if (str::Eq(Str(ps), StrL("AppDefault"))) {
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
    auto* ctx = e->Ctx();

    ScopedRecursiveMutex scope(&e->docLock);
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
    ScopedRecursiveMutex scope(&e->docLock);

    auto* ctx = e->Ctx();
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
        FzPageInfo* pageInfo = e->pages[i];
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
    if (str::ContainsCharAny(spec, StrL("/\\:"))) {
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
    auto* ctx = Ctx();
    pdfdoc = pdf_specifics(ctx, _doc);
    if (pdfdoc) {
        // allow loading external-file image streams from next to the PDF (#3731)
        pdf_set_load_external_stream_fn(ctx, pdfdoc, EngineMupdfLoadExternalStream, this);
        // records every change so Undo / Redo can step through them. From here
        // on MuPDF throws on a change made outside an operation, so anything
        // that edits the document goes through EngineMupdfBeginOperation()
        // (MuPDF's own annotation setters begin one themselves).
        fz_try(ctx) {
            pdf_enable_journal(ctx, pdfdoc);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("FinishLoading: pdf_enable_journal() failed\n");
        }
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
    // mupdf renders EPUBs but doesn't read the spine's
    // page-progression-direction, so a manga EPUB came out left-to-right
    // (#1264). Read it ourselves.
    if (GuessFileTypeFromName(FilePath()) == FileType::Epub) {
        EpubReadingDirection dir = EpubGetReadingDirection(FilePath());
        preferredLayout.r2lDeclared = dir.declared;
        if (dir.rtl) {
            preferredLayout.r2l = true;
            // right-to-left only shows in a two page spread, so ask for one
            if (preferredLayout.type == PageLayout::Type::Single) {
                preferredLayout.type = PageLayout::Type::Book;
            }
        }
    }
    allowsPrinting = fz_has_permission(ctx, _doc, FZ_PERMISSION_PRINT);
    allowsCopyingText = fz_has_permission(ctx, _doc, FZ_PERMISSION_COPY);

    for (int i = 0; i < pageCount; i++) {
        auto* pi = New<FzPageInfo>(arena);
        VecAppend(pages, pi);
    }
    if (!pdfdoc) {
        FinishNonPDFLoading(this);
        return true;
    }

    ScopedRecursiveMutex scope(&docLock);

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
            logf("cannot find page size for page %d", pageNo);
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
        logf("Couldn't load outline for '%s'\n", FilePath());
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
                    str::StartsWith(Str(pdf_to_name(ctx, intent)), StrL("GTS_PDF"))) {
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

    // when Off, skip the detection pass; the manual toggle command runs it
    // lazily if needed (see EngineMupdfToggleCadEnhance)
    if (GetEngineeringDrawingEnhanceMode() != EngineeringDrawingEnhanceMode::Off) {
        RunCadDetection();
    }

    return true;
}

static NO_INLINE IPageDestination* DestFromAttachment(EngineMupdf* engine, fz_outline* outline) {
    PageDestination* dest = new PageDestination();
    dest->kind = kindDestinationAttachment;
    // WCHAR* path = ToWStr(outline->uri);
    dest->name = str::Dup(Str(outline->title));
    // page is really a stream number
    Str title = outline->title ? Str(outline->title) : StrL("");
    TempStr nameHex = str::MemToHexTemp(title);
    dest->value = str::Dup(fmt("%s:%d:attachname=%s", engine->FilePath(), outline->page.page, nameHex));
    dest->pageNo = outline->page.page;
    return dest;
}

TocItem* EngineMupdf::BuildTocTree(TocItem* parent, fz_outline* outline, int& idCounter, bool isAttachment, int depth) {
    if (depth >= 64) {
        return nullptr;
    }
    TocItem* root = nullptr;
    TocItem* curr = nullptr;

    auto* ctx = Ctx();
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
        // style (bold / italic / color) is filled in by ApplyOutlineStyles()
        item->fontFlags = 0;
        item->pageNo = pageNo;
        ReportIf(!isAttachment && !item->PageNumbersMatch());

        if (outline->down) {
            item->child = BuildTocTree(item, outline->down, idCounter, isAttachment, depth + 1);
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

// Outline entries can ask for a color (/C) and for bold / italic (/F), and
// SumatraPDF draws the bookmarks tree with them. fz_load_outline() throws that
// away: converting the outline iterator into fz_outline nodes copies the title,
// the uri and is_open but not flags / r / g / b, so every entry comes out
// unstyled (regression since we moved to that mupdf API, issue #3560). Walking
// the iterator ourselves gets them back; it visits entries in exactly the order
// fz_load_outline() builds them (item, its children, then the next sibling), so
// the styles line up with the TocItems built from the same outline.
static void ApplyOutlineStyles(fz_context* ctx, fz_outline_iterator* iter, TocItem* item) {
    while (item) {
        fz_outline_item* it = fz_outline_iterator_item(ctx, iter);
        if (!it) {
            return;
        }
        // our kFontBit* are the /F bit numbers (kFontBitItalic is bit 1 of /F,
        // kFontBitBold is bit 2) and mupdf keeps /F as-is, so the low two bits
        // carry over directly. Don't go by fz_outline's FZ_OUTLINE_FLAG_*
        // names, they have bold and italic the other way round from the spec
        item->fontFlags = it->flags & 3;
        // the pdf outline iterator scales /C to 0..255. No /C leaves it at 0,
        // so black means "no color of its own", which is what we want anyway:
        // an explicitly black entry would be invisible in a dark theme and is
        // better drawn in the theme's text color
        u8 r = (u8)it->r, g = (u8)it->g, b = (u8)it->b;
        if (r || g || b) {
            item->color = MkRgb(r, g, b);
        }

        int res = fz_outline_iterator_down(ctx, iter);
        if (res == 0 && item->child) {
            ApplyOutlineStyles(ctx, iter, item->child);
        }
        if (res >= 0) {
            fz_outline_iterator_up(ctx, iter);
        }
        if (fz_outline_iterator_next(ctx, iter) != 0) {
            return;
        }
        item = item->next;
    }
}

static void ApplyOutlineStyles(fz_context* ctx, fz_document* doc, TocItem* first) {
    if (!doc || !first) {
        return;
    }
    fz_outline_iterator* iter = nullptr;
    fz_try(ctx) {
        iter = fz_new_outline_iterator(ctx, doc);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        iter = nullptr;
    }
    if (!iter) {
        return;
    }
    fz_try(ctx) {
        ApplyOutlineStyles(ctx, iter, first);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    fz_drop_outline_iterator(ctx, iter);
}

static void AppendTocChild(TocItem* parent, TocItem* item) {
    item->parent = parent;
    item->next = nullptr;
    if (!parent->child) {
        parent->child = item;
        return;
    }
    TocItem* p = parent->child;
    while (p->next) {
        p = p->next;
    }
    p->next = item;
}

// Numbered heading prefix as in academic papers: "I.", "II.A.", "1.2."
// (sioyek is_string_titlish). Match must start at the beginning of the title.
static int HeadingNumberPrefixLen(Str s) {
    int n = len(s);
    int i = 0;
    bool any = false;
    while (i < n) {
        int start = i;
        while (i < n) {
            char c = s.s[i];
            if ((c >= '0' && c <= '9') || c == 'I' || c == 'V' || c == 'X' || c == 'C') {
                i++;
            } else {
                break;
            }
        }
        if (i == start || i >= n || s.s[i] != '.') {
            break;
        }
        i++;
        any = true;
    }
    if (!any) {
        return 0;
    }
    return i;
}

static bool IsHeadingTitle(Str s) {
    int n = len(s);
    if (n < 6 || n > 160) {
        return false;
    }
    int prefix = HeadingNumberPrefixLen(s);
    if (prefix <= 0) {
        return false;
    }
    int i = prefix;
    while (i < n && s.s[i] == ' ') {
        i++;
    }
    if (i >= n) {
        return false;
    }
    unsigned char c = (unsigned char)s.s[i];
    if (c >= 'a' && c <= 'z') {
        return false;
    }
    return true;
}

// sioyek is_title_parent_of: walk until the parent hits a space. Same title if
// the child hits a space there too (running headers). Else the parent owns
// numbered children such as "II." → "II.A.".
static bool HeadingIsParentOf(Str parent, Str child, bool* sameOut) {
    *sameOut = false;
    int n = std::min(len(parent), len(child));
    for (int i = 0; i < n; i++) {
        if (parent.s[i] == ' ') {
            if (child.s[i] == ' ') {
                *sameOut = true;
                return false;
            }
            return true;
        }
        if (child.s[i] != parent.s[i]) {
            return false;
        }
    }
    return true;
}

static void AppendHeadingLineText(fz_stext_line* line, str::Builder& b) {
    for (fz_stext_char* c = line->first_char; c; c = c->next) {
        int rune = c->c;
        if (rune <= 0 || rune == 0xFFFD) {
            continue;
        }
        bool isWs = rune > 0 && rune <= 0x7f && str::IsWs((char)rune);
        if (isWs) {
            if (len(b) == 0 || b.LastChar() == ' ') {
                continue;
            }
            b.AppendChar(' ');
            continue;
        }
        if (rune < 32) {
            continue;
        }
        char buf[4];
        int n = fz_runetochar(buf, rune);
        if (n > 0) {
            b.Append(Str(buf, n));
        }
    }
}

static constexpr int kMaxGeneratedTocEntries = 400;

// When the PDF has no outline, build one from numbered headings (issue #5724),
// same heuristic as sioyek: a short line starting with "I." / "1.2." / "II.A.".
static TocItem* GenerateTocFromHeadings(EngineMupdf* e, int& idCounter) {
    if (!e || !e->_doc || e->pageCount <= 0) {
        return nullptr;
    }
    auto* ctx = e->Ctx();
    if (!ctx) {
        return nullptr;
    }

    Vec<TocItem*> stack;
    TocItem* first = nullptr;
    TocItem* lastTop = nullptr;
    int nAdded = 0;
    fz_stext_options opts = NewTextPageOptions();

    auto addNode = [&](TocItem* node) {
        bool same = false;
        while (len(stack) > 0 && !HeadingIsParentOf(stack[len(stack) - 1]->title, node->title, &same) && !same) {
            VecRemoveLast(stack);
        }
        if (same) {
            FreeTocItemRec(nullptr, node);
            return;
        }
        nAdded++;
        if (len(stack) == 0) {
            node->parent = nullptr;
            if (lastTop) {
                lastTop->next = node;
            } else {
                first = node;
            }
            lastTop = node;
        } else {
            AppendTocChild(stack[len(stack) - 1], node);
        }
        VecAppend(stack, node);
    };

    auto walkBlocks = [&](auto& self, fz_stext_block* block, int pageNo) -> void {
        while (block && nAdded < kMaxGeneratedTocEntries) {
            if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down) {
                self(self, block->u.s.down->first_block, pageNo);
            } else if (block->type == FZ_STEXT_BLOCK_TEXT) {
                for (fz_stext_line* line = block->u.t.first_line; line && nAdded < kMaxGeneratedTocEntries;
                     line = line->next) {
                    str::Builder b;
                    AppendHeadingLineText(line, b);
                    Str title = b.TakeStr();
                    str::TrimWSInPlace(title, str::TrimOpt::Both);
                    if (IsHeadingTitle(title)) {
                        IPageDestination* dest = NewSimpleDest(pageNo, RectF{0, line->bbox.y0, 0, 0}, 0, {});
                        TocItem* item = NewTocItemWithDestination(nullptr, title, dest);
                        item->pageNo = pageNo;
                        item->id = ++idCounter;
                        item->isOpenDefault = true;
                        addNode(item);
                    }
                    str::Free(title);
                }
            }
            block = block->next;
        }
    };

    TimeStamp t0 = TimeGet();
    for (int i = 0; i < e->pageCount && nAdded < kMaxGeneratedTocEntries; i++) {
        if (AtomicIntGet(&e->headingTocCancel)) {
            break;
        }
        fz_page* page = nullptr;
        fz_stext_page* stext = nullptr;
        fz_var(page);
        fz_var(stext);
        // Take locks per page so a render thread can run between pages. Holding
        // them for the whole document blocked the first page while every page
        // was extracted (annot-stress-99.pdf).
        ScopedRecursiveMutex csPages(&e->pagesLock);
        ScopedMutex csRender(&e->renderLock);
        ScopedRecursiveMutex csDoc(&e->docLock);
        fz_try(ctx) {
            page = fz_load_page(ctx, e->_doc, i);
            stext = fz_new_stext_page_from_page(ctx, page, &opts);
        }
        fz_always(ctx) {
            fz_drop_page(ctx, page);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            stext = nullptr;
        }
        if (stext) {
            walkBlocks(walkBlocks, stext->first_block, i + 1);
            fz_drop_stext_page(ctx, stext);
        }
    }
    if (AtomicIntGet(&e->headingTocCancel)) {
        FreeTocItemRec(nullptr, first);
        return nullptr;
    }
    logf("GenerateTocFromHeadings: %d pages, %d entries, %.1f ms\n", e->pageCount, nAdded, TimeSinceInMs(t0));
    return first;
}

// Swap in a heading-generated tree (plus attachments). Returns the previous
// tree so the caller can TocChanged() before deleting it.
static TocTree* ReplaceTocWithHeadings(EngineMupdf* e, TocItem* headings, int idCounter) {
    if (!e || !headings) {
        return nullptr;
    }
    ScopedRecursiveMutex cs(&e->docLock);
    TocTree* old = e->tocTree;
    e->tocTree = nullptr;
    TocItem* root = headings;
    if (e->attachments) {
        TocItem* att = e->BuildTocTree(nullptr, e->attachments, idCounter, true, 0);
        if (root) {
            root->AddSiblingAtEnd(att);
        } else {
            root = att;
        }
    }
    if (!root) {
        return old;
    }
    TocItem* realRoot = AllocTocItem(nullptr, {}, 0);
    realRoot->child = root;
    e->tocTree = new TocTree(realRoot);
    return old;
}

static void HeadingTocBuildFinished(EngineMupdf* e) {
    TocItem* headings = e->pendingHeadingToc;
    e->pendingHeadingToc = nullptr;
    int idCounter = e->pendingHeadingTocIdCounter;
    TocTree* old = nullptr;
    if (!AtomicIntGet(&e->headingTocCancel) && headings) {
        old = ReplaceTocWithHeadings(e, headings, idCounter);
    } else {
        FreeTocItemRec(nullptr, headings);
    }
    e->headingTocDone = true;
    Func0 cb = e->headingTocDoneCb;
    cb.Call();
    delete old;
    AtomicIntDec(&gDangerousThreadCount);
    e->Release();
}

static void HeadingTocBuildThread(EngineMupdf* e) {
    int idCounter = 0;
    TocItem* headings = GenerateTocFromHeadings(e, idCounter);
    e->ReleaseTextExtractionThreadContext();
    if (AtomicIntGet(&e->headingTocCancel)) {
        FreeTocItemRec(nullptr, headings);
        AtomicIntDec(&gDangerousThreadCount);
        e->Release();
        return;
    }
    e->pendingHeadingToc = headings;
    e->pendingHeadingTocIdCounter = idCounter;
    auto fn = MkFunc0(HeadingTocBuildFinished, e);
    uitask::Post(fn, "HeadingTocBuildFinished");
}

bool EngineMupdf::HasToc() {
    if (tocTree) {
        return true;
    }
    return outline != nullptr || attachments != nullptr;
}

bool EngineMupdf::HeadingTocPending() const {
    return headingTocStarted && !headingTocDone;
}

// Kick off heading extraction on a background thread. DisplayModel starts this
// after load so HasToc()/GetToc() on the UI thread stay cheap (issue #5724
// still fills the sidebar when the scan finishes).
void EngineMupdf::StartHeadingTocIfNeeded() {
    if (outline) {
        return;
    }
    {
        ScopedRecursiveMutex cs(&docLock);
        if (headingTocStarted) {
            return;
        }
        headingTocStarted = true;
    }
    AddRef();
    AtomicIntInc(&gDangerousThreadCount);
    auto fn = MkFunc0(HeadingTocBuildThread, this);
    ThreadHandle th = StartThread(fn, StrL("HeadingToc"));
    if (!th) {
        AtomicIntDec(&gDangerousThreadCount);
        Release();
        headingTocDone = true;
        return;
    }
    SafeCloseThreadHandle(&th);
}

TocTree* EngineMupdf::GetToc() {
    if (tocTree) {
        return tocTree;
    }
    // No DisplayModel (tests, -dump): generate headings now. The UI path starts
    // StartHeadingTocIfNeeded() instead so opening a long document does not
    // freeze the message loop.
    if (!outline && !headingTocStarted) {
        headingTocStarted = true;
        int idCounter = 0;
        TocItem* headings = GenerateTocFromHeadings(this, idCounter);
        ReplaceTocWithHeadings(this, headings, idCounter);
        headingTocDone = true;
    }
    return BuildToc();
}

TocTree* EngineMupdf::BuildToc() {
    int idCounter = 0;

    ScopedRecursiveMutex cs(&docLock);
    if (tocTree) {
        return tocTree;
    }

    TocItem* root = nullptr;
    TocItem* att = nullptr;
    if (outline) {
        root = BuildTocTree(nullptr, outline, idCounter, false, 0);
        ApplyOutlineStyles(Ctx(), _doc, root);
    }
    if (attachments) {
        att = BuildTocTree(nullptr, attachments, idCounter, true, 0);
        if (root) {
            root->AddSiblingAtEnd(att);
        } else {
            root = att;
        }
    }
    if (!root) {
        return nullptr;
    }
    TocItem* realRoot = AllocTocItem(nullptr, {}, 0);
    realRoot->child = root;
    tocTree = new TocTree(realRoot);
    return tocTree;
}

IPageDestination* EngineMupdf::GetNamedDest(Str name) {
    if (!pdfdoc) {
        return nullptr;
    }
    auto* ctx = Ctx();
    IPageDestination* pageDest = nullptr;
    ScopedRecursiveMutex scope2(&docLock);
    TempStr uri = str::JoinTemp(StrL("#nameddest="), name);
    float zoom = 0;
    RectF r;
    int pageNo = ResolveLink(ctx, _doc, uri, nullptr, nullptr, &zoom, &r);
    if (pageNo < 0) {
        return nullptr;
    }

    // kDestUseDefault dx/dy selects the /XYZ path in DisplayModel::ScrollTo
    // (IsEmpty would also work for 0,0 but would treat unspecified as bottom).
    pageDest = NewSimpleDest(pageNo, r, zoom);
    return pageDest;
}

// Resolve a PDF destination (array, name, or string) to a 1-based page number.
// Returns 0 if the dest is missing or cannot be resolved safely.
static int PageNoFromPdfDest(fz_context* ctx, pdf_document* doc, pdf_obj* dest) {
    if (!dest) {
        return 0;
    }
    dest = pdf_resolve_indirect(ctx, dest);
    if (pdf_is_name(ctx, dest) || pdf_is_string(ctx, dest)) {
        dest = pdf_lookup_dest(ctx, doc, dest);
        dest = pdf_resolve_indirect(ctx, dest);
    }
    if (!pdf_is_array(ctx, dest) || pdf_array_len(ctx, dest) < 1) {
        return 0;
    }
    pdf_obj* pageObj = pdf_array_get(ctx, dest, 0);
    if (pdf_is_int(ctx, pageObj)) {
        // PDF destination integers are zero-based page indices
        int n = pdf_to_int(ctx, pageObj);
        return n >= 0 ? n + 1 : 0;
    }
    int n = pdf_lookup_page_number(ctx, doc, pageObj);
    return n >= 0 ? n + 1 : 0;
}

// Catalog /OpenAction → 1-based page, only for safe internal GoTo (issue #1631).
// Rejects URI/Launch/GoToR/JavaScript and other action types that could leave the PDF.
int EngineMupdf::GetOpenActionPageNo() {
    if (!pdfdoc) {
        return 0;
    }
    auto* ctx = Ctx();
    ScopedRecursiveMutex scope(&docLock);

    int pageNo = 0;
    fz_var(pageNo);
    fz_try(ctx) {
        pdf_obj* root = pdf_dict_gets(ctx, pdf_trailer(ctx, pdfdoc), "Root");
        pdf_obj* open = pdf_dict_gets(ctx, root, "OpenAction");
        if (!open) {
            // no open action
        } else if (pdf_is_array(ctx, open)) {
            // legacy: OpenAction is a destination array
            pageNo = PageNoFromPdfDest(ctx, pdfdoc, open);
        } else if (pdf_is_dict(ctx, open)) {
            pdf_obj* s = pdf_dict_get(ctx, open, PDF_NAME(S));
            if (pdf_name_eq(ctx, s, PDF_NAME(GoTo))) {
                pageNo = PageNoFromPdfDest(ctx, pdfdoc, pdf_dict_get(ctx, open, PDF_NAME(D)));
            } else if (pdf_name_eq(ctx, s, PDF_NAME(Named))) {
                // Only absolute page jumps at open; Next/Prev need a current page.
                pdf_obj* n = pdf_dict_get(ctx, open, PDF_NAME(N));
                if (pdf_name_eq(ctx, n, PDF_NAME(FirstPage))) {
                    pageNo = 1;
                } else if (pdf_name_eq(ctx, n, PDF_NAME(LastPage))) {
                    pageNo = pageCount > 0 ? pageCount : 0;
                }
            }
            // deliberately ignore URI, Launch, GoToR, JavaScript, etc.
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        pageNo = 0;
    }
    if (pageNo < 1 || (pageCount > 0 && pageNo > pageCount)) {
        return 0;
    }
    return pageNo;
}

#if 0
IPageDestination* EngineMupdf::GetNamedDest(Str name) {
    if (!pdfdoc) {
        return nullptr;
    }

    ScopedRecursiveMutex scope1(&pagesLock);
    ScopedRecursiveMutex scope2(&docLock);

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
    ScopedRecursiveMutex scope(&pagesLock);
    ReportIf(pageNo < 1 || pageNo > pageCount);
    FzPageInfo* pageInfo = pages[pageNo - 1];
    if (!pageInfo->page || !pageInfo->fullyLoaded) {
        return nullptr;
    }
    return pageInfo;
}

static IPageElement* NewFzComment(Str comment, int pageNo, RectF rect) {
    auto* res = new PageElementComment(comment);
    res->pageNo = pageNo;
    res->rect = rect;
    return res;
}

// An old SumatraPDF round-trip bug grew the line separator in annotation
// contents by one CR on every save: "\r\n" became "\r\r\n", then "\r\r\r\n" and
// so on. GDI draws each of those as its own break, so a short note renders as a
// tall column of blank lines (issue #2873). No producer writes a CR run before a
// LF, so collapse each such run to a single LF. A run of bare CRs is left alone
// (one LF each): that is PDF's own line separator, where "\r\r" really is a
// blank line. Trailing blank lines are dropped either way -- they only make the
// hover tip taller.
static Str NormalizeCommentNewlinesTemp(Str s) {
    if (!str::ContainsChar(s, '\r')) {
        Str res = str::DupTemp(s);
        str::TrimWSInPlace(res, str::TrimOpt::Right);
        return res;
    }
    str::Builder b;
    int n = len(s);
    for (int i = 0; i < n; i++) {
        char c = s.s[i];
        if (c != '\r') {
            b.AppendChar(c);
            continue;
        }
        int j = i;
        while (j < n && s.s[j] == '\r') {
            j++;
        }
        b.AppendChar('\n');
        if (j < n && s.s[j] == '\n') {
            i = j; // the whole CR run plus its LF is one break
        }
    }
    Str res = ToStrTemp(b);
    str::TrimWSInPlace(res, str::TrimOpt::Right);
    return res;
}

// Acrobat hover tooltip for a form field is /TU. pdf_annot_field_label also
// falls back to /T (the field name) and "Unnamed", which are not tooltips.
// must be called inside fz_try
static Str WidgetTooltipTemp(fz_context* ctx, pdf_annot* annot) {
    pdf_obj* tu = pdf_dict_get_inheritable(ctx, pdf_annot_obj(ctx, annot), PDF_NAME(TU));
    if (!tu) {
        return {};
    }
    const char* s = pdf_to_text_string(ctx, tu);
    if (!s || !s[0]) {
        return {};
    }
    return Str(s);
}

// Hover tip for an annotation: author and/or contents (issue #5329).
// FreeText already draws its contents on the page, so the tip is just the author.
// must be called inside fz_try
static IPageElement* MakePdfCommentFromPdfAnnot(fz_context* ctx, int pageNo, pdf_annot* annot) {
    fz_rect rect = pdf_bound_annot(ctx, annot);
    auto tp = pdf_annot_type(ctx, annot);
    Str contents = NormalizeCommentNewlinesTemp(Str(pdf_annot_contents(ctx, annot)));
    Str author;
    if (pdf_annot_has_author(ctx, annot)) {
        author = Str(pdf_annot_author(ctx, annot));
        if (str::IsEmptyOrWhiteSpace(author)) {
            author = {};
        }
    }
    if (str::IsEmptyOrWhiteSpace(contents)) {
        contents = {};
    }

    Str s;
    if (tp == PDF_ANNOT_FREE_TEXT) {
        s = author ? author : StrL("Anonymous");
    } else if (author && contents) {
        s = str::JoinTemp(author, StrL("\n"), contents);
    } else if (contents) {
        s = contents;
    } else {
        s = author;
    }
    if (!s) {
        return nullptr;
    }
    return NewFzComment(s, pageNo, ToRectF(rect));
}

// must be called inside fz_try
static void RebuildCommentsFromAnnotationsInner(fz_context* ctx, pdf_annot* annot, int pageNo,
                                                Vec<IPageElement*>& comments) {
    auto tp = pdf_annot_type(ctx, annot);
    // only used to tell an empty annotation from one with something to show;
    // MakePdfCommentFromPdfAnnot() below re-reads the contents in full
    Str contents = Str(pdf_annot_contents(ctx, annot)); // don't free
    bool isContentsEmpty = !contents;
    Str author;
    if (pdf_annot_has_author(ctx, annot)) {
        author = Str(pdf_annot_author(ctx, annot));
        if (str::IsEmptyOrWhiteSpace(author)) {
            author = {};
        }
    }
    bool isEmpty = isContentsEmpty && !author;

    if (PDF_ANNOT_FILE_ATTACHMENT == tp) {
        pdf_filespec_params fileParams = {};
        pdf_obj* fs = pdf_annot_filespec(ctx, annot);
        int num = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
        pdf_get_filespec_params(ctx, fs, &fileParams);
        const char* attname = fileParams.filename;
        fz_rect rect = pdf_bound_annot(ctx, annot);
        if (len(attname) == 0 || fz_is_empty_rect(rect) || !pdf_is_embedded_file(ctx, fs)) {
            return;
        }

        // logf("attachment: %s, num: %d\n", Str(attname), num);

        auto* dest = new PageDestination();
        dest->kind = kindDestinationLaunchEmbedded;
        dest->value = str::Dup(Str(attname));
        dest->embedObjNum = num;

        auto* el = new PageElementDestination(dest);
        el->pageNo = pageNo;
        el->rect = ToRectF(rect);

        VecAppend(comments, el);
        // TODO: expose /Contents in addition to the file path
        return;
    }

    // Acrobat hover tooltip for a form field is /TU. Widgets live on
    // page->widgets, not page->annots, and pdfcomment/hyperref buttons are
    // usually read-only — skipping those left no tip at all (issue #2083).
    if (tp == PDF_ANNOT_WIDGET) {
        Str tu = WidgetTooltipTemp(ctx, annot);
        if (!tu) {
            return;
        }
        fz_rect rect = pdf_bound_annot(ctx, annot);
        if (fz_is_empty_rect(rect)) {
            return;
        }
        VecAppend(comments, NewFzComment(tu, pageNo, ToRectF(rect)));
        return;
    }

    if (tp == PDF_ANNOT_FREE_TEXT || !isEmpty) {
        auto* comment = MakePdfCommentFromPdfAnnot(ctx, pageNo, annot);
        if (comment) {
            VecAppend(comments, comment);
        }
    }
}

static void RebuildCommentsFromAnnotations(fz_context* ctx, FzPageInfo* pageInfo) {
    DeleteVecMembers(pageInfo->comments);

    Vec<IPageElement*>& comments = pageInfo->comments;

    auto* page = pageInfo->page;
    if (!page) {
        return;
    }
    auto* pdfpage = pdf_page_from_fz_page(ctx, page);
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
    // form widgets are a separate list from markup annotations
    for (annot = pdf_first_widget(ctx, pdfpage); annot; annot = pdf_next_widget(ctx, annot)) {
        fz_try(ctx) {
            RebuildCommentsFromAnnotationsInner(ctx, annot, pageNo, comments);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }

    // re-order list into top-to-bottom order (i.e. last-to-first)
    VecReverse(comments);
}

/* SumatraPDF */
static fz_stext_page* fz_new_stext_page_from_page2(fz_context* ctx, fz_page* page, const fz_stext_options* options,
                                                   fz_cookie* cookie) {
    fz_stext_page* text;
    fz_device* dev = nullptr;

    fz_var(dev);

    if (page == nullptr) return nullptr;

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

// caller must hold pagesLock and renderLock
static FzPageInfo* GetFzPageInfoLocked(EngineMupdf* e, int pageNo, bool loadQuick, fz_cookie* cookie) {
    auto* ctx = e->Ctx();
    // docLock too: loading a page loads its annotations and has MuPDF generate
    // their appearance streams, reading the same pdf objects the UI thread
    // rewrites when an annotation is edited. Without this, editing one while
    // the annotation-loading or heading-TOC thread walks the pages is a
    // use-after-free (and trips mupdf's local_xref_nesting assert).
    ScopedRecursiveMutex docScope(&e->docLock);
    ReportIf(pageNo < 1 || pageNo > e->pageCount);
    if (pageNo < 1 || pageNo > e->pageCount) {
        return nullptr;
    }
    int pageIdx = pageNo - 1;
    FzPageInfo* pageInfo = e->pages[pageIdx];
    if (!pageInfo) {
        return nullptr;
    }

    if (!pageInfo->page) {
        fz_try(ctx) {
            pageInfo->page = fz_load_page(ctx, e->_doc, pageIdx);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }

    fz_page* page = pageInfo->page;
    if (!page) {
        return nullptr;
    }

    if (e->pdfdoc && !pageInfo->annotsLoaded) {
        pageInfo->annotsLoaded = true;
        fz_try(ctx) {
            // null for a page that isn't a pdf_page; pdf_first_widget()
            // dereferences it without checking (pdf_first_annot() doesn't)
            pdf_page* pdfpage = pdf_page_from_fz_page(ctx, pageInfo->page);
            pdf_annot* annot = pdfpage ? pdf_first_annot(ctx, pdfpage) : nullptr;
            while (annot) {
                Annotation* a = MakeAnnotationWrapper(e, annot, pageNo);
                if (a) {
                    VecAppend(pageInfo->annotations, a);
                }
                annot = pdf_next_annot(ctx, annot);
            }
            pdf_annot* widget = pdfpage ? pdf_first_widget(ctx, pdfpage) : nullptr;
            while (widget) {
                Annotation* a = MakeAnnotationWrapper(e, widget, pageNo);
                if (a) {
                    VecAppend(pageInfo->widgets, a);
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
    link = FixupPageLinks(link);
    if (e->pdfdoc) {
        fz_link* btnLinks = nullptr;
        fz_try(ctx) {
            pdf_page* pdfpage = pdf_page_from_fz_page(ctx, page);
            btnLinks = MakePushButtonWidgetLinks(ctx, e->pdfdoc, pdfpage, pageNo);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            btnLinks = nullptr;
        }
        // appended, not prepended: a real /Link annotation covering the same
        // spot is still preferred (see FixupPageLinks)
        fz_link** tail = &link;
        while (*tail) {
            tail = &(*tail)->next;
        }
        *tail = btnLinks;
    }
    pageInfo->retainedLinks = link;
    pdf_page* pdfpage = nullptr;
    if (e->pdfdoc) {
        fz_try(ctx) {
            pdfpage = pdf_page_from_fz_page(ctx, page);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            pdfpage = nullptr;
        }
    }
    while (link) {
        auto* pel = NewLinkDestination(pageNo, ctx, e->_doc, link, nullptr);
        if (pel) {
            // a link that goes somewhere in this document has no URL to show,
            // so show the description the PDF gives it, like other viewers do
            auto* dest = (PageDestinationMupdf*)pel->AsLink();
            if (dest && !PageDestGetValue(dest)) {
                dest->value = PdfLinkContents(ctx, e->pdfdoc, pdfpage, pageNo, link->rect);
            }
            VecAppend(pageInfo->links, pel);
        }
        link = link->next;
    }

    if (e->pdfdoc && pdfpage) {
        fz_try(ctx) {
            AppendJsMenuLinks(ctx, e->pdfdoc, pdfpage, pageNo, pageInfo->links);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }

    if (!stext) {
        return pageInfo;
    }

    if (!e->disableAutoLinks) {
        FzLinkifyPageText(pageInfo, stext);
    }
    FzFindImagePositions(ctx, pageNo, pageInfo->images, stext);
    fz_drop_stext_page(ctx, stext);
    return pageInfo;
}

// like GetFzPageInfo() but fails if we can't acquire locks
// prevents blocking main thread due to render thread keeping the lock
// https://github.com/sumatrapdfreader/sumatrapdf/issues/4145
// https://github.com/sumatrapdfreader/sumatrapdf/issues/4187
FzPageInfo* EngineMupdf::GetFzPageInfoCanFail(int pageNo) {
    if (!pagesLock.TryLock()) {
        return nullptr;
    }
    if (!renderLock.TryLock()) {
        pagesLock.Unlock();
        return nullptr;
    }
    // GetFzPageInfoLocked() needs docLock; take it here too so this stays the
    // "give up rather than block the UI thread" path it exists to be
    if (!docLock.TryLock()) {
        renderLock.Unlock();
        pagesLock.Unlock();
        return nullptr;
    }
    FzPageInfo* res = GetFzPageInfoLocked(this, pageNo, true, nullptr);
    docLock.Unlock();
    renderLock.Unlock();
    pagesLock.Unlock();
    return res;
}

// Maybe: handle FZ_ERROR_TRYLATER, which can happen when parsing from network.
// (I don't think we read from network now).
// Maybe: when loading fully, cache extracted text in FzPageInfo
// so that we don't have to re-do fz_new_stext_page_from_page() when doing search
FzPageInfo* EngineMupdf::GetFzPageInfo(int pageNo, bool loadQuick, fz_cookie* cookie) {
    // TODO: minimize time spent under pagesLock when fully loading
    ScopedRecursiveMutex scope(&pagesLock);
    // page-running operations on this specific page run under per-page lock.
    // pagesLock (held above) serializes concurrent fz_load_page on _doc.
    ScopedMutex ctxScope(&renderLock);
    return GetFzPageInfoLocked(this, pageNo, loadQuick, cookie);
}

RectF EngineMupdf::PageMediabox(int pageNo) {
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo < 1 || pageNo > pageCount) return {};
    FzPageInfo* pi = pages[pageNo - 1];
    return pi->mediabox;
}

// Boxes the page (or an ancestor /Pages node) actually names. Crop/Bleed/Trim/Art
// default to Crop/Media when absent; we skip those so the overlay only draws
// what is in the file (issue #814). Rects are in the same space as PageMediabox.
void EngineMupdf::GetPdfPageBoxes(int pageNo, Vec<PdfPageBox>& out) {
    VecReset(out);
    if (!pdfdoc || pageNo < 1 || pageNo > pageCount) {
        return;
    }
    FzPageInfo* pi = GetFzPageInfo(pageNo, true);
    if (!pi || !pi->page) {
        return;
    }
    fz_context* ctx = Ctx();
    ScopedRecursiveMutex scope(&docLock);
    pdf_page* page = nullptr;
    fz_try(ctx) {
        page = pdf_page_from_fz_page(ctx, pi->page);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return;
    }
    if (!page) {
        return;
    }

    struct BoxSpec {
        pdf_obj* name;
        PdfPageBoxKind kind;
        fz_box_type fzBox;
    };
    const BoxSpec specs[] = {
        {PDF_NAME(MediaBox), PdfPageBoxKind::Media, FZ_MEDIA_BOX},
        {PDF_NAME(CropBox), PdfPageBoxKind::Crop, FZ_CROP_BOX},
        {PDF_NAME(BleedBox), PdfPageBoxKind::Bleed, FZ_BLEED_BOX},
        {PDF_NAME(TrimBox), PdfPageBoxKind::Trim, FZ_TRIM_BOX},
        {PDF_NAME(ArtBox), PdfPageBoxKind::Art, FZ_ART_BOX},
    };
    for (const BoxSpec& spec : specs) {
        pdf_obj* obj = nullptr;
        fz_rect r{};
        fz_try(ctx) {
            obj = pdf_dict_get_inheritable(ctx, page->obj, spec.name);
            if (pdf_is_array(ctx, obj)) {
                r = pdf_bound_page(ctx, page, spec.fzBox);
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            obj = nullptr;
        }
        if (!pdf_is_array(ctx, obj)) {
            continue;
        }
        PdfPageBox box;
        box.kind = spec.kind;
        box.rect = ToRectF(r);
        if (box.rect.IsEmpty()) {
            continue;
        }
        VecAppend(out, box);
    }
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

// Like fz_new_bbox_device(), but bounds what is actually *visible on the page*,
// which is what "Fit Content" needs. Two differences from the mupdf device:
//
//  - every op's bounds are clipped to the page rect before being unioned, so
//    ink drawn outside the page contributes nothing. Print-ready PDFs draw crop
//    and registration marks in the bleed area, outside the media box.
//  - paths are bounded per subpath rather than as one rect. All four corners'
//    crop marks are typically emitted as a single path, so its overall bounds
//    span (and overhang) the whole page; clipping *that* to the page reports a
//    full-page content box and makes Fit Content a no-op (that is what page 2 of
//    a print-ready book PDF looked like: content 500.6x643.6 of a 501.1x643.7
//    page, all of it from two crop-mark paths).
//
// Clip paths are still bounded as a whole: a clip is a region, not ink, and its
// bbox is the conservative thing to intersect subsequent ops against.
constexpr int kContentBboxStackSize = 96;

typedef struct {
    fz_device super;
    fz_rect* result;
    fz_rect pageRect;
    int top;
    fz_rect stack[kContentBboxStackSize];
    // mask content and tiles are ignored
    int ignore;
} fz_content_bbox_device;

static void fz_content_bbox_add_rect(fz_device* dev, fz_rect rect, bool clip) {
    fz_content_bbox_device* d = (fz_content_bbox_device*)dev;

    if (0 < d->top && d->top <= kContentBboxStackSize) {
        rect = fz_intersect_rect(rect, d->stack[d->top - 1]);
    }
    if (!clip && d->top <= kContentBboxStackSize && !d->ignore) {
        // only the part that lands on the page is content. Disjoint rects
        // intersect to an invalid one, which fz_union_rect() ignores - so ink
        // fully outside the page contributes nothing
        *d->result = fz_union_rect(*d->result, fz_intersect_rect(rect, d->pageRect));
    }
    if (clip && ++d->top <= kContentBboxStackSize) {
        d->stack[d->top - 1] = rect;
    }
}

// walks a path and hands each subpath's bounds to the device separately
struct ContentBBoxPathWalk {
    fz_context* ctx;
    fz_device* dev;
    fz_matrix ctm;
    const fz_stroke_state* stroke;
    fz_rect cur;
    bool haveCur;
    // a subpath that is only a moveto draws nothing; fz_bound_path() skips
    // those too ("trailing moves are ignored")
    bool haveSegment;
};

static void fz_content_bbox_walk_flush(ContentBBoxPathWalk* w) {
    bool draws = w->haveCur && w->haveSegment;
    w->haveCur = false;
    w->haveSegment = false;
    if (!draws) {
        return;
    }
    fz_rect r = fz_transform_rect(w->cur, w->ctm);
    if (w->stroke) {
        r = fz_adjust_rect_for_stroke(w->ctx, r, w->stroke, w->ctm);
    }
    fz_content_bbox_add_rect(w->dev, r, false);
}

static void fz_content_bbox_walk_point(ContentBBoxPathWalk* w, float x, float y) {
    if (!w->haveCur) {
        w->cur = fz_make_rect(x, y, x, y);
        w->haveCur = true;
        return;
    }
    w->cur.x0 = std::min(w->cur.x0, x);
    w->cur.y0 = std::min(w->cur.y0, y);
    w->cur.x1 = std::max(w->cur.x1, x);
    w->cur.y1 = std::max(w->cur.y1, y);
}

static void fz_content_bbox_walk_moveto(fz_context*, void* arg, float x, float y) {
    auto* w = (ContentBBoxPathWalk*)arg;
    // a moveto starts a new subpath, so the previous one is complete
    fz_content_bbox_walk_flush(w);
    fz_content_bbox_walk_point(w, x, y);
}

static void fz_content_bbox_walk_lineto(fz_context*, void* arg, float x, float y) {
    auto* w = (ContentBBoxPathWalk*)arg;
    w->haveSegment = true;
    fz_content_bbox_walk_point(w, x, y);
}

// bounding the control points is what fz_bound_path() does too: conservative,
// but it never reports less than the curve covers
static void fz_content_bbox_walk_curveto(fz_context*, void* arg, float x1, float y1, float x2, float y2, float x3,
                                         float y3) {
    auto* w = (ContentBBoxPathWalk*)arg;
    w->haveSegment = true;
    fz_content_bbox_walk_point(w, x1, y1);
    fz_content_bbox_walk_point(w, x2, y2);
    fz_content_bbox_walk_point(w, x3, y3);
}

static void fz_content_bbox_walk_closepath(fz_context*, void*) {
    // the subpath is closed but not yet finished: it stays current until the
    // next moveto (or the end of the path)
}

static void fz_content_bbox_add_path(fz_context* ctx, fz_device* dev, const fz_path* path,
                                     const fz_stroke_state* stroke, fz_matrix ctm) {
    static const fz_path_walker walker = {
        fz_content_bbox_walk_moveto,
        fz_content_bbox_walk_lineto,
        fz_content_bbox_walk_curveto,
        fz_content_bbox_walk_closepath,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };
    ContentBBoxPathWalk w{ctx, dev, ctm, stroke, fz_empty_rect, false, false};
    fz_walk_path(ctx, path, &walker, &w);
    fz_content_bbox_walk_flush(&w);
}

static void fz_content_bbox_fill_path(fz_context* ctx, fz_device* dev, const fz_path* path, int /*evenOdd*/,
                                      fz_matrix ctm, fz_colorspace*, const float*, float, fz_color_params) {
    fz_content_bbox_add_path(ctx, dev, path, nullptr, ctm);
}

static void fz_content_bbox_stroke_path(fz_context* ctx, fz_device* dev, const fz_path* path,
                                        const fz_stroke_state* stroke, fz_matrix ctm, fz_colorspace*, const float*,
                                        float, fz_color_params) {
    fz_content_bbox_add_path(ctx, dev, path, stroke, ctm);
}

static void fz_content_bbox_fill_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm,
                                      fz_colorspace*, const float*, float, fz_color_params) {
    fz_content_bbox_add_rect(dev, fz_bound_text(ctx, text, nullptr, ctm), false);
}

static void fz_content_bbox_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text,
                                        const fz_stroke_state* stroke, fz_matrix ctm, fz_colorspace*, const float*,
                                        float, fz_color_params) {
    fz_content_bbox_add_rect(dev, fz_bound_text(ctx, text, stroke, ctm), false);
}

static void fz_content_bbox_fill_shade(fz_context* ctx, fz_device* dev, fz_shade* shade, fz_matrix ctm, float,
                                       fz_color_params) {
    fz_content_bbox_add_rect(dev, fz_bound_shade(ctx, shade, ctm), false);
}

static void fz_content_bbox_fill_image(fz_context*, fz_device* dev, fz_image*, fz_matrix ctm, float, fz_color_params) {
    fz_content_bbox_add_rect(dev, fz_transform_rect(fz_unit_rect, ctm), false);
}

static void fz_content_bbox_fill_image_mask(fz_context*, fz_device* dev, fz_image*, fz_matrix ctm, fz_colorspace*,
                                            const float*, float, fz_color_params) {
    fz_content_bbox_add_rect(dev, fz_transform_rect(fz_unit_rect, ctm), false);
}

static void fz_content_bbox_clip_path(fz_context* ctx, fz_device* dev, const fz_path* path, int /*evenOdd*/,
                                      fz_matrix ctm, fz_rect /*scissor*/) {
    fz_content_bbox_add_rect(dev, fz_bound_path(ctx, path, nullptr, ctm), true);
}

static void fz_content_bbox_clip_stroke_path(fz_context* ctx, fz_device* dev, const fz_path* path,
                                             const fz_stroke_state* stroke, fz_matrix ctm, fz_rect /*scissor*/) {
    fz_content_bbox_add_rect(dev, fz_bound_path(ctx, path, stroke, ctm), true);
}

static void fz_content_bbox_clip_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm,
                                      fz_rect /*scissor*/) {
    fz_content_bbox_add_rect(dev, fz_bound_text(ctx, text, nullptr, ctm), true);
}

static void fz_content_bbox_clip_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text,
                                             const fz_stroke_state* stroke, fz_matrix ctm, fz_rect /*scissor*/) {
    fz_content_bbox_add_rect(dev, fz_bound_text(ctx, text, stroke, ctm), true);
}

static void fz_content_bbox_clip_image_mask(fz_context*, fz_device* dev, fz_image*, fz_matrix ctm,
                                            fz_rect /*scissor*/) {
    fz_content_bbox_add_rect(dev, fz_transform_rect(fz_unit_rect, ctm), true);
}

static void fz_content_bbox_pop_clip(fz_context*, fz_device* dev) {
    fz_content_bbox_device* d = (fz_content_bbox_device*)dev;
    if (d->top > 0) {
        d->top--;
    }
}

static void fz_content_bbox_begin_mask(fz_context*, fz_device* dev, fz_rect rect, int /*luminosity*/, fz_colorspace*,
                                       const float*, fz_color_params) {
    fz_content_bbox_device* d = (fz_content_bbox_device*)dev;
    fz_content_bbox_add_rect(dev, rect, true);
    d->ignore++;
}

static void fz_content_bbox_end_mask(fz_context*, fz_device* dev, fz_function*) {
    fz_content_bbox_device* d = (fz_content_bbox_device*)dev;
    if (d->ignore > 0) {
        d->ignore--;
    }
}

static void fz_content_bbox_begin_group(fz_context*, fz_device* dev, fz_rect rect, fz_colorspace*, int /*isolated*/,
                                        int /*knockout*/, int /*blendmode*/, float /*alpha*/) {
    fz_content_bbox_add_rect(dev, rect, true);
}

static void fz_content_bbox_end_group(fz_context* ctx, fz_device* dev) {
    fz_content_bbox_pop_clip(ctx, dev);
}

static int fz_content_bbox_begin_tile(fz_context*, fz_device* dev, fz_rect area, fz_rect /*view*/, float /*xstep*/,
                                      float /*ystep*/, fz_matrix ctm, int /*id*/, int /*docId*/) {
    fz_content_bbox_device* d = (fz_content_bbox_device*)dev;
    fz_content_bbox_add_rect(dev, fz_transform_rect(area, ctm), false);
    d->ignore++;
    return 0;
}

static void fz_content_bbox_end_tile(fz_context*, fz_device* dev) {
    fz_content_bbox_device* d = (fz_content_bbox_device*)dev;
    if (d->ignore > 0) {
        d->ignore--;
    }
}

static fz_device* FzNewContentBBoxDevice(fz_context* ctx, fz_rect* result, fz_rect pageRect) {
    fz_content_bbox_device* d = fz_new_derived_device(ctx, fz_content_bbox_device);

    d->super.fill_path = fz_content_bbox_fill_path;
    d->super.stroke_path = fz_content_bbox_stroke_path;
    d->super.clip_path = fz_content_bbox_clip_path;
    d->super.clip_stroke_path = fz_content_bbox_clip_stroke_path;

    d->super.fill_text = fz_content_bbox_fill_text;
    d->super.stroke_text = fz_content_bbox_stroke_text;
    d->super.clip_text = fz_content_bbox_clip_text;
    d->super.clip_stroke_text = fz_content_bbox_clip_stroke_text;

    d->super.fill_shade = fz_content_bbox_fill_shade;
    d->super.fill_image = fz_content_bbox_fill_image;
    d->super.fill_image_mask = fz_content_bbox_fill_image_mask;
    d->super.clip_image_mask = fz_content_bbox_clip_image_mask;

    d->super.pop_clip = fz_content_bbox_pop_clip;

    d->super.begin_mask = fz_content_bbox_begin_mask;
    d->super.end_mask = fz_content_bbox_end_mask;
    d->super.begin_group = fz_content_bbox_begin_group;
    d->super.end_group = fz_content_bbox_end_group;

    d->super.begin_tile = fz_content_bbox_begin_tile;
    d->super.end_tile = fz_content_bbox_end_tile;

    d->result = result;
    d->pageRect = pageRect;
    d->top = 0;
    d->ignore = 0;

    *result = fz_empty_rect;

    return &d->super;
}

RectF EngineMupdf::PageContentBox(int pageNo, RenderTarget /*target*/) {
    auto* ctx = Ctx();

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo) {
        // maybe should return a dummy size. not sure how this
        // will play with layout. The page should fail to render
        // since the doc is broken and page is missing
        return {};
    }

    RectF mediabox = pageInfo->mediabox;

    fz_rect pagerect;
    fz_display_list* keptList = nullptr;
    {
        // Hold per-page lock briefly: page bounds + (re-)acquire cached display list.
        // docLock as well - see the comment in RenderPage: building the list runs
        // the page's annotations, which a concurrent annotation edit can free.
        ScopedMutex scope(&renderLock);
        ScopedRecursiveMutex docScope(&docLock);
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
        dev = FzNewContentBBoxDevice(ctx, &rect, pagerect);
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

static u32 DarkLegacySkipHash(FzPageInfo* pageInfo, float zoom, int rotation) {
    u32 h = PdfDarkModeComputeOptionsHash();
    h = (h * 31) + (u32)(zoom * 1000.f);
    h = (h * 31) + (u32)rotation;
    h = (h * 31) + (u32)GetPreservePdfImagesMinSize();
    h = (h * 31) + (u32)GetPreservePdfImagesInDarkMode();
    h = (h * 31) + (u32)(pageInfo ? len(pageInfo->images) : 0);
    return h;
}

// Illustrated pages often contain many small content-stream images alongside
// one main artwork. Preserving all of them leaves patchy gaps that get
// dark-recolored. Keep only the largest preserve region per page (#5806).
static void DarkLegacySkipKeepLargestArtwork(FzPageInfo* pageInfo) {
    Vec<Rect>& skipRects = pageInfo->darkLegacySkipDevAbs;
    if (len(skipRects) <= 1) {
        return;
    }
    int bestIdx = 0;
    i64 bestArea = 0;
    for (int i = 0; i < len(skipRects); i++) {
        i64 a = (i64)skipRects[i].dx * skipRects[i].dy;
        if (a > bestArea) {
            bestArea = a;
            bestIdx = i;
        }
    }
    Rect keep = skipRects[bestIdx];
    VecClear(skipRects);
    VecAppend(skipRects, keep);
    pageInfo->darkLegacyArtworkPageBottom = 0.f;
}

static int DarkLegacyTileFindRoot(Vec<int>& parent, int i) {
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

// Some PDFs slice a single illustration into a grid of image tiles. Judged one
// at a time each tile is just a small picture: the page-dominance rule never
// fires, the decorative-strip rule throws away the thin edge pieces, and
// DarkLegacySkipKeepLargestArtwork keeps only one tile of the set. Group tiles
// that touch into one region and let the rest of the code treat that region as
// the image.
//
// A group only counts as a tiling if its tiles actually fill its bounding box.
// Without that check, scattered figures on a busy page would chain together
// into one huge region covering unrelated page content.
//
// Fills groupOf (one entry per rect, indexing groupRect) and groupRect. Rects
// that don't group land in a group of their own, so callers see no difference
// from the ungrouped case.
static void DarkLegacyGroupImageTiles(const Vec<RectF>& rects, float tol, Vec<int>& groupOf, Vec<RectF>& groupRect) {
    VecClear(groupOf);
    VecClear(groupRect);
    int n = len(rects);
    Vec<int> parent;
    for (int i = 0; i < n; i++) {
        VecAppend(parent, i);
    }
    for (int i = 0; i < n; i++) {
        if (rects[i].IsEmpty()) {
            continue;
        }
        RectF grown = rects[i];
        grown.Inflate(tol, tol);
        for (int j = i + 1; j < n; j++) {
            if (rects[j].IsEmpty() || grown.Intersect(rects[j]).IsEmpty()) {
                continue;
            }
            int ri = DarkLegacyTileFindRoot(parent, i);
            int rj = DarkLegacyTileFindRoot(parent, j);
            if (ri != rj) {
                parent[ri] = rj;
            }
        }
    }

    Vec<RectF> bbox;
    Vec<float> tileArea;
    Vec<int> nTiles;
    for (int i = 0; i < n; i++) {
        VecAppend(bbox, RectF());
        VecAppend(tileArea, 0.f);
        VecAppend(nTiles, 0);
    }
    for (int i = 0; i < n; i++) {
        if (rects[i].IsEmpty()) {
            continue;
        }
        int r = DarkLegacyTileFindRoot(parent, i);
        bbox[r] = nTiles[r] == 0 ? rects[i] : bbox[r].Union(rects[i]);
        tileArea[r] += rects[i].dx * rects[i].dy;
        nTiles[r]++;
    }
    for (int i = 0; i < n; i++) {
        if (nTiles[i] < 2) {
            continue;
        }
        float bboxArea = bbox[i].dx * bbox[i].dy;
        if (bboxArea <= 0.f || tileArea[i] < bboxArea * 0.85f) {
            nTiles[i] = 0; // scattered images, not a tiling - dissolve the group
        }
    }

    Vec<int> rootToGroup;
    for (int i = 0; i < n; i++) {
        VecAppend(rootToGroup, -1);
    }
    for (int i = 0; i < n; i++) {
        int r = DarkLegacyTileFindRoot(parent, i);
        if (rects[i].IsEmpty() || nTiles[r] < 2) {
            VecAppend(groupOf, len(groupRect));
            VecAppend(groupRect, rects[i]);
            continue;
        }
        if (rootToGroup[r] < 0) {
            rootToGroup[r] = len(groupRect);
            VecAppend(groupRect, bbox[r]);
        }
        VecAppend(groupOf, rootToGroup[r]);
    }
}

// find the images on the page whose colors the dark-mode bitmap recolor
// should preserve (photos, artwork) and cache their absolute device rects
static void BuildPageDarkLegacySkipRects(EngineMupdf* engine, FzPageInfo* pageInfo, float zoom, int rotation,
                                         u32 hash) {
    VecClear(pageInfo->darkLegacySkipDevAbs);
    pageInfo->darkLegacyArtworkPageBottom = 0.f;
    pageInfo->darkLegacySkipHash = hash;
    pageInfo->darkLegacySkipZoom = zoom;
    pageInfo->darkLegacySkipRotation = rotation;

    if (!pageInfo->page || len(pageInfo->images) == 0) {
        return;
    }
    fz_context* ctx = engine->Ctx();
    fz_page* page = pageInfo->page;
    fz_matrix ctm = engine->viewctm(page, zoom, rotation);
    int minDx = GetPreservePdfImagesMinSize();
    int minDy = minDx;

    RectF pageBounds = pageInfo->mediabox;
    if (pageBounds.IsEmpty()) {
        pageBounds = ToRectF(fz_bound_page(ctx, page));
    }
    float pageArea = pageBounds.dx * pageBounds.dy;
    if (pageArea <= 0.f) {
        pageArea = 1.f;
    }

    int nImages = len(pageInfo->images);
    Vec<RectF> imgPageRects;
    Vec<RectF> imgOnPageRects;
    for (int imgIdx = 0; imgIdx < nImages; imgIdx++) {
        RectF imgPage = ToRectF(pageInfo->images[imgIdx]->rect);
        fz_image* image = FzGetKeptPageImage(ctx, pageInfo, imgIdx);
        if (image && image->w > 0 && image->h > 0) {
            imgPage = PdfDarkModeClampImagePageRect(imgPage, image->w, image->h);
        } else {
            imgPage = PdfDarkModeCapUnknownImagePageRect(imgPage, pageBounds.dy);
        }
        if (image) {
            fz_drop_image(ctx, image);
        }
        VecAppend(imgPageRects, imgPage);
        VecAppend(imgOnPageRects, imgPage.Intersect(pageBounds));
    }
    // tiles of one sliced illustration are judged as the region they form
    float tileTol = std::max(1.0f, std::min(pageBounds.dx, pageBounds.dy) * 0.005f);
    Vec<int> groupOf;
    Vec<RectF> groupRect;
    DarkLegacyGroupImageTiles(imgOnPageRects, tileTol, groupOf, groupRect);
    Vec<int> groupAppended;
    for (int i = 0; i < len(groupRect); i++) {
        VecAppend(groupAppended, 0);
    }

    for (int imgIdx = 0; imgIdx < nImages; imgIdx++) {
        RectF imgPage = imgPageRects[imgIdx];
        int groupIdx = groupOf[imgIdx];
        RectF imgOnPage = groupRect[groupIdx];
        fz_image* image = FzGetKeptPageImage(ctx, pageInfo, imgIdx);
        float coverage = (imgOnPage.dx * imgOnPage.dy) / pageArea;
        if (PdfDarkModePageDominantImageRecolors(ctx, image, coverage)) {
            // full-bleed backgrounds / scans recolor with the page; artwork
            // that happens to fill the page does not
            if (image) {
                fz_drop_image(ctx, image);
            }
            continue;
        }
        if (PdfDarkModeIsDecorativeStripImage(imgOnPage, pageBounds)) {
            if (image) {
                fz_drop_image(ctx, image);
            }
            continue;
        }
        fz_irect fullDev = fz_round_rect(fz_transform_rect(ToFzRect(imgPage), ctm));
        int fullDx = fullDev.x1 - fullDev.x0;
        int fullDy = fullDev.y1 - fullDev.y0;
        if (fullDx < minDx || fullDy < minDy) {
            if (image) {
                fz_drop_image(ctx, image);
            }
            continue;
        }
        if (!image) {
            continue;
        }
        if (!PdfDarkModeShouldPreserveEmbeddedImageRect(ctx, image, coverage, fullDx, fullDy)) {
            fz_drop_image(ctx, image);
            continue;
        }
        // Wide bboxes often span a layout column; only preserve if clearly a dark painting.
        if (imgOnPage.dx > pageBounds.dx * 0.44f && !PdfDarkModeImageLooksLikeDarkArtwork(ctx, image, coverage)) {
            fz_drop_image(ctx, image);
            continue;
        }
        fz_irect dev = fz_round_rect(fz_transform_rect(ToFzRect(imgOnPage), ctm));
        Rect r(dev.x0, dev.y0, dev.x1 - dev.x0, dev.y1 - dev.y0);
        // tiles of one sliced illustration share a region - append it once
        if (!r.IsEmpty() && !groupAppended[groupIdx]) {
            groupAppended[groupIdx] = 1;
            VecAppend(pageInfo->darkLegacySkipDevAbs, r);
            float bottom = imgOnPage.y + imgOnPage.dy;
            pageInfo->darkLegacyArtworkPageBottom = std::max(bottom, pageInfo->darkLegacyArtworkPageBottom);
        }
        if (image) {
            fz_drop_image(ctx, image);
        }
    }
    DarkLegacySkipKeepLargestArtwork(pageInfo);
}

void EngineMupdf::GetBitmapRecolorSkipRects(int pageNo, float zoom, int rotation, const RectF& renderPageRect,
                                            Size bmpSize, Vec<Rect>& skipRects) {
    VecClear(skipRects);
    if (renderPageRect.IsEmpty() || bmpSize.dx <= 0 || bmpSize.dy <= 0) {
        return;
    }
    // Everything below runs pages through mupdf (FzCollectImagesFromPageContent
    // and, via FzGetKeptPageImage, stext) and mutates shared FzPageInfo state,
    // so it must hold the same locks as every other page-running path. Several
    // render threads render tiles of the same document at once; without this,
    // two of them ran fz_run_page on one fz_document concurrently and corrupted
    // the shared content-stream filter state (crash in next_endstream/memcpy).
    ScopedRecursiveMutex pagesScope(&pagesLock);
    ScopedMutex renderScope(&renderLock);
    // docLock as well: running a page reads its annotations, which a concurrent
    // annotation edit on the UI thread frees
    ScopedRecursiveMutex docScope(&docLock);

    FzPageInfo* pageInfo = GetFzPageInfoLocked(this, pageNo, false, nullptr);
    if (!pageInfo || !pageInfo->page) {
        return;
    }
    // only called when the recolor pass wants to preserve images, so always
    // worth collecting content-stream images the text extractor didn't see
    if (!pageInfo->contentImagesCollected) {
        fz_context* ctx = Ctx();
        FzCollectImagesFromPageContent(ctx, pageNo, pageInfo, pageInfo->page, nullptr);
        pageInfo->contentImagesCollected = true;
        pageInfo->darkLegacySkipHash = 0;
    }
    if (len(pageInfo->images) == 0) {
        return;
    }

    u32 hash = DarkLegacySkipHash(pageInfo, zoom, rotation);
    if (pageInfo->darkLegacySkipHash != hash || pageInfo->darkLegacySkipZoom != zoom ||
        pageInfo->darkLegacySkipRotation != rotation) {
        BuildPageDarkLegacySkipRects(this, pageInfo, zoom, rotation, hash);
    }

    // Text/layout tiles below the artwork band always recolor uniformly.
    if (pageInfo->darkLegacyArtworkPageBottom > 0.f && renderPageRect.y >= pageInfo->darkLegacyArtworkPageBottom) {
        return;
    }

    fz_page* page = pageInfo->page;
    fz_rect pRect = ToFzRect(renderPageRect);
    fz_matrix ctm = viewctm(page, zoom, rotation);
    fz_irect ibounds = fz_round_rect(fz_transform_rect(pRect, ctm));

    Rect tileAbs(ibounds.x0, ibounds.y0, ibounds.x1 - ibounds.x0, ibounds.y1 - ibounds.y0);
    for (Rect& skipAbs : pageInfo->darkLegacySkipDevAbs) {
        Rect clipped = skipAbs.Intersect(tileAbs);
        if (clipped.IsEmpty()) {
            continue;
        }
        Rect local(clipped.x - ibounds.x0, clipped.y - ibounds.y0, clipped.dx, clipped.dy);
        local.Inflate(3, 3);
        local = local.Intersect(Rect(0, 0, bmpSize.dx, bmpSize.dy));
        if (!local.IsEmpty()) {
            VecAppend(skipRects, local);
        }
    }
}

bool EngineMupdf::CadEnhanceActive() const {
    if (!pdfdoc) {
        return false;
    }
    CadDetectResult detect;
    detect.enable = cadDetectEnable;
    detect.score = cadDetectScore;
    return CadEnhanceEnabledForEngine(detect, cadEnhanceOverride);
}

// Analyze the document once for CAD/engineering-drawing content. Caller must
// hold docLock (or own the document exclusively, as during FinishLoading).
void EngineMupdf::RunCadDetection() {
    if (!pdfdoc || cadDetectDone) {
        return;
    }
    CadDetectResult res = DetectCadPdf(Ctx(), pdfdoc);
    cadDetectEnable = res.enable;
    cadDetectScore = res.score;
    cadRasterDominant = res.rasterDominant;
    cadHairlineVector = res.hairlineVector;
    cadDetectDone = true;
    if (cadDetectEnable) {
        logf("CAD enhance detect: score=%d reason=%s raster=%d hairline=%d\n", cadDetectScore,
             Str(CadEnhanceReasonName(res.reason)), (int)cadRasterDominant, (int)cadHairlineVector);
    } else if (cadDetectScore >= 30) {
        logf("CAD enhance not enabled: score=%d hairline=%d (auto threshold 60, or metadata+45)\n", cadDetectScore,
             (int)cadHairlineVector);
    }
}

// First toggle flips away from the current effective state; after that it
// alternates between forced on and forced off.
void EngineMupdf::ToggleCadEnhanceOverride() {
    if (cadEnhanceOverride == CadEnhanceOverride::Unset) {
        cadEnhanceOverride = CadEnhanceActive() ? CadEnhanceOverride::ForceOff : CadEnhanceOverride::ForceOn;
    } else if (cadEnhanceOverride == CadEnhanceOverride::ForceOn) {
        cadEnhanceOverride = CadEnhanceOverride::ForceOff;
    } else if (cadEnhanceOverride == CadEnhanceOverride::ForceOff) {
        cadEnhanceOverride = CadEnhanceOverride::ForceOn;
    }
}

// Transparent backdrop: leave unpainted samples at alpha 0 so the canvas
// checkerboard (CmdToggleTransparencyGrid) shows through (issue #1809).
static void ClearRenderedPagePixmap(fz_context* ctx, fz_pixmap* pix, const RenderPageArgs& args, bool objectLevelDark) {
    if (args.transparentBackdrop) {
        fz_clear_pixmap(ctx, pix);
        return;
    }
    if (objectLevelDark && args.darkProfile) {
        PdfDarkModeClearPixmapToThemeBackground(ctx, pix, args.darkProfile->palette);
        return;
    }
    fz_clear_pixmap_with_value(ctx, pix, 0xff);
}

static void MarkTransparentBackdropPixmap(Pixmap* pixmap, bool transparentBackdrop) {
    if (pixmap && transparentBackdrop) {
        pixmap->hasAlpha = true;
        pixmap->premultiplied = true;
    }
}

Pixmap* EngineMupdf::RenderPage(RenderPageArgs& args) {
    auto* ctx = Ctx();
    auto pageNo = args.pageNo;

    fz_cookie* fzcookie = nullptr;
    FitzAbortCookie* cookie = nullptr;
    if (args.cookie_out) {
        cookie = new FitzAbortCookie();
        *args.cookie_out = cookie;
        fzcookie = (fz_cookie*)cookie->GetData();
    }

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false, fzcookie);
    if (!pageInfo) {
        return nullptr;
    }
    // Do not keep pageInfo->page across the gap before renderLock: theme
    // restyle (ApplyReflowThemeCss) drops fz_page under that lock.
    fz_page* page = pageInfo->page;
    if (!page) {
        return nullptr;
    }

    // AA level is per-thread-context state since Ctx() clones; no lock needed.
    if (disableAntiAlias) {
        fz_set_aa_level(ctx, 0);
    } else {
        // 8 seems to be the default
        fz_set_aa_level(ctx, 8);
    }

    auto* pageRect = args.pageRect;
    auto zoom = args.zoom;
    auto rotation = args.rotation;

    // like the AA level, min line width is per-thread-context state
    CadMinLineWidthScope cadMinLineWidth(ctx, zoom, CadEnhanceActive(), cadHairlineVector);

    // The "View" rendering (no Print, no hideAnnotations) is what
    // fz_new_display_list_from_page produces; safe to cache and re-run lock-free.
    bool useCache = (args.target == RenderTarget::View) && !hideAnnotations;

    fz_rect pRect;
    fz_matrix ctm;
    fz_irect ibounds;
    fz_display_list* keptList = nullptr;

    {
        // Hold per-page lock while we touch the page (bounds, optional list build).
        // docLock too: building the list runs the page *and its annotations*, and
        // an annotation edit on the UI thread (pdf_create_annot / pdf_update_annot,
        // which hold docLock) frees the pdf objects we'd be reading -- crash in
        // pdf_annot_flags on a freed annot dict.
        ScopedMutex cs(&renderLock);
        ScopedRecursiveMutex docScope(&docLock);

        page = pageInfo->page;
        if (!page) {
            return nullptr;
        }

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
    Pixmap* pixmap = nullptr;

    fz_var(dev);
    fz_var(pix);
    fz_var(pixmap);

    if (keptList) {
        // Display-list replay still decodes shared images (JBIG2 etc.) under
        // the hood, and mupdf's image store races on concurrent decode of the
        // same image -- crashes seen in template_image_compose_opt with use-
        // after-free. Hold renderLock to serialize.
        ScopedMutex rls(&renderLock);
        fz_try(ctx) {
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            bool objectLevelDark = args.darkProfile && DarkModeProfileUsesObjectLevel(args.darkProfile);
            ClearRenderedPagePixmap(ctx, pix, args, objectLevelDark);
            dev = fz_new_draw_device(ctx, ctm, pix);
            if (disableAntiAlias) {
                fz_enable_device_hints(ctx, dev, FZ_DONT_INTERPOLATE_IMAGES);
            }
            DarkModeReplayState replayState{};
            if (objectLevelDark && pdfdoc) {
                DarkModePageAnalysis* analysis =
                    PdfDarkModeGetOrBuildAnalysis(ctx, pageInfo, keptList, args.darkProfile->hash, darkModeEngineCache);
                if (analysis) {
                    dev = PdfDarkModeWrapDevice(ctx, dev, analysis, &args.darkProfile->palette, &replayState,
                                                darkModeEngineCache, args.darkProfile->hash,
                                                args.darkProfile->debugOverlay);
                }
            }
            if (CadEnhanceActive()) {
                CadEnhanceRenderOpts opts;
                opts.zoom = zoom;
                opts.hairlineVector = cadHairlineVector;
                dev = PdfCadEnhanceWrapDevice(ctx, dev, opts);
            }
            fz_run_display_list(ctx, keptList, dev, fz_identity, pRect, fzcookie);
            fz_close_device(ctx, dev);
            if (CadEnhanceActive() && cadRasterDominant) {
                PdfCadEnhancePixmap(ctx, pix, zoom, true);
            }
            pixmap = NewPixmapFromFzPixmap(ctx, pix, args.transparentBackdrop);
            MarkTransparentBackdropPixmap(pixmap, args.transparentBackdrop);
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
            FreePixmap(pixmap);
            return {};
        }
        return pixmap;
    }

    // Fallback: Print or hideAnnotations (each needs different content/usage,
    // not what the cached display list captured), or display-list construction
    // failed. Run the page directly under per-page lock.
    ScopedMutex cs(&renderLock);

    page = pageInfo->page;
    if (!page) {
        return nullptr;
    }

    Str usage = (args.target == RenderTarget::Print) ? StrL("Print") : StrL("View");
    const char* usageZ = CStrTemp(usage);

    pdf_page* pdfpage = nullptr;
    fz_var(pdfpage);
    if (pdfdoc) {
        fz_try(ctx) {
            pdfpage = pdf_page_from_fz_page(ctx, page);
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            ClearRenderedPagePixmap(ctx, pix, args, false);
            dev = fz_new_draw_device(ctx, ctm, pix);
            if (disableAntiAlias) {
                fz_enable_device_hints(ctx, dev, FZ_DONT_INTERPOLATE_IMAGES);
            }
            if (hideAnnotations) {
                pdf_run_page_contents_with_usage(ctx, pdfpage, dev, fz_identity, usageZ, fzcookie);
                pdf_run_page_widgets_with_usage(ctx, pdfpage, dev, fz_identity, usageZ, fzcookie);
            } else {
                pdf_run_page_with_usage(ctx, pdfpage, dev, fz_identity, usageZ, fzcookie);
            }
            fz_close_device(ctx, dev);
            if (CadEnhanceActive() && cadRasterDominant) {
                PdfCadEnhancePixmap(ctx, pix, zoom, true);
            }
            pixmap = NewPixmapFromFzPixmap(ctx, pix, args.transparentBackdrop);
            MarkTransparentBackdropPixmap(pixmap, args.transparentBackdrop);
        }
        fz_always(ctx) {
            if (dev) {
                fz_drop_device(ctx, dev);
            }
            fz_drop_pixmap(ctx, pix);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            FreePixmap(pixmap);
            return {};
        }
    } else {
        fz_try(ctx) {
            pix = fz_new_pixmap_with_bbox(ctx, csRgb, ibounds, nullptr, 1);
            ClearRenderedPagePixmap(ctx, pix, args, false);
            dev = fz_new_draw_device(ctx, ctm, pix);
            if (disableAntiAlias) {
                fz_enable_device_hints(ctx, dev, FZ_DONT_INTERPOLATE_IMAGES);
            }
            fz_run_page_contents(ctx, page, dev, fz_identity, nullptr);
            fz_close_device(ctx, dev);
            fz_drop_device(ctx, dev);
            pixmap = NewPixmapFromFzPixmap(ctx, pix, args.transparentBackdrop);
            MarkTransparentBackdropPixmap(pixmap, args.transparentBackdrop);
        }
        fz_always(ctx) {
            fz_drop_pixmap(ctx, pix);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            FreePixmap(pixmap);
            return {};
        }
    }

    return pixmap;
}

// don't delete the result
IPageElement* EngineMupdf::GetElementAtPos(int pageNo, PointF pt) {
    FzPageInfo* pageInfo = GetFzPageInfoCanFail(pageNo);
    return FzGetElementAtPos(pageInfo, pt);
}

// TOOD: optimize by returning reference or pointer so that
// we don't have to re-create the Vec every time
Vec<IPageElement*> EngineMupdf::GetElements(int pageNo) {
    auto* pageInfo = GetFzPageInfoFast(pageNo);
    if (!pageInfo) {
        return Vec<IPageElement*>();
    }

    BuildElementsInfo(pageInfo);
    return pageInfo->allElements;
}

// The UI thread draws link boxes from here on every repaint, and pagesLock can
// be held for the length of an image decode by a render thread that is itself
// queued on renderLock. Skip the decoration for this paint rather than freeze
// the window; the next repaint draws it.
bool EngineMupdf::TryGetElements(int pageNo, Vec<IPageElement*>* out) {
    *out = Vec<IPageElement*>();
    if (!pagesLock.TryLock()) {
        return false;
    }
    ReportIf(pageNo < 1 || pageNo > pageCount);
    if (pageNo >= 1 && pageNo <= pageCount) {
        FzPageInfo* pageInfo = pages[pageNo - 1];
        if (pageInfo && pageInfo->page && pageInfo->fullyLoaded) {
            BuildElementsInfo(pageInfo);
            *out = pageInfo->allElements;
        }
    }
    pagesLock.Unlock();
    return true;
}

static void HandleLinkMupdf(EngineMupdf* e, IPageDestination* dest, ILinkHandler* linkHandler) {
    ReportIf(kindDestinationMupdf != dest->GetKind());
    PageDestinationMupdf* link = (PageDestinationMupdf*)dest;
    if (!link->outline && !link->link) {
        ReportIf(true);
        return;
    }
    Str uri = link->outline ? Str(link->outline->uri) : Str(link->link->uri);
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
    ScopedRecursiveMutex csPages(&e->pagesLock);
    ScopedRecursiveMutex cs(&e->docLock);

    int pageNo = -1;
    fz_link_dest ldest{};
    auto* ctx = e->Ctx();
    fz_var(pageNo);
    fz_try(ctx) {
        ldest = fz_resolve_link_dest(ctx, e->_doc, CStrTemp(uri));
        pageNo = fz_page_number_from_location(ctx, e->_doc, ldest.loc);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("HandleLinkMupdf: fz_resolve_link() for '%s' failed\n", uri);
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

    // Adobe /Fit, /FitH, /FitV, /FitB*, /XYZ, /FitR → zoom modes + scroll (issue #5828)
    RectF r;
    float zoom = 0.f;
    DestFromFzLinkDest(ldest, &r, &zoom);
    linkHandler->ScrollTo(pageNo + 1, r, zoom);
}

bool EngineMupdf::HandleLink(IPageDestination* dest, ILinkHandler* linkHandler) {
    if (!dest || !linkHandler) {
        return false;
    }
    Kind k = dest->GetKind();
    if (k == kindDestinationMupdf) {
        HandleLinkMupdf(this, dest, linkHandler);
        return true;
    }
    linkHandler->GotoLink(dest);
    return true;
}

RenderedBitmap* EngineMupdf::GetImageForPageElement(IPageElement* ipel) {
#if OS_WIN
    ReportIf(kindPageElementImage != ipel->GetKind());
    auto* pel = (PageElementImage*)ipel;
    auto r = pel->rect;
    int pageNo = pel->pageNo;
    int imageID = pel->imageID;
    return GetPageImage(pageNo, r, imageID);
#else
    (void)ipel;
    return nullptr;
#endif
}

// PDF-embedded CMYK JPEG uses PDF polarity (0 = no ink). A standalone JPEG
// uses the Adobe/Photoshop convention (0 = full ink). Dumping the DCT stream
// as a .jpg therefore looks inverted. Re-encode from the decoded pixmap with
// invert_cmyk so Save Image keeps CMYK and displays correctly.
static Str StandaloneJpegFromPdfCmykImage(fz_context* ctx, fz_image* image) {
    fz_pixmap* pix = nullptr;
    fz_pixmap* deviceCmyk = nullptr;
    fz_buffer* buf = nullptr;
    Str result;
    fz_var(pix);
    fz_var(deviceCmyk);
    fz_var(buf);

    fz_try(ctx) {
        pix = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
        if (pix && pix->colorspace && fz_colorspace_is_cmyk(ctx, pix->colorspace) && !pix->alpha && pix->s == 0) {
            fz_pixmap* src = pix;
            if (pix->colorspace != fz_device_cmyk(ctx)) {
                deviceCmyk =
                    fz_convert_pixmap(ctx, pix, fz_device_cmyk(ctx), nullptr, nullptr, fz_default_color_params, 1);
                src = deviceCmyk;
            }
            if (src) {
                buf = fz_new_buffer_from_pixmap_as_jpeg(ctx, src, fz_default_color_params, 95, 1);
                unsigned char* data = nullptr;
                size_t n = fz_buffer_storage(ctx, buf, &data);
                if (data && n > 0 && n <= (size_t)INT_MAX) {
                    result = str::Dup(Str((char*)data, (int)n));
                }
            }
        }
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buf);
        fz_drop_pixmap(ctx, deviceCmyk);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        str::Free(result);
        result = {};
    }
    return result;
}

// JPEG/PNG/GIF/BMP/TIFF streams that are already a complete file. Flate-raw
// samples, JPEG2000, JBIG2, and images with a decode/mask are not. CMYK JPEG
// is re-encoded (see StandaloneJpegFromPdfCmykImage).
Str EngineMupdf::GetImageDataForPageElement(IPageElement* ipel) {
    if (!ipel || ipel->GetKind() != kindPageElementImage) {
        return {};
    }
    auto* pel = (PageElementImage*)ipel;
    FzPageInfo* pageInfo = GetFzPageInfo(pel->pageNo, false);
    if (!pageInfo || !pageInfo->page) {
        return {};
    }
    auto* ctx = Ctx();
    ScopedRecursiveMutex scope(&docLock);
    fz_image* image = FzFindImageAtIdx(ctx, pageInfo, pel->imageID);
    if (!image) {
        return {};
    }
    fz_compressed_buffer* cbuf = fz_compressed_image_buffer(ctx, image);
    if (!cbuf || !cbuf->buffer) {
        return {};
    }
    int type = cbuf->params.type;
    if (image->use_colorkey || image->mask) {
        return {};
    }
    if (type == FZ_IMAGE_JPEG && image->n == 4) {
        return StandaloneJpegFromPdfCmykImage(ctx, image);
    }
    if (image->use_decode) {
        return {};
    }
    if (type != FZ_IMAGE_JPEG && type != FZ_IMAGE_PNG && type != FZ_IMAGE_GIF && type != FZ_IMAGE_BMP &&
        type != FZ_IMAGE_TIFF && type != FZ_IMAGE_WEBP) {
        return {};
    }
    unsigned char* data = nullptr;
    size_t n = fz_buffer_storage(ctx, cbuf->buffer, &data);
    if (!data || n == 0 || n > (size_t)INT_MAX) {
        return {};
    }
    return str::Dup(Str((char*)data, (int)n));
}

bool EngineMupdf::BenchLoadPage(int pageNo) {
    return GetFzPageInfo(pageNo, false) != nullptr;
}

fz_matrix EngineMupdf::viewctm(int pageNo, float zoom, int rotation) {
    const fz_rect tmpRc = ToFzRect(PageMediabox(pageNo));
    return FzCreateViewCtm(tmpRc, zoom, rotation);
}

fz_matrix EngineMupdf::viewctm(fz_page* page, float zoom, int rotation) const {
    auto* ctx = Ctx();

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
#if !OS_WIN
    (void)pageNo;
    (void)rect;
    (void)imageIdx;
    return nullptr;
#else
    auto* ctx = Ctx();

    FzPageInfo* pageInfo = GetFzPageInfo(pageNo, false);
    if (!pageInfo->page) {
        return nullptr;
    }
    const auto& images = pageInfo->images;
    bool outOfBounds = imageIdx >= len(images);
    fz_rect imgRect = images[imageIdx]->rect;
    bool badRect = ToRectF(imgRect) != rect;
    ReportIf(outOfBounds);
    ReportIf(badRect);
    if (outOfBounds || badRect) {
        return nullptr;
    }

    ScopedRecursiveMutex scope(&docLock);

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
                        int a = mp[((size_t)my * mask->stride) + ((size_t)mx * mn)]; // smask = alpha
                        u8* px = bp + ((size_t)y * pixmap->stride) + ((size_t)x * bn);
                        for (int k = 0; k < 3; k++) {
                            px[k] = (u8)(((px[k] * a) + (255 * (255 - a))) / 255);
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
        // we own bmp if it was already created, so drop it rather than just forgetting it
        delete bmp;
        bmp = nullptr;
    }

    return bmp;
#endif
}

static PageText ExtractPageTextLocked(EngineMupdf* e, FzPageInfo* pageInfo) {
    auto* ctx = e->Ctx();
    // callers hold pagesLock + renderLock; docLock is needed too because this
    // runs the whole page, annotations included, and text extraction happens on
    // a background thread while the UI thread can be editing those annotations
    ScopedRecursiveMutex docScope(&e->docLock);
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

PageText EngineMupdf::ExtractPageText(int pageNo) {
    ScopedRecursiveMutex pagesScope(&pagesLock);
    ScopedMutex renderScope(&renderLock);
    FzPageInfo* pageInfo = GetFzPageInfoLocked(this, pageNo, true, nullptr);
    if (!pageInfo) {
        return {};
    }
    return ExtractPageTextLocked(this, pageInfo);
}

bool EngineMupdf::TryExtractPageText(int pageNo, PageText* out) {
    if (!pagesLock.TryLock()) {
        return false;
    }
    if (!renderLock.TryLock()) {
        pagesLock.Unlock();
        return false;
    }
    FzPageInfo* pageInfo = GetFzPageInfoLocked(this, pageNo, true, nullptr);
    if (!pageInfo) {
        renderLock.Unlock();
        pagesLock.Unlock();
        *out = {};
        return true;
    }
    *out = ExtractPageTextLocked(this, pageInfo);
    renderLock.Unlock();
    pagesLock.Unlock();
    return true;
}

void EngineMupdf::ReleaseTextExtractionThreadContext() {
    ReleasePerThreadContext(this);
}

static void pdf_extract_fonts(fz_context* ctx, pdf_obj* res, Vec<pdf_obj*>& fontList, Vec<pdf_obj*>& resList,
                              int depth) {
    // dedupe/cycle-protect via resList, not pdf_mark_obj: marks mutate shared
    // pdf_obj flags, which races with other threads using marks (and would
    // leave objects marked while locks are dropped between pages)
    if (!res || depth >= 64 || VecContains(resList, res)) {
        return;
    }
    VecAppend(resList, res);

    pdf_obj* fonts = pdf_dict_gets(ctx, res, "Font");
    for (int k = 0; k < pdf_dict_len(ctx, fonts); k++) {
        pdf_obj* font = pdf_resolve_indirect(ctx, pdf_dict_get_val(ctx, fonts, k));
        if (font && !VecContains(fontList, font)) {
            VecAppend(fontList, font);
        }
    }
    // also extract fonts for all XObjects (recursively)
    pdf_obj* xobjs = pdf_dict_gets(ctx, res, "XObject");
    for (int k = 0; k < pdf_dict_len(ctx, xobjs); k++) {
        pdf_obj* xobj = pdf_dict_get_val(ctx, xobjs, k);
        pdf_obj* xres = pdf_dict_gets(ctx, xobj, "Resources");
        pdf_extract_fonts(ctx, xres, fontList, resList, depth + 1);
    }
}

TempStr EngineMupdf::ExtractFontListTemp() {
    if (!pdfdoc) {
        return {};
    }

    Vec<pdf_obj*> fontList;
    Vec<pdf_obj*> resList;

    auto* ctx = Ctx();

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
        ScopedMutex renderScope(&renderLock);
        ScopedRecursiveMutex perPageScope(&docLock);
        fz_try(ctx) {
            pdf_obj* pageObj = pdf_lookup_page_obj(ctx, pdfdoc, i);
            pdf_obj* resources = pdf_dict_gets(ctx, pageObj, "Resources");
            pdf_extract_fonts(ctx, resources, fontList, resList, 0);
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
                    pdf_extract_fonts(ctx, pdf_dict_gets(ctx, ap, "Resources"), fontList, resList, 0);
                } else {
                    // appearance state sub-dictionary
                    for (int j = 0; j < pdf_dict_len(ctx, ap); j++) {
                        pdf_obj* state = pdf_dict_get_val(ctx, ap, j);
                        pdf_extract_fonts(ctx, pdf_dict_gets(ctx, state, "Resources"), fontList, resList, 0);
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
    ScopedMutex renderScope(&renderLock);
    ScopedRecursiveMutex scope(&docLock);

    str::Builder info;
    StrVec fonts;
    for (int i = 0; i < len(fontList); i++) {
        Str name, type, encoding;
        bool embedded = false;
        fz_try(ctx) {
            pdf_obj* font = fontList[i];
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
                name = Str(name.s + 7, name.len - 7);
            }
            type = Str(pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Subtype")));
            if (font2 != font) {
                Str type2 = Str(pdf_to_name(ctx, pdf_dict_gets(ctx, font2, "Subtype")));
                if (str::Eq(type2, StrL("CIDFontType0"))) {
                    type = StrL("Type1 (CID)");
                } else if (str::Eq(type2, StrL("CIDFontType2"))) {
                    type = StrL("TrueType (CID)");
                }
            }
            if (str::Eq(type, StrL("Type3"))) {
                embedded = pdf_dict_gets(ctx, font2, "CharProcs") != nullptr;
            }

            encoding = Str(pdf_to_name(ctx, pdf_dict_gets(ctx, font, "Encoding")));
            if (str::Eq(encoding, StrL("WinAnsiEncoding"))) {
                encoding = StrL("Ansi");
            } else if (str::Eq(encoding, StrL("MacRomanEncoding"))) {
                encoding = StrL("Roman");
            } else if (str::Eq(encoding, StrL("MacExpertEncoding"))) {
                encoding = StrL("Expert");
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            continue;
        }
        // skip if name/type/encoding pointers are null (pdf_to_name can return
        // nullptr). Empty strings are fine. Do not ReportIf-then-continue with
        // null deref on name.s[0] (would kill GetFontsThread mid-list).
        if (!name.s || !type.s || !encoding.s) {
            continue;
        }

        info.Reset();
#if OS_WIN
        if (name.s[0] < 0 && MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, name.s, -1, nullptr, 0)) {
            TempStr s = strconv::ToMultiByteTemp(name, 936, CP_UTF8);
            info.Append(s);
        } else {
            info.Append(name);
        }
#else
        info.Append(name);
#endif
        if (len(encoding) > 0 || len(type) > 0 || embedded) {
            info.Append(StrL(" ("));
            if (len(type) > 0) {
                info.Append(fmt("%s; ", type));
            }
            if (len(encoding) > 0) {
                info.Append(fmt("%s; ", encoding));
            }
            if (embedded) {
                info.Append(StrL("embedded; "));
            }
            info.RemoveAt(len(info) - 2, 2);
            info.Append(StrL(")"));
        }

        if (len(info) == 0) {
            continue;
        }
        AppendIfNotExists(&fonts, ToStr(info));
    }
    if (len(fonts) == 0) {
        return {};
    }

    SortNatural(&fonts);
    return JoinTemp(&fonts, StrL("\n"));
}

// @gen-start docprop-mupdf
// clang-format off
static SeqStrNum mupdfPropsMap =
    "info:Title\0" "\x02"
    "info:Author\0" "\x04"
    "info:Subject\0" "\x08"
    "info:Producer\0" "\x16"
    "info:Creator\0" "\x0e"
    "info:CreationDate\0" "\x0a"
    "info:ModDate\0" "\x0c"
    "\0";
// clang-format on
// @gen-end docprop-mupdf

TempStr EngineMupdf::GetPropertyTemp(DocProp prop) {
    // Font list walks every page under renderLock+docLock and intentionally
    // drops those locks between pages so the UI/renderer can run. Holding
    // docLock here for the whole call inverts lock order vs render (which
    // takes renderLock then docLock) and can deadlock the background
    // GetFontsThread — Properties dialog stuck on "Getting font information..."
    // or a truncated font list (issue #5853).
    if (prop == DocProp::FontList) {
        return ExtractFontListTemp();
    }

    auto* ctx = Ctx();
    ScopedRecursiveMutex ctxScope(&docLock);

    Str key = SeqStrNumStrByNumber(mupdfPropsMap, (i64)prop);
    if (key) {
        char buf[1024]{};
        int bufSize = dimofi(buf);
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

    if (prop == DocProp::PdfVersion) {
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

    if (prop == DocProp::PdfFileStructure) {
        StrVec fstruct;
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, pdfInfo, "Linearized"))) {
            fstruct.Append(StrL("linearized"));
        }
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, pdfInfo, "Marked"))) {
            fstruct.Append(StrL("tagged"));
        }
        if (pdf_dict_gets(ctx, pdfInfo, "OutputIntents")) {
            int n = pdf_array_len(ctx, pdf_dict_gets(ctx, pdfInfo, "OutputIntents"));
            for (int i = 0; i < n; i++) {
                pdf_obj* intent = pdf_array_get(ctx, pdf_dict_gets(ctx, pdfInfo, "OutputIntents"), i);
                ReportIf(!str::StartsWith(Str(pdf_to_name(ctx, intent)), StrL("GTS_")));
                const char* intentName = pdf_to_name(ctx, intent);
                fstruct.Append(Str(intentName + 4));
            }
        }
        if (len(fstruct) == 0) {
            return {};
        }
        return JoinTemp(&fstruct, StrL(","));
    }

    if (prop == DocProp::UnsupportedFeatures) {
        if (pdf_to_bool(ctx, pdf_dict_gets(ctx, pdfInfo, "Unsupported_XFA"))) {
            return StrL("XFA");
        }
        return {};
    }

    // @gen-start docprop-pdf-info
    // clang-format off
static SeqStrNum pdfPropNames =
    "Title\0" "\x02"
    "Author\0" "\x04"
    "Subject\0" "\x08"
    "Copyright\0" "\x06"
    "CreationDate\0" "\x0a"
    "ModDate\0" "\x0c"
    "Creator\0" "\x0e"
    "Producer\0" "\x16"
    "\0";
    // clang-format on
    // @gen-end docprop-pdf-info
    Str pdfPropName = SeqStrNumStrByNumber(pdfPropNames, (i64)prop);
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
    int n = fz_lookup_metadata(ctx, doc, CStrTemp(key), buf, dimofi(buf));
    if (n <= 0) {
        return {};
    }
    if (n > dimofi(buf)) {
        n = dimofi(buf) - 1;
        buf[n] = 0;
    }
    return str::DupTemp(Str(buf, (int)((size_t)n - 1)));
}

#if OS_WIN
static bool (*gEutlLookupFn)(const u8* der, int derLen) = nullptr;

void SetEutlLookupFn(bool (*fn)(const u8* der, int derLen)) {
    gEutlLookupFn = fn;
}

static bool CertIsEuTrusted(const u8* der, int derLen) {
    if (!gEutlLookupFn || !der || derLen <= 0) {
        return false;
    }
    return gEutlLookupFn(der, derLen);
}

static TempStr FormatUnixTimeTemp(int64_t unixSecs) {
    if (unixSecs <= 0) {
        return {};
    }
    time_t t = (time_t)unixSecs;
    struct tm tm;
    gmtime_s(&tm, &t);
    char buf[64];
    strftime(buf, sizeof buf, "%Y/%m/%d %H:%M:%S UTC", &tm);
    return str::DupTemp(Str(buf));
}

static TempStr FormatPdfDateRawTemp(fz_context* ctx, pdf_obj* obj) {
    if (!obj) {
        return {};
    }
    const char* raw = nullptr;
    fz_try(ctx) {
        raw = pdf_to_str_buf(ctx, obj);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        raw = nullptr;
    }
    if (!raw || !raw[0]) {
        return {};
    }
    Str s = Str(raw);
    str::TrimPrefix(s, StrL("D:"));
    if (s.len < 14) {
        return {};
    }
    char buf[80];
    int n =
        snprintf(buf, sizeof(buf), "%.4s/%.2s/%.2s %.2s:%.2s:%.2s", s.s, s.s + 4, s.s + 6, s.s + 8, s.s + 10, s.s + 12);
    if (n < 0) {
        return {};
    }
    if (s.len > 14) {
        Str tz = Str(s.s + 14, s.len - 14);
        if (tz.len > 0 && (tz.s[0] == '+' || tz.s[0] == '-' || tz.s[0] == 'Z')) {
            snprintf(buf + n, sizeof(buf) - (size_t)n, " %s", tz.s);
        }
    }
    return str::DupTemp(Str(buf));
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

static bool PdfHasDssRevocation(fz_context* ctx, pdf_document* pdfdoc) {
    pdf_obj* root = pdf_dict_get(ctx, pdf_trailer(ctx, pdfdoc), PDF_NAME(Root));
    pdf_obj* dss = root ? pdf_dict_gets(ctx, root, "DSS") : nullptr;
    if (!dss) {
        return false;
    }
    pdf_obj* crls = pdf_dict_gets(ctx, dss, "CRLs");
    pdf_obj* ocsps = pdf_dict_gets(ctx, dss, "OCSPs");
    int nCrl = crls ? pdf_array_len(ctx, crls) : 0;
    int nOcsp = ocsps ? pdf_array_len(ctx, ocsps) : 0;
    return nCrl > 0 || nOcsp > 0;
}

static const char* SigSubFilter(fz_context* ctx, pdf_obj* vDict) {
    pdf_obj* sf = pdf_dict_get(ctx, vDict, PDF_NAME(SubFilter));
    if (!sf) {
        return nullptr;
    }
    return pdf_to_name(ctx, sf);
}

static bool SubFilterIsDocTimeStamp(const char* sf) {
    return sf && str::Eq(Str(sf), StrL("ETSI.RFC3161"));
}

static bool SubFilterIsCades(const char* sf) {
    return sf && str::StartsWith(Str(sf), StrL("ETSI.CAdES"));
}

static void AppendTrustSource(str::Builder& s, const u8* der, int derLen) {
    if (CertIsEuTrusted(der, derLen)) {
        s.Append(StrL("  Source of trust: European Union Trusted List (EUTL)\n"));
    } else {
        s.Append(StrL("  Source of trust: Windows Certificate Store\n"));
    }
}

static void AppendLtvLine(str::Builder& s, bool ltv, int64_t notAfterUnix) {
    if (ltv) {
        s.Append(StrL("  This signature is LTV enabled.\n"));
        return;
    }
    TempStr exp = FormatUnixTimeTemp(notAfterUnix);
    if (exp) {
        s.Append(fmt("  This signature isn't LTV enabled and expires after: %s\n", exp));
    } else {
        s.Append(StrL("  This signature isn't LTV enabled.\n"));
    }
}

static void AppendPadesLevel(str::Builder& s, bool isCades, bool hasTs, bool ltv, bool hasDocTs, bool isDocTs,
                             bool hasPolicy) {
    if (isDocTs) {
        return;
    }
    if (!isCades) {
        return;
    }
    const char* level = "PAdES B-B";
    if (hasDocTs && ltv && hasTs) {
        level = "PAdES B-LTA";
    } else if (ltv && hasTs) {
        level = "PAdES B-LT";
    } else if (hasTs) {
        level = "PAdES B-T";
    }
    if (hasPolicy && str::Eq(Str(level), StrL("PAdES B-B"))) {
        s.Append(StrL("  Signature level: PAdES-EPES\n"));
        return;
    }
    s.Append(fmt("  Signature level: %s\n", Str(level)));
}

struct SigFieldWalk {
    Vec<pdf_obj*> fields;
};

static void OnSigFieldArrive(fz_context* ctx, pdf_obj* node, void* arg, pdf_obj** values) {
    pdf_obj* ft = values && values[0] ? values[0] : pdf_dict_get_inheritable(ctx, node, PDF_NAME(FT));
    if (ft && pdf_name_eq(ctx, ft, PDF_NAME(Sig))) {
        VecAppend(((SigFieldWalk*)arg)->fields, pdf_keep_obj(ctx, node));
    }
}

static void CollectSignatureFields(fz_context* ctx, pdf_document* pdfdoc, Vec<pdf_obj*>& fields) {
    SigFieldWalk walk;
    pdf_obj* formFields = pdf_dict_getp(ctx, pdf_trailer(ctx, pdfdoc), "Root/AcroForm/Fields");
    pdf_obj* ftName[2] = {PDF_NAME(FT), nullptr};
    pdf_obj* ftVal = nullptr;
    pdf_walk_tree(ctx, formFields, PDF_NAME(Kids), OnSigFieldArrive, nullptr, &walk, ftName, &ftVal);
    fields = walk.fields;
}

static int PageNoForSigField(fz_context* ctx, pdf_document* pdfdoc, pdf_obj* field) {
    pdf_obj* p = pdf_dict_get(ctx, field, PDF_NAME(P));
    if (!p) {
        return 0;
    }
    int n = pdf_lookup_page_number(ctx, pdfdoc, p);
    return n >= 0 ? n + 1 : 0;
}

static void AppendSignatureFieldInfo(fz_context* ctx, str::Builder& s, pdf_pkcs7_verifier* verifier,
                                     pdf_document* pdfdoc, pdf_obj* sigObj, int sigNo, int pageNo, bool docHasDss,
                                     bool docHasDocTs) {
    if (len(s) > 0) {
        s.AppendChar('\n');
    }
    pdf_obj* vDict = pdf_dict_get(ctx, sigObj, PDF_NAME(V));
    if (!vDict) {
        vDict = sigObj;
    }
    const char* subFilter = SigSubFilter(ctx, vDict);
    bool isDocTs = SubFilterIsDocTimeStamp(subFilter);
    bool isSigned = pdf_signature_is_signed(ctx, pdfdoc, sigObj);
    if (!isSigned && isDocTs && pdf_dict_get(ctx, vDict, PDF_NAME(Contents))) {
        isSigned = true;
    }
    if (isDocTs) {
        s.Append(fmt("Signature %d (document timestamp):\n", sigNo));
    } else if (pageNo > 0) {
        s.Append(fmt("Signature %d (page %d):\n", sigNo, pageNo));
    } else {
        s.Append(fmt("Signature %d:\n", sigNo));
    }
    if (!isSigned) {
        s.Append(StrL("  not signed\n"));
        return;
    }

    char* contents = nullptr;
    size_t contentsLen = 0;
    pkcs7_windows_sig_info info{};
    fz_try(ctx) {
        contentsLen = pdf_signature_contents(ctx, pdfdoc, sigObj, &contents);
        if (contentsLen > 0) {
            pkcs7_windows_inspect(ctx, (unsigned char*)contents, contentsLen, &info);
        }
    }
    fz_always(ctx) {
        fz_free(ctx, contents);
        contents = nullptr;
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }

    s.Append(isDocTs ? StrL("  Timestamp certificate:\n") : StrL("  User certificate:\n"));
    AppendTrustSource(s, info.cert_der, info.cert_der_len);
    if (info.signer_cn && info.signer_cn[0]) {
        s.Append(fmt("  Signed by: %s\n", Str(info.signer_cn)));
    } else {
        s.Append(StrL("  Signed by: (unknown)\n"));
    }

    pdf_obj* mObj = pdf_dict_get(ctx, vDict, PDF_NAME(M));
    TempStr signedAt = FormatPdfDateRawTemp(ctx, mObj);
    if (!signedAt && info.n_ts > 0) {
        signedAt = FormatUnixTimeTemp(info.ts[0].gen_time_unix);
    }
    if (signedAt) {
        s.Append(fmt("  Signature time: %s\n", signedAt));
        if (isDocTs) {
            s.Append(StrL("  The time and date displayed is from the secure time & date server.\n"));
        } else {
            s.Append(StrL("  The time and date displayed is from the user device.\n"));
        }
    }
    if (info.issuer_cn && info.issuer_cn[0]) {
        s.Append(fmt("  Certificate issued by: %s\n", Str(info.issuer_cn)));
    }
    if (info.has_qc_statement) {
        s.Append(StrL("  Qualified certificate (eIDAS qcStatements).\n"));
    }

    pdf_signature_error certErr = PDF_SIGNATURE_ERROR_UNKNOWN;
    pdf_signature_error digErr = PDF_SIGNATURE_ERROR_UNKNOWN;
    int edits = 0;
    fz_try(ctx) {
        certErr = pdf_check_certificate(ctx, verifier, pdfdoc, sigObj);
        digErr = pdf_check_digest(ctx, verifier, pdfdoc, sigObj);
        edits = pdf_signature_incremental_change_since_signing(ctx, pdfdoc, sigObj);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    if (!isDocTs) {
        if (certErr) {
            s.Append(fmt("  Certificate: %s\n", Str(pdf_signature_error_description(certErr))));
        }
        if (digErr) {
            s.Append(fmt("  Digest: %s\n", Str(pdf_signature_error_description(digErr))));
        } else if (edits) {
            s.Append(StrL("  The document was changed since the signature was applied.\n"));
        } else {
            s.Append(StrL("  The document wasn't changed since the signature was applied.\n"));
        }
    }

    bool ltv = docHasDss;
    AppendLtvLine(s, ltv, info.not_after_unix);
    if (info.hash_algo) {
        s.Append(fmt("  Hash algorithm: %s\n", Str(info.hash_algo)));
    }
    if (info.sig_algo) {
        s.Append(fmt("  Signature algorithm: %s\n", Str(info.sig_algo)));
    }
    if (info.digest_hex) {
        s.Append(fmt("  Document hash: %s\n", Str(info.digest_hex)));
    }
    bool isCades = SubFilterIsCades(subFilter) || info.has_cades_attr;
    AppendPadesLevel(s, isCades, info.has_timestamp, ltv, docHasDocTs, isDocTs, info.has_sig_policy_attr);

    AppendSigDictText(ctx, s, vDict, StrL("reason"), PDF_NAME(Reason));
    AppendSigDictText(ctx, s, vDict, StrL("location"), PDF_NAME(Location));
    AppendSigDictText(ctx, s, vDict, StrL("contact"), PDF_NAME(ContactInfo));

    for (int ti = 0; ti < info.n_ts; ti++) {
        const pkcs7_windows_ts_info& ts = info.ts[ti];
        s.Append(StrL("  -----\n"));
        s.Append(StrL("  Included Time Stamp:\n"));
        AppendTrustSource(s, ts.cert_der, ts.cert_der_len);
        if (ts.signer_cn && ts.signer_cn[0]) {
            s.Append(fmt("  Signed by: %s\n", Str(ts.signer_cn)));
        }
        TempStr tsTime = FormatUnixTimeTemp(ts.gen_time_unix);
        if (tsTime) {
            s.Append(fmt("  Signature time: %s\n", tsTime));
            s.Append(StrL("  The time and date displayed is from the secure time & date server.\n"));
        }
        if (ts.issuer_cn && ts.issuer_cn[0]) {
            s.Append(fmt("  Certificate issued by: %s\n", Str(ts.issuer_cn)));
        }
        AppendLtvLine(s, ltv, ts.not_after_unix);
        if (ts.hash_algo) {
            s.Append(fmt("  Hash algorithm: %s\n", Str(ts.hash_algo)));
        }
        if (ts.policy_oid) {
            s.Append(fmt("  Policy ID: %s\n", Str(ts.policy_oid)));
        }
    }

    pkcs7_windows_sig_info_free(ctx, &info);
}

void EngineMupdfGetSignatureCerts(EngineBase* engine, Vec<PdfSigCert>& out) {
    FreePdfSigCerts(out);
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        return;
    }
    fz_context* ctx = e->Ctx();
    ScopedRecursiveMutex scope(&e->docLock);
    Vec<pdf_obj*> fields;
    fz_try(ctx) {
        CollectSignatureFields(ctx, e->pdfdoc, fields);
        int sigNo = 0;
        for (pdf_obj* field : fields) {
            ++sigNo;
            char* contents = nullptr;
            size_t contentsLen = pdf_signature_contents(ctx, e->pdfdoc, field, &contents);
            if (contentsLen == 0) {
                fz_free(ctx, contents);
                continue;
            }
            pkcs7_windows_sig_info info{};
            if (contentsLen > 0) {
                pkcs7_windows_inspect(ctx, (unsigned char*)contents, contentsLen, &info);
            }
            fz_free(ctx, contents);
            auto add = [&](const char* who, const u8* der, int derLen) {
                if (!der || derLen <= 0) {
                    return;
                }
                PdfSigCert c;
                c.label = str::Dup(fmt("Signature %d %s", sigNo, Str(who)));
                c.der = str::Dup(Str((const char*)der, derLen));
                VecAppend(out, c);
            };
            pdf_obj* v = pdf_dict_get(ctx, field, PDF_NAME(V));
            bool docTs = SubFilterIsDocTimeStamp(SigSubFilter(ctx, v ? v : field));
            add(docTs ? "timestamp" : "signer", info.cert_der, info.cert_der_len);
            for (int ti = 0; ti < info.n_ts; ti++) {
                add("timestamp", info.ts[ti].cert_der, info.ts[ti].cert_der_len);
            }
            pkcs7_windows_sig_info_free(ctx, &info);
        }
    }
    fz_always(ctx) {
        for (pdf_obj* field : fields) {
            pdf_drop_obj(ctx, field);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
}

void FreePdfSigCerts(Vec<PdfSigCert>& certs) {
    for (PdfSigCert& c : certs) {
        str::Free(c.label);
        str::Free(c.der);
    }
    VecReset(certs);
}
#endif

void EngineMupdf::GetProperties(Props& propsOut) {
    EngineBase::GetProperties(propsOut);

    auto* ctx = Ctx();
    ScopedRecursiveMutex ctxScope(&docLock);

    TempStr val = LookupMetadataTemp(ctx, _doc, StrL("info:Keywords"));
    if (val) {
        AddProp(propsOut, DocProp::Keywords, val);
    }

    val = LookupMetadataTemp(ctx, _doc, StrL("encryption"));
    if (val) {
        AddProp(propsOut, DocProp::Encryption, val);
    }

    // pdf signatures (signed form widgets). Walks each page's widget set;
    // for each signature widget, pulls signer DN + cert/digest verdict via
    // the Windows CryptoAPI pdf_pkcs7_verifier.
#if OS_WIN
    if (pdfdoc && pdf_count_signatures(ctx, pdfdoc) > 0) {
        str::Builder sigs;
        pdf_pkcs7_verifier* verifier = nullptr;
        Vec<pdf_obj*> fields;
        fz_var(verifier);
        fz_try(ctx) {
            verifier = pkcs7_windows_new_verifier(ctx);
            CollectSignatureFields(ctx, pdfdoc, fields);
            bool hasDss = PdfHasDssRevocation(ctx, pdfdoc);
            bool hasDocTs = false;
            for (pdf_obj* field : fields) {
                pdf_obj* v = pdf_dict_get(ctx, field, PDF_NAME(V));
                if (SubFilterIsDocTimeStamp(SigSubFilter(ctx, v ? v : field))) {
                    hasDocTs = true;
                    break;
                }
            }
            int sigNo = 0;
            for (pdf_obj* field : fields) {
                ++sigNo;
                int pageNo = PageNoForSigField(ctx, pdfdoc, field);
                AppendSignatureFieldInfo(ctx, sigs, verifier, pdfdoc, field, sigNo, pageNo, hasDss, hasDocTs);
            }
        }
        fz_always(ctx) {
            for (pdf_obj* field : fields) {
                pdf_drop_obj(ctx, field);
            }
            pdf_drop_verifier(ctx, verifier);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
        if (len(sigs) > 0) {
            AddProp(propsOut, DocProp::Signatures, str::DupTemp(ToStr(sigs)));
        }
    }
#endif

    // for epub files, list all files in the archive
    Str path = FilePath();
    if (path && str::EndsWithI(path, StrL(".epub"))) {
        ArchiveExtractProgressCb emptyCb;
        Archive* zip = OpenArchiveFromFile(path, /*eagerLoad=*/false, emptyCb);
        if (zip) {
            str::Builder filesStr;
            const auto& fileInfos = zip->GetFileInfos();
            int n = len(fileInfos);
            for (int i = 0; i < n; i++) {
                auto* fi = fileInfos[i];
                if (len(fi->name) == 0) {
                    continue;
                }
                filesStr.AppendChar('\n');
                filesStr.Append(fi->name);
            }
            AddProp(propsOut, DocProp::Files, str::DupTemp(ToStr(filesStr)));
            delete zip;
        }
    }
}

Str EngineMupdf::GetFileData() {
    auto* ctx = Ctx();

    if (!pdfdoc) {
        return {};
    }

    Str res;
    ScopedRecursiveMutex scope(&docLock);

    fz_var(res);
    fz_try(ctx) {
        res = FzExtractStreamData(ctx, pdfdoc->file);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        res = {};
    }

    if (len(res) > 0) {
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
    if (len(d) > 0) {
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
        return {};
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
    if (len(path) == 0) {
        path = currPath;
    }
    auto* ctx = epdf->Ctx();
    ScopedRecursiveMutex scope(&epdf->docLock);

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
        // undoing back to here means the file on disk matches again
        epdf->savedUndoPos = EngineMupdfUndoPos(epdf, nullptr);
    }
    return ok;
}

// Write a standalone (non-incremental) copy of the live PDF, including
// unsaved annotations, without marking the document clean. Used so tools
// that re-open the file from disk (bake) see the current session (issue #5977).
bool EngineMupdfSaveCopy(EngineBase* engine, Str path) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc || !path) {
        return false;
    }
    auto* ctx = epdf->Ctx();
    ScopedRecursiveMutex scope(&epdf->docLock);
    pdf_write_options save_opts{};
    save_opts = pdf_default_write_options2;
    save_opts.do_incremental = 0;
    save_opts.do_compress = 1;
    bool ok = false;
    fz_try(ctx) {
        pdf_save_document(ctx, epdf->pdfdoc, CStrTemp(path), &save_opts);
        ok = true;
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("EngineMupdfSaveCopy: saving '%s' failed: '%s'\n", path, Str(fz_caught_message(ctx)));
    }
    return ok;
}

// caller must hold pagesLock (protects pages[] and pageInfo->images)
static bool HasClipOptimizationsLocked(EngineMupdf* e, int pageNo) {
    ReportIf(pageNo < 1 || pageNo > e->pageCount);
    if (pageNo < 1 || pageNo > e->pageCount) {
        return false;
    }
    FzPageInfo* pageInfo = e->pages[pageNo - 1];
    if (!pageInfo || !pageInfo->page || !pageInfo->fullyLoaded) {
        return false;
    }

    fz_rect mbox = ToFzRect(e->PageMediabox(pageNo));
    // check if any image covers at least 90% of the page
    for (auto& img : pageInfo->images) {
        fz_rect ir = img->rect;
        if (FzRectOverlap(mbox, ir) >= 0.9f) {
            return false;
        }
    }
    return true;
}

bool EngineMupdf::HasClipOptimizations(int pageNo) {
    if (!pdfdoc) {
        return false;
    }
    // This only tunes tile size (RenderCache::GetTileRes) and the UI thread asks
    // on every zoom/scroll, so never wait for the answer: pagesLock can be held
    // for the length of an image decode by a render thread that is itself queued
    // on renderLock, which stalls the UI mid-mouse-wheel. "false" is what we
    // already return for a page that isn't loaded yet, i.e. "can't tell, use the
    // smaller tiles".
    if (!pagesLock.TryLock()) {
        return false;
    }
    bool res = HasClipOptimizationsLocked(this, pageNo);
    pagesLock.Unlock();
    return res;
}

TempStr EngineMupdf::GetPageLabeTemp(int pageNo) const {
    if (!pageLabels || pageNo < 1 || PageCount() < pageNo) {
        return EngineBase::GetPageLabeTemp(pageNo);
    }

    TempStr res = (*pageLabels)[pageNo - 1];
    if (len(res) == 0 || str::ContainsI(res, StrL(".pdg"))) {
        return EngineBase::GetPageLabeTemp(pageNo);
    }
    return res;
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

bool IsEngineMupdfSupportedFileType(FileType kind) {
    if (kind == FileType::PDF) {
        return true;
    }
    if (kind == FileType::Epub) {
        return true;
    }
    if (kind == FileType::Markdown) {
        return true;
    }
    if (kind == FileType::Fb2) {
        return true;
    }
    if (kind == FileType::Fb2z) {
        return true;
    }
    if (kind == FileType::HTML) {
        return true;
    }
    if (kind == FileType::Svg) {
        return true;
    }
    if (kind == FileType::Xps) {
        return true;
    }
    if (kind == FileType::Txt) {
        return true;
    }
    if (kind == FileType::PalmDoc) {
        return true;
    }
    return false;
}

EngineBase* CreateEngineMupdfFromFile(Str path, FileType kind, int displayDPI, PasswordUI* pwdUI) {
    if (len(path) == 0) {
        return nullptr;
    }
    if (kind == FileType::Fb2z) {
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
        Str d = Str(fi->data, fi->fileSizeUncompressed);
        EngineMupdf* engine = new EngineMupdf();
        if (displayDPI < 70) {
            displayDPI = 96;
        }
        engine->displayDPI = displayDPI;
        fz_stream* stm = FzStreamFromData(engine->Ctx(), (u8*)d.s, d.len);
        if (!engine->LoadFromStream(stm, StrL("foo.fb2"), pwdUI) || !engine->FinishLoading()) {
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
    TempStr ext = GetExtForFileTypeTemp(kind);
    if (ext) {
        SetDefaultExt(engine->defaultExt, ext);
    }
    return engine;
}

EngineBase* CreateEngineMupdfFromData(Str data, Str nameHint, PasswordUI* pwdUI) {
    EngineMupdf* engine = new EngineMupdf();
    fz_stream* stm = FzStreamFromData(engine->Ctx(), (u8*)data.s, data.len);
    if (!engine->LoadFromStream(stm, nameHint, pwdUI) || !engine->FinishLoading()) {
        SafeEngineRelease(&engine);
        return nullptr;
    }
    return engine;
}

static void AppendLoadedAnnotations(EngineMupdf* e, Vec<Annotation*>& annotsOut) {
    VecClear(annotsOut);
    for (FzPageInfo* pi : e->pages) {
        if (pi && pi->annotsLoaded) {
            VecAppendVec(annotsOut, pi->annotations);
        }
    }
}

// Collect Annotation* already sitting on FzPageInfo. Does not load pages.
void EngineMupdfGetLoadedAnnotations(EngineBase* engine, Vec<Annotation*>& annotsOut) {
    VecClear(annotsOut);
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        return;
    }
    ScopedRecursiveMutex scope(&e->pagesLock);
    AppendLoadedAnnotations(e, annotsOut);
}

// Like EngineMupdfGetLoadedAnnotations but does not wait for pagesLock.
bool EngineMupdfTryGetLoadedAnnotations(EngineBase* engine, Vec<Annotation*>& annotsOut) {
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        VecClear(annotsOut);
        return true;
    }
    if (!e->pagesLock.TryLock()) {
        return false;
    }
    AppendLoadedAnnotations(e, annotsOut);
    e->pagesLock.Unlock();
    return true;
}

// Load each page just far enough to read its annots (not stext/links). Callers
// that need the complete list now (tests, matching after reload) use this.
void EngineMupdfGetAnnotations(EngineBase* engine, Vec<Annotation*>& annotsOut) {
    VecClear(annotsOut);
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        return;
    }
    for (int i = 1; i <= e->pageCount; i++) {
        FzPageInfo* pi = e->GetFzPageInfo(i, true);
        if (!pi) {
            continue;
        }
        VecAppendVec(annotsOut, pi->annotations);
    }
}

static void AnnotLoadFinished(EngineMupdf* e) {
    e->annotLoadDone = true;
    Func0 cb = e->annotLoadDoneCb;
    if (!AtomicIntGet(&e->annotLoadCancel)) {
        cb.Call();
    }
    AtomicIntDec(&gDangerousThreadCount);
    e->Release();
}

static void AnnotLoadProgressUi(EngineMupdf* e) {
    Func0 cb = e->annotLoadDoneCb;
    if (!AtomicIntGet(&e->annotLoadCancel) && cb.IsValid()) {
        cb.Call();
    }
    e->Release();
}

static void PostAnnotLoadProgress(EngineMupdf* e) {
    if (AtomicIntGet(&e->annotLoadCancel)) {
        return;
    }
    if (!e->annotLoadDoneCb.IsValid()) {
        return;
    }
    e->AddRef();
    auto fn = MkFunc0(AnnotLoadProgressUi, e);
    uitask::Post(fn, "AnnotLoadProgress");
}

static int CountLoadedAnnots(EngineMupdf* e) {
    int n = 0;
    ScopedRecursiveMutex scope(&e->pagesLock);
    for (FzPageInfo* pi : e->pages) {
        if (pi && pi->annotsLoaded) {
            n += len(pi->annotations);
        }
    }
    return n;
}

static int LoadAnnotsForPageNo(EngineMupdf* e, int pageNo) {
    if (pageNo < 1 || pageNo > e->pageCount) {
        return 0;
    }
    int before = 0;
    {
        ScopedRecursiveMutex scope(&e->pagesLock);
        FzPageInfo* pi = e->pages[pageNo - 1];
        if (pi && pi->annotsLoaded) {
            return 0;
        }
        if (pi) {
            before = len(pi->annotations);
        }
    }
    FzPageInfo* pi = e->GetFzPageInfo(pageNo, true);
    if (!pi) {
        return 0;
    }
    int after = len(pi->annotations);
    if (after < before) {
        return 0;
    }
    return after - before;
}

static void AnnotLoadThread(EngineMupdf* e) {
    Vec<int> first = e->annotLoadFirstPages;
    int nPages = e->pageCount;
    int nSincePost = 0;
    bool postedAny = false;
    TimeStamp lastPost = TimeGet();

    auto maybePost = [&](bool force) {
        if (AtomicIntGet(&e->annotLoadCancel)) {
            return;
        }
        if (!force && nSincePost < 16) {
            return;
        }
        if (!force && postedAny && TimeSinceInMs(lastPost) < 1000) {
            return;
        }
        if (!force && nSincePost < 1) {
            return;
        }
        if (force && CountLoadedAnnots(e) == 0 && nSincePost == 0) {
            return;
        }
        logf("AnnotLoadProgress: force=%d nSincePost=%d loaded=%d\n", (int)force, nSincePost, CountLoadedAnnots(e));
        PostAnnotLoadProgress(e);
        nSincePost = 0;
        lastPost = TimeGet();
        postedAny = true;
    };

    for (int pageNo : first) {
        if (AtomicIntGet(&e->annotLoadCancel)) {
            break;
        }
        nSincePost += LoadAnnotsForPageNo(e, pageNo);
    }
    // Current / visible pages: show them immediately, even if fewer than 16.
    maybePost(true);

    for (int i = 1; i <= nPages; i++) {
        if (AtomicIntGet(&e->annotLoadCancel)) {
            break;
        }
        nSincePost += LoadAnnotsForPageNo(e, i);
        maybePost(false);
    }
    e->ReleaseTextExtractionThreadContext();
    if (AtomicIntGet(&e->annotLoadCancel)) {
        AtomicIntDec(&gDangerousThreadCount);
        e->Release();
        return;
    }
    auto fn = MkFunc0(AnnotLoadFinished, e);
    uitask::Post(fn, "AnnotLoadFinished");
}

void EngineMupdfStartLoadAllAnnotations(EngineBase* engine, const Vec<int>& firstPages, const Func0& onProgress) {
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        return;
    }
    e->annotLoadDoneCb = onProgress;
    if (e->annotLoadDone) {
        return;
    }
    if (e->annotLoadStarted) {
        return;
    }
    bool allLoaded = true;
    {
        ScopedRecursiveMutex scope(&e->pagesLock);
        for (FzPageInfo* pi : e->pages) {
            if (!pi || !pi->annotsLoaded) {
                allLoaded = false;
                break;
            }
        }
    }
    if (allLoaded) {
        e->annotLoadStarted = true;
        e->annotLoadDone = true;
        return;
    }
    e->annotLoadFirstPages = firstPages;
    e->annotLoadStarted = true;
    e->AddRef();
    AtomicIntInc(&gDangerousThreadCount);
    auto fn = MkFunc0(AnnotLoadThread, e);
    ThreadHandle th = StartThread(fn, StrL("LoadAnnots"));
    if (!th) {
        AtomicIntDec(&gDangerousThreadCount);
        e->Release();
        e->annotLoadDone = true;
        onProgress.Call();
        return;
    }
    SafeCloseThreadHandle(&th);
}

bool EngineMupdfHasUnsavedAnnotations(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return false;
    }
    return epdf->modifiedAnnotations;
}

bool EngineMupdfHasRedactMarks(EngineBase* engine) {
    Vec<Annotation*> annots;
    EngineMupdfGetLoadedAnnotations(engine, annots);
    for (Annotation* a : annots) {
        if (a && a->type == AnnotationType::Redact) {
            return true;
        }
    }
    return false;
}

static void DropPageImages(fz_context* ctx, FzPageInfo* pi) {
    for (FitzPageImageInfo* img : pi->images) {
        if (img && img->image) {
            fz_drop_image(ctx, img->image);
            img->image = nullptr;
        }
    }
    DeleteVecMembers(pi->images);
}

// Drop cached page bits that were built from the old content stream.
// Caller holds pagesLock and renderLock. Leaves annotation wrappers in place.
static void InvalidateFzPageAfterContentChange(EngineMupdf* e, FzPageInfo* pi) {
    auto* ctx = e->Ctx();
    if (pi->displayList) {
        fz_drop_display_list(ctx, pi->displayList);
        pi->displayList = nullptr;
    }
    PdfDarkModeInvalidatePage(ctx, pi);
    pi->contentImagesCollected = false;
    DropPageImages(ctx, pi);
    DeleteVecMembers(pi->links);
    DeleteVecMembers(pi->autoLinks);
    if (pi->retainedLinks) {
        fz_drop_link(ctx, pi->retainedLinks);
        pi->retainedLinks = nullptr;
    }
    pi->elementsNeedRebuilding = true;
    pi->fullyLoaded = false;
}

// Apply every /Redact annotation: rewrite the page content, drop the marks,
// and return the Sumatra wrappers that MuPDF deleted (caller must Detach +
// DeleteAnnotation). Incremental save is unsafe afterwards; pdfdoc->redacted
// already forces a full rewrite in EngineMupdfSaveUpdated.
bool EngineMupdfApplyRedactions(EngineBase* engine, Vec<Annotation*>& deletedOut) {
    VecReset(deletedOut);
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        return false;
    }

    pdf_redact_options opts{};
    opts.black_boxes = 1;
    opts.image_method = PDF_REDACT_IMAGE_PIXELS;
    opts.line_art = PDF_REDACT_LINE_ART_REMOVE_IF_TOUCHED;
    opts.text = PDF_REDACT_TEXT_REMOVE;

    auto* ctx = e->Ctx();
    bool any = false;
    ScopedRecursiveMutex pagesScope(&e->pagesLock);
    ScopedMutex renderScope(&e->renderLock);

    for (int pageNo = 1; pageNo <= e->pageCount; pageNo++) {
        FzPageInfo* pi = GetFzPageInfoLocked(e, pageNo, true, nullptr);
        if (!pi || !pi->page) {
            continue;
        }

        bool hasRedact = false;
        for (Annotation* a : pi->annotations) {
            if (a && a->type == AnnotationType::Redact) {
                hasRedact = true;
                break;
            }
        }
        if (!hasRedact) {
            continue;
        }

        Vec<Annotation*> before = pi->annotations;
        int did = 0;
        bool failed = false;
        {
            ScopedRecursiveMutex docScope(&e->docLock);
            fz_try(ctx) {
                pdf_page* page = pdf_page_from_fz_page(ctx, pi->page);
                if (page) {
                    did = pdf_redact_page(ctx, e->pdfdoc, page, &opts);
                }
            }
            fz_catch(ctx) {
                fz_report_error(ctx);
                failed = true;
                logf("EngineMupdfApplyRedactions: pdf_redact_page failed on page %d\n", pageNo);
            }
        }
        if (failed || !did) {
            continue;
        }

        Vec<pdf_annot*> live;
        bool listedLive = false;
        {
            ScopedRecursiveMutex docScope(&e->docLock);
            fz_try(ctx) {
                pdf_page* page = pdf_page_from_fz_page(ctx, pi->page);
                for (pdf_annot* a = page ? pdf_first_annot(ctx, page) : nullptr; a; a = pdf_next_annot(ctx, a)) {
                    VecAppend(live, a);
                }
                listedLive = true;
            }
            fz_catch(ctx) {
                fz_report_error(ctx);
            }
        }

        for (Annotation* w : before) {
            if (!w) {
                continue;
            }
            bool gone = listedLive ? VecFind(live, w->pdfannot) < 0 : w->type == AnnotationType::Redact;
            if (!gone) {
                continue;
            }
            w->pdfannot = nullptr;
            VecRemove(pi->annotations, w);
            VecAppend(deletedOut, w);
        }

        {
            ScopedRecursiveMutex docScope(&e->docLock);
            RebuildCommentsFromAnnotations(ctx, pi);
        }
        InvalidateFzPageAfterContentChange(e, pi);
        e->InvalidateTextForPage(pageNo);
        any = true;
    }

    if (any) {
        e->modifiedAnnotations = true;
    }
    return any;
}

//--- Undo / redo, on top of MuPDF's journal (see pdf_enable_journal)

// Where we are in the undo history: 0 is the document as it was loaded,
// *stepsOut is the number of recorded steps. -1 if the document isn't
// journalled or MuPDF refused to answer (it does while an operation is open).
int EngineMupdfUndoPos(EngineMupdf* e, int* stepsOut) {
    if (stepsOut) {
        *stepsOut = 0;
    }
    if (!e || !e->pdfdoc || e->journalNesting > 0) {
        return -1;
    }
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex docScope(&e->docLock);
    int steps = 0;
    int pos = -1;
    fz_try(ctx) {
        pos = pdf_undoredo_state(ctx, e->pdfdoc, &steps);
    }
    fz_catch(ctx) {
        // MuPDF throws while an edit/appearance op is still open (e.g. selecting
        // a file attachment rebuilds its property row, then the toolbar asks
        // undo/redo). That is expected; do not log it as an error.
        if (fz_caught(ctx) != FZ_ERROR_ARGUMENT) {
            fz_report_error(ctx);
        } else {
            fz_ignore_error(ctx);
        }
        pos = -1;
    }
    if (stepsOut && pos >= 0) {
        *stepsOut = steps;
    }
    return pos;
}

// One operation is one undo step. MuPDF's annotation setters open one
// themselves, so this is for grouping a whole gesture (create, paste, a resize
// drag that writes on every mouse move) into a single step. Nested operations
// fold into the outermost one. Must be paired with EngineMupdfEndOperation().
void EngineMupdfBeginOperation(EngineBase* engine, const char* name) {
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        return;
    }
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex docScope(&e->docLock);
    fz_try(ctx) {
        pdf_begin_operation(ctx, e->pdfdoc, name);
        e->journalNesting++;
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
}

void EngineMupdfEndOperation(EngineBase* engine) {
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc || e->journalNesting <= 0) {
        return;
    }
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex docScope(&e->docLock);
    fz_try(ctx) {
        pdf_end_operation(ctx, e->pdfdoc);
        e->journalNesting--;
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        e->journalNesting--;
    }
}

bool EngineMupdfCanUndo(EngineBase* engine) {
    EngineMupdf* e = AsEngineMupdf(engine);
    int pos = EngineMupdfUndoPos(e, nullptr);
    return pos > 0;
}

bool EngineMupdfCanRedo(EngineBase* engine) {
    EngineMupdf* e = AsEngineMupdf(engine);
    int steps = 0;
    int pos = EngineMupdfUndoPos(e, &steps);
    return pos >= 0 && pos < steps;
}

static Annotation* FindWrapperForPdfAnnot(const Vec<Annotation*>& annots, pdf_annot* a) {
    for (Annotation* w : annots) {
        if (w && w->pdfannot == a) {
            return w;
        }
    }
    return nullptr;
}

// Rebuild `wrappers` so it matches `live`, in page order: keep the wrapper of
// an annotation that is still there, make one for an annotation that (re)appeared
// and hand back the ones whose pdf_annot MuPDF freed.
// A wrapper we keep across an undo / redo caches the bounds the annotation had
// before the step, and the step may well have been a resize or a move. Nothing
// else the wrapper holds can change under it: an annotation that went away is
// dropped, one that came back gets a fresh wrapper.
static void RefreshWrapperBounds(EngineMupdf* e, Annotation* w) {
    if (!w || !w->pdfannot) {
        return;
    }
    fz_context* ctx = e->Ctx();
    ScopedRecursiveMutex docScope(&e->docLock);
    fz_rect bounds = {};
    fz_try(ctx) {
        bounds = pdf_bound_annot(ctx, w->pdfannot);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return;
    }
    w->bounds = ToRectF(bounds);
}

static void ResyncWrapperList(EngineMupdf* e, int pageNo, Vec<Annotation*>& wrappers, const Vec<pdf_annot*>& live,
                              Vec<Annotation*>& removedOut) {
    Vec<Annotation*> res;
    for (pdf_annot* a : live) {
        Annotation* w = FindWrapperForPdfAnnot(wrappers, a);
        if (!w) {
            w = MakeAnnotationWrapper(e, a, pageNo);
            if (!w) {
                continue;
            }
        } else {
            RefreshWrapperBounds(e, w);
        }
        VecAppend(res, w);
    }
    for (Annotation* w : wrappers) {
        if (!w || VecContains(res, w)) {
            continue;
        }
        // its pdf_annot is gone; the wrapper must not reach MuPDF again
        w->pdfannot = nullptr;
        VecAppend(removedOut, w);
    }
    wrappers = res;
}

// Undo / redo restores objects under our feet: MuPDF re-syncs each open page,
// which frees the pdf_annot of an annotation that went away and makes a fresh
// one for an annotation that came back. Bring our wrappers in line and drop
// every cached rendering of the page.
static void SyncPagesAfterUndoRedo(EngineMupdf* e, Vec<Annotation*>& removedOut) {
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex pagesScope(&e->pagesLock);
    ScopedMutex renderScope(&e->renderLock);
    for (FzPageInfo* pi : e->pages) {
        if (!pi || !pi->page) {
            continue;
        }
        if (pi->annotsLoaded) {
            Vec<pdf_annot*> liveAnnots;
            Vec<pdf_annot*> liveWidgets;
            {
                ScopedRecursiveMutex docScope(&e->docLock);
                fz_try(ctx) {
                    pdf_page* page = pdf_page_from_fz_page(ctx, pi->page);
                    for (pdf_annot* a = page ? pdf_first_annot(ctx, page) : nullptr; a; a = pdf_next_annot(ctx, a)) {
                        VecAppend(liveAnnots, a);
                    }
                    for (pdf_annot* a = page ? pdf_first_widget(ctx, page) : nullptr; a; a = pdf_next_widget(ctx, a)) {
                        VecAppend(liveWidgets, a);
                    }
                }
                fz_catch(ctx) {
                    fz_report_error(ctx);
                    continue;
                }
            }
            ResyncWrapperList(e, pi->pageNo, pi->annotations, liveAnnots, removedOut);
            ResyncWrapperList(e, pi->pageNo, pi->widgets, liveWidgets, removedOut);
            {
                ScopedRecursiveMutex docScope(&e->docLock);
                RebuildCommentsFromAnnotations(ctx, pi);
            }
        }
        InvalidateFzPageAfterContentChange(e, pi);
        e->InvalidateTextForPage(pi->pageNo);
    }
}

// Step one operation back (or forward with redo). Returns false if there was
// nothing to step to. The wrappers in removedOut are detached from the document
// already; the caller must take them out of the UI and delete them.
static bool EngineMupdfUndoRedo(EngineBase* engine, bool redo, Vec<Annotation*>& removedOut) {
    VecReset(removedOut);
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        return false;
    }
    bool can = redo ? EngineMupdfCanRedo(engine) : EngineMupdfCanUndo(engine);
    if (!can) {
        return false;
    }
    auto* ctx = e->Ctx();
    bool ok = false;
    {
        ScopedRecursiveMutex docScope(&e->docLock);
        fz_try(ctx) {
            if (redo) {
                pdf_redo(ctx, e->pdfdoc);
            } else {
                pdf_undo(ctx, e->pdfdoc);
            }
            ok = true;
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("EngineMupdfUndoRedo: pdf_%s() failed\n", redo ? StrL("redo") : StrL("undo"));
        }
    }
    if (!ok) {
        return false;
    }
    SyncPagesAfterUndoRedo(e, removedOut);
    return true;
}

bool EngineMupdfUndo(EngineBase* engine, Vec<Annotation*>& removedOut) {
    return EngineMupdfUndoRedo(engine, false, removedOut);
}

bool EngineMupdfRedo(EngineBase* engine, Vec<Annotation*>& removedOut) {
    return EngineMupdfUndoRedo(engine, true, removedOut);
}

// The journal knows exactly whether the document differs from the file, which
// the modifiedAnnotations flag can only approximate (it stays set after the
// change that set it is undone). Call after undo / redo.
void EngineMupdfRefreshModifiedState(EngineBase* engine) {
    EngineMupdf* e = AsEngineMupdf(engine);
    if (!e || !e->pdfdoc) {
        return;
    }
    int pos = EngineMupdfUndoPos(e, nullptr);
    if (pos < 0) {
        return;
    }
    e->modifiedAnnotations = (pos != e->savedUndoPos);
}

// the mupdf engine also renders epub, mobi, fb2, xps, svg and more; only a real
// PDF has a pdf_document behind it
bool EngineMupdfIsPdf(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    return epdf && epdf->pdfdoc != nullptr;
}

bool EngineMupdfSupportsAnnotations(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf) {
        return false;
    }
    return (epdf->pdfdoc != nullptr);
}

// Restyle a reflowable document with the current theme page colors and drop
// cached page display lists so the next render uses the new HTML. Color-only
// CSS should not change page count; if it does we keep the existing slots.
void EngineMupdf::ApplyReflowThemeCss() {
    if (!isReflowable || !_doc || ebookLayoutW <= 0 || ebookLayoutH <= 0) {
        return;
    }
    TempStr themeCss = ReflowDocumentThemeCssTemp();
    TempStr fullCss = ebookUserCss;
    if (themeCss) {
        fullCss = fullCss ? str::JoinTemp(fullCss, themeCss) : themeCss;
    }
    const char* cssZ = fullCss ? CStrTemp(fullCss) : "";

    ScopedRecursiveMutex pagesScope(&pagesLock);
    ScopedMutex renderScope(&renderLock);
    ScopedRecursiveMutex docScope(&docLock);

    fz_context* ctx = Ctx();
    if (!ctx) {
        return;
    }
    for (FzPageInfo* pi : pages) {
        if (!pi) {
            continue;
        }
        InvalidateFzPageAfterContentChange(this, pi);
        if (pi->page) {
            fz_drop_page(ctx, pi->page);
            pi->page = nullptr;
        }
    }

    fz_try(ctx) {
        fz_style_document(ctx, _doc, ebookPublisherCss, cssZ);
        fz_layout_document(ctx, _doc, ebookLayoutW, ebookLayoutH, ebookLayoutEm);
        int n = fz_count_pages(ctx, _doc);
        if (n != pageCount) {
            logf("ApplyReflowThemeCss: page count %d -> %d, keeping %d\n", pageCount, n, pageCount);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
}

// Drop cached dark-mode analyses and processed images; call when dark-mode
// options (theme, color mode, preserve toggle) change. Reflowable docs also
// restyle with the current theme CSS.
void EngineMupdfInvalidateDarkMode(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf) {
        return;
    }
    epdf->ApplyReflowThemeCss();
    ScopedRecursiveMutex scope(&epdf->pagesLock);
    fz_context* ctx = epdf->Ctx();
    if (epdf->darkModeEngineCache) {
        PdfDarkModeEngineCacheClear(ctx, epdf->darkModeEngineCache);
    }
    for (FzPageInfo* pi : epdf->pages) {
        if (pi) {
            PdfDarkModeInvalidatePage(ctx, pi);
        }
    }
}

// PDF documents support the object-level smart dark renderer
bool EngineSupportsSmartDarkMode(EngineBase* engine) {
    if (!engine || engine->kind != kindEngineMupdf) {
        return false;
    }
    if (!str::EqI(engine->defaultExt, StrL(".pdf"))) {
        return false;
    }
    EngineMupdf* epdf = AsEngineMupdf(engine);
    return epdf && epdf->pdfdoc;
}

// Toggle CAD/engineering-drawing line enhancement for this document
// (CmdToggleEngineeringDrawingEnhance); caller re-renders. Runs the detection
// pass lazily for documents loaded while the mode pref was "off".
// the state CmdToggleEngineeringDrawingEnhance would flip. Deliberately doesn't
// run detection - that takes the document locks - so before it has run this
// reads as off, which is what the toggle would flip away from anyway
// is CAD/engineering-drawing line enhancement in effect for this document?
bool EngineMupdfCadEnhanceActive(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return false;
    }
    return epdf->CadEnhanceActive();
}

// toggle CAD/engineering-drawing line enhancement for this document
void EngineMupdfToggleCadEnhance(EngineBase* engine) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return;
    }
    if (!epdf->cadDetectDone) {
        // lock order: renderLock before docLock (see EngineMupdf.h)
        ScopedMutex render(&epdf->renderLock);
        ScopedRecursiveMutex doc(&epdf->docLock);
        epdf->RunCadDetection();
    }
    epdf->ToggleCadEnhanceOverride();
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
    ScopedRecursiveMutex scope(&epdf->docLock);
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

    ScopedRecursiveMutex cs(&epdf->docLock);
    Vec<Annotation*> els;
    for (auto& annot : pi->annotations) {
        auto& atp = annot->type;
        RectF bounds = annot->bounds;
        if (!bounds.Contains(pos)) {
            continue;
        }
        VecAppend(els, annot);
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
    ScopedRecursiveMutex cs(&epdf->docLock);
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
    int idx = VecFind(ws, cur);
    if (n == 0 || idx < 0) {
        return nullptr;
    }
    // read type/flags via mupdf directly (this file is also compiled into
    // PdfPreview/PdfFilter, which don't link Annotation.cpp's GetWidget*)
    auto* ctx = epdf->Ctx();
    ScopedRecursiveMutex cs(&epdf->docLock);
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

static bool FormFieldValueIsEmpty(int wt, const char* val) {
    if (!val || !val[0]) {
        return true;
    }
    if (wt == PDF_WIDGET_TYPE_CHECKBOX || wt == PDF_WIDGET_TYPE_RADIOBUTTON) {
        return str::Eq(Str(val), StrL("Off"));
    }
    return str::IsEmptyOrWhiteSpace(Str(val));
}

// Page-space rects of empty fillable fields on pageNo (issue #5966). skip is
// the field currently being edited, if any, so its overlay isn't double-tinted.
void EngineMupdfGetFormFieldHighlightRects(EngineBase* engine, int pageNo, Annotation* skip, Vec<RectF>& out) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return;
    }
    FzPageInfo* pi = epdf->GetFzPageInfoCanFail(pageNo);
    if (!pi) {
        return;
    }
    auto* ctx = epdf->Ctx();
    ScopedRecursiveMutex cs(&epdf->docLock);
    for (Annotation* w : pi->widgets) {
        if (!w || w == skip || !w->pdfannot || w->bounds.IsEmpty()) {
            continue;
        }
        bool highlight = false;
        fz_try(ctx) {
            int aflags = pdf_annot_flags(ctx, w->pdfannot);
            int hidden = PDF_ANNOT_IS_HIDDEN | PDF_ANNOT_IS_NO_VIEW | PDF_ANNOT_IS_INVISIBLE;
            if (!(aflags & hidden)) {
                int flags = pdf_annot_field_flags(ctx, w->pdfannot);
                if (!(flags & PDF_FIELD_IS_READ_ONLY)) {
                    int wt = (int)pdf_widget_type(ctx, w->pdfannot);
                    if (wt == PDF_WIDGET_TYPE_SIGNATURE) {
                        highlight = !pdf_widget_is_signed(ctx, w->pdfannot);
                    } else if (wt == PDF_WIDGET_TYPE_TEXT || wt == PDF_WIDGET_TYPE_COMBOBOX ||
                               wt == PDF_WIDGET_TYPE_LISTBOX || wt == PDF_WIDGET_TYPE_CHECKBOX ||
                               wt == PDF_WIDGET_TYPE_RADIOBUTTON) {
                        highlight = FormFieldValueIsEmpty(wt, pdf_annot_field_value(ctx, w->pdfannot));
                    }
                }
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            highlight = false;
        }
        if (highlight) {
            VecAppend(out, w->bounds);
        }
    }
}

// Note: this code is compiled in release mode even if debug build so
// DEBUG is not defined so we can't do #if defined(DEBUG) here
// so we use this runtime boolean instead
static bool gSkipAnnotatoinValidation = true;

// check that pageInfo->annotations has the same info as in mupdf
static NO_INLINE void ValidateAnnotationsInSync(EngineMupdf* /*e*/, FzPageInfo* /*pageInfo*/) {
    if (gSkipAnnotatoinValidation) {
        return;
    }
    // TODO: write me
}

// in a function so that we can set a breakpoint or add logging
// to easily trace all places that modify annotations
void MarkNotificationAsModified(EngineMupdf* e, Annotation* annot) {
    MarkNotificationAsModified(e, annot, AnnotationChange::Modify);
}

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
    ScopedRecursiveMutex scope(&e->pagesLock);
    FzPageInfo* pageInfo = e->pages[pageIdx];

    if (change == AnnotationChange::Remove) {
        // Markup and form widgets live in separate vectors.
        int removedPos = VecRemove(pageInfo->annotations, annot);
        if (removedPos < 0) {
            removedPos = VecRemove(pageInfo->widgets, annot);
        }
        ReportIf(removedPos < 0); // must exist in one of the lists
        ValidateAnnotationsInSync(e, pageInfo);
    } else if (change == AnnotationChange::Add) {
        int sizeBefore = len(pageInfo->annotations);
        int pos = VecFind(pageInfo->annotations, annot);
        ReportIf(pos >= 0); // shouldn't exist
        VecAppend(pageInfo->annotations, annot);
        int sizeNow = len(pageInfo->annotations);
        ReportIf(sizeBefore != sizeNow - 1);
        ValidateAnnotationsInSync(e, pageInfo);
    } else {
        ReportIf(change != AnnotationChange::Modify);
    }
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex ctxScope(&e->docLock);
        RebuildCommentsFromAnnotations(ctx, pageInfo);
    }
    pageInfo->elementsNeedRebuilding = true;

    // cached display list captured the old annotations; drop it so the next
    // render rebuilds with the new state.
    {
        auto* ctx = e->Ctx();
        ScopedMutex rl(&e->renderLock);
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
    ScopedRecursiveMutex cs(&engine->docLock);

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
    TempStr res;
    fz_document* doc = nullptr;
    fz_outline* outline = nullptr;
    fz_buffer* buf = nullptr;
    fz_output* out = nullptr;
    fz_try(ctx) {
        doc = fz_open_document(ctx, CStrTemp(path));
        outline = fz_load_outline(ctx, doc);
        if (!outline) {
            res = str::DupTemp(StrL("(no outline)"));
        } else {
            buf = fz_new_buffer(ctx, 1024);
            out = fz_new_output_with_buffer(ctx, buf);
            outline_to_buffer_rec(ctx, out, outline, 0);
            fz_close_output(ctx, out);
            unsigned char* data;
            size_t n = fz_buffer_storage(ctx, buf, &data);
            res = str::DupTemp(Str((char*)data, (int)n));
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
    TempStr res;
    fz_buffer* buf = nullptr;
    fz_try(ctx) {
        buf = pdfinfo_to_buffer(ctx, path.s);
        unsigned char* data;
        size_t n = fz_buffer_storage(ctx, buf, &data);
        res = str::DupTemp(Str((char*)data, (int)n));
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    fz_drop_buffer(ctx, buf);
    fz_drop_context(ctx);
    return res;
}
