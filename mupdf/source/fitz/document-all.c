#include "mupdf/fitz.h"

extern fz_document_handler pdf_document_handler;
extern fz_document_handler xps_document_handler;
extern fz_document_handler svg_document_handler;
extern fz_document_handler cbz_document_handler;
extern fz_document_handler img_document_handler;
extern fz_document_handler html_document_handler;
extern fz_document_handler epub_document_handler;

/*
	Register handlers
	for all the standard document types supported in
	this build.
*/
void fz_register_document_handlers(fz_context *ctx)
{
#if FZ_ENABLE_PDF
	fz_register_document_handler(ctx, &pdf_document_handler);
#endif /* FZ_ENABLE_PDF */
#if FZ_ENABLE_XPS
	fz_register_document_handler(ctx, &xps_document_handler);
#endif /* FZ_ENABLE_XPS */
#if FZ_ENABLE_SVG
	fz_register_document_handler(ctx, &svg_document_handler);
#endif /* FZ_ENABLE_SVG */
#if FZ_ENABLE_CBZ
	fz_register_document_handler(ctx, &cbz_document_handler);
#endif /* FZ_ENABLE_CBZ */
#if FZ_ENABLE_IMG
	fz_register_document_handler(ctx, &img_document_handler);
#endif /* FZ_ENABLE_IMG */
#if FZ_ENABLE_HTML
	fz_register_document_handler(ctx, &html_document_handler);
#endif /* FZ_ENABLE_HTML */
#if FZ_ENABLE_EPUB
	fz_register_document_handler(ctx, &epub_document_handler);
#endif /* FZ_ENABLE_EPUB */
}
