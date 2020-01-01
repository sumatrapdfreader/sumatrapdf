/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

typedef struct fz_context_s fz_context;
typedef struct fz_image_s fz_image;
typedef struct pdf_document_s pdf_document;

class PdfCreator {
  public:
    fz_context* ctx = nullptr;
    pdf_document* doc = nullptr;

    PdfCreator();
    ~PdfCreator();

    bool AddPageFromFzImage(fz_image* image, float imgDpi = 0);
    bool AddPageFromGdiplusBitmap(Gdiplus::Bitmap* bmp, float imgDpi = 0);
    bool AddPageFromImageData(const char* data, size_t len, float imgDpi = 0);

    bool SetProperty(DocumentProperty prop, const WCHAR* value);
    bool CopyProperties(EngineBase* engine);

    bool SaveToFile(const char* filePath);

    // this name is included in all saved PDF files
    static void SetProducerName(const WCHAR* name);

    // creates a simple PDF with all pages rendered as a single image
    static bool RenderToFile(const char* pdfFileName, EngineBase* engine, int dpi = 150);
};
