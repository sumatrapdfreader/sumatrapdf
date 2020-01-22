#ifndef MUPDF_PKCS7_CHECK_H
#define MUPDF_PKCS7_CHECK_H

char *pdf_signature_error_description(pdf_signature_error err);
void pdf_signature_designated_name(fz_context *ctx, pdf_document *doc, pdf_obj *signature, char *buf, size_t buflen);
pdf_signature_error pdf_check_digest(fz_context *ctx, pdf_document *doc, pdf_obj *signature);
pdf_signature_error pdf_check_certificate(fz_context *ctx, pdf_document *doc, pdf_obj *signature);
/*
	check a signature's certificate chain and digest

	This is a helper function defined to provide compatibility with older
	versions of mupdf
*/
int pdf_check_signature(fz_context *ctx, pdf_document *doc, pdf_obj *signature, char *ebuf, size_t ebufsize);

int pdf_supports_signatures(fz_context *ctx);

#endif
