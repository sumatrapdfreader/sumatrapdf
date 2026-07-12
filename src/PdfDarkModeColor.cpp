/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "Settings.h"
#include "GlobalPrefs.h"
#include "Theme.h"
#include "Translations.h"

#include "PdfDarkMode.h"
#include "PdfDarkModeInternal.h"

// Hardcoded PDF dark mode defaults (not persisted in settings file).
static constexpr int kPreservePdfImagesMinSize = 72;
// Object-level Smart Dark is opt-in until image/color heuristics are ready (Phase 2+).
static constexpr PdfDarkModeRenderer kPdfDarkModeRenderer = PdfDarkModeRenderer::LegacyBitmapPostProcess;

static bool gPreservePdfImagesInDarkMode = true;

// dark page rendering is active when the effective page background is dark
// (DocumentColorsFollowTheme or custom dark FixedPageUI colors); master's
// themes never touch page colors, unlike the fork's
static bool DarkChromeActive() {
    COLORREF bg;
    ThemePageRenderColors(bg);
    return !IsLightColor(bg);
}

static DocumentColorsFollowTheme DocumentColorsFollowThemeFromString(Str v) {
    if (!v || str::EqI(v, StrL("off"))) {
        return DocumentColorsFollowTheme::Off;
    }
    if (str::EqI(v, StrL("smart"))) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (str::EqI(v, StrL("legacy"))) {
        return DocumentColorsFollowTheme::Legacy;
    }
    // migrate pre-3.7 DocumentColorMode values
    if (str::EqI(v, StrL("auto"))) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (str::EqI(v, StrL("black"))) {
        return DocumentColorsFollowTheme::Legacy;
    }
    if (str::EqI(v, StrL("none")) || str::EqI(v, StrL("light"))) {
        return DocumentColorsFollowTheme::Off;
    }
    return DocumentColorsFollowTheme::Off;
}

static const char* DocumentColorsFollowThemeToString(DocumentColorsFollowTheme mode) {
    switch (mode) {
        case DocumentColorsFollowTheme::Smart:
            return "smart";
        case DocumentColorsFollowTheme::Legacy:
            return "legacy";
        case DocumentColorsFollowTheme::Off:
        default:
            return "off";
    }
}

static int gShadeForwardCount = 0;

void PdfDarkModeRecordShadeForward() {
    gShadeForwardCount++;
}

int PdfDarkModeTakeShadeForwardCount() {
    int n = gShadeForwardCount;
    gShadeForwardCount = 0;
    return n;
}

bool GetPreservePdfImagesInDarkMode() {
    return gPreservePdfImagesInDarkMode;
}

void SetPreservePdfImagesInDarkMode(bool preserve) {
    gPreservePdfImagesInDarkMode = preserve;
}

int GetPreservePdfImagesMinSize() {
    return kPreservePdfImagesMinSize;
}

PdfDarkModeRenderer GetPdfDarkModeRenderer() {
    return kPdfDarkModeRenderer;
}

bool DocumentColorsFollowThemeEnabled() {
    return GetDocumentColorsFollowTheme() != DocumentColorsFollowTheme::Off;
}

DocumentColorsFollowTheme GetDocumentColorsFollowTheme() {
    if (!gGlobalPrefs || !gGlobalPrefs->documentColorsFollowTheme) {
        return DocumentColorsFollowTheme::Off;
    }
    return DocumentColorsFollowThemeFromString(gGlobalPrefs->documentColorsFollowTheme);
}

void SetDocumentColorsFollowTheme(DocumentColorsFollowTheme mode) {
    if (mode < DocumentColorsFollowTheme::Off || mode > DocumentColorsFollowTheme::Legacy) {
        mode = DocumentColorsFollowTheme::Off;
    }
    if (!gGlobalPrefs) {
        return;
    }
    Str name(DocumentColorsFollowThemeToString(mode));
    if (!str::EqI(gGlobalPrefs->documentColorsFollowTheme, name)) {
        str::ReplaceWithCopy(&gGlobalPrefs->documentColorsFollowTheme, name);
    }
}

const char* DocumentColorsFollowThemeDescription(DocumentColorsFollowTheme mode) {
    switch (mode) {
        case DocumentColorsFollowTheme::Smart:
            return _TRN("Document colors follow theme: Smart (recolor text and background, not images)");
        case DocumentColorsFollowTheme::Legacy:
            return _TRN("Document colors follow theme: Legacy (recolor text, background and images)");
        case DocumentColorsFollowTheme::Off:
        default:
            return _TRN("Document colors follow theme: Off");
    }
}

bool PdfDarkModeUsesObjectLevel() {
    if (!DarkChromeActive()) {
        return false;
    }
    if (GetDocumentColorsFollowTheme() != DocumentColorsFollowTheme::Smart) {
        return false;
    }
    return GetPdfDarkModeRenderer() == PdfDarkModeRenderer::ObjectLevelDevice;
}

void PdfDarkModeClearPixmapToThemeBackground(fz_context* ctx, fz_pixmap* pix, const DarkModePalette& palette) {
    if (!pix || !pix->samples) {
        return;
    }
    byte rb = (byte)(palette.bgR * 255.f + 0.5f);
    byte gb = (byte)(palette.bgG * 255.f + 0.5f);
    byte bb = (byte)(palette.bgB * 255.f + 0.5f);
    int w = pix->w;
    int h = pix->h;
    int n = pix->n;
    for (int y = 0; y < h; y++) {
        unsigned char* row = pix->samples + (size_t)y * pix->stride;
        for (int x = 0; x < w; x++) {
            unsigned char* p = row + x * n;
            p[0] = rb;
            p[1] = gb;
            p[2] = bb;
            if (pix->alpha && n >= 4) {
                p[3] = 255;
            }
        }
    }
}

DarkModeOptions PdfDarkModeCurrentOptions() {
    DarkModeOptions opts;
    if (PdfDarkModeUsesObjectLevel()) {
        opts.preserveImagePaperSoftening = 0.75f;
    }
    return opts;
}

u32 PdfDarkModeComputeOptionsHash() {
    DarkModeProfile profile;
    BuildViewDarkModeProfile(nullptr, &profile);
    return profile.hash;
}

DarkModePalette PdfDarkModeBuildPalette() {
    DarkModeProfile profile;
    BuildViewDarkModeProfile(nullptr, &profile);
    return profile.palette;
}

static bool IsLikelyLinkRgb(float r, float g, float b) {
    int ri = (int)(r * 255.f + 0.5f);
    int gi = (int)(g * 255.f + 0.5f);
    int bi = (int)(b * 255.f + 0.5f);
    int maxRG = ri > gi ? ri : gi;
    if (bi < maxRG + 25) {
        return false;
    }
    if (bi < 72) {
        return false;
    }
    int lum = (ri + gi + bi) / 3;
    if (lum > 230) {
        return false;
    }
    return true;
}

static float SmoothStep(float edge0, float edge1, float x) {
    if (edge0 == edge1) {
        return x >= edge1 ? 1.f : 0.f;
    }
    float t = (x - edge0) / (edge1 - edge0);
    if (t <= 0.f) {
        return 0.f;
    }
    if (t >= 1.f) {
        return 1.f;
    }
    return t * t * (3.f - 2.f * t);
}

void ApplyAdaptiveDocumentDarkMode(float r, float g, float b, const DarkModePalette& palette, float* outR, float* outG,
                                   float* outB) {
    float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float chroma = maxC - minC;

    const float lowChroma = 0.08f;
    const float paperLum = 0.62f;
    const float inkLum = 0.28f;

    if (chroma < lowChroma) {
        // Low luminance = ink -> theme text; high luminance = paper -> theme background.
        float inkW = 1.f - SmoothStep(inkLum, paperLum, lum);
        float paperW = SmoothStep(inkLum, paperLum, lum);
        float nr = palette.textR * inkW + palette.bgR * paperW;
        float ng = palette.textG * inkW + palette.bgG * paperW;
        float nb = palette.textB * inkW + palette.bgB * paperW;
        float grayW = 1.f - chroma / lowChroma;
        *outR = nr * grayW + r * (1.f - grayW);
        *outG = ng * grayW + g * (1.f - grayW);
        *outB = nb * grayW + b * (1.f - grayW);
        return;
    }

    float h = 0.f;
    float delta = maxC - minC;
    if (delta > 0.0001f) {
        if (maxC == r) {
            h = fmodf((g - b) / delta, 6.f);
        } else if (maxC == g) {
            h = (b - r) / delta + 2.f;
        } else {
            h = (r - g) / delta + 4.f;
        }
        h /= 6.f;
        if (h < 0.f) {
            h += 1.f;
        }
    }

    float cappedV = lum;
    const float maxBright = 0.82f;
    if (cappedV > maxBright) {
        cappedV = maxBright;
    }
    const float minBright = 0.12f;
    if (cappedV < minBright) {
        cappedV = minBright;
    }

    float s = maxC > 0.f ? delta / maxC : 0.f;
    float c = cappedV * s;
    float x = c * (1.f - fabsf(fmodf(h * 6.f, 2.f) - 1.f));
    float m = cappedV - c;
    float rr = 0.f, gg = 0.f, bb = 0.f;
    int hi = (int)(h * 6.f);
    switch (hi % 6) {
        case 0:
            rr = c;
            gg = x;
            break;
        case 1:
            rr = x;
            gg = c;
            break;
        case 2:
            gg = c;
            bb = x;
            break;
        case 3:
            gg = x;
            bb = c;
            break;
        case 4:
            rr = x;
            bb = c;
            break;
        default:
            rr = c;
            bb = x;
            break;
    }
    *outR = rr + m;
    *outG = gg + m;
    *outB = bb + m;
}

void MapRgbToDarkTheme(float r, float g, float b, const DarkModePalette& palette, float* outRgb) {
    if (PdfDarkModeUsesObjectLevel()) {
        MapRgbToDarkThemeOklab(r, g, b, palette, outRgb);
        return;
    }
    outRgb[0] = palette.textR + r * palette.diffR;
    outRgb[1] = palette.textG + g * palette.diffG;
    outRgb[2] = palette.textB + b * palette.diffB;
}

void MapColorToDarkTheme(fz_context* ctx, fz_colorspace* cs, const float* color, fz_color_params colorParams,
                         const DarkModePalette& palette, float* outRgb) {
    float rgb[FZ_MAX_COLORS] = {};
    fz_colorspace* ds = fz_device_rgb(ctx);
    fz_convert_color(ctx, cs, color, ds, rgb, cs, colorParams);
    if (DarkChromeActive() && IsLikelyLinkRgb(rgb[0], rgb[1], rgb[2])) {
        outRgb[0] = palette.linkR;
        outRgb[1] = palette.linkG;
        outRgb[2] = palette.linkB;
        return;
    }
    MapRgbToDarkTheme(rgb[0], rgb[1], rgb[2], palette, outRgb);
}

void MapFillColorToDarkTheme(fz_context* ctx, fz_colorspace* cs, const float* color, fz_color_params colorParams,
                             const DarkModePalette& palette, float* outRgb) {
    float rgb[FZ_MAX_COLORS] = {};
    fz_colorspace* ds = fz_device_rgb(ctx);
    fz_convert_color(ctx, cs, color, ds, rgb, cs, colorParams);
    MapRgbFillToDarkTheme(rgb[0], rgb[1], rgb[2], palette, outRgb);
}

void MapRgbFillToDarkTheme(float r, float g, float b, const DarkModePalette& palette, float* outRgb) {
    float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float chroma = maxC - minC;
    DarkModeOptions opts = PdfDarkModeCurrentOptions();
    if (GetDocumentColorsFollowTheme() != DocumentColorsFollowTheme::Smart && lum >= opts.lightFillLuminanceThreshold &&
        chroma >= opts.lightFillChromaThreshold) {
        ApplyAdaptiveDocumentDarkMode(r, g, b, palette, &outRgb[0], &outRgb[1], &outRgb[2]);
        return;
    }
    MapRgbToDarkTheme(r, g, b, palette, outRgb);
}

void ApplyPreserveImagePaperSoftening(float r, float g, float b, const DarkModePalette& palette, float strength,
                                      float* outR, float* outG, float* outB) {
    if (strength <= 0.f) {
        *outR = r;
        *outG = g;
        *outB = b;
        return;
    }

    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float chroma = maxC - minC;

    const float lowChroma = 0.10f;
    float paperW = 0.f;
    if (chroma < lowChroma) {
        // Only soften near-white paper; never pull ink pixels toward the background.
        paperW = SmoothStep(0.72f, 0.94f, lum);
    } else {
        float chromaFactor = 1.f - chroma / 0.45f;
        if (chromaFactor < 0.f) {
            chromaFactor = 0.f;
        }
        paperW = SmoothStep(0.72f, 0.94f, lum) * chromaFactor;
    }
    paperW *= strength;

    *outR = r + (palette.bgR - r) * paperW;
    *outG = g + (palette.bgG - g) * paperW;
    *outB = b + (palette.bgB - b) * paperW;
}

bool PdfDarkModeIsDecorativeStripImage(const RectF& imgRect, const RectF& pageBounds) {
    if (imgRect.IsEmpty() || pageBounds.IsEmpty()) {
        return false;
    }
    float w = imgRect.dx;
    float h = imgRect.dy;
    if (w <= 0.f || h <= 0.f) {
        return false;
    }
    float pageW = pageBounds.dx;
    float pageH = pageBounds.dy;
    if (pageW <= 0.f || pageH <= 0.f) {
        return false;
    }

    float wFrac = w / pageW;
    float hFrac = h / pageH;
    float minDim = w < h ? w : h;
    float maxDim = w > h ? w : h;
    float aspect = minDim / maxDim;

    // Tall narrow or wide shallow strips (spiral margins, side shadows).
    if (aspect < 0.22f) {
        return true;
    }
    // Edge-aligned column/row spanning a substantial part of the page.
    if (wFrac < 0.20f && hFrac > 0.30f) {
        return true;
    }
    if (hFrac < 0.20f && wFrac > 0.30f) {
        return true;
    }
    return false;
}
