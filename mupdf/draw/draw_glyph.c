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

fz_glyph_cache *
fz_new_glyph_cache(void)
{
	fz_glyph_cache *cache;

	cache = fz_malloc(sizeof(fz_glyph_cache));
	cache->hash = fz_new_hash_table(509, sizeof(fz_glyph_key));
	cache->total = 0;

	return cache;
}

static void
fz_evict_glyph_cache(fz_glyph_cache *cache)
{
	fz_glyph_key *key;
	fz_pixmap *pixmap;
	int i;

	for (i = 0; i < fz_hash_len(cache->hash); i++)
	{
		key = fz_hash_get_key(cache->hash, i);
		if (key->font)
			fz_drop_font(key->font);
		pixmap = fz_hash_get_val(cache->hash, i);
		if (pixmap)
			fz_drop_pixmap(pixmap);
	}

	cache->total = 0;

	fz_empty_hash(cache->hash);
}

void
fz_free_glyph_cache(fz_glyph_cache *cache)
{
	fz_evict_glyph_cache(cache);
	fz_free_hash(cache->hash);
	fz_free(cache);
}

fz_pixmap *
fz_render_stroked_glyph(fz_glyph_cache *cache, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, fz_stroke_state *stroke)
{
	if (font->ft_face)
		return fz_render_ft_stroked_glyph(font, gid, trm, ctm, stroke);
	return fz_render_glyph(cache, font, gid, trm, NULL);
}

fz_pixmap *
fz_render_glyph(fz_glyph_cache *cache, fz_font *font, int gid, fz_matrix ctm, fz_colorspace *model)
{
	fz_glyph_key key;
	fz_pixmap *val;
	float size = fz_matrix_expansion(ctm);

	if (size > MAX_FONT_SIZE)
	{
		/* TODO: this case should be handled by rendering glyph as a path fill */
		fz_warn("font size too large (%g), not rendering glyph", size);
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

	val = fz_hash_find(cache->hash, &key);
	if (val)
		return fz_keep_pixmap(val);

	ctm.e = floorf(ctm.e) + key.e / 256.0f;
	ctm.f = floorf(ctm.f) + key.f / 256.0f;

	if (font->ft_face)
	{
		val = fz_render_ft_glyph(font, gid, ctm);
	}
	else if (font->t3procs)
	{
		val = fz_render_t3_glyph(font, gid, ctm, model);
	}
	else
	{
		fz_warn("assert: uninitialized font structure");
		return NULL;
	}

	if (val)
	{
		if (val->w < MAX_GLYPH_SIZE && val->h < MAX_GLYPH_SIZE)
		{
			if (cache->total + val->w * val->h > MAX_CACHE_SIZE)
				fz_evict_glyph_cache(cache);
			fz_keep_font(key.font);
			fz_hash_insert(cache->hash, &key, val);
			cache->total += val->w * val->h;
			return fz_keep_pixmap(val);
		}
		return val;
	}

	return NULL;
}
