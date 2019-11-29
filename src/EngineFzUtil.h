/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Common for EnginePdf.cpp and EngineXps.cpp
// they are implemented in EnginePdf.cpp

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
    fz_image* image = nullptr;
    fz_rect rect = fz_unit_rect;
};

struct FzPageInfo {
    int pageNo = 0; // 1-based
    fz_page* page = nullptr;
    fz_link* links = nullptr;
    fz_display_list* list = nullptr;
    fz_stext_page* stext = nullptr;
    RectD mediabox = {};
    Vec<pdf_annot*> pageAnnots;
    Vec<fz_rect> imageRects;
};

fz_rect fz_RectD_to_rect(RectD rect);
RectD fz_rect_to_RectD(fz_rect rect);
fz_matrix fz_create_view_ctm(fz_rect mediabox, float zoom, int rotation);
fz_stream* fz_open_istream(fz_context* ctx, IStream* stream);
fz_stream* fz_open_file2(fz_context* ctx, const WCHAR* filePath);
RenderedBitmap* new_rendered_fz_pixmap(fz_context* ctx, fz_pixmap* pixmap);
u8* fz_extract_stream_data(fz_context* ctx, fz_stream* stream, size_t* cbCount);
float fz_calc_overlap(fz_rect r1, fz_rect r2);
WCHAR* fz_text_page_to_str(fz_stext_page* text, const WCHAR* lineSep, RectI** coordsOut);
