#include "mupdf/pdf.h"

/*
	These functions have been split out of pdf_xref.c to allow tools
	to be linked without pulling in the interpreter. The interpreter
	references the built-in font and cmap resources which are quite
	big. Not linking those into the tools saves roughly 6MB in the
	resulting executables.
*/

static void pdf_run_page_contents_shim(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie)
{
	pdf_run_page_contents((pdf_document*)doc, (pdf_page*)page, dev, transform, cookie);
}

static void pdf_run_annot_shim(fz_document *doc, fz_page *page, fz_annot *annot, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie)
{
	pdf_run_annot((pdf_document*)doc, (pdf_page*)page, (pdf_annot *)annot, dev, transform, cookie);
}

pdf_document *
pdf_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	pdf_document *doc = pdf_open_document_no_run_with_stream(ctx, file);
	doc->super.run_page_contents = pdf_run_page_contents_shim;
	doc->super.run_annot = pdf_run_annot_shim;
	doc->update_appearance = pdf_update_appearance;
	return doc;
}

pdf_document *
pdf_open_document(fz_context *ctx, const char *filename)
{
	pdf_document *doc = pdf_open_document_no_run(ctx, filename);
	doc->super.run_page_contents = pdf_run_page_contents_shim;
	doc->super.run_annot = pdf_run_annot_shim;
	doc->update_appearance = pdf_update_appearance;
	return doc;
}
