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
#include "openssl/pkcs12.h"

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
	if (bdata == NULL)
		goto exit;
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

typedef struct pdf_designated_name_openssl_s
{
	pdf_designated_name base;
	fz_context *ctx;
	char buf[8192];
} pdf_designated_name_openssl;

struct pdf_signer_s
{
	fz_context *ctx;
	int refs;
	X509 *x509;
	EVP_PKEY *pkey;
};

void pdf_free_designated_name(pdf_designated_name *dn)
{
	if (dn)
		fz_free(((pdf_designated_name_openssl *)dn)->ctx, dn);
}


static void add_from_bags(X509 **pX509, EVP_PKEY **pPkey, STACK_OF(PKCS12_SAFEBAG) *bags, const char *pw);

static void add_from_bag(X509 **pX509, EVP_PKEY **pPkey, PKCS12_SAFEBAG *bag, const char *pw)
{
	EVP_PKEY *pkey = NULL;
	X509 *x509 = NULL;
	PKCS8_PRIV_KEY_INFO *p8 = NULL;
	switch (M_PKCS12_bag_type(bag))
	{
	case NID_keyBag:
		p8 = bag->value.keybag;
		pkey = EVP_PKCS82PKEY(p8);
		break;

	case NID_pkcs8ShroudedKeyBag:
		p8 = PKCS12_decrypt_skey(bag, pw, (int)strlen(pw));
		if (p8)
		{
			pkey = EVP_PKCS82PKEY(p8);
			PKCS8_PRIV_KEY_INFO_free(p8);
		}
		break;

	case NID_certBag:
		if (M_PKCS12_cert_bag_type(bag) == NID_x509Certificate)
			x509 = PKCS12_certbag2x509(bag);
		break;

	case NID_safeContentsBag:
		add_from_bags(pX509, pPkey, bag->value.safes, pw);
		break;
	}

	if (pkey)
	{
		if (!*pPkey)
			*pPkey = pkey;
		else
			EVP_PKEY_free(pkey);
	}

	if (x509)
	{
		if (!*pX509)
			*pX509 = x509;
		else
			X509_free(x509);
	}
}

static void add_from_bags(X509 **pX509, EVP_PKEY **pPkey, STACK_OF(PKCS12_SAFEBAG) *bags, const char *pw)
{
	int i;

	for (i = 0; i < sk_PKCS12_SAFEBAG_num(bags); i++)
		add_from_bag(pX509, pPkey, sk_PKCS12_SAFEBAG_value(bags, i), pw);
}

pdf_signer *pdf_read_pfx(fz_context *ctx, const char *pfile, const char *pw)
{
	BIO *pfxbio = NULL;
	PKCS12 *p12 = NULL;
	STACK_OF(PKCS7) *asafes;
	pdf_signer *signer = NULL;
	int i;

	fz_var(pfxbio);
	fz_var(p12);
	fz_var(signer);
	fz_try(ctx)
	{
		signer = fz_malloc_struct(ctx, pdf_signer);
		signer->ctx = ctx;
		signer->refs = 1;

		OpenSSL_add_all_algorithms();

		EVP_add_digest(EVP_md5());
		EVP_add_digest(EVP_sha1());

		ERR_load_crypto_strings();

		ERR_clear_error();

		pfxbio = BIO_new_file(pfile, "r");
		if (pfxbio == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Can't open pfx file: %s", pfile);

		p12 = d2i_PKCS12_bio(pfxbio, NULL);
		if (p12 == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Invalid pfx file: %s", pfile);

		asafes = PKCS12_unpack_authsafes(p12);
		if (asafes == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Invalid pfx file: %s", pfile);

		/* Nothing in this for loop can fz_throw */
		for (i = 0; i < sk_PKCS7_num(asafes); i++)
		{
			PKCS7 *p7;
			STACK_OF(PKCS12_SAFEBAG) *bags;
			int bagnid;

			p7 = sk_PKCS7_value(asafes, i);
			bagnid = OBJ_obj2nid(p7->type);
			switch (bagnid)
			{
			case NID_pkcs7_data:
				bags = PKCS12_unpack_p7data(p7);
				break;
			case NID_pkcs7_encrypted:
				bags = PKCS12_unpack_p7encdata(p7, pw, (int)strlen(pw));
				break;
			default:
				continue;
			}

			if (bags)
			{
				add_from_bags(&signer->x509, &signer->pkey, bags, pw);
				sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
			}
		}
		sk_PKCS7_pop_free (asafes, PKCS7_free);

		if (signer->pkey == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to obtain public key");

		if (signer->x509 == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to obtain certificate");
	}
	fz_always(ctx)
	{
		BIO_free(pfxbio);
		PKCS12_free(p12);
	}
	fz_catch(ctx)
	{
		pdf_drop_signer(signer);
		fz_rethrow(ctx);
	}

	return signer;
}

pdf_signer *pdf_keep_signer(pdf_signer *signer)
{
	if (signer)
		signer->refs++;

	return signer;
}

void pdf_drop_signer(pdf_signer *signer)
{
	if (signer)
	{
		if (--signer->refs == 0)
		{
			X509_free(signer->x509);
			EVP_PKEY_free(signer->pkey);
			fz_free(signer->ctx, signer);
		}
	}
}

pdf_designated_name *pdf_signer_designated_name(pdf_signer *signer)
{
	fz_context *ctx = signer->ctx;
	pdf_designated_name_openssl *dn = fz_malloc_struct(ctx, pdf_designated_name_openssl);
	char *p;

	dn->ctx = ctx;
	X509_NAME_oneline(X509_get_subject_name(signer->x509), dn->buf, sizeof(dn->buf));
	p = strstr(dn->buf, "/CN=");
	if (p) dn->base.cn = p+4;
	p = strstr(dn->buf, "/O=");
	if (p) dn->base.o = p+3;
	p = strstr(dn->buf, "/OU=");
	if (p) dn->base.ou = p+4;
	p = strstr(dn->buf, "/emailAddress=");
	if (p) dn->base.email = p+14;
	p = strstr(dn->buf, "/C=");
	if (p) dn->base.c = p+3;

	for (p = dn->buf; *p; p++)
		if (*p == '/')
			*p = 0;

	return (pdf_designated_name *)dn;
}

void pdf_write_digest(pdf_document *doc, char *filename, pdf_obj *byte_range, int digest_offset, int digest_length, pdf_signer *signer)
{
	fz_context *ctx = doc->ctx;
	BIO *bdata = NULL;
	BIO *bsegs = NULL;
	BIO *bp7in = NULL;
	BIO *bp7 = NULL;
	PKCS7 *p7 = NULL;
	PKCS7_SIGNER_INFO *si;
	FILE *f = NULL;

	int (*brange)[2] = NULL;
	int brange_len = pdf_array_len(byte_range)/2;

	fz_var(bdata);
	fz_var(bsegs);
	fz_var(bp7in);
	fz_var(bp7);
	fz_var(p7);
	fz_var(f);

	fz_try(ctx)
	{
		unsigned char *p7_ptr;
		int p7_len;
		int i;

		brange = fz_calloc(ctx, brange_len, sizeof(*brange));
		for (i = 0; i < brange_len; i++)
		{
			brange[i][0] = pdf_to_int(pdf_array_get(byte_range, 2*i));
			brange[i][1] = pdf_to_int(pdf_array_get(byte_range, 2*i+1));
		}

		bdata = BIO_new(BIO_s_file());
		if (bdata == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create file BIO");
		BIO_read_filename(bdata, filename);

		bsegs = BIO_new(BIO_f_segments());
		if (bsegs == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create segment filter");

		bsegs->next_bio = bdata;
		BIO_set_segments(bsegs, brange, brange_len);

		p7 = PKCS7_new();
		if (p7 == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create p7 object");

		PKCS7_set_type(p7, NID_pkcs7_signed);
		si = PKCS7_add_signature(p7, signer->x509, signer->pkey, EVP_sha1());
		if (si == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to add signature");

		PKCS7_add_signed_attribute(si, NID_pkcs9_contentType, V_ASN1_OBJECT, OBJ_nid2obj(NID_pkcs7_data));
		PKCS7_add_certificate(p7, signer->x509);

		PKCS7_content_new(p7, NID_pkcs7_data);
		PKCS7_set_detached(p7, 1);

		bp7in = PKCS7_dataInit(p7, NULL);
		if (bp7in == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to write to digest");

		while(1)
		{
			char buf[4096];
			int n = BIO_read(bsegs, buf, sizeof(buf));
			if (n <= 0)
				break;
			BIO_write(bp7in, buf, n);
		}

		if (!PKCS7_dataFinal(p7, bp7in))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to write to digest");

		BIO_free(bsegs);
		bsegs = NULL;
		BIO_free(bdata);
		bdata = NULL;

		bp7 = BIO_new(BIO_s_mem());
		if (bp7 == NULL || !i2d_PKCS7_bio(bp7, p7))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create memory buffer for digest");

		p7_len = BIO_get_mem_data(bp7, &p7_ptr);
		if (p7_len*2 + 2 > digest_length)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Insufficient space for digest");

		f = fopen(filename, "rb+");
		if (f == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to write digest");

		fseek(f, digest_offset+1, SEEK_SET);

		for (i = 0; i < p7_len; i++)
			fprintf(f, "%02x", p7_ptr[i]);
	}
	fz_always(ctx)
	{
		PKCS7_free(p7);
		BIO_free(bsegs);
		BIO_free(bdata);
		BIO_free(bp7in);
		BIO_free(bp7);
		if (f)
			fclose(f);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

int pdf_check_signature(pdf_document *doc, pdf_widget *widget, char *file, char *ebuf, int ebufsize)
{
	fz_context *ctx = doc->ctx;
	int (*byte_range)[2] = NULL;
	int byte_range_len;
	char *contents = NULL;
	int contents_len;
	int res = 0;
	pdf_unsaved_sig *usig;

	for (usig = doc->unsaved_sigs; usig; usig = usig->next)
	{
		if (usig->field == ((pdf_annot *)widget)->obj)
		{
			fz_strlcpy(ebuf, "Signed but document yet to be saved", ebufsize);
			if (ebufsize > 0)
				ebuf[ebufsize-1] = 0;
			return 0;
		}
	}

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

void pdf_sign_signature(pdf_document *doc, pdf_widget *widget, const char *sigfile, const char *password)
{
	fz_context *ctx = doc->ctx;
	pdf_signer *signer = pdf_read_pfx(ctx, sigfile, password);
	pdf_designated_name *dn = NULL;
	fz_buffer *fzbuf = NULL;

	fz_try(ctx)
	{
		char *dn_str;
		pdf_obj *wobj = ((pdf_annot *)widget)->obj;
		fz_rect rect = fz_empty_rect;

		pdf_signature_set_value(doc, wobj, signer);

		pdf_to_rect(ctx, pdf_dict_gets(wobj, "Rect"), &rect);
		/* Create an appearance stream only if the signature is intended to be visible */
		if (!fz_is_empty_rect(&rect))
		{
			dn = pdf_signer_designated_name(signer);
			fzbuf = fz_new_buffer(ctx, 256);
			if (!dn->cn)
				fz_throw(ctx, FZ_ERROR_GENERIC, "Certificate has no common name");

			fz_buffer_printf(ctx, fzbuf, "cn=%s", dn->cn);

			if (dn->o)
				fz_buffer_printf(ctx, fzbuf, ", o=%s", dn->o);

			if (dn->ou)
				fz_buffer_printf(ctx, fzbuf, ", ou=%s", dn->ou);

			if (dn->email)
				fz_buffer_printf(ctx, fzbuf, ", email=%s", dn->email);

			if (dn->c)
				fz_buffer_printf(ctx, fzbuf, ", c=%s", dn->c);

			(void)fz_buffer_storage(ctx, fzbuf, (unsigned char **) &dn_str);
			pdf_set_signature_appearance(doc, (pdf_annot *)widget, dn->cn, dn_str, NULL);
		}
	}
	fz_always(ctx)
	{
		pdf_drop_signer(signer);
		pdf_free_designated_name(dn);
		fz_drop_buffer(ctx, fzbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

int pdf_signatures_supported(void)
{
	return 1;
}

#else /* HAVE_OPENSSL */

int pdf_check_signature(pdf_document *doc, pdf_widget *widget, char *file, char *ebuf, int ebufsize)
{
	fz_strlcpy(ebuf, "This version of MuPDF was built without signature support", ebufsize);
	return 0;
}

void pdf_sign_signature(pdf_document *doc, pdf_widget *widget, const char *sigfile, const char *password)
{
}

pdf_signer *pdf_keep_signer(pdf_signer *signer)
{
	return NULL;
}

void pdf_drop_signer(pdf_signer *signer)
{
}

void pdf_write_digest(pdf_document *doc, char *filename, pdf_obj *byte_range, int digest_offset, int digest_length, pdf_signer *signer)
{
}

int pdf_signatures_supported(void)
{
	return 0;
}

#endif /* HAVE_OPENSSL */
