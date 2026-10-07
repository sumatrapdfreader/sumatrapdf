/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "PdfDarkMode.h"

struct ImageStats {
    int significantBuckets = 0;
    float lumVar = 0.f;
    float satRatio = 0.f;
    float highLumRatio = 0.f;
    float borderLightRatio = 0.f;
    float borderUniformity = 0.f;
};

struct PixelColor {
    float r, g, b;
};

// Border has to be one light color all the way round, e.g. the flat backdrop a
// 3D render or a chart is drawn on.
static constexpr float kLightBackdropBorderLight = 0.95f;
static constexpr float kLightBackdropBorderUniformity = 0.90f;
static constexpr int kBorderSamplesPerEdge = 32;

static PixelColor SampleImageRgb(fz_context* ctx, fz_pixmap* pix, int x, int y) {
    if (!pix || !pix->samples || x < 0 || y < 0 || x >= pix->w || y >= pix->h) {
        return {};
    }
    fz_colorspace* cs = pix->colorspace ? pix->colorspace : fz_device_rgb(ctx);
    fz_colorspace* rgb = fz_device_rgb(ctx);
    int n = pix->n;
    int stride = (int)pix->stride;
    unsigned char* px = pix->samples + ((size_t)y * stride) + ((size_t)x * n);
    float conv[FZ_MAX_COLORS] = {};
    float srcRgb[FZ_MAX_COLORS] = {};
    int components = fz_colorspace_n(ctx, cs);
    for (int c = 0; c < components && c < FZ_MAX_COLORS; c++) {
        conv[c] = (float)px[c] / 255.f;
    }
    fz_convert_color(ctx, cs, conv, rgb, srcRgb, cs, fz_default_color_params);
    return {srcRgb[0], srcRgb[1], srcRgb[2]};
}

// How light the outermost ring of pixels is, and how close it is to a single
// color. borderUniformity is 1 for a perfectly flat border and drops to 0 as
// the mean squared RGB distance from the border's average color reaches 0.12.
static void SampleImageBorder(fz_context* ctx, fz_pixmap* pix, ImageStats* stats) {
    PixelColor samples[kBorderSamplesPerEdge * 4] = {};
    int n = 0;
    int light = 0;

    auto sampleAt = [&](int x, int y) {
        if (n >= dimofi(samples)) {
            return;
        }
        PixelColor& c = samples[n];
        c = SampleImageRgb(ctx, pix, x, y);
        float lum = (0.2126f * c.r) + (0.7152f * c.g) + (0.0722f * c.b);
        if (lum > 0.72f) {
            light++;
        }
        n++;
    };

    int stepX = pix->w >= kBorderSamplesPerEdge ? pix->w / kBorderSamplesPerEdge : 1;
    for (int x = 0; x < pix->w; x += stepX) {
        sampleAt(x, 0);
        sampleAt(x, pix->h - 1);
    }
    int stepY = pix->h >= kBorderSamplesPerEdge ? pix->h / kBorderSamplesPerEdge : 1;
    for (int y = 0; y < pix->h; y += stepY) {
        sampleAt(0, y);
        sampleAt(pix->w - 1, y);
    }

    float mr = 0.f, mg = 0.f, mb = 0.f;
    for (int i = 0; i < n; i++) {
        mr += samples[i].r;
        mg += samples[i].g;
        mb += samples[i].b;
    }
    mr /= (float)n;
    mg /= (float)n;
    mb /= (float)n;

    float var = 0.f;
    for (int i = 0; i < n; i++) {
        float dr = samples[i].r - mr;
        float dg = samples[i].g - mg;
        float db = samples[i].b - mb;
        var += (dr * dr) + (dg * dg) + (db * db);
    }
    var /= (float)n;

    stats->borderLightRatio = (float)light / (float)n;
    stats->borderUniformity = limitValue(1.f - (var / 0.12f), 0.f, 1.f);
}

// Zeroed stats reject failed images in every classifier.
static ImageStats SampleImageStats(fz_context* ctx, fz_image* image) {
    ImageStats stats;
    if (!ctx || !image) {
        return stats;
    }

    fz_pixmap* pix = nullptr;
    fz_var(pix);
    fz_try(ctx) {
        int targetW = image->w > 0 ? image->w : 1;
        int targetH = image->h > 0 ? image->h : 1;
        const int maxDim = 64;
        if (targetW > maxDim || targetH > maxDim) {
            float scale = (float)maxDim / (float)(targetW > targetH ? targetW : targetH);
            fz_matrix ctm = fz_scale(scale, scale);
            pix = fz_get_pixmap_from_image(ctx, image, nullptr, &ctm, nullptr, nullptr);
        } else {
            pix = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
        }
        if (!pix || !pix->samples || pix->w <= 0 || pix->h <= 0) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "empty image pixmap");
        }

        int buckets[4096] = {};
        int n = 0;
        int saturated = 0;
        int highLum = 0;
        float lumSum = 0.f;
        float lumSqSum = 0.f;

        int stepX = pix->w >= 32 ? pix->w / 32 : 1;
        int stepY = pix->h >= 32 ? pix->h / 32 : 1;
        for (int y = 0; y < pix->h; y += stepY) {
            for (int x = 0; x < pix->w; x += stepX) {
                PixelColor c = SampleImageRgb(ctx, pix, x, y);
                int ri = (int)lroundf(c.r * 255.f);
                int gi = (int)lroundf(c.g * 255.f);
                int bi = (int)lroundf(c.b * 255.f);
                int bucket = ((ri >> 4) << 8) | ((gi >> 4) << 4) | (bi >> 4);
                buckets[bucket]++;

                float maxC = std::max({c.r, c.g, c.b});
                float minC = std::min({c.r, c.g, c.b});
                float lum = (0.2126f * c.r) + (0.7152f * c.g) + (0.0722f * c.b);
                lumSum += lum;
                lumSqSum += lum * lum;
                if (maxC - minC > 0.12f) {
                    saturated++;
                }
                if (lum > 0.72f) {
                    highLum++;
                }
                n++;
            }
        }

        int significantBuckets = 0;
        for (int bucket : buckets) {
            if (bucket * 100 > n) {
                significantBuckets++;
            }
        }

        float lumMean = lumSum / (float)n;
        stats.significantBuckets = significantBuckets;
        stats.lumVar = (lumSqSum / (float)n) - (lumMean * lumMean);
        stats.satRatio = (float)saturated / (float)n;
        stats.highLumRatio = (float)highLum / (float)n;
        SampleImageBorder(ctx, pix, &stats);
    }
    fz_always(ctx) {
        if (pix) {
            fz_drop_pixmap(ctx, pix);
        }
    }
    fz_catch(ctx) {
        stats = ImageStats{};
    }
    return stats;
}

static bool LooksLikePhoto(const ImageStats& stats) {
    if (stats.highLumRatio > 0.58f && stats.satRatio < 0.18f) {
        return false;
    }
    if (stats.significantBuckets <= 12 && stats.lumVar < 0.012f && stats.highLumRatio > 0.45f) {
        return false;
    }
    return stats.significantBuckets >= 16 || stats.satRatio >= 0.18f || stats.lumVar >= 0.014f;
}

static bool LooksLikeLayoutBackground(const ImageStats& stats) {
    // Cream/tan/yellow textbook panels and title cards - recolor for uniform dark page.
    return (stats.highLumRatio > 0.44f && stats.lumVar < 0.022f) ||
           (stats.highLumRatio > 0.50f && stats.lumVar < 0.038f && stats.satRatio < 0.22f &&
            stats.significantBuckets <= 14);
}

RectF PdfDarkModeClampImagePageRect(const RectF& imgPage, int imageW, int imageH) {
    if (imageW <= 0 || imageH <= 0 || imgPage.IsEmpty()) {
        return imgPage;
    }
    float imageAspect = (float)imageW / (float)imageH;
    float bboxAspect = imgPage.dx / imgPage.dy;
    if (bboxAspect <= 0.f) {
        return imgPage;
    }
    const float maxSkew = 1.40f;
    float newDx = imgPage.dx;
    float newDy = imgPage.dy;
    // Bbox taller than bitmap → trim height (painting drawn in top of tall column).
    if (bboxAspect < imageAspect / maxSkew) {
        newDy = imgPage.dx / imageAspect;
        newDy = std::min(newDy, imgPage.dy);
    }
    // Bbox wider than bitmap → trim width (avoids preserving a whole page column).
    if (bboxAspect > imageAspect * maxSkew) {
        float clampedDx = imgPage.dy * imageAspect;
        newDx = std::min(clampedDx, newDx);
    }
    if (newDx == imgPage.dx && newDy == imgPage.dy) {
        return imgPage;
    }
    return {imgPage.x, imgPage.y, newDx, newDy};
}

// Cap bbox when embedded image dimensions are unknown (common with content-stream tiles).
RectF PdfDarkModeCapUnknownImagePageRect(const RectF& imgPage, float pageHeight) {
    if (imgPage.IsEmpty() || pageHeight <= 0.f) {
        return imgPage;
    }
    float maxH = pageHeight * 0.48f;
    if (imgPage.dy <= maxH) {
        return imgPage;
    }
    return {imgPage.x, imgPage.y, imgPage.dx, maxH};
}

static bool LooksLikeDarkArtwork(const ImageStats& stats, float pageCoverage) {
    if (pageCoverage < 0.035f) {
        return false;
    }
    return stats.highLumRatio < 0.48f && stats.lumVar >= 0.004f &&
           (stats.significantBuckets >= 8 || stats.satRatio >= 0.08f);
}

static bool LooksLikeLightBackdrop(const ImageStats& stats) {
    return stats.borderLightRatio >= kLightBackdropBorderLight &&
           stats.borderUniformity >= kLightBackdropBorderUniformity;
}

static bool LooksLikePaperTextBox(const ImageStats& stats) {
    return stats.highLumRatio > 0.64f && stats.lumVar < 0.014f && stats.significantBuckets <= 12 &&
           stats.satRatio < 0.20f;
}

bool PdfDarkModeImageLooksLikeDarkArtwork(fz_context* ctx, fz_image* image, float pageCoverage) {
    return LooksLikeDarkArtwork(SampleImageStats(ctx, image), pageCoverage);
}

static bool ImageIsArtwork(fz_context* ctx, fz_image* image, float pageCoverage) {
    ImageStats stats = SampleImageStats(ctx, image);
    if (LooksLikeLayoutBackground(stats)) {
        return false;
    }
    // artwork on a flat light backdrop: recolor so the backdrop follows the page
    // instead of staying a bright block on it (#6088)
    if (LooksLikeLightBackdrop(stats)) {
        return false;
    }
    if (LooksLikeDarkArtwork(stats, pageCoverage)) {
        return true;
    }
    if (LooksLikePhoto(stats)) {
        if (pageCoverage < 0.14f && LooksLikePaperTextBox(stats)) {
            return false;
        }
        return true;
    }
    return false;
}

// A page-sized image is normally a scan or a full-bleed background, and those
// should recolor along with the page. Artwork shouldn't: keeping pictures as
// they are is what smart mode is for, and a cover illustration is no less a
// picture for filling the page (issue #5887). LooksLikeDarkArtwork is the
// discriminator - a scanned page is mostly bright paper, which it rejects.
bool PdfDarkModePageDominantImageRecolors(fz_context* ctx, fz_image* image, float pageCoverage) {
    if (pageCoverage < kMaxPreserveImagePageCoverage) {
        return false; // not page-dominant, the ordinary rules decide
    }
    return !PdfDarkModeImageLooksLikeDarkArtwork(ctx, image, pageCoverage);
}

// Gate for Legacy skip-rect preserve: combines bbox size, pixel stats, and artwork heuristics.
bool PdfDarkModeShouldPreserveEmbeddedImageRect(fz_context* ctx, fz_image* image, float pageCoverage, int devW,
                                                int devH) {
    if (PdfDarkModePageDominantImageRecolors(ctx, image, pageCoverage)) {
        return false;
    }
    int minPx = kPreservePdfImagesMinSize;
    if (devW < minPx || devH < minPx) {
        return false;
    }
    return ImageIsArtwork(ctx, image, pageCoverage);
}

#if IS_DEBUG
bool PdfDarkModeImageStats_UnitTest() {
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    fz_pixmap* pix = fz_new_pixmap(ctx, fz_device_rgb(ctx), 40, 40, nullptr, 1);
    fz_clear_pixmap_with_value(ctx, pix, 255);
    memset(pix->samples + 39 * pix->stride, 0, 40 * pix->n);

    ImageStats stats;
    SampleImageBorder(ctx, pix, &stats);
    bool ok = stats.borderLightRatio == 88.f / 128.f && stats.borderUniformity == 0.f;

    fz_clear_pixmap_with_value(ctx, pix, 255);
    SampleImageBorder(ctx, pix, &stats);
    ok = ok && stats.borderLightRatio == 1.f && stats.borderUniformity == 1.f;

    fz_clear_pixmap(ctx, pix);
    SampleImageBorder(ctx, pix, &stats);
    ok = ok && stats.borderLightRatio == 0.f && stats.borderUniformity == 1.f;

    fz_drop_pixmap(ctx, pix);
    fz_drop_context(ctx);
    return ok;
}
#endif
