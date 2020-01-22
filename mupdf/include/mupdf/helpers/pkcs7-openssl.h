#ifndef MUPDF_PKCS7_OPENSSL_H
#define MUPDF_PKCS7_OPENSSL_H

/* This an example pkcs7 implementation using openssl. These are the types of functions that you
 * will likely need to sign documents and check signatures within documents. In particular, to
 * sign a document, you need a function that derives a pdf_pkcs7_signer object from a certificate
 * stored by the operating system or within a file. */

/* Check a signature's digest against ranges of bytes drawn from a stream */
pdf_signature_error pkcs7_openssl_check_digest(fz_context *ctx, fz_stream *stm, char *sig, size_t sig_len);

/* Check a signature's certificate is trusted */
pdf_signature_error pkcs7_openssl_check_certificate(char *sig, size_t sig_len);

/* Obtain the designated name information from signature's certificate */
pdf_pkcs7_designated_name *pkcs7_openssl_designated_name(fz_context *ctx, char *sig, size_t sig_len);

/* Free the resources associated with designated name information */
void pkcs7_openssl_drop_designated_name(fz_context *ctx, pdf_pkcs7_designated_name *dn);

/* Read the certificate and private key from a pfx file, holding it as an opaque structure */
pdf_pkcs7_signer *pkcs7_openssl_read_pfx(fz_context *ctx, const char *pfile, const char *pw);

#endif
