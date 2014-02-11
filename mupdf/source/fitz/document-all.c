#include "mupdf/fitz.h"

void fz_register_document_handlers(fz_context *ctx)
{
	fz_register_document_handler(ctx, &pdf_document_handler);
	fz_register_document_handler(ctx, &xps_document_handler);
	fz_register_document_handler(ctx, &cbz_document_handler);
	fz_register_document_handler(ctx, &img_document_handler);
	fz_register_document_handler(ctx, &tiff_document_handler);
}
