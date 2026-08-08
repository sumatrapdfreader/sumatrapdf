/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct fz_context;
struct fz_image;
struct pdf_document;
enum class DocProp : u8;

class PdfCreator {
  public:
    fz_context* ctx = nullptr;
    pdf_document* doc = nullptr;

    PdfCreator();
    ~PdfCreator();

    bool AddPageFromFzImage(fz_image* image, float imgDpi = 0) const;
    bool AddPageFromGdiplusBitmap(Gdiplus::Bitmap* bmp, float imgDpi = 0);
    bool AddPageFromImageData(Str data, float imgDpi = 0) const;

    bool SetProperty(DocProp prop, Str value) const;
    bool CopyProperties(EngineBase* engine) const;

    bool SaveToFile(Str filePath) const;

    static void SetProducerName(Str name);

    static bool RenderToFile(Str pdfFileName, EngineBase* engine, int dpi = 150);
    // Called only when raw page bytes cannot be embedded as-is (MuPDF /
    // PDF cannot re-wrap the format). Return owned Str (caller frees) with
    // embeddable image data (e.g. optimized PNG), or empty to skip to render.
    using ImageDataFallbackFn = Str (*)(Str imageData);
    // Multi-page PDF from a comic / image-folder / multi-page image engine.
    // Prefers embedding original image bytes (JPEG/PNG); if that fails and
    // fallbackToEmbeddable is set, tries converted data (e.g. WebP/JXL/HEIC →
    // PNG); then renders. Skips pages that fail every path (issue #4118).
    static bool SaveImageCollectionAsPdf(Str pdfFileName, EngineBase* engine,
                                         ImageDataFallbackFn fallbackToEmbeddable = nullptr);
};
