/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "PdfCadEnhanceDevice.h"

// A pass-through fz_device that remaps typical CAD-export grays toward
// Acrobat-like darker strokes, scaled by zoom (stronger when zoomed out,
// none when zoomed in), and slightly emboldens the tiny text matrices that
// WPS-style hairline exports use.
typedef struct {
    fz_device super;
    fz_device* inner;
    CadEnhanceRenderOpts opts;
} pdf_cad_enhance_device;

static bool CadIsNeutralGray(float r, float g, float b, float* outLum) {
    float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
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
    return sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
}

// WPS and similar tools export CAD labels with a tiny text matrix (~0.05).
static fz_matrix CadEmboldenTinyTextMatrix(fz_matrix ctm, bool hairlineDoc) {
    float expansion = CadMatrixExpansion(ctm);
    if (!hairlineDoc || expansion >= 0.22f) {
        return ctm;
    }
    float boost;
    if (expansion <= 0.08f) {
        boost = 1.55f;
    } else {
        boost = 1.f + (0.22f - expansion) * 2.5f;
        if (boost > 1.55f) {
            boost = 1.55f;
        }
    }
    return fz_concat(ctm, fz_scale(boost, boost));
}

// Stronger when zoomed out (small CTM expansion), none when zoomed in.
static float CadEnhanceBlendForExpansion(float expansion) {
    if (expansion < 0.001f) {
        expansion = 1.f;
    }
    float blend = (0.84f - expansion) / 0.60f;
    if (blend < 0.f) {
        blend = 0.f;
    }
    if (blend > 1.f) {
        blend = 1.f;
    }
    return blend;
}

static void CadBlendRgb(float r, float g, float b, float mr, float mg, float mb, float blend, float* outR, float* outG,
                        float* outB) {
    *outR = r + (mr - r) * blend;
    *outG = g + (mg - g) * blend;
    *outB = b + (mb - b) * blend;
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
    if (t > 1.f) {
        t = 1.f;
    }
    float targetLum = 0.15f + t * 0.21f;
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
    ctm = CadEmboldenTinyTextMatrix(ctm, d->opts.hairlineVector);
    fz_fill_text(ctx, d->inner, text, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void cad_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text, const fz_stroke_state* stroke,
                            fz_matrix ctm, fz_colorspace* colorspace, const float* color, float alpha,
                            fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    CadMapColor(ctx, colorspace, color, color_params, ctm, mapped);
    ctm = CadEmboldenTinyTextMatrix(ctm, d->opts.hairlineVector);
    fz_stroke_text(ctx, d->inner, text, stroke, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void cad_fill_path(fz_context* ctx, fz_device* dev, const fz_path* path, int even_odd, fz_matrix ctm,
                          fz_colorspace* colorspace, const float* color, float alpha, fz_color_params color_params) {
    pdf_cad_enhance_device* d = (pdf_cad_enhance_device*)dev;
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
fz_device* PdfCadEnhanceWrapDevice(fz_context* ctx, fz_device* inner, const CadEnhanceRenderOpts& opts) {
    pdf_cad_enhance_device* d = fz_new_derived_device(ctx, pdf_cad_enhance_device);
    d->inner = inner;
    d->opts = opts;

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
    return (unsigned char)(v + 0.5f);
}

// Post-process a rendered page bitmap for raster/screenshot CAD PDFs: darken
// mid-gray pixels toward Acrobat-like line contrast, leaving near-white
// background alone. Used instead of the wrap device for pages whose content
// is one big embedded image.
void PdfCadEnhancePixmap(fz_context*, fz_pixmap* pix, float zoom, bool rasterDominant) {
    if (!pix || pix->n < 3 || !rasterDominant) {
        return;
    }

    float expansion = zoom > 0.01f ? 1.f / zoom : 1.f;
    float blend = CadEnhanceBlendForExpansion(expansion);
    if (blend < 0.55f) {
        blend = 0.55f;
    }

    unsigned char* s = pix->samples;
    int n = pix->n;
    int n1 = pix->n - pix->alpha;
    for (int y = 0; y < pix->h; y++) {
        for (int x = 0; x < pix->w; x++) {
            float fr = s[0] / 255.f;
            float fg = s[1] / 255.f;
            float fb = s[2] / 255.f;
            if (fr > 0.96f && fg > 0.96f && fb > 0.96f) {
                s += n;
                continue;
            }

            float outR, outG, outB;
            CadAcrobatGrayRgb(fr, fg, fb, &outR, &outG, &outB);
            CadBlendRgb(fr, fg, fb, outR, outG, outB, blend, &outR, &outG, &outB);

            float lum = 0.2126f * outR + 0.7152f * outG + 0.0722f * outB;
            float maxC = outR > outG ? (outR > outB ? outR : outB) : (outG > outB ? outG : outB);
            float minC = outR < outG ? (outR < outB ? outR : outB) : (outG < outB ? outG : outB);
            if (lum > 0.40f && lum < 0.90f && maxC - minC < 0.15f) {
                float factor = 1.f - 0.28f * blend * (lum - 0.40f) / 0.50f;
                outR *= factor;
                outG *= factor;
                outB *= factor;
            }

            for (int k = 0; k < n1; k++) {
                float c = k == 0 ? outR : (k == 1 ? outG : outB);
                s[k] = CadClampByte(c * 255.f);
            }
            s += n;
        }
        s += pix->stride - pix->w * n;
    }
}

static float CadMinLineWidthForZoom(float zoom, bool hairlineDoc) {
    float z = zoom;
    if (z < 0.20f) {
        z = 0.20f;
    }
    // Device pixels. Hairline CAD needs a modest floor; avoid double-boosting with stroke rewrites.
    float minLw = hairlineDoc ? (0.50f + 0.55f / z) : (0.14f + 0.38f / z);
    float maxLw = hairlineDoc ? 1.25f : 0.62f;
    float minFloor = hairlineDoc ? 0.50f : 0.14f;
    if (minLw > maxLw) {
        minLw = maxLw;
    }
    if (minLw < minFloor) {
        minLw = minFloor;
    }
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
