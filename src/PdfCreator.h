/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

typedef struct fz_context_s fz_context;
typedef struct fz_image_s fz_image;
typedef struct pdf_document_s pdf_document;

class PdfCreator {
    fz_context *ctx;
    pdf_document *doc;

public:
    PdfCreator();
    ~PdfCreator();

    bool AddImagePage(fz_image *image, float imgDpi=0);
    bool AddImagePage(HBITMAP hbmp, SizeI size, float imgDpi=0);
    bool AddImagePage(Gdiplus::Bitmap *bmp, float imgDpi=0);
    // recommended for JPEG and JP2 images (don't need to be recompressed)
    bool AddImagePage(const char *data, size_t len, float imgDpi=0);

    bool SetProperty(DocumentProperty prop, const WCHAR *value);
    bool CopyProperties(BaseEngine *engine);

    bool SaveToFile(const WCHAR *filePath);

    // this name is included in all saved PDF files
    static void SetProducerName(const WCHAR *name);

    // creates a simple PDF with all pages rendered as a single image
    static bool RenderToFile(const WCHAR *pdfFileName, BaseEngine *engine, int dpi=150);
};
