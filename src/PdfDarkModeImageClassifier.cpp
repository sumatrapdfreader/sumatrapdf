/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "PdfDarkMode.h"
#include "PdfDarkModeInternal.h"

// Chromium DarkModeImageClassifier inspired sampling/decision rules (lightweight, no Skia/Blink).

static constexpr int kMaxImageSamples = 1000;
static constexpr int kGridBlocks = 10;
static constexpr int kColorBuckets = 4096;

static void SamplePixmapRgb(fz_context* ctx, fz_pixmap* pix, int x, int y, float* outR, float* outG, float* outB,
                            float* outA) {
    *outR = *outG = *outB = 0.f;
    *outA = 1.f;
    if (!pix || !pix->samples || x < 0 || y < 0 || x >= pix->w || y >= pix->h) {
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
    if (pix->alpha && n > components) {
        *outA = px[components] / 255.f;
    }
}

static bool PdfDarkModeExtractFeatures(fz_context* ctx, fz_image* image, float pageCoverage,
                                       DarkImageFeatures* outFeatures, PixelColor* outBackground) {
    if (!ctx || !image || !outFeatures) {
        return false;
    }
    *outFeatures = DarkImageFeatures{};
    outFeatures->pageCoverage = pageCoverage;

    fz_pixmap* pix = nullptr;
    fz_var(pix);
    fz_try(ctx) {
        int targetW = image->w > 0 ? image->w : 1;
        int targetH = image->h > 0 ? image->h : 1;
        const int maxDim = 128;
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

        int buckets[kColorBuckets] = {};
        int n = 0;
        int transparent = 0;
        int highLum = 0;
        int saturated = 0;
        int chromatic = 0;
        float lumSum = 0.f;
        float lumSqSum = 0.f;

        int blockW = pix->w / kGridBlocks;
        int blockH = pix->h / kGridBlocks;
        if (blockW < 1) {
            blockW = 1;
        }
        if (blockH < 1) {
            blockH = 1;
        }
        int samplesPerBlock = (blockW * blockH > 0) ? (kMaxImageSamples / (kGridBlocks * kGridBlocks)) + 1 : 1;
        if (samplesPerBlock < 1) {
            samplesPerBlock = 1;
        }

        float blockLum[kGridBlocks * kGridBlocks] = {};
        int blockCount[kGridBlocks * kGridBlocks] = {};

        for (int by = 0; by < kGridBlocks && n < kMaxImageSamples; by++) {
            for (int bx = 0; bx < kGridBlocks && n < kMaxImageSamples; bx++) {
                int x0 = bx * blockW;
                int y0 = by * blockH;
                int x1 = bx == kGridBlocks - 1 ? pix->w : x0 + blockW;
                int y1 = by == kGridBlocks - 1 ? pix->h : y0 + blockH;
                int stepX = (x1 - x0) > samplesPerBlock ? (x1 - x0) / samplesPerBlock : 1;
                int stepY = (y1 - y0) > samplesPerBlock ? (y1 - y0) / samplesPerBlock : 1;
                for (int y = y0; y < y1 && n < kMaxImageSamples; y += stepY) {
                    for (int x = x0; x < x1 && n < kMaxImageSamples; x += stepX) {
                        float r, g, b, a;
                        SamplePixmapRgb(ctx, pix, x, y, &r, &g, &b, &a);
                        if (a < 0.08f) {
                            transparent++;
                            n++;
                            continue;
                        }
                        int ri = (int)(r * 255.f + 0.5f);
                        int gi = (int)(g * 255.f + 0.5f);
                        int bi = (int)(b * 255.f + 0.5f);
                        buckets[((ri >> 4) << 8) | ((gi >> 4) << 4) | (bi >> 4)]++;

                        float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
                        float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
                        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                        lumSum += lum;
                        lumSqSum += lum * lum;
                        if (maxC - minC > 0.12f) {
                            saturated++;
                        }
                        if (maxC - minC > 0.06f) {
                            chromatic++;
                        }
                        if (lum > 0.72f) {
                            highLum++;
                        }
                        int bi2 = by * kGridBlocks + bx;
                        blockLum[bi2] += lum;
                        blockCount[bi2]++;
                        n++;
                    }
                }
            }
        }

        if (n <= 0) {
            fz_throw(ctx, FZ_ERROR_GENERIC, "no image samples");
        }

        int significantBuckets = 0;
        for (int i = 0; i < kColorBuckets; i++) {
            if (buckets[i] * 100 > n) {
                significantBuckets++;
            }
        }

        float lumMean = lumSum / n;
        outFeatures->luminanceVariance = lumSqSum / n - lumMean * lumMean;
        outFeatures->colorBucketRatio = (float)significantBuckets / (float)kColorBuckets;
        outFeatures->transparentRatio = (float)transparent / (float)n;
        outFeatures->highLuminanceRatio = (float)highLum / (float)n;
        outFeatures->saturatedPixelRatio = (float)saturated / (float)n;
        outFeatures->chromaticPixelRatio = (float)chromatic / (float)n;
        outFeatures->isColorful = significantBuckets >= 14 || outFeatures->saturatedPixelRatio >= 0.16f;

        float blockVarSum = 0.f;
        int flatBlocks = 0;
        for (int i = 0; i < kGridBlocks * kGridBlocks; i++) {
            if (blockCount[i] <= 0) {
                continue;
            }
            float mean = blockLum[i] / blockCount[i];
            blockVarSum += (mean - lumMean) * (mean - lumMean);
            if (mean > 0.78f || mean < 0.12f) {
                flatBlocks++;
            }
        }
        outFeatures->textureScore = blockVarSum / (float)(kGridBlocks * kGridBlocks);
        outFeatures->flatAreaRatio = (float)flatBlocks / (float)(kGridBlocks * kGridBlocks);

        // Border sampling (4 edges, up to 64 px each).
        float borderLum[256] = {};
        float borderR[256] = {};
        float borderG[256] = {};
        float borderB[256] = {};
        int borderN = 0;
        int borderLight = 0;
        auto sample_edge = [&](int x, int y) {
            if (borderN >= 256) {
                return;
            }
            float r, g, b, a;
            SamplePixmapRgb(ctx, pix, x, y, &r, &g, &b, &a);
            if (a < 0.08f) {
                return;
            }
            float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            borderLum[borderN] = lum;
            borderR[borderN] = r;
            borderG[borderN] = g;
            borderB[borderN] = b;
            if (lum > 0.72f) {
                borderLight++;
            }
            borderN++;
        };
        int edgeStep = pix->w >= 32 ? pix->w / 32 : 1;
        for (int x = 0; x < pix->w; x += edgeStep) {
            sample_edge(x, 0);
            sample_edge(x, pix->h - 1);
        }
        edgeStep = pix->h >= 32 ? pix->h / 32 : 1;
        for (int y = 0; y < pix->h; y += edgeStep) {
            sample_edge(0, y);
            sample_edge(pix->w - 1, y);
        }
        if (borderN > 0) {
            outFeatures->borderLightRatio = (float)borderLight / (float)borderN;
            float br = 0.f, bg = 0.f, bb = 0.f;
            for (int i = 0; i < borderN; i++) {
                br += borderR[i];
                bg += borderG[i];
                bb += borderB[i];
            }
            br /= borderN;
            bg /= borderN;
            bb /= borderN;
            if (outBackground) {
                outBackground->r = br;
                outBackground->g = bg;
                outBackground->b = bb;
            }
            float borderVar = 0.f;
            for (int i = 0; i < borderN; i++) {
                float dr = borderR[i] - br;
                float dg = borderG[i] - bg;
                float db = borderB[i] - bb;
                borderVar += dr * dr + dg * dg + db * db;
            }
            borderVar /= borderN;
            outFeatures->borderUniformity = 1.f - (borderVar / 0.12f);
            if (outFeatures->borderUniformity < 0.f) {
                outFeatures->borderUniformity = 0.f;
            }
            if (outFeatures->borderUniformity > 1.f) {
                outFeatures->borderUniformity = 1.f;
            }
        }
    }
    fz_always(ctx) {
        if (pix) {
            fz_drop_pixmap(ctx, pix);
        }
    }
    fz_catch(ctx) {
        return false;
    }
    return true;
}

DarkImageAnalysis PdfDarkModeAnalyzeImage(fz_context* ctx, fz_image* image, float pageCoverage,
                                          bool pageIsScannedHint) {
    return PdfDarkModeAnalyzeImageCached(ctx, image, pageCoverage, pageIsScannedHint, nullptr);
}

DarkImageAnalysis PdfDarkModeAnalyzeImageCached(fz_context* ctx, fz_image* image, float pageCoverage,
                                                bool pageIsScannedHint, DarkModeEngineCache* engineCache) {
    DarkImageAnalysis result;
    if (!ctx || !image) {
        return result;
    }
    bool haveFeatures = false;
    if (engineCache &&
        PdfDarkModeEngineCacheLookupFeatures(engineCache, image, &result.features, &result.estimatedBackground)) {
        haveFeatures = true;
    }
    if (!haveFeatures) {
        if (!PdfDarkModeExtractFeatures(ctx, image, pageCoverage, &result.features, &result.estimatedBackground)) {
            result.kind = DarkImageKind::Unknown;
            result.confidence = 0.f;
            return result;
        }
        if (engineCache) {
            PdfDarkModeEngineCacheStoreFeatures(ctx, engineCache, image, result.features, result.estimatedBackground);
        }
    }
    result.kind =
        PdfDarkModeClassifyImageFeatures(result.features, pageCoverage, pageIsScannedHint, &result.confidence);
    DarkImagePolicy policy = PdfDarkModePolicyForImageKind(result.kind, false);
    if (policy == DarkImagePolicy::AdaptiveDocument &&
        PdfDarkModeImageShouldPreserveInLegacy(ctx, image, pageCoverage)) {
        result.kind = DarkImageKind::Photo;
        result.confidence = 0.72f;
    }
    return result;
}
