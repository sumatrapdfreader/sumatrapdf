#include "mupdf/fitz.h"

typedef struct fz_item_s fz_item;

struct fz_item_s
{
	void *key;
	fz_storable *val;
	unsigned int size;
	fz_item *next;
	fz_item *prev;
	fz_store *store;
	fz_store_type *type;
};

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
	unsigned int max;
	unsigned int size;
};

void
fz_new_store_context(fz_context *ctx, unsigned int max)
{
	fz_store *store;
	store = fz_malloc_struct(ctx, fz_store);
	fz_try(ctx)
	{
		store->hash = fz_new_hash_table(ctx, 4096, sizeof(fz_store_hash), FZ_LOCK_ALLOC);
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
	ctx->store = store;
}

void *
fz_keep_storable(fz_context *ctx, fz_storable *s)
{
	if (s == NULL)
		return NULL;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	if (s->refs > 0)
		s->refs++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	return s;
}

void
fz_drop_storable(fz_context *ctx, fz_storable *s)
{
	int do_free = 0;

	if (s == NULL)
		return;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	if (s->refs < 0)
	{
		/* It's a static object. Dropping does nothing. */
	}
	else if (--s->refs == 0)
	{
		/* If we are dropping the last reference to an object, then
		 * it cannot possibly be in the store (as the store always
		 * keeps a ref to everything in it, and doesn't drop via
		 * this method. So we can simply drop the storable object
		 * itself without any operations on the fz_store. */
		do_free = 1;
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (do_free)
		s->free(ctx, s);
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
	drop = (item->val->refs > 0 && --item->val->refs == 0);
	/* Remove from the hash table */
	if (item->type->make_hash_key)
	{
		fz_store_hash hash = { NULL };
		hash.free = item->val->free;
		if (item->type->make_hash_key(&hash, item->key))
			fz_hash_remove(ctx, store->hash, &hash);
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (drop)
		item->val->free(ctx, item->val);
	/* Always drops the key and free the item */
	item->type->drop_key(ctx, item->key);
	fz_free(ctx, item);
	fz_lock(ctx, FZ_LOCK_ALLOC);
}

static int
ensure_space(fz_context *ctx, unsigned int tofree)
{
	fz_item *item, *prev;
	unsigned int count;
	fz_store *store = ctx->store;

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

	/* Actually free the items */
	count = 0;
	for (item = store->tail; item; item = prev)
	{
		prev = item->prev;
		if (item->val->refs == 1)
		{
			/* Free this item. Evict has to drop the lock to
			 * manage that, which could cause prev to be removed
			 * in the meantime. To avoid that we bump its reference
			 * count here. This may cause another simultaneous
			 * evict process to fail to make enough space as prev is
			 * pinned - but that will only happen if we're near to
			 * the limit anyway, and it will only cause something to
			 * not be cached. */
			count += item->size;
			if (prev)
				prev->val->refs++;
			evict(ctx, item); /* Drops then retakes lock */
			/* So the store has 1 reference to prev, as do we, so
			 * no other evict process can have thrown prev away in
			 * the meantime. So we are safe to just decrement its
			 * reference count here. */
			if (prev)
				--prev->val->refs;

			if (count >= tofree)
				return count;
		}
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

void *
fz_store_item(fz_context *ctx, void *key, void *val_, unsigned int itemsize, fz_store_type *type)
{
	fz_item *item = NULL;
	unsigned int size;
	fz_storable *val = (fz_storable *)val_;
	fz_store *store = ctx->store;
	fz_store_hash hash = { NULL };
	int use_hash = 0;
	unsigned pos;

	if (!store)
		return NULL;

	fz_var(item);

	if (store->max != FZ_STORE_UNLIMITED && store->max < itemsize)
	{
		/* Our item would take up more room than we can ever
		 * possibly have in the store. Just give up now. */
		return NULL;
	}

	/* If we fail for any reason, we swallow the exception and continue.
	 * All that the above program will see is that we failed to store
	 * the item. */
	fz_try(ctx)
	{
		item = fz_malloc_struct(ctx, fz_item);
	}
	fz_catch(ctx)
	{
		return NULL;
	}

	if (type->make_hash_key)
	{
		hash.free = val->free;
		use_hash = type->make_hash_key(&hash, key);
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
		fz_item *existing;

		fz_try(ctx)
		{
			/* May drop and retake the lock */
			existing = fz_hash_insert_with_pos(ctx, store->hash, &hash, item, &pos);
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
				existing->val->refs++;
			fz_unlock(ctx, FZ_LOCK_ALLOC);
			fz_free(ctx, item);
			type->drop_key(ctx, key);
			return existing->val;
		}
	}
	/* Now bump the ref */
	if (val->refs > 0)
		val->refs++;
	/* If we haven't got an infinite store, check for space within it */
	if (store->max != FZ_STORE_UNLIMITED)
	{
		size = store->size + itemsize;
		while (size > store->max)
		{
			/* ensure_space may drop, then retake the lock */
			int saved = ensure_space(ctx, size - store->max);
			if (saved == 0)
			{
				/* Failed to free any space. */
				/* If we are using the hash table, then we've already
				 * inserted item - remove it. */
				if (use_hash)
				{
					/* If someone else has already picked up a reference
					 * to item, then we cannot remove it. Leave it in the
					 * store, and we'll live with being over budget. We
					 * know this is the case, if it's in the linked list. */
					if (item->next != item)
						break;
					fz_hash_remove_fast(ctx, store->hash, &hash, pos);
				}
				fz_unlock(ctx, FZ_LOCK_ALLOC);
				fz_free(ctx, item);
				type->drop_key(ctx, key);
				if (val->refs > 0)
					val->refs--;
				return NULL;
			}
			size -= saved;
		}
	}
	store->size += itemsize;

	/* Regardless of whether it's indexed, it goes into the linked list */
	touch(store, item);
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	return NULL;
}

void *
fz_find_item(fz_context *ctx, fz_store_free_fn *free, void *key, fz_store_type *type)
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
		hash.free = free;
		use_hash = type->make_hash_key(&hash, key);
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
			if (item->val->free == free && !type->cmp_key(item->key, key))
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
			item->val->refs++;
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		return (void *)item->val;
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	return NULL;
}

void
fz_remove_item(fz_context *ctx, fz_store_free_fn *free, void *key, fz_store_type *type)
{
	fz_item *item;
	fz_store *store = ctx->store;
	int drop;
	fz_store_hash hash = { NULL };
	int use_hash = 0;

	if (type->make_hash_key)
	{
		hash.free = free;
		use_hash = type->make_hash_key(&hash, key);
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
			if (item->val->free == free && !type->cmp_key(item->key, key))
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
		drop = (item->val->refs > 0 && --item->val->refs == 0);
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		if (drop)
			item->val->free(ctx, item->val);
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
	{
		evict(ctx, store->head); /* Drops then retakes lock */
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}

fz_store *
fz_keep_store_context(fz_context *ctx)
{
	if (ctx == NULL || ctx->store == NULL)
		return NULL;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	ctx->store->refs++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	return ctx->store;
}

void
fz_drop_store_context(fz_context *ctx)
{
	int refs;
	if (ctx == NULL || ctx->store == NULL)
		return;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	refs = --ctx->store->refs;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (refs != 0)
		return;

	fz_empty_store(ctx);
	fz_free_hash(ctx, ctx->store->hash);
	fz_free(ctx, ctx->store);
	ctx->store = NULL;
}

#ifndef NDEBUG
static void
print_item(FILE *out, void *item_)
{
	fz_item *item = (fz_item *)item_;
	fprintf(out, " val=%p item=%p\n", item->val, item);
	fflush(out);
}

void
fz_print_store_locked(fz_context *ctx, FILE *out)
{
	fz_item *item, *next;
	fz_store *store = ctx->store;

	fprintf(out, "-- resource store contents --\n");
	fflush(out);

	for (item = store->head; item; item = next)
	{
		next = item->next;
		if (next)
			next->val->refs++;
		fprintf(out, "store[*][refs=%d][size=%d] ", item->val->refs, item->size);
		fz_unlock(ctx, FZ_LOCK_ALLOC);
		item->type->debug(out, item->key);
		fprintf(out, " = %p\n", item->val);
		fflush(out);
		fz_lock(ctx, FZ_LOCK_ALLOC);
		if (next)
			next->val->refs--;
	}
	fprintf(out, "-- resource store hash contents --\n");
	fz_print_hash_details(ctx, out, store->hash, print_item);
	fprintf(out, "-- end --\n");
	fflush(out);
}

void
fz_print_store(fz_context *ctx, FILE *out)
{
	fz_lock(ctx, FZ_LOCK_ALLOC);
	fz_print_store_locked(ctx, out);
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}
#endif

/* This is now an n^2 algorithm - not ideal, but it'll only be bad if we are
 * actually managing to scavenge lots of blocks back. */
static int
scavenge(fz_context *ctx, unsigned int tofree)
{
	fz_store *store = ctx->store;
	unsigned int count = 0;
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

int fz_store_scavenge(fz_context *ctx, unsigned int size, int *phase)
{
	fz_store *store;
	unsigned int max;

	if (ctx == NULL)
		return 0;
	store = ctx->store;
	if (store == NULL)
		return 0;

#ifdef DEBUG_SCAVENGING
	printf("Scavenging: store=%d size=%d phase=%d\n", store->size, size, *phase);
	fz_print_store_locked(ctx, stderr);
	Memento_stats();
#endif
	do
	{
		unsigned int tofree;

		/* Calculate 'max' as the maximum size of the store for this phase */
		if (*phase >= 16)
			max = 0;
		else if (store->max != FZ_STORE_UNLIMITED)
			max = store->max / 16 * (16 - *phase);
		else
			max = store->size / (16 - *phase) * (15 - *phase);
		(*phase)++;

		/* Slightly baroque calculations to avoid overflow */
		if (size > UINT_MAX - store->size)
			tofree = UINT_MAX - max;
		else if (size + store->size > max)
			continue;
		else
			tofree = size + store->size - max;

		if (scavenge(ctx, tofree))
		{
#ifdef DEBUG_SCAVENGING
			printf("scavenged: store=%d\n", store->size);
			fz_print_store(ctx, stderr);
			Memento_stats();
#endif
			return 1;
		}
	}
	while (max > 0);

#ifdef DEBUG_SCAVENGING
	printf("scavenging failed\n");
	fz_print_store(ctx, stderr);
	Memento_listBlocks();
#endif
	return 0;
}
