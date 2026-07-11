/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "PdfDarkMode.h"
#include "PdfDarkModeInternal.h"

// Stub implementations for binaries that compile EngineMupdf.cpp but not
// PdfDarkMode*.cpp / Theme.cpp (PdfFilter, PdfPreview, etc.).

bool PdfDarkModeUsesObjectLevel() {
    return false;
}

bool DarkModeProfileUsesObjectLevel(const DarkModeProfile* profile) {
    (void)profile;
    return false;
}

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

u32 PdfDarkModeComputeProfileHash(const DarkModeProfile* profile) {
    (void)profile;
    return 0;
}

u32 PdfDarkModeComputeOptionsHash() {
    return 0;
}

DarkModePalette PdfDarkModeBuildPalette() {
    return DarkModePalette{};
}

DarkModeEngineCache* PdfDarkModeEngineCacheCreate() {
    return nullptr;
}

void PdfDarkModeEngineCacheFree(fz_context* ctx, DarkModeEngineCache* cache) {
    (void)ctx;
    (void)cache;
}

void PdfDarkModeEngineCacheClear(fz_context* ctx, DarkModeEngineCache* cache) {
    (void)ctx;
    (void)cache;
}

DarkModePageAnalysis* PdfDarkModeGetOrBuildAnalysis(fz_context* ctx, FzPageInfo* pageInfo, fz_display_list* list,
                                                    u32 optionsHash, DarkModeEngineCache* engineCache) {
    (void)ctx;
    (void)pageInfo;
    (void)list;
    (void)optionsHash;
    (void)engineCache;
    return nullptr;
}

fz_device* PdfDarkModeWrapDevice(fz_context* ctx, fz_device* inner, DarkModePageAnalysis* analysis,
                                 const DarkModePalette* palette, DarkModeReplayState* replayState,
                                 DarkModeEngineCache* engineCache, u32 profileHash, bool debugOverlay) {
    (void)ctx;
    (void)analysis;
    (void)palette;
    (void)replayState;
    (void)engineCache;
    (void)profileHash;
    (void)debugOverlay;
    return inner;
}

void PdfDarkModeInvalidatePage(fz_context* ctx, FzPageInfo* pageInfo) {
    (void)ctx;
    (void)pageInfo;
}

int GetPreservePdfImagesMinSize() {
    return 72;
}

bool GetPreservePdfImagesInDarkMode() {
    return true;
}

void SetPreservePdfImagesInDarkMode(bool preserve) {
    (void)preserve;
}

PdfDarkModeRenderer GetPdfDarkModeRenderer() {
    return PdfDarkModeRenderer::LegacyBitmapPostProcess;
}

PdfDocumentColorMode GetPdfDocumentColorMode() {
    return PdfDocumentColorMode::Auto;
}

void SetPdfDocumentColorMode(PdfDocumentColorMode mode) {
    (void)mode;
}

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

RectF PdfDarkModeCapUnknownImagePageRect(const RectF& imgPage, float pageHeight) {
    (void)pageHeight;
    return imgPage;
}

bool PdfDarkModeShouldPreserveEmbeddedImageRect(fz_context* ctx, fz_image* image, float pageCoverage, int devW,
                                                int devH) {
    (void)ctx;
    (void)image;
    (void)pageCoverage;
    (void)devW;
    (void)devH;
    return false;
}

void PdfDarkModeClearPixmapToThemeBackground(fz_context* ctx, fz_pixmap* pix, const DarkModePalette& palette) {
    (void)ctx;
    (void)pix;
    (void)palette;
}
