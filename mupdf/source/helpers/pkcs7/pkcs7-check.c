#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include "mupdf/helpers/pkcs7-check.h"

char *pdf_signature_error_description(pdf_signature_error err)
{
	switch (err)
	{
	case PDF_SIGNATURE_ERROR_OKAY:
		return "OK";
	case PDF_SIGNATURE_ERROR_NO_SIGNATURES:
		return "No signatures.";
	case PDF_SIGNATURE_ERROR_NO_CERTIFICATE:
		return "No certificate.";
	case PDF_SIGNATURE_ERROR_DIGEST_FAILURE:
		return "Signature invalidated by change to document.";
	case PDF_SIGNATURE_ERROR_SELF_SIGNED:
		return "Self-signed certificate.";
	case PDF_SIGNATURE_ERROR_SELF_SIGNED_IN_CHAIN:
		return "Self-signed certificate in chain.";
	case PDF_SIGNATURE_ERROR_NOT_TRUSTED:
		return "Certificate not trusted.";
	default:
	case PDF_SIGNATURE_ERROR_UNKNOWN:
		return "Unknown error.";
	}
}

#ifdef HAVE_LIBCRYPTO
#include "mupdf/helpers/pkcs7-openssl.h"
#include <string.h>

static void pdf_format_designated_name(pdf_pkcs7_designated_name *name, char *buf, size_t buflen)
{
	int i, n;
	const char *part[] = {
		"CN=", name->cn,
		", O=", name->o,
		", OU=", name->ou,
		", emailAddress=", name->email,
		", C=", name->c};

		if (buflen)
		buf[0] = 0;

	n = sizeof(part)/sizeof(*part);
	for (i = 0; i < n; i++)
		if (part[i])
			fz_strlcat(buf, part[i], buflen);
}

void pdf_signature_designated_name(fz_context *ctx, pdf_document *doc, pdf_obj *signature, char *buf, size_t buflen)
{
	char *contents = NULL;
	size_t contents_len = pdf_signature_contents(ctx, doc, signature, &contents);
	pdf_pkcs7_designated_name *name = NULL;
	fz_try(ctx)
	{
		name = pkcs7_openssl_designated_name(ctx, contents, contents_len);
		if (name)
		{
			pdf_format_designated_name(name, buf, buflen);
			pkcs7_openssl_drop_designated_name(ctx, name);
		}
		else if (buflen > 0)
			buf[0] = '\0';
	}
	fz_always(ctx)
		fz_free(ctx, contents);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

pdf_signature_error pdf_check_digest(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	pdf_signature_error err;
	fz_stream *bytes = NULL;
	char *contents = NULL;
	size_t contents_len = pdf_signature_contents(ctx, doc, signature, &contents);
	fz_var(err);
	fz_var(bytes);
	fz_try(ctx)
	{
		bytes = pdf_signature_hash_bytes(ctx, doc, signature);
		err = pkcs7_openssl_check_digest(ctx, bytes, contents, contents_len);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, bytes);
		fz_free(ctx, contents);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return err;
}

pdf_signature_error pdf_check_certificate(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	char *contents = NULL;
	size_t contents_len = pdf_signature_contents(ctx, doc, signature, &contents);
	pdf_signature_error result;
	fz_try(ctx)
		result = pkcs7_openssl_check_certificate(contents, contents_len);
	fz_always(ctx)
		fz_free(ctx, contents);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return result;
}

int pdf_check_signature(fz_context *ctx, pdf_document *doc, pdf_obj *signature, char *ebuf, size_t ebufsize)
{
	int res = 0;

	if (pdf_xref_obj_is_unsaved_signature(doc, signature))
	{
		fz_strlcpy(ebuf, "Signed but document yet to be saved.", ebufsize);
		if (ebufsize > 0)
			ebuf[ebufsize-1] = 0;
		return 0;
	}

	fz_var(res);
	fz_try(ctx)
	{
		if (pdf_signature_is_signed(ctx, doc, signature))
		{
			pdf_signature_error err;

			err = pdf_check_digest(ctx, doc, signature);
			if (err == PDF_SIGNATURE_ERROR_OKAY)
				err = pdf_check_certificate(ctx, doc, signature);

			fz_strlcpy(ebuf, pdf_signature_error_description(err), ebufsize);
			res = (err == PDF_SIGNATURE_ERROR_OKAY);

			switch (err)
			{
			case PDF_SIGNATURE_ERROR_SELF_SIGNED:
			case PDF_SIGNATURE_ERROR_SELF_SIGNED_IN_CHAIN:
			case PDF_SIGNATURE_ERROR_NOT_TRUSTED:
				{
					size_t len;
					fz_strlcat(ebuf, " (", ebufsize);
					len = strlen(ebuf);
					pdf_signature_designated_name(ctx, doc, signature, ebuf + len, ebufsize - len);
					fz_strlcat(ebuf, ")", ebufsize);
				}
				break;
			default:
				break;
			}
		}
		else
		{
			res = 0;
			fz_strlcpy(ebuf, "Not signed.", ebufsize);
		}
	}
	fz_catch(ctx)
	{
		res = 0;
		fz_strlcpy(ebuf, fz_caught_message(ctx), ebufsize);
	}

	if (ebufsize > 0)
		ebuf[ebufsize-1] = 0;

	return res;
}

int pdf_supports_signatures(fz_context *ctx)
{
	return 1;
}

#else

void pdf_signature_designated_name(fz_context *ctx, pdf_document *doc, pdf_obj *signature, char *buf, size_t buflen)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OpenSSL support.");
}

pdf_signature_error pdf_check_digest(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OpenSSL support.");
	return 0;
}

pdf_signature_error pdf_check_certificate(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OpenSSL support.");
	return 0;
}


int pdf_check_signature(fz_context *ctx, pdf_document *doc, pdf_obj *signature, char *ebuf, size_t ebufsize)
{
	fz_strlcpy(ebuf, "No digital signing support in this build", ebufsize);
	return 0;
}

int pdf_supports_signatures(fz_context *ctx)
{
	return 0;
}

#endif
