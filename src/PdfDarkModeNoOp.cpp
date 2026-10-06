/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "PdfDarkMode.h"

// Stub implementations for binaries that compile EngineMupdf.cpp but not
// PdfDarkMode*.cpp / Theme.cpp (PdfFilter, PdfPreview, etc.).

bool DarkModeProfileUsesLegacyPostProcess(const DarkModeProfile* profile) {
    (void)profile;
    return false;
}

void BuildViewDarkModeProfile(EngineBase* engine, DarkModeProfile* profile) {
    (void)engine;
    if (profile) {
        *profile = DarkModeProfile{};
    }
}

u32 PdfDarkModeComputeOptionsHash() {
    return 0;
}

// PDF dark mode runtime options (not stored in settings file)
bool GetPreservePdfImagesInDarkMode() {
    return true;
}

void SetPreservePdfImagesInDarkMode(bool preserve) {
    (void)preserve;
}

bool EngineUsesDocumentColorsFollowTheme(EngineBase* engine) {
    (void)engine;
    return false;
}

bool EngineUsesReflowThemeCss(EngineBase* engine) {
    (void)engine;
    return false;
}

TempStr ReflowDocumentThemeCssTemp() {
    return {};
}

bool DocumentColorsFollowThemeEnabled() {
    return false;
}

DocumentColorsFollowTheme GetDocumentColorsFollowTheme() {
    return DocumentColorsFollowTheme::Off;
}

DocumentColorsFollowTheme DocumentColorsFollowThemeFromString(Str v) {
    (void)v;
    return DocumentColorsFollowTheme::Off;
}

void SetDocumentColorsFollowTheme(DocumentColorsFollowTheme mode) {
    (void)mode;
}

void SetDocumentColorsFollowThemePreview(DocumentColorsFollowTheme mode) {
    (void)mode;
}

void ClearDocumentColorsFollowThemePreview() {}

bool PdfDarkModeIsDecorativeStripImage(const RectF& imgRect, const RectF& pageBounds) {
    (void)imgRect;
    (void)pageBounds;
    return false;
}

bool PdfDarkModeImageLooksLikeDarkArtwork(fz_context* ctx, fz_image* image, float pageCoverage) {
    (void)ctx;
    (void)image;
    (void)pageCoverage;
    return false;
}

RectF PdfDarkModeClampImagePageRect(const RectF& imgPage, int imageW, int imageH) {
    (void)imageW;
    (void)imageH;
    return imgPage;
}

// Cap bbox when embedded image dimensions are unknown (common with content-stream tiles).
RectF PdfDarkModeCapUnknownImagePageRect(const RectF& imgPage, float pageHeight) {
    (void)pageHeight;
    return imgPage;
}

bool PdfDarkModePageDominantImageRecolors(fz_context* ctx, fz_image* image, float pageCoverage) {
    (void)ctx;
    (void)image;
    (void)pageCoverage;
    return false;
}

// Gate for Legacy skip-rect preserve: combines bbox size, pixel stats, and artwork heuristics.
bool PdfDarkModeShouldPreserveEmbeddedImageRect(fz_context* ctx, fz_image* image, float pageCoverage, int devW,
                                                int devH) {
    (void)ctx;
    (void)image;
    (void)pageCoverage;
    (void)devW;
    (void)devH;
    return false;
}
