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
        blockW = std::max(blockW, 1);
        blockH = std::max(blockH, 1);
        int samplesPerBlock = (blockW * blockH > 0) ? (kMaxImageSamples / (kGridBlocks * kGridBlocks)) + 1 : 1;
        samplesPerBlock = std::max(samplesPerBlock, 1);

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
                        PdfDarkModeSampleRgb(ctx, pix, x, y, &r, &g, &b, &a);
                        if (a < kImageMinAlpha) {
                            transparent++;
                            n++;
                            continue;
                        }
                        int ri = (int)lroundf(r * 255.f);
                        int gi = (int)lroundf(g * 255.f);
                        int bi = (int)lroundf(b * 255.f);
                        buckets[((ri >> 4) << 8) | ((gi >> 4) << 4) | (bi >> 4)]++;

                        float maxC = std::max({r, g, b});
                        float minC = std::min({r, g, b});
                        float lum = (0.2126f * r) + (0.7152f * g) + (0.0722f * b);
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
                        int bi2 = (by * kGridBlocks) + bx;
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
        for (int bucket : buckets) {
            if (bucket * 100 > n) {
                significantBuckets++;
            }
        }

        float lumMean = lumSum / (float)n;
        outFeatures->luminanceVariance = (lumSqSum / (float)n) - (lumMean * lumMean);
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
            float mean = blockLum[i] / (float)blockCount[i];
            blockVarSum += (mean - lumMean) * (mean - lumMean);
            if (mean > 0.78f || mean < 0.12f) {
                flatBlocks++;
            }
        }
        outFeatures->textureScore = blockVarSum / (float)(kGridBlocks * kGridBlocks);
        outFeatures->flatAreaRatio = (float)flatBlocks / (float)(kGridBlocks * kGridBlocks);

        PdfDarkModeSampleBorder(ctx, pix, kImageBorderSamples, kImageMinAlpha, &outFeatures->borderLightRatio,
                                &outFeatures->borderUniformity, outBackground);
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
    if (policy == DarkImagePolicy::AdaptiveDocument && PdfDarkModeImageIsConfirmedArtwork(ctx, image, pageCoverage)) {
        result.kind = DarkImageKind::Photo;
        result.confidence = 0.72f;
    }
    return result;
}
