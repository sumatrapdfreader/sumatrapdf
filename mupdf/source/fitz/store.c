#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct fz_item_s fz_item;

struct fz_item_s
{
	void *key;
	fz_storable *val;
	size_t size;
	fz_item *next;
	fz_item *prev;
	fz_store *store;
	const fz_store_type *type;
};

/* Every entry in fz_store is protected by the alloc lock */
struct fz_store_s
{
	int refs;

	/* Every item in the store is kept in a doubly linked list, ordered
	 * by usage (so LRU entries are at the end). */
	fz_item *head;
	fz_item *tail;

	/* We have a hash table that allows to quickly find a subset of the
	 * entries (those whose keys are indirect objects). */
	fz_hash_table *hash;

	/* We keep track of the size of the store, and keep it below max. */
	size_t max;
	size_t size;

	int defer_reap_count;
	int needs_reaping;
};

/*
	Create a new store inside the context

	max: The maximum size (in bytes) that the store is allowed to grow
	to. FZ_STORE_UNLIMITED means no limit.
*/
void
fz_new_store_context(fz_context *ctx, size_t max)
{
	fz_store *store;
	store = fz_malloc_struct(ctx, fz_store);
	fz_try(ctx)
	{
		store->hash = fz_new_hash_table(ctx, 4096, sizeof(fz_store_hash), FZ_LOCK_ALLOC, NULL);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, store);
		fz_rethrow(ctx);
	}
	store->refs = 1;
	store->head = NULL;
	store->tail = NULL;
	store->size = 0;
	store->max = max;
	store->defer_reap_count = 0;
	store->needs_reaping = 0;
	ctx->store = store;
}

void *
fz_keep_storable(fz_context *ctx, const fz_storable *sc)
{
	/* Explicitly drop const to allow us to use const
	 * sanely throughout the code. */
	fz_storable *s = (fz_storable *)sc;

	return fz_keep_imp(ctx, s, &s->refs);
}

void *fz_keep_key_storable(fz_context *ctx, const fz_key_storable *sc)
{
	return fz_keep_storable(ctx, &sc->storable);
}

/*
	Entered with FZ_LOCK_ALLOC held.
	Drops FZ_LOCK_ALLOC.
*/
static void
do_reap(fz_context *ctx)
{
	fz_store *store = ctx->store;
	fz_item *item, *prev, *remove;

	if (store == NULL)
	{
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		return;
	}

	fz_assert_lock_held(ctx, FZ_LOCK_ALLOC);

	ctx->store->needs_reaping = 0;

	/* Reap the items */
	remove = NULL;
	for (item = store->tail; item; item = prev)
	{
		prev = item->prev;

		if (item->type->needs_reap == NULL || item->type->needs_reap(ctx, item->key) == 0)
			continue;

		/* We have to drop it */
		store->size -= item->size;

		/* Unlink from the linked list */
		if (item->next)
			item->next->prev = item->prev;
		else
			store->tail = item->prev;
		if (item->prev)
			item->prev->next = item->next;
		else
			store->head = item->next;

		/* Remove from the hash table */
		if (item->type->make_hash_key)
		{
			fz_store_hash hash = { NULL };
			hash.drop = item->val->drop;
			if (item->type->make_hash_key(ctx, &hash, item->key))
				fz_hash_remove(ctx, store->hash, &hash);
		}

		/* Store whether to drop this value or not in 'prev' */
		if (item->val->refs > 0)
			(void)Memento_dropRef(item->val);
		item->prev = (item->val->refs > 0 && --item->val->refs == 0) ? item : NULL;

		/* Store it in our removal chain - just singly linked */
		item->next = remove;
		remove = item;
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	/* Now drop the remove chain */
	for (item = remove; item != NULL; item = remove)
	{
		remove = item->next;

		/* Drop a reference to the value (freeing if required) */
		if (item->prev)
			item->val->drop(ctx, item->val);

		/* Always drops the key and drop the item */
		item->type->drop_key(ctx, item->key);
		fz_free(ctx, item);
	}
}

void fz_drop_key_storable(fz_context *ctx, const fz_key_storable *sc)
{
	/* Explicitly drop const to allow us to use const
	 * sanely throughout the code. */
	fz_key_storable *s = (fz_key_storable *)sc;
	int drop;
	int unlock = 1;

	if (s == NULL)
		return;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	if (s->storable.refs > 0)
	{
		(void)Memento_dropRef(s);
		drop = --s->storable.refs == 0;
		if (!drop && s->storable.refs == s->store_key_refs)
		{
			if (ctx->store->defer_reap_count > 0)
			{
				ctx->store->needs_reaping = 1;
			}
			else
			{
				do_reap(ctx);
				unlock = 0;
			}
		}
	}
	else
		drop = 0;
	if (unlock)
		fz_unlock(ctx, FZ_LOCK_ALLOC);
	/*
		If we are dropping the last reference to an object, then
		it cannot possibly be in the store (as the store always
		keeps a ref to everything in it, and doesn't drop via
		this method. So we can simply drop the storable object
		itself without any operations on the fz_store.
	 */
	if (drop)
		s->storable.drop(ctx, &s->storable);
}

void *fz_keep_key_storable_key(fz_context *ctx, const fz_key_storable *sc)
{
	/* Explicitly drop const to allow us to use const
	 * sanely throughout the code. */
	fz_key_storable *s = (fz_key_storable *)sc;

	if (s == NULL)
		return NULL;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	if (s->storable.refs > 0)
	{
		(void)Memento_takeRef(s);
		++s->storable.refs;
		++s->store_key_refs;
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	return s;
}

void fz_drop_key_storable_key(fz_context *ctx, const fz_key_storable *sc)
{
	/* Explicitly drop const to allow us to use const
	 * sanely throughout the code. */
	fz_key_storable *s = (fz_key_storable *)sc;
	int drop;

	if (s == NULL)
		return;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	assert(s->store_key_refs > 0 && s->storable.refs >= s->store_key_refs);
	(void)Memento_dropRef(s);
	drop = --s->storable.refs == 0;
	--s->store_key_refs;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	/*
		If we are dropping the last reference to an object, then
		it cannot possibly be in the store (as the store always
		keeps a ref to everything in it, and doesn't drop via
		this method. So we can simply drop the storable object
		itself without any operations on the fz_store.
	 */
	if (drop)
		s->storable.drop(ctx, &s->storable);
}

static void
evict(fz_context *ctx, fz_item *item)
{
	fz_store *store = ctx->store;
	int drop;

	store->size -= item->size;
	/* Unlink from the linked list */
	if (item->next)
		item->next->prev = item->prev;
	else
		store->tail = item->prev;
	if (item->prev)
		item->prev->next = item->next;
	else
		store->head = item->next;

	/* Drop a reference to the value (freeing if required) */
	if (item->val->refs > 0)
		(void)Memento_dropRef(item->val);
	drop = (item->val->refs > 0 && --item->val->refs == 0);

	/* Remove from the hash table */
	if (item->type->make_hash_key)
	{
		fz_store_hash hash = { NULL };
		hash.drop = item->val->drop;
		if (item->type->make_hash_key(ctx, &hash, item->key))
			fz_hash_remove(ctx, store->hash, &hash);
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (drop)
		item->val->drop(ctx, item->val);

	/* Always drops the key and drop the item */
	item->type->drop_key(ctx, item->key);
	fz_free(ctx, item);
	fz_lock(ctx, FZ_LOCK_ALLOC);
}

static size_t
ensure_space(fz_context *ctx, size_t tofree)
{
	fz_item *item, *prev;
	size_t count;
	fz_store *store = ctx->store;
	fz_item *to_be_freed = NULL;

	fz_assert_lock_held(ctx, FZ_LOCK_ALLOC);

	/* First check that we *can* free tofree; if not, we'd rather not
	 * cache this. */
	count = 0;
	for (item = store->tail; item; item = item->prev)
	{
		if (item->val->refs == 1)
		{
			count += item->size;
			if (count >= tofree)
				break;
		}
	}

	/* If we ran out of items to search, then we can never free enough */
	if (item == NULL)
	{
		return 0;
	}

	/* Now move all the items to be freed onto 'to_be_freed' */
	count = 0;
	for (item = store->tail; item; item = prev)
	{
		prev = item->prev;
		if (item->val->refs != 1)
			continue;

		store->size -= item->size;

		/* Unlink from the linked list */
		if (item->next)
			item->next->prev = item->prev;
		else
			store->tail = item->prev;
		if (item->prev)
			item->prev->next = item->next;
		else
			store->head = item->next;

		/* Remove from the hash table */
		if (item->type->make_hash_key)
		{
			fz_store_hash hash = { NULL };
			hash.drop = item->val->drop;
			if (item->type->make_hash_key(ctx, &hash, item->key))
				fz_hash_remove(ctx, store->hash, &hash);
		}

		/* Link into to_be_freed */
		item->next = to_be_freed;
		to_be_freed = item;

		count += item->size;
		if (count >= tofree)
			break;
	}

	/* Now we can safely drop the lock and free our pending items. These
	 * have all been removed from both the store list, and the hash table,
	 * so they can't be 'found' by anyone else in the meantime. */

	while (to_be_freed)
	{
		fz_item *item = to_be_freed;
		int drop;

		to_be_freed = to_be_freed->next;

		/* Drop a reference to the value (freeing if required) */
		if (item->val->refs > 0)
			(void)Memento_dropRef(item->val);
		drop = (item->val->refs > 0 && --item->val->refs == 0);

		fz_unlock(ctx, FZ_LOCK_ALLOC);
		if (drop)
			item->val->drop(ctx, item->val);

		/* Always drops the key and drop the item */
		item->type->drop_key(ctx, item->key);
		fz_free(ctx, item);
		fz_lock(ctx, FZ_LOCK_ALLOC);
	}

	return count;
}

static void
touch(fz_store *store, fz_item *item)
{
	if (item->next != item)
	{
		/* Already in the list - unlink it */
		if (item->next)
			item->next->prev = item->prev;
		else
			store->tail = item->prev;
		if (item->prev)
			item->prev->next = item->next;
		else
			store->head = item->next;
	}
	/* Now relink it at the start of the LRU chain */
	item->next = store->head;
	if (item->next)
		item->next->prev = item;
	else
		store->tail = item;
	store->head = item;
	item->prev = NULL;
}

/*
	Add an item to the store.

	Add an item into the store, returning NULL for success. If an item
	with the same key is found in the store, then our item will not be
	inserted, and the function will return a pointer to that value
	instead. This function takes its own reference to val, as required
	(i.e. the caller maintains ownership of its own reference).

	key: The key used to index the item.

	val: The value to store.

	itemsize: The size in bytes of the value (as counted towards the
	store size).

	type: Functions used to manipulate the key.
*/
void *
fz_store_item(fz_context *ctx, void *key, void *val_, size_t itemsize, const fz_store_type *type)
{
	fz_item *item = NULL;
	size_t size;
	fz_storable *val = (fz_storable *)val_;
	fz_store *store = ctx->store;
	fz_store_hash hash = { NULL };
	int use_hash = 0;

	if (!store)
		return NULL;

	/* If we fail for any reason, we swallow the exception and continue.
	 * All that the above program will see is that we failed to store
	 * the item. */

	item = Memento_label(fz_malloc_no_throw(ctx, sizeof (fz_item)), "fz_item");
	if (!item)
		return NULL;
	memset(item, 0, sizeof (fz_item));

	if (type->make_hash_key)
	{
		hash.drop = val->drop;
		use_hash = type->make_hash_key(ctx, &hash, key);
	}

	type->keep_key(ctx, key);
	fz_lock(ctx, FZ_LOCK_ALLOC);

	/* Fill out the item. To start with, we always set item->next == item
	 * and item->prev == item. This is so that we can spot items that have
	 * been put into the hash table without having made it into the linked
	 * list yet. */
	item->key = key;
	item->val = val;
	item->size = itemsize;
	item->next = item;
	item->prev = item;
	item->type = type;

	/* If we can index it fast, put it into the hash table. This serves
	 * to check whether we have one there already. */
	if (use_hash)
	{
		fz_item *existing = NULL;

		fz_try(ctx)
		{
			/* May drop and retake the lock */
			existing = fz_hash_insert(ctx, store->hash, &hash, item);
		}
		fz_catch(ctx)
		{
			/* Any error here means that item never made it into the
			 * hash - so no one else can have a reference. */
			fz_unlock(ctx, FZ_LOCK_ALLOC);
			fz_free(ctx, item);
			type->drop_key(ctx, key);
			return NULL;
		}
		if (existing)
		{
			/* There was one there already! Take a new reference
			 * to the existing one, and drop our current one. */
			touch(store, existing);
			if (existing->val->refs > 0)
			{
				(void)Memento_takeRef(existing->val);
				existing->val->refs++;
			}
			fz_unlock(ctx, FZ_LOCK_ALLOC);
			fz_free(ctx, item);
			type->drop_key(ctx, key);
			return existing->val;
		}
	}

	/* Now bump the ref */
	if (val->refs > 0)
	{
		(void)Memento_takeRef(val);
		val->refs++;
	}

	/* If we haven't got an infinite store, check for space within it */
	if (store->max != FZ_STORE_UNLIMITED)
	{
		size = store->size + itemsize;
		while (size > store->max)
		{
			size_t saved;

			/* First, do any outstanding reaping, even if defer_reap_count > 0 */
			if (store->needs_reaping)
			{
				do_reap(ctx); /* Drops alloc lock */
				fz_lock(ctx, FZ_LOCK_ALLOC);
			}
			size = store->size + itemsize;
			if (size <= store->max)
				break;

			/* ensure_space may drop, then retake the lock */
			saved = ensure_space(ctx, size - store->max);
			size -= saved;
			if (saved == 0)
			{
				/* Failed to free any space. */
				/* We used to 'unstore' it here, but that's wrong.
				 * If we've already spent the memory to malloc it
				 * then not putting it in the store just means that
				 * a resource used multiple times will just be malloced
				 * again. Better to put it in the store, have the
				 * store account for it, and for it to potentially be reused.
				 * When the caller drops the reference to it, it can then
				 * be dropped from the store on the next attempt to store
				 * anything else. */
				break;
			}
		}
	}
	store->size += itemsize;

	/* Regardless of whether it's indexed, it goes into the linked list */
	touch(store, item);
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	return NULL;
}

/*
	Find an item within the store.

	drop: The function used to free the value (to ensure we get a value
	of the correct type).

	key: The key used to index the item.

	type: Functions used to manipulate the key.

	Returns NULL for not found, otherwise returns a pointer to the value
	indexed by key to which a reference has been taken.
*/
void *
fz_find_item(fz_context *ctx, fz_store_drop_fn *drop, void *key, const fz_store_type *type)
{
	fz_item *item;
	fz_store *store = ctx->store;
	fz_store_hash hash = { NULL };
	int use_hash = 0;

	if (!store)
		return NULL;

	if (!key)
		return NULL;

	if (type->make_hash_key)
	{
		hash.drop = drop;
		use_hash = type->make_hash_key(ctx, &hash, key);
	}

	fz_lock(ctx, FZ_LOCK_ALLOC);
	if (use_hash)
	{
		/* We can find objects keyed on indirected objects quickly */
		item = fz_hash_find(ctx, store->hash, &hash);
	}
	else
	{
		/* Others we have to hunt for slowly */
		for (item = store->head; item; item = item->next)
		{
			if (item->val->drop == drop && !type->cmp_key(ctx, item->key, key))
				break;
		}
	}
	if (item)
	{
		/* LRU the block. This also serves to ensure that any item
		 * picked up from the hash before it has made it into the
		 * linked list does not get whipped out again due to the
		 * store being full. */
		touch(store, item);
		/* And bump the refcount before returning */
		if (item->val->refs > 0)
		{
			(void)Memento_takeRef(item->val);
			item->val->refs++;
		}
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		return (void *)item->val;
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	return NULL;
}

/*
	Remove an item from the store.

	If an item indexed by the given key exists in the store, remove it.

	drop: The function used to free the value (to ensure we get a value
	of the correct type).

	key: The key used to find the item to remove.

	type: Functions used to manipulate the key.
*/
void
fz_remove_item(fz_context *ctx, fz_store_drop_fn *drop, void *key, const fz_store_type *type)
{
	fz_item *item;
	fz_store *store = ctx->store;
	int dodrop;
	fz_store_hash hash = { NULL };
	int use_hash = 0;

	if (type->make_hash_key)
	{
		hash.drop = drop;
		use_hash = type->make_hash_key(ctx, &hash, key);
	}

	fz_lock(ctx, FZ_LOCK_ALLOC);
	if (use_hash)
	{
		/* We can find objects keyed on indirect objects quickly */
		item = fz_hash_find(ctx, store->hash, &hash);
		if (item)
			fz_hash_remove(ctx, store->hash, &hash);
	}
	else
	{
		/* Others we have to hunt for slowly */
		for (item = store->head; item; item = item->next)
			if (item->val->drop == drop && !type->cmp_key(ctx, item->key, key))
				break;
	}
	if (item)
	{
		/* Momentarily things can be in the hash table without being
		 * in the list. Don't attempt to unlink these. We indicate
		 * such items by setting item->next == item. */
		if (item->next != item)
		{
			if (item->next)
				item->next->prev = item->prev;
			else
				store->tail = item->prev;
			if (item->prev)
				item->prev->next = item->next;
			else
				store->head = item->next;
		}
		if (item->val->refs > 0)
			(void)Memento_dropRef(item->val);
		dodrop = (item->val->refs > 0 && --item->val->refs == 0);
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		if (dodrop)
			item->val->drop(ctx, item->val);
		type->drop_key(ctx, item->key);
		fz_free(ctx, item);
	}
	else
		fz_unlock(ctx, FZ_LOCK_ALLOC);
}

void
fz_empty_store(fz_context *ctx)
{
	fz_store *store = ctx->store;

	if (store == NULL)
		return;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	/* Run through all the items in the store */
	while (store->head)
		evict(ctx, store->head); /* Drops then retakes lock */
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}

fz_store *
fz_keep_store_context(fz_context *ctx)
{
	if (ctx == NULL || ctx->store == NULL)
		return NULL;
	return fz_keep_imp(ctx, ctx->store, &ctx->store->refs);
}

void
fz_drop_store_context(fz_context *ctx)
{
	if (!ctx)
		return;
	if (fz_drop_imp(ctx, ctx->store, &ctx->store->refs))
	{
		fz_empty_store(ctx);
		fz_drop_hash_table(ctx, ctx->store->hash);
		fz_free(ctx, ctx->store);
		ctx->store = NULL;
	}
}

static void
fz_debug_store_item(fz_context *ctx, void *state, void *key_, int keylen, void *item_)
{
	unsigned char *key = key_;
	fz_item *item = item_;
	int i;
	char buf[256];
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	item->type->format_key(ctx, buf, sizeof buf, item->key);
	fz_lock(ctx, FZ_LOCK_ALLOC);
	printf("hash[");
	for (i=0; i < keylen; ++i)
		printf("%02x", key[i]);
	printf("][refs=%d][size=%d] key=%s val=%p\n", item->val->refs, (int)item->size, buf, (void *)item->val);
}

static void
fz_debug_store_locked(fz_context *ctx)
{
	fz_item *item, *next;
	char buf[256];
	fz_store *store = ctx->store;

	printf("-- resource store contents --\n");

	for (item = store->head; item; item = next)
	{
		next = item->next;
		if (next)
		{
			(void)Memento_takeRef(next->val);
			next->val->refs++;
		}
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		item->type->format_key(ctx, buf, sizeof buf, item->key);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		printf("store[*][refs=%d][size=%d] key=%s val=%p\n",
				item->val->refs, (int)item->size, buf, (void *)item->val);
		if (next)
		{
			(void)Memento_dropRef(next->val);
			next->val->refs--;
		}
	}

	printf("-- resource store hash contents --\n");
	fz_hash_for_each(ctx, store->hash, NULL, fz_debug_store_item);
	printf("-- end --\n");
}

void
fz_debug_store(fz_context *ctx)
{
	fz_lock(ctx, FZ_LOCK_ALLOC);
	fz_debug_store_locked(ctx);
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}

/* This is now an n^2 algorithm - not ideal, but it'll only be bad if we are
 * actually managing to scavenge lots of blocks back. */
static int
scavenge(fz_context *ctx, size_t tofree)
{
	fz_store *store = ctx->store;
	size_t count = 0;
	fz_item *item, *prev;

	/* Free the items */
	for (item = store->tail; item; item = prev)
	{
		prev = item->prev;
		if (item->val->refs == 1)
		{
			/* Free this item */
			count += item->size;
			evict(ctx, item); /* Drops then retakes lock */

			if (count >= tofree)
				break;

			/* Have to restart search again, as prev may no longer
			 * be valid due to release of lock in evict. */
			prev = store->tail;
		}
	}
	/* Success is managing to evict any blocks */
	return count != 0;
}

void
fz_drop_storable(fz_context *ctx, const fz_storable *sc)
{
	/* Explicitly drop const to allow us to use const
	 * sanely throughout the code. */
	fz_storable *s = (fz_storable *)sc;
	int num;

	if (s == NULL)
		return;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	/* Drop the ref, and leave num as being the number of
	 * refs left (-1 meaning, "statically allocated"). */
	if (s->refs > 0)
	{
		(void)Memento_dropIntRef(s);
		num = --s->refs;
	}
	else
		num = -1;

	/* If we have just 1 ref left, it's possible that
	 * this ref is held by the store. If the store is
	 * oversized, we ought to throw any such references
	 * away to try to bring the store down to a "legal"
	 * size. Run a scavenge to check for this case. */
	if (ctx->store->max != FZ_STORE_UNLIMITED)
		if (num == 1 && ctx->store->size > ctx->store->max)
			scavenge(ctx, ctx->store->size - ctx->store->max);
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	/* If we have no references to an object left, then
	 * it cannot possibly be in the store (as the store always
	 * keeps a ref to everything in it, and doesn't drop via
	 * this method). So we can simply drop the storable object
	 * itself without any operations on the fz_store.
	 */
	if (num == 0)
		s->drop(ctx, s);
}

/*
	External function for callers to use
	to scavenge while trying allocations.

	size: The number of bytes we are trying to have free.

	phase: What phase of the scavenge we are in. Updated on exit.

	Returns non zero if we managed to free any memory.
*/
int fz_store_scavenge_external(fz_context *ctx, size_t size, int *phase)
{
	int ret;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	ret = fz_store_scavenge(ctx, size, phase);
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	return ret;
}

/*
	Internal function used as part of the scavenging
	allocator; when we fail to allocate memory, before returning a
	failure to the caller, we try to scavenge space within the store by
	evicting at least 'size' bytes. The allocator then retries.

	size: The number of bytes we are trying to have free.

	phase: What phase of the scavenge we are in. Updated on exit.

	Returns non zero if we managed to free any memory.
*/
int fz_store_scavenge(fz_context *ctx, size_t size, int *phase)
{
	fz_store *store;
	size_t max;

	store = ctx->store;
	if (store == NULL)
		return 0;

#ifdef DEBUG_SCAVENGING
	fz_write_printf(ctx, fz_stdout(ctx), "Scavenging: store=%zu size=%zu phase=%d\n", store->size, size, *phase);
	fz_debug_store_locked(ctx);
	Memento_stats();
#endif
	do
	{
		size_t tofree;

		/* Calculate 'max' as the maximum size of the store for this phase */
		if (*phase >= 16)
			max = 0;
		else if (store->max != FZ_STORE_UNLIMITED)
			max = store->max / 16 * (16 - *phase);
		else
			max = store->size / (16 - *phase) * (15 - *phase);
		(*phase)++;

		/* Slightly baroque calculations to avoid overflow */
		if (size > SIZE_MAX - store->size)
			tofree = SIZE_MAX - max;
		else if (size + store->size > max)
			continue;
		else
			tofree = size + store->size - max;

		if (scavenge(ctx, tofree))
		{
#ifdef DEBUG_SCAVENGING
			fz_write_printf(ctx, fz_stdout(ctx), "scavenged: store=%zu\n", store->size);
			fz_debug_store(ctx);
			Memento_stats();
#endif
			return 1;
		}
	}
	while (max > 0);

#ifdef DEBUG_SCAVENGING
	printf("scavenging failed\n");
	fz_debug_store(ctx);
	Memento_listBlocks();
#endif
	return 0;
}

/*
	Evict items from the store until the total size of
	the objects in the store is reduced to a given percentage of its
	current size.

	percent: %age of current size to reduce the store to.

	Returns non zero if we managed to free enough memory, zero otherwise.
*/
int
fz_shrink_store(fz_context *ctx, unsigned int percent)
{
	int success;
	fz_store *store;
	size_t new_size;

	if (percent >= 100)
		return 1;

	store = ctx->store;
	if (store == NULL)
		return 0;

#ifdef DEBUG_SCAVENGING
	fz_write_printf(ctx, fz_stdout(ctx), "fz_shrink_store: %zu\n", store->size/(1024*1024));
#endif
	fz_lock(ctx, FZ_LOCK_ALLOC);

	new_size = (size_t)(((uint64_t)store->size * percent) / 100);
	if (store->size > new_size)
		scavenge(ctx, store->size - new_size);

	success = (store->size <= new_size) ? 1 : 0;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
#ifdef DEBUG_SCAVENGING
	fz_write_printf(ctx, fz_stdout(ctx), "fz_shrink_store after: %zu\n", store->size/(1024*1024));
#endif

	return success;
}

void fz_filter_store(fz_context *ctx, fz_store_filter_fn *fn, void *arg, const fz_store_type *type)
{
	fz_store *store;
	fz_item *item, *prev, *remove;

	store = ctx->store;
	if (store == NULL)
		return;

	fz_lock(ctx, FZ_LOCK_ALLOC);

	/* Filter the items */
	remove = NULL;
	for (item = store->tail; item; item = prev)
	{
		prev = item->prev;
		if (item->type != type)
			continue;

		if (fn(ctx, arg, item->key) == 0)
			continue;

		/* We have to drop it */
		store->size -= item->size;

		/* Unlink from the linked list */
		if (item->next)
			item->next->prev = item->prev;
		else
			store->tail = item->prev;
		if (item->prev)
			item->prev->next = item->next;
		else
			store->head = item->next;

		/* Remove from the hash table */
		if (item->type->make_hash_key)
		{
			fz_store_hash hash = { NULL };
			hash.drop = item->val->drop;
			if (item->type->make_hash_key(ctx, &hash, item->key))
				fz_hash_remove(ctx, store->hash, &hash);
		}

		/* Store whether to drop this value or not in 'prev' */
		if (item->val->refs > 0)
			(void)Memento_dropRef(item->val);
		item->prev = (item->val->refs > 0 && --item->val->refs == 0) ? item : NULL;

		/* Store it in our removal chain - just singly linked */
		item->next = remove;
		remove = item;
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	/* Now drop the remove chain */
	for (item = remove; item != NULL; item = remove)
	{
		remove = item->next;

		/* Drop a reference to the value (freeing if required) */
		if (item->prev) /* See above for our abuse of prev here */
			item->val->drop(ctx, item->val);

		/* Always drops the key and drop the item */
		item->type->drop_key(ctx, item->key);
		fz_free(ctx, item);
	}
}

/*
	Increment the defer reap count.

	No reap operations will take place (except for those
	triggered by an immediate failed malloc) until the
	defer reap count returns to 0.

	Call this at the start of a process during which you
	potentially might drop many reapable objects.

	It is vital that every fz_defer_reap_start is matched
	by a fz_defer_reap_end call.
*/
void fz_defer_reap_start(fz_context *ctx)
{
	if (ctx->store == NULL)
		return;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	ctx->store->defer_reap_count++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}

/*
	Decrement the defer reap count.

	If the defer reap count returns to 0, and the store
	has reapable objects in, a reap pass will begin.

	Call this at the end of a process during which you
	potentially might drop many reapable objects.

	It is vital that every fz_defer_reap_start is matched
	by a fz_defer_reap_end call.
*/
void fz_defer_reap_end(fz_context *ctx)
{
	int reap;

	if (ctx->store == NULL)
		return;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	--ctx->store->defer_reap_count;
	reap = ctx->store->defer_reap_count == 0 && ctx->store->needs_reaping;
	if (reap)
		do_reap(ctx); /* Drops FZ_LOCK_ALLOC */
	else
		fz_unlock(ctx, FZ_LOCK_ALLOC);
}
