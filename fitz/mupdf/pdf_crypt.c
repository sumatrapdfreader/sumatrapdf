#include "fitz.h"
#include "mupdf.h"

static const unsigned char padding[32] =
{
	0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41,
	0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
	0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80,
	0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};

static void voodoo50(unsigned char *buf, int n)
{
	fz_md5 md5;
	int i;

	for (i = 0; i < 50; i++)
	{
		fz_md5init(&md5);
		fz_md5update(&md5, buf, n);
		fz_md5final(&md5, buf);
	}
}

static void voodoo19(unsigned char *data, int ndata, unsigned char *key, int nkey)
{
	fz_arc4 arc4;
	unsigned char keybuf[16];
	int i, k;

	for (i = 1; i <= 19; i++)
	{
		for (k = 0; k < nkey; k++)
			keybuf[k] = key[k] ^ (unsigned char)i;
		fz_arc4init(&arc4, keybuf, nkey);
		fz_arc4encrypt(&arc4, data, data, ndata);
	}
}

static void padpassword(unsigned char *buf, unsigned char *pw, int pwlen)
{
	if (pwlen > 32)
		pwlen = 32;
	memcpy(buf, pw, pwlen);
	memcpy(buf + pwlen, padding, 32 - pwlen);
}

static fz_error *
pdf_parsecryptfilt(fz_obj *filters, char *filter, char **method, int *length)
{
	fz_obj *cryptfilt;
	fz_obj *obj;

	cryptfilt = fz_dictgets(filters, filter);
	if (!fz_isdict(cryptfilt))
		goto cleanup;

	obj = fz_dictgets(cryptfilt, "CFM");
	*method = "None";
	if (fz_isname(obj))
		*method = fz_toname(obj);

	obj = fz_dictgets(cryptfilt, "Length");
	*length = 40;
	if (fz_isint(obj))
		*length = fz_toint(obj);

	/* Work-around to fix PDF-generators that assume that the
	   length is specified in bytes instead of in bits */
	if (*length < 40)
		if (*length * 8 >= 40 && *length * 8 <= 128)
			*length = *length * 8;

	return fz_okay;

cleanup:
	return fz_throw("corrupt encryption filter dictionary");
}

static fz_error *
pdf_parseencdict(pdf_crypt *crypt, fz_obj *enc)
{
	fz_obj *obj;
	fz_error *error = fz_okay;

	obj = fz_dictgets(enc, "Filter");
	if (!fz_isname(obj))
		goto cleanup;
	crypt->handler = fz_toname(obj);

	obj = fz_dictgets(enc, "V");
	crypt->v = 0;
	if (fz_isint(obj))
		crypt->v = fz_toint(obj);

	obj = fz_dictgets(enc, "R");
	if (!fz_isint(obj))
		goto cleanup;
	crypt->r = fz_toint(obj);

	if (crypt->v == 1)
	{
		crypt->len = 40;
	}
	else if (crypt->v >= 2 && crypt->v <= 3)
	{
		obj = fz_dictgets(enc, "Length");
		crypt->len = 40;
		if (fz_isint(obj))
			crypt->len = fz_toint(obj);
	}
	else if (crypt->v == 4)
	{
		fz_obj *cryptfilt;
		char *filt;

		cryptfilt = fz_dictgets(enc, "CF");
		if (cryptfilt && !fz_isdict(cryptfilt))
			goto cleanup;

		obj = fz_dictgets(enc, "StmF");
		filt = "Identity";
		if (fz_isname(obj))
			filt = fz_toname(obj);

		error = pdf_parsecryptfilt(cryptfilt, filt,
				&crypt->stmmethod, &crypt->stmlength);
		if (error)
			goto cleanup;

		obj = fz_dictgets(enc, "StrF");
		filt = "Identity";
		if (fz_isname(obj))
			filt = fz_toname(obj);

		error = pdf_parsecryptfilt(cryptfilt, filt,
				&crypt->strmethod, &crypt->strlength);
		if (error)
			goto cleanup;

		obj = fz_dictgets(enc, "EncryptMetadata");
		crypt->encryptedmeta = 1;
		if (fz_isbool(obj))
			crypt->encryptedmeta = fz_tobool(obj);
	}

	obj = fz_dictgets(enc, "O");
	if (!fz_isstring(obj) || fz_tostrlen(obj) != 32)
		goto cleanup;
	memcpy(crypt->o, fz_tostrbuf(obj), 32);

	obj = fz_dictgets(enc, "U");
	if (!fz_isstring(obj) || fz_tostrlen(obj) != 32)
		goto cleanup;
	memcpy(crypt->u, fz_tostrbuf(obj), 32);

	obj = fz_dictgets(enc, "P");
	if (!fz_isint(obj))
		goto cleanup;
	crypt->p = fz_toint(obj);

	return fz_okay;

cleanup:
	if (error)
		return fz_rethrow(error, "corrupt encryption dictionary");

	return fz_throw("corrupt encryption dictionary");
}


/*
 * Create crypt object for decrypting given the
 * Encoding dictionary and file ID
 */
fz_error *
pdf_newdecrypt(pdf_crypt **cp, fz_obj *enc, fz_obj *id)
{
	pdf_crypt *crypt;
	fz_error *error;
	fz_obj *obj;

	crypt = fz_malloc(sizeof(pdf_crypt));
	if (!crypt)
		return fz_throw("outofmem: crypt struct");

	memset(crypt->o, 0x00, sizeof(crypt->o));
	memset(crypt->u, 0x00, sizeof(crypt->u));
	crypt->p = 0;
	crypt->v = 0;
	crypt->r = 0;
	crypt->len = 0;
	crypt->handler = nil;
	crypt->stmmethod = nil;
	crypt->stmlength = 0;
	crypt->strmethod = nil;
	crypt->strlength = 0;
	crypt->encryptedmeta = 0;

	crypt->encrypt = fz_keepobj(enc);
	crypt->id = nil;

	memset(crypt->key, 0x00, sizeof(crypt->key));
	crypt->keylen = 0;

	error = pdf_parseencdict(crypt, enc);
	if (error)
	{
		pdf_dropcrypt(crypt);
		return fz_rethrow(error, "unable to to create decryptor");
	}

	if (strcmp(crypt->handler, "Standard") != 0)
	{
		char *handler = crypt->handler;
		pdf_dropcrypt(crypt);
		return fz_throw("unsupported security handler: %s\n", handler);
	}

	if (crypt->v == 4)
	{
		if (crypt->stmmethod && strcmp(crypt->stmmethod, "V2") != 0)
		{
			char *method = crypt->stmmethod;
			pdf_dropcrypt(crypt);
			return fz_throw("unsupported stream encryption method: %s\n", method);
		}

		if (crypt->strmethod && strcmp(crypt->strmethod, "V2") != 0)
		{
			char *method = crypt->strmethod;
			pdf_dropcrypt(crypt);
			return fz_throw("unsupported string encryption: %s\n", method);
		}

		if (crypt->stmlength != crypt->strlength)
		{
			int stmlength = crypt->stmlength;
			int strlength = crypt->strlength;
			pdf_dropcrypt(crypt);
			return fz_throw("unsupport encryption key lengths: %d vs. %d\n", stmlength, strlength);
		}

		crypt->len = crypt->stmlength;

		crypt->v = 2;
	}

	if (crypt->len % 8 != 0)
		goto cleanup;
	crypt->len = crypt->len / 8;

	if (crypt->v == 1 && crypt->len != 5) goto cleanup;
	if (crypt->v == 2 && crypt->len < 5) goto cleanup;
	if (crypt->v == 3 && (crypt->len < 5 || crypt->len > 16)) goto cleanup;
	if (crypt->v == 4 && (crypt->len < 5 || crypt->len > 16)) goto cleanup;

	if (crypt->v != 1 && crypt->v != 2)
	{
		pdf_dropcrypt(crypt);
		return fz_throw("unsupported encryption algorithm: %d", crypt->v);
	}

	if (!fz_isarray(id) || fz_arraylen(id) != 2)
		goto cleanup;
	obj = fz_arrayget(id, 0);
	if (!fz_isstring(obj))
		goto cleanup;
	crypt->id = fz_keepobj(obj);

	crypt->keylen = crypt->len + 5;
	if (crypt->keylen > 16)
		crypt->keylen = 16;

	memset(crypt->key, 0, 16);

	*cp = crypt;
	return fz_okay;

cleanup:
	pdf_dropcrypt(crypt);
	return fz_throw("corrupt encryption dictionary");
}

void
pdf_dropcrypt(pdf_crypt *crypt)
{
	if (crypt->encrypt) fz_dropobj(crypt->encrypt);
	if (crypt->id) fz_dropobj(crypt->id);
	fz_free(crypt);
}

static void
createobjkey(pdf_crypt *crypt, unsigned oid, unsigned gid, unsigned char *key)
{
	unsigned char message[5];
	fz_md5 md5;

	/* Algorithm 3.1 Encryption of data using an encryption key */

	fz_md5init(&md5);

	fz_md5update(&md5, crypt->key, crypt->len);

	message[0] = oid & 0xFF;
	message[1] = (oid >> 8) & 0xFF;
	message[2] = (oid >> 16) & 0xFF;

	message[3] = gid & 0xFF;
	message[4] = (gid >> 8) & 0xFF;

	fz_md5update(&md5, message, 5);

	fz_md5final(&md5, key);
}

/*
 * Algorithm 3.2 Computing an encryption key as of pdf 1.7
 */
static void
createkey(pdf_crypt *crypt, unsigned char *userpw, int pwlen)
{
	unsigned char buf[32];
	fz_md5 md5;

	/* Step 1 + 2 */
	fz_md5init(&md5);
	padpassword(buf, userpw, pwlen);
	fz_md5update(&md5, buf, 32);

	/* Step 3 */
	fz_md5update(&md5, crypt->o, 32);

	/* Step 4 */
	buf[0] = crypt->p & 0xFF;
	buf[1] = (crypt->p >> 8) & 0xFF;
	buf[2] = (crypt->p >> 16) & 0xFF;
	buf[3] = (crypt->p >> 24) & 0xFF;
	fz_md5update(&md5, buf, 4);

	/* Step 5 */
	fz_md5update(&md5, (unsigned char *) fz_tostrbuf(crypt->id),
		fz_tostrlen(crypt->id));

	/* Step 6 (rev 4 or later) */
	if (crypt->r >= 4 && !crypt->encryptedmeta)
	{
		memset(buf, 0xff, 4);
		fz_md5update(&md5, buf, 4);
	}

	/* Step 7 */
	fz_md5final(&md5, crypt->key);

	/* Step 8 (rev 3, or later) */
	if (crypt->r >= 3)
		voodoo50(crypt->key, crypt->len);

	/* Step 9: key is in crypt->key */
}

/*
 * Algorithm 3.3 Computing the O value
 */
static void
createowner(pdf_crypt *crypt, unsigned char *userpw, int userpwlen, unsigned char *ownerpw, int ownerpwlen)
{
	unsigned char buf[32];
	unsigned char key[16];
	fz_arc4 arc4;
	fz_md5 md5;

	/* Step 1 + 2 */
	if (ownerpwlen == 0)
	{
		ownerpw = userpw;
		ownerpwlen = userpwlen;
	}
	padpassword(buf, ownerpw, ownerpwlen);
	fz_md5init(&md5);
	fz_md5update(&md5, buf, 32);
	fz_md5final(&md5, key);

	/* Step 3 (rev 3 or later) */
	if (crypt->r >= 3)
		voodoo50(key, crypt->len);

	/* Step 4 */
	fz_arc4init(&arc4, key, crypt->len);

	/* Step 5 */
	padpassword(buf, userpw, ownerpwlen);

	/* Step 6 */
	fz_arc4encrypt(&arc4, buf, buf, 32);

	/* Step 7 (rev 3 or later) */
	if (crypt->r >= 3)
		voodoo19(buf, 32, key, crypt->len);

	/* Step 8 */
	memcpy(crypt->o, buf, 32);
}

/*
 * Algorithm 3.4 Computing the U value (rev 2)
 * Algorithm 3.5 Computing the U value (rev 3 or later)
 */
static void
createuser(pdf_crypt *crypt, unsigned char *userpw, int pwlen)
{
	unsigned char key[16];
	fz_arc4 arc4;
	fz_md5 md5;

	if (crypt->r == 2)
	{
		createkey(crypt, userpw, pwlen);
		fz_arc4init(&arc4, crypt->key, crypt->len);
		fz_arc4encrypt(&arc4, crypt->u, (unsigned char *) padding, 32);
	}

	if (crypt->r >= 3)
	{
		/* Step 1 */
		createkey(crypt, userpw, pwlen);

		/* Step 2 */
		fz_md5init(&md5);
		fz_md5update(&md5, (unsigned char *) padding, 32);

		/* Step 3 */
		fz_md5update(&md5, (unsigned char *) fz_tostrbuf(crypt->id),
			fz_tostrlen(crypt->id));
		fz_md5final(&md5, key);

		/* Step 4 */
		fz_arc4init(&arc4, crypt->key, crypt->len);
		fz_arc4encrypt(&arc4, key, key, 16);

		/* Step 5 */
		voodoo19(key, 16, crypt->key, crypt->len);

		/* Step 6 */
		memcpy(crypt->u, key, 16);
		memset(crypt->u + 16, 0, 16);
	}
}

/*
 * Create crypt object for encrypting, given passwords,
 * permissions, and file ID
 */
fz_error *
pdf_newencrypt(pdf_crypt **cp, char *userpw, char *ownerpw, int p, int n, fz_obj *id)
{
	fz_error *error;
	pdf_crypt *crypt;

	crypt = fz_malloc(sizeof(pdf_crypt));
	if (!crypt)
		return fz_throw("outofmem: crypt struct");

	crypt->encrypt = nil;
	crypt->id = fz_keepobj(fz_arrayget(id, 0));
	crypt->p = p;
	crypt->len = MIN(MAX(n / 8, 5), 16);
	crypt->keylen = MIN(crypt->len + 5, 16);
	crypt->r = crypt->len == 5 ? 2 : 3;

	createowner(crypt,
		(unsigned char *) userpw, strlen(userpw),
		(unsigned char *) ownerpw, strlen(ownerpw));
	createuser(crypt,
		(unsigned char *) userpw, strlen(userpw));

	error = fz_packobj(&crypt->encrypt,
			"<< /Filter /Standard "
			"/V %i /R %i "
			"/O %# /U %# "
			"/P %i "
			"/Length %i >>",
			crypt->r == 2 ? 1 : 2,
			crypt->r,
			crypt->o, 32,
			crypt->u, 32,
			crypt->p,
			crypt->len * 8);
	if (error)
	{
		pdf_dropcrypt(crypt);
		return fz_rethrow(error, "cannot create encryption dictionary");
	}

	*cp = crypt;
	return fz_okay;
}

int
pdf_setpassword(pdf_crypt *crypt, char *pw)
{
	int okay = pdf_setuserpassword(crypt, pw, strlen(pw));
	if (!okay)
	{
		okay = pdf_setownerpassword(crypt, pw, strlen(pw));
		if (!okay)
			return 0;
	}

	return 1;
}

/*
 * Alorithm 3.6 Authenticating the user password as of pdf 1.7
 */
int
pdf_setuserpassword(pdf_crypt *crypt, char *userpw, int pwlen)
{
	unsigned char saved[32];
	unsigned char test[32];

	/* Step 1 */
	memcpy(saved, crypt->u, 32);
	createuser(crypt, (unsigned char *) userpw, pwlen);
	memcpy(test, crypt->u, 32);
	memcpy(crypt->u, saved, 32);

	/* Step 2 */
	if (memcmp(test, saved, crypt->r >= 3 ? 16 : 32) != 0)
		return 0;

	return 1;
}

/*
 * Algorithm 3.7 Authenticating the owner password as of pdf 1.7
 */
int
pdf_setownerpassword(pdf_crypt *crypt, char *ownerpw, int pwlen)
{
	unsigned char saved[32];
	unsigned char test[32];
	unsigned char buf[32];
	unsigned char key[16];
	fz_arc4 arc4;
	fz_md5 md5;

	/* Step 1 */
	/* Algorithm 3.3 Step 1 + 2 */
	padpassword(buf, (unsigned char *) ownerpw, pwlen);
	fz_md5init(&md5);
	fz_md5update(&md5, buf, 32);
	fz_md5final(&md5, key);

	/* Algorithm 3.3 Step 3 (rev 3 or later) */
	if (crypt->r >= 3)
		voodoo50(key, crypt->len);

	/* Step 2 */
	fz_arc4init(&arc4, key, crypt->len);

	if (crypt->r == 2)
	{
		fz_arc4encrypt(&arc4, buf, crypt->o, 32);
	}
	if (crypt->r >= 3)
	{
		unsigned char keyxor[16];
		int i;
		int k;

		memcpy(buf, crypt->o, 32);

		for(i = 19; i >= 0; --i)
		{
			for(k = 0; k < crypt->keylen; ++k)
				keyxor[k] = key[k] ^ i;
			fz_arc4init(&arc4, keyxor, crypt->keylen);
			fz_arc4encrypt(&arc4, buf, buf, 32);
		}
	}

	/* Step 3 */
	/* Algorithm 3.6 Step 1 */
	memcpy(saved, crypt->u, 32);
	createuser(crypt, buf, 32);
	memcpy(test, crypt->u, 32);
	memcpy(crypt->u, saved, 32);

	/* Algorithm 3.6 Step 2 */
	if (memcmp(test, saved, crypt->r >= 3 ? 16 : 32) != 0)
		return 0;

	return 1;
}

/*
 * Recursively (and destructively!) de/encrypt all strings in obj
 */
void
pdf_cryptobj(pdf_crypt *crypt, fz_obj *obj, int oid, int gid)
{
	fz_arc4 arc4;
	unsigned char key[16];
	unsigned char *s;
	int i, n;

	if (fz_isstring(obj))
	{
		s = (unsigned char *) fz_tostrbuf(obj);
		n = fz_tostrlen(obj);
		createobjkey(crypt, oid, gid, key);
		fz_arc4init(&arc4, key, crypt->keylen);
		fz_arc4encrypt(&arc4, s, s, n);
	}

	else if (fz_isarray(obj))
	{
		n = fz_arraylen(obj);
		for (i = 0; i < n; i++)
		{
			pdf_cryptobj(crypt, fz_arrayget(obj, i), oid, gid);
		}
	}

	else if (fz_isdict(obj))
	{
		n = fz_dictlen(obj);
		for (i = 0; i < n; i++)
		{
			pdf_cryptobj(crypt, fz_dictgetval(obj, i), oid, gid);
		}
	}
}

/*
 * Create filter suitable for de/encrypting a stream
 */
fz_error *
pdf_cryptstream(fz_filter **fp, pdf_crypt *crypt, int oid, int gid)
{
	fz_error *error;
	unsigned char key[16];
	createobjkey(crypt, oid, gid, key);
	error = fz_newarc4filter(fp, key, crypt->keylen);
	if (error)
	    return fz_rethrow(error, "cannot create crypt filter");
	return fz_okay;
}

