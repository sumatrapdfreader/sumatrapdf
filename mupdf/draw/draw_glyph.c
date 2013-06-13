#include "fitz-internal.h"

#define MAX_GLYPH_SIZE 256
#define MAX_CACHE_SIZE (1024*1024)

typedef struct fz_glyph_key_s fz_glyph_key;

struct fz_glyph_cache_s
{
	int refs;
	fz_hash_table *hash;
	int total;
};

struct fz_glyph_key_s
{
	fz_font *font;
	int a, b;
	int c, d;
	unsigned short gid;
	unsigned char e, f;
	int aa;
};

void
fz_new_glyph_cache_context(fz_context *ctx)
{
	fz_glyph_cache *cache;

	cache = fz_malloc_struct(ctx, fz_glyph_cache);
	fz_try(ctx)
	{
		cache->hash = fz_new_hash_table(ctx, 509, sizeof(fz_glyph_key), FZ_LOCK_GLYPHCACHE);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, cache);
		fz_rethrow(ctx);
	}
	cache->total = 0;
	cache->refs = 1;

	ctx->glyph_cache = cache;
}

/* The glyph cache lock is always held when this function is called. */
static void
fz_evict_glyph_cache(fz_context *ctx)
{
	fz_glyph_cache *cache = ctx->glyph_cache;
	fz_glyph_key *key;
	fz_pixmap *pixmap;
	int i;

	for (i = 0; i < fz_hash_len(ctx, cache->hash); i++)
	{
		key = fz_hash_get_key(ctx, cache->hash, i);
		if (key->font)
			fz_drop_font(ctx, key->font);
		pixmap = fz_hash_get_val(ctx, cache->hash, i);
		if (pixmap)
			fz_drop_pixmap(ctx, pixmap);
	}

	cache->total = 0;

	fz_empty_hash(ctx, cache->hash);
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
		fz_free_hash(ctx, ctx->glyph_cache->hash);
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
	val = fz_hash_find(ctx, cache->hash, &key);
	if (val)
	{
		fz_keep_pixmap(ctx, val);
		fz_unlock(ctx, FZ_LOCK_GLYPHCACHE);
		return val;
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
				fz_pixmap *pix;

				/* If we throw an exception whilst caching,
				 * just ignore the exception and carry on. */
				caching = 1;
				if (cache->total + val->w * val->h > MAX_CACHE_SIZE)
					fz_evict_glyph_cache(ctx);

				pix = fz_hash_insert(ctx, cache->hash, &key, val);
				if (pix)
				{
					fz_drop_pixmap(ctx, val);
					val = pix;
				}
				else
				{
					fz_keep_font(ctx, key.font);
					cache->total += val->w * val->h;
				}
				val = fz_keep_pixmap(ctx, val);
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
