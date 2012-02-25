#include "fitz.h"
#include "mupdf.h"

static int
pdf_make_hash_key(fz_store_hash *hash, void *key_)
{
	fz_obj *key = (fz_obj *)key_;

	if (!fz_is_indirect(key))
		return 0;
	hash->u.i.i0 = fz_to_num(key);
	hash->u.i.i1 = fz_to_gen(key);
	return 1;
}

static void *
pdf_keep_key(fz_context *ctx, void *key)
{
	return (void *)fz_keep_obj((fz_obj *)key);
}

static void
pdf_drop_key(fz_context *ctx, void *key)
{
	fz_drop_obj((fz_obj *)key);
}

static int
pdf_cmp_key(void *k0, void *k1)
{
	return fz_objcmp((fz_obj *)k0, (fz_obj *)k1);
}

static fz_store_type pdf_obj_store_type =
{
	pdf_make_hash_key,
	pdf_keep_key,
	pdf_drop_key,
	pdf_cmp_key
};

void
pdf_store_item(fz_context *ctx, fz_obj *key, void *val, unsigned int itemsize)
{
	fz_store_item(ctx, key, val, itemsize, &pdf_obj_store_type);
}

void *
pdf_find_item(fz_context *ctx, fz_store_free_fn *free, fz_obj *key)
{
	return fz_find_item(ctx, free, key, &pdf_obj_store_type);
}

void
pdf_remove_item(fz_context *ctx, fz_store_free_fn *free, fz_obj *key)
{
	fz_remove_item(ctx, free, key, &pdf_obj_store_type);
}

