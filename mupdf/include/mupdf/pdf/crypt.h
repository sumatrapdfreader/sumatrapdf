#ifndef MUPDF_PDF_CRYPT_H
#define MUPDF_PDF_CRYPT_H

enum
{
	PDF_ENCRYPT_KEEP,
	PDF_ENCRYPT_NONE,
	PDF_ENCRYPT_RC4_40,
	PDF_ENCRYPT_RC4_128,
	PDF_ENCRYPT_AES_128,
	PDF_ENCRYPT_AES_256,
	PDF_ENCRYPT_UNKNOWN
};

pdf_crypt *pdf_new_crypt(fz_context *ctx, pdf_obj *enc, pdf_obj *id);
pdf_crypt *pdf_new_encrypt(fz_context *ctx, const char *opwd_utf8, const char *upwd_utf8, pdf_obj *id, int permissions, int algorithm);
void pdf_drop_crypt(fz_context *ctx, pdf_crypt *crypt);

void pdf_crypt_obj(fz_context *ctx, pdf_crypt *crypt, pdf_obj *obj, int num, int gen);
void pdf_crypt_buffer(fz_context *ctx, pdf_crypt *crypt, fz_buffer *buf, int num, int gen);
fz_stream *pdf_open_crypt(fz_context *ctx, fz_stream *chain, pdf_crypt *crypt, int num, int gen);
fz_stream *pdf_open_crypt_with_filter(fz_context *ctx, fz_stream *chain, pdf_crypt *crypt, pdf_obj *name, int num, int gen);

int pdf_crypt_version(fz_context *ctx, pdf_crypt *crypt);
int pdf_crypt_revision(fz_context *ctx, pdf_crypt *crypt);
char *pdf_crypt_method(fz_context *ctx, pdf_crypt *crypt);
int pdf_crypt_length(fz_context *ctx, pdf_crypt *crypt);
int pdf_crypt_permissions(fz_context *ctx, pdf_crypt *crypt);
int pdf_crypt_encrypt_metadata(fz_context *ctx, pdf_crypt *crypt);
unsigned char *pdf_crypt_owner_password(fz_context *ctx, pdf_crypt *crypt);
unsigned char *pdf_crypt_user_password(fz_context *ctx, pdf_crypt *crypt);
unsigned char *pdf_crypt_owner_encryption(fz_context *ctx, pdf_crypt *crypt);
unsigned char *pdf_crypt_user_encryption(fz_context *ctx, pdf_crypt *crypt);
unsigned char *pdf_crypt_permissions_encryption(fz_context *ctx, pdf_crypt *crypt);
unsigned char *pdf_crypt_key(fz_context *ctx, pdf_crypt *crypt);

void pdf_print_crypt(fz_context *ctx, fz_output *out, pdf_crypt *crypt);

void pdf_write_digest(fz_context *ctx, fz_output *out, pdf_obj *byte_range, size_t digest_offset, size_t digest_length, pdf_pkcs7_signer *signer);

/*
	User access permissions from PDF reference.
*/
enum
{
	PDF_PERM_PRINT = 1 << 2,
	PDF_PERM_MODIFY = 1 << 3,
	PDF_PERM_COPY = 1 << 4,
	PDF_PERM_ANNOTATE = 1 << 5,
	PDF_PERM_FORM = 1 << 8,
	PDF_PERM_ACCESSIBILITY = 1 << 9, /* deprecated in pdf 2.0 (this permission is always granted) */
	PDF_PERM_ASSEMBLE = 1 << 10,
	PDF_PERM_PRINT_HQ = 1 << 11,
};

int pdf_document_permissions(fz_context *ctx, pdf_document *doc);

int pdf_signature_byte_range(fz_context *ctx, pdf_document *doc, pdf_obj *signature, fz_range *byte_range);

fz_stream *pdf_signature_hash_bytes(fz_context *ctx, pdf_document *doc, pdf_obj *signature);

int pdf_signature_incremental_change_since_signing(fz_context *ctx, pdf_document *doc, pdf_obj *signature);

size_t pdf_signature_contents(fz_context *ctx, pdf_document *doc, pdf_obj *signature, char **contents);

void pdf_sign_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget, pdf_pkcs7_signer *signer);

void pdf_clear_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget);

void pdf_encrypt_data(fz_context *ctx, pdf_crypt *crypt, int num, int gen, void (*fmt_str_out)(fz_context *, void *, const unsigned char *, size_t), void *arg, const unsigned char *s, size_t n);

size_t pdf_encrypted_len(fz_context *ctx, pdf_crypt *crypt, int num, int gen, size_t len);

#endif
