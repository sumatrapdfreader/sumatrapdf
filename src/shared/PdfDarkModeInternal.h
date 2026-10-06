/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// include after mupdf headers and PdfDarkMode.h (headers are not self-sufficient)

static constexpr int kImageBorderSamples = 256;
static constexpr float kImageMinAlpha = 0.08f;

void PdfDarkModeSampleRgb(fz_context* ctx, fz_pixmap* pix, int x, int y, float* r, float* g, float* b,
                          float* alpha = nullptr);
void PdfDarkModeSampleBorder(fz_context* ctx, fz_pixmap* pix, int maxSamples, float minAlpha, float* lightRatio,
                             float* uniformity, PixelColor* background = nullptr);

void MapColorToDarkTheme(fz_context* ctx, fz_colorspace* cs, const float* color, fz_color_params colorParams,
                         const DarkModePalette& palette, float* outRgb);

void MapFillColorToDarkTheme(fz_context* ctx, fz_colorspace* cs, const float* color, fz_color_params colorParams,
                             const DarkModePalette& palette, float* outRgb);

void MapRgbFillToDarkTheme(float r, float g, float b, const DarkModePalette& palette, float* outRgb);

void MapRgbToDarkTheme(float r, float g, float b, const DarkModePalette& palette, float* outRgb);

void ApplyPreserveImagePaperSoftening(float r, float g, float b, const DarkModePalette& palette, float strength,
                                      float* outR, float* outG, float* outB);
