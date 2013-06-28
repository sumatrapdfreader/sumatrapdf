#include "mupdf/pdf.h" // TODO: move this file to pdf module

#ifdef HAVE_OPENSSL

#include "openssl/err.h"
#include "openssl/bio.h"
#include "openssl/asn1.h"
#include "openssl/x509.h"
#include "openssl/err.h"
#include "openssl/objects.h"
#include "openssl/pem.h"
#include "openssl/pkcs7.h"

enum
{
	SEG_START = 0,
	SEG_SIZE = 1
};

typedef struct bsegs_struct
{
	int (*seg)[2];
	int nsegs;
	int current_seg;
	int seg_pos;
} BIO_SEGS_CTX;

static int bsegs_read(BIO *b, char *buf, int size)
{
	BIO_SEGS_CTX *ctx = (BIO_SEGS_CTX *)b->ptr;
	int read = 0;

	while (size > 0 && ctx->current_seg < ctx->nsegs)
	{
		int nb = ctx->seg[ctx->current_seg][SEG_SIZE] - ctx->seg_pos;

		if (nb > size)
			nb = size;

		if (nb > 0)
		{
			if (ctx->seg_pos == 0)
				(void)BIO_seek(b->next_bio, ctx->seg[ctx->current_seg][SEG_START]);

			(void)BIO_read(b->next_bio, buf, nb);
			ctx->seg_pos += nb;
			read += nb;
			buf += nb;
			size -= nb;
		}
		else
		{
			ctx->current_seg++;

			if (ctx->current_seg < ctx->nsegs)
				ctx->seg_pos = 0;
		}
	}

	return read;
}

static long bsegs_ctrl(BIO *b, int cmd, long arg1, void *arg2)
{
	return BIO_ctrl(b->next_bio, cmd, arg1, arg2);
}

static int bsegs_new(BIO *b)
{
	BIO_SEGS_CTX *ctx;

	ctx = (BIO_SEGS_CTX *)malloc(sizeof(BIO_SEGS_CTX));
	if (ctx == NULL)
		return 0;

	ctx->current_seg = 0;
	ctx->seg_pos = 0;
	ctx->seg = NULL;
	ctx->nsegs = 0;

	b->init = 1;
	b->ptr = (char *)ctx;
	b->flags = 0;
	b->num = 0;

	return 1;
}

static int bsegs_free(BIO *b)
{
	if (b == NULL)
		return 0;

	free(b->ptr);
	b->ptr = NULL;
	b->init = 0;
	b->flags = 0;

	return 1;
}

static long bsegs_callback_ctrl(BIO *b, int cmd, bio_info_cb *fp)
{
	return BIO_callback_ctrl(b->next_bio, cmd, fp);
}

static BIO_METHOD methods_bsegs =
{
	0,"segment reader",
	NULL,
	bsegs_read,
	NULL,
	NULL,
	bsegs_ctrl,
	bsegs_new,
	bsegs_free,
	bsegs_callback_ctrl,
};

static BIO_METHOD *BIO_f_segments(void)
{
	return &methods_bsegs;
}

static void BIO_set_segments(BIO *b, int (*seg)[2], int nsegs)
{
	BIO_SEGS_CTX *ctx = (BIO_SEGS_CTX *)b->ptr;

	ctx->seg = seg;
	ctx->nsegs = nsegs;
}

typedef struct verify_context_s
{
	X509_STORE_CTX x509_ctx;
	char certdesc[256];
	int err;
} verify_context;

static int verify_callback(int ok, X509_STORE_CTX *ctx)
{
	verify_context *vctx;
	X509 *err_cert;
	int err, depth;

	vctx = (verify_context *)ctx;

	err_cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);

	X509_NAME_oneline(X509_get_subject_name(err_cert), vctx->certdesc, sizeof(vctx->certdesc));

	if (!ok && depth >= 6)
	{
		X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_CHAIN_TOO_LONG);
	}

	switch (ctx->error)
	{
	case X509_V_ERR_INVALID_PURPOSE:
	case X509_V_ERR_CERT_HAS_EXPIRED:
	case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
		err = X509_V_OK;
		X509_STORE_CTX_set_error(ctx, X509_V_OK);
		ok = 1;
		break;

	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
		/*
			In this case, don't reset err to X509_V_OK, so that it can be reported,
			although we do return 1, so that the digest will still be checked
		*/
		ok = 1;
		break;

	default:
		break;
	}

	if (ok && vctx->err == X509_V_OK)
		vctx->err = err;
	return ok;
}

static int pk7_verify(X509_STORE *cert_store, PKCS7 *p7, BIO *detached, char *ebuf, int ebufsize)
{
	PKCS7_SIGNER_INFO *si;
	verify_context vctx;
	BIO *p7bio=NULL;
	char readbuf[1024*4];
	int res = 1;
	int i;
	STACK_OF(PKCS7_SIGNER_INFO) *sk;

	vctx.err = X509_V_OK;
	ebuf[0] = 0;

	OpenSSL_add_all_algorithms();

	EVP_add_digest(EVP_md5());
	EVP_add_digest(EVP_sha1());

	ERR_load_crypto_strings();

	ERR_clear_error();

	X509_VERIFY_PARAM_set_flags(cert_store->param, X509_V_FLAG_CB_ISSUER_CHECK);
	X509_STORE_set_verify_cb_func(cert_store, verify_callback);

	p7bio = PKCS7_dataInit(p7, detached);

	/* We now have to 'read' from p7bio to calculate digests etc. */
	while (BIO_read(p7bio, readbuf, sizeof(readbuf)) > 0)
		;

	/* We can now verify signatures */
	sk = PKCS7_get_signer_info(p7);
	if (sk == NULL)
	{
		/* there are no signatures on this data */
		res = 0;
		fz_strlcpy(ebuf, "No signatures", ebufsize);
		goto exit;
	}

	for (i=0; i<sk_PKCS7_SIGNER_INFO_num(sk); i++)
	{
		int rc;
		si = sk_PKCS7_SIGNER_INFO_value(sk, i);
		rc = PKCS7_dataVerify(cert_store, &vctx.x509_ctx, p7bio,p7, si);
		if (rc <= 0 || vctx.err != X509_V_OK)
		{
			char tbuf[120];

			if (rc <= 0)
			{
				fz_strlcpy(ebuf, ERR_error_string(ERR_get_error(), tbuf), ebufsize);
			}
			else
			{
				/* Error while checking the certificate chain */
				snprintf(ebuf, ebufsize, "%s(%d): %s", X509_verify_cert_error_string(vctx.err), vctx.err, vctx.certdesc);
			}

			res = 0;
			goto exit;
		}
	}

exit:
	X509_STORE_CTX_cleanup(&vctx.x509_ctx);
	ERR_free_strings();

	return res;
}

static unsigned char adobe_ca[] =
{
#include "gen_adobe_ca.h"
};

static int verify_sig(char *sig, int sig_len, char *file, int (*byte_range)[2], int byte_range_len, char *ebuf, int ebufsize)
{
	PKCS7 *pk7sig = NULL;
	PKCS7 *pk7cert = NULL;
	X509_STORE *st = NULL;
	BIO *bsig = NULL;
	BIO *bcert = NULL;
	BIO *bdata = NULL;
	BIO *bsegs = NULL;
	STACK_OF(X509) *certs = NULL;
	int t;
	int res = 0;

	bsig = BIO_new_mem_buf(sig, sig_len);
	pk7sig = d2i_PKCS7_bio(bsig, NULL);
	if (pk7sig == NULL)
		goto exit;

	bdata = BIO_new(BIO_s_file());
	BIO_read_filename(bdata, file);

	bsegs = BIO_new(BIO_f_segments());
	if (bsegs == NULL)
		goto exit;

	bsegs->next_bio = bdata;
	BIO_set_segments(bsegs, byte_range, byte_range_len);

	/* Find the certificates in the pk7 file */
	bcert = BIO_new_mem_buf(adobe_ca, sizeof(adobe_ca));
	pk7cert = d2i_PKCS7_bio(bcert, NULL);
	if (pk7cert == NULL)
		goto exit;

	t = OBJ_obj2nid(pk7cert->type);
	switch (t)
	{
	case NID_pkcs7_signed:
		certs = pk7cert->d.sign->cert;
		break;

	case NID_pkcs7_signedAndEnveloped:
		certs = pk7cert->d.sign->cert;
		break;

	default:
		break;
	}

	st = X509_STORE_new();
	if (st == NULL)
		goto exit;

	/* Add the certificates to the store */
	if (certs != NULL)
	{
		int i, n = sk_X509_num(certs);

		for (i = 0; i < n; i++)
		{
			X509 *c = sk_X509_value(certs, i);
			X509_STORE_add_cert(st, c);
		}
	}

	res = pk7_verify(st, pk7sig, bsegs, ebuf, ebufsize);

exit:
	BIO_free(bsig);
	BIO_free(bdata);
	BIO_free(bsegs);
	BIO_free(bcert);
	PKCS7_free(pk7sig);
	PKCS7_free(pk7cert);
	X509_STORE_free(st);

	return res;
}

int pdf_check_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget, char *file, char *ebuf, int ebufsize)
{
	int (*byte_range)[2] = NULL;
	int byte_range_len;
	char *contents = NULL;
	int contents_len;
	int res = 0;

	fz_var(byte_range);
	fz_var(res);
	fz_try(ctx);
	{
		byte_range_len = pdf_signature_widget_byte_range(doc, widget, NULL);
		if (byte_range_len)
		{
			byte_range = fz_calloc(ctx, byte_range_len, sizeof(*byte_range));
			pdf_signature_widget_byte_range(doc, widget, byte_range);
		}

		contents_len = pdf_signature_widget_contents(doc, widget, &contents);
		if (byte_range && contents)
		{
			res = verify_sig(contents, contents_len, file, byte_range, byte_range_len, ebuf, ebufsize);
		}
		else
		{
			res = 0;
			fz_strlcpy(ebuf, "Not signed", ebufsize);
		}

	}
	fz_always(ctx)
	{
		fz_free(ctx, byte_range);
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

#else /* HAVE_OPENSSL */

int pdf_check_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget, char *file, char *ebuf, int ebufsize)
{
	fz_strlcpy(ebuf, "This version of MuPDF was built without signature support", ebufsize);
	return 0;
}

#endif /* HAVE_OPENSSL */
