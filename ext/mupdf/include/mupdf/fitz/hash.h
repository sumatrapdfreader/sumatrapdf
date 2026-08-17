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

#ifndef MUPDF_FITZ_HASH_H
#define MUPDF_FITZ_HASH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"

#define FZ_HASH_TABLE_KEY_LENGTH 48

/**
	Generic hash-table with fixed-length keys.

	The keys and values are NOT reference counted by the hash table.
	Callers are responsible for taking care the reference counts are
	correct. Inserting a duplicate entry will NOT overwrite the old
	value, and will return the old value.

	The drop_val callback function is only used to release values
	when the hash table is destroyed.
*/

typedef struct fz_hash_table fz_hash_table;

/**
	Function type called when a hash table entry is dropped.

	Only used when the entire hash table is dropped.
*/
typedef void (fz_hash_table_drop_fn)(fz_context *ctx, void *val);

/**
	Create a new hash table.

	initialsize: The initial size of the hashtable. The hashtable
	may grow (double in size) if it starts to get crowded (80%
	full).

	keylen: byte length for each key.

	lock: -1 for no lock, otherwise the FZ_LOCK to use to protect
	this table.

	drop_val: Function to use to destroy values on table drop.
*/
fz_hash_table *fz_new_hash_table(fz_context *ctx, int initialsize, int keylen, int lock, fz_hash_table_drop_fn *drop_val);

/**
	Destroy the hash table.

	Values are dropped using the drop function.
*/
void fz_drop_hash_table(fz_context *ctx, fz_hash_table *table);

/**
	Search for a matching hash within the table, and return the
	associated value.
*/
void *fz_hash_find(fz_context *ctx, fz_hash_table *table, const void *key);

/**
	Insert a new key/value pair into the hash table.

	If an existing entry with the same key is found, no change is
	made to the hash table, and a pointer to the existing value is
	returned.

	If no existing entry with the same key is found, ownership of
	val passes in, key is copied, and NULL is returned.
*/
void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, const void *key, void *val);

/**
	Remove the entry for a given key.

	The value is NOT freed, so the caller is expected to take care
	of this.
*/
void fz_hash_remove(fz_context *ctx, fz_hash_table *table, const void *key);

/**
	Callback function called on each key/value pair in the hash
	table, when fz_hash_for_each is run.
*/
typedef void (fz_hash_table_for_each_fn)(fz_context *ctx, void *state, void *key, int keylen, void *val);

/**
	Iterate over the entries in a hash table.
*/
void fz_hash_for_each(fz_context *ctx, fz_hash_table *table, void *state, fz_hash_table_for_each_fn *callback);

/**
	Callback function called on each key/value pair in the hash
	table, when fz_hash_filter is run to remove entries where the
	callback returns true.
*/
typedef int (fz_hash_table_filter_fn)(fz_context *ctx, void *state, void *key, int keylen, void *val);

/**
	Iterate over the entries in a hash table, removing all the ones where callback returns true.
	Does NOT free the value of the entry, so the caller is expected to take care of this.
*/
void fz_hash_filter(fz_context *ctx, fz_hash_table *table, void *state, fz_hash_table_filter_fn *callback);

#endif
