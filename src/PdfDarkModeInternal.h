/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// include after mupdf headers and PdfDarkMode.h (headers are not self-sufficient)

struct DarkModeEngineCache;

DarkModeEngineCache* PdfDarkModeEngineCacheCreate();
void PdfDarkModeEngineCacheFree(fz_context* ctx, DarkModeEngineCache* cache);
void PdfDarkModeEngineCacheClear(fz_context* ctx, DarkModeEngineCache* cache);

bool PdfDarkModeEngineCacheLookupFeatures(DarkModeEngineCache* cache, fz_image* image, DarkImageFeatures* outFeatures,
                                          PixelColor* outBackground);
void PdfDarkModeEngineCacheStoreFeatures(fz_context* ctx, DarkModeEngineCache* cache, fz_image* image,
                                         const DarkImageFeatures& features, const PixelColor& background);

fz_image* PdfDarkModeEngineCacheLookupProcessed(fz_context* ctx, DarkModeEngineCache* cache, fz_image* src,
                                                u32 profileHash, DarkImagePolicy policy, DarkImageKind kind);
void PdfDarkModeEngineCacheStoreProcessed(fz_context* ctx, DarkModeEngineCache* cache, fz_image* src, u32 profileHash,
                                          DarkImagePolicy policy, DarkImageKind kind, fz_image* processed);

DarkModePageAnalysis* PdfDarkModeGetOrBuildAnalysis(fz_context* ctx, FzPageInfo* pageInfo, fz_display_list* list,
                                                    u32 optionsHash, DarkModeEngineCache* engineCache = nullptr);

DarkImageAnalysis PdfDarkModeAnalyzeImageCached(fz_context* ctx, fz_image* image, float pageCoverage,
                                                bool pageIsScannedHint, DarkModeEngineCache* engineCache);

fz_device* PdfDarkModeWrapDevice(fz_context* ctx, fz_device* inner, DarkModePageAnalysis* analysis,
                                 const DarkModePalette* palette, DarkModeReplayState* replayState,
                                 DarkModeEngineCache* engineCache, u32 profileHash, bool debugOverlay = false);

void MapColorToDarkTheme(fz_context* ctx, fz_colorspace* cs, const float* color, fz_color_params colorParams,
                         const DarkModePalette& palette, float* outRgb);

void MapFillColorToDarkTheme(fz_context* ctx, fz_colorspace* cs, const float* color, fz_color_params colorParams,
                             const DarkModePalette& palette, float* outRgb);

void MapRgbFillToDarkTheme(float r, float g, float b, const DarkModePalette& palette, float* outRgb);

void MapRgbToDarkTheme(float r, float g, float b, const DarkModePalette& palette, float* outRgb);

void PdfDarkModeRecordShadeForward();
int PdfDarkModeTakeShadeForwardCount();

void ApplyPreserveImagePaperSoftening(float r, float g, float b, const DarkModePalette& palette, float strength,
                                      float* outR, float* outG, float* outB);

// Margin strips, drop shadows, and similar layout art — not photos to preserve.
bool PdfDarkModeIsDecorativeStripImage(const RectF& imgRect, const RectF& pageBounds);

void PdfDarkModeFreeProcessCache(fz_context* ctx, DarkModePageAnalysis* analysis);

// Returns a kept fz_image for fill_image, or nullptr to use the source image.
fz_image* PdfDarkModeGetCachedImage(fz_context* ctx, DarkModeEngineCache* engineCache, DarkModePageAnalysis* analysis,
                                    int occurrenceIndex, fz_image* srcImage, DarkImagePolicy policy,
                                    const DarkModePalette& palette, u32 profileHash);

// Phase 4: returns kept fz_image with alpha, or nullptr to fall back to per-pixel adaptive recolor.
fz_pixmap* PdfDarkModeProcessLightBackgroundPixmap(fz_context* ctx, fz_pixmap* src, const DarkImageAnalysis& analysis,
                                                   const DarkModePalette& palette);

// Phase 5: returns processed pixmap for FullPageScan, or nullptr to fall back.
fz_pixmap* PdfDarkModeProcessScanPixmap(fz_context* ctx, fz_pixmap* src, const DarkImageAnalysis& analysis,
                                        const DarkModePalette& palette);

// Returns a kept fz_image covering bounds, or nullptr to fall back to direct shade fill.
fz_image* PdfDarkModeGetCachedShade(fz_context* ctx, DarkModePageAnalysis* analysis, fz_shade* shade, fz_matrix ctm,
                                    float alpha, fz_irect bounds, const DarkModePalette& palette);

void PdfDarkModeClearPixmapToThemeBackground(fz_context* ctx, fz_pixmap* pix, const DarkModePalette& palette);
