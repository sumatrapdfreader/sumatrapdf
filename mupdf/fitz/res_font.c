#include "fitz.h"

#include <ft2build.h>
#include FT_FREETYPE_H

static void fz_finalizefreetype(void);

static fz_font *
fz_newfont(void)
{
	fz_font *font;

	font = fz_malloc(sizeof(fz_font));
	font->refs = 1;
	strcpy(font->name, "<unknown>");

	font->ftface = nil;
	font->ftsubstitute = 0;
	font->fthint = 0;

	font->t3matrix = fz_identity();
	font->t3resources = nil;
	font->t3procs = nil;
	font->t3widths = nil;
	font->t3xref = nil;
	font->t3runcontentstream = nil;

	font->bbox.x0 = 0;
	font->bbox.y0 = 0;
	font->bbox.x1 = 1000;
	font->bbox.y1 = 1000;

	font->widthcount = 0;
	font->widthtable = nil;

	return font;
}

fz_font *
fz_keepfont(fz_font *font)
{
	font->refs ++;
	return font;
}

void
fz_dropfont(fz_font *font)
{
	int fterr;
	int i;

	if (font && --font->refs == 0)
	{
		if (font->t3procs)
		{
			if (font->t3resources)
				fz_dropobj(font->t3resources);
			for (i = 0; i < 256; i++)
				if (font->t3procs[i])
					fz_dropbuffer(font->t3procs[i]);
			fz_free(font->t3procs);
			fz_free(font->t3widths);
		}

		if (font->ftface)
		{
			fterr = FT_Done_Face((FT_Face)font->ftface);
			if (fterr)
				fz_warn("freetype finalizing face: %s", ft_errorstring(fterr));
			fz_finalizefreetype();
		}

		if (font->widthtable)
			fz_free(font->widthtable);

		fz_free(font);
	}
}

void
fz_setfontbbox(fz_font *font, float xmin, float ymin, float xmax, float ymax)
{
	font->bbox.x0 = xmin;
	font->bbox.y0 = ymin;
	font->bbox.x1 = xmax;
	font->bbox.y1 = ymax;
}

/*
 * Freetype hooks
 */

static FT_Library fz_ftlib = nil;
static int fz_ftlib_refs = 0;

#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s)	{ (e), (s) },
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST	{ 0, NULL }

struct ft_error
{
	int err;
	char *str;
};

static const struct ft_error ft_errors[] =
{
#include FT_ERRORS_H
};

char *ft_errorstring(int err)
{
	const struct ft_error *e;

	for (e = ft_errors; e->str != NULL; e++)
		if (e->err == err)
			return e->str;

	return "Unknown error";
}

static fz_error
fz_initfreetype(void)
{
	int fterr;
	int maj, min, pat;

	if (fz_ftlib)
	{
		fz_ftlib_refs++;
		return fz_okay;
	}

	fterr = FT_Init_FreeType(&fz_ftlib);
	if (fterr)
		return fz_throw("cannot init freetype: %s", ft_errorstring(fterr));

	FT_Library_Version(fz_ftlib, &maj, &min, &pat);
	if (maj == 2 && min == 1 && pat < 7)
	{
		fterr = FT_Done_FreeType(fz_ftlib);
		if (fterr)
			fz_warn("freetype finalizing: %s", ft_errorstring(fterr));
		return fz_throw("freetype version too old: %d.%d.%d", maj, min, pat);
	}

	fz_ftlib_refs++;
	return fz_okay;
}

static void
fz_finalizefreetype(void)
{
	int fterr;

	if (--fz_ftlib_refs == 0)
	{
		fterr = FT_Done_FreeType(fz_ftlib);
		if (fterr)
			fz_warn("freetype finalizing: %s", ft_errorstring(fterr));
		fz_ftlib = nil;
	}
}

fz_error
fz_newfontfromfile(fz_font **fontp, char *path, int index)
{
	fz_error error;
	fz_font *font;
	int fterr;

	error = fz_initfreetype();
	if (error)
		return fz_rethrow(error, "cannot init freetype library");

	font = fz_newfont();

	fterr = FT_New_Face(fz_ftlib, path, index, (FT_Face*)&font->ftface);
	if (fterr)
	{
		fz_free(font);
		return fz_throw("freetype: cannot load font: %s", ft_errorstring(fterr));
	}

	*fontp = font;
	return fz_okay;
}

fz_error
fz_newfontfrombuffer(fz_font **fontp, unsigned char *data, int len, int index)
{
	fz_error error;
	fz_font *font;
	int fterr;

	error = fz_initfreetype();
	if (error)
		return fz_rethrow(error, "cannot init freetype library");

	font = fz_newfont();

	fterr = FT_New_Memory_Face(fz_ftlib, data, len, index, (FT_Face*)&font->ftface);
	if (fterr)
	{
		fz_free(font);
		return fz_throw("freetype: cannot load font: %s", ft_errorstring(fterr));
	}

	*fontp = font;
	return fz_okay;
}

fz_pixmap *
fz_renderftglyph(fz_font *font, int gid, fz_matrix trm)
{
	FT_Face face = font->ftface;
	FT_Matrix m;
	FT_Vector v;
	FT_Error fterr;
	fz_pixmap *glyph;
	int y;

	/* Fudge the font matrix to stretch the glyph if we've substituted the font. */
	if (font->ftsubstitute && gid < font->widthcount)
	{
		int subw;
		int realw;
		float scale;

		/* TODO: use FT_Get_Advance */
		fterr = FT_Set_Char_Size(face, 1000, 1000, 72, 72);
		if (fterr)
			fz_warn("freetype setting character size: %s", ft_errorstring(fterr));

		fterr = FT_Load_Glyph(font->ftface, gid,
			FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
		if (fterr)
			fz_warn("freetype failed to load glyph: %s", ft_errorstring(fterr));

		realw = ((FT_Face)font->ftface)->glyph->advance.x;
		subw = font->widthtable[gid];
		if (realw)
			scale = (float) subw / realw;
		else
			scale = 1.0;

		trm = fz_concat(fz_scale(scale, 1.0), trm);
	}

	/* freetype mutilates complex glyphs if they are loaded
	 * with FT_Set_Char_Size 1.0. it rounds the coordinates
	 * before applying transformation. to get more precision in
	 * freetype, we shift part of the scale in the matrix
	 * into FT_Set_Char_Size instead
	 */

	m.xx = trm.a * 64; /* should be 65536 */
	m.yx = trm.b * 64;
	m.xy = trm.c * 64;
	m.yy = trm.d * 64;
	v.x = trm.e * 64;
	v.y = trm.f * 64;

	fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); /* should be 64, 64 */
	if (fterr)
		fz_warn("freetype setting character size: %s", ft_errorstring(fterr));
	FT_Set_Transform(face, &m, &v);

	if (font->fthint)
	{
		/* Enable hinting, but keep the huge char size so that
		 * it is hinted for a character. This will in effect nullify
		 * the effect of grid fitting. This form of hinting should
		 * only be used for DynaLab and similar tricky TrueType fonts,
		 * so that we get the correct outline shape.
		 */
#ifdef USE_HINTING
		/* If you really want grid fitting, enable this code. */
		float scale = fz_matrixexpansion(trm);
		m.xx = trm.a * 65536 / scale;
		m.xy = trm.b * 65536 / scale;
		m.yx = trm.c * 65536 / scale;
		m.yy = trm.d * 65536 / scale;
		v.x = 0;
		v.y = 0;

		fterr = FT_Set_Char_Size(face, 64 * scale, 64 * scale, 72, 72);
		if (fterr)
			fz_warn("freetype setting character size: %s", ft_errorstring(fterr));
		FT_Set_Transform(face, &m, &v);
#endif
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP);
		if (fterr)
			fz_warn("freetype load glyph (gid %d): %s", gid, ft_errorstring(fterr));
	}
	else
	{
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
		if (fterr)
			fz_warn("freetype load glyph (gid %d): %s", gid, ft_errorstring(fterr));
	}

	fterr = FT_Render_Glyph(face->glyph, ft_render_mode_normal);
	if (fterr)
		fz_warn("freetype render glyph (gid %d): %s", gid, ft_errorstring(fterr));

	glyph = fz_newpixmap(NULL,
		face->glyph->bitmap_left,
		face->glyph->bitmap_top - face->glyph->bitmap.rows,
		face->glyph->bitmap.width,
		face->glyph->bitmap.rows);

	for (y = 0; y < glyph->h; y++)
	{
		memcpy(glyph->samples + y * glyph->w,
			face->glyph->bitmap.buffer + (glyph->h - y - 1) * face->glyph->bitmap.pitch,
			glyph->w);
	}

	return glyph;
}

/*
 * Type 3 fonts...
 */

fz_font *
fz_newtype3font(char *name, fz_matrix matrix)
{
	fz_font *font;
	int i;

	font = fz_newfont();
	font->t3procs = fz_malloc(sizeof(fz_buffer*) * 256);
	font->t3widths = fz_malloc(sizeof(float) * 256);

	fz_strlcpy(font->name, name, sizeof(font->name));
	font->t3matrix = matrix;
	for (i = 0; i < 256; i++)
	{
		font->t3procs[i] = nil;
		font->t3widths[i] = 0;
	}

	return font;
}

fz_pixmap *
fz_rendert3glyph(fz_font *font, int gid, fz_matrix trm)
{
	fz_error error;
	fz_matrix ctm;
	fz_buffer *contents;
	fz_bbox bbox;
	fz_device *dev;
	fz_glyphcache *cache;
	fz_pixmap *glyph;

	if (gid < 0 || gid > 255)
		return NULL;

	contents = font->t3procs[gid];
	if (!contents)
		return NULL;

	ctm = fz_concat(font->t3matrix, trm);
	dev = fz_newbboxdevice(&bbox);
	error = font->t3runcontentstream(dev, ctm, font->t3xref, font->t3resources, contents);
	if (error)
		fz_catch(error, "cannot draw type3 glyph");
	fz_freedevice(dev);

	glyph = fz_newpixmap(nil, bbox.x0-1, bbox.y0-1, bbox.x1 - bbox.x0 + 1, bbox.y1 - bbox.y0 + 1);
	fz_clearpixmap(glyph, 0x00);

	cache = fz_newglyphcache();
	dev = fz_newdrawdevice(cache, glyph);
	error = font->t3runcontentstream(dev, ctm, font->t3xref, font->t3resources, contents);
	if (error)
		fz_catch(error, "cannot draw type3 glyph");
	fz_freedevice(dev);
	fz_freeglyphcache(cache);

	return glyph;
}

void
fz_debugfont(fz_font *font)
{
	printf("font '%s' {\n", font->name);

	if (font->ftface)
	{
		printf("  freetype face %p\n", font->ftface);
		if (font->ftsubstitute)
			printf("  substitute font\n");
	}

	if (font->t3procs)
	{
		printf("  type3 matrix [%g %g %g %g]\n",
			font->t3matrix.a, font->t3matrix.b,
			font->t3matrix.c, font->t3matrix.d);
	}

	printf("  bbox [%g %g %g %g]\n",
		font->bbox.x0, font->bbox.y0,
		font->bbox.x1, font->bbox.y1);

	printf("}\n");
}

