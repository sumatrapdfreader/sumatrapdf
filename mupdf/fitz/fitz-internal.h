#ifndef FITZ_INTERNAL_H
#define FITZ_INTERNAL_H

#include "fitz.h"

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

/*
 * Basic runtime and utility functions
 */

/* Range checking atof */
float fz_atof(const char *s);

/*
 * Generic hash-table with fixed-length keys.
 */

typedef struct fz_hash_table_s fz_hash_table;

fz_hash_table *fz_new_hash_table(fz_context *ctx, int initialsize, int keylen, int lock);
void fz_print_hash(fz_context *ctx, FILE *out, fz_hash_table *table);
void fz_empty_hash(fz_context *ctx, fz_hash_table *table);
void fz_free_hash(fz_context *ctx, fz_hash_table *table);

void *fz_hash_find(fz_context *ctx, fz_hash_table *table, void *key);
void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, void *key, void *val);
void fz_hash_remove(fz_context *ctx, fz_hash_table *table, void *key);

int fz_hash_len(fz_context *ctx, fz_hash_table *table);
void *fz_hash_get_key(fz_context *ctx, fz_hash_table *table, int idx);
void *fz_hash_get_val(fz_context *ctx, fz_hash_table *table, int idx);

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
float fz_matrix_max_expansion(fz_matrix m);

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

void aes_setkey_enc( fz_aes *ctx, const unsigned char *key, int keysize );
void aes_setkey_dec( fz_aes *ctx, const unsigned char *key, int keysize );
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
	} u;
};

typedef struct fz_store_type_s fz_store_type;

struct fz_store_type_s
{
	int (*make_hash_key)(fz_store_hash *, void *);
	void *(*keep_key)(fz_context *,void *);
	void (*drop_key)(fz_context *,void *);
	int (*cmp_key)(void *, void *);
	void (*debug)(void *);
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
	fz_print_store: Dump the contents of the store for debugging.
*/
void fz_print_store(fz_context *ctx, FILE *out);

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

struct fz_buffer_s
{
	int refs;
	unsigned char *data;
	int cap, len;
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

struct fz_stream_s
{
	fz_context *ctx;
	int refs;
	int error;
	int eof;
	int pos;
	int avail;
	int bits;
	int locked;
	unsigned char *bp, *rp, *wp, *ep;
	void *state;
	int (*read)(fz_stream *stm, unsigned char *buf, int len);
	void (*close)(fz_context *ctx, void *state);
	void (*seek)(fz_stream *stm, int offset, int whence);
	/* SumatraPDF: allow to clone a stream */
	fz_stream *(*reopen)(fz_context *ctx, fz_stream *stm);
	unsigned char buf[4096];
};

void fz_lock_stream(fz_stream *stm);

fz_stream *fz_new_stream(fz_context *ctx, void*, int(*)(fz_stream*, unsigned char*, int), void(*)(fz_context *, void *));
fz_stream *fz_keep_stream(fz_stream *stm);
void fz_fill_buffer(fz_stream *stm);

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
fz_stream *fz_open_null(fz_stream *chain, int len);
fz_stream *fz_open_arc4(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_aesd(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_a85d(fz_stream *chain);
fz_stream *fz_open_ahxd(fz_stream *chain);
fz_stream *fz_open_rld(fz_stream *chain);
fz_stream *fz_open_dctd(fz_stream *chain, int color_transform);
fz_stream *fz_open_resized_dctd(fz_stream *chain, int color_transform, int factor);
fz_stream *fz_open_faxd(fz_stream *chain,
	int k, int end_of_line, int encoded_byte_align,
	int columns, int rows, int end_of_block, int black_is_1);
fz_stream *fz_open_flated(fz_stream *chain);
fz_stream *fz_open_lzwd(fz_stream *chain, int early_change);
fz_stream *fz_open_predict(fz_stream *chain, int predictor, int columns, int colors, int bpc);
fz_stream *fz_open_jbig2d(fz_stream *chain, fz_buffer *global);

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

void fz_clear_pixmap_rect_with_value(fz_context *ctx, fz_pixmap *pix, int value, fz_bbox r);
void fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, fz_bbox r);
void fz_premultiply_pixmap(fz_context *ctx, fz_pixmap *pix);
fz_pixmap *fz_alpha_from_gray(fz_context *ctx, fz_pixmap *gray, int luminosity);
unsigned int fz_pixmap_size(fz_context *ctx, fz_pixmap *pix);

fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, fz_bbox *clip);

fz_bbox fz_pixmap_bbox_no_ctx(fz_pixmap *src);

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
	float *t3widths; /* has 256 entries if used */
	char *t3flags; /* has 256 entries if used */
	void *t3doc; /* a pdf_document for the callback */
	void (*t3run)(void *doc, void *resources, fz_buffer *contents, fz_device *dev, fz_matrix ctm, void *gstate);
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

fz_font *fz_new_type3_font(fz_context *ctx, char *name, fz_matrix matrix);

fz_font *fz_new_font_from_memory(fz_context *ctx, unsigned char *data, int len, int index, int use_glyph_bbox);
fz_font *fz_new_font_from_file(fz_context *ctx, char *path, int index, int use_glyph_bbox);

fz_font *fz_keep_font(fz_context *ctx, fz_font *font);
void fz_drop_font(fz_context *ctx, fz_font *font);

void fz_print_font(fz_context *ctx, FILE *out, fz_font *font);

void fz_set_font_bbox(fz_context *ctx, fz_font *font, float xmin, float ymin, float xmax, float ymax);
fz_rect fz_bound_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm);
int fz_glyph_cacheable(fz_context *ctx, fz_font *font, int gid);

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
void fz_moveto(fz_context*, fz_path*, float x, float y);
void fz_lineto(fz_context*, fz_path*, float x, float y);
void fz_curveto(fz_context*,fz_path*, float, float, float, float, float, float);
void fz_curvetov(fz_context*,fz_path*, float, float, float, float);
void fz_curvetoy(fz_context*,fz_path*, float, float, float, float);
void fz_closepath(fz_context*,fz_path*);
void fz_free_path(fz_context *ctx, fz_path *path);

void fz_transform_path(fz_context *ctx, fz_path *path, fz_matrix transform);

fz_path *fz_clone_path(fz_context *ctx, fz_path *old);

fz_rect fz_bound_path(fz_context *ctx, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm);
void fz_print_path(fz_context *ctx, FILE *out, fz_path *, int indent);

fz_stroke_state *fz_new_stroke_state(fz_context *ctx);
fz_stroke_state *fz_new_stroke_state_with_len(fz_context *ctx, int len);
fz_stroke_state *fz_keep_stroke_state(fz_context *ctx, fz_stroke_state *stroke);
void fz_drop_stroke_state(fz_context *ctx, fz_stroke_state *stroke);
fz_stroke_state *fz_unshare_stroke_state(fz_context *ctx, fz_stroke_state *shared);
fz_stroke_state *fz_unshare_stroke_state_with_len(fz_context *ctx, fz_stroke_state *shared, int len);

/*
 * Glyph cache
 */

void fz_new_glyph_cache_context(fz_context *ctx);
fz_glyph_cache *fz_keep_glyph_cache(fz_context *ctx);
void fz_drop_glyph_cache_context(fz_context *ctx);
void fz_purge_glyph_cache(fz_context *ctx);

fz_pixmap *fz_render_ft_glyph(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, int aa);
fz_pixmap *fz_render_t3_glyph(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, fz_colorspace *model);
fz_pixmap *fz_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, fz_stroke_state *state);
fz_pixmap *fz_render_glyph(fz_context *ctx, fz_font*, int, fz_matrix, fz_colorspace *model);
fz_pixmap *fz_render_stroked_glyph(fz_context *ctx, fz_font*, int, fz_matrix, fz_matrix, fz_stroke_state *stroke);
void fz_render_t3_glyph_direct(fz_context *ctx, fz_device *dev, fz_font *font, int gid, fz_matrix trm, void *gstate);

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

fz_text *fz_new_text(fz_context *ctx, fz_font *face, fz_matrix trm, int wmode);
void fz_add_text(fz_context *ctx, fz_text *text, int gid, int ucs, float x, float y);
void fz_free_text(fz_context *ctx, fz_text *text);
fz_rect fz_bound_text(fz_context *ctx, fz_text *text, fz_matrix ctm);
fz_text *fz_clone_text(fz_context *ctx, fz_text *old);
void fz_print_text(fz_context *ctx, FILE *out, fz_text*);

/*
 * The shading code uses gouraud shaded triangle meshes.
 */

enum
{
	FZ_LINEAR,
	FZ_RADIAL,
	FZ_MESH,
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

	int type; /* linear, radial, mesh */
	int extend[2];

	int mesh_len;
	int mesh_cap;
	float *mesh; /* [x y 0], [x y r], [x y t] or [x y c1 ... cn] */
};

fz_shade *fz_keep_shade(fz_context *ctx, fz_shade *shade);
void fz_drop_shade(fz_context *ctx, fz_shade *shade);
void fz_free_shade_imp(fz_context *ctx, fz_storable *shade);
void fz_print_shade(fz_context *ctx, FILE *out, fz_shade *shade);

fz_rect fz_bound_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm);
void fz_paint_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox);

/*
 * Scan converter
 */

typedef struct fz_gel_s fz_gel;

fz_gel *fz_new_gel(fz_context *ctx);
void fz_insert_gel(fz_gel *gel, float x0, float y0, float x1, float y1);
void fz_reset_gel(fz_gel *gel, fz_bbox clip);
void fz_sort_gel(fz_gel *gel);
fz_bbox fz_bound_gel(fz_gel *gel);
void fz_free_gel(fz_gel *gel);
int fz_is_rect_gel(fz_gel *gel);

void fz_scan_convert(fz_gel *gel, int eofill, fz_bbox clip, fz_pixmap *pix, unsigned char *colorbv);

void fz_flatten_fill_path(fz_gel *gel, fz_path *path, fz_matrix ctm, float flatness);
void fz_flatten_stroke_path(fz_gel *gel, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth);
void fz_flatten_dash_path(fz_gel *gel, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth);

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

	void (*fill_path)(fz_device *, fz_path *, int even_odd, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*stroke_path)(fz_device *, fz_path *, fz_stroke_state *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*clip_path)(fz_device *, fz_path *, fz_rect *rect, int even_odd, fz_matrix);
	void (*clip_stroke_path)(fz_device *, fz_path *, fz_rect *rect, fz_stroke_state *, fz_matrix);

	void (*fill_text)(fz_device *, fz_text *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*stroke_text)(fz_device *, fz_text *, fz_stroke_state *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*clip_text)(fz_device *, fz_text *, fz_matrix, int accumulate);
	void (*clip_stroke_text)(fz_device *, fz_text *, fz_stroke_state *, fz_matrix);
	void (*ignore_text)(fz_device *, fz_text *, fz_matrix);

	void (*fill_shade)(fz_device *, fz_shade *shd, fz_matrix ctm, float alpha);
	void (*fill_image)(fz_device *, fz_image *img, fz_matrix ctm, float alpha);
	void (*fill_image_mask)(fz_device *, fz_image *img, fz_matrix ctm, fz_colorspace *, float *color, float alpha);
	void (*clip_image_mask)(fz_device *, fz_image *img, fz_rect *rect, fz_matrix ctm);

	void (*pop_clip)(fz_device *);

	void (*begin_mask)(fz_device *, fz_rect, int luminosity, fz_colorspace *, float *bc);
	void (*end_mask)(fz_device *);
	void (*begin_group)(fz_device *, fz_rect, int isolated, int knockout, int blendmode, float alpha);
	void (*end_group)(fz_device *);

	void (*begin_tile)(fz_device *, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm);
	void (*end_tile)(fz_device *);
};

void fz_fill_path(fz_device *dev, fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_path(fz_device *dev, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm);
void fz_clip_stroke_path(fz_device *dev, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm);
void fz_fill_text(fz_device *dev, fz_text *text, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_text(fz_device *dev, fz_text *text, fz_matrix ctm, int accumulate);
void fz_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm);
void fz_ignore_text(fz_device *dev, fz_text *text, fz_matrix ctm);
void fz_pop_clip(fz_device *dev);
void fz_fill_shade(fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha);
void fz_fill_image(fz_device *dev, fz_image *image, fz_matrix ctm, float alpha);
void fz_fill_image_mask(fz_device *dev, fz_image *image, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_image_mask(fz_device *dev, fz_image *image, fz_rect *rect, fz_matrix ctm);
void fz_begin_mask(fz_device *dev, fz_rect area, int luminosity, fz_colorspace *colorspace, float *bc);
void fz_end_mask(fz_device *dev);
void fz_begin_group(fz_device *dev, fz_rect area, int isolated, int knockout, int blendmode, float alpha);
void fz_end_group(fz_device *dev);
void fz_begin_tile(fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm);
void fz_end_tile(fz_device *dev);

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

void fz_paint_image(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *shape, fz_pixmap *img, fz_matrix ctm, int alpha);
void fz_paint_image_with_color(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *shape, fz_pixmap *img, fz_matrix ctm, unsigned char *colorbv);

void fz_paint_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha);
void fz_paint_pixmap_with_mask(fz_pixmap *dst, fz_pixmap *src, fz_pixmap *msk);
void fz_paint_pixmap_with_rect(fz_pixmap *dst, fz_pixmap *src, int alpha, fz_bbox bbox);

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
	fz_rect (*bound_page)(fz_document *doc, fz_page *page);
	void (*run_page)(fz_document *doc, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);
	void (*free_page)(fz_document *doc, fz_page *page);
};

#endif
