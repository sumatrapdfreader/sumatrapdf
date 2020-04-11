#ifndef MUPDF_PDF_OUTPUT_PDF_H
#define MUPDF_PDF_OUTPUT_PDF_H

/*
	Create a pdf device. Rendering to the device creates
	new pdf content. WARNING: this device is work in progress. It doesn't
	currently support all rendering cases.

	Note that contents must be a stream (dictionary) to be updated (or
	a reference to a stream). Callers should take care to ensure that it
	is not an array, and that is it not shared with other objects/pages.
*/
fz_device *pdf_new_pdf_device(fz_context *ctx, pdf_document *doc, fz_matrix topctm,
		fz_rect mediabox, pdf_obj *resources, fz_buffer *contents);

void pdf_localise_page_resources(fz_context *ctx, pdf_document *doc);

#endif
