#include "mupdf/pdf.h"

/*
	These functions have been split out of pdf_xref.c to allow tools
	to be linked without pulling in the interpreter. The interpreter
	references the built-in font and cmap resources which are quite
	big. Not linking those into the tools saves roughly 6MB in the
	resulting executables.
*/

pdf_document *
pdf_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	pdf_document *doc = pdf_open_document_no_run_with_stream(ctx, file);
	doc->super.run_page_contents = (fz_document_run_page_contents_fn *)pdf_run_page_contents;
	doc->super.run_annot = (fz_document_run_annot_fn *)pdf_run_annot;
	doc->update_appearance = pdf_update_appearance;
	return doc;
}

pdf_document *
pdf_open_document(fz_context *ctx, const char *filename)
{
	pdf_document *doc = pdf_open_document_no_run(ctx, filename);
	doc->super.run_page_contents = (fz_document_run_page_contents_fn *)pdf_run_page_contents;
	doc->super.run_annot = (fz_document_run_annot_fn *)pdf_run_annot;
	doc->update_appearance = pdf_update_appearance;
	return doc;
}

fz_document_handler pdf_document_handler =
{
	(fz_document_recognize_fn *)&pdf_recognize,
	(fz_document_open_fn *)&pdf_open_document,
	(fz_document_open_with_stream_fn *)&pdf_open_document_with_stream
};
