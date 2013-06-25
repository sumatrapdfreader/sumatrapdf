#ifndef MUPDF_FITZ_HASH_H
#define MUPDF_FITZ_HASH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/*
 * Generic hash-table with fixed-length keys.
 */

typedef struct fz_hash_table_s fz_hash_table;

fz_hash_table *fz_new_hash_table(fz_context *ctx, int initialsize, int keylen, int lock);
void fz_empty_hash(fz_context *ctx, fz_hash_table *table);
void fz_free_hash(fz_context *ctx, fz_hash_table *table);

void *fz_hash_find(fz_context *ctx, fz_hash_table *table, void *key);
void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, void *key, void *val);
void *fz_hash_insert_with_pos(fz_context *ctx, fz_hash_table *table, void *key, void *val, unsigned *pos);
void fz_hash_remove(fz_context *ctx, fz_hash_table *table, void *key);
void fz_hash_remove_fast(fz_context *ctx, fz_hash_table *table, void *key, unsigned pos);

int fz_hash_len(fz_context *ctx, fz_hash_table *table);
void *fz_hash_get_key(fz_context *ctx, fz_hash_table *table, int idx);
void *fz_hash_get_val(fz_context *ctx, fz_hash_table *table, int idx);

#ifndef NDEBUG
void fz_print_hash(fz_context *ctx, FILE *out, fz_hash_table *table);
void fz_print_hash_details(fz_context *ctx, FILE *out, fz_hash_table *table, void (*details)(FILE *, void *));
#endif

#endif
