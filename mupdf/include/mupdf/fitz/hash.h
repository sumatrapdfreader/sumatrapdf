#ifndef MUPDF_FITZ_HASH_H
#define MUPDF_FITZ_HASH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"

/*
 * Generic hash-table with fixed-length keys.
 *
 * The keys and values are NOT reference counted by the hash table.
 * Callers are responsible for taking care the reference counts are correct.
 * Inserting a duplicate entry will NOT overwrite the old value, and will
 * return the old value.
 *
 * The drop_val callback function is only used to release values when the hash table
 * is destroyed.
 */

typedef struct fz_hash_table_s fz_hash_table;
typedef void (fz_hash_table_drop_fn)(fz_context *ctx, void *val);
typedef void (fz_hash_table_for_each_fn)(fz_context *ctx, void *state, void *key, int keylen, void *val);

fz_hash_table *fz_new_hash_table(fz_context *ctx, int initialsize, int keylen, int lock, fz_hash_table_drop_fn *drop_val);
void fz_drop_hash_table(fz_context *ctx, fz_hash_table *table);

void *fz_hash_find(fz_context *ctx, fz_hash_table *table, const void *key);
void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, const void *key, void *val);
void fz_hash_remove(fz_context *ctx, fz_hash_table *table, const void *key);
void fz_hash_for_each(fz_context *ctx, fz_hash_table *table, void *state, fz_hash_table_for_each_fn *callback);

#endif
