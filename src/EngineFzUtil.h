/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Common for EnginePdf.cpp and EngineXps.cpp

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

struct FitzImagePos {
    fz_rect rect = fz_unit_rect;
    fz_matrix transform;
};

struct FzPageInfo {
    int pageNo = 0; // 1-based
    fz_page* page = nullptr;

    fz_link* links = nullptr;

    // auto-detected links
    Vec<IPageElement*> autoLinks;
    // comments are made out of annotations
    Vec<IPageElement*> comments;

    RectF mediabox = {};
    Vec<FitzImagePos> images;

    // if false, only loaded page (fast)
    // if true, loaded expensive info (extracted text etc.)
    bool fullyLoaded = false;
};

struct LinkRectList {
    WStrVec links;
    Vec<fz_rect> coords;
};

fz_rect To_fz_rect(RectF rect);
RectF ToRectFl(fz_rect rect);
fz_matrix fz_create_view_ctm(fz_rect mediabox, float zoom, int rotation);

bool fz_is_pt_in_rect(fz_rect rect, fz_point pt);
float fz_calc_overlap(fz_rect r1, fz_rect r2);

WCHAR* pdf_to_wstr(fz_context* ctx, pdf_obj* obj);
WCHAR* pdf_clean_string(WCHAR* string);

fz_stream* fz_open_istream(fz_context* ctx, IStream* stream);
fz_stream* fz_open_file2(fz_context* ctx, const WCHAR* filePath);
void fz_stream_fingerprint(fz_context* ctx, fz_stream* stm, u8 digest[16]);
std::span<u8> fz_extract_stream_data(fz_context* ctx, fz_stream* stream);

RenderedBitmap* new_rendered_fz_pixmap(fz_context* ctx, fz_pixmap* pixmap);

WCHAR* fz_text_page_to_str(fz_stext_page* text, Rect** coordsOut);

LinkRectList* LinkifyText(const WCHAR* pageText, Rect* coords);
int is_external_link(const char* uri);
int resolve_link(const char* uri, float* xp, float* yp);
TocItem* newTocItemWithDestination(TocItem* parent, WCHAR* title, PageDestination* dest);
PageElement* newFzImage(int pageNo, fz_rect rect, size_t imageIdx);
PageElement* newFzLink(int pageNo, fz_link* link, fz_outline* outline);
PageDestination* newFzDestination(fz_outline*);
IPageElement* FzGetElementAtPos(FzPageInfo* pageInfo, PointF pt);
void FzGetElements(Vec<IPageElement*>* els, FzPageInfo* pageInfo);
void FzLinkifyPageText(FzPageInfo* pageInfo, fz_stext_page* stext);
fz_pixmap* fz_convert_pixmap2(fz_context* ctx, fz_pixmap* pix, fz_colorspace* ds, fz_colorspace* prf,
                              fz_default_colorspaces* default_cs, fz_color_params color_params, int keep_alpha);
fz_image* fz_find_image_at_idx(fz_context* ctx, FzPageInfo* pageInfo, int idx);
void fz_find_image_positions(fz_context* ctx, Vec<FitzImagePos>& images, fz_stext_page* stext);

// float is in range 0...1
COLORREF FromPdfColor(fz_context* ctx, int n, float color[4]);
int ToPdfRgba(COLORREF c, float col[4]);
