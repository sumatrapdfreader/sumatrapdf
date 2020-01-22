#ifndef MUPDF_FITZ_STORE_H
#define MUPDF_FITZ_STORE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"

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
typedef struct fz_key_storable_s fz_key_storable;

typedef void (fz_store_drop_fn)(fz_context *, fz_storable *);

struct fz_storable_s {
	int refs;
	fz_store_drop_fn *drop;
};

struct fz_key_storable_s {
	fz_storable storable;
	short store_key_refs;
};

#define FZ_INIT_STORABLE(S_,RC,DROP) \
	do { fz_storable *S = &(S_)->storable; S->refs = (RC); \
	S->drop = (DROP); \
	} while (0)

#define FZ_INIT_KEY_STORABLE(KS_,RC,DROP) \
	do { fz_key_storable *KS = &(KS_)->key_storable; KS->store_key_refs = 0;\
	FZ_INIT_STORABLE(KS,RC,DROP); \
	} while (0)

void *fz_keep_storable(fz_context *, const fz_storable *);
void fz_drop_storable(fz_context *, const fz_storable *);

void *fz_keep_key_storable(fz_context *, const fz_key_storable *);
void fz_drop_key_storable(fz_context *, const fz_key_storable *);

void *fz_keep_key_storable_key(fz_context *, const fz_key_storable *);
void fz_drop_key_storable_key(fz_context *, const fz_key_storable *);

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
	to a fz_store_hash structure. If make_hash_key function returns 0,
	then the key is determined not to be hashable, and the value is
	not stored in the hash table.

	Some objects can be used both as values within the store, and as a
	component of keys within the store. We refer to these objects as
	"key storable" objects. In this case, we need to take additional
	care to ensure that we do not end up keeping an item within the
	store, purely because its value is referred to by another key in
	the store.

	An example of this are fz_images in PDF files. Each fz_image is
	placed into the	store to enable it to be easily reused. When the
	image is rendered, a pixmap is generated from the image, and the
	pixmap is placed into the store so it can be reused on subsequent
	renders. The image forms part of the key for the pixmap.

	When we close the pdf document (and any associated pages/display
	lists etc), we drop the images from the store. This may leave us
	in the position of the images having non-zero reference counts
	purely because they are used as part of the keys for the pixmaps.

	We therefore use special reference counting functions to keep
	track of these "key storable" items, and hence store the number of
	references to these items that are used in keys.

	When the number of references to an object == the number of
	references to an object from keys in the store, we know that we can
	remove all the items which have that object as part of the key.
	This is done by running a pass over the store, 'reaping' those
	items.

	Reap passes are slower than we would like as they touch every
	item in the store. We therefore provide a way to 'batch' such
	reap passes together, using fz_defer_reap_start/fz_defer_reap_end
	to bracket a region in which many may be triggered.
*/
typedef struct fz_store_hash_s
{
	fz_store_drop_fn *drop;
	union
	{
		struct
		{
			const void *ptr;
			int i;
		} pi; /* 8 or 12 bytes */
		struct
		{
			const void *ptr;
			int i;
			fz_irect r;
		} pir; /* 24 or 28 bytes */
		struct
		{
			int id;
			char has_shape;
			char has_group_alpha;
			float m[4];
			void *ptr;
		} im; /* 24 or 28 bytes */
		struct
		{
			unsigned char src_md5[16];
			unsigned char dst_md5[16];
			unsigned int ri:2;
			unsigned int bp:1;
			unsigned int format:1;
			unsigned int proof:1;
			unsigned int src_extras:5;
			unsigned int dst_extras:5;
			unsigned int copy_spots:1;
			unsigned int bgr:1;
		} link; /* 36 bytes */
	} u;
} fz_store_hash; /* 40 or 44 bytes */

typedef struct fz_store_type_s
{
	int (*make_hash_key)(fz_context *ctx, fz_store_hash *hash, void *key);
	void *(*keep_key)(fz_context *ctx, void *key);
	void (*drop_key)(fz_context *ctx, void *key);
	int (*cmp_key)(fz_context *ctx, void *a, void *b);
	void (*format_key)(fz_context *ctx, char *buf, size_t size, void *key);
	int (*needs_reap)(fz_context *ctx, void *key);
} fz_store_type;

void fz_new_store_context(fz_context *ctx, size_t max);

void fz_drop_store_context(fz_context *ctx);
fz_store *fz_keep_store_context(fz_context *ctx);

void *fz_store_item(fz_context *ctx, void *key, void *val, size_t itemsize, const fz_store_type *type);

void *fz_find_item(fz_context *ctx, fz_store_drop_fn *drop, void *key, const fz_store_type *type);

void fz_remove_item(fz_context *ctx, fz_store_drop_fn *drop, void *key, const fz_store_type *type);

void fz_empty_store(fz_context *ctx);

int fz_store_scavenge(fz_context *ctx, size_t size, int *phase);

int fz_store_scavenge_external(fz_context *ctx, size_t size, int *phase);

int fz_shrink_store(fz_context *ctx, unsigned int percent);

typedef int (fz_store_filter_fn)(fz_context *ctx, void *arg, void *key);

void fz_filter_store(fz_context *ctx, fz_store_filter_fn *fn, void *arg, const fz_store_type *type);

void fz_debug_store(fz_context *ctx);

void fz_defer_reap_start(fz_context *ctx);

void fz_defer_reap_end(fz_context *ctx);

#endif
