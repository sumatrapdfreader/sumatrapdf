#include "mupdf/fitz.h"
#include "draw-imp.h"
#include "fitz-imp.h"

#include <string.h>
#include <math.h>

#define MAX_GLYPH_SIZE 256
#define MAX_CACHE_SIZE (1024*1024)

#define GLYPH_HASH_LEN 509

typedef struct fz_glyph_cache_entry_s fz_glyph_cache_entry;
typedef struct fz_glyph_key_s fz_glyph_key;

struct fz_glyph_key_s
{
	fz_font *font;
	int a, b;
	int c, d;
	unsigned short gid;
	unsigned char e, f;
	int aa;
};

struct fz_glyph_cache_entry_s
{
	fz_glyph_key key;
	unsigned hash;
	fz_glyph_cache_entry *lru_prev;
	fz_glyph_cache_entry *lru_next;
	fz_glyph_cache_entry *bucket_next;
	fz_glyph_cache_entry *bucket_prev;
	fz_glyph *val;
};

struct fz_glyph_cache_s
{
	int refs;
	size_t total;
#ifndef NDEBUG
	int num_evictions;
	ptrdiff_t evicted;
#endif
	fz_glyph_cache_entry *entry[GLYPH_HASH_LEN];
	fz_glyph_cache_entry *lru_head;
	fz_glyph_cache_entry *lru_tail;
};

void
fz_new_glyph_cache_context(fz_context *ctx)
{
	fz_glyph_cache *cache;

	cache = fz_malloc_struct(ctx, fz_glyph_cache);
	cache->total = 0;
	cache->refs = 1;

	ctx->glyph_cache = cache;
}

static void
drop_glyph_cache_entry(fz_context *ctx, fz_glyph_cache_entry *entry)
{
	fz_glyph_cache *cache = ctx->glyph_cache;

	if (entry->lru_next)
		entry->lru_next->lru_prev = entry->lru_prev;
	else
		cache->lru_tail = entry->lru_prev;
	if (entry->lru_prev)
		entry->lru_prev->lru_next = entry->lru_next;
	else
		cache->lru_head = entry->lru_next;
	cache->total -= fz_glyph_size(ctx, entry->val);
	if (entry->bucket_next)
		entry->bucket_next->bucket_prev = entry->bucket_prev;
	if (entry->bucket_prev)
		entry->bucket_prev->bucket_next = entry->bucket_next;
	else
		cache->entry[entry->hash] = entry->bucket_next;
	fz_drop_font(ctx, entry->key.font);
	fz_drop_glyph(ctx, entry->val);
	fz_free(ctx, entry);
}

/* The glyph cache lock is always held when this function is called. */
static void
do_purge(fz_context *ctx)
{
	fz_glyph_cache *cache = ctx->glyph_cache;
	int i;

	for (i = 0; i < GLYPH_HASH_LEN; i++)
	{
		while (cache->entry[i])
			drop_glyph_cache_entry(ctx, cache->entry[i]);
	}

	cache->total = 0;
}

void
fz_purge_glyph_cache(fz_context *ctx)
{
	fz_lock(ctx, FZ_LOCK_GLYPHCACHE);
	do_purge(ctx);
	fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
}

void
fz_drop_glyph_cache_context(fz_context *ctx)
{
	if (!ctx || !ctx->glyph_cache)
		return;

	fz_lock(ctx, FZ_LOCK_GLYPHCACHE);
	ctx->glyph_cache->refs--;
	if (ctx->glyph_cache->refs == 0)
	{
		do_purge(ctx);
		fz_free(ctx, ctx->glyph_cache);
		ctx->glyph_cache = NULL;
	}
	fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
}

fz_glyph_cache *
fz_keep_glyph_cache(fz_context *ctx)
{
	fz_lock(ctx, FZ_LOCK_GLYPHCACHE);
	ctx->glyph_cache->refs++;
	fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
	return ctx->glyph_cache;
}

float
fz_subpixel_adjust(fz_context *ctx, fz_matrix *ctm, fz_matrix *subpix_ctm, unsigned char *qe, unsigned char *qf)
{
	float size = fz_matrix_expansion(*ctm);
	int q;
	float pix_e, pix_f, r;

	/* Quantise the subpixel positions. */
	/* We never need more than 4 subpixel positions for glyphs - arguably
	 * even that is too much. */
	if (size >= 48)
		q = 0, r = 0.5f;
	else if (size >= 24)
		q = 128, r = 0.25f;
	else
		q = 192, r = 0.125f;

	/* Split translation into pixel and subpixel parts */
	subpix_ctm->a = ctm->a;
	subpix_ctm->b = ctm->b;
	subpix_ctm->c = ctm->c;
	subpix_ctm->d = ctm->d;
	subpix_ctm->e = ctm->e + r;
	pix_e = floorf(subpix_ctm->e);
	subpix_ctm->e -= pix_e;
	subpix_ctm->f = ctm->f + r;
	pix_f = floorf(subpix_ctm->f);
	subpix_ctm->f -= pix_f;

	/* Quantise the subpixel part */
	*qe = (int)(subpix_ctm->e * 256) & q;
	subpix_ctm->e = *qe / 256.0f;
	*qf = (int)(subpix_ctm->f * 256) & q;
	subpix_ctm->f = *qf / 256.0f;

	/* Reassemble the complete translation */
	ctm->e = subpix_ctm->e + pix_e;
	ctm->f = subpix_ctm->f + pix_f;

	return size;
}

fz_glyph *
fz_render_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix *trm, fz_matrix ctm, const fz_stroke_state *stroke, const fz_irect *scissor, int aa)
{
	if (fz_font_ft_face(ctx, font))
	{
		fz_matrix subpix_trm;
		unsigned char qe, qf;

		if (stroke->dash_len > 0)
			return NULL;
		(void)fz_subpixel_adjust(ctx, trm, &subpix_trm, &qe, &qf);
		return fz_render_ft_stroked_glyph(ctx, font, gid, subpix_trm, ctm, stroke, aa);
	}
	return fz_render_glyph(ctx, font, gid, trm, NULL, scissor, 1, aa);
}

fz_pixmap *
fz_render_stroked_glyph_pixmap(fz_context *ctx, fz_font *font, int gid, fz_matrix *trm, fz_matrix ctm, const fz_stroke_state *stroke, const fz_irect *scissor, int aa)
{
	if (fz_font_ft_face(ctx, font))
	{
		fz_matrix subpix_trm;
		unsigned char qe, qf;

		if (stroke->dash_len > 0)
			return NULL;
		(void)fz_subpixel_adjust(ctx, trm, &subpix_trm, &qe, &qf);
		return fz_render_ft_stroked_glyph_pixmap(ctx, font, gid, subpix_trm, ctm, stroke, aa);
	}
	return fz_render_glyph_pixmap(ctx, font, gid, trm, scissor, aa);
}

static unsigned do_hash(unsigned char *s, int len)
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

static inline void
move_to_front(fz_glyph_cache *cache, fz_glyph_cache_entry *entry)
{
	if (entry->lru_prev == NULL)
		return; /* At front already */

	/* Unlink */
	entry->lru_prev->lru_next = entry->lru_next;
	if (entry->lru_next)
		entry->lru_next->lru_prev = entry->lru_prev;
	else
		cache->lru_tail = entry->lru_prev;
	/* Relink */
	entry->lru_next = cache->lru_head;
	if (entry->lru_next)
		entry->lru_next->lru_prev = entry;
	cache->lru_head = entry;
	entry->lru_prev = NULL;
}

fz_glyph *
fz_render_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix *ctm, fz_colorspace *model, const fz_irect *scissor, int alpha, int aa)
{
	fz_glyph_cache *cache;
	fz_glyph_key key;
	fz_matrix subpix_ctm;
	fz_irect subpix_scissor;
	float size;
	fz_glyph *val;
	int do_cache, locked, caching;
	fz_glyph_cache_entry *entry;
	unsigned hash;
	int is_ft_font = !!fz_font_ft_face(ctx, font);

	fz_var(locked);
	fz_var(caching);
	fz_var(val);

	memset(&key, 0, sizeof key);
	size = fz_subpixel_adjust(ctx, ctm, &subpix_ctm, &key.e, &key.f);
	if (size <= MAX_GLYPH_SIZE)
	{
		scissor = &fz_infinite_irect;
		do_cache = 1;
	}
	else
	{
		if (is_ft_font)
			return NULL;
		subpix_scissor.x0 = scissor->x0 - floorf(ctm->e);
		subpix_scissor.y0 = scissor->y0 - floorf(ctm->f);
		subpix_scissor.x1 = scissor->x1 - floorf(ctm->e);
		subpix_scissor.y1 = scissor->y1 - floorf(ctm->f);
		scissor = &subpix_scissor;
		do_cache = 0;
	}

	cache = ctx->glyph_cache;

	key.font = font;
	key.gid = gid;
	key.a = subpix_ctm.a * 65536;
	key.b = subpix_ctm.b * 65536;
	key.c = subpix_ctm.c * 65536;
	key.d = subpix_ctm.d * 65536;
	key.aa = aa;

	hash = do_hash((unsigned char *)&key, sizeof(key)) % GLYPH_HASH_LEN;
	fz_lock(ctx, FZ_LOCK_GLYPHCACHE);
	entry = cache->entry[hash];
	while (entry)
	{
		if (memcmp(&entry->key, &key, sizeof(key)) == 0)
		{
			move_to_front(cache, entry);
			val = fz_keep_glyph(ctx, entry->val);
			fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
			return val;
		}
		entry = entry->bucket_next;
	}

	locked = 1;
	caching = 0;
	val = NULL;

	fz_try(ctx)
	{
		if (is_ft_font)
		{
			val = fz_render_ft_glyph(ctx, font, gid, subpix_ctm, aa);
		}
		else if (fz_font_t3_procs(ctx, font))
		{
			/* We drop the glyphcache here, and execute the t3
			 * glyph code. The danger here is that some other
			 * thread will come along, and want the same glyph
			 * too. If it does, we may both end up rendering
			 * pixmaps. We cope with this later on, by ensuring
			 * that only one gets inserted into the cache. If
			 * we insert ours to find one already there, we
			 * abandon ours, and use the one there already.
			 */
			fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
			locked = 0;
			val = fz_render_t3_glyph(ctx, font, gid, subpix_ctm, model, scissor, aa);
			fz_lock(ctx, FZ_LOCK_GLYPHCACHE);
			locked = 1;
		}
		else
		{
			fz_warn(ctx, "assert: uninitialized font structure");
		}
		if (val && do_cache)
		{
			if (val->w < MAX_GLYPH_SIZE && val->h < MAX_GLYPH_SIZE)
			{
				/* If we throw an exception whilst caching,
				 * just ignore the exception and carry on. */
				caching = 1;
				if (!is_ft_font)
				{
					/* We had to unlock. Someone else might
					 * have rendered in the meantime */
					entry = cache->entry[hash];
					while (entry)
					{
						if (memcmp(&entry->key, &key, sizeof(key)) == 0)
						{
							fz_drop_glyph(ctx, val);
							move_to_front(cache, entry);
							val = fz_keep_glyph(ctx, entry->val);
							goto unlock_and_return_val;
						}
						entry = entry->bucket_next;
					}
				}

				entry = fz_malloc_struct(ctx, fz_glyph_cache_entry);
				entry->key = key;
				entry->hash = hash;
				entry->bucket_next = cache->entry[hash];
				if (entry->bucket_next)
					entry->bucket_next->bucket_prev = entry;
				cache->entry[hash] = entry;
				entry->val = fz_keep_glyph(ctx, val);
				fz_keep_font(ctx, key.font);

				entry->lru_next = cache->lru_head;
				if (entry->lru_next)
					entry->lru_next->lru_prev = entry;
				else
					cache->lru_tail = entry;
				cache->lru_head = entry;

				cache->total += fz_glyph_size(ctx, val);
				while (cache->total > MAX_CACHE_SIZE)
				{
#ifndef NDEBUG
					cache->num_evictions++;
					cache->evicted += fz_glyph_size(ctx, cache->lru_tail->val);
#endif
					drop_glyph_cache_entry(ctx, cache->lru_tail);
				}
			}
		}
unlock_and_return_val:
		{
		}
	}
	fz_always(ctx)
	{
		if (locked)
			fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
	}
	fz_catch(ctx)
	{
		if (caching)
			fz_warn(ctx, "cannot encache glyph; continuing");
		else
			fz_rethrow(ctx);
	}

	return val;
}

fz_pixmap *
fz_render_glyph_pixmap(fz_context *ctx, fz_font *font, int gid, fz_matrix *ctm, const fz_irect *scissor, int aa)
{
	fz_pixmap *val = NULL;
	unsigned char qe, qf;
	fz_matrix subpix_ctm;
	float size = fz_subpixel_adjust(ctx, ctm, &subpix_ctm, &qe, &qf);
	int is_ft_font = !!fz_font_ft_face(ctx, font);

	if (size <= MAX_GLYPH_SIZE)
	{
		scissor = &fz_infinite_irect;
	}
	else
	{
		if (is_ft_font)
			return NULL;
	}

	if (is_ft_font)
	{
		val = fz_render_ft_glyph_pixmap(ctx, font, gid, subpix_ctm, aa);
	}
	else if (fz_font_t3_procs(ctx, font))
	{
		val = fz_render_t3_glyph_pixmap(ctx, font, gid, subpix_ctm, NULL, scissor, aa);
	}
	else
	{
		fz_warn(ctx, "assert: uninitialized font structure");
		val = NULL;
	}

	return val;
}

void
fz_dump_glyph_cache_stats(fz_context *ctx)
{
	fz_glyph_cache *cache = ctx->glyph_cache;
	fz_write_printf(ctx, fz_stderr(ctx), "Glyph Cache Size: %zu\n", cache->total);
#ifndef NDEBUG
	fz_write_printf(ctx, fz_stderr(ctx), "Glyph Cache Evictions: %d (%zu bytes)\n", cache->num_evictions, cache->evicted);
#endif
}
