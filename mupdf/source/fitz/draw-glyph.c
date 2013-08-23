#include "mupdf/fitz.h"
#include "draw-imp.h"

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
	fz_pixmap *val;
};

struct fz_glyph_cache_s
{
	int refs;
	int total;
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
	cache->total -= entry->val->w * entry->val->h;
	if (entry->bucket_next)
		entry->bucket_next->bucket_prev = entry->bucket_prev;
	if (entry->bucket_prev)
		entry->bucket_prev->bucket_next = entry->bucket_next;
	else
		cache->entry[entry->hash] = entry->bucket_next;
	fz_drop_font(ctx, entry->key.font);
	fz_drop_pixmap(ctx, entry->val);
	fz_free(ctx, entry);
}

/* The glyph cache lock is always held when this function is called. */
static void
fz_evict_glyph_cache(fz_context *ctx)
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
	fz_evict_glyph_cache(ctx);
	fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
}

void
fz_drop_glyph_cache_context(fz_context *ctx)
{
	if (!ctx->glyph_cache)
		return;

	fz_lock(ctx, FZ_LOCK_GLYPHCACHE);
	ctx->glyph_cache->refs--;
	if (ctx->glyph_cache->refs == 0)
	{
		fz_evict_glyph_cache(ctx);
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

fz_pixmap *
fz_render_stroked_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *trm, const fz_matrix *ctm, fz_stroke_state *stroke, fz_irect scissor)
{
	if (font->ft_face)
	{
		if (stroke->dash_len > 0)
			return NULL;
		return fz_render_ft_stroked_glyph(ctx, font, gid, trm, ctm, stroke);
	}
	return fz_render_glyph(ctx, font, gid, trm, NULL, scissor);
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

/*
	Render a glyph and return a bitmap.
	If the glyph is too large to fit the cache we have two choices:
	1) Return NULL so the caller can draw the glyph using an outline.
		Only supported for freetype fonts.
	2) Render a clipped glyph by using the scissor rectangle.
		Only supported for type 3 fonts.
		This must not be inserted into the cache.
 */
fz_pixmap *
fz_render_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *ctm, fz_colorspace *model, fz_irect scissor)
{
	fz_glyph_cache *cache;
	fz_glyph_key key;
	fz_pixmap *val;
	float size = fz_matrix_expansion(ctm);
	int do_cache, locked, caching;
	fz_matrix local_ctm = *ctm;
	fz_glyph_cache_entry *entry;
	unsigned hash;

	fz_var(locked);
	fz_var(caching);

	if (size <= MAX_GLYPH_SIZE)
	{
		scissor = fz_infinite_irect;
		do_cache = 1;
	}
	else
	{
		/* SumatraPDF: don't break clipping by larger glyphs */
		if (font->ft_face && size > 3000)
			return NULL;
		do_cache = 0;
	}

	cache = ctx->glyph_cache;

	memset(&key, 0, sizeof key);
	key.font = font;
	key.gid = gid;
	key.a = local_ctm.a * 65536;
	key.b = local_ctm.b * 65536;
	key.c = local_ctm.c * 65536;
	key.d = local_ctm.d * 65536;
	key.e = (local_ctm.e - floorf(local_ctm.e)) * 256;
	key.f = (local_ctm.f - floorf(local_ctm.f)) * 256;
	key.aa = fz_aa_level(ctx);

	local_ctm.e = floorf(local_ctm.e) + key.e / 256.0f;
	local_ctm.f = floorf(local_ctm.f) + key.f / 256.0f;

	fz_lock(ctx, FZ_LOCK_GLYPHCACHE);
	hash = do_hash((unsigned char *)&key, sizeof(key)) % GLYPH_HASH_LEN;
	entry = cache->entry[hash];
	while (entry)
	{
		if (memcmp(&entry->key, &key, sizeof(key)) == 0)
		{
			move_to_front(cache, entry);
			val = fz_keep_pixmap(ctx, entry->val);
			fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
			return val;
		}
		entry = entry->bucket_next;
	}

	locked = 1;
	caching = 0;

	fz_try(ctx)
	{
		if (font->ft_face)
		{
			val = fz_render_ft_glyph(ctx, font, gid, &local_ctm, key.aa);
		}
		else if (font->t3procs)
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
			val = fz_render_t3_glyph(ctx, font, gid, &local_ctm, model, scissor);
			fz_lock(ctx, FZ_LOCK_GLYPHCACHE);
			locked = 1;
		}
		else
		{
			fz_warn(ctx, "assert: uninitialized font structure");
			val = NULL;
		}
		if (val && do_cache)
		{
			if (val->w < MAX_GLYPH_SIZE && val->h < MAX_GLYPH_SIZE)
			{
				/* If we throw an exception whilst caching,
				 * just ignore the exception and carry on. */
				caching = 1;
				if (!font->ft_face)
				{
					/* We had to unlock. Someone else might
					 * have rendered in the meantime */
					entry = cache->entry[hash];
					while (entry)
					{
						if (memcmp(&entry->key, &key, sizeof(key)) == 0)
						{
							fz_drop_pixmap(ctx, val);
							move_to_front(cache, entry);
							val = fz_keep_pixmap(ctx, entry->val);
							fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
							return val;
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
				entry->val = fz_keep_pixmap(ctx, val);
				fz_keep_font(ctx, key.font);

				entry->lru_next = cache->lru_head;
				if (entry->lru_next)
					entry->lru_next->lru_prev = entry;
				else
					cache->lru_tail = entry;
				cache->lru_head = entry;

				cache->total += val->w * val->h;
				while (cache->total > MAX_CACHE_SIZE)
				{
					drop_glyph_cache_entry(ctx, cache->lru_tail);
				}

			}
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
