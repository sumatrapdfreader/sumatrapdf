/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PdfCreator_h
#define PdfCreator_h

typedef struct fz_context_s fz_context;
typedef struct fz_image_s fz_image;
typedef struct pdf_document_s pdf_document;
class BaseEngine;
enum DocumentProperty;

class PdfCreator {
    fz_context *ctx;
    pdf_document *doc;
    int dpi;

public:
    PdfCreator(int dpi);
    ~PdfCreator();

    bool AddImagePage(fz_image *image, float imgDpi=0);
    bool AddImagePage(HBITMAP hbmp, SizeI size, float imgDpi=0);
    bool AddImagePage(Gdiplus::Bitmap *bmp, float imgDpi=0);
    // recommended for JPEG and JP2 images (don't need to be recompressed)
    bool AddImagePage(const char *data, size_t len, float imgDpi=0);

    bool AddRenderedPage(BaseEngine *engine, int pageNo);
    bool SetProperty(DocumentProperty prop, const WCHAR *value);
    bool SaveToFile(const WCHAR *filePath);

    static PdfCreator *Create(int dpi);
};

// creates a simple PDF with all pages rendered as a single image
bool RenderToPDF(const WCHAR *pdfFileName, BaseEngine *engine, int dpi=150);

#endif
