// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_FITZ_STORE_H
#define MUPDF_FITZ_STORE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/log.h"
#include "mupdf/fitz/types.h"

/**
	Resource store

	MuPDF stores decoded "objects" into a store for potential reuse.
	If the size of the store gets too big, objects stored within it
	can be evicted and freed to recover space. When MuPDF comes to
	decode such an object, it will check to see if a version of this
	object is already in the store - if it is, it will simply reuse
	it. If not, it will decode it and place it into the store.

	All objects that can be placed into the store are derived from
	the fz_storable type (i.e. this should be the first component of
	the objects structure). This allows for consistent (thread safe)
	reference counting, and includes a function that will be called
	to free the object as soon as the reference count reaches zero.

	Most objects offer fz_keep_XXXX/fz_drop_XXXX functions derived
	from fz_keep_storable/fz_drop_storable. Creation of such objects
	includes a call to FZ_INIT_STORABLE to set up the fz_storable
	header.
 */
typedef struct fz_storable fz_storable;

/**
	Function type for a function to drop a storable object.

	Objects within the store are identified by type by comparing
	their drop_fn pointers.
*/
typedef void (fz_store_drop_fn)(fz_context *, fz_storable *);

/**
	Function type for a function to check whether a storable
	object can be dropped at the moment.

	Return 0 for 'cannot be dropped', 1 otherwise.
*/
typedef int (fz_store_droppable_fn)(fz_context *, fz_storable *);

/**
	Any storable object should include an fz_storable structure
	at the start (by convention at least) of their structure.
	(Unless it starts with an fz_key_storable, see below).
*/
struct fz_storable {
	int refs;
	fz_store_drop_fn *drop;
	fz_store_droppable_fn *droppable;
};

/**
	Any storable object that can appear in the key of another
	storable object should include an fz_key_storable structure
	at the start (by convention at least) of their structure.
*/
typedef struct
{
	fz_storable storable;
	short store_key_refs;
} fz_key_storable;

/**
	Macros to initialise a storable object.
*/
#define FZ_INIT_STORABLE(S_,RC,DROP) \
	do { fz_storable *S = &(S_)->storable; S->refs = (RC); \
	S->drop = (DROP); S->droppable = NULL; \
	} while (0)

#define FZ_INIT_AWKWARD_STORABLE(S_,RC,DROP,DROPPABLE) \
	do { fz_storable *S = &(S_)->storable; S->refs = (RC); \
	S->drop = (DROP); S->droppable = (DROPPABLE); \
	} while (0)

/**
	Macro to initialise a key storable object.
*/
#define FZ_INIT_KEY_STORABLE(KS_,RC,DROP) \
	do { fz_key_storable *KS = &(KS_)->key_storable; KS->store_key_refs = 0;\
	FZ_INIT_STORABLE(KS,RC,DROP); \
	} while (0)

/**
	Increment the reference count for a storable object.
	Returns the same pointer.

	Never throws exceptions.
*/
void *fz_keep_storable(fz_context *, const fz_storable *);

/**
	Decrement the reference count for a storable object. When the
	reference count hits zero, the drop function for that object
	is called to free the object.

	Never throws exceptions.
*/
void fz_drop_storable(fz_context *, const fz_storable *);

/**
	Increment the (normal) reference count for a key storable
	object. Returns the same pointer.

	Never throws exceptions.
*/
void *fz_keep_key_storable(fz_context *, const fz_key_storable *);

/**
	Decrement the (normal) reference count for a storable object.
	When the total reference count hits zero, the drop function for
	that object is called to free the object.

	Never throws exceptions.
*/
void fz_drop_key_storable(fz_context *, const fz_key_storable *);

/**
	Increment the (key) reference count for a key storable
	object. Returns the same pointer.

	Never throws exceptions.
*/
void *fz_keep_key_storable_key(fz_context *, const fz_key_storable *);

/**
	Decrement the (key) reference count for a storable object.
	When the total reference count hits zero, the drop function for
	that object is called to free the object.

	Never throws exceptions.
*/
void fz_drop_key_storable_key(fz_context *, const fz_key_storable *);

/**
	The store can be seen as a dictionary that maps keys to
	fz_storable values. In order to allow keys of different types to
	be stored, we have a structure full of functions for each key
	'type'; this fz_store_type pointer is stored with each key, and
	tells the store how to perform certain operations (like taking/
	dropping a reference, comparing two keys, outputting details for
	debugging etc).

	The store uses a hash table internally for speed where possible.
	In order for this to work, we need a mechanism for turning a
	generic 'key' into 'a hashable string'. For this purpose the
	type structure contains a make_hash_key function pointer that
	maps from a void * to a fz_store_hash structure. If
	make_hash_key function returns 0, then the key is determined not
	to be hashable, and the value is not stored in the hash table.

	Some objects can be used both as values within the store, and as
	a component of keys within the store. We refer to these objects
	as "key storable" objects. In this case, we need to take
	additional care to ensure that we do not end up keeping an item
	within the store, purely because its value is referred to by
	another key in the store.

	An example of this are fz_images in PDF files. Each fz_image is
	placed into the	store to enable it to be easily reused. When the
	image is rendered, a pixmap is generated from the image, and the
	pixmap is placed into the store so it can be reused on
	subsequent renders. The image forms part of the key for the
	pixmap.

	When we close the pdf document (and any associated pages/display
	lists etc), we drop the images from the store. This may leave us
	in the position of the images having non-zero reference counts
	purely because they are used as part of the keys for the
	pixmaps.

	We therefore use special reference counting functions to keep
	track of these "key storable" items, and hence store the number
	of references to these items that are used in keys.

	When the number of references to an object == the number of
	references to an object from keys in the store, we know that we
	can remove all the items which have that object as part of the
	key. This is done by running a pass over the store, 'reaping'
	those items.

	Reap passes are slower than we would like as they touch every
	item in the store. We therefore provide a way to 'batch' such
	reap passes together, using fz_defer_reap_start/
	fz_defer_reap_end to bracket a region in which many may be
	triggered.
*/
typedef struct
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
			int doc_id;
		} im; /* 32 or 36 bytes */
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
	/* 0 or 4 bytes padding */
} fz_store_hash; /* 40 or 48 bytes */

/**
	Every type of object to be placed into the store defines an
	fz_store_type. This contains the pointers to functions to
	make hashes, manipulate keys, and check for needing reaping.
*/
typedef struct
{
	const char *name;
	int (*make_hash_key)(fz_context *ctx, fz_store_hash *hash, void *key);
	void *(*keep_key)(fz_context *ctx, void *key);
	void (*drop_key)(fz_context *ctx, void *key);
	int (*cmp_key)(fz_context *ctx, void *a, void *b);
	void (*format_key)(fz_context *ctx, char *buf, size_t size, void *key);
	int (*needs_reap)(fz_context *ctx, void *key);
} fz_store_type;

/**
	Create a new store inside the context

	max: The maximum size (in bytes) that the store is allowed to
	grow to. FZ_STORE_UNLIMITED means no limit.
*/
void fz_new_store_context(fz_context *ctx, size_t max);

/**
	Increment the reference count for the store context. Returns
	the same pointer.

	Never throws exceptions.
*/
fz_store *fz_keep_store_context(fz_context *ctx);

/**
	Decrement the reference count for the store context. When the
	reference count hits zero, the store context is freed.

	Never throws exceptions.
*/
void fz_drop_store_context(fz_context *ctx);

/**
	Add an item to the store.

	Add an item into the store, returning NULL for success. If an
	item with the same key is found in the store, then our item will
	not be inserted, and the function will return a pointer to that
	value instead. This function takes its own reference to val, as
	required (i.e. the caller maintains ownership of its own
	reference).

	key: The key used to index the item.

	val: The value to store.

	itemsize: The size in bytes of the value (as counted towards the
	store size).

	type: Functions used to manipulate the key.
*/
void *fz_store_item(fz_context *ctx, void *key, void *val, size_t itemsize, const fz_store_type *type);

/**
	Find an item within the store.

	drop: The function used to free the value (to ensure we get a
	value of the correct type).

	key: The key used to index the item.

	type: Functions used to manipulate the key.

	Returns NULL for not found, otherwise returns a pointer to the
	value indexed by key to which a reference has been taken.
*/
void *fz_find_item(fz_context *ctx, fz_store_drop_fn *drop, void *key, const fz_store_type *type);

/**
	Remove an item from the store.

	If an item indexed by the given key exists in the store, remove
	it.

	drop: The function used to free the value (to ensure we get a
	value of the correct type).

	key: The key used to find the item to remove.

	type: Functions used to manipulate the key.
*/
void fz_remove_item(fz_context *ctx, fz_store_drop_fn *drop, void *key, const fz_store_type *type);

/**
	Evict every item from the store.
*/
void fz_empty_store(fz_context *ctx);

/**
	Internal function used as part of the scavenging
	allocator; when we fail to allocate memory, before returning a
	failure to the caller, we try to scavenge space within the store
	by evicting at least 'size' bytes. The allocator then retries.

	size: The number of bytes we are trying to have free.

	phase: What phase of the scavenge we are in. Updated on exit.

	Returns non zero if we managed to free any memory.
*/
int fz_store_scavenge(fz_context *ctx, size_t size, int *phase);

/**
	External function for callers to use
	to scavenge while trying allocations.

	size: The number of bytes we are trying to have free.

	phase: What phase of the scavenge we are in. Updated on exit.

	Returns non zero if we managed to free any memory.
*/
int fz_store_scavenge_external(fz_context *ctx, size_t size, int *phase);

/**
	Evict items from the store until the total size of
	the objects in the store is reduced to a given percentage of its
	current size.

	percent: %age of current size to reduce the store to.

	Returns non zero if we managed to free enough memory, zero
	otherwise.
*/
int fz_shrink_store(fz_context *ctx, unsigned int percent);

/**
	Callback function called by fz_filter_store on every item within
	the store.

	Return 1 to drop the item from the store, 0 to retain.
*/
typedef int (fz_store_filter_fn)(fz_context *ctx, void *arg, void *key);

/**
	Filter every element in the store with a matching type with the
	given function.

	If the function returns 1 for an element, drop the element.
*/
void fz_filter_store(fz_context *ctx, fz_store_filter_fn *fn, void *arg, const fz_store_type *type);

/**
	Filter the store and throw away any stored tiles drawn for a
	given document.
*/
void fz_drop_drawn_tiles_for_document(fz_context *ctx, fz_document *doc);

/**
	Output debugging information for the current state of the store
	to the given output channel.
*/
void fz_debug_store(fz_context *ctx, fz_output *out);

/**
	Increment the defer reap count.

	No reap operations will take place (except for those
	triggered by an immediate failed malloc) until the
	defer reap count returns to 0.

	Call this at the start of a process during which you
	potentially might drop many reapable objects.

	It is vital that every fz_defer_reap_start is matched
	by a fz_defer_reap_end call.
*/
void fz_defer_reap_start(fz_context *ctx);

/**
	Decrement the defer reap count.

	If the defer reap count returns to 0, and the store
	has reapable objects in, a reap pass will begin.

	Call this at the end of a process during which you
	potentially might drop many reapable objects.

	It is vital that every fz_defer_reap_start is matched
	by a fz_defer_reap_end call.
*/
void fz_defer_reap_end(fz_context *ctx);

#ifdef ENABLE_STORE_LOGGING

void fz_log_dump_store(fz_context *ctx, const char *fmt, ...);

#define FZ_LOG_STORE(CTX, ...) fz_log_module(CTX, "STORE", __VA_ARGS__)
#define FZ_LOG_DUMP_STORE(...) fz_log_dump_store(__VA_ARGS__)

#else

#define FZ_LOG_STORE(...) do {} while (0)
#define FZ_LOG_DUMP_STORE(...) do {} while (0)

#endif

#endif
