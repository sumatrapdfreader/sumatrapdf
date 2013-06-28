#ifndef MUPDF_PDF_OUTPUT_PDF_H
#define MUPDF_PDF_OUTPUT_PDF_H

/*
	pdf_new_pdf_device: Create a pdf device. Rendering to the device creates
	new pdf content. WARNING: this device is work in progress. It doesn't
	currently support all rendering cases.
*/
fz_device *pdf_new_pdf_device(pdf_document *doc, pdf_obj *contents, pdf_obj *resources, const fz_matrix *ctm);

/*
	pdf_write_document: Write out the document to a file with all changes finalised.
*/
void pdf_write_document(pdf_document *doc, char *filename, fz_write_options *opts);

void pdf_localise_page_resources(pdf_document *doc);

/* SumatraPDF: support PDF document updates */
typedef struct pdf_file_update_list_s pdf_file_update_list;
pdf_file_update_list *pdf_file_update_start(pdf_document *doc, const char *filepath, int max_xref_size);
#ifdef _WIN32
pdf_file_update_list *pdf_file_update_start_w(pdf_document *doc, const wchar_t *filepath, int max_xref_size);
#endif
void pdf_file_update_append(pdf_file_update_list *list, pdf_obj *dict, int num, int gen, fz_buffer *stream);
void pdf_file_update_end(pdf_file_update_list *list, pdf_obj *prev_trailer, int prev_xref_offset);

#endif
