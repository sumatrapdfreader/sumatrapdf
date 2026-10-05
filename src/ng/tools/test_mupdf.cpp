// Smoke test for the mupdf static library: opens a document and prints how
// many pages it has.

#include <stdio.h>

#include "mupdf/fitz.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_mupdf <file>\n");
        return 2;
    }
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        fprintf(stderr, "fz_new_context() failed\n");
        return 1;
    }
    fz_document* doc = nullptr;
    int nPages = 0;
    fz_var(doc);
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        doc = fz_open_document(ctx, argv[1]);
        nPages = fz_count_pages(ctx, doc);
    }
    fz_always(ctx) {
        fz_drop_document(ctx, doc);
    }
    fz_catch(ctx) {
        fprintf(stderr, "%s: %s\n", argv[1], fz_caught_message(ctx));
        fz_drop_context(ctx);
        return 1;
    }
    printf("%s: %d pages\n", argv[1], nPages);
    fz_drop_context(ctx);
    return 0;
}
