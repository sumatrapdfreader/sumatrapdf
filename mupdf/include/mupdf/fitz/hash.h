#ifndef MUPDF_FITZ_HASH_H
#define MUPDF_FITZ_HASH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"

/*
	Generic hash-table with fixed-length keys.

	The keys and values are NOT reference counted by the hash table.
	Callers are responsible for taking care the reference counts are
	correct. Inserting a duplicate entry will NOT overwrite the old
	value, and will return the old value.

	The drop_val callback function is only used to release values
	when the hash table is destroyed.
*/

typedef struct fz_hash_table fz_hash_table;

/*
	Function type called when a hash table entry is dropped.

	Only used when the entire hash table is dropped.
*/
typedef void (fz_hash_table_drop_fn)(fz_context *ctx, void *val);

/*
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

/*
	Destroy the hash table.

	Values are dropped using the drop function.
*/
void fz_drop_hash_table(fz_context *ctx, fz_hash_table *table);

/*
	Search for a matching hash within the table, and return the
	associated value.
*/
void *fz_hash_find(fz_context *ctx, fz_hash_table *table, const void *key);

/*
	Insert a new key/value pair into the hash table.

	If an existing entry with the same key is found, no change is
	made to the hash table, and a pointer to the existing value is
	returned.

	If no existing entry with the same key is found, ownership of
	val passes in, key is copied, and NULL is returned.
*/
void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, const void *key, void *val);

/*
	Remove the entry for a given key.

	The value is NOT freed, so the caller is expected to take care
	of this.
*/
void fz_hash_remove(fz_context *ctx, fz_hash_table *table, const void *key);

/*
	Callback function called on each key/value pair in the hash
	table, when fz_hash_for_each is run.
*/
typedef void (fz_hash_table_for_each_fn)(fz_context *ctx, void *state, void *key, int keylen, void *val);

/*
	Iterate over the entries in a hash table.
*/
void fz_hash_for_each(fz_context *ctx, fz_hash_table *table, void *state, fz_hash_table_for_each_fn *callback);

#endif
