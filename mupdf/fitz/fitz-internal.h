#ifndef FITZ_INTERNAL_H
#define FITZ_INTERNAL_H

#include "fitz.h"

#ifdef _WIN32 /* Microsoft Visual C++ */

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef __int64 int64_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

#else
#include <inttypes.h>
#endif

struct fz_warn_context_s
{
	char message[256];
	int count;
};

fz_context *fz_clone_context_internal(fz_context *ctx);

void fz_new_aa_context(fz_context *ctx);
void fz_free_aa_context(fz_context *ctx);
void fz_copy_aa_context(fz_context *dst, fz_context *src);

/* Default allocator */
extern fz_alloc_context fz_alloc_default;

/* Default locks */
extern fz_locks_context fz_locks_default;

#if defined(MEMENTO) || defined(DEBUG)
#define FITZ_DEBUG_LOCKING
#endif

#ifdef FITZ_DEBUG_LOCKING

void fz_assert_lock_held(fz_context *ctx, int lock);
void fz_assert_lock_not_held(fz_context *ctx, int lock);
void fz_lock_debug_lock(fz_context *ctx, int lock);
void fz_lock_debug_unlock(fz_context *ctx, int lock);

#else

#define fz_assert_lock_held(A,B) do { } while (0)
#define fz_assert_lock_not_held(A,B) do { } while (0)
#define fz_lock_debug_lock(A,B) do { } while (0)
#define fz_lock_debug_unlock(A,B) do { } while (0)

#endif /* !FITZ_DEBUG_LOCKING */

static inline void
fz_lock(fz_context *ctx, int lock)
{
	fz_lock_debug_lock(ctx, lock);
	ctx->locks->lock(ctx->locks->user, lock);
}

static inline void
fz_unlock(fz_context *ctx, int lock)
{
	fz_lock_debug_unlock(ctx, lock);
	ctx->locks->unlock(ctx->locks->user, lock);
}

/* SumatraPDF: basic global synchronizing */
#ifdef _WIN32
void fz_synchronize_begin();
void fz_synchronize_end();
#endif

/* ARM assembly specific defines */

#ifdef ARCH_ARM
#ifdef NDK_PROFILER
extern void __gnu_mcount_nc(void);
#define ENTER_PG "push {lr}\nbl __gnu_mcount_nc\n"
#else
#define ENTER_PG
#endif

/* If we're compiling as thumb code, then we need to tell the compiler
 * to enter and exit ARM mode around our assembly sections. If we move
 * the ARM functions to a separate file and arrange for it to be compiled
 * without thumb mode, we can save some time on entry.
 */
#ifdef ARCH_THUMB
#define ENTER_ARM ".balign 4\nmov r12,pc\nbx r12\n0:.arm\n" ENTER_PG
#define ENTER_THUMB "9:.thumb\n" ENTER_PG
#else
#define ENTER_ARM
#define ENTER_THUMB
#endif
#endif

/*
 * Basic runtime and utility functions
 */

#ifdef CLUSTER
#define LOCAL_TRIG_FNS
#endif

#ifdef LOCAL_TRIG_FNS
/*
 * Trig functions
 */
static float
my_atan_table[258] =
{
0.0000000000f, 0.00390623013f,0.00781234106f,0.0117182136f,
0.0156237286f, 0.0195287670f, 0.0234332099f, 0.0273369383f,
0.0312398334f, 0.0351417768f, 0.0390426500f, 0.0429423347f,
0.0468407129f, 0.0507376669f, 0.0546330792f, 0.0585268326f,
0.0624188100f, 0.0663088949f, 0.0701969711f, 0.0740829225f,
0.0779666338f, 0.0818479898f, 0.0857268758f, 0.0896031775f,
0.0934767812f, 0.0973475735f, 0.1012154420f, 0.1050802730f,
0.1089419570f, 0.1128003810f, 0.1166554350f, 0.1205070100f,
0.1243549950f, 0.1281992810f, 0.1320397620f, 0.1358763280f,
0.1397088740f, 0.1435372940f, 0.1473614810f, 0.1511813320f,
0.1549967420f, 0.1588076080f, 0.1626138290f, 0.1664153010f,
0.1702119250f, 0.1740036010f, 0.1777902290f, 0.1815717110f,
0.1853479500f, 0.1891188490f, 0.1928843120f, 0.1966442450f,
0.2003985540f, 0.2041471450f, 0.2078899270f, 0.2116268090f,
0.2153577000f, 0.2190825110f, 0.2228011540f, 0.2265135410f,
0.2302195870f, 0.2339192060f, 0.2376123140f, 0.2412988270f,
0.2449786630f, 0.2486517410f, 0.2523179810f, 0.2559773030f,
0.2596296290f, 0.2632748830f, 0.2669129880f, 0.2705438680f,
0.2741674510f, 0.2777836630f, 0.2813924330f, 0.2849936890f,
0.2885873620f, 0.2921733830f, 0.2957516860f, 0.2993222020f,
0.3028848680f, 0.3064396190f, 0.3099863910f, 0.3135251230f,
0.3170557530f, 0.3205782220f, 0.3240924700f, 0.3275984410f,
0.3310960770f, 0.3345853220f, 0.3380661230f, 0.3415384250f,
0.3450021770f, 0.3484573270f, 0.3519038250f, 0.3553416220f,
0.3587706700f, 0.3621909220f, 0.3656023320f, 0.3690048540f,
0.3723984470f, 0.3757830650f, 0.3791586690f, 0.3825252170f,
0.3858826690f, 0.3892309880f, 0.3925701350f, 0.3959000740f,
0.3992207700f, 0.4025321870f, 0.4058342930f, 0.4091270550f,
0.4124104420f, 0.4156844220f, 0.4189489670f, 0.4222040480f,
0.4254496370f, 0.4286857080f, 0.4319122350f, 0.4351291940f,
0.4383365600f, 0.4415343100f, 0.4447224240f, 0.4479008790f,
0.4510696560f, 0.4542287350f, 0.4573780990f, 0.4605177290f,
0.4636476090f, 0.4667677240f, 0.4698780580f, 0.4729785980f,
0.4760693300f, 0.4791502430f, 0.4822213240f, 0.4852825630f,
0.4883339510f, 0.4913754780f, 0.4944071350f, 0.4974289160f,
0.5004408130f, 0.5034428210f, 0.5064349340f, 0.5094171490f,
0.5123894600f, 0.5153518660f, 0.5183043630f, 0.5212469510f,
0.5241796290f, 0.5271023950f, 0.5300152510f, 0.5329181980f,
0.5358112380f, 0.5386943730f, 0.5415676050f, 0.5444309400f,
0.5472843810f, 0.5501279330f, 0.5529616020f, 0.5557853940f,
0.5585993150f, 0.5614033740f, 0.5641975770f, 0.5669819340f,
0.5697564530f, 0.5725211450f, 0.5752760180f, 0.5780210840f,
0.5807563530f, 0.5834818390f, 0.5861975510f, 0.5889035040f,
0.5915997100f, 0.5942861830f, 0.5969629370f, 0.5996299860f,
0.6022873460f, 0.6049350310f, 0.6075730580f, 0.6102014430f,
0.6128202020f, 0.6154293530f, 0.6180289120f, 0.6206188990f,
0.6231993300f, 0.6257702250f, 0.6283316020f, 0.6308834820f,
0.6334258830f, 0.6359588250f, 0.6384823300f, 0.6409964180f,
0.6435011090f, 0.6459964250f, 0.6484823880f, 0.6509590190f,
0.6534263410f, 0.6558843770f, 0.6583331480f, 0.6607726790f,
0.6632029930f, 0.6656241120f, 0.6680360620f, 0.6704388650f,
0.6728325470f, 0.6752171330f, 0.6775926450f, 0.6799591110f,
0.6823165550f, 0.6846650020f, 0.6870044780f, 0.6893350100f,
0.6916566220f, 0.6939693410f, 0.6962731940f, 0.6985682070f,
0.7008544080f, 0.7031318220f, 0.7054004770f, 0.7076604000f,
0.7099116190f, 0.7121541600f, 0.7143880520f, 0.7166133230f,
0.7188300000f, 0.7210381110f, 0.7232376840f, 0.7254287490f,
0.7276113330f, 0.7297854640f, 0.7319511710f, 0.7341084830f,
0.7362574290f, 0.7383980370f, 0.7405303370f, 0.7426543560f,
0.7447701260f, 0.7468776740f, 0.7489770290f, 0.7510682220f,
0.7531512810f, 0.7552262360f, 0.7572931160f, 0.7593519510f,
0.7614027700f, 0.7634456020f, 0.7654804790f, 0.7675074280f,
0.7695264800f, 0.7715376650f, 0.7735410110f, 0.7755365500f,
0.7775243100f, 0.7795043220f, 0.7814766150f, 0.7834412190f,
0.7853981630f, 0.7853981630f /* Extended by 1 for interpolation */
};

static inline float my_sinf(float x)
{
	float x2, xn;
	int i;
	/* Map x into the -PI to PI range. We could do this using:
	 * x = fmodf(x, (float)(2.0 * M_PI));
	 * but that's C99, and seems to misbehave with negative numbers
	 * on some platforms. */
	x -= (float)M_PI;
	i = x / (float)(2.0f * M_PI);
	x -= i * (float)(2.0f * M_PI);
	if (x < 0.0f)
		x += (float)(2.0f * M_PI);
	x -= (float)M_PI;
	if (x <= (float)(-M_PI/2.0))
		x = -(float)M_PI-x;
	else if (x >= (float)(M_PI/2.0))
		x = (float)M_PI-x;
	x2 = x*x;
	xn = x*x2/6.0f;
	x -= xn;
	xn *= x2/20.0f;
	x += xn;
	xn *= x2/42.0f;
	x -= xn;
	xn *= x2/72.0f;
	x += xn;
	return x;
}

static inline float my_atan2f(float o, float a)
{
	int negate = 0, flip = 0, i;
	float r, s;
	if (o == 0.0f)
	{
		if (a > 0)
			return 0.0f;
		else
			return (float)M_PI;
	}
	if (o < 0)
		o = -o, negate = 1;
	if (a < 0)
		a = -a, flip = 1;
	if (o < a)
		i = (int)(65536.0f*o/a + 0.5f);
	else
		i = (int)(65536.0f*a/o + 0.5f);
	r = my_atan_table[i>>8];
	s = my_atan_table[(i>>8)+1];
	r += (s-r)*(i&255)/256.0f;
	if (o >= a)
		r = (float)(M_PI/2.0f) - r;
	if (flip)
		r = (float)M_PI - r;
	if (negate)
		r = -r;
	return r;
}

#define sinf(x) my_sinf(x)
#define cosf(x) my_sinf(((float)(M_PI/2.0f)) + (x))
#define atan2f(x,y) my_atan2f((x),(y))
#endif

/* Range checking atof */
float fz_atof(const char *s);

/* atoi that copes with NULL */
int fz_atoi(const char *s);

/*
 * Generic hash-table with fixed-length keys.
 */

typedef struct fz_hash_table_s fz_hash_table;

fz_hash_table *fz_new_hash_table(fz_context *ctx, int initialsize, int keylen, int lock);
void fz_empty_hash(fz_context *ctx, fz_hash_table *table);
void fz_free_hash(fz_context *ctx, fz_hash_table *table);

void *fz_hash_find(fz_context *ctx, fz_hash_table *table, void *key);
void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, void *key, void *val);
void *fz_hash_insert_with_pos(fz_context *ctx, fz_hash_table *table, void *key, void *val, unsigned *pos);
void fz_hash_remove(fz_context *ctx, fz_hash_table *table, void *key);
void fz_hash_remove_fast(fz_context *ctx, fz_hash_table *table, void *key, unsigned pos);

int fz_hash_len(fz_context *ctx, fz_hash_table *table);
void *fz_hash_get_key(fz_context *ctx, fz_hash_table *table, int idx);
void *fz_hash_get_val(fz_context *ctx, fz_hash_table *table, int idx);

#ifndef NDEBUG
void fz_print_hash(fz_context *ctx, FILE *out, fz_hash_table *table);
#endif

/*
 * Math and geometry
 */

/* Multiply scaled two integers in the 0..255 range */
static inline int fz_mul255(int a, int b)
{
	/* see Jim Blinn's book "Dirty Pixels" for how this works */
	int x = a * b + 128;
	x += x >> 8;
	return x >> 8;
}

/* Expand a value A from the 0...255 range to the 0..256 range */
#define FZ_EXPAND(A) ((A)+((A)>>7))

/* Combine values A (in any range) and B (in the 0..256 range),
 * to give a single value in the same range as A was. */
#define FZ_COMBINE(A,B) (((A)*(B))>>8)

/* Combine values A and C (in the same (any) range) and B and D (in the
 * 0..256 range), to give a single value in the same range as A and C were. */
#define FZ_COMBINE2(A,B,C,D) (FZ_COMBINE((A), (B)) + FZ_COMBINE((C), (D)))

/* Blend SRC and DST (in the same range) together according to
 * AMOUNT (in the 0...256 range). */
#define FZ_BLEND(SRC, DST, AMOUNT) ((((SRC)-(DST))*(AMOUNT) + ((DST)<<8))>>8)

void fz_gridfit_matrix(fz_matrix *m);
float fz_matrix_max_expansion(const fz_matrix *m);

/*
 * Basic crypto functions.
 * Independent of the rest of fitz.
 * For further encapsulation in filters, or not.
 */

/* md5 digests */

typedef struct fz_md5_s fz_md5;

struct fz_md5_s
{
	unsigned int state[4];
	unsigned int count[2];
	unsigned char buffer[64];
};

void fz_md5_init(fz_md5 *state);
void fz_md5_update(fz_md5 *state, const unsigned char *input, unsigned inlen);
void fz_md5_final(fz_md5 *state, unsigned char digest[16]);

/* sha-256 digests */

typedef struct fz_sha256_s fz_sha256;

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
void fz_sha256_update(fz_sha256 *state, const unsigned char *input, unsigned int inlen);
void fz_sha256_final(fz_sha256 *state, unsigned char digest[32]);

/* sha-512 digests */

typedef struct fz_sha512_s fz_sha512;

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
void fz_sha512_update(fz_sha512 *state, const unsigned char *input, unsigned int inlen);
void fz_sha512_final(fz_sha512 *state, unsigned char digest[64]);

/* sha-384 digests */

typedef struct fz_sha512_s fz_sha384;

void fz_sha384_init(fz_sha384 *state);
void fz_sha384_update(fz_sha384 *state, const unsigned char *input, unsigned int inlen);
void fz_sha384_final(fz_sha384 *state, unsigned char digest[64]);

/* arc4 crypto */

typedef struct fz_arc4_s fz_arc4;

struct fz_arc4_s
{
	unsigned x;
	unsigned y;
	unsigned char state[256];
};

void fz_arc4_init(fz_arc4 *state, const unsigned char *key, unsigned len);
void fz_arc4_encrypt(fz_arc4 *state, unsigned char *dest, const unsigned char *src, unsigned len);

/* AES block cipher implementation from XYSSL */

typedef struct fz_aes_s fz_aes;

#define AES_DECRYPT 0
#define AES_ENCRYPT 1

struct fz_aes_s
{
	int nr; /* number of rounds */
	unsigned long *rk; /* AES round keys */
	unsigned long buf[68]; /* unaligned data */
};

int aes_setkey_enc( fz_aes *ctx, const unsigned char *key, int keysize );
int aes_setkey_dec( fz_aes *ctx, const unsigned char *key, int keysize );
void aes_crypt_cbc( fz_aes *ctx, int mode, int length,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output );

/*
	Resource store

	MuPDF stores decoded "objects" into a store for potential reuse.
	If the size of the store gets too big, objects stored within it can
	be evicted and freed to recover space. When MuPDF comes to decode
	such an object, it will check to see if a version of this object is
	already in the store - if it is, it will simply reuse it. If not, it
	will decode it and place it into the store.

	All objects that can be placed into the store are derived from the
	fz_storable type (i.e. this should be the first component of the
	objects structure). This allows for consistent (thread safe)
	reference counting, and includes a function that will be called to
	free the object as soon as the reference count reaches zero.

	Most objects offer fz_keep_XXXX/fz_drop_XXXX functions derived
	from fz_keep_storable/fz_drop_storable. Creation of such objects
	includes a call to FZ_INIT_STORABLE to set up the fz_storable header.
 */

typedef struct fz_storable_s fz_storable;

typedef void (fz_store_free_fn)(fz_context *, fz_storable *);

struct fz_storable_s {
	int refs;
	fz_store_free_fn *free;
};

#define FZ_INIT_STORABLE(S_,RC,FREE) \
	do { fz_storable *S = &(S_)->storable; S->refs = (RC); \
	S->free = (FREE); \
	} while (0)

void *fz_keep_storable(fz_context *, fz_storable *);
void fz_drop_storable(fz_context *, fz_storable *);

/*
	The store can be seen as a dictionary that maps keys to fz_storable
	values. In order to allow keys of different types to be stored, we
	have a structure full of functions for each key 'type'; this
	fz_store_type pointer is stored with each key, and tells the store
	how to perform certain operations (like taking/dropping a reference,
	comparing two keys, outputting details for debugging etc).

	The store uses a hash table internally for speed where possible. In
	order for this to work, we need a mechanism for turning a generic
	'key' into 'a hashable string'. For this purpose the type structure
	contains a make_hash_key function pointer that maps from a void *
	to an fz_store_hash structure. If make_hash_key function returns 0,
	then the key is determined not to be hashable, and the value is
	not stored in the hash table.
*/
typedef struct fz_store_hash_s fz_store_hash;

struct fz_store_hash_s
{
	fz_store_free_fn *free;
	union
	{
		struct
		{
			int i0;
			int i1;
		} i;
		struct
		{
			void *ptr;
			int i;
		} pi;
		struct
		{
			int id;
			float m[4];
		} im;
	} u;
};

typedef struct fz_store_type_s fz_store_type;

struct fz_store_type_s
{
	int (*make_hash_key)(fz_store_hash *, void *);
	void *(*keep_key)(fz_context *,void *);
	void (*drop_key)(fz_context *,void *);
	int (*cmp_key)(void *, void *);
#ifndef NDEBUG
	void (*debug)(FILE *, void *);
#endif
};

/*
	fz_store_new_context: Create a new store inside the context

	max: The maximum size (in bytes) that the store is allowed to grow
	to. FZ_STORE_UNLIMITED means no limit.
*/
void fz_new_store_context(fz_context *ctx, unsigned int max);

/*
	fz_drop_store_context: Drop a reference to the store.
*/
void fz_drop_store_context(fz_context *ctx);

/*
	fz_keep_store_context: Take a reference to the store.
*/
fz_store *fz_keep_store_context(fz_context *ctx);

/*
	fz_store_item: Add an item to the store.

	Add an item into the store, returning NULL for success. If an item
	with the same key is found in the store, then our item will not be
	inserted, and the function will return a pointer to that value
	instead. This function takes its own reference to val, as required
	(i.e. the caller maintains ownership of its own reference).

	key: The key to use to index the item.

	val: The value to store.

	itemsize: The size in bytes of the value (as counted towards the
	store size).

	type: Functions used to manipulate the key.
*/
void *fz_store_item(fz_context *ctx, void *key, void *val, unsigned int itemsize, fz_store_type *type);

/*
	fz_find_item: Find an item within the store.

	free: The function used to free the value (to ensure we get a value
	of the correct type).

	key: The key to use to index the item.

	type: Functions used to manipulate the key.

	Returns NULL for not found, otherwise returns a pointer to the value
	indexed by key to which a reference has been taken.
*/
void *fz_find_item(fz_context *ctx, fz_store_free_fn *free, void *key, fz_store_type *type);

/*
	fz_remove_item: Remove an item from the store.

	If an item indexed by the given key exists in the store, remove it.

	free: The function used to free the value (to ensure we get a value
	of the correct type).

	key: The key to use to find the item to remove.

	type: Functions used to manipulate the key.
*/
void fz_remove_item(fz_context *ctx, fz_store_free_fn *free, void *key, fz_store_type *type);

/*
	fz_empty_store: Evict everything from the store.
*/
void fz_empty_store(fz_context *ctx);

/*
	fz_store_scavenge: Internal function used as part of the scavenging
	allocator; when we fail to allocate memory, before returning a
	failure to the caller, we try to scavenge space within the store by
	evicting at least 'size' bytes. The allocator then retries.

	size: The number of bytes we are trying to have free.

	phase: What phase of the scavenge we are in. Updated on exit.

	Returns non zero if we managed to free any memory.
*/
int fz_store_scavenge(fz_context *ctx, unsigned int size, int *phase);

/*
	fz_print_store: Dump the contents of the store for debugging.
*/
#ifndef NDEBUG
void fz_print_store(fz_context *ctx, FILE *out);
#endif

struct fz_buffer_s
{
	int refs;
	unsigned char *data;
	int cap, len;
	int unused_bits;
};

/*
	fz_new_buffer: Create a new buffer.

	capacity: Initial capacity.

	Returns pointer to new buffer. Throws exception on allocation
	failure.
*/
fz_buffer *fz_new_buffer(fz_context *ctx, int capacity);

/*
	fz_resize_buffer: Ensure that a buffer has a given capacity,
	truncating data if required.

	buf: The buffer to alter.

	capacity: The desired capacity for the buffer. If the current size
	of the buffer contents is smaller than capacity, it is truncated.

*/
void fz_resize_buffer(fz_context *ctx, fz_buffer *buf, int capacity);

/*
	fz_grow_buffer: Make some space within a buffer (i.e. ensure that
	capacity > size).

	buf: The buffer to grow.

	May throw exception on failure to allocate.
*/
void fz_grow_buffer(fz_context *ctx, fz_buffer *buf);

/*
	fz_trim_buffer: Trim wasted capacity from a buffer.

	buf: The buffer to trim.
*/
void fz_trim_buffer(fz_context *ctx, fz_buffer *buf);

/*
	fz_buffer_cat: Concatenate buffers

	buf: first to concatenate and the holder of the result
	extra: second to concatenate

	May throw exception on failure to allocate.
*/
void fz_buffer_cat(fz_context *ctx, fz_buffer *buf, fz_buffer *extra);

void fz_write_buffer(fz_context *ctx, fz_buffer *buf, unsigned char *data, int len);

void fz_write_buffer_byte(fz_context *ctx, fz_buffer *buf, int val);

void fz_write_buffer_rune(fz_context *ctx, fz_buffer *buf, int val);

void fz_write_buffer_bits(fz_context *ctx, fz_buffer *buf, int val, int bits);

void fz_write_buffer_pad(fz_context *ctx, fz_buffer *buf);

/*
	fz_buffer_printf: print formatted to a buffer. The buffer will grow
	as required.
*/
int fz_buffer_printf(fz_context *ctx, fz_buffer *buffer, const char *fmt, ...);
int fz_buffer_vprintf(fz_context *ctx, fz_buffer *buffer, const char *fmt, va_list args);

/*
	fz_buffer_printf: print a string formatted as a pdf string to a buffer.
	The buffer will grow.
*/
void
fz_buffer_cat_pdf_string(fz_context *ctx, fz_buffer *buffer, const char *text);

struct fz_stream_s
{
	fz_context *ctx;
	int refs;
	int error;
	int eof;
	int pos;
	int avail;
	int bits;
	unsigned char *bp, *rp, *wp, *ep;
	void *state;
	int (*read)(fz_stream *stm, unsigned char *buf, int len);
	void (*close)(fz_context *ctx, void *state);
	void (*seek)(fz_stream *stm, int offset, int whence);
	/* SumatraPDF: allow to clone a stream */
	fz_stream *(*reopen)(fz_context *ctx, fz_stream *stm);
	unsigned char buf[4096];
};

fz_stream *fz_new_stream(fz_context *ctx, void*, int(*)(fz_stream*, unsigned char*, int), void(*)(fz_context *, void *));
fz_stream *fz_keep_stream(fz_stream *stm);
void fz_fill_buffer(fz_stream *stm);

/*
	fz_read_best: Attempt to read a stream into a buffer. If truncated
	is NULL behaves as fz_read_all, otherwise does not throw exceptions
	in the case of failure, but instead sets a truncated flag.

	stm: The stream to read from.

	initial: Suggested initial size for the buffer.

	truncated: Flag to store success/failure indication in.

	Returns a buffer created from reading from the stream.
*/
fz_buffer *fz_read_best(fz_stream *stm, int initial, int *truncated);

void fz_read_line(fz_stream *stm, char *buf, int max);

static inline int fz_read_byte(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		fz_fill_buffer(stm);
		return stm->rp < stm->wp ? *stm->rp++ : EOF;
	}
	return *stm->rp++;
}

static inline int fz_peek_byte(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		fz_fill_buffer(stm);
		return stm->rp < stm->wp ? *stm->rp : EOF;
	}
	return *stm->rp;
}

static inline void fz_unread_byte(fz_stream *stm)
{
	if (stm->rp > stm->bp)
		stm->rp--;
}

static inline int fz_is_eof(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		if (stm->eof)
			return 1;
		return fz_peek_byte(stm) == EOF;
	}
	return 0;
}

static inline unsigned int fz_read_bits(fz_stream *stm, int n)
{
	unsigned int x;

	if (n <= stm->avail)
	{
		stm->avail -= n;
		x = (stm->bits >> stm->avail) & ((1 << n) - 1);
	}
	else
	{
		x = stm->bits & ((1 << stm->avail) - 1);
		n -= stm->avail;
		stm->avail = 0;

		while (n > 8)
		{
			x = (x << 8) | fz_read_byte(stm);
			n -= 8;
		}

		if (n > 0)
		{
			stm->bits = fz_read_byte(stm);
			stm->avail = 8 - n;
			x = (x << n) | (stm->bits >> stm->avail);
		}
	}

	return x;
}

static inline void fz_sync_bits(fz_stream *stm)
{
	stm->avail = 0;
}

static inline int fz_is_eof_bits(fz_stream *stm)
{
	return fz_is_eof(stm) && (stm->avail == 0 || stm->bits == EOF);
}

/*
 * Data filters.
 */

fz_stream *fz_open_copy(fz_stream *chain);
fz_stream *fz_open_null(fz_stream *chain, int len, int offset);
fz_stream *fz_open_concat(fz_context *ctx, int max, int pad);
void fz_concat_push(fz_stream *concat, fz_stream *chain); /* Ownership of chain is passed in */
fz_stream *fz_open_arc4(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_aesd(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_a85d(fz_stream *chain);
fz_stream *fz_open_ahxd(fz_stream *chain);
fz_stream *fz_open_rld(fz_stream *chain);
fz_stream *fz_open_dctd(fz_stream *chain, int color_transform);
fz_stream *fz_open_resized_dctd(fz_stream *chain, int color_transform, int l2factor);
fz_stream *fz_open_faxd(fz_stream *chain,
	int k, int end_of_line, int encoded_byte_align,
	int columns, int rows, int end_of_block, int black_is_1);
fz_stream *fz_open_flated(fz_stream *chain);
fz_stream *fz_open_lzwd(fz_stream *chain, int early_change);
fz_stream *fz_open_predict(fz_stream *chain, int predictor, int columns, int colors, int bpc);
/* SumatraPDF: reuse JBIG2Globals */
typedef struct fz_jbig2_globals_s fz_jbig2_globals;
fz_jbig2_globals *fz_load_jbig2_globals(fz_context *ctx, unsigned char *data, int size);
void fz_free_jbig2_globals_imp(fz_context *ctx, fz_storable *globals);
fz_stream *fz_open_jbig2d(fz_stream *chain, fz_jbig2_globals *globals);

/*
 * Resources and other graphics related objects.
 */

/* SumatraPDF: make fz_shades use less memory */
enum { FZ_MAX_COLORS = 8 };

int fz_lookup_blendmode(char *name);
char *fz_blendmode_name(int blendmode);

struct fz_bitmap_s
{
	int refs;
	int w, h, stride, n;
	unsigned char *samples;
};

fz_bitmap *fz_new_bitmap(fz_context *ctx, int w, int h, int n);

void fz_bitmap_details(fz_bitmap *bitmap, int *w, int *h, int *n, int *stride);

void fz_clear_bitmap(fz_context *ctx, fz_bitmap *bit);

/*
	Pixmaps represent a set of pixels for a 2 dimensional region of a
	plane. Each pixel has n components per pixel, the last of which is
	always alpha. The data is in premultiplied alpha when rendering, but
	non-premultiplied for colorspace conversions and rescaling.

	x, y: The minimum x and y coord of the region in pixels.

	w, h: The width and height of the region in pixels.

	n: The number of color components in the image. Always
	includes a separate alpha channel. For mask images n=1, for greyscale
	(plus alpha) images n=2, for rgb (plus alpha) images n=3.

	interpolate: A boolean flag set to non-zero if the image
	will be drawn using linear interpolation, or set to zero if
	image will be using nearest neighbour sampling.

	xres, yres: Image resolution in dpi. Default is 96 dpi.

	colorspace: Pointer to a colorspace object describing the colorspace
	the pixmap is in. If NULL, the image is a mask.

	samples: A simple block of memory w * h * n bytes of memory in which
	the components are stored. The first n bytes are components 0 to n-1
	for the pixel at (x,y). Each successive n bytes gives another pixel
	in scanline order. Subsequent scanlines follow on with no padding.

	free_samples: Is zero when an application has provided its own
	buffer for pixel data through fz_new_pixmap_with_bbox_and_data.
	If not zero the buffer will be freed when fz_drop_pixmap is
	called for the pixmap.
*/
struct fz_pixmap_s
{
	fz_storable storable;
	int x, y, w, h, n;
	int interpolate;
	int xres, yres;
	fz_colorspace *colorspace;
	unsigned char *samples;
	int free_samples;
	int has_alpha; /* SumatraPDF: allow optimizing non-alpha pixmaps */
	int single_bit; /* SumatraPDF: allow optimizing 1-bit pixmaps */
};

void fz_free_pixmap_imp(fz_context *ctx, fz_storable *pix);

void fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, const fz_irect *r);
void fz_premultiply_pixmap(fz_context *ctx, fz_pixmap *pix);
fz_pixmap *fz_alpha_from_gray(fz_context *ctx, fz_pixmap *gray, int luminosity);
unsigned int fz_pixmap_size(fz_context *ctx, fz_pixmap *pix);

fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, fz_irect *clip);

typedef struct fz_scale_cache_s fz_scale_cache;

fz_scale_cache *fz_new_scale_cache(fz_context *ctx);
void fz_free_scale_cache(fz_context *ctx, fz_scale_cache *cache);
fz_pixmap *fz_scale_pixmap_cached(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip, fz_scale_cache *cache_x, fz_scale_cache *cache_y);

void fz_subsample_pixmap(fz_context *ctx, fz_pixmap *tile, int factor);

fz_irect *fz_pixmap_bbox_no_ctx(fz_pixmap *src, fz_irect *bbox);

typedef struct fz_compression_params_s fz_compression_params;

typedef struct fz_compressed_buffer_s fz_compressed_buffer;
unsigned int fz_compressed_buffer_size(fz_compressed_buffer *buffer);

fz_stream *fz_open_compressed_buffer(fz_context *ctx, fz_compressed_buffer *);
fz_stream *fz_open_image_decomp_stream(fz_context *ctx, fz_compressed_buffer *, int *l2factor);

enum
{
	FZ_IMAGE_UNKNOWN = 0,
	FZ_IMAGE_JPEG = 1,
	FZ_IMAGE_JPX = 2, /* Placeholder until supported */
	FZ_IMAGE_FAX = 3,
	FZ_IMAGE_JBIG2 = 4, /* Placeholder until supported */
	FZ_IMAGE_RAW = 5,
	FZ_IMAGE_RLD = 6,
	FZ_IMAGE_FLATE = 7,
	FZ_IMAGE_LZW = 8
};

struct fz_compression_params_s
{
	int type;
	union {
		struct {
			int color_transform;
		} jpeg;
		struct {
			int smask_in_data;
		} jpx;
		struct {
			int columns;
			int rows;
			int k;
			int end_of_line;
			int encoded_byte_align;
			int end_of_block;
			int black_is_1;
			int damaged_rows_before_error;
		} fax;
		struct
		{
			int columns;
			int colors;
			int predictor;
			int bpc;
		}
		flate;
		struct
		{
			int columns;
			int colors;
			int predictor;
			int bpc;
			int early_change;
		} lzw;
	} u;
};

struct fz_compressed_buffer_s
{
	fz_compression_params params;
	fz_buffer *buffer;
};

void fz_free_compressed_buffer(fz_context *ctx, fz_compressed_buffer *buf);

struct fz_image_s
{
	fz_storable storable;
	int w, h;
	fz_image *mask;
	fz_colorspace *colorspace;
	fz_pixmap *(*get_pixmap)(fz_context *, fz_image *, int w, int h);
};

fz_pixmap *fz_load_jpx(fz_context *ctx, unsigned char *data, int size, fz_colorspace *cs, int indexed);
fz_pixmap *fz_load_jpeg(fz_context *doc, unsigned char *data, int size);
fz_pixmap *fz_load_png(fz_context *doc, unsigned char *data, int size);
fz_pixmap *fz_load_tiff(fz_context *doc, unsigned char *data, int size);
/* SumatraPDF: support JPEG-XR images */
fz_pixmap *fz_load_jxr(fz_context *ctx, unsigned char *data, int size);

struct fz_halftone_s
{
	int refs;
	int n;
	fz_pixmap *comp[1];
};

fz_halftone *fz_new_halftone(fz_context *ctx, int num_comps);
fz_halftone *fz_default_halftone(fz_context *ctx, int num_comps);
void fz_drop_halftone(fz_context *ctx, fz_halftone *half);
fz_halftone *fz_keep_halftone(fz_context *ctx, fz_halftone *half);

struct fz_colorspace_s
{
	fz_storable storable;
	unsigned int size;
	char name[16];
	int n;
	void (*to_rgb)(fz_context *ctx, fz_colorspace *, float *src, float *rgb);
	void (*from_rgb)(fz_context *ctx, fz_colorspace *, float *rgb, float *dst);
	void (*free_data)(fz_context *Ctx, fz_colorspace *);
	void *data;
};

fz_colorspace *fz_new_colorspace(fz_context *ctx, char *name, int n);
fz_colorspace *fz_keep_colorspace(fz_context *ctx, fz_colorspace *colorspace);
void fz_drop_colorspace(fz_context *ctx, fz_colorspace *colorspace);
void fz_free_colorspace_imp(fz_context *ctx, fz_storable *colorspace);

void fz_convert_color(fz_context *ctx, fz_colorspace *dsts, float *dstv, fz_colorspace *srcs, float *srcv);

typedef struct fz_color_converter_s fz_color_converter;

/* This structure is public because it allows us to avoid dynamic allocations.
 * Callers should only rely on the convert entry - the rest of the structure
 * is subject to change without notice.
 */
struct fz_color_converter_s
{
	void (*convert)(fz_color_converter *, float *, float *);
	fz_context *ctx;
	fz_colorspace *ds;
	fz_colorspace *ss;
};

void fz_find_color_converter(fz_color_converter *cc, fz_context *ctx, fz_colorspace *ds, fz_colorspace *ss);

/*
 * Fonts come in two variants:
 *	Regular fonts are handled by FreeType.
 *	Type 3 fonts have callbacks to the interpreter.
 */

char *ft_error_string(int err);

struct fz_font_s
{
	int refs;
	char name[32];

	void *ft_face; /* has an FT_Face if used */
	int ft_substitute; /* ... substitute metrics */
	int ft_bold; /* ... synthesize bold */
	int ft_italic; /* ... synthesize italic */
	int ft_hint; /* ... force hinting for DynaLab fonts */

	/* origin of font data */
	char *ft_file;
	unsigned char *ft_data;
	int ft_size;

	fz_matrix t3matrix;
	void *t3resources;
	fz_buffer **t3procs; /* has 256 entries if used */
	fz_display_list **t3lists; /* has 256 entries if used */
	float *t3widths; /* has 256 entries if used */
	char *t3flags; /* has 256 entries if used */
	void *t3doc; /* a pdf_document for the callback */
	void (*t3run)(void *doc, void *resources, fz_buffer *contents, fz_device *dev, const fz_matrix *ctm, void *gstate, int nestedDepth);
	void (*t3freeres)(void *doc, void *resources);

	fz_rect bbox;	/* font bbox is used only for t3 fonts */

	/* per glyph bounding box cache */
	int use_glyph_bbox;
	int bbox_count;
	fz_rect *bbox_table;

	/* substitute metrics */
	int width_count;
	int *width_table; /* in 1000 units */
};

void fz_new_font_context(fz_context *ctx);
fz_font_context *fz_keep_font_context(fz_context *ctx);
void fz_drop_font_context(fz_context *ctx);

fz_font *fz_new_type3_font(fz_context *ctx, char *name, const fz_matrix *matrix);

fz_font *fz_new_font_from_memory(fz_context *ctx, char *name, unsigned char *data, int len, int index, int use_glyph_bbox);
fz_font *fz_new_font_from_file(fz_context *ctx, char *name, char *path, int index, int use_glyph_bbox);

fz_font *fz_keep_font(fz_context *ctx, fz_font *font);
void fz_drop_font(fz_context *ctx, fz_font *font);

void fz_set_font_bbox(fz_context *ctx, fz_font *font, float xmin, float ymin, float xmax, float ymax);
fz_rect *fz_bound_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *trm, fz_rect *r);
int fz_glyph_cacheable(fz_context *ctx, fz_font *font, int gid);

#ifndef NDEBUG
void fz_print_font(fz_context *ctx, FILE *out, fz_font *font);
#endif

/*
 * Vector path buffer.
 * It can be stroked and dashed, or be filled.
 * It has a fill rule (nonzero or even_odd).
 *
 * When rendering, they are flattened, stroked and dashed straight
 * into the Global Edge List.
 */

typedef struct fz_path_s fz_path;
typedef struct fz_stroke_state_s fz_stroke_state;

typedef union fz_path_item_s fz_path_item;

typedef enum fz_path_item_kind_e
{
	FZ_MOVETO,
	FZ_LINETO,
	FZ_CURVETO,
	FZ_CLOSE_PATH
} fz_path_item_kind;

typedef enum fz_linecap_e
{
	FZ_LINECAP_BUTT = 0,
	FZ_LINECAP_ROUND = 1,
	FZ_LINECAP_SQUARE = 2,
	FZ_LINECAP_TRIANGLE = 3
} fz_linecap;

typedef enum fz_linejoin_e
{
	FZ_LINEJOIN_MITER = 0,
	FZ_LINEJOIN_ROUND = 1,
	FZ_LINEJOIN_BEVEL = 2,
	FZ_LINEJOIN_MITER_XPS = 3
} fz_linejoin;

union fz_path_item_s
{
	fz_path_item_kind k;
	float v;
};

struct fz_path_s
{
	int len, cap;
	fz_path_item *items;
	int last;
};

struct fz_stroke_state_s
{
	int refs;
	fz_linecap start_cap, dash_cap, end_cap;
	fz_linejoin linejoin;
	float linewidth;
	float miterlimit;
	float dash_phase;
	int dash_len;
	float dash_list[32];
};

fz_path *fz_new_path(fz_context *ctx);
fz_point fz_currentpoint(fz_context *ctx, fz_path *path);
void fz_moveto(fz_context*, fz_path*, float x, float y);
void fz_lineto(fz_context*, fz_path*, float x, float y);
void fz_curveto(fz_context*,fz_path*, float, float, float, float, float, float);
void fz_curvetov(fz_context*,fz_path*, float, float, float, float);
void fz_curvetoy(fz_context*,fz_path*, float, float, float, float);
void fz_closepath(fz_context*,fz_path*);
void fz_free_path(fz_context *ctx, fz_path *path);

void fz_transform_path(fz_context *ctx, fz_path *path, const fz_matrix *transform);

fz_path *fz_clone_path(fz_context *ctx, fz_path *old);

fz_rect *fz_bound_path(fz_context *ctx, fz_path *path, fz_stroke_state *stroke, const fz_matrix *ctm, fz_rect *r);
fz_rect *fz_adjust_rect_for_stroke(fz_rect *r, fz_stroke_state *stroke, const fz_matrix *ctm);

fz_stroke_state *fz_new_stroke_state(fz_context *ctx);
fz_stroke_state *fz_new_stroke_state_with_len(fz_context *ctx, int len);
fz_stroke_state *fz_keep_stroke_state(fz_context *ctx, fz_stroke_state *stroke);
void fz_drop_stroke_state(fz_context *ctx, fz_stroke_state *stroke);
fz_stroke_state *fz_unshare_stroke_state(fz_context *ctx, fz_stroke_state *shared);
fz_stroke_state *fz_unshare_stroke_state_with_len(fz_context *ctx, fz_stroke_state *shared, int len);

#ifndef NDEBUG
void fz_print_path(fz_context *ctx, FILE *out, fz_path *, int indent);
#endif

/*
 * Glyph cache
 */

void fz_new_glyph_cache_context(fz_context *ctx);
fz_glyph_cache *fz_keep_glyph_cache(fz_context *ctx);
void fz_drop_glyph_cache_context(fz_context *ctx);
void fz_purge_glyph_cache(fz_context *ctx);

fz_path *fz_outline_ft_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *trm);
fz_path *fz_outline_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *ctm);
fz_pixmap *fz_render_ft_glyph(fz_context *ctx, fz_font *font, int cid, const fz_matrix *trm, int aa);
fz_pixmap *fz_render_t3_glyph(fz_context *ctx, fz_font *font, int cid, const fz_matrix *trm, fz_colorspace *model, fz_irect scissor);
fz_pixmap *fz_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *trm, const fz_matrix *ctm, fz_stroke_state *state);
fz_pixmap *fz_render_glyph(fz_context *ctx, fz_font*, int, const fz_matrix *, fz_colorspace *model, fz_irect scissor);
fz_pixmap *fz_render_stroked_glyph(fz_context *ctx, fz_font*, int, const fz_matrix *, const fz_matrix *, fz_stroke_state *stroke, fz_irect scissor);
void fz_render_t3_glyph_direct(fz_context *ctx, fz_device *dev, fz_font *font, int gid, const fz_matrix *trm, void *gstate, int nestedDepth);
void fz_prepare_t3_glyph(fz_context *ctx, fz_font *font, int gid, int nestedDepth);

/*
	fz_create_annot: create a new annotation of the specified type on the
	specified page. The returned pdf_annot structure is owned by the page
	and does not need to be freed.
*/
fz_annot *fz_create_annot(fz_interactive *idoc, fz_page *page, fz_annot_type type);

/*
	fz_delete_annot: delete an annotation
*/
void fz_delete_annot(fz_interactive *idoc, fz_page *page, fz_annot *annot);

/*
	fz_set_annot_appearance: update the appearance of an annotation based
	on a display list.
*/
void fz_set_annot_appearance(fz_interactive *idoc, fz_annot *annot, fz_rect *rect, fz_display_list *disp_list);

/*
	fz_set_markup_annot_quadpoints: set the quadpoints for a text-markup annotation.
*/
void fz_set_markup_annot_quadpoints(fz_interactive *idoc, fz_annot *annot, fz_point *qp, int n);

/*
	fz_set_markup_appearance: set the appearance stream of a text markup annotations, basing it on
	its QuadPoints array
*/
void fz_set_markup_appearance(fz_interactive *idoc, fz_annot *annot, float color[3], float alpha, float line_thickness, float line_height);

/*
	fz_set_ink_annot_list: set the details of an ink annotation. All the points of the multiple arcs
	are carried in a single array, with the counts for each arc held in a secondary array.
*/
void fz_set_ink_annot_list(fz_interactive *idoc, fz_annot *annot, fz_point *pts, int *counts, int ncount, float color[3], float thickness);

/*
 * Text buffer.
 *
 * The trm field contains the a, b, c and d coefficients.
 * The e and f coefficients come from the individual elements,
 * together they form the transform matrix for the glyph.
 *
 * Glyphs are referenced by glyph ID.
 * The Unicode text equivalent is kept in a separate array
 * with indexes into the glyph array.
 */

typedef struct fz_text_s fz_text;
typedef struct fz_text_item_s fz_text_item;

struct fz_text_item_s
{
	float x, y;
	int gid; /* -1 for one gid to many ucs mappings */
	int ucs; /* -1 for one ucs to many gid mappings */
};

struct fz_text_s
{
	fz_font *font;
	fz_matrix trm;
	int wmode;
	int len, cap;
	fz_text_item *items;
};

fz_text *fz_new_text(fz_context *ctx, fz_font *face, const fz_matrix *trm, int wmode);
void fz_add_text(fz_context *ctx, fz_text *text, int gid, int ucs, float x, float y);
void fz_free_text(fz_context *ctx, fz_text *text);
fz_rect *fz_bound_text(fz_context *ctx, fz_text *text, const fz_matrix *ctm, fz_rect *r);
fz_text *fz_clone_text(fz_context *ctx, fz_text *old);
void fz_print_text(fz_context *ctx, FILE *out, fz_text*);

/*
 * The generic function support.
 */

typedef struct fz_function_s fz_function;

void fz_eval_function(fz_context *ctx, fz_function *func, float *in, int inlen, float *out, int outlen);
fz_function *fz_keep_function(fz_context *ctx, fz_function *func);
void fz_drop_function(fz_context *ctx, fz_function *func);
unsigned int fz_function_size(fz_function *func);
#ifndef NDEBUG
void pdf_debug_function(fz_function *func);
#endif

enum
{
	FZ_FN_MAXN = FZ_MAX_COLORS,
	FZ_FN_MAXM = FZ_MAX_COLORS
};

struct fz_function_s
{
	fz_storable storable;
	unsigned int size;
	int m;					/* number of input values */
	int n;					/* number of output values */
	void (*evaluate)(fz_context *ctx, fz_function *func, float *in, float *out);
#ifndef NDEBUG
	void (*debug)(fz_function *func);
#endif
};

/*
 * The shading code uses gouraud shaded triangle meshes.
 */

enum
{
	FZ_FUNCTION_BASED = 1,
	FZ_LINEAR = 2,
	FZ_RADIAL = 3,
	FZ_MESH_TYPE4 = 4,
	FZ_MESH_TYPE5 = 5,
	FZ_MESH_TYPE6 = 6,
	FZ_MESH_TYPE7 = 7
};

typedef struct fz_shade_s fz_shade;

struct fz_shade_s
{
	fz_storable storable;

	fz_rect bbox;		/* can be fz_infinite_rect */
	fz_colorspace *colorspace;

	fz_matrix matrix;	/* matrix from pattern dict */
	int use_background;	/* background color for fills but not 'sh' */
	float background[FZ_MAX_COLORS];

	int use_function;
	float function[256][FZ_MAX_COLORS + 1];

	int type; /* function, linear, radial, mesh */
	union
	{
		struct
		{
			int extend[2];
			float coords[2][3]; /* (x,y,r) twice */
		} l_or_r;
		struct
		{
			int vprow;
			int bpflag;
			int bpcoord;
			int bpcomp;
			float x0, x1;
			float y0, y1;
			float c0[FZ_MAX_COLORS];
			float c1[FZ_MAX_COLORS];
		} m;
		struct
		{
			fz_matrix matrix;
			int xdivs;
			int ydivs;
			float domain[2][2];
			float *fn_vals;
		} f;
	} u;

	fz_compressed_buffer *buffer;
};

fz_shade *fz_keep_shade(fz_context *ctx, fz_shade *shade);
void fz_drop_shade(fz_context *ctx, fz_shade *shade);
void fz_free_shade_imp(fz_context *ctx, fz_storable *shade);

fz_rect *fz_bound_shade(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_rect *r);
void fz_paint_shade(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_pixmap *dest, const fz_irect *bbox);

/*
 *	Handy routine for processing mesh based shades
 */
typedef struct fz_vertex_s fz_vertex;

struct fz_vertex_s
{
	fz_point p;
	float c[FZ_MAX_COLORS];
};

typedef struct fz_mesh_processor_s fz_mesh_processor;

typedef void (fz_mesh_process_fn)(void *arg, fz_vertex *av, fz_vertex *bv, fz_vertex *cv);

struct fz_mesh_processor_s {
	fz_context *ctx;
	fz_shade *shade;
	fz_mesh_process_fn *process;
	void *process_arg;
};

void fz_process_mesh(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm,
			fz_mesh_process_fn *process, void *process_arg);

#ifndef NDEBUG
void fz_print_shade(fz_context *ctx, FILE *out, fz_shade *shade);
#endif

/*
 * Scan converter
 */

typedef struct fz_gel_s fz_gel;

fz_gel *fz_new_gel(fz_context *ctx);
void fz_insert_gel(fz_gel *gel, float x0, float y0, float x1, float y1);
void fz_reset_gel(fz_gel *gel, const fz_irect *clip);
void fz_sort_gel(fz_gel *gel);
fz_irect *fz_bound_gel(const fz_gel *gel, fz_irect *bbox);
void fz_free_gel(fz_gel *gel);
int fz_is_rect_gel(fz_gel *gel);

void fz_scan_convert(fz_gel *gel, int eofill, const fz_irect *clip, fz_pixmap *pix, unsigned char *colorbv);

void fz_flatten_fill_path(fz_gel *gel, fz_path *path, const fz_matrix *ctm, float flatness);
void fz_flatten_stroke_path(fz_gel *gel, fz_path *path, fz_stroke_state *stroke, const fz_matrix *ctm, float flatness, float linewidth);
void fz_flatten_dash_path(fz_gel *gel, fz_path *path, fz_stroke_state *stroke, const fz_matrix *ctm, float flatness, float linewidth);

/* SumatraPDF: support transfer functions */
typedef struct fz_transfer_function_s fz_transfer_function;
struct fz_transfer_function_s
{
	fz_storable storable;
	unsigned char function[4][256];
};

fz_transfer_function *fz_keep_transfer_function(fz_context *ctx, fz_transfer_function *tr);
void fz_drop_transfer_function(fz_context *ctx, fz_transfer_function *tr);

/*
 * The device interface.
 */

fz_device *fz_new_draw_device_type3(fz_context *ctx, fz_pixmap *dest);

enum
{
	/* Hints */
	FZ_IGNORE_IMAGE = 1,
	FZ_IGNORE_SHADE = 2,

	/* Flags */
	FZ_DEVFLAG_MASK = 1,
	FZ_DEVFLAG_COLOR = 2,
	FZ_DEVFLAG_UNCACHEABLE = 4,
	FZ_DEVFLAG_FILLCOLOR_UNDEFINED = 8,
	FZ_DEVFLAG_STROKECOLOR_UNDEFINED = 16,
	FZ_DEVFLAG_STARTCAP_UNDEFINED = 32,
	FZ_DEVFLAG_DASHCAP_UNDEFINED = 64,
	FZ_DEVFLAG_ENDCAP_UNDEFINED = 128,
	FZ_DEVFLAG_LINEJOIN_UNDEFINED = 256,
	FZ_DEVFLAG_MITERLIMIT_UNDEFINED = 512,
	FZ_DEVFLAG_LINEWIDTH_UNDEFINED = 1024,
	/* Arguably we should have a bit for the dash pattern itself being
	 * undefined, but that causes problems; do we assume that it should
	 * always be set to non-dashing at the start of every glyph? */
};

struct fz_device_s
{
	int hints;
	int flags;

	void *user;
	void (*free_user)(fz_device *);
	fz_context *ctx;

	void (*fill_path)(fz_device *, fz_path *, int even_odd, const fz_matrix *, fz_colorspace *, float *color, float alpha);
	void (*stroke_path)(fz_device *, fz_path *, fz_stroke_state *, const fz_matrix *, fz_colorspace *, float *color, float alpha);
	void (*clip_path)(fz_device *, fz_path *, const fz_rect *rect, int even_odd, const fz_matrix *);
	void (*clip_stroke_path)(fz_device *, fz_path *, const fz_rect *rect, fz_stroke_state *, const fz_matrix *);

	void (*fill_text)(fz_device *, fz_text *, const fz_matrix *, fz_colorspace *, float *color, float alpha);
	void (*stroke_text)(fz_device *, fz_text *, fz_stroke_state *, const fz_matrix *, fz_colorspace *, float *color, float alpha);
	void (*clip_text)(fz_device *, fz_text *, const fz_matrix *, int accumulate);
	void (*clip_stroke_text)(fz_device *, fz_text *, fz_stroke_state *, const fz_matrix *);
	void (*ignore_text)(fz_device *, fz_text *, const fz_matrix *);

	void (*fill_shade)(fz_device *, fz_shade *shd, const fz_matrix *ctm, float alpha);
	void (*fill_image)(fz_device *, fz_image *img, const fz_matrix *ctm, float alpha);
	void (*fill_image_mask)(fz_device *, fz_image *img, const fz_matrix *ctm, fz_colorspace *, float *color, float alpha);
	void (*clip_image_mask)(fz_device *, fz_image *img, const fz_rect *rect, const fz_matrix *ctm);

	void (*pop_clip)(fz_device *);

	void (*begin_mask)(fz_device *, const fz_rect *, int luminosity, fz_colorspace *, float *bc);
	void (*end_mask)(fz_device *);
	void (*begin_group)(fz_device *, const fz_rect *, int isolated, int knockout, int blendmode, float alpha);
	void (*end_group)(fz_device *);

	int (*begin_tile)(fz_device *, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm, int id);
	void (*end_tile)(fz_device *);

	/* SumatraPDF: support transfer functions */
	void (*apply_transfer_function)(fz_device *, fz_transfer_function *tr, int for_mask);

	int error_depth;
	char errmess[256];
};

void fz_fill_path(fz_device *dev, fz_path *path, int even_odd, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_path(fz_device *dev, fz_path *path, const fz_rect *rect, int even_odd, const fz_matrix *ctm);
void fz_clip_stroke_path(fz_device *dev, fz_path *path, const fz_rect *rect, fz_stroke_state *stroke, const fz_matrix *ctm);
void fz_fill_text(fz_device *dev, fz_text *text, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_text(fz_device *dev, fz_text *text, const fz_matrix *ctm, int accumulate);
void fz_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm);
void fz_ignore_text(fz_device *dev, fz_text *text, const fz_matrix *ctm);
void fz_pop_clip(fz_device *dev);
void fz_fill_shade(fz_device *dev, fz_shade *shade, const fz_matrix *ctm, float alpha);
void fz_fill_image(fz_device *dev, fz_image *image, const fz_matrix *ctm, float alpha);
void fz_fill_image_mask(fz_device *dev, fz_image *image, const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_image_mask(fz_device *dev, fz_image *image, const fz_rect *rect, const fz_matrix *ctm);
void fz_begin_mask(fz_device *dev, const fz_rect *area, int luminosity, fz_colorspace *colorspace, float *bc);
void fz_end_mask(fz_device *dev);
void fz_begin_group(fz_device *dev, const fz_rect *area, int isolated, int knockout, int blendmode, float alpha);
void fz_end_group(fz_device *dev);
void fz_begin_tile(fz_device *dev, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm);
int fz_begin_tile_id(fz_device *dev, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm, int id);
void fz_end_tile(fz_device *dev);
/* SumatraPDF: support transfer functions */
void fz_apply_transfer_function(fz_device *dev, fz_transfer_function *tr, int for_mask);

fz_device *fz_new_device(fz_context *ctx, void *user);

/*
 * Plotting functions.
 */

void fz_decode_tile(fz_pixmap *pix, float *decode);
void fz_decode_indexed_tile(fz_pixmap *pix, float *decode, int maxval);
void fz_unpack_tile(fz_pixmap *dst, unsigned char * restrict src, int n, int depth, int stride, int scale);

void fz_paint_solid_alpha(unsigned char * restrict dp, int w, int alpha);
void fz_paint_solid_color(unsigned char * restrict dp, int n, int w, unsigned char *color);

void fz_paint_span(unsigned char * restrict dp, unsigned char * restrict sp, int n, int w, int alpha);
void fz_paint_span_with_color(unsigned char * restrict dp, unsigned char * restrict mp, int n, int w, unsigned char *color);

void fz_paint_image(fz_pixmap *dst, const fz_irect *scissor, fz_pixmap *shape, fz_pixmap *img, const fz_matrix *ctm, int alpha);
void fz_paint_image_with_color(fz_pixmap *dst, const fz_irect *scissor, fz_pixmap *shape, fz_pixmap *img, const fz_matrix *ctm, unsigned char *colorbv);

void fz_paint_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha);
void fz_paint_pixmap_with_mask(fz_pixmap *dst, fz_pixmap *src, fz_pixmap *msk);
void fz_paint_pixmap_with_bbox(fz_pixmap *dst, fz_pixmap *src, int alpha, fz_irect bbox);

void fz_blend_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha, int blendmode, int isolated, fz_pixmap *shape);
void fz_blend_pixel(unsigned char dp[3], unsigned char bp[3], unsigned char sp[3], int blendmode);

enum
{
	/* PDF 1.4 -- standard separable */
	FZ_BLEND_NORMAL,
	FZ_BLEND_MULTIPLY,
	FZ_BLEND_SCREEN,
	FZ_BLEND_OVERLAY,
	FZ_BLEND_DARKEN,
	FZ_BLEND_LIGHTEN,
	FZ_BLEND_COLOR_DODGE,
	FZ_BLEND_COLOR_BURN,
	FZ_BLEND_HARD_LIGHT,
	FZ_BLEND_SOFT_LIGHT,
	FZ_BLEND_DIFFERENCE,
	FZ_BLEND_EXCLUSION,

	/* PDF 1.4 -- standard non-separable */
	FZ_BLEND_HUE,
	FZ_BLEND_SATURATION,
	FZ_BLEND_COLOR,
	FZ_BLEND_LUMINOSITY,

	/* For packing purposes */
	FZ_BLEND_MODEMASK = 15,
	FZ_BLEND_ISOLATED = 16,
	FZ_BLEND_KNOCKOUT = 32
};

struct fz_document_s
{
	void (*close)(fz_document *);
	int (*needs_password)(fz_document *doc);
	int (*authenticate_password)(fz_document *doc, char *password);
	fz_outline *(*load_outline)(fz_document *doc);
	int (*count_pages)(fz_document *doc);
	fz_page *(*load_page)(fz_document *doc, int number);
	fz_link *(*load_links)(fz_document *doc, fz_page *page);
	fz_rect *(*bound_page)(fz_document *doc, fz_page *page, fz_rect *);
	void (*run_page_contents)(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie);
	void (*run_annot)(fz_document *doc, fz_page *page, fz_annot *annot, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie);
	void (*free_page)(fz_document *doc, fz_page *page);
	int (*meta)(fz_document *doc, int key, void *ptr, int size);
	fz_transition *(*page_presentation)(fz_document *doc, fz_page *page, float *duration);
	fz_interactive *(*interact)(fz_document *doc);
	void (*write)(fz_document *doc, char *filename, fz_write_options *opts);
	fz_annot *(*first_annot)(fz_document *doc, fz_page *page);
	fz_annot *(*next_annot)(fz_document *doc, fz_annot *annot);
	fz_rect *(*bound_annot)(fz_document *doc, fz_annot *annot, fz_rect *rect);
};

#endif
