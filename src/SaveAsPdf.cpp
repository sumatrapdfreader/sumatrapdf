/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Translations.h"
#include "SaveAsPdf.h"
#include "resource.h"
#include "Commands.h"

#include "utils/Log.h"

// based on pdfmerge.c in mupdf

const pdf_write_options pdf_default_write_options2 = {
    0,   /* do_incremental */
    0,   /* do_pretty */
    0,   /* do_ascii */
    0,   /* do_compress */
    0,   /* do_compress_images */
    0,   /* do_compress_fonts */
    0,   /* do_decompress */
    0,   /* do_garbage */
    0,   /* do_linear */
    0,   /* do_clean */
    0,   /* do_sanitize */
    0,   /* do_appearance */
    0,   /* do_encrypt */
    ~0,  /* permissions */
    {0}, /* opwd_utf8[128] */
    {0}, /* upwd_utf8[128] */
};

/* Copy as few key/value pairs as we can. Do not include items that reference other pages. */
// clang-format off
static pdf_obj* const copy_list[] = {
    PDF_NAME(Contents),
    PDF_NAME(Resources),
    PDF_NAME(MediaBox),
    PDF_NAME(CropBox),
    PDF_NAME(BleedBox),
    PDF_NAME(TrimBox),
    PDF_NAME(ArtBox),
    PDF_NAME(Rotate),
    PDF_NAME(UserUnit)
};
// clang-format on

struct PdfMerger {
    fz_context* ctx = nullptr;
    pdf_document* doc_des = nullptr;
    pdf_document* doc_src = nullptr;
    StrVec filePaths;

    PdfMerger() = default;
    ~PdfMerger();
    bool MergeAndSave(TocItem*, char* dstPath);
    bool MergePdfFile(const char*);
    void MergePdfPage(int page_from, int page_to, pdf_graft_map* graft_map) const;
};

PdfMerger::~PdfMerger() {
    pdf_drop_document(ctx, doc_des);
    fz_flush_warnings(ctx);
    fz_drop_context(ctx);
}

void PdfMerger::MergePdfPage(int page_from, int page_to, pdf_graft_map* graft_map) const {
    pdf_obj* page_ref = nullptr;
    pdf_obj* page_dict = nullptr;
    pdf_obj* obj = nullptr;
    pdf_obj* ref = nullptr;
    int i;

    fz_var(ref);
    fz_var(page_dict);

    fz_try(ctx) {
        page_ref = pdf_lookup_page_obj(ctx, doc_src, page_from - 1);
        pdf_flatten_inheritable_page_items(ctx, page_ref);

        page_dict = pdf_new_dict(ctx, doc_des, 4);

        pdf_dict_put(ctx, page_dict, PDF_NAME(Type), PDF_NAME(Page));
        for (i = 0; i < (int)nelem(copy_list); i++) {
            obj = pdf_dict_get(ctx, page_ref, copy_list[i]);
            if (obj != nullptr) {
                pdf_dict_put_drop(ctx, page_dict, copy_list[i], pdf_graft_mapped_object(ctx, graft_map, obj));
            }
        }

        ref = pdf_add_object(ctx, doc_des, page_dict);

        pdf_insert_page(ctx, doc_des, page_to - 1, ref);
    }
    fz_always(ctx) {
        pdf_drop_obj(ctx, page_dict);
        pdf_drop_obj(ctx, ref);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

bool PdfMerger::MergePdfFile(const char* path) {
    doc_src = pdf_open_document(ctx, path);
    if (!doc_src) {
        return false;
    }

    int nPages = 0;
    pdf_graft_map* graft_map = nullptr;

    fz_try(ctx) {
        nPages = pdf_count_pages(ctx, doc_src);
        graft_map = pdf_new_graft_map(ctx, doc_des);
        for (int i = 1; i <= nPages; i++) {
            MergePdfPage(i, -1, graft_map);
        }
    }
    fz_always(ctx) {
        pdf_drop_graft_map(ctx, graft_map);
        pdf_drop_document(ctx, doc_src);
    }
    fz_catch(ctx) {
        // TODO: show error message
        fz_report_error(ctx);
        return false;
    }
    return true;
}

bool PdfMerger::MergeAndSave(TocItem* root, char* dstPath) {
    VisitTocTree(root, [this](TocItem* ti) -> bool {
        if (!ti->engineFilePath) {
            return true;
        }
        char* path = ti->engineFilePath;
        filePaths.Append(path);
        return true;
    });

    int nFiles = filePaths.Size();
    if (nFiles == 0) {
        // TODO: show an error message
        return false;
    }

    ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);

    // TODO: install warnigngs redirect
    fz_try(ctx) {
        doc_des = pdf_create_document(ctx);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        doc_des = nullptr;
    }
    if (doc_des == nullptr) {
        return false;
    }

    bool ok;
    for (int i = 0; i < nFiles; i++) {
        const char* path = filePaths.at(i);
        ok = MergePdfFile(path);
        if (!ok) {
            // TODO: show error message
            return false;
        }
    }

    pdf_write_options opts = pdf_default_write_options2;
    // TODO: compare results with and without compression
    opts.do_compress = 1;
    opts.do_compress_images = 1;
    opts.do_compress_fonts = 1;

    fz_try(ctx) {
        pdf_save_document(ctx, doc_des, dstPath, &opts);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        // TODO: show an error message?
        return false;
    }
    return true;
}

bool SaveVirtualAsPdf(TocItem* root, char* dstPath) {
    PdfMerger* merger = new PdfMerger();

    bool ok = merger->MergeAndSave(root, dstPath);
    delete merger;
    if (!ok) {
        // TODO: show error message
    }
    return ok;
}
