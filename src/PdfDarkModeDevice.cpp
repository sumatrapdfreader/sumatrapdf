/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
}

#include "base/Base.h"

#include "PdfDarkMode.h"
#include "PdfDarkModeInternal.h"

#include <stdio.h>

typedef struct {
    fz_device super;
    fz_device* inner;
    DarkModePageAnalysis* analysis;
    const DarkModePalette* palette;
    DarkModeReplayState* replayState;
    DarkModeEngineCache* engineCache;
    u32 profileHash;
    bool debugOverlay;
} pdf_dark_mode_device;

static DarkImagePolicy dm_current_image_policy(pdf_dark_mode_device* d) {
    if (!d->analysis || !d->replayState) {
        return DarkImagePolicy::Preserve;
    }
    int idx = d->replayState->nextImageOccurrence;
    if (idx < 0 || idx >= len(d->analysis->images)) {
        return DarkImagePolicy::Preserve;
    }
    return d->analysis->images[idx].policy;
}

static void dm_next_image_occurrence(pdf_dark_mode_device* d) {
    if (d->replayState) {
        d->replayState->nextImageOccurrence++;
    }
}

static void dm_map_color(pdf_dark_mode_device* d, fz_context* ctx, fz_colorspace* cs, const float* color,
                         fz_color_params colorParams, float* mapped) {
    MapColorToDarkTheme(ctx, cs, color, colorParams, *d->palette, mapped);
}

static void dm_map_fill_color(pdf_dark_mode_device* d, fz_context* ctx, fz_colorspace* cs, const float* color,
                              fz_color_params colorParams, float* mapped) {
    MapFillColorToDarkTheme(ctx, cs, color, colorParams, *d->palette, mapped);
}

static void dm_forward_close(fz_context* ctx, fz_device* dev) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    if (d->inner && d->inner->close_device) {
        d->inner->close_device(ctx, d->inner);
    }
}

static void dm_forward_drop(fz_context* ctx, fz_device* dev) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    if (d->inner) {
        fz_drop_device(ctx, d->inner);
        d->inner = nullptr;
    }
}

#define DM_FORWARD1(name, t1, v1)                                       \
    static void dm_fwd_##name(fz_context* ctx, fz_device* dev, t1 v1) { \
        pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;           \
        if (d->inner && d->inner->name) {                               \
            d->inner->name(ctx, d->inner, v1);                          \
        }                                                               \
    }

#define DM_FORWARD_CLIP(name)                                                                                    \
    static void dm_fwd_##name(fz_context* ctx, fz_device* dev, const fz_path* path, int even_odd, fz_matrix ctm, \
                              fz_rect scissor) {                                                                 \
        pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;                                                    \
        if (d->inner && d->inner->name) {                                                                        \
            d->inner->name(ctx, d->inner, path, even_odd, ctm, scissor);                                         \
        }                                                                                                        \
    }

static void dm_fill_path(fz_context* ctx, fz_device* dev, const fz_path* path, int even_odd, fz_matrix ctm,
                         fz_colorspace* colorspace, const float* color, float alpha, fz_color_params color_params) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    dm_map_fill_color(d, ctx, colorspace, color, color_params, mapped);
    fz_fill_path(ctx, d->inner, path, even_odd, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void dm_stroke_path(fz_context* ctx, fz_device* dev, const fz_path* path, const fz_stroke_state* stroke,
                           fz_matrix ctm, fz_colorspace* colorspace, const float* color, float alpha,
                           fz_color_params color_params) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    dm_map_color(d, ctx, colorspace, color, color_params, mapped);
    fz_stroke_path(ctx, d->inner, path, stroke, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void dm_fill_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm, fz_colorspace* colorspace,
                         const float* color, float alpha, fz_color_params color_params) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    dm_map_color(d, ctx, colorspace, color, color_params, mapped);
    fz_fill_text(ctx, d->inner, text, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void dm_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text, const fz_stroke_state* stroke,
                           fz_matrix ctm, fz_colorspace* colorspace, const float* color, float alpha,
                           fz_color_params color_params) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    dm_map_color(d, ctx, colorspace, color, color_params, mapped);
    fz_stroke_text(ctx, d->inner, text, stroke, ctm, fz_device_rgb(ctx), mapped, alpha, color_params);
}

static void dm_fill_shade(fz_context* ctx, fz_device* dev, fz_shade* shd, fz_matrix ctm, float alpha,
                          fz_color_params color_params) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;

    if (alpha == 0) {
        return;
    }

    if (!d->analysis || !d->palette || !shd) {
        fz_fill_shade(ctx, d->inner, shd, ctm, alpha, color_params);
        return;
    }

    fz_irect ibounds = fz_irect_from_rect(fz_bound_shade(ctx, shd, ctm));
    if (fz_is_empty_irect(ibounds)) {
        return;
    }

    int w = ibounds.x1 - ibounds.x0;
    int h = ibounds.y1 - ibounds.y0;
    static constexpr int kMaxShadeDim = 2048;
    static constexpr i64 kMaxShadePixels = 2048 * 2048;
    if (w <= 0 || h <= 0 || w > kMaxShadeDim || h > kMaxShadeDim || (i64)w * h > kMaxShadePixels) {
        PdfDarkModeRecordShadeForward();
        fz_fill_shade(ctx, d->inner, shd, ctm, alpha, color_params);
        return;
    }

    fz_image* cached = PdfDarkModeGetCachedShade(ctx, d->analysis, shd, ctm, alpha, ibounds, *d->palette);
    if (!cached) {
        PdfDarkModeRecordShadeForward();
        fz_fill_shade(ctx, d->inner, shd, ctm, alpha, color_params);
        return;
    }

    fz_try(ctx) {
        fz_matrix image_ctm = fz_make_matrix((float)w, 0.f, 0.f, (float)h, (float)ibounds.x0, (float)ibounds.y0);
        // Alpha was applied when rasterizing the shade into the cached pixmap.
        fz_fill_image(ctx, d->inner, cached, image_ctm, 1.f, color_params);
    }
    fz_always(ctx) {
        fz_drop_image(ctx, cached);
    }
    fz_catch(ctx) {
        PdfDarkModeRecordShadeForward();
        fz_fill_shade(ctx, d->inner, shd, ctm, alpha, color_params);
    }
}

static void dm_stroke_debug_image_box(fz_context* ctx, fz_device* dev, fz_matrix ctm) {
    fz_rect r = fz_transform_rect(fz_unit_rect, ctm);
    fz_path* path = fz_new_path(ctx);
    fz_moveto(ctx, path, r.x0, r.y0);
    fz_lineto(ctx, path, r.x1, r.y0);
    fz_lineto(ctx, path, r.x1, r.y1);
    fz_lineto(ctx, path, r.x0, r.y1);
    fz_closepath(ctx, path);
    float yellow[3] = {1.f, 0.92f, 0.1f};
    fz_stroke_state* stroke = fz_new_stroke_state(ctx);
    fz_stroke_path(ctx, dev, path, stroke, fz_identity, fz_device_rgb(ctx), yellow, 1.f, fz_default_color_params);
    fz_drop_stroke_state(ctx, stroke);
    fz_drop_path(ctx, path);
}

static void dm_fill_image(fz_context* ctx, fz_device* dev, fz_image* image, fz_matrix ctm, float alpha,
                          fz_color_params color_params) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    int idx = d->replayState ? d->replayState->nextImageOccurrence : -1;
    DarkImagePolicy policy = dm_current_image_policy(d);
    dm_next_image_occurrence(d);

    fz_image* cached =
        PdfDarkModeGetCachedImage(ctx, d->engineCache, d->analysis, idx, image, policy, *d->palette, d->profileHash);
    fz_image* draw = cached ? cached : image;
    fz_try(ctx) {
        fz_fill_image(ctx, d->inner, draw, ctm, alpha, color_params);
        if (d->debugOverlay && d->analysis && idx >= 0 && idx < len(d->analysis->images)) {
            dm_stroke_debug_image_box(ctx, d->inner, ctm);
        }
    }
    fz_always(ctx) {
        if (cached) {
            fz_drop_image(ctx, cached);
        }
    }
    fz_catch(ctx) {
        if (cached) {
            fz_fill_image(ctx, d->inner, image, ctm, alpha, color_params);
        } else {
            fz_rethrow(ctx);
        }
    }
}

static void dm_fill_image_mask(fz_context* ctx, fz_device* dev, fz_image* image, fz_matrix ctm,
                               fz_colorspace* colorspace, const float* color, float alpha,
                               fz_color_params color_params) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    DarkImagePolicy policy = dm_current_image_policy(d);
    dm_next_image_occurrence(d);

    float mapped[FZ_MAX_COLORS] = {};
    const float* useColor = color;
    if (policy == DarkImagePolicy::ThemeRecolor) {
        dm_map_color(d, ctx, colorspace, color, color_params, mapped);
        useColor = mapped;
        colorspace = fz_device_rgb(ctx);
    }
    fz_fill_image_mask(ctx, d->inner, image, ctm, colorspace, useColor, alpha, color_params);
}

static void dm_clip_path(fz_context* ctx, fz_device* dev, const fz_path* path, int even_odd, fz_matrix ctm,
                         fz_rect scissor) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_clip_path(ctx, d->inner, path, even_odd, ctm, scissor);
}

static void dm_clip_stroke_path(fz_context* ctx, fz_device* dev, const fz_path* path, const fz_stroke_state* stroke,
                                fz_matrix ctm, fz_rect scissor) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_clip_stroke_path(ctx, d->inner, path, stroke, ctm, scissor);
}

static void dm_clip_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm, fz_rect scissor) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_clip_text(ctx, d->inner, text, ctm, scissor);
}

static void dm_clip_stroke_text(fz_context* ctx, fz_device* dev, const fz_text* text, const fz_stroke_state* stroke,
                                fz_matrix ctm, fz_rect scissor) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_clip_stroke_text(ctx, d->inner, text, stroke, ctm, scissor);
}

static void dm_clip_image_mask(fz_context* ctx, fz_device* dev, fz_image* image, fz_matrix ctm, fz_rect scissor) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_clip_image_mask(ctx, d->inner, image, ctm, scissor);
}

static void dm_pop_clip(fz_context* ctx, fz_device* dev) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_pop_clip(ctx, d->inner);
}

static void dm_begin_mask(fz_context* ctx, fz_device* dev, fz_rect area, int luminosity, fz_colorspace* colorspace,
                          const float* bc, fz_color_params color_params) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    float mapped[FZ_MAX_COLORS] = {};
    dm_map_color(d, ctx, colorspace, bc, color_params, mapped);
    fz_begin_mask(ctx, d->inner, area, luminosity, fz_device_rgb(ctx), mapped, color_params);
}

static void dm_end_mask(fz_context* ctx, fz_device* dev, fz_function* fn) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_end_mask_tr(ctx, d->inner, fn);
}

static void dm_begin_group(fz_context* ctx, fz_device* dev, fz_rect area, fz_colorspace* cs, int isolated, int knockout,
                           int blendmode, float alpha) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_begin_group(ctx, d->inner, area, cs, isolated, knockout, blendmode, alpha);
}

static void dm_end_group(fz_context* ctx, fz_device* dev) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_end_group(ctx, d->inner);
}

static int dm_begin_tile(fz_context* ctx, fz_device* dev, fz_rect area, fz_rect view, float xstep, float ystep,
                         fz_matrix ctm, int id, int doc_id) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    return fz_begin_tile_tid(ctx, d->inner, area, view, xstep, ystep, ctm, id, doc_id);
}

static void dm_end_tile(fz_context* ctx, fz_device* dev) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_end_tile(ctx, d->inner);
}

static void dm_render_flags(fz_context* ctx, fz_device* dev, int set, int clear) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_render_flags(ctx, d->inner, set, clear);
}

static void dm_set_default_colorspaces(fz_context* ctx, fz_device* dev, fz_default_colorspaces* default_cs) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_set_default_colorspaces(ctx, d->inner, default_cs);
}

static void dm_begin_layer(fz_context* ctx, fz_device* dev, const char* layer_name) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_begin_layer(ctx, d->inner, layer_name);
}

static void dm_end_layer(fz_context* ctx, fz_device* dev) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_end_layer(ctx, d->inner);
}

static void dm_begin_structure(fz_context* ctx, fz_device* dev, fz_structure standard, const char* raw, int idx) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_begin_structure(ctx, d->inner, standard, raw, idx);
}

static void dm_end_structure(fz_context* ctx, fz_device* dev) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_end_structure(ctx, d->inner);
}

static void dm_begin_metatext(fz_context* ctx, fz_device* dev, fz_metatext meta, const char* text) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_begin_metatext(ctx, d->inner, meta, text);
}

static void dm_end_metatext(fz_context* ctx, fz_device* dev) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_end_metatext(ctx, d->inner);
}

static void dm_ignore_text(fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm) {
    pdf_dark_mode_device* d = (pdf_dark_mode_device*)dev;
    fz_ignore_text(ctx, d->inner, text, ctm);
}

fz_device* PdfDarkModeWrapDevice(fz_context* ctx, fz_device* inner, DarkModePageAnalysis* analysis,
                                 const DarkModePalette* palette, DarkModeReplayState* replayState,
                                 DarkModeEngineCache* engineCache, u32 profileHash, bool debugOverlay) {
    pdf_dark_mode_device* d = fz_new_derived_device(ctx, pdf_dark_mode_device);
    d->inner = inner;
    d->analysis = analysis;
    d->palette = palette;
    d->replayState = replayState;
    d->engineCache = engineCache;
    d->profileHash = profileHash;
    d->debugOverlay = debugOverlay;
    PdfDarkModeTakeShadeForwardCount();

    d->super.close_device = dm_forward_close;
    d->super.drop_device = dm_forward_drop;
    d->super.fill_path = dm_fill_path;
    d->super.stroke_path = dm_stroke_path;
    d->super.fill_text = dm_fill_text;
    d->super.stroke_text = dm_stroke_text;
    d->super.fill_shade = dm_fill_shade;
    d->super.fill_image = dm_fill_image;
    d->super.fill_image_mask = dm_fill_image_mask;
    d->super.clip_path = dm_clip_path;
    d->super.clip_stroke_path = dm_clip_stroke_path;
    d->super.clip_text = dm_clip_text;
    d->super.clip_stroke_text = dm_clip_stroke_text;
    d->super.clip_image_mask = dm_clip_image_mask;
    d->super.pop_clip = dm_pop_clip;
    d->super.begin_mask = dm_begin_mask;
    d->super.end_mask = dm_end_mask;
    d->super.begin_group = dm_begin_group;
    d->super.end_group = dm_end_group;
    d->super.begin_tile = dm_begin_tile;
    d->super.end_tile = dm_end_tile;
    d->super.render_flags = dm_render_flags;
    d->super.set_default_colorspaces = dm_set_default_colorspaces;
    d->super.begin_layer = dm_begin_layer;
    d->super.end_layer = dm_end_layer;
    d->super.begin_structure = dm_begin_structure;
    d->super.end_structure = dm_end_structure;
    d->super.begin_metatext = dm_begin_metatext;
    d->super.end_metatext = dm_end_metatext;
    d->super.ignore_text = dm_ignore_text;

    return &d->super;
}
