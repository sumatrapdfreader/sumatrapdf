/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct fz_context;
struct fz_image;

enum class DocumentColorsFollowTheme {
    Off = 0,
    Smart = 1,
    Legacy = 2,
};

// Per-render dark mode path (View target only for Smart/Legacy PDF paths).
enum class PageColorMode {
    Normal,
    LegacyInvert,
    PreserveImages,
};

// Full-bleed backgrounds / scans at or above this threshold are recolored with the page.
static constexpr float kMaxPreserveImagePageCoverage = 0.75f;

struct DarkModeProfile {
    PageColorMode mode = PageColorMode::Normal;
    Color foreground = 0;
    Color pageBackground = 0;
    Color linkColor = 0;
    bool preservePdfImages = false;
    u32 hash = 0;
};

bool GetPreservePdfImagesInDarkMode();
void SetPreservePdfImagesInDarkMode(bool preserve);
static constexpr int kPreservePdfImagesMinSize = 72;

bool DarkModeProfileUsesLegacyPostProcess(const DarkModeProfile* profile);
void BuildViewDarkModeProfile(EngineBase* engine, DarkModeProfile* profile);
bool EngineUsesDocumentColorsFollowTheme(EngineBase* engine);
bool EngineUsesReflowThemeCss(EngineBase* engine);
TempStr ReflowDocumentThemeCssTemp();
bool DocumentColorsFollowThemeEnabled();
DocumentColorsFollowTheme GetDocumentColorsFollowTheme();
DocumentColorsFollowTheme DocumentColorsFollowThemeFromString(Str v);
void SetDocumentColorsFollowTheme(DocumentColorsFollowTheme mode);
void SetDocumentColorsFollowThemePreview(DocumentColorsFollowTheme mode);
void ClearDocumentColorsFollowThemePreview();
u32 PdfDarkModeComputeOptionsHash();

bool PdfDarkModeIsDecorativeStripImage(const RectF& imgRect, const RectF& pageBounds);

bool PdfDarkModeImageLooksLikeDarkArtwork(fz_context* ctx, fz_image* image, float pageCoverage);
bool PdfDarkModePageDominantImageRecolors(fz_context* ctx, fz_image* image, float pageCoverage);

RectF PdfDarkModeClampImagePageRect(const RectF& imgPage, int imageW, int imageH);

RectF PdfDarkModeCapUnknownImagePageRect(const RectF& imgPage, float pageHeight);

bool PdfDarkModeShouldPreserveEmbeddedImageRect(fz_context* ctx, fz_image* image, float pageCoverage, int devW,
                                                int devH);
