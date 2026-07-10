/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "base/Base.h"

#include "PdfCadDetect.h"

// CAD/engineering-drawing enhancement mode. Set by the app from the
// EngineeringDrawingEnhance pref; PdfPreview/PdfFilter and the macOS app don't
// link GlobalPrefs, so they keep the default (Off), which also skips the
// per-document detection pass.
static EngineeringDrawingEnhanceMode gCadEnhanceMode = EngineeringDrawingEnhanceMode::Off;

// Parse the EngineeringDrawingEnhance pref ("off", "auto" or "on").
void SetEngineeringDrawingEnhanceMode(Str mode) {
    if (!mode || str::EqI(mode, StrL("auto"))) {
        gCadEnhanceMode = EngineeringDrawingEnhanceMode::Auto;
    } else if (str::EqI(mode, StrL("on"))) {
        gCadEnhanceMode = EngineeringDrawingEnhanceMode::On;
    } else {
        gCadEnhanceMode = EngineeringDrawingEnhanceMode::Off;
    }
}

EngineeringDrawingEnhanceMode GetEngineeringDrawingEnhanceMode() {
    return gCadEnhanceMode;
}

const char* CadEnhanceReasonName(CadEnhanceReason reason) {
    switch (reason) {
        case CadEnhanceReason::Pdfe:
            return "PDF/E";
        case CadEnhanceReason::Metadata:
            return "metadata";
        case CadEnhanceReason::Heuristic:
            return "heuristic";
        case CadEnhanceReason::RasterImage:
            return "raster-image";
        default:
            return "none";
    }
}

// The manual toggle wins over the global mode, which wins over auto-detection.
bool CadEnhanceEnabledForEngine(const CadDetectResult& detect, CadEnhanceOverride overrideState) {
    switch (overrideState) {
        case CadEnhanceOverride::ForceOn:
            return true;
        case CadEnhanceOverride::ForceOff:
            return false;
        default:
            break;
    }
    switch (GetEngineeringDrawingEnhanceMode()) {
        case EngineeringDrawingEnhanceMode::On:
            return true;
        case EngineeringDrawingEnhanceMode::Off:
            return false;
        case EngineeringDrawingEnhanceMode::Auto:
        default:
            return detect.enable;
    }
}

static bool ContainsAnyI(Str haystack, const char* const* needles, int count) {
    if (!haystack) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (str::ContainsI(haystack, Str(needles[i]))) {
            return true;
        }
    }
    return false;
}

// Creator/Producer values of CAD authoring tools and CAD-to-PDF converters.
static const char* kMetadataStrong[] = {
    "autocad",    "dwg to pdf", "dwg trueview", "revit",    "microstation", "solidworks", "catia",
    " creo",      " nx ",       "zwcad",        "gstarcad", "浩辰",         "中望",       "bluebeam",
    "pdffactory", "tekla",      "sketchup",     "archicad", "vectorworks",  "bentley",
};

static const char* kMetadataWeak[] = {
    "cad",       "dwg",        "plot",           "engineering", "layout", "draft", "mechanical",
    "architect", "screenshot", "screen capture", "snipaste",    "截图",   "wps",
};

// Producers that never emit CAD drawings; any match disables detection.
static const char* kMetadataBlacklist[] = {
    "microsoft word", "libreoffice", "openoffice", "indesign", "itext",     "pdflatex", "xelatex",
    "lualatex",       "latex",       " prince",    "chrome",   "skia/pdf",  "mozilla",  "calibre",
    "epub",           "powerpoint",  "excel",      "onenote",  "doctotext",
};

// PDF/E is the ISO profile for engineering documents; its marker alone is proof.
static bool HasPdfEMarker(fz_context* ctx, pdf_document* doc) {
    pdf_obj* trailer = pdf_trailer(ctx, doc);
    pdf_obj* info = pdf_dict_get(ctx, trailer, PDF_NAME(Info));
    if (info) {
        pdf_obj* v = pdf_dict_gets(ctx, info, "ISO_PDFEVersion");
        if (pdf_is_string(ctx, v)) {
            return true;
        }
    }

    pdf_obj* root = pdf_dict_get(ctx, trailer, PDF_NAME(Root));
    pdf_obj* meta = pdf_dict_get(ctx, root, PDF_NAME(Metadata));
    if (!meta) {
        return false;
    }

    fz_buffer* buf = nullptr;
    bool found = false;
    fz_var(buf);
    fz_var(found);
    fz_try(ctx) {
        buf = pdf_load_stream(ctx, meta);
        unsigned char* data = nullptr;
        size_t len = fz_buffer_storage(ctx, buf, &data);
        if (data && len > 0) {
            Str xmp((const char*)data, (int)len);
            found = str::Contains(xmp, StrL("pdfe:ISO_PDFEVersion")) || str::Contains(xmp, StrL("PDF/E-1")) ||
                    str::Contains(xmp, StrL("PDF/E-2"));
        }
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return found;
}

struct CadMetadataScore {
    int score = 0;
    bool strong = false;
    bool blacklisted = false;
};

static void ScoreMetadataField(Str field, CadMetadataScore* acc) {
    if (!field) {
        return;
    }
    if (ContainsAnyI(field, kMetadataBlacklist, dimof(kMetadataBlacklist))) {
        acc->blacklisted = true;
        return;
    }
    if (ContainsAnyI(field, kMetadataStrong, dimof(kMetadataStrong))) {
        acc->strong = true;
        acc->score += 40;
    } else if (ContainsAnyI(field, kMetadataWeak, dimof(kMetadataWeak))) {
        acc->score += 15;
    }
}

static void ScoreMetadataInfoKey(fz_context* ctx, pdf_obj* info, const char* key, CadMetadataScore* acc) {
    if (!info) {
        return;
    }
    pdf_obj* val = pdf_dict_gets(ctx, info, key);
    if (pdf_is_string(ctx, val)) {
        ScoreMetadataField(Str(pdf_to_text_string(ctx, val)), acc);
    }
}

// Score Creator/Producer and the XMP metadata stream against the keyword lists.
// A blacklist hit returns a large negative score that disables detection.
static int ScoreMetadata(fz_context* ctx, pdf_document* doc, bool* strongMatchOut) {
    CadMetadataScore acc;
    pdf_obj* trailer = pdf_trailer(ctx, doc);
    pdf_obj* info = pdf_dict_get(ctx, trailer, PDF_NAME(Info));
    ScoreMetadataInfoKey(ctx, info, "Creator", &acc);
    ScoreMetadataInfoKey(ctx, info, "Producer", &acc);

    pdf_obj* root = pdf_dict_get(ctx, trailer, PDF_NAME(Root));
    pdf_obj* meta = pdf_dict_get(ctx, root, PDF_NAME(Metadata));
    if (meta && !acc.blacklisted) {
        fz_buffer* buf = nullptr;
        fz_var(buf);
        fz_try(ctx) {
            buf = pdf_load_stream(ctx, meta);
            unsigned char* data = nullptr;
            size_t len = fz_buffer_storage(ctx, buf, &data);
            if (data && len > 0) {
                ScoreMetadataField(Str((const char*)data, (int)len), &acc);
            }
        }
        fz_always(ctx) {
            fz_drop_buffer(ctx, buf);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }

    *strongMatchOut = acc.strong;
    if (acc.blacklisted) {
        return -100;
    }
    return acc.score;
}

// Per-page content statistics collected by the analysis device.
struct CadPageStats {
    int strokes = 0;
    int fills = 0;
    int textOps = 0;
    int grayStrokes = 0;
    int thinStrokes = 0;
    float maxImageCoverage = 0.f;
    float pageArea = 0.f;
};

typedef struct {
    fz_device super;
    CadPageStats* stats;
} cad_analysis_device;

static float CadRectArea(fz_rect r) {
    if (fz_is_empty_rect(r) || fz_is_infinite_rect(r)) {
        return 0.f;
    }
    return (r.x1 - r.x0) * (r.y1 - r.y0);
}

// Mid-luminance, low-chroma colors typical of CAD line work.
static bool CadIsGrayRgb(float r, float g, float b) {
    float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float chroma = maxC - minC;
    if (chroma > 0.12f) {
        return false;
    }
    return lum >= 0.38f && lum <= 0.88f;
}

static void cad_analysis_note_stroke(CadPageStats* stats, const fz_stroke_state* stroke, float r, float g, float b) {
    stats->strokes++;
    if (CadIsGrayRgb(r, g, b)) {
        stats->grayStrokes++;
    }
    if (stroke && stroke->linewidth <= 0.25f) {
        stats->thinStrokes++;
    }
}

static void cad_analysis_stroke_path(fz_context* ctx, fz_device* dev, const fz_path*, const fz_stroke_state* stroke,
                                     fz_matrix, fz_colorspace* colorspace, const float* color, float,
                                     fz_color_params color_params) {
    cad_analysis_device* d = (cad_analysis_device*)dev;
    float rgb[FZ_MAX_COLORS] = {};
    fz_colorspace* ds = fz_device_rgb(ctx);
    fz_convert_color(ctx, colorspace, color, ds, rgb, colorspace, color_params);
    cad_analysis_note_stroke(d->stats, stroke, rgb[0], rgb[1], rgb[2]);
}

static void cad_analysis_fill_path(fz_context*, fz_device* dev, const fz_path*, int, fz_matrix, fz_colorspace*,
                                   const float*, float, fz_color_params) {
    cad_analysis_device* d = (cad_analysis_device*)dev;
    d->stats->fills++;
}

static void cad_analysis_fill_text(fz_context* ctx, fz_device* dev, const fz_text*, fz_matrix,
                                   fz_colorspace* colorspace, const float* color, float, fz_color_params color_params) {
    cad_analysis_device* d = (cad_analysis_device*)dev;
    d->stats->textOps++;
    float rgb[FZ_MAX_COLORS] = {};
    fz_colorspace* ds = fz_device_rgb(ctx);
    fz_convert_color(ctx, colorspace, color, ds, rgb, colorspace, color_params);
    cad_analysis_note_stroke(d->stats, nullptr, rgb[0], rgb[1], rgb[2]);
}

static void cad_analysis_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text, const fz_stroke_state*,
                                     fz_matrix ctm, fz_colorspace* colorspace, const float* color, float alpha,
                                     fz_color_params color_params) {
    cad_analysis_fill_text(ctx, dev, text, ctm, colorspace, color, alpha, color_params);
}

static void cad_analysis_fill_image(fz_context*, fz_device* dev, fz_image*, fz_matrix ctm, float, fz_color_params) {
    cad_analysis_device* d = (cad_analysis_device*)dev;
    fz_rect bbox = fz_transform_rect(fz_unit_rect, ctm);
    if (d->stats->pageArea > 0.f) {
        float coverage = CadRectArea(bbox) / d->stats->pageArea;
        if (coverage > d->stats->maxImageCoverage) {
            d->stats->maxImageCoverage = coverage;
        }
    }
}

static fz_device* NewCadAnalysisDevice(fz_context* ctx, CadPageStats* stats) {
    cad_analysis_device* d = fz_new_derived_device(ctx, cad_analysis_device);
    d->super.stroke_path = cad_analysis_stroke_path;
    d->super.fill_path = cad_analysis_fill_path;
    d->super.fill_text = cad_analysis_fill_text;
    d->super.stroke_text = cad_analysis_stroke_text;
    d->super.fill_image = cad_analysis_fill_image;
    d->stats = stats;
    return &d->super;
}

static int CountOcgLayers(fz_context* ctx, pdf_document* doc) {
    pdf_obj* trailer = pdf_trailer(ctx, doc);
    pdf_obj* root = pdf_dict_get(ctx, trailer, PDF_NAME(Root));
    pdf_obj* ocp = pdf_dict_get(ctx, root, PDF_NAME(OCProperties));
    if (!ocp) {
        return 0;
    }
    pdf_obj* ocgs = pdf_dict_get(ctx, ocp, PDF_NAME(OCGs));
    if (!pdf_is_array(ctx, ocgs)) {
        return 0;
    }
    return pdf_array_len(ctx, ocgs);
}

// Square annotations are common markup in reviewed engineering drawings.
static int CountSquareAnnots(fz_context* ctx, pdf_document* doc, int pageCount) {
    int count = 0;
    int pages = pageCount > 3 ? 3 : pageCount;
    for (int i = 0; i < pages; i++) {
        pdf_obj* pageObj = pdf_lookup_page_obj(ctx, doc, i);
        pdf_obj* annots = pdf_dict_get(ctx, pageObj, PDF_NAME(Annots));
        if (!pdf_is_array(ctx, annots)) {
            continue;
        }
        int n = pdf_array_len(ctx, annots);
        for (int j = 0; j < n; j++) {
            pdf_obj* annot = pdf_array_get(ctx, annots, j);
            pdf_obj* subtype = pdf_dict_get(ctx, annot, PDF_NAME(Subtype));
            if (pdf_name_eq(ctx, subtype, PDF_NAME(Square))) {
                count++;
            }
        }
    }
    return count;
}

// Run one page's content stream through the analysis device, accumulating
// into <stats> (which the device references directly).
static void AnalyzePage(fz_context* ctx, pdf_document* doc, int pageNo, CadPageStats* stats) {
    pdf_page* page = nullptr;
    fz_device* dev = nullptr;
    fz_var(page);
    fz_var(dev);
    fz_try(ctx) {
        page = pdf_load_page(ctx, doc, pageNo);
        dev = NewCadAnalysisDevice(ctx, stats);
        pdf_run_page_contents(ctx, page, dev, fz_identity, nullptr);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_page(ctx, (fz_page*)page);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

// Score page content: CAD drawings are dominated by thin, gray, unfilled
// strokes with little text, often on large pages with OCG layers.
static int ScoreHeuristic(fz_context* ctx, pdf_document* doc, int pageCount, float maxPageSide, bool* rasterDominantOut,
                          bool* hairlineVectorOut) {
    CadPageStats stats;
    stats.pageArea = maxPageSide * maxPageSide;
    int pages = pageCount > 2 ? 2 : pageCount;
    for (int i = 0; i < pages; i++) {
        AnalyzePage(ctx, doc, i, &stats);
    }

    int score = 0;
    int strokeFillDenom = stats.strokes + stats.fills;
    float strokeFillRatio = strokeFillDenom > 0 ? (float)stats.strokes / (float)strokeFillDenom : 0.f;
    if (strokeFillRatio > 0.85f) {
        score += 25;
    }

    float grayRatio = stats.strokes > 0 ? (float)stats.grayStrokes / (float)stats.strokes : 0.f;
    if (grayRatio > 0.30f) {
        score += 25;
    }

    float thinRatio = stats.strokes > 0 ? (float)stats.thinStrokes / (float)stats.strokes : 0.f;
    if (thinRatio > 0.20f) {
        score += 15;
    }

    if (CountOcgLayers(ctx, doc) >= 2) {
        score += 15;
    }

    // A4 landscape and larger
    if (maxPageSide >= 842.f) {
        score += 10;
    }

    float textStrokeRatio = stats.strokes > 0 ? (float)stats.textOps / (float)stats.strokes : 0.f;
    if (textStrokeRatio < 0.15f) {
        score += 10;
    }

    int squareAnnots = CountSquareAnnots(ctx, doc, pageCount);
    if (squareAnnots > 20) {
        score += 5;
    }

    // Text-heavy documents with many fills are most likely not drawings.
    if (stats.textOps > 500 && strokeFillRatio < 0.5f) {
        score -= 30;
    }

    // Hairline vector exports (e.g. WPS "print to PDF" from a CAD screenshot):
    // dense 0.05pt strokes, almost no embedded bitmap.
    bool hairlineCad =
        stats.maxImageCoverage < 0.05f && stats.strokes >= 40 && thinRatio > 0.25f && strokeFillRatio > 0.55f;
    if (hairlineCad) {
        score += 35;
        if (hairlineVectorOut) {
            *hairlineVectorOut = true;
        }
    }

    // Screenshot / raster CAD: one large image per page, almost no vector content.
    bool rasterCad = stats.maxImageCoverage >= 0.80f && stats.strokes + stats.fills < 50 && stats.textOps < 200;
    if (rasterCad) {
        score += 55;
        if (rasterDominantOut) {
            *rasterDominantOut = true;
        }
    } else if (stats.maxImageCoverage > 0.5f) {
        score -= 40;
    }

    return score;
}

// Decide whether <doc> looks like a CAD / engineering drawing. Checked in
// order of confidence: PDF/E marker, then authoring-tool metadata, then
// content heuristics on the first pages.
CadDetectResult DetectCadPdf(fz_context* ctx, pdf_document* doc) {
    CadDetectResult res;
    if (!ctx || !doc) {
        return res;
    }

    if (HasPdfEMarker(ctx, doc)) {
        res.enable = true;
        res.reason = CadEnhanceReason::Pdfe;
        res.score = 100;
        return res;
    }

    bool strongMetadata = false;
    int metadataScore = ScoreMetadata(ctx, doc, &strongMetadata);
    if (metadataScore <= -100) {
        return res;
    }
    if (strongMetadata) {
        res.enable = true;
        res.reason = CadEnhanceReason::Metadata;
        res.score = metadataScore;
        return res;
    }

    int pageCount = pdf_count_pages(ctx, doc);
    float maxPageSide = 0.f;
    int samplePages = pageCount > 3 ? 3 : pageCount;
    for (int i = 0; i < samplePages; i++) {
        pdf_page* page = nullptr;
        fz_var(page);
        fz_try(ctx) {
            page = pdf_load_page(ctx, doc, i);
            fz_rect bounds = pdf_bound_page(ctx, page, FZ_CROP_BOX);
            float side = bounds.x1 - bounds.x0;
            float sideY = bounds.y1 - bounds.y0;
            if (sideY > side) {
                side = sideY;
            }
            if (side > maxPageSide) {
                maxPageSide = side;
            }
        }
        fz_always(ctx) {
            fz_drop_page(ctx, (fz_page*)page);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }

    bool rasterDominant = false;
    bool hairlineVector = false;
    int heuristicScore = 0;
    fz_var(heuristicScore);
    fz_try(ctx) {
        heuristicScore = ScoreHeuristic(ctx, doc, pageCount, maxPageSide, &rasterDominant, &hairlineVector);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    res.score = heuristicScore + metadataScore;
    res.rasterDominant = rasterDominant;
    res.hairlineVector = hairlineVector;
    if (rasterDominant && res.score >= 45) {
        res.enable = true;
        res.reason = CadEnhanceReason::RasterImage;
        return res;
    }
    if (hairlineVector && res.score >= 45) {
        res.enable = true;
        res.reason = CadEnhanceReason::Heuristic;
        return res;
    }
    if (!strongMetadata && metadataScore > 0 && res.score >= 45) {
        res.enable = true;
        res.reason = CadEnhanceReason::Heuristic;
        return res;
    }
    if (res.score >= 60) {
        res.enable = true;
        res.reason = CadEnhanceReason::Heuristic;
    }
    return res;
}
