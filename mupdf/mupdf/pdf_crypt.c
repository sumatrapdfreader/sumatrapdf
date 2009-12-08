#include "fitz.h"
#include "mupdf.h"

/*
 * Create crypt object for decrypting strings and streams
 * given the Encryption and ID objects.
 */

fz_error
pdf_newcrypt(pdf_crypt **cryptp, fz_obj *dict, fz_obj *id)
{
	pdf_crypt *crypt;
	fz_error error;
	fz_obj *obj;

	crypt = fz_malloc(sizeof(pdf_crypt));
	memset(crypt, 0x00, sizeof(pdf_crypt));
	crypt->cf = nil;

	/*
	 * Common to all security handlers (PDF 1.7 table 3.18)
	 */

	obj = fz_dictgets(dict, "Filter");
	if (!fz_isname(obj))
	{
		pdf_freecrypt(crypt);
		return fz_throw("unspecified encryption handler");
	}
	if (strcmp(fz_toname(obj), "Standard") != 0)
	{
		pdf_freecrypt(crypt);
		return fz_throw("unknown encryption handler: '%s'", fz_toname(obj));
	}

	crypt->v = 0;
	obj = fz_dictgets(dict, "V");
	if (fz_isint(obj))
		crypt->v = fz_toint(obj);
	if (crypt->v != 1 && crypt->v != 2 && crypt->v != 4)
	{
		pdf_freecrypt(crypt);
		return fz_throw("unknown encryption version");
	}

	crypt->length = 40;
	if (crypt->v == 2 || crypt->v == 4)
	{
		obj = fz_dictgets(dict, "Length");
		if (fz_isint(obj))
			crypt->length = fz_toint(obj);

		/* work-around for pdf generators that assume length is in bytes */
		if (crypt->length < 40)
			crypt->length = crypt->length * 8;

		if (crypt->length % 8 != 0)
		{
			pdf_freecrypt(crypt);
			return fz_throw("invalid encryption key length: %d", crypt->length);
		}
		if (crypt->length > 256)
		{
			pdf_freecrypt(crypt);
			return fz_throw("invalid encryption key length: %d", crypt->length);
		}
	}

	if (crypt->v == 1 || crypt->v == 2)
	{
		crypt->stmf.method = PDF_CRYPT_RC4;
		crypt->stmf.length = crypt->length;

		crypt->strf.method = PDF_CRYPT_RC4;
		crypt->strf.length = crypt->length;
	}

	if (crypt->v == 4)
	{
		crypt->stmf.method = PDF_CRYPT_NONE;
		crypt->stmf.length = crypt->length;

		crypt->strf.method = PDF_CRYPT_NONE;
		crypt->strf.length = crypt->length;

		obj = fz_dictgets(dict, "CF");
		if (fz_isdict(obj))
		{
			crypt->cf = fz_keepobj(obj);

			obj = fz_dictgets(dict, "StmF");
			if (fz_isname(obj))
			{
				/* should verify that it is either Identity or StdCF */
				obj = fz_dictgets(crypt->cf, fz_toname(obj));
				if (fz_isdict(obj))
				{
					error = pdf_parsecryptfilter(&crypt->stmf, obj, crypt->length);
					if (error)
					{
						pdf_freecrypt(crypt);
						return fz_rethrow(error, "cannot parse stream crypt filter");
					}
				}
			}

			obj = fz_dictgets(dict, "StrF");
			if (fz_isname(obj))
			{
				/* should verify that it is either Identity or StdCF */
				obj = fz_dictgets(crypt->cf, fz_toname(obj));
				if (fz_isdict(obj))
				{
					error = pdf_parsecryptfilter(&crypt->strf, obj, crypt->length);
					if (error)
					{
						pdf_freecrypt(crypt);
						return fz_rethrow(error, "cannot parse string crypt filter");
					}
				}
			}
		}
	}

	/*
	 * Standard security handler (PDF 1.7 table 3.19)
	 */

	obj = fz_dictgets(dict, "R");
	if (fz_isint(obj))
		crypt->r = fz_toint(obj);
	else
	{
		pdf_freecrypt(crypt);
		return fz_throw("encryption dictionary missing revision value");
	}

	obj = fz_dictgets(dict, "O");
	if (fz_isstring(obj) && fz_tostrlen(obj) == 32)
		memcpy(crypt->o, fz_tostrbuf(obj), 32);
	else
	{
		pdf_freecrypt(crypt);
		return fz_throw("encryption dictionary missing owner password");
	}

	obj = fz_dictgets(dict, "U");
	if (fz_isstring(obj) && fz_tostrlen(obj) == 32)
		memcpy(crypt->u, fz_tostrbuf(obj), 32);
	else
	{
		pdf_freecrypt(crypt);
		return fz_throw("encryption dictionary missing user password");
	}

	obj = fz_dictgets(dict, "P");
	if (fz_isint(obj))
		crypt->p = fz_toint(obj);
	else
	{
		pdf_freecrypt(crypt);
		return fz_throw("encryption dictionary missing permissions value");
	}

	crypt->encryptmetadata = 1;
	obj = fz_dictgets(dict, "EncryptMetadata");
	if (fz_isbool(obj))
		crypt->encryptmetadata = fz_tobool(obj);

	/*
	 * Extract file identifier string
	 */

	crypt->idlength = 0;

	if (fz_isarray(id) && fz_arraylen(id) == 2)
	{
		obj = fz_arrayget(id, 0);
		if (fz_isstring(obj))
		{
			if (fz_tostrlen(obj) <= sizeof(crypt->idstring))
			{
				memcpy(crypt->idstring, fz_tostrbuf(obj), fz_tostrlen(obj));
				crypt->idlength = fz_tostrlen(obj);
			}
		}
	}
	else
		fz_warn("missing file identifier, may not be able to do decryption");

#if 0
	{
		int i;
		printf("crypt: v=%d length=%d\n", crypt->v, crypt->length);
		printf("crypt: stmf method=%d length=%d\n", crypt->stmf.method, crypt->stmf.length);
		printf("crypt: strf method=%d length=%d\n", crypt->strf.method, crypt->strf.length);
		printf("crypt: r=%d\n", crypt->r);
		printf("crypt: o=<"); for (i = 0; i < 32; i++) printf("%02X", crypt->o[i]); printf(">\n");
		printf("crypt: u=<"); for (i = 0; i < 32; i++) printf("%02X", crypt->u[i]); printf(">\n");
		printf("crypt: p=0x%08X\n", crypt->p);
	}
#endif

	*cryptp = crypt;
	return fz_okay;
}

void
pdf_freecrypt(pdf_crypt *crypt)
{
	if (crypt->cf) fz_dropobj(crypt->cf);
	fz_free(crypt);
}

/*
 * Parse a CF dictionary entry (PDF 1.7 table 3.22)
 */

fz_error
pdf_parsecryptfilter(pdf_cryptfilter *cf, fz_obj *dict, int defaultlength)
{
	fz_obj *obj;

	cf->method = PDF_CRYPT_NONE;
	cf->length = defaultlength;

	obj = fz_dictgets(dict, "CFM");
	if (fz_isname(obj))
	{
		if (!strcmp(fz_toname(obj), "None"))
			cf->method = PDF_CRYPT_NONE;
		else if (!strcmp(fz_toname(obj), "V2"))
			cf->method = PDF_CRYPT_RC4;
		else if (!strcmp(fz_toname(obj), "AESV2"))
			cf->method = PDF_CRYPT_AESV2;
		else
			fz_throw("unknown encryption method: %s", fz_toname(obj));
	}

	obj = fz_dictgets(dict, "Length");
	if (fz_isint(obj))
		cf->length = fz_toint(obj);

	if ((cf->length % 8) != 0)
		return fz_throw("invalid key length: %d", cf->length);

	return fz_okay;
}

/*
 * Compute an encryption key (PDF 1.7 algorithm 3.2)
 */

static const unsigned char padding[32] =
{
	0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41,
	0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
	0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80,
	0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};

static void
pdf_computeencryptionkey(pdf_crypt *crypt, unsigned char *password, int pwlen, unsigned char *key)
{
	unsigned char buf[32];
	unsigned int p;
	int i, n;
	fz_md5 md5;

	n = crypt->length / 8;

	/* Step 1 - copy and pad password string */
	if (pwlen > 32)
		pwlen = 32;
	memcpy(buf, password, pwlen);
	memcpy(buf + pwlen, padding, 32 - pwlen);

	/* Step 2 - init md5 and pass value of step 1 */
	fz_md5init(&md5);
	fz_md5update(&md5, buf, 32);

	/* Step 3 - pass O value */
	fz_md5update(&md5, crypt->o, 32);

	/* Step 4 - pass P value as unsigned int, low-order byte first */
	p = (unsigned int) crypt->p;
	buf[0] = (p) & 0xFF;
	buf[1] = (p >> 8) & 0xFF;
	buf[2] = (p >> 16) & 0xFF;
	buf[3] = (p >> 24) & 0xFF;
	fz_md5update(&md5, buf, 4);

	/* Step 5 - pass first element of ID array */
	fz_md5update(&md5, crypt->idstring, crypt->idlength);

	/* Step 6 (revision 4 or greater) - if metadata is not encrypted pass 0xFFFFFFFF */
	if (crypt->r >= 4)
	{
		if (!crypt->encryptmetadata)
		{
			buf[0] = 0xFF;
			buf[1] = 0xFF;
			buf[2] = 0xFF;
			buf[3] = 0xFF;
			fz_md5update(&md5, buf, 4);
		}
	}

	/* Step 7 - finish the hash */
	fz_md5final(&md5, buf);

	/* Step 8 (revision 3 or greater) - do some voodoo 50 times */
	if (crypt->r >= 3)
	{
		for (i = 0; i < 50; i++)
		{
			fz_md5init(&md5);
			fz_md5update(&md5, buf, n);
			fz_md5final(&md5, buf);
		}
	}

	/* Step 9 - the key is the first 'n' bytes of the result */
	memcpy(key, buf, n);
}

/*
 * Computing the user password (PDF 1.7 algorithm 3.4 and 3.5)
 * Also save the generated key for decrypting objects and streams in crypt->key.
 */

static void
pdf_computeuserpassword(pdf_crypt *crypt, unsigned char *password, int pwlen, unsigned char *output)
{
	if (crypt->r == 2)
	{
		fz_arc4 arc4;

		pdf_computeencryptionkey(crypt, password, pwlen, crypt->key);
		fz_arc4init(&arc4, crypt->key, crypt->length / 8);
		fz_arc4encrypt(&arc4, output, padding, 32);
	}

	if (crypt->r >= 3)
	{
		unsigned char xor[32];
		unsigned char digest[16];
		fz_md5 md5;
		fz_arc4 arc4;
		int i, x, n;

		n = crypt->length / 8;

		pdf_computeencryptionkey(crypt, password, pwlen, crypt->key);

		fz_md5init(&md5);
		fz_md5update(&md5, padding, 32);
		fz_md5update(&md5, crypt->idstring, crypt->idlength);
		fz_md5final(&md5, digest);

		fz_arc4init(&arc4, crypt->key, n);
		fz_arc4encrypt(&arc4, output, digest, 16);

		for (x = 1; x <= 19; x++)
		{
			for (i = 0; i < n; i++)
				xor[i] = crypt->key[i] ^ x;
			fz_arc4init(&arc4, xor, n);
			fz_arc4encrypt(&arc4, output, output, 16);
		}

		memcpy(output + 16, padding, 16);
	}
}

/*
 * Authenticating the user password (PDF 1.7 algorithm 3.6)
 * This also has the side effect of saving a key generated
 * from the password for decrypting objects and streams.
 */

static int
pdf_authenticateuserpassword(pdf_crypt *crypt, unsigned char *password, int pwlen)
{
	unsigned char output[32];
	pdf_computeuserpassword(crypt, password, pwlen, output);
	if (crypt->r == 2)
		return memcmp(output, crypt->u, 32) == 0;
	if (crypt->r >= 3)
		return memcmp(output, crypt->u, 16) == 0;
	return 0;
}

/*
 * Authenticating the owner password (PDF 1.7 algorithm 3.7)
 * Generates the user password from the owner password
 * and calls pdf_authenticateuserpassword.
 */

static int
pdf_authenticateownerpassword(pdf_crypt *crypt, unsigned char *ownerpass, int pwlen)
{
	unsigned char pwbuf[32];
	unsigned char key[32];
	unsigned char xor[32];
	unsigned char userpass[32];
	int i, n, x;
	fz_md5 md5;
	fz_arc4 arc4;

	n = crypt->length / 8;

	/* Step 1 -- steps 1 to 4 of PDF 1.7 algorithm 3.3 */

	/* copy and pad password string */
	if (pwlen > 32)
		pwlen = 32;
	memcpy(pwbuf, ownerpass, pwlen);
	memcpy(pwbuf + pwlen, padding, 32 - pwlen);

	/* take md5 hash of padded password */
	fz_md5init(&md5);
	fz_md5update(&md5, pwbuf, 32);
	fz_md5final(&md5, key);

	/* do some voodoo 50 times (Revision 3 or greater) */
	if (crypt->r >= 3)
	{
		for (i = 0; i < 50; i++)
		{
			fz_md5init(&md5);
			fz_md5update(&md5, key, 16);
			fz_md5final(&md5, key);
		}
	}

	/* Step 2 (Revision 2) */
	if (crypt->r == 2)
	{
		fz_arc4init(&arc4, key, n);
		fz_arc4encrypt(&arc4, userpass, crypt->o, 32);
	}

	/* Step 2 (Revision 3 or greater) */
	if (crypt->r >= 3)
	{
		memcpy(userpass, crypt->o, 32);
		for (x = 0; x < 20; x++)
		{
			for (i = 0; i < n; i++)
				xor[i] = key[i] ^ (19 - x);
			fz_arc4init(&arc4, xor, n);
			fz_arc4encrypt(&arc4, userpass, userpass, 32);
		}
	}

	return pdf_authenticateuserpassword(crypt, userpass, 32);
}


int
pdf_authenticatepassword(pdf_xref *xref, char *password)
{
	if (xref->crypt)
	{
		if (pdf_authenticateuserpassword(xref->crypt, (unsigned char *)password, strlen(password)))
			return 1;
		if (pdf_authenticateownerpassword(xref->crypt, (unsigned char *)password, strlen(password)))
			return 1;
		return 0;
	}
	return 1;
}

int
pdf_needspassword(pdf_xref *xref)
{
	if (!xref->crypt)
		return 0;
	if (pdf_authenticatepassword(xref, ""))
		return 0;
	return 1;
}

/*
 * PDF 1.7 algorithm 3.1
 *
 * Using the global encryption key that was generated from the
 * password, create a new key that is used to decrypt indivual
 * objects and streams. This key is based on the object and
 * generation numbers.
 */

static int
pdf_computeobjectkey(pdf_crypt *crypt, pdf_cryptfilter *cf, int num, int gen, unsigned char *key)
{
	fz_md5 md5;
	unsigned char message[5];

	fz_md5init(&md5);
	fz_md5update(&md5, crypt->key, crypt->length / 8);
	message[0] = (num) & 0xFF;
	message[1] = (num >> 8) & 0xFF;
	message[2] = (num >> 16) & 0xFF;
	message[3] = (gen) & 0xFF;
	message[4] = (gen >> 8) & 0xFF;
	fz_md5update(&md5, message, 5);

	if (cf->method == PDF_CRYPT_AESV2)
		fz_md5update(&md5, (unsigned char *)"sAlT", 4);

	fz_md5final(&md5, key);

	if (crypt->length / 8 + 5 > 16)
		return 16;
	return crypt->length / 8 + 5;
}

/*
 * PDF 1.7 algorithm 3.1
 *
 * Decrypt all strings in obj modifying the data in-place.
 * Recurse through arrays and dictionaries, but do not follow
 * indirect references.
 */

static void
pdf_cryptobjimp(pdf_crypt *crypt, fz_obj *obj, unsigned char *key, int keylen)
{
	unsigned char *s;
	int i, n;

	if (fz_isindirect(obj))
		return;

	if (fz_isstring(obj))
	{
		s = (unsigned char *) fz_tostrbuf(obj);
		n = fz_tostrlen(obj);

		if (crypt->strf.method == PDF_CRYPT_RC4)
		{
			fz_arc4 arc4;
			fz_arc4init(&arc4, key, keylen);
			fz_arc4encrypt(&arc4, s, s, n);
		}

		if (crypt->strf.method == PDF_CRYPT_AESV2)
		{
			if (n >= 32)
			{
				unsigned char iv[16];
				fz_aes aes;
				memcpy(iv, s, 16);
				aes_setkey_dec(&aes, key, keylen * 8);
				aes_crypt_cbc(&aes, AES_DECRYPT, n - 16, iv, s + 16, s);
				obj->u.s.len -= 16; /* delete space used for iv */
				obj->u.s.len -= s[n - 17]; /* delete padding bytes at end */
			}
		}
	}

	else if (fz_isarray(obj))
	{
		n = fz_arraylen(obj);
		for (i = 0; i < n; i++)
		{
			pdf_cryptobjimp(crypt, fz_arrayget(obj, i), key, keylen);
		}
	}

	else if (fz_isdict(obj))
	{
		n = fz_dictlen(obj);
		for (i = 0; i < n; i++)
		{
			pdf_cryptobjimp(crypt, fz_dictgetval(obj, i), key, keylen);
		}
	}
}

void
pdf_cryptobj(pdf_crypt *crypt, fz_obj *obj, int num, int gen)
{
	unsigned char key[16];
	int len;

	len = pdf_computeobjectkey(crypt, &crypt->strf, num, gen, key);

	pdf_cryptobjimp(crypt, obj, key, len);
}

/*
 * PDF 1.7 algorithm 3.1
 *
 * Create filter suitable for de/encrypting a stream.
 */
fz_filter *
pdf_cryptstream(pdf_crypt * crypt, pdf_cryptfilter * stmf, int num, int gen)
{
	unsigned char key[16];
	int len;

	len = pdf_computeobjectkey(crypt, stmf, num, gen, key);

	if (stmf->method == PDF_CRYPT_RC4)
		return fz_newarc4filter(key, len);

	if (stmf->method == PDF_CRYPT_AESV2)
		return fz_newaesdfilter(key, len);

	return fz_newcopyfilter();
}

