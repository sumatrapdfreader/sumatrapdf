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
    bool valid = false;
};

static void SamplePixmapRgb(fz_context* ctx, fz_pixmap* pix, int x, int y, float* outR, float* outG, float* outB) {
    if (!pix || !pix->samples || x < 0 || y < 0 || x >= pix->w || y >= pix->h) {
        *outR = *outG = *outB = 0.f;
        return;
    }
    fz_colorspace* cs = pix->colorspace ? pix->colorspace : fz_device_rgb(ctx);
    fz_colorspace* rgb = fz_device_rgb(ctx);
    int n = pix->n;
    int stride = pix->stride;
    unsigned char* px = pix->samples + y * stride + x * n;
    float conv[FZ_MAX_COLORS] = {};
    float srcRgb[FZ_MAX_COLORS] = {};
    int components = fz_colorspace_n(ctx, cs);
    for (int c = 0; c < components && c < FZ_MAX_COLORS; c++) {
        conv[c] = px[c] / 255.f;
    }
    fz_convert_color(ctx, cs, conv, rgb, srcRgb, cs, fz_default_color_params);
    *outR = srcRgb[0];
    *outG = srcRgb[1];
    *outB = srcRgb[2];
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
                int ri = (int)(r * 255.f + 0.5f);
                int gi = (int)(g * 255.f + 0.5f);
                int bi = (int)(b * 255.f + 0.5f);
                int bucket = ((ri >> 4) << 8) | ((gi >> 4) << 4) | (bi >> 4);
                buckets[bucket]++;

                float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
                float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
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
        for (int i = 0; i < 4096; i++) {
            if (buckets[i] * 100 > n) {
                significantBuckets++;
            }
        }

        float lumMean = lumSum / n;
        stats.significantBuckets = significantBuckets;
        stats.lumVar = lumSqSum / n - lumMean * lumMean;
        stats.satRatio = (float)saturated / (float)n;
        stats.highLumRatio = (float)highLum / (float)n;
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
        if (newDy > imgPage.dy) {
            newDy = imgPage.dy;
        }
    }
    // Bbox wider than bitmap → trim width (avoids preserving a whole page column).
    if (bboxAspect > imageAspect * maxSkew) {
        float clampedDx = imgPage.dy * imageAspect;
        if (clampedDx < newDx) {
            newDx = clampedDx;
        }
    }
    if (newDx == imgPage.dx && newDy == imgPage.dy) {
        return imgPage;
    }
    return RectF(imgPage.x, imgPage.y, newDx, newDy);
}

RectF PdfDarkModeCapUnknownImagePageRect(const RectF& imgPage, float pageHeight) {
    if (imgPage.IsEmpty() || pageHeight <= 0.f) {
        return imgPage;
    }
    float maxH = pageHeight * 0.48f;
    if (imgPage.dy <= maxH) {
        return imgPage;
    }
    return RectF(imgPage.x, imgPage.y, imgPage.dx, maxH);
}

static bool PdfDarkModeStatsLookLikeDarkArtwork(const PdfDarkModeImageSampleStats& stats, float pageCoverage) {
    if (!stats.valid || pageCoverage < 0.035f) {
        return false;
    }
    return stats.highLumRatio < 0.48f && stats.lumVar >= 0.004f &&
           (stats.significantBuckets >= 8 || stats.satRatio >= 0.08f);
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

bool PdfDarkModeImageShouldPreserveInLegacy(fz_context* ctx, fz_image* image, float pageCoverage, int devW, int devH) {
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

bool PdfDarkModeImageIsConfirmedArtwork(fz_context* ctx, fz_image* image, float pageCoverage, int devW, int devH) {
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

bool PdfDarkModeShouldPreserveEmbeddedImageRect(fz_context* ctx, fz_image* image, float pageCoverage, int devW,
                                                int devH) {
    if (pageCoverage >= kMaxPreserveImagePageCoverage) {
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
