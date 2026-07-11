/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PdfDarkMode.h"

#include "base/UtAssert.h"

static DarkImageFeatures PhotoLikeFeatures() {
    DarkImageFeatures f;
    f.isColorful = true;
    f.colorBucketRatio = 0.04f;
    f.highLuminanceRatio = 0.28f;
    f.saturatedPixelRatio = 0.18f;
    f.luminanceVariance = 0.022f;
    f.borderLightRatio = 0.35f;
    f.borderUniformity = 0.40f;
    f.flatAreaRatio = 0.12f;
    return f;
}

static DarkImageFeatures LightBackgroundArtFeatures() {
    DarkImageFeatures f;
    f.isColorful = false;
    f.colorBucketRatio = 0.015f;
    f.highLuminanceRatio = 0.58f;
    f.saturatedPixelRatio = 0.04f;
    f.luminanceVariance = 0.008f;
    f.borderLightRatio = 0.78f;
    f.borderUniformity = 0.82f;
    f.flatAreaRatio = 0.62f;
    return f;
}

static DarkImageFeatures IconFeatures() {
    DarkImageFeatures f;
    f.isColorful = false;
    f.colorBucketRatio = 0.008f;
    f.highLuminanceRatio = 0.68f;
    f.saturatedPixelRatio = 0.02f;
    f.luminanceVariance = 0.006f;
    f.borderLightRatio = 0.55f;
    f.borderUniformity = 0.50f;
    f.flatAreaRatio = 0.70f;
    return f;
}

static DarkImageFeatures BrightFilmStillFeatures() {
    DarkImageFeatures f;
    f.isColorful = true;
    f.colorBucketRatio = 22.f / 4096.f;
    f.highLuminanceRatio = 0.62f;
    f.saturatedPixelRatio = 0.22f;
    f.chromaticPixelRatio = 0.28f;
    f.luminanceVariance = 0.021f;
    f.borderLightRatio = 0.72f;
    f.borderUniformity = 0.75f;
    f.flatAreaRatio = 0.40f;
    return f;
}

void PdfDarkModeImageClassifier_UnitTests() {
    float confidence = 0.f;

    DarkImageKind kind = PdfDarkModeClassifyImageFeatures(PhotoLikeFeatures(), 0.22f, false, &confidence);
    utassert(kind == DarkImageKind::Photo);
    utassert(confidence >= 0.6f);
    utassert(PdfDarkModePolicyForImageKind(kind, false) == DarkImagePolicy::Preserve);

    kind = PdfDarkModeClassifyImageFeatures(LightBackgroundArtFeatures(), 0.35f, false, &confidence);
    utassert(kind == DarkImageKind::LightBackgroundArtwork);
    utassert(PdfDarkModePolicyForImageKind(kind, false) == DarkImagePolicy::AdaptiveDocument);

    kind = PdfDarkModeClassifyImageFeatures(IconFeatures(), 0.02f, false, &confidence);
    utassert(kind == DarkImageKind::IconOrLineArt);
    utassert(PdfDarkModePolicyForImageKind(kind, false) == DarkImagePolicy::AdaptiveDocument);

    kind = PdfDarkModeClassifyImageFeatures(BrightFilmStillFeatures(), 0.08f, false, &confidence);
    utassert(kind == DarkImageKind::Photo);
    utassert(PdfDarkModePolicyForImageKind(kind, false) == DarkImagePolicy::Preserve);

    DarkImageFeatures scan = PhotoLikeFeatures();
    scan.highLuminanceRatio = 0.62f;
    kind = PdfDarkModeClassifyImageFeatures(scan, 0.88f, false, &confidence);
    utassert(kind == DarkImageKind::Photo);
    utassert(PdfDarkModePolicyForImageKind(kind, false) == DarkImagePolicy::Preserve);
    utassert(PdfDarkModeShouldPreserveImageFeatures(scan, 0.88f));

    DarkImageFeatures flatScan = LightBackgroundArtFeatures();
    flatScan.highLuminanceRatio = 0.62f;
    kind = PdfDarkModeClassifyImageFeatures(flatScan, 0.88f, false, &confidence);
    utassert(kind == DarkImageKind::FullPageScan);
    utassert(PdfDarkModePolicyForImageKind(kind, false) == DarkImagePolicy::AdaptiveDocument);

    kind = PdfDarkModeClassifyImageFeatures(scan, 0.58f, true, &confidence);
    utassert(kind == DarkImageKind::FullPageScan);
    utassert(confidence >= 0.7f);

    utassert(PdfDarkModePolicyForImageKind(DarkImageKind::Photo, true) == DarkImagePolicy::ThemeRecolor);

    float outR = 0.f, outG = 0.f, outB = 0.f;
    PdfDarkModeCompressPhotoHighlights(0.5f, 0.5f, 0.5f, &outR, &outG, &outB);
    utassert(outR == 0.5f && outG == 0.5f && outB == 0.5f);

    PdfDarkModeCompressPhotoHighlights(1.f, 1.f, 1.f, &outR, &outG, &outB);
    utassert(outR <= 0.91f && outG <= 0.91f && outB <= 0.91f);
    utassert(outR > 0.82f);

    utassert(str::Eq(PdfDarkModeKindDebugLabel(DarkImageKind::Photo), "Photo"));

    DarkImageAnalysis blendArt;
    blendArt.kind = DarkImageKind::LightBackgroundArtwork;
    blendArt.confidence = 0.78f;
    blendArt.features.borderUniformity = 0.72f;
    blendArt.features.borderLightRatio = 0.68f;
    blendArt.features.saturatedPixelRatio = 0.06f;
    blendArt.features.luminanceVariance = 0.009f;
    utassert(PdfDarkModeShouldBlendLightBackground(blendArt));

    DarkImageAnalysis photoArt = blendArt;
    photoArt.kind = DarkImageKind::Photo;
    utassert(!PdfDarkModeShouldBlendLightBackground(photoArt));

    DarkImageAnalysis lowConf = blendArt;
    lowConf.confidence = 0.40f;
    utassert(!PdfDarkModeShouldBlendLightBackground(lowConf));
}
