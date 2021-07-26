/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// this is for adding temporary code for testing

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/Timer.h"

#include "utils/Log.h"

#define DEFRES 96

struct texture {
    int x, y, w, h;
    float s, t;
};

static float layout_w = FZ_DEFAULT_LAYOUT_W;
static float layout_h = FZ_DEFAULT_LAYOUT_H;
static float layout_em = FZ_DEFAULT_LAYOUT_EM;

static char* layout_css{nullptr};
static int layout_use_doc_css{1};
static int enable_js = 1;

static fz_document* doc{nullptr};
static fz_page* fzpage = NULL;
static fz_separations* seps = NULL;
static fz_outline* outline = NULL;
static fz_link* links = NULL;

static int number = 0;

static fz_pixmap* page_contents = NULL;
static struct texture page_tex = {0};
static int screen_w = 0, screen_h = 0;
static int scroll_x = 0, scroll_y = 0;
static int canvas_x = 0, canvas_w = 100;
static int canvas_y = 0, canvas_h = 100;

static fz_location oldpage = {0, 0}, currentpage = {0, 0};

static fz_context* ctx{nullptr};
pdf_document* pdf = NULL;
pdf_page* page = NULL;
pdf_annot* selected_annot = NULL;
fz_stext_page* page_text = NULL;
fz_matrix draw_page_ctm, view_page_ctm, view_page_inv_ctm;
fz_rect page_bounds, draw_page_bounds, view_page_bounds;
fz_irect view_page_area;

static char* reflow_options = NULL;

static float oldzoom = DEFRES, currentzoom = DEFRES;
static float oldrotate = 0, currentrotate = 0;

struct mark {
    fz_location loc;
    fz_point scroll;
};

static void load_document(const char* path) {
    fz_drop_outline(ctx, outline);
    fz_drop_document(ctx, doc);

    // TODO: accelerator filename
    doc = fz_open_document(ctx, path);
    pdf = pdf_specifics(ctx, doc);

    fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);
}

void reflow_document() {
}

void transform_page(void) {
    draw_page_ctm = fz_transform_page(page_bounds, currentzoom, currentrotate);
    draw_page_bounds = fz_transform_rect(page_bounds, draw_page_ctm);
}

static void load_page() {
    fz_irect area;

    fz_drop_stext_page(ctx, page_text);
    page_text = NULL;
    fz_drop_separations(ctx, seps);
    seps = NULL;
    fz_drop_link(ctx, links);
    links = NULL;
    fz_drop_page(ctx, fzpage);
    fzpage = NULL;

    fzpage = fz_load_chapter_page(ctx, doc, currentpage.chapter, currentpage.page);
    if (pdf)
        page = (pdf_page*)fzpage;

    links = fz_load_links(ctx, fzpage);
    page_text = fz_new_stext_page_from_page(ctx, fzpage, NULL);

    /* compute bounds here for initial window size */
    page_bounds = fz_bound_page(ctx, fzpage);
    transform_page();

    area = fz_irect_from_rect(draw_page_bounds);
    page_tex.w = area.x1 - area.x0;
    page_tex.h = area.y1 - area.y0;
}

static void render_page() {
    fz_irect bbox;
    fz_pixmap* pix;
    fz_device* dev;

    transform_page();
    fz_drop_pixmap(ctx, page_contents);
    page_contents = NULL;

    bbox = fz_round_rect(fz_transform_rect(fz_bound_page(ctx, fzpage), draw_page_ctm));
    page_contents = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, seps, 0);
    fz_clear_pixmap(ctx, page_contents);

    dev = fz_new_draw_device(ctx, draw_page_ctm, page_contents);
    fz_run_page_contents(ctx, fzpage, dev, fz_identity, NULL);
    fz_close_device(ctx, dev);
    fz_drop_device(ctx, dev);

    pix = fz_clone_pixmap_area_with_different_seps(ctx, page_contents, NULL, fz_device_rgb(ctx), NULL,
                                                   fz_default_color_params, NULL);
    {
        dev = fz_new_draw_device(ctx, draw_page_ctm, pix);
        fz_run_page_annots(ctx, fzpage, dev, fz_identity, NULL);
        fz_run_page_widgets(ctx, fzpage, dev, fz_identity, NULL);
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
    }
    fz_drop_pixmap(ctx, pix);
}

void NewEpub(const WCHAR* pathW) {
    auto t = TimeGet();
    ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    fz_register_document_handlers(ctx);

    if (layout_css) {
        fz_buffer* buf = fz_read_file(ctx, layout_css);
        fz_set_user_css(ctx, fz_string_from_buffer(ctx, buf));
        fz_drop_buffer(ctx, buf);
    }
    fz_set_use_document_css(ctx, layout_use_doc_css);
    char* path = ToUtf8Temp(pathW);

    fz_try(ctx) {
        page_tex.w = 600;
        page_tex.h = 700;
        load_document(path);
    }
    fz_always(ctx) {
    }
    fz_catch(ctx) {
        logf("Failed to open document %s\n", path);
    }
    if (!doc) {
        return;
    }

    int nPages = 0;
    fz_try(ctx) {
        int nc = fz_count_chapters(ctx, doc);
        for (int c = 0; c < nc; c++) {
            int np = fz_count_chapter_pages(ctx, doc, c);
            nPages += np;
            for (int p = 0; p < np; p++) {
                currentpage.chapter = c;
                currentpage.page = p;
                load_page();
                render_page();
            }
        }
    }
    fz_catch(ctx) {
        logf("something failed\n");
    }
    fz_drop_document(ctx, doc);
    doc = nullptr;
    double timeMs = TimeSinceInMs(t);
    logf(L"NewEpub(): %d pages finished in %.2f ms\n", nPages, timeMs);
}
