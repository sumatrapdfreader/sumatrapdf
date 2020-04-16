#ifndef MUPDF_FITZ_CRYPT_H
#define MUPDF_FITZ_CRYPT_H

#include "mupdf/fitz/system.h"

/* md5 digests */

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
typedef struct
{
	unsigned int state[4];
	unsigned int count[2];
	unsigned char buffer[64];
} fz_md5;

/*
	MD5 initialization. Begins an MD5 operation, writing a new
	context.

	Never throws an exception.
*/
void fz_md5_init(fz_md5 *state);

/*
	MD5 block update operation. Continues an MD5 message-digest
	operation, processing another message block, and updating the
	context.

	Never throws an exception.
*/
void fz_md5_update(fz_md5 *state, const unsigned char *input, size_t inlen);

/*
	MD5 finalization. Ends an MD5 message-digest operation, writing
	the message digest and zeroizing the context.

	Never throws an exception.
*/
void fz_md5_final(fz_md5 *state, unsigned char digest[16]);

/* sha-256 digests */

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
typedef struct
{
	unsigned int state[8];
	unsigned int count[2];
	union {
		unsigned char u8[64];
		unsigned int u32[16];
	} buffer;
} fz_sha256;

/*
	SHA256 initialization. Begins an SHA256 operation, initialising
	the supplied context.

	Never throws an exception.
*/
void fz_sha256_init(fz_sha256 *state);

/*
	SHA256 block update operation. Continues an SHA256 message-
	digest operation, processing another message block, and updating
	the context.

	Never throws an exception.
*/
void fz_sha256_update(fz_sha256 *state, const unsigned char *input, size_t inlen);

/*
	MD5 finalization. Ends an MD5 message-digest operation, writing
	the message digest and zeroizing the context.

	Never throws an exception.
*/
void fz_sha256_final(fz_sha256 *state, unsigned char digest[32]);

/* sha-512 digests */

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
typedef struct
{
	uint64_t state[8];
	unsigned int count[2];
	union {
		unsigned char u8[128];
		uint64_t u64[16];
	} buffer;
} fz_sha512;

/*
	SHA512 initialization. Begins an SHA512 operation, initialising
	the supplied context.

	Never throws an exception.
*/
void fz_sha512_init(fz_sha512 *state);

/*
	SHA512 block update operation. Continues an SHA512 message-
	digest operation, processing another message block, and updating
	the context.

	Never throws an exception.
*/
void fz_sha512_update(fz_sha512 *state, const unsigned char *input, size_t inlen);

/*
	SHA512 finalization. Ends an SHA512 message-digest operation,
	writing the message digest and zeroizing the context.

	Never throws an exception.
*/
void fz_sha512_final(fz_sha512 *state, unsigned char digest[64]);

/* sha-384 digests */

typedef fz_sha512 fz_sha384;

/*
	SHA384 initialization. Begins an SHA384 operation, initialising
	the supplied context.

	Never throws an exception.
*/
void fz_sha384_init(fz_sha384 *state);

/*
	SHA384 block update operation. Continues an SHA384 message-
	digest operation, processing another message block, and updating
	the context.

	Never throws an exception.
*/
void fz_sha384_update(fz_sha384 *state, const unsigned char *input, size_t inlen);

/*
	SHA384 finalization. Ends an SHA384 message-digest operation,
	writing the message digest and zeroizing the context.

	Never throws an exception.
*/
void fz_sha384_final(fz_sha384 *state, unsigned char digest[64]);

/* arc4 crypto */

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
typedef struct
{
	unsigned x;
	unsigned y;
	unsigned char state[256];
} fz_arc4;

/*
	RC4 initialization. Begins an RC4 operation, writing a new
	context.

	Never throws an exception.
*/
void fz_arc4_init(fz_arc4 *state, const unsigned char *key, size_t len);

/*
	RC4 block encrypt operation; encrypt src into dst (both of
	length len) updating the RC4 state as we go.

	Never throws an exception.
*/
void fz_arc4_encrypt(fz_arc4 *state, unsigned char *dest, const unsigned char *src, size_t len);

/*
	RC4 finalization. Zero the context.

	Never throws an exception.
*/
void fz_arc4_final(fz_arc4 *state);

/* AES block cipher implementation from XYSSL */

/*
	Structure definitions are public to enable stack
	based allocation. Do not access the members directly.
*/
typedef struct
{
	int nr; /* number of rounds */
	unsigned long *rk; /* AES round keys */
	unsigned long buf[68]; /* unaligned data */
} fz_aes;

#define FZ_AES_DECRYPT 0
#define FZ_AES_ENCRYPT 1

/*
	AES encryption intialisation. Fills in the supplied context
	and prepares for encryption using the given key.

	Returns non-zero for error (key size other than 128/192/256).

	Never throws an exception.
*/
int fz_aes_setkey_enc(fz_aes *ctx, const unsigned char *key, int keysize);

/*
	AES decryption intialisation. Fills in the supplied context
	and prepares for decryption using the given key.

	Returns non-zero for error (key size other than 128/192/256).

	Never throws an exception.
*/
int fz_aes_setkey_dec(fz_aes *ctx, const unsigned char *key, int keysize);

/*
	AES block processing. Encrypts or Decrypts (according to mode,
	which must match what was initially set up) length bytes (which
	must be a multiple of 16), using (and modifying) the insertion
	vector iv, reading from input, and writing to output.

	Never throws an exception.
*/
void fz_aes_crypt_cbc(fz_aes *ctx, int mode, size_t length,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output );

#endif
