#ifndef MUPDF_PDF_CRYPT_H
#define MUPDF_PDF_CRYPT_H

/*
 * Encryption
 */

pdf_crypt *pdf_new_crypt(fz_context *ctx, pdf_obj *enc, pdf_obj *id);
void pdf_free_crypt(fz_context *ctx, pdf_crypt *crypt);

void pdf_crypt_obj(fz_context *ctx, pdf_crypt *crypt, pdf_obj *obj, int num, int gen);
void pdf_crypt_buffer(fz_context *ctx, pdf_crypt *crypt, fz_buffer *buf, int num, int gen);
fz_stream *pdf_open_crypt(fz_stream *chain, pdf_crypt *crypt, int num, int gen);
fz_stream *pdf_open_crypt_with_filter(fz_stream *chain, pdf_crypt *crypt, char *name, int num, int gen);

int pdf_crypt_version(pdf_document *doc);
int pdf_crypt_revision(pdf_document *doc);
char *pdf_crypt_method(pdf_document *doc);
int pdf_crypt_length(pdf_document *doc);
unsigned char *pdf_crypt_key(pdf_document *doc);

#ifndef NDEBUG
void pdf_print_crypt(pdf_crypt *crypt);
#endif

/*
	pdf_signature_widget_byte_range: retrieve the byte range for a signature widget
*/
int pdf_signature_widget_byte_range(pdf_document *doc, pdf_widget *widget, int (*byte_range)[2]);

/*
	pdf_signature_widget_contents: retrieve the contents for a signature widget
*/
int pdf_signature_widget_contents(pdf_document *doc, pdf_widget *widget, char **contents);

/*
	fz_check_signature: check a signature's certificate chain and digest
*/
int pdf_check_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget, char *file, char *ebuf, int ebufsize);

#endif
