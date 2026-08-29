/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "PdfDarkMode.h"

struct PdfDarkModeImageSampleStats {
    int significantBuckets = 0;
    float lumVar = 0.f;
    float satRatio = 0.f;
    float highLumRatio = 0.f;
    float borderLightRatio = 0.f;
    float borderUniformity = 0.f;
    bool valid = false;
};

// Border has to be one light color all the way round, e.g. the flat backdrop a
// 3D render or a chart is drawn on.
static constexpr float kLightBackdropBorderLight = 0.95f;
static constexpr float kLightBackdropBorderUniformity = 0.90f;
static constexpr int kBorderSamplesPerEdge = 32;

static void SamplePixmapRgb(fz_context* ctx, fz_pixmap* pix, int x, int y, float* outR, float* outG, float* outB) {
    if (!pix || !pix->samples || x < 0 || y < 0 || x >= pix->w || y >= pix->h) {
        *outR = *outG = *outB = 0.f;
        return;
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
    *outR = srcRgb[0];
    *outG = srcRgb[1];
    *outB = srcRgb[2];
}

// How light the outermost ring of pixels is, and how close it is to a single
// color. borderUniformity is 1 for a perfectly flat border and drops to 0 as
// the mean squared RGB distance from the border's average color reaches 0.12.
static void SampleBorderStats(fz_context* ctx, fz_pixmap* pix, PdfDarkModeImageSampleStats* stats) {
    float r[kBorderSamplesPerEdge * 4] = {};
    float g[kBorderSamplesPerEdge * 4] = {};
    float b[kBorderSamplesPerEdge * 4] = {};
    int n = 0;
    int light = 0;

    auto sampleAt = [&](int x, int y) {
        if (n >= kBorderSamplesPerEdge * 4) {
            return;
        }
        SamplePixmapRgb(ctx, pix, x, y, &r[n], &g[n], &b[n]);
        float lum = (0.2126f * r[n]) + (0.7152f * g[n]) + (0.0722f * b[n]);
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
    if (n <= 0) {
        return;
    }

    float mr = 0.f, mg = 0.f, mb = 0.f;
    for (int i = 0; i < n; i++) {
        mr += r[i];
        mg += g[i];
        mb += b[i];
    }
    mr /= (float)n;
    mg /= (float)n;
    mb /= (float)n;

    float var = 0.f;
    for (int i = 0; i < n; i++) {
        float dr = r[i] - mr;
        float dg = g[i] - mg;
        float db = b[i] - mb;
        var += (dr * dr) + (dg * dg) + (db * db);
    }
    var /= (float)n;

    stats->borderLightRatio = (float)light / (float)n;
    stats->borderUniformity = limitValue(1.f - (var / 0.12f), 0.f, 1.f);
}

static PdfDarkModeImageSampleStats PdfDarkModeSampleImageStats(fz_context* ctx, fz_image* image) {
    PdfDarkModeImageSampleStats stats;
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
                float r, g, b;
                SamplePixmapRgb(ctx, pix, x, y, &r, &g, &b);
                int ri = (int)lroundf(r * 255.f);
                int gi = (int)lroundf(g * 255.f);
                int bi = (int)lroundf(b * 255.f);
                int bucket = ((ri >> 4) << 8) | ((gi >> 4) << 4) | (bi >> 4);
                buckets[bucket]++;

                float maxC = std::max({r, g, b});
                float minC = std::min({r, g, b});
                float lum = (0.2126f * r) + (0.7152f * g) + (0.0722f * b);
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
        if (n <= 0) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "no image samples");
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
        SampleBorderStats(ctx, pix, &stats);
        stats.valid = true;
    }
    fz_always(ctx) {
        if (pix) {
            fz_drop_pixmap(ctx, pix);
        }
    }
    fz_catch(ctx) {
        stats = PdfDarkModeImageSampleStats{};
    }
    return stats;
}

static bool PdfDarkModeStatsLookLikePhoto(const PdfDarkModeImageSampleStats& stats) {
    if (!stats.valid) {
        return false;
    }

    bool isPhoto = stats.significantBuckets >= 16 || stats.satRatio >= 0.18f || stats.lumVar >= 0.014f;
    if (stats.highLumRatio > 0.58f && stats.satRatio < 0.18f) {
        isPhoto = false;
    }
    if (stats.significantBuckets <= 12 && stats.lumVar < 0.012f && stats.highLumRatio > 0.45f) {
        isPhoto = false;
    }
    if (stats.highLumRatio > 0.72f && stats.satRatio < 0.18f) {
        isPhoto = false;
    }
    return isPhoto;
}

static bool PdfDarkModeStatsLookLikeFlatLayoutPanel(const PdfDarkModeImageSampleStats& stats) {
    if (!stats.valid) {
        return false;
    }
    return stats.highLumRatio > 0.76f && stats.lumVar < 0.011f && stats.significantBuckets <= 11 &&
           stats.satRatio < 0.17f;
}

static bool PdfDarkModeStatsLookLikeLayoutBackground(const PdfDarkModeImageSampleStats& stats) {
    if (!stats.valid) {
        return false;
    }
    if (PdfDarkModeStatsLookLikeFlatLayoutPanel(stats)) {
        return true;
    }
    // Cream/tan/yellow textbook panels and title cards — recolor for uniform dark page.
    if (stats.highLumRatio > 0.58f && stats.lumVar < 0.018f) {
        return true;
    }
    if (stats.highLumRatio > 0.44f && stats.lumVar < 0.022f) {
        return true;
    }
    if (stats.highLumRatio > 0.50f && stats.lumVar < 0.038f && stats.satRatio < 0.22f &&
        stats.significantBuckets <= 14) {
        return true;
    }
    return false;
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

static bool PdfDarkModeStatsLookLikeDarkArtwork(const PdfDarkModeImageSampleStats& stats, float pageCoverage) {
    if (!stats.valid || pageCoverage < 0.035f) {
        return false;
    }
    return stats.highLumRatio < 0.48f && stats.lumVar >= 0.004f &&
           (stats.significantBuckets >= 8 || stats.satRatio >= 0.08f);
}

// Mirrors PdfDarkModeFeaturesLookLikeLightBackdrop in PdfDarkModeImageRules.cpp.
static bool PdfDarkModeStatsLookLikeLightBackdrop(const PdfDarkModeImageSampleStats& stats) {
    if (!stats.valid) {
        return false;
    }
    return stats.borderLightRatio >= kLightBackdropBorderLight &&
           stats.borderUniformity >= kLightBackdropBorderUniformity;
}

static bool PdfDarkModeStatsLookLikePaperTextBox(const PdfDarkModeImageSampleStats& stats) {
    if (!stats.valid) {
        return false;
    }
    return stats.highLumRatio > 0.64f && stats.lumVar < 0.014f && stats.significantBuckets <= 12 &&
           stats.satRatio < 0.20f;
}

bool PdfDarkModeImageLooksLikePhoto(fz_context* ctx, fz_image* image) {
    return PdfDarkModeStatsLookLikePhoto(PdfDarkModeSampleImageStats(ctx, image));
}

bool PdfDarkModeImageLooksLikeDarkArtwork(fz_context* ctx, fz_image* image, float pageCoverage) {
    return PdfDarkModeStatsLookLikeDarkArtwork(PdfDarkModeSampleImageStats(ctx, image), pageCoverage);
}

// Stricter pixel gate used by PdfDarkModeShouldPreserveEmbeddedImageRect.
bool PdfDarkModeImageShouldPreserveInLegacy(fz_context* ctx, fz_image* image, float pageCoverage, int /*devW*/,
                                            int /*devH*/) {
    if (!ctx || !image) {
        return false;
    }
    PdfDarkModeImageSampleStats stats = PdfDarkModeSampleImageStats(ctx, image);
    if (PdfDarkModeStatsLookLikeFlatLayoutPanel(stats)) {
        return false;
    }
    if (PdfDarkModeStatsLookLikeLayoutBackground(stats)) {
        return false;
    }
    // artwork on a flat light backdrop: recolor so the backdrop follows the page
    // instead of staying a bright block on it (#6088)
    if (PdfDarkModeStatsLookLikeLightBackdrop(stats)) {
        return false;
    }
    if (PdfDarkModeStatsLookLikeDarkArtwork(stats, pageCoverage)) {
        return true;
    }
    if (PdfDarkModeStatsLookLikePhoto(stats)) {
        if (pageCoverage < 0.14f && PdfDarkModeStatsLookLikePaperTextBox(stats)) {
            return false;
        }
        return true;
    }
    return false;
}

bool PdfDarkModeImageIsConfirmedArtwork(fz_context* ctx, fz_image* image, float pageCoverage, int /*devW*/,
                                        int /*devH*/) {
    if (!ctx || !image) {
        return false;
    }
    PdfDarkModeImageSampleStats stats = PdfDarkModeSampleImageStats(ctx, image);
    if (PdfDarkModeStatsLookLikeFlatLayoutPanel(stats)) {
        return false;
    }
    if (PdfDarkModeStatsLookLikeLayoutBackground(stats)) {
        return false;
    }
    // artwork on a flat light backdrop: recolor so the backdrop follows the page
    // instead of staying a bright block on it (#6088)
    if (PdfDarkModeStatsLookLikeLightBackdrop(stats)) {
        return false;
    }
    if (PdfDarkModeStatsLookLikeDarkArtwork(stats, pageCoverage)) {
        return true;
    }
    if (PdfDarkModeStatsLookLikePhoto(stats)) {
        if (pageCoverage < 0.14f && PdfDarkModeStatsLookLikePaperTextBox(stats)) {
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
    int minPx = GetPreservePdfImagesMinSize();
    if (devW < minPx || devH < minPx) {
        return false;
    }
    if (!image) {
        return false;
    }
    return PdfDarkModeImageIsConfirmedArtwork(ctx, image, pageCoverage, devW, devH);
}
