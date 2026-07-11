/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PdfDarkMode.h"

static int PdfDarkModeFeatureColorBuckets(const DarkImageFeatures& f) {
    return (int)(f.colorBucketRatio * 4096.f + 0.5f);
}

// Mirrors PdfDarkModeStatsLookLikePhoto in PdfDarkModeImageStats.cpp.
bool PdfDarkModeFeaturesLookLikePhoto(const DarkImageFeatures& f) {
    int buckets = PdfDarkModeFeatureColorBuckets(f);
    bool isPhoto = buckets >= 16 || f.saturatedPixelRatio >= 0.18f || f.luminanceVariance >= 0.014f;
    if (f.highLuminanceRatio > 0.58f && f.saturatedPixelRatio < 0.18f) {
        isPhoto = false;
    }
    if (buckets <= 12 && f.luminanceVariance < 0.012f && f.highLuminanceRatio > 0.45f) {
        isPhoto = false;
    }
    if (f.highLuminanceRatio > 0.72f && f.saturatedPixelRatio < 0.18f) {
        isPhoto = false;
    }
    return isPhoto;
}

static bool PdfDarkModeFeaturesLookLikeFlatLayoutPanel(const DarkImageFeatures& f) {
    int buckets = PdfDarkModeFeatureColorBuckets(f);
    return f.highLuminanceRatio > 0.76f && f.luminanceVariance < 0.011f && buckets <= 11 &&
           f.saturatedPixelRatio < 0.17f;
}

// Mirrors PdfDarkModeStatsLookLikeLayoutBackground in PdfDarkModeImageStats.cpp.
static bool PdfDarkModeFeaturesLookLikeLayoutBackground(const DarkImageFeatures& f, float pageCoverage) {
    int buckets = PdfDarkModeFeatureColorBuckets(f);
    if (PdfDarkModeFeaturesLookLikeFlatLayoutPanel(f)) {
        return pageCoverage >= 0.04f;
    }
    if (f.highLuminanceRatio > 0.58f && f.luminanceVariance < 0.018f && f.saturatedPixelRatio < 0.12f) {
        return pageCoverage >= 0.04f;
    }
    if (f.highLuminanceRatio > 0.44f && f.luminanceVariance < 0.022f && f.saturatedPixelRatio < 0.12f) {
        return pageCoverage >= 0.06f;
    }
    if (f.highLuminanceRatio > 0.50f && f.luminanceVariance < 0.038f && f.saturatedPixelRatio < 0.22f &&
        buckets <= 14) {
        return pageCoverage >= 0.04f;
    }
    return false;
}

static bool PdfDarkModeFeaturesLookLikePaperTextBox(const DarkImageFeatures& f) {
    int buckets = PdfDarkModeFeatureColorBuckets(f);
    return f.highLuminanceRatio > 0.64f && f.luminanceVariance < 0.014f && buckets <= 12 &&
           f.saturatedPixelRatio < 0.20f;
}

DarkImageKind PdfDarkModeClassifyImageFeatures(const DarkImageFeatures& f, float pageCoverage, bool pageIsScannedHint,
                                               float* outConfidence) {
    float confidence = 0.4f;
    DarkImageKind kind = DarkImageKind::Unknown;

    if (pageCoverage >= kMaxPreserveImagePageCoverage && f.highLuminanceRatio > 0.45f) {
        if (PdfDarkModeFeaturesLookLikePhoto(f)) {
            kind = DarkImageKind::Photo;
            confidence = 0.76f;
        } else {
            kind = DarkImageKind::FullPageScan;
            confidence = 0.82f;
        }
    } else if (pageIsScannedHint && pageCoverage >= 0.55f) {
        kind = DarkImageKind::FullPageScan;
        confidence = 0.72f;
    } else if (PdfDarkModeFeaturesLookLikeLayoutBackground(f, pageCoverage)) {
        kind = DarkImageKind::LightBackgroundArtwork;
        confidence = 0.80f;
    } else if (PdfDarkModeFeaturesLookLikePhoto(f)) {
        if (pageCoverage < 0.14f && PdfDarkModeFeaturesLookLikePaperTextBox(f)) {
            kind = DarkImageKind::IconOrLineArt;
            confidence = 0.66f;
        } else {
            kind = DarkImageKind::Photo;
            confidence = 0.78f;
        }
    } else if (f.borderLightRatio > 0.62f && f.borderUniformity > 0.62f && f.highLuminanceRatio > 0.44f &&
               f.flatAreaRatio > 0.35f && f.saturatedPixelRatio < 0.12f) {
        kind = DarkImageKind::LightBackgroundArtwork;
        confidence = 0.76f;
    } else if (f.highLuminanceRatio > 0.52f && f.luminanceVariance < 0.014f && f.colorBucketRatio < 0.025f &&
               f.saturatedPixelRatio < 0.10f) {
        kind = DarkImageKind::IconOrLineArt;
        confidence = 0.68f;
    } else if (pageCoverage < 0.05f && f.colorBucketRatio < 0.018f) {
        kind = DarkImageKind::IconOrLineArt;
        confidence = 0.60f;
    } else {
        kind = DarkImageKind::Unknown;
        confidence = 0.42f;
    }

    if (outConfidence) {
        *outConfidence = confidence;
    }
    return kind;
}

DarkImagePolicy PdfDarkModePolicyForImageKind(DarkImageKind kind, bool isImageMask) {
    if (isImageMask) {
        return DarkImagePolicy::ThemeRecolor;
    }
    switch (kind) {
        case DarkImageKind::Photo:
        case DarkImageKind::Unknown:
            return DarkImagePolicy::Preserve;
        case DarkImageKind::FullPageScan:
        case DarkImageKind::LightBackgroundArtwork:
        case DarkImageKind::IconOrLineArt:
            return DarkImagePolicy::AdaptiveDocument;
    }
    return DarkImagePolicy::AdaptiveDocument;
}

bool PdfDarkModeShouldPreserveImageFeatures(const DarkImageFeatures& f, float pageCoverage) {
    if (PdfDarkModeFeaturesLookLikeFlatLayoutPanel(f)) {
        return false;
    }
    if (PdfDarkModeFeaturesLookLikeLayoutBackground(f, pageCoverage)) {
        return false;
    }
    if (PdfDarkModeFeaturesLookLikePhoto(f)) {
        if (pageCoverage < 0.14f && PdfDarkModeFeaturesLookLikePaperTextBox(f)) {
            return false;
        }
        return true;
    }
    return false;
}

void PdfDarkModeCompressPhotoHighlights(float r, float g, float b, float* outR, float* outG, float* outB) {
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    const float knee = 0.82f;
    const float cap = 0.90f;
    if (lum <= knee) {
        *outR = r;
        *outG = g;
        *outB = b;
        return;
    }
    float t = (lum - knee) / (1.f - knee);
    if (t > 1.f) {
        t = 1.f;
    }
    float targetLum = knee + (cap - knee) * t;
    float scale = lum > 0.0001f ? targetLum / lum : 1.f;
    *outR = r * scale;
    *outG = g * scale;
    *outB = b * scale;
}

const char* PdfDarkModeKindDebugLabel(DarkImageKind kind) {
    switch (kind) {
        case DarkImageKind::Photo:
            return "Photo";
        case DarkImageKind::LightBackgroundArtwork:
            return "LightBg";
        case DarkImageKind::IconOrLineArt:
            return "Icon";
        case DarkImageKind::FullPageScan:
            return "Scan";
        case DarkImageKind::Unknown:
        default:
            return "Unknown";
    }
}

bool PdfDarkModeShouldBlendLightBackground(const DarkImageAnalysis& analysis) {
    if (analysis.kind != DarkImageKind::LightBackgroundArtwork) {
        return false;
    }
    if (analysis.confidence < 0.65f) {
        return false;
    }
    const DarkImageFeatures& f = analysis.features;
    if (f.borderUniformity < 0.55f || f.borderLightRatio < 0.50f) {
        return false;
    }
    if (f.saturatedPixelRatio >= 0.22f && f.luminanceVariance >= 0.020f) {
        return false;
    }
    return true;
}
