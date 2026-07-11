/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
}

#include "base/Base.h"

#include "PdfDarkMode.h"
#include "PdfDarkModeInternal.h"

#include <math.h>

static constexpr int kMaxMaskDim = 256;
static constexpr int kMaxBlendPixels = 4096 * 4096;

static float Clamp01(float v) {
    if (v < 0.f) {
        return 0.f;
    }
    if (v > 1.f) {
        return 1.f;
    }
    return v;
}

static float SmoothStep(float e0, float e1, float x) {
    if (e0 == e1) {
        return x >= e1 ? 1.f : 0.f;
    }
    float t = (x - e0) / (e1 - e0);
    if (t <= 0.f) {
        return 0.f;
    }
    if (t >= 1.f) {
        return 1.f;
    }
    return t * t * (3.f - 2.f * t);
}

static void ReadPixmapPixel(fz_context* ctx, fz_pixmap* pix, int x, int y, float* outR, float* outG, float* outB,
                            float* outA) {
    *outR = *outG = *outB = 0.f;
    *outA = 1.f;
    if (!pix || !pix->samples || x < 0 || y < 0 || x >= pix->w || y >= pix->h) {
        return;
    }
    fz_colorspace* cs = pix->colorspace ? pix->colorspace : fz_device_rgb(ctx);
    fz_colorspace* rgb = fz_device_rgb(ctx);
    int n = pix->n;
    int components = fz_colorspace_n(ctx, cs);
    unsigned char* px = pix->samples + y * pix->stride + x * n;
    float conv[FZ_MAX_COLORS] = {};
    float srcRgb[FZ_MAX_COLORS] = {};
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

static void RemapForegroundPixel(float r, float g, float b, const DarkModePalette& palette, float* outR, float* outG,
                                 float* outB) {
    float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float chroma = maxC - minC;

    if (chroma >= 0.06f) {
        ApplyAdaptiveDocumentDarkMode(r, g, b, palette, outR, outG, outB);
        return;
    }
    if (lum < 0.62f) {
        float mapped[3] = {};
        MapRgbToDarkThemeOklab(r, g, b, palette, mapped);
        *outR = mapped[0];
        *outG = mapped[1];
        *outB = mapped[2];
        return;
    }
    float mapped[3] = {};
    MapRgbToDarkThemeOklab(r, g, b, palette, mapped);
    *outR = mapped[0];
    *outG = mapped[1];
    *outB = mapped[2];
}

static float SampleMaskBilinear(const float* mask, int maskW, int maskH, float u, float v) {
    if (!mask || maskW <= 0 || maskH <= 0) {
        return 0.f;
    }
    float fx = u * (float)(maskW - 1);
    float fy = v * (float)(maskH - 1);
    int x0 = (int)fx;
    int y0 = (int)fy;
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    if (x1 >= maskW) {
        x1 = maskW - 1;
    }
    if (y1 >= maskH) {
        y1 = maskH - 1;
    }
    float tx = fx - x0;
    float ty = fy - y0;
    float v00 = mask[y0 * maskW + x0];
    float v10 = mask[y0 * maskW + x1];
    float v01 = mask[y1 * maskW + x0];
    float v11 = mask[y1 * maskW + x1];
    float a = v00 + (v10 - v00) * tx;
    float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty;
}

static void BlurMaskBox3(float* mask, int maskW, int maskH) {
    if (!mask || maskW < 3 || maskH < 3) {
        return;
    }
    int n = maskW * maskH;
    float* tmp = (float*)malloc((size_t)n * sizeof(float));
    if (!tmp) {
        return;
    }
    for (int y = 0; y < maskH; y++) {
        for (int x = 0; x < maskW; x++) {
            float sum = 0.f;
            int count = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < maskW && ny >= 0 && ny < maskH) {
                        sum += mask[ny * maskW + nx];
                        count++;
                    }
                }
            }
            tmp[y * maskW + x] = sum / (float)count;
        }
    }
    memcpy(mask, tmp, (size_t)n * sizeof(float));
    free(tmp);
}

static bool BuildEdgeConnectedBgMask(fz_context* ctx, fz_pixmap* src, float bgR, float bgG, float bgB, float* outFgConf,
                                     int maskW, int maskH) {
    if (!src || !outFgConf || maskW <= 0 || maskH <= 0) {
        return false;
    }

    int n = maskW * maskH;
    unsigned char* flood = (unsigned char*)malloc((size_t)n);
    unsigned char* queued = (unsigned char*)malloc((size_t)n);
    int* queue = (int*)malloc((size_t)n * sizeof(int));
    float* bgScore = (float*)malloc((size_t)n * sizeof(float));
    if (!flood || !queued || !queue || !bgScore) {
        free(flood);
        free(queued);
        free(queue);
        free(bgScore);
        return false;
    }
    memset(flood, 0, (size_t)n);
    memset(queued, 0, (size_t)n);

    const float bgDistHard = 0.055f;
    const float bgDistSoft = 0.11f;

    for (int my = 0; my < maskH; my++) {
        int sy = (my * src->h) / maskH;
        if (sy >= src->h) {
            sy = src->h - 1;
        }
        for (int mx = 0; mx < maskW; mx++) {
            int sx = (mx * src->w) / maskW;
            if (sx >= src->w) {
                sx = src->w - 1;
            }
            float r, g, b, a;
            ReadPixmapPixel(ctx, src, sx, sy, &r, &g, &b, &a);
            if (a < 0.08f) {
                bgScore[my * maskW + mx] = 1.f;
                continue;
            }
            float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            float dist = PdfDarkModeOklabDistance(r, g, b, bgR, bgG, bgB);
            float score = 1.f - SmoothStep(bgDistHard, bgDistSoft, dist);
            if (lum < 0.50f) {
                score *= SmoothStep(0.50f, 0.38f, lum);
            }
            bgScore[my * maskW + mx] = Clamp01(score);
        }
    }

    int qHead = 0;
    int qTail = 0;
    auto enqueue = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= maskW || y >= maskH) {
            return;
        }
        int idx = y * maskW + x;
        if (queued[idx]) {
            return;
        }
        queued[idx] = 1;
        queue[qTail++] = idx;
    };

    for (int x = 0; x < maskW; x++) {
        enqueue(x, 0);
        enqueue(x, maskH - 1);
    }
    for (int y = 0; y < maskH; y++) {
        enqueue(0, y);
        enqueue(maskW - 1, y);
    }

    while (qHead < qTail) {
        int idx = queue[qHead++];
        int x = idx % maskW;
        int y = idx / maskW;
        if (bgScore[idx] < 0.35f) {
            continue;
        }
        flood[idx] = 1;
        enqueue(x - 1, y);
        enqueue(x + 1, y);
        enqueue(x, y - 1);
        enqueue(x, y + 1);
    }

    for (int i = 0; i < n; i++) {
        float bgProb = bgScore[i] * (flood[i] ? 1.f : 0.f);
        outFgConf[i] = Clamp01(1.f - bgProb);
    }

    BlurMaskBox3(outFgConf, maskW, maskH);

    free(flood);
    free(queued);
    free(queue);
    free(bgScore);
    return true;
}

fz_pixmap* PdfDarkModeProcessLightBackgroundPixmap(fz_context* ctx, fz_pixmap* src, const DarkImageAnalysis& analysis,
                                                   const DarkModePalette& palette) {
    if (!ctx || !src || !src->samples || src->w <= 0 || src->h <= 0) {
        return nullptr;
    }
    if (!PdfDarkModeShouldBlendLightBackground(analysis)) {
        return nullptr;
    }
    if ((i64)src->w * src->h > kMaxBlendPixels) {
        return nullptr;
    }

    float bgR = analysis.estimatedBackground.r;
    float bgG = analysis.estimatedBackground.g;
    float bgB = analysis.estimatedBackground.b;
    if (bgR <= 0.f && bgG <= 0.f && bgB <= 0.f) {
        bgR = bgG = bgB = 0.95f;
    }

    int maskW = src->w;
    int maskH = src->h;
    if (maskW > kMaxMaskDim || maskH > kMaxMaskDim) {
        if (maskW >= maskH) {
            maskH = (maskH * kMaxMaskDim) / maskW;
            maskW = kMaxMaskDim;
        } else {
            maskW = (maskW * kMaxMaskDim) / maskH;
            maskH = kMaxMaskDim;
        }
        if (maskW < 1) {
            maskW = 1;
        }
        if (maskH < 1) {
            maskH = 1;
        }
    }

    int maskN = maskW * maskH;
    float* fgMask = (float*)malloc((size_t)maskN * sizeof(float));
    if (!fgMask) {
        return nullptr;
    }
    if (!BuildEdgeConnectedBgMask(ctx, src, bgR, bgG, bgB, fgMask, maskW, maskH)) {
        free(fgMask);
        return nullptr;
    }

    fz_colorspace* cs = src->colorspace ? src->colorspace : fz_device_rgb(ctx);
    fz_pixmap* dst = nullptr;
    fz_var(dst);
    fz_try(ctx) {
        dst = fz_new_pixmap(ctx, cs, src->w, src->h, src->seps, 1);
        fz_clear_pixmap_with_value(ctx, dst, 0x00);

        int n = dst->n;
        int components = fz_colorspace_n(ctx, cs);
        fz_colorspace* rgb = fz_device_rgb(ctx);

        for (int y = 0; y < src->h; y++) {
            float v = src->h > 1 ? (float)y / (float)(src->h - 1) : 0.f;
            for (int x = 0; x < src->w; x++) {
                float u = src->w > 1 ? (float)x / (float)(src->w - 1) : 0.f;
                float fgConf = SampleMaskBilinear(fgMask, maskW, maskH, u, v);

                float r, g, b, a;
                ReadPixmapPixel(ctx, src, x, y, &r, &g, &b, &a);
                unsigned char* px = dst->samples + y * dst->stride + x * n;

                if (fgConf < 0.04f || a < 0.02f) {
                    for (int c = 0; c < components && c < FZ_MAX_COLORS; c++) {
                        px[c] = 0;
                    }
                    if (dst->alpha) {
                        px[components] = 0;
                    }
                    continue;
                }

                float nr, ng, nb;
                RemapForegroundPixel(r, g, b, palette, &nr, &ng, &nb);

                float outRgb[FZ_MAX_COLORS] = {nr, ng, nb};
                float back[FZ_MAX_COLORS] = {};
                fz_convert_color(ctx, rgb, outRgb, cs, back, cs, fz_default_color_params);
                for (int c = 0; c < components && c < FZ_MAX_COLORS; c++) {
                    int vpx = (int)(back[c] * 255.f + 0.5f);
                    if (vpx < 0) {
                        vpx = 0;
                    }
                    if (vpx > 255) {
                        vpx = 255;
                    }
                    px[c] = (unsigned char)vpx;
                }
                if (dst->alpha) {
                    int av = (int)(a * fgConf * 255.f + 0.5f);
                    if (av < 0) {
                        av = 0;
                    }
                    if (av > 255) {
                        av = 255;
                    }
                    px[components] = (unsigned char)av;
                }
            }
        }
    }
    fz_always(ctx) {
        free(fgMask);
    }
    fz_catch(ctx) {
        if (dst) {
            fz_drop_pixmap(ctx, dst);
        }
        return nullptr;
    }
    return dst;
}
