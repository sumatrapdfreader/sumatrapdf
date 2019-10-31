#ifndef MUPDF_FITZ_CRYPT_H
#define MUPDF_FITZ_CRYPT_H

#include "mupdf/fitz/system.h"

/* md5 digests */

typedef struct fz_md5_s fz_md5;

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
struct fz_md5_s
{
	unsigned int state[4];
	unsigned int count[2];
	unsigned char buffer[64];
};

void fz_md5_init(fz_md5 *state);
void fz_md5_update(fz_md5 *state, const unsigned char *input, size_t inlen);
void fz_md5_final(fz_md5 *state, unsigned char digest[16]);

/* sha-256 digests */

typedef struct fz_sha256_s fz_sha256;

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
struct fz_sha256_s
{
	unsigned int state[8];
	unsigned int count[2];
	union {
		unsigned char u8[64];
		unsigned int u32[16];
	} buffer;
};

void fz_sha256_init(fz_sha256 *state);
void fz_sha256_update(fz_sha256 *state, const unsigned char *input, size_t inlen);
void fz_sha256_final(fz_sha256 *state, unsigned char digest[32]);

/* sha-512 digests */

typedef struct fz_sha512_s fz_sha512;

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
struct fz_sha512_s
{
	uint64_t state[8];
	unsigned int count[2];
	union {
		unsigned char u8[128];
		uint64_t u64[16];
	} buffer;
};

void fz_sha512_init(fz_sha512 *state);
void fz_sha512_update(fz_sha512 *state, const unsigned char *input, size_t inlen);
void fz_sha512_final(fz_sha512 *state, unsigned char digest[64]);

/* sha-384 digests */

typedef struct fz_sha512_s fz_sha384;

void fz_sha384_init(fz_sha384 *state);
void fz_sha384_update(fz_sha384 *state, const unsigned char *input, size_t inlen);
void fz_sha384_final(fz_sha384 *state, unsigned char digest[64]);

/* arc4 crypto */

typedef struct fz_arc4_s fz_arc4;

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
struct fz_arc4_s
{
	unsigned x;
	unsigned y;
	unsigned char state[256];
};

void fz_arc4_init(fz_arc4 *state, const unsigned char *key, size_t len);
void fz_arc4_encrypt(fz_arc4 *state, unsigned char *dest, const unsigned char *src, size_t len);

/* AES block cipher implementation from XYSSL */

typedef struct fz_aes_s fz_aes;

#define FZ_AES_DECRYPT 0
#define FZ_AES_ENCRYPT 1

/*
	Structure definition is public to enable stack
	based allocation. Do not access the members directly.
*/
struct fz_aes_s
{
	int nr; /* number of rounds */
	unsigned long *rk; /* AES round keys */
	unsigned long buf[68]; /* unaligned data */
};

int fz_aes_setkey_enc( fz_aes *ctx, const unsigned char *key, int keysize );
int fz_aes_setkey_dec( fz_aes *ctx, const unsigned char *key, int keysize );
void fz_aes_crypt_cbc( fz_aes *ctx, int mode, size_t length,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output );

#endif
