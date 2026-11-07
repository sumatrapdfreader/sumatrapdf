/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct fz_context;
struct Pixmap;
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
#if OS_WIN
    bool AddPageFromGdiplusBitmap(Gdiplus::Bitmap* bmp, float imgDpi = 0);
#endif
    bool AddPageFromImageData(Str data, float imgDpi = 0) const;
    // the pixels go in as they are: encoding them first (e.g. as PNG) is wasted,
    // mupdf would decode it again
    bool AddPageFromPixmap(Pixmap* px, float imgDpi = 0);

    bool SetProperty(DocProp prop, Str value) const;
    bool CopyProperties(EngineBase* engine) const;

    bool SaveToFile(Str filePath) const;

    static void SetProducerName(Str name);

    static bool SaveImageCollectionAsPdf(Str pdfFileName, EngineBase* engine);
};
