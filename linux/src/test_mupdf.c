/*
 * SumatraPDF Linux Port - Phase 1 Test
 *
 * Minimal test to verify MuPDF compiles and links correctly on Linux.
 * Opens a PDF (if provided as argv[1]) and prints page count,
 * or just verifies the library initializes correctly.
 */

#include <stdio.h>
#include <stdlib.h>
#include "mupdf/fitz.h"

int main(int argc, char *argv[])
{
    fz_context *ctx;
    fz_document *doc;
    int page_count;

    /* Create a MuPDF context */
    ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    if (!ctx) {
        fprintf(stderr, "error: cannot create mupdf context\n");
        return 1;
    }

    /* Register default document handlers */
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
    }
    fz_catch(ctx) {
        fprintf(stderr, "error: cannot register document handlers: %s\n", fz_caught_message(ctx));
        fz_drop_context(ctx);
        return 1;
    }

    printf("MuPDF context initialized successfully.\n");
    printf("Registered document handlers: PDF, XPS, SVG, CBZ, IMG, HTML, EPUB\n");

    /* If a file was provided, try to open it */
    if (argc >= 2) {
        const char *filename = argv[1];
        printf("Opening: %s\n", filename);

        fz_try(ctx) {
            doc = fz_open_document(ctx, filename);
            page_count = fz_count_pages(ctx, doc);
            printf("Page count: %d\n", page_count);

            /* Render first page to verify rendering pipeline */
            if (page_count > 0) {
                fz_pixmap *pix;
                fz_matrix ctm = fz_scale(1.0f, 1.0f); /* 72 DPI */

                pix = fz_new_pixmap_from_page_number(ctx, doc, 0, ctm, fz_device_rgb(ctx), 0);
                printf("Page 1 rendered: %d x %d pixels\n", pix->w, pix->h);

                /* Optionally write to PNG if second arg is provided */
                if (argc >= 3) {
                    fz_save_pixmap_as_png(ctx, pix, argv[2]);
                    printf("Saved to: %s\n", argv[2]);
                }

                fz_drop_pixmap(ctx, pix);
            }

            fz_drop_document(ctx, doc);
        }
        fz_catch(ctx) {
            fprintf(stderr, "error: %s\n", fz_caught_message(ctx));
            fz_drop_context(ctx);
            return 1;
        }
    } else {
        printf("Usage: test-mupdf [file.pdf] [output.png]\n");
        printf("  No file provided - context-only test passed.\n");
    }

    fz_drop_context(ctx);
    printf("OK\n");
    return 0;
}
