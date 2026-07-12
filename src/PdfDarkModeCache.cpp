/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "PdfDarkMode.h"
#include "PdfDarkModeInternal.h"

struct DarkModeShadeCacheEntry {
    fz_shade* shade = nullptr;
    fz_matrix ctm{};
    float alpha = 0.f;
    fz_irect bounds{};
    fz_image* processedImage = nullptr;
};

struct DarkModeProcessCache {
    Vec<fz_image*> processedImages;
    Vec<DarkModeShadeCacheEntry> shadeCache;
};

static const int kMaxShadeCacheEntries = 32;

static DarkModeProcessCache* PdfDarkModeEnsureProcessCache(DarkModePageAnalysis* analysis) {
    if (!analysis) {
        return nullptr;
    }
    auto* cache = (DarkModeProcessCache*)analysis->processCache;
    if (!cache) {
        cache = new DarkModeProcessCache();
        analysis->processCache = cache;
    }
    int n = len(analysis->images);
    if (len(cache->processedImages) != n) {
        cache->processedImages.SetSize(n);
        for (int i = 0; i < n; i++) {
            cache->processedImages[i] = nullptr;
        }
    }
    return cache;
}

void PdfDarkModeFreeProcessCache(fz_context* ctx, DarkModePageAnalysis* analysis) {
    if (!analysis || !analysis->processCache) {
        return;
    }
    auto* cache = (DarkModeProcessCache*)analysis->processCache;
    if (ctx) {
        for (fz_image* img : cache->processedImages) {
            if (img) {
                fz_drop_image(ctx, img);
            }
        }
        for (DarkModeShadeCacheEntry& entry : cache->shadeCache) {
            if (entry.processedImage) {
                fz_drop_image(ctx, entry.processedImage);
            }
        }
    }
    delete cache;
    analysis->processCache = nullptr;
}

typedef void (*dm_rgb_map_fn)(float r, float g, float b, const DarkModePalette& palette, float* outR, float* outG,
                              float* outB);

static void dm_transform_pixmap_rgb(fz_context* ctx, fz_pixmap* pix, const DarkModePalette& palette, dm_rgb_map_fn fn) {
    if (!pix || !pix->samples) {
        return;
    }
    fz_colorspace* cs = pix->colorspace ? pix->colorspace : fz_device_rgb(ctx);
    fz_colorspace* rgb = fz_device_rgb(ctx);
    int components = fz_colorspace_n(ctx, cs);
    int n = pix->n;
    int stride = pix->stride;
    int w = pix->w;
    int h = pix->h;
    for (int y = 0; y < h; y++) {
        unsigned char* row = pix->samples + y * stride;
        for (int x = 0; x < w; x++) {
            unsigned char* px = row + x * n;
            float conv[FZ_MAX_COLORS] = {};
            float srcRgb[FZ_MAX_COLORS] = {};
            for (int c = 0; c < components && c < FZ_MAX_COLORS; c++) {
                conv[c] = px[c] / 255.f;
            }
            fz_convert_color(ctx, cs, conv, rgb, srcRgb, cs, fz_default_color_params);
            float nr = 0.f, ng = 0.f, nb = 0.f;
            fn(srcRgb[0], srcRgb[1], srcRgb[2], palette, &nr, &ng, &nb);
            float out[FZ_MAX_COLORS] = {nr, ng, nb};
            float back[FZ_MAX_COLORS] = {};
            fz_convert_color(ctx, rgb, out, cs, back, cs, fz_default_color_params);
            for (int c = 0; c < components && c < FZ_MAX_COLORS; c++) {
                int v = (int)(back[c] * 255.f + 0.5f);
                if (v < 0) {
                    v = 0;
                }
                if (v > 255) {
                    v = 255;
                }
                px[c] = (unsigned char)v;
            }
        }
    }
}

static void dm_preserve_pixel(float r, float g, float b, const DarkModePalette& palette, float* outR, float* outG,
                              float* outB) {
    float cr = r, cg = g, cb = b;
    PdfDarkModeCompressPhotoHighlights(r, g, b, &cr, &cg, &cb);
    float softening = PdfDarkModeCurrentOptions().preserveImagePaperSoftening;
    if (softening > 0.f) {
        ApplyPreserveImagePaperSoftening(cr, cg, cb, palette, softening, outR, outG, outB);
    } else {
        *outR = cr;
        *outG = cg;
        *outB = cb;
    }
}

static void dm_theme_recolor_pixel(float r, float g, float b, const DarkModePalette& palette, float* outR, float* outG,
                                   float* outB) {
    float mapped[3] = {};
    MapRgbToDarkTheme(r, g, b, palette, mapped);
    *outR = mapped[0];
    *outG = mapped[1];
    *outB = mapped[2];
}

static void dm_adaptive_pixel(float r, float g, float b, const DarkModePalette& palette, float* outR, float* outG,
                              float* outB) {
    ApplyAdaptiveDocumentDarkMode(r, g, b, palette, outR, outG, outB);
}

static void dm_shade_pixel(float r, float g, float b, const DarkModePalette& palette, float* outR, float* outG,
                           float* outB) {
    float mapped[FZ_MAX_COLORS] = {};
    MapRgbFillToDarkTheme(r, g, b, palette, mapped);
    *outR = mapped[0];
    *outG = mapped[1];
    *outB = mapped[2];
}

static fz_pixmap* dm_copy_and_transform_pixmap(fz_context* ctx, fz_pixmap* src, const DarkModePalette& palette,
                                               dm_rgb_map_fn fn) {
    if (!src || !src->samples) {
        return src;
    }
    fz_colorspace* cs = src->colorspace ? src->colorspace : fz_device_rgb(ctx);
    int w = src->w;
    int h = src->h;
    fz_pixmap* dst = fz_new_pixmap(ctx, cs, w, h, src->seps, src->alpha);
    fz_copy_pixmap_rect(ctx, dst, src, fz_make_irect(0, 0, w, h), nullptr);
    dm_transform_pixmap_rgb(ctx, dst, palette, fn);
    return dst;
}

static bool dm_matrix_near_equal(fz_matrix a, fz_matrix b) {
    const float eps = 1e-4f;
    return fabsf(a.a - b.a) < eps && fabsf(a.b - b.b) < eps && fabsf(a.c - b.c) < eps && fabsf(a.d - b.d) < eps &&
           fabsf(a.e - b.e) < eps && fabsf(a.f - b.f) < eps;
}

static bool dm_irect_equal(fz_irect a, fz_irect b) {
    return a.x0 == b.x0 && a.y0 == b.y0 && a.x1 == b.x1 && a.y1 == b.y1;
}

static fz_image* dm_build_processed_image(fz_context* ctx, fz_image* srcImage, DarkImagePolicy policy,
                                          const DarkImageAnalysis* imgAnalysis, const DarkModePalette& palette) {
    fz_pixmap* src = nullptr;
    fz_pixmap* processed = nullptr;
    fz_image* result = nullptr;
    fz_var(src);
    fz_var(processed);
    fz_var(result);
    fz_try(ctx) {
        src = fz_get_pixmap_from_image(ctx, srcImage, nullptr, nullptr, nullptr, nullptr);
        if (policy == DarkImagePolicy::Preserve) {
            processed = dm_copy_and_transform_pixmap(ctx, src, palette, dm_preserve_pixel);
        } else if (policy == DarkImagePolicy::AdaptiveDocument) {
            if (imgAnalysis && imgAnalysis->kind == DarkImageKind::FullPageScan) {
                processed = PdfDarkModeProcessScanPixmap(ctx, src, *imgAnalysis, palette);
            }
            if (!processed && imgAnalysis && PdfDarkModeShouldBlendLightBackground(*imgAnalysis)) {
                processed = PdfDarkModeProcessLightBackgroundPixmap(ctx, src, *imgAnalysis, palette);
            }
            if (!processed) {
                processed = dm_copy_and_transform_pixmap(ctx, src, palette, dm_adaptive_pixel);
            }
        } else {
            processed = dm_copy_and_transform_pixmap(ctx, src, palette, dm_theme_recolor_pixel);
        }
        if (processed == src) {
            fz_drop_pixmap(ctx, src);
            src = nullptr;
            return nullptr;
        }
        fz_drop_pixmap(ctx, src);
        src = nullptr;
        result = fz_new_image_from_pixmap(ctx, processed, nullptr);
        fz_drop_pixmap(ctx, processed);
        processed = nullptr;
    }
    fz_always(ctx) {
        if (src) {
            fz_drop_pixmap(ctx, src);
        }
        if (processed) {
            fz_drop_pixmap(ctx, processed);
        }
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return result;
}

static fz_image* dm_build_processed_shade(fz_context* ctx, fz_shade* shade, fz_matrix ctm, float alpha, fz_irect bounds,
                                          const DarkModePalette& palette) {
    int w = bounds.x1 - bounds.x0;
    int h = bounds.y1 - bounds.y0;
    if (w <= 0 || h <= 0) {
        return nullptr;
    }

    fz_pixmap* pix = nullptr;
    fz_device* shadeDev = nullptr;
    fz_image* result = nullptr;
    fz_var(pix);
    fz_var(shadeDev);
    fz_var(result);
    fz_try(ctx) {
        pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bounds, nullptr, 1);
        fz_clear_pixmap_with_value(ctx, pix, 0xff);
        fz_matrix local_ctm = fz_concat(fz_translate(-bounds.x0, -bounds.y0), ctm);
        shadeDev = fz_new_draw_device(ctx, local_ctm, pix);
        fz_fill_shade(ctx, shadeDev, shade, fz_identity, alpha, fz_default_color_params);
        fz_close_device(ctx, shadeDev);
        fz_drop_device(ctx, shadeDev);
        shadeDev = nullptr;
        dm_transform_pixmap_rgb(ctx, pix, palette, dm_shade_pixel);
        result = fz_new_image_from_pixmap(ctx, pix, nullptr);
        fz_drop_pixmap(ctx, pix);
        pix = nullptr;
    }
    fz_always(ctx) {
        if (shadeDev) {
            fz_drop_device(ctx, shadeDev);
        }
        if (pix) {
            fz_drop_pixmap(ctx, pix);
        }
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return result;
}

fz_image* PdfDarkModeGetCachedImage(fz_context* ctx, DarkModeEngineCache* engineCache, DarkModePageAnalysis* analysis,
                                    int occurrenceIndex, fz_image* srcImage, DarkImagePolicy policy,
                                    const DarkModePalette& palette, u32 profileHash) {
    if (!analysis || !srcImage) {
        return nullptr;
    }
    if (policy != DarkImagePolicy::Preserve && policy != DarkImagePolicy::AdaptiveDocument) {
        return nullptr;
    }
    if (occurrenceIndex < 0 || occurrenceIndex >= len(analysis->images)) {
        return nullptr;
    }

    const DarkImageAnalysis* imgAnalysis = &analysis->images[occurrenceIndex].analysis;
    DarkImageKind kind = imgAnalysis->kind;

    if (engineCache) {
        fz_image* engineHit =
            PdfDarkModeEngineCacheLookupProcessed(ctx, engineCache, srcImage, profileHash, policy, kind);
        if (engineHit) {
            return engineHit;
        }
    }

    DarkModeProcessCache* cache = PdfDarkModeEnsureProcessCache(analysis);
    if (!cache) {
        return nullptr;
    }

    fz_image* cached = cache->processedImages[occurrenceIndex];
    if (cached) {
        return fz_keep_image(ctx, cached);
    }

    fz_image* built = dm_build_processed_image(ctx, srcImage, policy, imgAnalysis, palette);
    if (!built) {
        return nullptr;
    }
    cache->processedImages[occurrenceIndex] = built;
    if (engineCache) {
        PdfDarkModeEngineCacheStoreProcessed(ctx, engineCache, srcImage, profileHash, policy, kind, built);
    }
    return fz_keep_image(ctx, built);
}

fz_image* PdfDarkModeGetCachedShade(fz_context* ctx, DarkModePageAnalysis* analysis, fz_shade* shade, fz_matrix ctm,
                                    float alpha, fz_irect bounds, const DarkModePalette& palette) {
    if (!analysis || !shade) {
        return nullptr;
    }

    DarkModeProcessCache* cache = PdfDarkModeEnsureProcessCache(analysis);
    if (!cache) {
        return nullptr;
    }

    for (DarkModeShadeCacheEntry& entry : cache->shadeCache) {
        if (entry.shade == shade && entry.alpha == alpha && dm_irect_equal(entry.bounds, bounds) &&
            dm_matrix_near_equal(entry.ctm, ctm) && entry.processedImage) {
            return fz_keep_image(ctx, entry.processedImage);
        }
    }

    if (len(cache->shadeCache) >= kMaxShadeCacheEntries) {
        return nullptr;
    }

    fz_image* built = dm_build_processed_shade(ctx, shade, ctm, alpha, bounds, palette);
    if (!built) {
        return nullptr;
    }

    DarkModeShadeCacheEntry entry;
    entry.shade = shade;
    entry.ctm = ctm;
    entry.alpha = alpha;
    entry.bounds = bounds;
    entry.processedImage = built;
    cache->shadeCache.Append(entry);
    return fz_keep_image(ctx, built);
}
