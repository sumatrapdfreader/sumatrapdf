#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

enum
{
	PDF_CRYPT_NONE,
	PDF_CRYPT_RC4,
	PDF_CRYPT_AESV2,
	PDF_CRYPT_AESV3,
	PDF_CRYPT_UNKNOWN,
};

typedef struct pdf_crypt_filter_s pdf_crypt_filter;

struct pdf_crypt_filter_s
{
	int method;
	int length;
};

struct pdf_crypt_s
{
	pdf_obj *id;

	int v;
	int length;
	pdf_obj *cf;
	pdf_crypt_filter stmf;
	pdf_crypt_filter strf;

	int r;
	unsigned char o[48];
	unsigned char u[48];
	unsigned char oe[32];
	unsigned char ue[32];
	unsigned char perms[16];
	int p;
	int encrypt_metadata;

	unsigned char key[32]; /* decryption key generated from password */
};

static void pdf_parse_crypt_filter(fz_context *ctx, pdf_crypt_filter *cf, pdf_crypt *crypt, pdf_obj *name);

/*
 * Create crypt object for decrypting strings and streams
 * given the Encryption and ID objects.
 */

pdf_crypt *
pdf_new_crypt(fz_context *ctx, pdf_obj *dict, pdf_obj *id)
{
	pdf_crypt *crypt;
	pdf_obj *obj;

	crypt = fz_malloc_struct(ctx, pdf_crypt);

	/* Common to all security handlers (PDF 1.7 table 3.18) */

	obj = pdf_dict_get(ctx, dict, PDF_NAME(Filter));
	if (!pdf_is_name(ctx, obj))
	{
		pdf_drop_crypt(ctx, crypt);
		fz_throw(ctx, FZ_ERROR_GENERIC, "unspecified encryption handler");
	}
	if (!pdf_name_eq(ctx, PDF_NAME(Standard), obj))
	{
		pdf_drop_crypt(ctx, crypt);
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown encryption handler: '%s'", pdf_to_name(ctx, obj));
	}

	crypt->v = 0;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(V));
	if (pdf_is_int(ctx, obj))
		crypt->v = pdf_to_int(ctx, obj);
	if (crypt->v != 0 && crypt->v != 1 && crypt->v != 2 && crypt->v != 4 && crypt->v != 5)
	{
		pdf_drop_crypt(ctx, crypt);
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown encryption version");
	}

	/* Standard security handler (PDF 1.7 table 3.19) */

	obj = pdf_dict_get(ctx, dict, PDF_NAME(R));
	if (pdf_is_int(ctx, obj))
		crypt->r = pdf_to_int(ctx, obj);
	else if (crypt->v <= 4)
	{
		fz_warn(ctx, "encryption dictionary missing revision value, guessing...");
		if (crypt->v < 2)
			crypt->r = 2;
		else if (crypt->v == 2)
			crypt->r = 3;
		else if (crypt->v == 4)
			crypt->r = 4;
	}
	else
	{
		pdf_drop_crypt(ctx, crypt);
		fz_throw(ctx, FZ_ERROR_GENERIC, "encryption dictionary missing version and revision value");
	}
	if (crypt->r < 1 || crypt->r > 6)
	{
		int r = crypt->r;
		pdf_drop_crypt(ctx, crypt);
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown crypt revision %d", r);
	}

	obj = pdf_dict_get(ctx, dict, PDF_NAME(O));
	if (pdf_is_string(ctx, obj) && pdf_to_str_len(ctx, obj) == 32)
		memcpy(crypt->o, pdf_to_str_buf(ctx, obj), 32);
	/* /O and /U are supposed to be 48 bytes long for revision 5 and 6, they're often longer, though */
	else if (crypt->r >= 5 && pdf_is_string(ctx, obj) && pdf_to_str_len(ctx, obj) >= 48)
		memcpy(crypt->o, pdf_to_str_buf(ctx, obj), 48);
	else
	{
		pdf_drop_crypt(ctx, crypt);
		fz_throw(ctx, FZ_ERROR_GENERIC, "encryption dictionary missing owner password");
	}

	obj = pdf_dict_get(ctx, dict, PDF_NAME(U));
	if (pdf_is_string(ctx, obj) && pdf_to_str_len(ctx, obj) == 32)
		memcpy(crypt->u, pdf_to_str_buf(ctx, obj), 32);
	/* /O and /U are supposed to be 48 bytes long for revision 5 and 6, they're often longer, though */
	else if (crypt->r >= 5 && pdf_is_string(ctx, obj) && pdf_to_str_len(ctx, obj) >= 48)
		memcpy(crypt->u, pdf_to_str_buf(ctx, obj), 48);
	else if (pdf_is_string(ctx, obj) && pdf_to_str_len(ctx, obj) < 32)
	{
		fz_warn(ctx, "encryption password key too short (%zu)", pdf_to_str_len(ctx, obj));
		memcpy(crypt->u, pdf_to_str_buf(ctx, obj), pdf_to_str_len(ctx, obj));
	}
	else
	{
		pdf_drop_crypt(ctx, crypt);
		fz_throw(ctx, FZ_ERROR_GENERIC, "encryption dictionary missing user password");
	}

	obj = pdf_dict_get(ctx, dict, PDF_NAME(P));
	if (pdf_is_int(ctx, obj))
		crypt->p = pdf_to_int(ctx, obj);
	else
	{
		fz_warn(ctx, "encryption dictionary missing permissions");
		crypt->p = 0xfffffffc;
	}

	if (crypt->r == 5 || crypt->r == 6)
	{
		obj = pdf_dict_get(ctx, dict, PDF_NAME(OE));
		if (!pdf_is_string(ctx, obj) || pdf_to_str_len(ctx, obj) != 32)
		{
			pdf_drop_crypt(ctx, crypt);
			fz_throw(ctx, FZ_ERROR_GENERIC, "encryption dictionary missing owner encryption key");
		}
		memcpy(crypt->oe, pdf_to_str_buf(ctx, obj), 32);

		obj = pdf_dict_get(ctx, dict, PDF_NAME(UE));
		if (!pdf_is_string(ctx, obj) || pdf_to_str_len(ctx, obj) != 32)
		{
			pdf_drop_crypt(ctx, crypt);
			fz_throw(ctx, FZ_ERROR_GENERIC, "encryption dictionary missing user encryption key");
		}
		memcpy(crypt->ue, pdf_to_str_buf(ctx, obj), 32);
	}

	crypt->encrypt_metadata = 1;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(EncryptMetadata));
	if (pdf_is_bool(ctx, obj))
		crypt->encrypt_metadata = pdf_to_bool(ctx, obj);

	/* Extract file identifier string */

	if (pdf_is_array(ctx, id) && pdf_array_len(ctx, id) == 2)
	{
		obj = pdf_array_get(ctx, id, 0);
		if (pdf_is_string(ctx, obj))
			crypt->id = pdf_keep_obj(ctx, obj);
	}
	else
		fz_warn(ctx, "missing file identifier, may not be able to do decryption");

	/* Determine encryption key length */

	crypt->length = 40;
	if (crypt->v == 2 || crypt->v == 4)
	{
		obj = pdf_dict_get(ctx, dict, PDF_NAME(Length));
		if (pdf_is_int(ctx, obj))
			crypt->length = pdf_to_int(ctx, obj);

		/* work-around for pdf generators that assume length is in bytes */
		if (crypt->length < 40)
			crypt->length = crypt->length * 8;

		if (crypt->length % 8 != 0)
		{
			pdf_drop_crypt(ctx, crypt);
			fz_throw(ctx, FZ_ERROR_GENERIC, "invalid encryption key length");
		}
		if (crypt->length < 40 || crypt->length > 128)
		{
			pdf_drop_crypt(ctx, crypt);
			fz_throw(ctx, FZ_ERROR_GENERIC, "invalid encryption key length");
		}
	}

	if (crypt->v == 5)
		crypt->length = 256;

	if (crypt->v == 0 || crypt->v == 1 || crypt->v == 2)
	{
		crypt->stmf.method = PDF_CRYPT_RC4;
		crypt->stmf.length = crypt->length;

		crypt->strf.method = PDF_CRYPT_RC4;
		crypt->strf.length = crypt->length;
	}

	if (crypt->v == 4 || crypt->v == 5)
	{
		crypt->stmf.method = PDF_CRYPT_NONE;
		crypt->stmf.length = crypt->length;

		crypt->strf.method = PDF_CRYPT_NONE;
		crypt->strf.length = crypt->length;

		obj = pdf_dict_get(ctx, dict, PDF_NAME(CF));
		if (pdf_is_dict(ctx, obj))
		{
			crypt->cf = pdf_keep_obj(ctx, obj);
		}
		else
		{
			crypt->cf = NULL;
		}

		fz_try(ctx)
		{
			obj = pdf_dict_get(ctx, dict, PDF_NAME(StmF));
			if (pdf_is_name(ctx, obj))
				pdf_parse_crypt_filter(ctx, &crypt->stmf, crypt, obj);

			obj = pdf_dict_get(ctx, dict, PDF_NAME(StrF));
			if (pdf_is_name(ctx, obj))
				pdf_parse_crypt_filter(ctx, &crypt->strf, crypt, obj);
		}
		fz_catch(ctx)
		{
			pdf_drop_crypt(ctx, crypt);
			fz_rethrow(ctx);
		}

		/* in crypt revision 4, the crypt filter determines the key length */
		if (crypt->strf.method != PDF_CRYPT_NONE)
			crypt->length = crypt->stmf.length;
	}

	return crypt;
}

void
pdf_drop_crypt(fz_context *ctx, pdf_crypt *crypt)
{
	if (!crypt)
		return;

	pdf_drop_obj(ctx, crypt->id);
	pdf_drop_obj(ctx, crypt->cf);
	fz_free(ctx, crypt);
}

/*
 * Parse a CF dictionary entry (PDF 1.7 table 3.22)
 */

static void
pdf_parse_crypt_filter(fz_context *ctx, pdf_crypt_filter *cf, pdf_crypt *crypt, pdf_obj *name)
{
	pdf_obj *obj;
	pdf_obj *dict;
	int is_identity = (pdf_name_eq(ctx, name, PDF_NAME(Identity)));
	int is_stdcf = (!is_identity && pdf_name_eq(ctx, name, PDF_NAME(StdCF)));

	if (!is_identity && !is_stdcf)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Crypt Filter not Identity or StdCF (%d 0 R)", pdf_to_num(ctx, crypt->cf));

	cf->method = PDF_CRYPT_NONE;
	cf->length = crypt->length;

	if (!crypt->cf)
	{
		cf->method = (is_identity ? PDF_CRYPT_NONE : PDF_CRYPT_RC4);
		return;
	}

	dict = pdf_dict_get(ctx, crypt->cf, name);
	if (pdf_is_dict(ctx, dict))
	{
		obj = pdf_dict_get(ctx, dict, PDF_NAME(CFM));
		if (pdf_is_name(ctx, obj))
		{
			if (pdf_name_eq(ctx, PDF_NAME(None), obj))
				cf->method = PDF_CRYPT_NONE;
			else if (pdf_name_eq(ctx, PDF_NAME(V2), obj))
				cf->method = PDF_CRYPT_RC4;
			else if (pdf_name_eq(ctx, PDF_NAME(AESV2), obj))
				cf->method = PDF_CRYPT_AESV2;
			else if (pdf_name_eq(ctx, PDF_NAME(AESV3), obj))
				cf->method = PDF_CRYPT_AESV3;
			else
				fz_warn(ctx, "unknown encryption method: %s", pdf_to_name(ctx, obj));
		}

		obj = pdf_dict_get(ctx, dict, PDF_NAME(Length));
		if (pdf_is_int(ctx, obj))
			cf->length = pdf_to_int(ctx, obj);
	}
	else if (!is_identity)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot parse crypt filter (%d 0 R)", pdf_to_num(ctx, crypt->cf));

	/* the length for crypt filters is supposed to be in bytes not bits */
	if (cf->length < 40)
		cf->length = cf->length * 8;

	if ((cf->length % 8) != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid key length: %d", cf->length);

	if ((crypt->r == 1 || crypt->r == 2 || crypt->r == 3 || crypt->r == 4) &&
		(cf->length < 40 || cf->length > 128))
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid key length: %d", cf->length);
	if ((crypt->r == 5 || crypt->r == 6) && cf->length != 256)
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid key length: %d", cf->length);
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
pdf_compute_encryption_key(fz_context *ctx, pdf_crypt *crypt, unsigned char *password, size_t pwlen, unsigned char *key)
{
	unsigned char buf[32];
	unsigned int p;
	int i, n;
	fz_md5 md5;

	n = fz_clampi(crypt->length / 8, 0, 16);

	/* Step 1 - copy and pad password string */
	if (pwlen > 32)
		pwlen = 32;
	memcpy(buf, password, pwlen);
	memcpy(buf + pwlen, padding, 32 - pwlen);

	/* Step 2 - init md5 and pass value of step 1 */
	fz_md5_init(&md5);
	fz_md5_update(&md5, buf, 32);

	/* Step 3 - pass O value */
	fz_md5_update(&md5, crypt->o, 32);

	/* Step 4 - pass P value as unsigned int, low-order byte first */
	p = (unsigned int) crypt->p;
	buf[0] = (p) & 0xFF;
	buf[1] = (p >> 8) & 0xFF;
	buf[2] = (p >> 16) & 0xFF;
	buf[3] = (p >> 24) & 0xFF;
	fz_md5_update(&md5, buf, 4);

	/* Step 5 - pass first element of ID array */
	fz_md5_update(&md5, (unsigned char *)pdf_to_str_buf(ctx, crypt->id), pdf_to_str_len(ctx, crypt->id));

	/* Step 6 (revision 4 or greater) - if metadata is not encrypted pass 0xFFFFFFFF */
	if (crypt->r >= 4)
	{
		if (!crypt->encrypt_metadata)
		{
			buf[0] = 0xFF;
			buf[1] = 0xFF;
			buf[2] = 0xFF;
			buf[3] = 0xFF;
			fz_md5_update(&md5, buf, 4);
		}
	}

	/* Step 7 - finish the hash */
	fz_md5_final(&md5, buf);

	/* Step 8 (revision 3 or greater) - do some voodoo 50 times */
	if (crypt->r >= 3)
	{
		for (i = 0; i < 50; i++)
		{
			fz_md5_init(&md5);
			fz_md5_update(&md5, buf, n);
			fz_md5_final(&md5, buf);
		}
	}

	/* Step 9 - the key is the first 'n' bytes of the result */
	memcpy(key, buf, n);
}

/*
 * Compute an encryption key (PDF 1.7 ExtensionLevel 3 algorithm 3.2a)
 */

static void
pdf_compute_encryption_key_r5(fz_context *ctx, pdf_crypt *crypt, unsigned char *password, size_t pwlen, int ownerkey, unsigned char *validationkey)
{
	unsigned char buffer[128 + 8 + 48];
	fz_sha256 sha256;
	fz_aes aes;

	/* Step 2 - truncate UTF-8 password to 127 characters */

	if (pwlen > 127)
		pwlen = 127;

	/* Step 3/4 - test password against owner/user key and compute encryption key */

	memcpy(buffer, password, pwlen);
	if (ownerkey)
	{
		memcpy(buffer + pwlen, crypt->o + 32, 8);
		memcpy(buffer + pwlen + 8, crypt->u, 48);
	}
	else
		memcpy(buffer + pwlen, crypt->u + 32, 8);

	fz_sha256_init(&sha256);
	fz_sha256_update(&sha256, buffer, pwlen + 8 + (ownerkey ? 48 : 0));
	fz_sha256_final(&sha256, validationkey);

	/* Step 3.5/4.5 - compute file encryption key from OE/UE */

	if (ownerkey)
	{
		memcpy(buffer + pwlen, crypt->o + 40, 8);
		memcpy(buffer + pwlen + 8, crypt->u, 48);
	}
	else
		memcpy(buffer + pwlen, crypt->u + 40, 8);

	fz_sha256_init(&sha256);
	fz_sha256_update(&sha256, buffer, pwlen + 8 + (ownerkey ? 48 : 0));
	fz_sha256_final(&sha256, buffer);

	/* clear password buffer and use it as iv */
	memset(buffer + 32, 0, sizeof(buffer) - 32);
	if (fz_aes_setkey_dec(&aes, buffer, crypt->length))
		fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=%d)", crypt->length);
	fz_aes_crypt_cbc(&aes, FZ_AES_DECRYPT, 32, buffer + 32, ownerkey ? crypt->oe : crypt->ue, crypt->key);
}

/*
 * Compute an encryption key (PDF 1.7 ExtensionLevel 8 algorithm)
 *
 * Adobe has not yet released the details, so the algorithm reference is:
 * http://esec-lab.sogeti.com/post/The-undocumented-password-validation-algorithm-of-Adobe-Reader-X
 */

static void
pdf_compute_hardened_hash_r6(fz_context *ctx, unsigned char *password, size_t pwlen, unsigned char salt[8], unsigned char *ownerkey, unsigned char hash[32])
{
	unsigned char data[(128 + 64 + 48) * 64];
	unsigned char block[64];
	int block_size = 32;
	size_t data_len = 0;
	int i, j, sum;

	fz_sha256 sha256;
	fz_sha384 sha384;
	fz_sha512 sha512;
	fz_aes aes;

	/* Step 1: calculate initial data block */
	fz_sha256_init(&sha256);
	fz_sha256_update(&sha256, password, pwlen);
	fz_sha256_update(&sha256, salt, 8);
	if (ownerkey)
		fz_sha256_update(&sha256, ownerkey, 48);
	fz_sha256_final(&sha256, block);

	for (i = 0; i < 64 || i < data[data_len * 64 - 1] + 32; i++)
	{
		/* Step 2: repeat password and data block 64 times */
		memcpy(data, password, pwlen);
		memcpy(data + pwlen, block, block_size);
		if (ownerkey)
			memcpy(data + pwlen + block_size, ownerkey, 48);
		data_len = pwlen + block_size + (ownerkey ? 48 : 0);
		for (j = 1; j < 64; j++)
			memcpy(data + j * data_len, data, data_len);

		/* Step 3: encrypt data using data block as key and iv */
		if (fz_aes_setkey_enc(&aes, block, 128))
			fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=%d)", 128);
		fz_aes_crypt_cbc(&aes, FZ_AES_ENCRYPT, data_len * 64, block + 16, data, data);

		/* Step 4: determine SHA-2 hash size for this round */
		for (j = 0, sum = 0; j < 16; j++)
			sum += data[j];

		/* Step 5: calculate data block for next round */
		block_size = 32 + (sum % 3) * 16;
		switch (block_size)
		{
		case 32:
			fz_sha256_init(&sha256);
			fz_sha256_update(&sha256, data, data_len * 64);
			fz_sha256_final(&sha256, block);
			break;
		case 48:
			fz_sha384_init(&sha384);
			fz_sha384_update(&sha384, data, data_len * 64);
			fz_sha384_final(&sha384, block);
			break;
		case 64:
			fz_sha512_init(&sha512);
			fz_sha512_update(&sha512, data, data_len * 64);
			fz_sha512_final(&sha512, block);
			break;
		}
	}

	memset(data, 0, sizeof(data));
	memcpy(hash, block, 32);
}

static void
pdf_compute_encryption_key_r6(fz_context *ctx, pdf_crypt *crypt, unsigned char *password, size_t pwlen, int ownerkey, unsigned char *validationkey)
{
	unsigned char hash[32];
	unsigned char iv[16];
	fz_aes aes;

	if (pwlen > 127)
		pwlen = 127;

	pdf_compute_hardened_hash_r6(ctx, password, pwlen,
		(ownerkey ? crypt->o : crypt->u) + 32,
		ownerkey ? crypt->u : NULL, validationkey);
	pdf_compute_hardened_hash_r6(ctx, password, pwlen,
		(ownerkey ? crypt->o : crypt->u) + 40,
		(ownerkey ? crypt->u : NULL),
		hash);

	memset(iv, 0, sizeof(iv));
	if (fz_aes_setkey_dec(&aes, hash, 256))
		fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=256)");
	fz_aes_crypt_cbc(&aes, FZ_AES_DECRYPT, 32, iv, ownerkey ? crypt->oe : crypt->ue, crypt->key);
}

/*
 * Computing the user password (PDF 1.7 algorithm 3.4 and 3.5)
 * Also save the generated key for decrypting objects and streams in crypt->key.
 */

static void
pdf_compute_user_password(fz_context *ctx, pdf_crypt *crypt, unsigned char *password, size_t pwlen, unsigned char *output)
{
	int n = fz_clampi(crypt->length / 8, 0, 16);

	if (crypt->r == 2)
	{
		fz_arc4 arc4;

		pdf_compute_encryption_key(ctx, crypt, password, pwlen, crypt->key);
		fz_arc4_init(&arc4, crypt->key, n);
		fz_arc4_encrypt(&arc4, output, padding, 32);
	}

	if (crypt->r == 3 || crypt->r == 4)
	{
		unsigned char xor[32];
		unsigned char digest[16];
		fz_md5 md5;
		fz_arc4 arc4;
		int i, x;

		pdf_compute_encryption_key(ctx, crypt, password, pwlen, crypt->key);

		fz_md5_init(&md5);
		fz_md5_update(&md5, padding, 32);
		fz_md5_update(&md5, (unsigned char*)pdf_to_str_buf(ctx, crypt->id), pdf_to_str_len(ctx, crypt->id));
		fz_md5_final(&md5, digest);

		fz_arc4_init(&arc4, crypt->key, n);
		fz_arc4_encrypt(&arc4, output, digest, 16);

		for (x = 1; x <= 19; x++)
		{
			for (i = 0; i < n; i++)
				xor[i] = crypt->key[i] ^ x;
			fz_arc4_init(&arc4, xor, n);
			fz_arc4_encrypt(&arc4, output, output, 16);
		}

		memcpy(output + 16, padding, 16);
	}

	if (crypt->r == 5)
	{
		pdf_compute_encryption_key_r5(ctx, crypt, password, pwlen, 0, output);
	}

	if (crypt->r == 6)
	{
		pdf_compute_encryption_key_r6(ctx, crypt, password, pwlen, 0, output);
	}
}

/*
 * Authenticating the user password (PDF 1.7 algorithm 3.6
 * and ExtensionLevel 3 algorithm 3.11)
 * This also has the side effect of saving a key generated
 * from the password for decrypting objects and streams.
 */

static int
pdf_authenticate_user_password(fz_context *ctx, pdf_crypt *crypt, unsigned char *password, size_t pwlen)
{
	unsigned char output[32];
	pdf_compute_user_password(ctx, crypt, password, pwlen, output);
	if (crypt->r == 2 || crypt->r == 5 || crypt->r == 6)
		return memcmp(output, crypt->u, 32) == 0;
	if (crypt->r == 3 || crypt->r == 4)
		return memcmp(output, crypt->u, 16) == 0;
	return 0;
}

/*
 * Authenticating the owner password (PDF 1.7 algorithm 3.7,
 * ExtensionLevel 3 algorithm 3.12, ExtensionLevel 8 algorithm)
 * Generates the user password from the owner password
 * and calls pdf_authenticate_user_password.
 */

static int
pdf_authenticate_owner_password(fz_context *ctx, pdf_crypt *crypt, unsigned char *ownerpass, size_t pwlen)
{
	int n = fz_clampi(crypt->length / 8, 0, 16);

	if (crypt->r == 2)
	{
		unsigned char pwbuf[32];
		unsigned char key[16];
		unsigned char userpass[32];
		fz_md5 md5;
		fz_arc4 arc4;

		if (pwlen > 32)
			pwlen = 32;
		memcpy(pwbuf, ownerpass, pwlen);
		memcpy(pwbuf + pwlen, padding, 32 - pwlen);

		fz_md5_init(&md5);
		fz_md5_update(&md5, pwbuf, 32);
		fz_md5_final(&md5, key);

		fz_arc4_init(&arc4, key, n);
		fz_arc4_encrypt(&arc4, userpass, crypt->o, 32);

		return pdf_authenticate_user_password(ctx, crypt, userpass, 32);
	}

	if (crypt->r == 3 || crypt->r == 4)
	{
		unsigned char pwbuf[32];
		unsigned char key[16];
		unsigned char xor[32];
		unsigned char userpass[32];
		int i, x;
		fz_md5 md5;
		fz_arc4 arc4;

		if (pwlen > 32)
			pwlen = 32;
		memcpy(pwbuf, ownerpass, pwlen);
		memcpy(pwbuf + pwlen, padding, 32 - pwlen);

		fz_md5_init(&md5);
		fz_md5_update(&md5, pwbuf, 32);
		fz_md5_final(&md5, key);

		for (i = 0; i < 50; i++)
		{
			fz_md5_init(&md5);
			fz_md5_update(&md5, key, n);
			fz_md5_final(&md5, key);
		}

		memcpy(userpass, crypt->o, 32);
		for (x = 0; x < 20; x++)
		{
			for (i = 0; i < n; i++)
				xor[i] = key[i] ^ (19 - x);
			fz_arc4_init(&arc4, xor, n);
			fz_arc4_encrypt(&arc4, userpass, userpass, 32);
		}

		return pdf_authenticate_user_password(ctx, crypt, userpass, 32);
	}

	if (crypt->r == 5)
	{
		unsigned char key[32];
		pdf_compute_encryption_key_r5(ctx, crypt, ownerpass, pwlen, 1, key);
		return !memcmp(key, crypt->o, 32);
	}

	if (crypt->r == 6)
	{
		unsigned char key[32];
		pdf_compute_encryption_key_r6(ctx, crypt, ownerpass, pwlen, 1, key);
		return !memcmp(key, crypt->o, 32);
	}

	return 0;
}

static void pdf_docenc_from_utf8(char *password, const char *utf8, int n)
{
	int i = 0, k, c;
	while (*utf8 && i + 1 < n)
	{
		utf8 += fz_chartorune(&c, utf8);
		for (k = 0; k < 256; k++)
		{
			if (c == fz_unicode_from_pdf_doc_encoding[k])
			{
				password[i++] = k;
				break;
			}
		}
		/* FIXME: drop characters that can't be encoded or return an error? */
	}
	password[i] = 0;
}

static void pdf_saslprep_from_utf8(char *password, const char *utf8, int n)
{
	/* TODO: stringprep with SALSprep profile */
	fz_strlcpy(password, utf8, n);
}

/*
	Attempt to authenticate a
	password.

	Returns 0 for failure, non-zero for success.

	In the non-zero case:
		bit 0 set => no password required
		bit 1 set => user password authenticated
		bit 2 set => owner password authenticated
*/
int
pdf_authenticate_password(fz_context *ctx, pdf_document *doc, const char *pwd_utf8)
{
	char password[2048];
	int auth;

	if (!doc->crypt)
		return 1; /* No password required */

	password[0] = 0;
	if (pwd_utf8)
	{
		if (doc->crypt->r <= 4)
			pdf_docenc_from_utf8(password, pwd_utf8, sizeof password);
		else
			pdf_saslprep_from_utf8(password, pwd_utf8, sizeof password);
	}

	auth = 0;
	if (pdf_authenticate_user_password(ctx, doc->crypt, (unsigned char *)password, strlen(password)))
		auth = 2;
	if (pdf_authenticate_owner_password(ctx, doc->crypt, (unsigned char *)password, strlen(password)))
		auth |= 4;
	else if (auth & 2)
	{
		/* We need to reauthenticate the user password,
		 * because the failed attempt to authenticate
		 * the owner password will have invalidated the
		 * stored keys. */
		(void)pdf_authenticate_user_password(ctx, doc->crypt, (unsigned char *)password, strlen(password));
	}

	/* To match Acrobat, we choose not to allow an empty owner
	 * password, unless the user password is also the empty one. */
	if (*password == 0 && auth == 4)
		return 0;

	return auth;
}

int
pdf_needs_password(fz_context *ctx, pdf_document *doc)
{
	if (!doc->crypt)
		return 0;
	if (pdf_authenticate_password(ctx, doc, ""))
		return 0;
	return 1;
}

int
pdf_has_permission(fz_context *ctx, pdf_document *doc, fz_permission p)
{
	if (!doc->crypt)
		return 1;
	switch (p)
	{
	case FZ_PERMISSION_PRINT: return doc->crypt->p & PDF_PERM_PRINT;
	case FZ_PERMISSION_COPY: return doc->crypt->p & PDF_PERM_COPY;
	case FZ_PERMISSION_EDIT: return doc->crypt->p & PDF_PERM_MODIFY;
	case FZ_PERMISSION_ANNOTATE: return doc->crypt->p & PDF_PERM_ANNOTATE;
	}
	return 1;
}

int
pdf_document_permissions(fz_context *ctx, pdf_document *doc)
{
	if (doc->crypt)
		return doc->crypt->p;
	/* all permissions granted, reserved bits set appropriately */
	return (int)0xFFFFFFFC;
}

/*
 * Compute the owner password (PDF 1.7 algorithm 3.3)
 */

static void
pdf_compute_owner_password(fz_context *ctx, pdf_crypt *crypt, unsigned char *opassword, size_t opwlen, unsigned char *upassword, size_t upwlen, unsigned char *output)
{
	unsigned char obuf[32];
	unsigned char ubuf[32];
	unsigned char digest[32];
	int i, n;
	fz_md5 md5;
	fz_arc4 arc4;

	n = fz_clampi(crypt->length / 8, 0, 16);

	/* Step 1 - copy and pad owner password string */
	if (opwlen > 32)
		opwlen = 32;
	memcpy(obuf, opassword, opwlen);
	memcpy(obuf + opwlen, padding, 32 - opwlen);

	/* Step 2 - init md5 and pass value of step 1 */
	fz_md5_init(&md5);
	fz_md5_update(&md5, obuf, 32);
	fz_md5_final(&md5, obuf);

	/* Step 3 (revision 3 or greater) - do some voodoo 50 times */
	if (crypt->r >= 3)
	{
		for (i = 0; i < 50; i++)
		{
			fz_md5_init(&md5);
			fz_md5_update(&md5, obuf, n);
			fz_md5_final(&md5, obuf);
		}
	}

	/* Step 4 - encrypt owner password md5 hash */
	fz_arc4_init(&arc4, obuf, n);

	/* Step 5 - copy and pad user password string */
	if (upwlen > 32)
		upwlen = 32;
	memcpy(ubuf, upassword, upwlen);
	memcpy(ubuf + upwlen, padding, 32 - upwlen);

	/* Step 6 - encrypt user password md5 hash */
	fz_arc4_encrypt(&arc4, digest, ubuf, 32);

	/* Step 7 - */
	if (crypt->r >= 3)
	{
		unsigned char xor[32];
		int x;

		for (x = 1; x <= 19; x++)
		{
			for (i = 0; i < n; i++)
				xor[i] = obuf[i] ^ x;
			fz_arc4_init(&arc4, xor, n);
			fz_arc4_encrypt(&arc4, digest, digest, 32);
		}
	}

	/* Step 8 - the owner password is the first 16 bytes of the result */
	memcpy(output, digest, 32);
}

unsigned char *
pdf_crypt_key(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->key;
	return NULL;
}

int
pdf_crypt_version(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->v;
	return 0;
}

int pdf_crypt_revision(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->r;
	return 0;
}

char *
pdf_crypt_method(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
	{
		switch (crypt->strf.method)
		{
		case PDF_CRYPT_NONE: return "None";
		case PDF_CRYPT_RC4: return "RC4";
		case PDF_CRYPT_AESV2: return "AES";
		case PDF_CRYPT_AESV3: return "AES";
		case PDF_CRYPT_UNKNOWN: return "Unknown";
		}
	}
	return "None";
}

int
pdf_crypt_length(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->length;
	return 0;
}

int
pdf_crypt_permissions(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->p;
	return 0;
}

int
pdf_crypt_encrypt_metadata(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->encrypt_metadata;
	return 0;
}

unsigned char *
pdf_crypt_owner_password(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->o;
	return NULL;
}

unsigned char *
pdf_crypt_user_password(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->u;
	return NULL;
}

unsigned char *
pdf_crypt_owner_encryption(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->oe;
	return NULL;
}

unsigned char *
pdf_crypt_user_encryption(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->ue;
	return NULL;
}

unsigned char *
pdf_crypt_permissions_encryption(fz_context *ctx, pdf_crypt *crypt)
{
	if (crypt)
		return crypt->perms;
	return 0;
}

/*
 * PDF 1.7 algorithm 3.1 and ExtensionLevel 3 algorithm 3.1a
 *
 * Using the global encryption key that was generated from the
 * password, create a new key that is used to decrypt individual
 * objects and streams. This key is based on the object and
 * generation numbers.
 */

static int
pdf_compute_object_key(pdf_crypt *crypt, pdf_crypt_filter *cf, int num, int gen, unsigned char *key, int max_len)
{
	fz_md5 md5;
	unsigned char message[5];
	int key_len = crypt->length / 8;

	if (key_len > max_len)
		key_len = max_len;

	/* Encryption method version 0 is undocumented, but a lucky
	   guess revealed that all streams/strings in those PDFs are
	   encrypted using the same 40 bit file enryption key using RC4. */
	if (crypt->v == 0 || cf->method == PDF_CRYPT_AESV3)
	{
		memcpy(key, crypt->key, key_len);
		return key_len;
	}

	fz_md5_init(&md5);
	fz_md5_update(&md5, crypt->key, key_len);
	message[0] = (num) & 0xFF;
	message[1] = (num >> 8) & 0xFF;
	message[2] = (num >> 16) & 0xFF;
	message[3] = (gen) & 0xFF;
	message[4] = (gen >> 8) & 0xFF;
	fz_md5_update(&md5, message, 5);

	if (cf->method == PDF_CRYPT_AESV2)
		fz_md5_update(&md5, (unsigned char *)"sAlT", 4);

	fz_md5_final(&md5, key);

	if (key_len + 5 > 16)
		return 16;
	return key_len + 5;
}

/*
 * PDF 1.7 algorithm 3.1 and ExtensionLevel 3 algorithm 3.1a
 *
 * Decrypt all strings in obj modifying the data in-place.
 * Recurse through arrays and dictionaries, but do not follow
 * indirect references.
 */

static void
pdf_crypt_obj_imp(fz_context *ctx, pdf_crypt *crypt, pdf_obj *obj, unsigned char *key, int keylen)
{
	unsigned char *s;
	int i;

	if (pdf_is_indirect(ctx, obj))
		return;

	if (pdf_is_string(ctx, obj))
	{
		size_t n = pdf_to_str_len(ctx, obj);
		s = (unsigned char *)pdf_to_str_buf(ctx, obj);

		if (crypt->strf.method == PDF_CRYPT_RC4)
		{
			fz_arc4 arc4;
			fz_arc4_init(&arc4, key, keylen);
			fz_arc4_encrypt(&arc4, s, s, n);
		}

		if (crypt->strf.method == PDF_CRYPT_AESV2 || crypt->strf.method == PDF_CRYPT_AESV3)
		{
			if (n == 0)
			{
				/* Empty strings are permissible */
			}
			else if (n & 15 || n < 32)
				fz_warn(ctx, "invalid string length for aes encryption");
			else
			{
				unsigned char iv[16];
				fz_aes aes;
				memcpy(iv, s, 16);
				if (fz_aes_setkey_dec(&aes, key, keylen * 8))
					fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=%d)", keylen * 8);
				fz_aes_crypt_cbc(&aes, FZ_AES_DECRYPT, n - 16, iv, s + 16, s);
				/* delete space used for iv and padding bytes at end */
				if (s[n - 17] < 1 || s[n - 17] > 16)
					fz_warn(ctx, "aes padding out of range");
				else
					pdf_set_str_len(ctx, obj, n - 16 - s[n - 17]);
			}
		}
	}

	else if (pdf_is_array(ctx, obj))
	{
		int n = pdf_array_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			pdf_crypt_obj_imp(ctx, crypt, pdf_array_get(ctx, obj, i), key, keylen);
		}
	}

	else if (pdf_is_dict(ctx, obj))
	{
		int n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			pdf_crypt_obj_imp(ctx, crypt, pdf_dict_get_val(ctx, obj, i), key, keylen);
		}
	}
}

void
pdf_crypt_obj(fz_context *ctx, pdf_crypt *crypt, pdf_obj *obj, int num, int gen)
{
	unsigned char key[32];
	int len;

	len = pdf_compute_object_key(crypt, &crypt->strf, num, gen, key, 32);

	pdf_crypt_obj_imp(ctx, crypt, obj, key, len);
}

/*
 * PDF 1.7 algorithm 3.1 and ExtensionLevel 3 algorithm 3.1a
 *
 * Create filter suitable for de/encrypting a stream.
 */
static fz_stream *
pdf_open_crypt_imp(fz_context *ctx, fz_stream *chain, pdf_crypt *crypt, pdf_crypt_filter *stmf, int num, int gen)
{
	unsigned char key[32];
	int len;

	len = pdf_compute_object_key(crypt, stmf, num, gen, key, 32);

	if (stmf->method == PDF_CRYPT_RC4)
		return fz_open_arc4(ctx, chain, key, len);

	if (stmf->method == PDF_CRYPT_AESV2 || stmf->method == PDF_CRYPT_AESV3)
		return fz_open_aesd(ctx, chain, key, len);

	return fz_keep_stream(ctx, chain);
}

fz_stream *
pdf_open_crypt(fz_context *ctx, fz_stream *chain, pdf_crypt *crypt, int num, int gen)
{
	return pdf_open_crypt_imp(ctx, chain, crypt, &crypt->stmf, num, gen);
}

fz_stream *
pdf_open_crypt_with_filter(fz_context *ctx, fz_stream *chain, pdf_crypt *crypt, pdf_obj *name, int num, int gen)
{
	if (!pdf_name_eq(ctx, name, PDF_NAME(Identity)))
	{
		pdf_crypt_filter cf;
		pdf_parse_crypt_filter(ctx, &cf, crypt, name);
		return pdf_open_crypt_imp(ctx, chain, crypt, &cf, num, gen);
	}
	return fz_keep_stream(ctx, chain);
}

void
pdf_print_crypt(fz_context *ctx, fz_output *out, pdf_crypt *crypt)
{
	int i;

	fz_write_printf(ctx, out, "crypt {\n");

	fz_write_printf(ctx, out, "\tv=%d length=%d\n", crypt->v, crypt->length);
	fz_write_printf(ctx, out, "\tstmf method=%d length=%d\n", crypt->stmf.method, crypt->stmf.length);
	fz_write_printf(ctx, out, "\tstrf method=%d length=%d\n", crypt->strf.method, crypt->strf.length);
	fz_write_printf(ctx, out, "\tr=%d\n", crypt->r);

	fz_write_printf(ctx, out, "\to=<");
	for (i = 0; i < 32; i++)
		fz_write_printf(ctx, out, "%02X", crypt->o[i]);
	fz_write_printf(ctx, out, ">\n");

	fz_write_printf(ctx, out, "\tu=<");
	for (i = 0; i < 32; i++)
		fz_write_printf(ctx, out, "%02X", crypt->u[i]);
	fz_write_printf(ctx, out, ">\n");

	fz_write_printf(ctx, out, "}\n");
}

void pdf_encrypt_data(fz_context *ctx, pdf_crypt *crypt, int num, int gen, void (*write_data)(fz_context *ctx, void *, const unsigned char *, size_t), void *arg, const unsigned char *s, size_t n)
{
	unsigned char buffer[256];
	unsigned char key[32];
	int keylen;

	if (crypt == NULL)
	{
		write_data(ctx, arg, s, n);
		return;
	}

	keylen = pdf_compute_object_key(crypt, &crypt->strf, num, gen, key, 32);

	if (crypt->strf.method == PDF_CRYPT_RC4)
	{
		fz_arc4 arc4;
		fz_arc4_init(&arc4, key, keylen);
		while (n > 0)
		{
			size_t len = n;
			if (len > (int)sizeof(buffer))
				len = sizeof(buffer);
			fz_arc4_encrypt(&arc4, buffer, s, len);
			write_data(ctx, arg, buffer, len);
			s += len;
			n -= len;
		}
		return;
	}

	if (crypt->strf.method == PDF_CRYPT_AESV2 || crypt->strf.method == PDF_CRYPT_AESV3)
	{
		fz_aes aes;
		unsigned char iv[16];

		/* Empty strings can be represented by empty strings */
		if (n == 0)
			return;

		if (fz_aes_setkey_enc(&aes, key, keylen * 8))
			fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=%d)", keylen * 8);

		fz_memrnd(ctx, iv, 16);
		write_data(ctx, arg, iv, 16);

		while (n > 0)
		{
			size_t len = n;
			if (len > 16)
				len = 16;
			memcpy(buffer, s, len);
			if (len != 16)
				memset(&buffer[len], 16-(int)len, 16-len);
			fz_aes_crypt_cbc(&aes, FZ_AES_ENCRYPT, 16, iv, buffer, buffer+16);
			write_data(ctx, arg, buffer+16, 16);
			s += 16;
			n -= 16;
		}
		if (n == 0) {
			memset(buffer, 16, 16);
			fz_aes_crypt_cbc(&aes, FZ_AES_ENCRYPT, 16, iv, buffer, buffer+16);
			write_data(ctx, arg, buffer+16, 16);
		}
		return;
	}

	/* Should never happen, but... */
	write_data(ctx, arg, s, n);
}

size_t pdf_encrypted_len(fz_context *ctx, pdf_crypt *crypt, int num, int gen, size_t len)
{
	if (crypt == NULL)
		return len;

	if (crypt->strf.method == PDF_CRYPT_AESV2 || crypt->strf.method == PDF_CRYPT_AESV3)
	{
		len += 16; /* 16 for IV */
		if ((len & 15) == 0)
			len += 16; /* Another 16 if our last block is full anyway */
		len = (len + 15) & ~15; /* And pad to the block */
	}

	return len;
}

/* PDF 2.0 algorithm 8 */
static void
pdf_compute_user_password_r6(fz_context *ctx, pdf_crypt *crypt, unsigned char *password, size_t pwlen, unsigned char *outputpw, unsigned char *outputencryption)
{
	unsigned char validationsalt[8];
	unsigned char keysalt[8];
	unsigned char hash[32];
	unsigned char iv[16];
	fz_aes aes;

	/* Step a) - Generate random salts. */
	fz_memrnd(ctx, validationsalt, nelem(validationsalt));
	fz_memrnd(ctx, keysalt, nelem(keysalt));

	/* Step a) - Compute 32 byte hash given password and validation salt. */
	pdf_compute_hardened_hash_r6(ctx, password, pwlen, validationsalt, NULL, outputpw);
	memcpy(outputpw + 32, validationsalt, nelem(validationsalt));
	memcpy(outputpw + 40, keysalt, nelem(keysalt));

	/* Step b) - Compute 32 byte hash given password and user salt. */
	pdf_compute_hardened_hash_r6(ctx, password, pwlen, keysalt, NULL, hash);

	/* Step b) - Use hash as AES-key when encrypting the file encryption key. */
	memset(iv, 0, sizeof(iv));
	if (fz_aes_setkey_enc(&aes, hash, 256))
		fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=256)");
	fz_aes_crypt_cbc(&aes, FZ_AES_ENCRYPT, 32, iv, crypt->key, outputencryption);
}

/* PDF 2.0 algorithm 9 */
static void
pdf_compute_owner_password_r6(fz_context *ctx, pdf_crypt *crypt, unsigned char *password, size_t pwlen, unsigned char *outputpw, unsigned char *outputencryption)
{
	unsigned char validationsalt[8];
	unsigned char keysalt[8];
	unsigned char hash[32];
	unsigned char iv[16];
	fz_aes aes;

	/* Step a) - Generate random salts. */
	fz_memrnd(ctx, validationsalt, nelem(validationsalt));
	fz_memrnd(ctx, keysalt, nelem(keysalt));

	/* Step a) - Compute 32 byte hash given owner password, validation salt and user password. */
	pdf_compute_hardened_hash_r6(ctx, password, pwlen, validationsalt, crypt->u, outputpw);
	memcpy(outputpw + 32, validationsalt, nelem(validationsalt));
	memcpy(outputpw + 40, keysalt, nelem(keysalt));

	/* Step b) - Compute 32 byte hash given owner password, user salt and user password. */
	pdf_compute_hardened_hash_r6(ctx, password, pwlen, keysalt, crypt->u, hash);

	/* Step b) - Use hash as AES-key when encrypting the file encryption key. */
	memset(iv, 0, sizeof(iv));
	if (fz_aes_setkey_enc(&aes, hash, 256))
		fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=256)");
	fz_aes_crypt_cbc(&aes, FZ_AES_ENCRYPT, 32, iv, crypt->key, outputencryption);
}

/* PDF 2.0 algorithm 10 */
static void
pdf_compute_permissions_r6(fz_context *ctx, pdf_crypt *crypt, unsigned char *output)
{
	unsigned char buf[16];
	unsigned char iv[16];
	fz_aes aes;

	/* Steps a) and b) - Extend permissions field and put into lower order bytes. */
	memcpy(buf, (unsigned char *) &crypt->p, 4);
	memset(&buf[4], 0xff, 4);

	/* Step c) - Encode EncryptMetadata as T/F. */
	buf[8] = crypt->encrypt_metadata ? 'T' : 'F';

	/* Step d) - Encode ASCII characters "adb". */
	buf[9] = 'a';
	buf[10] = 'd';
	buf[11] = 'b';

	/* Step e) - Encode 4 random bytes. */
	fz_memrnd(ctx, &buf[12], 4);

	/* Step f) - Use file encryption key as AES-key when encrypting buffer. */
	memset(iv, 0, sizeof(iv));
	if (fz_aes_setkey_enc(&aes, crypt->key, 256))
		fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=256)");
	fz_aes_crypt_cbc(&aes, FZ_AES_ENCRYPT, 16, iv, buf, output);
}

pdf_crypt *
pdf_new_encrypt(fz_context *ctx, const char *opwd_utf8, const char *upwd_utf8, pdf_obj *id, int permissions, int algorithm)
{
	pdf_crypt *crypt;
	int v, r, method, length;
	unsigned char opwd[2048];
	unsigned char upwd[2048];
	size_t opwdlen, upwdlen;

	crypt = fz_malloc_struct(ctx, pdf_crypt);

	/* Extract file identifier string */

	if (pdf_is_string(ctx, id))
		crypt->id = pdf_keep_obj(ctx, id);
	else
		fz_warn(ctx, "missing file identifier, may not be able to do decryption");

	switch (algorithm)
	{
	case PDF_ENCRYPT_RC4_40:
		v = 1; r = 2; method = PDF_CRYPT_RC4; length = 40; break;
	case PDF_ENCRYPT_RC4_128:
		v = 2; r = 3; method = PDF_CRYPT_RC4; length = 128; break;
	case PDF_ENCRYPT_AES_128:
		v = 4; r = 4; method = PDF_CRYPT_AESV2; length = 128; break;
	case PDF_ENCRYPT_AES_256:
		v = 5; r = 6; method = PDF_CRYPT_AESV3; length = 256; break;
	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid encryption method");
	}

	crypt->v = v;
	crypt->r = r;
	crypt->length = length;
	crypt->cf = NULL;
	crypt->stmf.method = method;
	crypt->stmf.length = length;
	crypt->strf.method = method;
	crypt->strf.length = length;
	crypt->encrypt_metadata = 1;
	crypt->p = (permissions & 0xf3c) | 0xfffff0c0;
	memset(crypt->o, 0, sizeof (crypt->o));
	memset(crypt->u, 0, sizeof (crypt->u));
	memset(crypt->oe, 0, sizeof (crypt->oe));
	memset(crypt->ue, 0, sizeof (crypt->ue));

	if (crypt->r <= 4)
	{
		pdf_docenc_from_utf8((char *) opwd, opwd_utf8, sizeof opwd);
		pdf_docenc_from_utf8((char *) upwd, upwd_utf8, sizeof upwd);
	}
	else
	{
		pdf_saslprep_from_utf8((char *) opwd, opwd_utf8, sizeof opwd);
		pdf_saslprep_from_utf8((char *) upwd, upwd_utf8, sizeof upwd);
	}

	opwdlen = strlen((char *) opwd);
	upwdlen = strlen((char *) upwd);

	if (crypt->r <= 4)
	{
		pdf_compute_owner_password(ctx, crypt, opwd, opwdlen, upwd, upwdlen, crypt->o);
		pdf_compute_user_password(ctx, crypt, upwd, upwdlen, crypt->u);
	}
	else if (crypt->r == 6)
	{
		/* 7.6.4.4.1 states that the file encryption key are 256 random bits. */
		fz_memrnd(ctx, crypt->key, nelem(crypt->key));

		pdf_compute_user_password_r6(ctx, crypt, upwd, upwdlen, crypt->u, crypt->ue);
		pdf_compute_owner_password_r6(ctx, crypt, opwd, opwdlen, crypt->o, crypt->oe);
		pdf_compute_permissions_r6(ctx, crypt, crypt->perms);
	}

	return crypt;
}
