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

typedef struct pdf_designated_name_s
{
	char *cn;
	char *o;
	char *ou;
	char *email;
	char *c;
}
pdf_designated_name;

void pdf_free_designated_name(pdf_designated_name *dn);


pdf_signer *pdf_read_pfx(fz_context *ctx, const char *sigfile, const char *password);
pdf_signer *pdf_keep_signer(pdf_signer *signer);
void pdf_drop_signer(pdf_signer *signer);
pdf_designated_name *pdf_signer_designated_name(pdf_signer *signer);
void pdf_write_digest(pdf_document *doc, char *filename, pdf_obj *byte_range, int digest_offset, int digest_length, pdf_signer *signer);

/*
	pdf_signature_widget_byte_range: retrieve the byte range for a signature widget
*/
int pdf_signature_widget_byte_range(pdf_document *doc, pdf_widget *widget, int (*byte_range)[2]);

/*
	pdf_signature_widget_contents: retrieve the contents for a signature widget
*/
int pdf_signature_widget_contents(pdf_document *doc, pdf_widget *widget, char **contents);

/*
	pdf_check_signature: check a signature's certificate chain and digest
*/
int pdf_check_signature(pdf_document *doc, pdf_widget *widget, char *file, char *ebuf, int ebufsize);

/*
	pdf_sign_signature: sign a signature form field
*/
void pdf_sign_signature(pdf_document *doc, pdf_widget *widget, const char *sigfile, const char *password);

#endif
