/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class CadEnhanceReason {
    None,
    Pdfe,
    Metadata,
    Heuristic,
    RasterImage,
};

enum class CadEnhanceOverride {
    Unset = 0,
    ForceOff = 1,
    ForceOn = 2,
};

enum class EngineeringDrawingEnhanceMode {
    Off,
    Auto,
    On,
};

struct CadDetectResult {
    bool enable = false;
    CadEnhanceReason reason = CadEnhanceReason::None;
    int score = 0;
    // true when pages are dominated by a single embedded image (screenshot CAD)
    bool rasterDominant = false;
    // true for WPS-style exports: dense hairline vector strokes, tiny text matrix
    bool hairlineVector = false;
};

struct fz_context;
struct pdf_document;
struct fz_device;
struct fz_pixmap;

void SetEngineeringDrawingEnhanceMode(Str mode);
EngineeringDrawingEnhanceMode GetEngineeringDrawingEnhanceMode();
CadDetectResult DetectCadPdf(fz_context* ctx, pdf_document* doc);
bool CadEnhanceEnabledForEngine(const CadDetectResult& detect, CadEnhanceOverride overrideState);
const char* CadEnhanceReasonName(CadEnhanceReason reason);

struct CadMinLineWidthScope {
    CadMinLineWidthScope(fz_context* ctx, float zoom, bool active, bool hairlineDoc = false);
    ~CadMinLineWidthScope();

    CadMinLineWidthScope(const CadMinLineWidthScope&) = delete;
    CadMinLineWidthScope& operator=(const CadMinLineWidthScope&) = delete;

  private:
    fz_context* ctx = nullptr;
    float saved = 0;
    bool active = false;
};

fz_device* PdfCadEnhanceWrapDevice(fz_context* ctx, fz_device* inner);
void PdfCadEnhancePixmap(fz_context* ctx, fz_pixmap* pix, float zoom, bool rasterDominant);
