#include "fitz.h"

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

void
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
	fz_drophash(cache->hash);
	fz_free(cache);
}

fz_pixmap *
fz_renderglyph(fz_glyphcache *cache, fz_font *font, int cid, fz_matrix ctm)
{
	fz_glyphkey key;
	fz_pixmap *val;

	key.font = font;
	key.cid = cid;
	key.a = ctm.a * 65536;
	key.b = ctm.b * 65536;
	key.c = ctm.c * 65536;
	key.d = ctm.d * 65536;
	key.e = (ctm.e - floor(ctm.e)) * 256;
	key.f = (ctm.f - floor(ctm.f)) * 256;

	val = fz_hashfind(cache->hash, &key);
	if (val)
		return fz_keeppixmap(val);

	ctm.e = floor(ctm.e) + key.e / 256.0;
	ctm.f = floor(ctm.f) + key.f / 256.0;

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
		return NULL;
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

	return NULL;
}
