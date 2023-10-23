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

#include "mupdf/fitz.h"

#include <string.h>
#include <assert.h>

/*
	Simple hashtable with open addressing linear probe.
	Unlike text book examples, removing entries works
	correctly in this implementation, so it won't start
	exhibiting bad behaviour if entries are inserted
	and removed frequently.
*/

typedef struct
{
	unsigned char key[FZ_HASH_TABLE_KEY_LENGTH];
	void *val;
} fz_hash_entry;

struct fz_hash_table
{
	int keylen;
	int size;
	int load;
	int lock; /* -1 or the lock used to protect this hash table */
	fz_hash_table_drop_fn *drop_val;
	fz_hash_entry *ents;
};

static unsigned hash(const unsigned char *s, int len)
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
fz_new_hash_table(fz_context *ctx, int initialsize, int keylen, int lock, fz_hash_table_drop_fn *drop_val)
{
	fz_hash_table *table;

	if (keylen > FZ_HASH_TABLE_KEY_LENGTH)
		fz_throw(ctx, FZ_ERROR_GENERIC, "hash table key length too large");

	table = fz_malloc_struct(ctx, fz_hash_table);
	table->keylen = keylen;
	table->size = initialsize;
	table->load = 0;
	table->lock = lock;
	table->drop_val = drop_val;
	fz_try(ctx)
	{
		table->ents = Memento_label(fz_malloc_array(ctx, table->size, fz_hash_entry), "hash_entries");
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
fz_drop_hash_table(fz_context *ctx, fz_hash_table *table)
{
	if (!table)
		return;

	if (table->drop_val)
	{
		int i, n = table->size;
		for (i = 0; i < n; ++i)
		{
			void *v = table->ents[i].val;
			if (v)
				table->drop_val(ctx, v);
		}
	}

	fz_free(ctx, table->ents);
	fz_free(ctx, table);
}

static void *
do_hash_insert(fz_context *ctx, fz_hash_table *table, const void *key, void *val)
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
			/* This is legal, but should rarely happen. */
			return ents[pos].val;
		}

		pos = (pos + 1) % size;
	}
}

/* Entered with the lock taken, held throughout and at exit, UNLESS the lock
 * is the alloc lock in which case it may be momentarily dropped. */
static void
fz_resize_hash(fz_context *ctx, fz_hash_table *table, int newsize)
{
	fz_hash_entry *oldents = table->ents;
	fz_hash_entry *newents;
	int oldsize = table->size;
	int oldload = table->load;
	int i;

	if (newsize < oldload * 8 / 10)
	{
		fz_warn(ctx, "assert: resize hash too small");
		return;
	}

	if (table->lock == FZ_LOCK_ALLOC)
		fz_unlock(ctx, table->lock);
	newents = fz_malloc_no_throw(ctx, newsize * sizeof (fz_hash_entry));
	if (table->lock == FZ_LOCK_ALLOC)
		fz_lock(ctx, table->lock);
	if (table->lock >= 0)
	{
		if (table->size >= newsize)
		{
			/* Someone else fixed it before we could lock! */
			if (table->lock == FZ_LOCK_ALLOC)
				fz_unlock(ctx, table->lock);
			fz_free(ctx, newents);
			if (table->lock == FZ_LOCK_ALLOC)
				fz_lock(ctx, table->lock);
			return;
		}
	}
	if (newents == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "hash table resize failed; out of memory (%d entries)", newsize);
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
		fz_unlock(ctx, table->lock);
	fz_free(ctx, oldents);
	if (table->lock == FZ_LOCK_ALLOC)
		fz_lock(ctx, table->lock);
}

void *
fz_hash_find(fz_context *ctx, fz_hash_table *table, const void *key)
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
fz_hash_insert(fz_context *ctx, fz_hash_table *table, const void *key, void *val)
{
	if (table->load > table->size * 8 / 10)
		fz_resize_hash(ctx, table, table->size * 2);
	return do_hash_insert(ctx, table, key, val);
}

static void
do_removal(fz_context *ctx, fz_hash_table *table, unsigned hole)
{
	fz_hash_entry *ents = table->ents;
	unsigned size = table->size;
	unsigned look, code;

	if (table->lock >= 0)
		fz_assert_lock_held(ctx, table->lock);

	ents[hole].val = NULL;

	look = hole + 1;
	if (look == size)
		look = 0;

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

		look++;
		if (look == size)
			look = 0;
	}

	table->load --;
}

void
fz_hash_remove(fz_context *ctx, fz_hash_table *table, const void *key)
{
	fz_hash_entry *ents = table->ents;
	unsigned size = table->size;
	unsigned pos = hash(key, table->keylen) % size;

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
			do_removal(ctx, table, pos);
			return;
		}

		pos++;
		if (pos == size)
			pos = 0;
	}
}

void
fz_hash_for_each(fz_context *ctx, fz_hash_table *table, void *state, fz_hash_table_for_each_fn *callback)
{
	int i;
	for (i = 0; i < table->size; ++i)
		if (table->ents[i].val)
			callback(ctx, state, table->ents[i].key, table->keylen, table->ents[i].val);
}

void
fz_hash_filter(fz_context *ctx, fz_hash_table *table, void *state, fz_hash_table_filter_fn *callback)
{
	int i;
restart:
	for (i = 0; i < table->size; ++i)
	{
		if (table->ents[i].val)
		{
			if (callback(ctx, state, table->ents[i].key, table->keylen, table->ents[i].val))
			{
				do_removal(ctx, table, i);
				goto restart; /* we may have moved some slots around, so just restart the scan */
			}
		}
	}
}
