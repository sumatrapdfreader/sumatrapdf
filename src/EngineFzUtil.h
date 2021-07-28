/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Common for EngineMupdf.cpp and EngineXps.cpp

// maximum amount of memory that MuPDF should use per fz_context store
#define MAX_CONTEXT_MEMORY (256 * 1024 * 1024)
// number of page content trees to cache for quicker rendering
#define MAX_PAGE_RUN_CACHE 8
// maximum estimated memory requirement allowed for the run cache of one document
#define MAX_PAGE_RUN_MEMORY (40 * 1024 * 1024)

class FitzAbortCookie : public AbortCookie {
  public:
    fz_cookie cookie;
    FitzAbortCookie() {
        memset(&cookie, 0, sizeof(cookie));
    }
    void Abort() override {
        cookie.abort = 1;
    }
};

struct FitzPageImageInfo {
    fz_rect rect = fz_unit_rect;
    fz_matrix transform;
    IPageElement* imageElement{nullptr};
};

struct FzPageInfo {
    int pageNo{0}; // 1-based
    fz_page* page{nullptr};

    fz_link* links{nullptr};

    // auto-detected links
    Vec<IPageElement*> autoLinks;
    // comments are made out of annotations
    Vec<IPageElement*> comments;

    RectF mediabox{};
    Vec<FitzPageImageInfo> images;

    // if false, only loaded page (fast)
    // if true, loaded expensive info (extracted text etc.)
    bool fullyLoaded{false};

    bool commentsNeedRebuilding{false};
};

struct LinkRectList {
    WStrVec links;
    Vec<fz_rect> coords;
};

fz_rect To_fz_rect(RectF rect);
RectF ToRectFl(fz_rect rect);
fz_matrix FzCreateViewCtm(fz_rect mediabox, float zoom, int rotation);

bool IsPointInRect(fz_rect rect, fz_point pt);
float FzRectOverlap(fz_rect r1, fz_rect r2);

WCHAR* PdfToWstr(fz_context* ctx, pdf_obj* obj);
WCHAR* PdfCleanString(WCHAR* string);

fz_stream* FzOpenIStream(fz_context* ctx, IStream* stream);
fz_stream* FzOpenFile2(fz_context* ctx, const WCHAR* filePath);
void FzStreamFingerprint(fz_context* ctx, fz_stream* stm, u8 digest[16]);
std::span<u8> FzExtractStreamData(fz_context* ctx, fz_stream* stream);

RenderedBitmap* NewRenderedFzPixmap(fz_context* ctx, fz_pixmap* pixmap);

WCHAR* FzTextPageToStr(fz_stext_page* text, Rect** coordsOut);

LinkRectList* LinkifyText(const WCHAR* pageText, Rect* coords);
int IsExternalLink(const char* uri);
TocItem* NewTocItemWithDestination(TocItem* parent, WCHAR* title, IPageDestination* dest);
IPageDestination* NewPageDestinationMupdf(fz_link*, fz_outline*);
IPageElement* FzGetElementAtPos(FzPageInfo* pageInfo, PointF pt);
void FzGetElements(Vec<IPageElement*>* els, FzPageInfo* pageInfo);
void FzLinkifyPageText(FzPageInfo* pageInfo, fz_stext_page* stext);
fz_pixmap* FzConvertPixmap2(fz_context* ctx, fz_pixmap* pix, fz_colorspace* ds, fz_colorspace* prf,
                            fz_default_colorspaces* default_cs, fz_color_params color_params, int keep_alpha);
fz_image* FzFindImageAtIdx(fz_context* ctx, FzPageInfo* pageInfo, int idx);
void FzFindImagePositions(fz_context* ctx, int pageNo, Vec<FitzPageImageInfo>& images, fz_stext_page* stext);

// float is in range 0...1
COLORREF ColorRefFromPdfFloat(fz_context* ctx, int n, float color[4]);

struct PageDestinationMupdf : IPageDestination {
    // TODO: should be private to EngineMupdf
    fz_outline* outline{nullptr};
    fz_link* link{nullptr};
    const char* uri{nullptr};

    PageDestinationMupdf(fz_link* l, fz_outline* o) {
        kind = kindDestinationMupdf;
        link = l;
        outline = o;
    }
    ~PageDestinationMupdf() override {
    }

    WCHAR* GetValue() override {
        return nullptr;
    }
    WCHAR* GetName() override {
        return nullptr;
    }
    IPageDestination* Clone() override {
        // TODO: remove or implement
        return nullptr;
    }
};
