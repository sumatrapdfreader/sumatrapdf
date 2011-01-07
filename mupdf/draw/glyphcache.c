#include "fitz.h"

#define MAXFONTSIZE 1000
#define MAXGLYPHSIZE 256
#define MAXCACHESIZE (1024*1024)

typedef struct fz_glyphkey_s fz_glyphkey;

struct fz_glyphcache_s
{
	fz_hashtable *hash;
	int total;
};

struct fz_glyphkey_s
{
	fz_font *font;
	int a, b;
	int c, d;
	unsigned short cid;
	unsigned char e, f;
};

fz_glyphcache *
fz_newglyphcache(void)
{
	fz_glyphcache *cache;

	cache = fz_malloc(sizeof(fz_glyphcache));
	cache->hash = fz_newhash(509, sizeof(fz_glyphkey));
	cache->total = 0;

	return cache;
}

static void
fz_evictglyphcache(fz_glyphcache *cache)
{
	fz_glyphkey *key;
	fz_pixmap *pixmap;
	int i;

	for (i = 0; i < fz_hashlen(cache->hash); i++)
	{
		key = fz_hashgetkey(cache->hash, i);
		if (key->font)
			fz_dropfont(key->font);
		pixmap = fz_hashgetval(cache->hash, i);
		if (pixmap)
			fz_droppixmap(pixmap);
	}

	cache->total = 0;

	fz_emptyhash(cache->hash);
}

void
fz_freeglyphcache(fz_glyphcache *cache)
{
	fz_evictglyphcache(cache);
	fz_freehash(cache->hash);
	fz_free(cache);
}

fz_pixmap *
fz_renderstrokedglyph(fz_glyphcache *cache, fz_font *font, int cid, fz_matrix trm, fz_matrix ctm, fz_strokestate *stroke)
{
	if (font->ftface)
		return fz_renderftstrokedglyph(font, cid, trm, ctm, stroke);
	return fz_renderglyph(cache, font, cid, trm);
}

fz_pixmap *
fz_renderglyph(fz_glyphcache *cache, fz_font *font, int cid, fz_matrix ctm)
{
	fz_glyphkey key;
	fz_pixmap *val;
	float size = fz_matrixexpansion(ctm);

	if (size > MAXFONTSIZE)
	{
		/* TODO: this case should be handled by rendering glyph as a path fill */
		fz_warn("font size too large (%g), not rendering glyph", size);
		return nil;
	}

	memset(&key, 0, sizeof key);
	key.font = font;
	key.cid = cid;
	key.a = ctm.a * 65536;
	key.b = ctm.b * 65536;
	key.c = ctm.c * 65536;
	key.d = ctm.d * 65536;
	key.e = (ctm.e - floorf(ctm.e)) * 256;
	key.f = (ctm.f - floorf(ctm.f)) * 256;

	val = fz_hashfind(cache->hash, &key);
	if (val)
		return fz_keeppixmap(val);

	ctm.e = floorf(ctm.e) + key.e / 256.0f;
	ctm.f = floorf(ctm.f) + key.f / 256.0f;

	if (font->ftface)
	{
		val = fz_renderftglyph(font, cid, ctm);
	}
	else if (font->t3procs)
	{
		val = fz_rendert3glyph(font, cid, ctm);
	}
	else
	{
		fz_warn("assert: uninitialized font structure");
		return nil;
	}

	if (val)
	{
		if (val->w < MAXGLYPHSIZE && val->h < MAXGLYPHSIZE)
		{
			if (cache->total + val->w * val->h > MAXCACHESIZE)
				fz_evictglyphcache(cache);
			fz_keepfont(key.font);
			fz_hashinsert(cache->hash, &key, val);
			cache->total += val->w * val->h;
			return fz_keeppixmap(val);
		}
		return val;
	}

	return nil;
}
