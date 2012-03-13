#include "fitz-internal.h"

/*
Simple hashtable with open addressing linear probe.
Unlike text book examples, removing entries works
correctly in this implementation, so it wont start
exhibiting bad behaviour if entries are inserted
and removed frequently.
*/

enum { MAX_KEY_LEN = 48 };
typedef struct fz_hash_entry_s fz_hash_entry;

struct fz_hash_entry_s
{
	unsigned char key[MAX_KEY_LEN];
	void *val;
};

struct fz_hash_table_s
{
	int keylen;
	int size;
	int load;
	int lock; /* -1 or the lock used to protect this hash table */
	fz_hash_entry *ents;
};

static unsigned hash(unsigned char *s, int len)
{
	unsigned val = 0;
	int i;
	for (i = 0; i < len; i++)
	{
		val += s[i];
		val += (val << 10);
		val ^= (val >> 6);
	}
	val += (val << 3);
	val ^= (val >> 11);
	val += (val << 15);
	return val;
}

fz_hash_table *
fz_new_hash_table(fz_context *ctx, int initialsize, int keylen, int lock)
{
	fz_hash_table *table;

	assert(keylen <= MAX_KEY_LEN);

	table = fz_malloc_struct(ctx, fz_hash_table);
	table->keylen = keylen;
	table->size = initialsize;
	table->load = 0;
	table->lock = lock;
	fz_try(ctx)
	{
		table->ents = fz_malloc_array(ctx, table->size, sizeof(fz_hash_entry));
		memset(table->ents, 0, sizeof(fz_hash_entry) * table->size);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, table);
		fz_rethrow(ctx);
	}

	return table;
}

void
fz_empty_hash(fz_context *ctx, fz_hash_table *table)
{
	table->load = 0;
	memset(table->ents, 0, sizeof(fz_hash_entry) * table->size);
}

int
fz_hash_len(fz_context *ctx, fz_hash_table *table)
{
	return table->size;
}

void *
fz_hash_get_key(fz_context *ctx, fz_hash_table *table, int idx)
{
	return table->ents[idx].key;
}

void *
fz_hash_get_val(fz_context *ctx, fz_hash_table *table, int idx)
{
	return table->ents[idx].val;
}

void
fz_free_hash(fz_context *ctx, fz_hash_table *table)
{
	fz_free(ctx, table->ents);
	fz_free(ctx, table);
}

static void *
do_hash_insert(fz_context *ctx, fz_hash_table *table, void *key, void *val)
{
	fz_hash_entry *ents;
	unsigned size;
	unsigned pos;

	ents = table->ents;
	size = table->size;
	pos = hash(key, table->keylen) % size;

	if (table->lock >= 0)
		fz_assert_lock_held(ctx, table->lock);

	while (1)
	{
		if (!ents[pos].val)
		{
			memcpy(ents[pos].key, key, table->keylen);
			ents[pos].val = val;
			table->load ++;
			return NULL;
		}

		if (memcmp(key, ents[pos].key, table->keylen) == 0)
		{
			fz_warn(ctx, "assert: overwrite hash slot");
			return ents[pos].val;
		}

		pos = (pos + 1) % size;
	}
}

static void
fz_resize_hash(fz_context *ctx, fz_hash_table *table, int newsize)
{
	fz_hash_entry *oldents = table->ents;
	fz_hash_entry *newents = table->ents;
	int oldsize = table->size;
	int oldload = table->load;
	int i;

	if (newsize < oldload * 8 / 10)
	{
		fz_warn(ctx, "assert: resize hash too small");
		return;
	}

	if (table->lock == FZ_LOCK_ALLOC)
		fz_unlock(ctx, FZ_LOCK_ALLOC);
	newents = fz_malloc_array(ctx, newsize, sizeof(fz_hash_entry));
	if (table->lock == FZ_LOCK_ALLOC)
		fz_lock(ctx, FZ_LOCK_ALLOC);
	if (table->lock >= 0)
	{
		if (table->size >= newsize)
		{
			/* Someone else fixed it before we could lock! */
			fz_unlock(ctx, table->lock);
			fz_free(ctx, newents);
			return;
		}
	}
	table->ents = newents;
	memset(table->ents, 0, sizeof(fz_hash_entry) * newsize);
	table->size = newsize;
	table->load = 0;

	for (i = 0; i < oldsize; i++)
	{
		if (oldents[i].val)
		{
			do_hash_insert(ctx, table, oldents[i].key, oldents[i].val);
		}
	}

	if (table->lock == FZ_LOCK_ALLOC)
		fz_unlock(ctx, FZ_LOCK_ALLOC);
	fz_free(ctx, oldents);
	if (table->lock == FZ_LOCK_ALLOC)
		fz_lock(ctx, FZ_LOCK_ALLOC);
}

void *
fz_hash_find(fz_context *ctx, fz_hash_table *table, void *key)
{
	fz_hash_entry *ents = table->ents;
	unsigned size = table->size;
	unsigned pos = hash(key, table->keylen) % size;

	if (table->lock >= 0)
		fz_assert_lock_held(ctx, table->lock);

	while (1)
	{
		if (!ents[pos].val)
			return NULL;

		if (memcmp(key, ents[pos].key, table->keylen) == 0)
			return ents[pos].val;

		pos = (pos + 1) % size;
	}
}

void *
fz_hash_insert(fz_context *ctx, fz_hash_table *table, void *key, void *val)
{
	if (table->load > table->size * 8 / 10)
	{
		fz_resize_hash(ctx, table, table->size * 2);
	}

	return do_hash_insert(ctx, table, key, val);
}

void
fz_hash_remove(fz_context *ctx, fz_hash_table *table, void *key)
{
	fz_hash_entry *ents = table->ents;
	unsigned size = table->size;
	unsigned pos = hash(key, table->keylen) % size;
	unsigned hole, look, code;

	if (table->lock >= 0)
		fz_assert_lock_held(ctx, table->lock);

	while (1)
	{
		if (!ents[pos].val)
		{
			fz_warn(ctx, "assert: remove non-existent hash entry");
			return;
		}

		if (memcmp(key, ents[pos].key, table->keylen) == 0)
		{
			ents[pos].val = NULL;

			hole = pos;
			look = (hole + 1) % size;

			while (ents[look].val)
			{
				code = hash(ents[look].key, table->keylen) % size;
				if ((code <= hole && hole < look) ||
					(look < code && code <= hole) ||
					(hole < look && look < code))
				{
					ents[hole] = ents[look];
					ents[look].val = NULL;
					hole = look;
				}

				look = (look + 1) % size;
			}

			table->load --;

			return;
		}

		pos = (pos + 1) % size;
	}
}

void
fz_print_hash(fz_context *ctx, FILE *out, fz_hash_table *table)
{
	int i, k;

	fprintf(out, "cache load %d / %d\n", table->load, table->size);

	for (i = 0; i < table->size; i++)
	{
		if (!table->ents[i].val)
			fprintf(out, "table % 4d: empty\n", i);
		else
		{
			fprintf(out, "table % 4d: key=", i);
			for (k = 0; k < MAX_KEY_LEN; k++)
				fprintf(out, "%02x", ((char*)table->ents[i].key)[k]);
			fprintf(out, " val=$%p\n", table->ents[i].val);
		}
	}
}
