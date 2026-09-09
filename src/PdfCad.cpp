/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "PdfCad.h"

// CAD/engineering-drawing enhancement mode. Set by the app from the
// EngineeringDrawingEnhance pref; PdfPreview/PdfFilter and the macOS app don't
// link Settings, so they keep the default (Off), which also skips the
// per-document detection pass.
static EngineeringDrawingEnhanceMode gCadEnhanceMode = EngineeringDrawingEnhanceMode::Off;

// Parse the EngineeringDrawingEnhance pref ("off", "auto" or "on").
void SetEngineeringDrawingEnhanceMode(Str mode) {
    if (str::EqI(mode, StrL("auto"))) {
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
    if (overrideState == CadEnhanceOverride::ForceOn) {
        return true;
    }
    if (overrideState == CadEnhanceOverride::ForceOff) {
        return false;
    }
    EngineeringDrawingEnhanceMode mode = GetEngineeringDrawingEnhanceMode();
    if (mode == EngineeringDrawingEnhanceMode::On) {
        return true;
    }
    if (mode == EngineeringDrawingEnhanceMode::Off) {
        return false;
    }
    // Auto (or unknown): follow detection
    return detect.enable;
}

static bool ContainsAnyI(Str haystack, const char* const* needles, int count) {
    if (len(haystack) == 0) {
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
    if (len(field) == 0) {
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
    float maxC = std::max({r, g, b});
    float minC = std::min({r, g, b});
    float lum = (0.2126f * r) + (0.7152f * g) + (0.0722f * b);
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

static void cad_analysis_stroke_path(fz_context* ctx, fz_device* dev, const fz_path* /*path*/,
                                     const fz_stroke_state* stroke, fz_matrix /*ctm*/, fz_colorspace* colorspace,
                                     const float* color, float /*alpha*/, fz_color_params color_params) {
    cad_analysis_device* d = (cad_analysis_device*)dev;
    float rgb[FZ_MAX_COLORS] = {};
    fz_colorspace* ds = fz_device_rgb(ctx);
    fz_convert_color(ctx, colorspace, color, ds, rgb, colorspace, color_params);
    cad_analysis_note_stroke(d->stats, stroke, rgb[0], rgb[1], rgb[2]);
}

static void cad_analysis_fill_path(fz_context* /*ctx*/, fz_device* dev, const fz_path* /*path*/, int /*even_odd*/,
                                   fz_matrix /*ctm*/, fz_colorspace* /*colorspace*/, const float* /*color*/,
                                   float /*alpha*/, fz_color_params /*color_params*/) {
    cad_analysis_device* d = (cad_analysis_device*)dev;
    d->stats->fills++;
}

static void cad_analysis_fill_text(fz_context* ctx, fz_device* dev, const fz_text* /*text*/, fz_matrix /*ctm*/,
                                   fz_colorspace* colorspace, const float* color, float /*alpha*/,
                                   fz_color_params color_params) {
    cad_analysis_device* d = (cad_analysis_device*)dev;
    d->stats->textOps++;
    float rgb[FZ_MAX_COLORS] = {};
    fz_colorspace* ds = fz_device_rgb(ctx);
    fz_convert_color(ctx, colorspace, color, ds, rgb, colorspace, color_params);
    cad_analysis_note_stroke(d->stats, nullptr, rgb[0], rgb[1], rgb[2]);
}

static void cad_analysis_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text,
                                     const fz_stroke_state* /*stroke*/, fz_matrix ctm, fz_colorspace* colorspace,
                                     const float* color, float alpha, fz_color_params color_params) {
    cad_analysis_fill_text(ctx, dev, text, ctm, colorspace, color, alpha, color_params);
}

static void cad_analysis_fill_image(fz_context* /*ctx*/, fz_device* dev, fz_image* /*image*/, fz_matrix ctm,
                                    float /*alpha*/, fz_color_params /*color_params*/) {
    cad_analysis_device* d = (cad_analysis_device*)dev;
    fz_rect bbox = fz_transform_rect(fz_unit_rect, ctm);
    if (d->stats->pageArea > 0.f) {
        float coverage = CadRectArea(bbox) / d->stats->pageArea;
        d->stats->maxImageCoverage = std::max(coverage, d->stats->maxImageCoverage);
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

    // Illustrated books / art catalogs: prominent images with normal text (not CAD).
    // Without this, full-bleed scanned pages look like "raster CAD" and the
    // grayscale enhance pass ruins color (Sumatra #5806; sumatrapdf-plus 3.5.16).
    if (stats.maxImageCoverage >= 0.15f && stats.textOps >= 40) {
        score -= 65;
    }
    if (pageCount >= 80 && stats.maxImageCoverage >= 0.12f && stats.textOps >= 25) {
        score -= 40;
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
        if (pageCount > 30) {
            // Full-bleed cover art in multi-page books mimics raster CAD; do not
            // treat as engineering scan (would gray-enhance photos/artwork).
            score -= 70;
        } else {
            score += 55;
            if (rasterDominantOut) {
                *rasterDominantOut = true;
            }
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
            side = std::max(sideY, side);
            maxPageSide = std::max(side, maxPageSide);
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
    // Long multi-page books are never raster CAD screenshots (those are short
    // exports). Clear the flag so PdfCadEnhancePixmap does not gray-blend.
    if (rasterDominant && pageCount > 30) {
        res.rasterDominant = false;
        rasterDominant = false;
    }
    if (rasterDominant && res.score >= 45 && pageCount <= 30) {
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

// Darken CAD-export grays without changing text geometry.
typedef struct {
    fz_device super;
    fz_device* inner;
} pdf_cad_enhance_device;

static bool CadIsNeutralGray(float r, float g, float b, float* outLum) {
    float maxC = std::max({r, g, b});
    float minC = std::min({r, g, b});
    float lum = (0.2126f * r) + (0.7152f * g) + (0.0722f * b);
    if (outLum) {
        *outLum = lum;
    }
    float chroma = maxC - minC;
    if (chroma > 0.14f) {
        return false;
    }
    return lum >= 0.48f && lum <= 0.86f;
}

static float CadMatrixExpansion(fz_matrix ctm) {
    return sqrtf((ctm.a * ctm.a) + (ctm.b * ctm.b));
}

// Stronger when zoomed out (small CTM expansion), none when zoomed in.
static float CadEnhanceBlendForExpansion(float expansion) {
    if (expansion < 0.001f) {
        expansion = 1.f;
    }
    float blend = (0.84f - expansion) / 0.60f;
    blend = limitValue(blend, 0.f, 1.f);
    return blend;
}

static void CadBlendRgb(float r, float g, float b, float mr, float mg, float mb, float blend, float* outR, float* outG,
                        float* outB) {
    *outR = r + ((mr - r) * blend);
    *outG = g + ((mg - g) * blend);
    *outB = b + ((mb - b) * blend);
}

// Map typical CAD export grays toward Acrobat-like darker strokes (not pure black).
static void CadAcrobatGrayRgb(float r, float g, float b, float* outR, float* outG, float* outB) {
    float lum;
    if (!CadIsNeutralGray(r, g, b, &lum)) {
        *outR = r;
        *outG = g;
        *outB = b;
        return;
    }
    if (lum <= 0.50f) {
        *outR = r;
        *outG = g;
        *outB = b;
        return;
    }
    float t = (lum - 0.50f) / 0.32f;
    t = std::min(t, 1.f);
    float targetLum = 0.15f + (t * 0.21f);
    if (targetLum >= lum || lum < 0.0001f) {
        *outR = r;
        *outG = g;
        *outB = b;
        return;
    }
    float scale = targetLum / lum;
    *outR = r * scale;
    *outG = g * scale;
    *outB = b * scale;
}

static void CadMapColor(fz_context* ctx, fz_colorspace* cs, const float* color, fz_color_params colorParams,
                        fz_matrix ctm, float* mapped) {
    float rgb[FZ_MAX_COLORS] = {};
    fz_colorspace* ds = fz_device_rgb(ctx);
    fz_convert_color(ctx, cs, color, ds, rgb, cs, colorParams);
    float enhanced[FZ_MAX_COLORS] = {};
    CadAcrobatGrayRgb(rgb[0], rgb[1], rgb[2], &enhanced[0], &enhanced[1], &enhanced[2]);
    float blend = CadEnhanceBlendForExpansion(CadMatrixExpansion(ctm));
    CadBlendRgb(rgb[0], rgb[1], rgb[2], enhanced[0], enhanced[1], enhanced[2], blend, &mapped[0], &mapped[1],
                &mapped[2]);
}

static void cad_forward_close(fz_context* ctx, fz_device* dev) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    if (d->inner && d->inner->close_device) {
        d->inner->close_device(ctx, d->inner);
    }
}

static void cad_forward_drop(fz_context* ctx, fz_device* dev) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    if (d->inner) {
        fz_drop_device(ctx, d->inner);
        d->inner = nullptr;
    }
}

static void cad_stroke_path(fz_context* ctx, fz_device* dev, const fz_path* path, const fz_stroke_state* stroke,
                            fz_matrix ctm, fz_colorspace* colorspace, const float* color, float alpha,
                            fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    CadMapColor(ctx, colorspace, color, color_params, ctm, mapped);
    fz_stroke_path(ctx, d->inner, path, stroke, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void cad_fill_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm,
                          fz_colorspace* colorspace, const float* color, float alpha, fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    CadMapColor(ctx, colorspace, color, color_params, ctm, mapped);
    fz_fill_text(ctx, d->inner, text, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void cad_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text, const fz_stroke_state* stroke,
                            fz_matrix ctm, fz_colorspace* colorspace, const float* color, float alpha,
                            fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    CadMapColor(ctx, colorspace, color, color_params, ctm, mapped);
    fz_stroke_text(ctx, d->inner, text, stroke, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

// device pixels: below this on its shorter side, a filled path is a line an
// exporter drew as a thin rectangle rather than a region
constexpr float kCadFillLineMaxDy = 4.0f;

// This device darkens grays so thin CAD lines stay readable when zoomed out.
// Exporters do draw lines as thin filled rectangles, so fills can't be skipped
// outright, but a fill that covers real area is a region -- a building, a hatch
// block, a legend swatch -- and Acrobat leaves its gray alone. Darkening those
// too turned mid-grays near-black: 0.6 gray rendered as 54 and 0.8 gray as 88
// (issue #5937).
static bool CadFillIsLineLike(fz_context* ctx, const fz_path* path, fz_matrix ctm) {
    fz_rect r = fz_bound_path(ctx, path, nullptr, ctm);
    float dx = r.x1 - r.x0;
    float dy = r.y1 - r.y0;
    return std::min(dx, dy) <= kCadFillLineMaxDy;
}

static void cad_fill_path(fz_context* ctx, fz_device* dev, const fz_path* path, int even_odd, fz_matrix ctm,
                          fz_colorspace* colorspace, const float* color, float alpha, fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    if (!CadFillIsLineLike(ctx, path, ctm)) {
        fz_fill_path(ctx, d->inner, path, even_odd, ctm, colorspace, color, alpha, color_params);
        return;
    }
    float mapped[FZ_MAX_COLORS] = {};
    CadMapColor(ctx, colorspace, color, color_params, ctm, mapped);
    fz_fill_path(ctx, d->inner, path, even_odd, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void cad_fill_shade(fz_context* ctx, fz_device* dev, fz_shade* shd, fz_matrix ctm, float alpha,
                           fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_fill_shade(ctx, d->inner, shd, ctm, alpha, color_params);
}

static void cad_fill_image(fz_context* ctx, fz_device* dev, fz_image* image, fz_matrix ctm, float alpha,
                           fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_fill_image(ctx, d->inner, image, ctm, alpha, color_params);
}

static void cad_fill_image_mask(fz_context* ctx, fz_device* dev, fz_image* image, fz_matrix ctm,
                                fz_colorspace* colorspace, const float* color, float alpha,
                                fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_fill_image_mask(ctx, d->inner, image, ctm, colorspace, color, alpha, color_params);
}

static void cad_clip_path(fz_context* ctx, fz_device* dev, const fz_path* path, int even_odd, fz_matrix ctm,
                          fz_rect scissor) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_clip_path(ctx, d->inner, path, even_odd, ctm, scissor);
}

static void cad_clip_stroke_path(fz_context* ctx, fz_device* dev, const fz_path* path, const fz_stroke_state* stroke,
                                 fz_matrix ctm, fz_rect scissor) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_clip_stroke_path(ctx, d->inner, path, stroke, ctm, scissor);
}

static void cad_clip_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm, fz_rect scissor) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_clip_text(ctx, d->inner, text, ctm, scissor);
}

static void cad_clip_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text, const fz_stroke_state* stroke,
                                 fz_matrix ctm, fz_rect scissor) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_clip_stroke_text(ctx, d->inner, text, stroke, ctm, scissor);
}

static void cad_clip_image_mask(fz_context* ctx, fz_device* dev, fz_image* image, fz_matrix ctm, fz_rect scissor) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_clip_image_mask(ctx, d->inner, image, ctm, scissor);
}

static void cad_pop_clip(fz_context* ctx, fz_device* dev) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_pop_clip(ctx, d->inner);
}

static void cad_begin_mask(fz_context* ctx, fz_device* dev, fz_rect area, int luminosity, fz_colorspace* colorspace,
                           const float* bc, fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_begin_mask(ctx, d->inner, area, luminosity, colorspace, bc, color_params);
}

static void cad_end_mask(fz_context* ctx, fz_device* dev, fz_function* fn) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_end_mask_tr(ctx, d->inner, fn);
}

static void cad_begin_group(fz_context* ctx, fz_device* dev, fz_rect area, fz_colorspace* cs, int isolated,
                            int knockout, int blendmode, float alpha) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_begin_group(ctx, d->inner, area, cs, isolated, knockout, blendmode, alpha);
}

static void cad_end_group(fz_context* ctx, fz_device* dev) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_end_group(ctx, d->inner);
}

static int cad_begin_tile(fz_context* ctx, fz_device* dev, fz_rect area, fz_rect view, float xstep, float ystep,
                          fz_matrix ctm, int id, int doc_id) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    return fz_begin_tile_tid(ctx, d->inner, area, view, xstep, ystep, ctm, id, doc_id);
}

static void cad_end_tile(fz_context* ctx, fz_device* dev) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_end_tile(ctx, d->inner);
}

static void cad_render_flags(fz_context* ctx, fz_device* dev, int set, int clear) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_render_flags(ctx, d->inner, set, clear);
}

static void cad_set_default_colorspaces(fz_context* ctx, fz_device* dev, fz_default_colorspaces* default_cs) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_set_default_colorspaces(ctx, d->inner, default_cs);
}

static void cad_begin_layer(fz_context* ctx, fz_device* dev, const char* layer_name) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_begin_layer(ctx, d->inner, layer_name);
}

static void cad_end_layer(fz_context* ctx, fz_device* dev) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_end_layer(ctx, d->inner);
}

static void cad_begin_structure(fz_context* ctx, fz_device* dev, fz_structure standard, const char* raw, int idx) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_begin_structure(ctx, d->inner, standard, raw, idx);
}

static void cad_end_structure(fz_context* ctx, fz_device* dev) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_end_structure(ctx, d->inner);
}

static void cad_begin_metatext(fz_context* ctx, fz_device* dev, fz_metatext meta, const char* text) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_begin_metatext(ctx, d->inner, meta, text);
}

static void cad_end_metatext(fz_context* ctx, fz_device* dev) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_end_metatext(ctx, d->inner);
}

static void cad_ignore_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    fz_ignore_text(ctx, d->inner, text, ctm);
}

// Wrap <inner> in the CAD-enhancing pass-through device. Takes ownership of
// <inner>: dropping the wrapper drops it.
fz_device* PdfCadEnhanceWrapDevice(fz_context* ctx, fz_device* inner) {
    pdf_cad_enhance_device* d = fz_new_derived_device(ctx, pdf_cad_enhance_device);
    d->inner = inner;

    d->super.close_device = cad_forward_close;
    d->super.drop_device = cad_forward_drop;
    d->super.fill_path = cad_fill_path;
    d->super.stroke_path = cad_stroke_path;
    d->super.fill_text = cad_fill_text;
    d->super.stroke_text = cad_stroke_text;
    d->super.fill_shade = cad_fill_shade;
    d->super.fill_image = cad_fill_image;
    d->super.fill_image_mask = cad_fill_image_mask;
    d->super.clip_path = cad_clip_path;
    d->super.clip_stroke_path = cad_clip_stroke_path;
    d->super.clip_text = cad_clip_text;
    d->super.clip_stroke_text = cad_clip_stroke_text;
    d->super.clip_image_mask = cad_clip_image_mask;
    d->super.pop_clip = cad_pop_clip;
    d->super.begin_mask = cad_begin_mask;
    d->super.end_mask = cad_end_mask;
    d->super.begin_group = cad_begin_group;
    d->super.end_group = cad_end_group;
    d->super.begin_tile = cad_begin_tile;
    d->super.end_tile = cad_end_tile;
    d->super.render_flags = cad_render_flags;
    d->super.set_default_colorspaces = cad_set_default_colorspaces;
    d->super.begin_layer = cad_begin_layer;
    d->super.end_layer = cad_end_layer;
    d->super.begin_structure = cad_begin_structure;
    d->super.end_structure = cad_end_structure;
    d->super.begin_metatext = cad_begin_metatext;
    d->super.end_metatext = cad_end_metatext;
    d->super.ignore_text = cad_ignore_text;

    return &d->super;
}

static unsigned char CadClampByte(float v) {
    if (v <= 0.f) {
        return 0;
    }
    if (v >= 255.f) {
        return 255;
    }
    return (unsigned char)lroundf(v);
}

// Post-process a rendered page bitmap for raster/screenshot CAD PDFs: darken
// mid-gray pixels toward Acrobat-like line contrast, leaving near-white
// background alone. Used instead of the wrap device for pages whose content
// is one big embedded image.
void PdfCadEnhancePixmap(fz_context* ctx, fz_pixmap* pix, float zoom, bool rasterDominant) {
    if (!pix || !rasterDominant) {
        return;
    }
    // we read s[0], s[1], s[2] as R, G, B, so anything else (e.g. CMYK, which
    // also passes an n >= 3 test) would be misinterpreted
    if (!fz_colorspace_is_rgb(ctx, pix->colorspace)) {
        return;
    }

    float expansion = zoom > 0.01f ? 1.f / zoom : 1.f;
    float blend = CadEnhanceBlendForExpansion(expansion);
    blend = std::max(blend, 0.55f);

    unsigned char* s = pix->samples;
    int n = pix->n;
    for (int y = 0; y < pix->h; y++) {
        for (int x = 0; x < pix->w; x++) {
            float fr = (float)s[0] / 255.f;
            float fg = (float)s[1] / 255.f;
            float fb = (float)s[2] / 255.f;
            if (fr > 0.96f && fg > 0.96f && fb > 0.96f) {
                s += n;
                continue;
            }

            float outR, outG, outB;
            CadAcrobatGrayRgb(fr, fg, fb, &outR, &outG, &outB);
            CadBlendRgb(fr, fg, fb, outR, outG, outB, blend, &outR, &outG, &outB);

            float lum = (0.2126f * outR) + (0.7152f * outG) + (0.0722f * outB);
            float maxC = std::max({outR, outG, outB});
            float minC = std::min({outR, outG, outB});
            if (lum > 0.40f && lum < 0.90f && maxC - minC < 0.15f) {
                float factor = 1.f - (0.28f * blend * (lum - 0.40f) / 0.50f);
                outR *= factor;
                outG *= factor;
                outB *= factor;
            }

            s[0] = CadClampByte(outR * 255.f);
            s[1] = CadClampByte(outG * 255.f);
            s[2] = CadClampByte(outB * 255.f);
            s += n;
        }
        s += pix->stride - ((size_t)pix->w * n);
    }
}

static float CadMinLineWidthForZoom(float zoom, bool hairlineDoc) {
    float z = zoom;
    z = std::max(z, 0.20f);
    // Device pixels. Hairline CAD needs a modest floor; avoid double-boosting with stroke rewrites.
    float minLw = hairlineDoc ? (0.50f + (0.55f / z)) : (0.14f + (0.38f / z));
    float maxLw = hairlineDoc ? 1.25f : 0.62f;
    float minFloor = hairlineDoc ? 0.50f : 0.14f;
    minLw = std::min(minLw, maxLw);
    minLw = std::max(minLw, minFloor);
    return minLw;
}

// RAII: raise the context's minimum rendered line width for the duration of a
// page render, so hairlines stay visible when zoomed out. Per-thread context
// state (Ctx() clones), so no locking needed.
CadMinLineWidthScope::CadMinLineWidthScope(fz_context* ctxIn, float zoom, bool activeIn, bool hairlineDoc) {
    if (!activeIn) {
        return;
    }
    ctx = ctxIn;
    active = true;
    saved = fz_graphics_min_line_width(ctx);
    fz_set_graphics_min_line_width(ctx, CadMinLineWidthForZoom(zoom, hairlineDoc));
}

CadMinLineWidthScope::~CadMinLineWidthScope() {
    if (active && ctx) {
        fz_set_graphics_min_line_width(ctx, saved);
    }
}
