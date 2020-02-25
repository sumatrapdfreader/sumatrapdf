/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/Log.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/TreeModel.h"

#include "resource.h"
#include "Translations.h"

#include "EngineBase.h"

#include "SaveAsPdf.h"

const pdf_write_options pdf_default_write_options2 = {
    0,  /* do_incremental */
    0,  /* do_pretty */
    0,  /* do_ascii */
    0,  /* do_compress */
    0,  /* do_compress_images */
    0,  /* do_compress_fonts */
    0,  /* do_decompress */
    0,  /* do_garbage */
    0,  /* do_linear */
    0,  /* do_clean */
    0,  /* do_sanitize */
    0,  /* do_appearance */
    0,  /* do_encrypt */
    ~0, /* permissions */
    "", /* opwd_utf8[128] */
    "", /* upwd_utf8[128] */
};

struct PdfMerger {
    fz_context* ctx = nullptr;
    pdf_document* doc_des = nullptr;
    EngineBase** engines = nullptr;
    VecStr filePaths;

    PdfMerger() = default;
    ~PdfMerger();
    bool MergeAndSave(TocItem*, char* dstPath);
};

PdfMerger::~PdfMerger() {
    if (engines) {
        int nFiles = filePaths.size();
        for (int i = 0; i < nFiles; i++) {
            delete engines[i];
        }
        free(engines);
    }
    pdf_drop_document(ctx, doc_des);
    fz_flush_warnings(ctx);
    fz_drop_context(ctx);
}

bool PdfMerger::MergeAndSave(TocItem* root, char* dstPath) {
    VisitTocTree(root, [this](TocItem* ti) -> bool {
        if (!ti->engineFilePath) {
            return true;
        }
        std::string_view path = ti->engineFilePath;
        filePaths.Append(path);
        return true;
    });

    int nFiles = filePaths.size();
    if (nFiles == 0) {
        // TODO: show an error message
        return false;
    }

    engines = AllocArray<EngineBase*>((size_t)nFiles);

    ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);

    // TODO: install warnigngs redirect
    fz_try(ctx) {
        doc_des = pdf_create_document(ctx);
    }
    fz_catch(ctx) {
        doc_des = nullptr;
    }
    if (doc_des == nullptr) {
        return false;
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
        // TODO: show an error message?
        return false;
    }
    return true;
}

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

void SaveVirtualAsPdf(TocItem* root, char* dstPath) {
    PdfMerger* merger = new PdfMerger();

    bool ok = merger->MergeAndSave(root, dstPath);
    if (!ok) {
        // TODO: show error message
    }
    delete merger;
}
