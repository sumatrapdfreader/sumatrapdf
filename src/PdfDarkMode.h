/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct FzPageInfo;
struct fz_context;
struct fz_image;

enum class DarkImagePolicy {
    Preserve,
    AdaptiveDocument,
    ThemeRecolor,
};

enum class DarkImageKind {
    Photo,
    LightBackgroundArtwork,
    IconOrLineArt,
    FullPageScan,
    Unknown,
};

struct PixelColor {
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
};

struct DarkImageFeatures {
    bool isColorful = false;
    float colorBucketRatio = 0.f;
    float transparentRatio = 0.f;
    float highLuminanceRatio = 0.f;
    float saturatedPixelRatio = 0.f;
    float chromaticPixelRatio = 0.f;
    float borderUniformity = 0.f;
    float borderLightRatio = 0.f;
    float flatAreaRatio = 0.f;
    float textureScore = 0.f;
    float luminanceVariance = 0.f;
    float pageCoverage = 0.f;
};

struct DarkImageAnalysis {
    DarkImageKind kind = DarkImageKind::Unknown;
    float confidence = 0.f;
    PixelColor estimatedBackground{};
    DarkImageFeatures features{};
};

enum class PdfDarkModeRenderer {
    LegacyBitmapPostProcess = 0,
    ObjectLevelDevice = 1,
};

enum class DocumentColorsFollowTheme {
    Off = 0,
    Smart = 1,
    Legacy = 2,
};

// Per-render dark mode path (View target only for Smart/Legacy PDF paths).
enum class PageColorMode {
    Normal,
    LegacyInvert,
    SmartDark,
    PreserveImages,
    ScanDark,
};

struct DarkModeOptions {
    float scanImageCoverageThreshold = 0.75f;
    float minScanDominantCoverage = 0.85f;
    float maxScanAspectSkew = 1.15f;
    int maxTextOpsForScanPage = 10;
    int maxVectorOpsForScanPage = 20;
    // 0=off, 1=blend near-white Preserve-image pixels toward page background
    float preserveImagePaperSoftening = 0.f;
    float lightFillChromaThreshold = 0.05f;
    float lightFillLuminanceThreshold = 0.45f;
};

// Full-bleed backgrounds / scans at or above this threshold are recolored with the page.
static constexpr float kMaxPreserveImagePageCoverage = 0.75f;

struct DarkModePalette {
    float textR = 0.f, textG = 0.f, textB = 0.f;
    float bgR = 1.f, bgG = 1.f, bgB = 1.f;
    float linkR = 0.f, linkG = 0.f, linkB = 0.f;
    float diffR = 1.f, diffG = 1.f, diffB = 1.f;
};

struct DarkModeProfile {
    PageColorMode mode = PageColorMode::Normal;
    COLORREF foreground = 0;
    COLORREF pageBackground = 0;
    COLORREF linkColor = 0;
    float strength = 1.f;
    bool debugOverlay = false;
    bool preservePdfImages = false;
    int preservePdfImagesMinSize = 72;
    DarkModePalette palette{};
    DarkModeOptions options{};
    u32 hash = 0;
};

struct ImageOccurrenceInfo {
    int occurrenceIndex = 0;
    RectF pageBounds{};
    bool isImageMask = false;
    bool hasAlpha = false;
    float pageCoverage = 0.f;
    bool looksLikePhoto = true;
    DarkImagePolicy policy = DarkImagePolicy::Preserve;
    DarkImageAnalysis analysis{};
};

struct DarkModePageAnalysis {
    int pageNumber = 0;
    RectF pageBounds{};
    bool isScannedPage = false;
    Vec<ImageOccurrenceInfo> images;
    u32 optionsHash = 0;
    void* processCache = nullptr;
};

struct DarkModeReplayState {
    int nextImageOccurrence = 0;
};

// PDF dark mode runtime options (not stored in settings file)
bool GetPreservePdfImagesInDarkMode();
void SetPreservePdfImagesInDarkMode(bool preserve);
int GetPreservePdfImagesMinSize();
PdfDarkModeRenderer GetPdfDarkModeRenderer();

bool PdfDarkModeUsesObjectLevel();
bool DarkModeProfileUsesObjectLevel(const DarkModeProfile* profile);
bool DarkModeProfileUsesLegacyPostProcess(const DarkModeProfile* profile);
void BuildViewDarkModeProfile(EngineBase* engine, DarkModeProfile* profile);
u32 PdfDarkModeComputeProfileHash(const DarkModeProfile* profile);
bool EngineUsesDocumentColorsFollowTheme(EngineBase* engine);
bool DocumentColorsFollowThemeEnabled();
DocumentColorsFollowTheme GetDocumentColorsFollowTheme();
void SetDocumentColorsFollowTheme(DocumentColorsFollowTheme mode);
const char* DocumentColorsFollowThemeDescription(DocumentColorsFollowTheme mode);
DarkModeOptions PdfDarkModeCurrentOptions();
u32 PdfDarkModeComputeOptionsHash();
DarkModePalette PdfDarkModeBuildPalette();

void PdfDarkModeFreeAnalysis(fz_context* ctx, DarkModePageAnalysis* analysis);
void PdfDarkModeInvalidatePage(fz_context* ctx, FzPageInfo* pageInfo);

void ApplyAdaptiveDocumentDarkMode(float r, float g, float b, const DarkModePalette& palette, float* outR, float* outG,
                                   float* outB);

bool PdfDarkModeIsDecorativeStripImage(const RectF& imgRect, const RectF& pageBounds);

// OKLab perceptual remap for SmartDark text/vector colors (Phase 2).
void MapRgbToDarkThemeOklab(float r, float g, float b, const DarkModePalette& palette, float* outRgb);

// Perceptual distance in OKLab (Phase 4 background matching).
float PdfDarkModeOklabDistance(float r1, float g1, float b1, float r2, float g2, float b2);

// Phase 4: edge-connected light background removal for LightBackgroundArtwork.
bool PdfDarkModeShouldBlendLightBackground(const DarkImageAnalysis& analysis);

// Phase 5: full-page scan remapping (Smart path only).
void PdfDarkModeRemapScanPixel(float r, float g, float b, const DarkImageAnalysis& analysis,
                               const DarkModePalette& palette, float* outR, float* outG, float* outB);

bool PdfDarkModeImageLooksLikePhoto(fz_context* ctx, fz_image* image);
bool PdfDarkModeImageLooksLikeDarkArtwork(fz_context* ctx, fz_image* image, float pageCoverage);

RectF PdfDarkModeClampImagePageRect(const RectF& imgPage, int imageW, int imageH);

// Cap bbox when embedded image dimensions are unknown (common with content-stream tiles).
RectF PdfDarkModeCapUnknownImagePageRect(const RectF& imgPage, float pageHeight);

// Gate for Legacy skip-rect preserve: combines bbox size, pixel stats, and artwork heuristics.
bool PdfDarkModeShouldPreserveEmbeddedImageRect(fz_context* ctx, fz_image* image, float pageCoverage, int devW,
                                                int devH);

// Stricter pixel gate used by PdfDarkModeShouldPreserveEmbeddedImageRect.
bool PdfDarkModeImageShouldPreserveInLegacy(fz_context* ctx, fz_image* image, float pageCoverage = 0.f, int devW = 0,
                                            int devH = 0);

bool PdfDarkModeImageIsConfirmedArtwork(fz_context* ctx, fz_image* image, float pageCoverage, int devW, int devH);

// Phase 3: fz_image pixel analysis (page-independent; safe for tile-free classification).
DarkImageAnalysis PdfDarkModeAnalyzeImage(fz_context* ctx, fz_image* image, float pageCoverage,
                                          bool pageIsScannedHint = false);

DarkImageKind PdfDarkModeClassifyImageFeatures(const DarkImageFeatures& features, float pageCoverage,
                                               bool pageIsScannedHint, float* outConfidence);

bool PdfDarkModeFeaturesLookLikePhoto(const DarkImageFeatures& f);
bool PdfDarkModeShouldPreserveImageFeatures(const DarkImageFeatures& f, float pageCoverage);

DarkImagePolicy PdfDarkModePolicyForImageKind(DarkImageKind kind, bool isImageMask);

void PdfDarkModeCompressPhotoHighlights(float r, float g, float b, float* outR, float* outG, float* outB);

const char* PdfDarkModeKindDebugLabel(DarkImageKind kind);
