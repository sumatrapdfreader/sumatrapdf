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

    // this name is included in all saved PDF files
    static void SetProducerName(Str name);

    // creates a simple PDF with all pages rendered as a single image
    static bool RenderToFile(Str pdfFileName, EngineBase* engine, int dpi = 150);
};
