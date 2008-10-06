/*
 * Basic crypto functions.
 * Independent of the rest of fitz.
 * For further encapsulation in filters, or not.
 */

/* crc-32 checksum */

unsigned long fz_crc32(unsigned long crc, unsigned char *buf, int len);

/* md5 digests */

typedef struct fz_md5_s fz_md5;

struct fz_md5_s
{
	unsigned long state[4];
	unsigned long count[2];
	unsigned char buffer[64];
};

void fz_md5init(fz_md5 *state);
void fz_md5update(fz_md5 *state, const unsigned char *input, const unsigned inlen);
void fz_md5final(fz_md5 *state, unsigned char digest[16]);

/* arc4 crypto */

typedef struct fz_arc4_s fz_arc4;

struct fz_arc4_s
{
	unsigned x;
	unsigned y;
	unsigned char state[256];
};

void fz_arc4init(fz_arc4 *state, unsigned char *key, unsigned len);
unsigned char fz_arc4next(fz_arc4 *state);
void fz_arc4encrypt(fz_arc4 *state, unsigned char *dest, unsigned char *src, unsigned len);

/* aes crypto */

/* Minimal definition from libtomcrypt.h */

/* max size of either a cipher/hash block or symmetric key [largest of the two] */
#define MAXBLOCKSIZE  128

/* this is the "32-bit at least" data type
 * Re-define it to suit your platform but it must be at least 32-bits
 */
#if defined(__x86_64__) || (defined(__sparc__) && defined(__arch64__))
   typedef unsigned ulong32;
#else
   typedef unsigned long ulong32;
#endif

/* This part extracted from tomcrypt_cipher.h */
struct rijndael_key {
   ulong32 eK[60], dK[60];
   int Nr;
};

typedef union Symmetric_key {
   struct rijndael_key rijndael;
} symmetric_key;

/** A block cipher CBC structure */
typedef struct {
   /** The block size of the given cipher */
   int                 blocklen;
   /** The current IV */
   unsigned char       IV[MAXBLOCKSIZE];
   /** The scheduled key */
   symmetric_key       key;
} symmetric_CBC;

/* End of part extracted from tomcrypt_cipher.h */

typedef struct fz_aes_s fz_aes;

struct fz_aes_s
{
	/* For aes IV is first 16 bytes of string/stream data so we must
	   know whether IV is not yet set (first iteration of encrypt)
	   or set (subsequent iterations) */
	int ivinited;
	symmetric_CBC cbckey;
};

void fz_aesinit(fz_aes *state, unsigned char *key, unsigned len);
void fz_setiv(fz_aes *state, unsigned char *iv);
void fz_aesdecrypt(fz_aes *state, unsigned char *dest, unsigned char *src, unsigned len);

/* TODO: sha1 */
