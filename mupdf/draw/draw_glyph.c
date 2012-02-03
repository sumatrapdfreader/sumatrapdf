#include "fitz.h"

/* SumatraPDF: changed max font size from 1000 to 3000 */
#define MAX_FONT_SIZE 3000
#define MAX_GLYPH_SIZE 256
#define MAX_CACHE_SIZE (1024*1024)

typedef struct fz_glyph_key_s fz_glyph_key;

struct fz_glyph_cache_s
{
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
};

void
fz_new_glyph_cache_context(fz_context *ctx)
{
	fz_glyph_cache *cache;

	cache = fz_malloc_struct(ctx, fz_glyph_cache);
	fz_try(ctx)
	{
		cache->hash = fz_new_hash_table(ctx, 509, sizeof(fz_glyph_key));
	}
	fz_catch(ctx)
	{
		fz_free(ctx, cache);
		fz_rethrow(ctx);
	}
	cache->total = 0;

	ctx->glyph_cache = cache;
}

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
fz_free_glyph_cache_context(fz_context *ctx)
{
	if (!ctx->glyph_cache)
		return;

	fz_evict_glyph_cache(ctx);
	fz_free_hash(ctx, ctx->glyph_cache->hash);
	fz_free(ctx, ctx->glyph_cache);
	ctx->glyph_cache = NULL;
}

fz_pixmap *
fz_render_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, fz_stroke_state *stroke)
{
	if (font->ft_face)
		return fz_render_ft_stroked_glyph(ctx, font, gid, trm, ctm, stroke);
	return fz_render_glyph(ctx, font, gid, trm, NULL);
}

fz_pixmap *
fz_render_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix ctm, fz_colorspace *model)
{
	fz_glyph_cache *cache;
	fz_glyph_key key;
	fz_pixmap *val;
	float size = fz_matrix_expansion(ctm);

	if (!ctx->glyph_cache)
		fz_new_glyph_cache_context(ctx);
	cache = ctx->glyph_cache;

	if (size > MAX_FONT_SIZE)
	{
		/* TODO: this case should be handled by rendering glyph as a path fill */
		fz_warn(ctx, "font size too large (%g), not rendering glyph", size);
		return NULL;
	}

	memset(&key, 0, sizeof key);
	key.font = font;
	key.gid = gid;
	key.a = ctm.a * 65536;
	key.b = ctm.b * 65536;
	key.c = ctm.c * 65536;
	key.d = ctm.d * 65536;
	key.e = (ctm.e - floorf(ctm.e)) * 256;
	key.f = (ctm.f - floorf(ctm.f)) * 256;

	val = fz_hash_find(ctx, cache->hash, &key);
	if (val)
		return fz_keep_pixmap(ctx, val);

	ctm.e = floorf(ctm.e) + key.e / 256.0f;
	ctm.f = floorf(ctm.f) + key.f / 256.0f;

	if (font->ft_face)
	{
		val = fz_render_ft_glyph(ctx, font, gid, ctm);
	}
	else if (font->t3procs)
	{
		val = fz_render_t3_glyph(ctx, font, gid, ctm, model);
	}
	else
	{
		fz_warn(ctx, "assert: uninitialized font structure");
		return NULL;
	}

	if (val)
	{
		if (val->w < MAX_GLYPH_SIZE && val->h < MAX_GLYPH_SIZE)
		{
			if (cache->total + val->w * val->h > MAX_CACHE_SIZE)
				fz_evict_glyph_cache(ctx);
			fz_try(ctx)
			{
				fz_hash_insert(ctx, cache->hash, &key, val);
				fz_keep_font(ctx, key.font);
				val = fz_keep_pixmap(ctx, val);
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "Failed to encache glyph - continuing");
			}
			cache->total += val->w * val->h;
			return val;
		}
		return val;
	}

	return NULL;
}
