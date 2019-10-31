#ifndef MUPDF_PDF_OUTPUT_PDF_H
#define MUPDF_PDF_OUTPUT_PDF_H

fz_device *pdf_new_pdf_device(fz_context *ctx, pdf_document *doc, fz_matrix topctm,
		fz_rect mediabox, pdf_obj *resources, fz_buffer *contents);

void pdf_localise_page_resources(fz_context *ctx, pdf_document *doc);

#endif
