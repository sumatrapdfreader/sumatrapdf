#include "fitz_base.h"
#include "fitz_tree.h"
#include "fitz_draw.h" /* FIXME -- for glyph rendering callbacks */

#include <ft2build.h>
#include FT_FREETYPE_H

static void fz_finalizefreetype(void);

static fz_font *
fz_newfont(void)
{
	fz_font *font;

	font = fz_malloc(sizeof(fz_font));
	if (!font)
		return nil;

	font->refs = 1;
	strcpy(font->name, "<unknown>");

	font->ftface = nil;
	font->ftsubstitute = 0;
	font->fthint = 0;

	font->t3matrix = fz_identity();
	font->t3procs = nil;

	font->bbox.x0 = 0;
	font->bbox.y0 = 0;
	font->bbox.x1 = 1000;
	font->bbox.y1 = 1000;

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
			for (i = 0; i < 256; i++)
				if (font->t3procs[i])
					fz_droptree(font->t3procs[i]);
			fz_free(font->t3procs);
		}

		if (font->ftface)
		{
			fterr = FT_Done_Face((FT_Face)font->ftface);
			if (fterr)
				fz_warn("freetype finalizing face: %s", ft_errorstring(fterr));
			fz_finalizefreetype();
		}

		fz_free(font);
	}
}

void
fz_setfontbbox(fz_font *font, int xmin, int ymin, int xmax, int ymax)
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
	if (!font)
		return fz_rethrow(-1, "out of memory: font struct");

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
	if (!font)
		return fz_rethrow(-1, "out of memory: font struct");

	fterr = FT_New_Memory_Face(fz_ftlib, data, len, index, (FT_Face*)&font->ftface);
	if (fterr)
	{
		fz_free(font);
		return fz_throw("freetype: cannot load font: %s", ft_errorstring(fterr));
	}

	*fontp = font;
	return fz_okay;
}

fz_error
fz_renderftglyph(fz_glyph *glyph, fz_font *font, int gid, fz_matrix trm)
{
	FT_Face face = font->ftface;
	FT_Matrix m;
	FT_Vector v;
	FT_Error fterr;
	int x, y;

#if 0
	/* We lost this feature in refactoring.
	 * We can't access pdf_fontdesc metrics from fz_font.
	 * The pdf_fontdesc metrics are character based (cid),
	 * where the glyph being rendered is given by glyph (gid).
	 */
	if (font->ftsubstitute && font->wmode == 0)
	{
		fz_hmtx subw;
		int realw;
		float scale;

		fterr = FT_Set_Char_Size(face, 1000, 1000, 72, 72);
		if (fterr)
			return fz_warn("freetype setting character size: %s", ft_errorstring(fterr));

		fterr = FT_Load_Glyph(font->ftface, gid,
				FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
		if (fterr)
			return fz_throw("freetype failed to load glyph: %s", ft_errorstring(fterr));

		realw = ((FT_Face)font->ftface)->glyph->advance.x;
		subw = fz_gethmtx(font, cid); // <-- this is the offender
		if (realw)
			scale = (float) subw.w / realw;
		else
			scale = 1.0;

		trm = fz_concat(fz_scale(scale, 1.0), trm);
	}
#endif

	glyph->w = 0;
	glyph->h = 0;
	glyph->x = 0;
	glyph->y = 0;
	glyph->samples = nil;

	/* freetype mutilates complex glyphs if they are loaded
	 * with FT_Set_Char_Size 1.0. it rounds the coordinates
	 * before applying transformation. to get more precision in
	 * freetype, we shift part of the scale in the matrix
	 * into FT_Set_Char_Size instead
	 */

	m.xx = trm.a * 64;      /* should be 65536 */
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

	glyph->w = face->glyph->bitmap.width;
	glyph->h = face->glyph->bitmap.rows;
	glyph->x = face->glyph->bitmap_left;
	glyph->y = face->glyph->bitmap_top - glyph->h;
	glyph->samples = face->glyph->bitmap.buffer;

	for (y = 0; y < glyph->h / 2; y++)
	{
		for (x = 0; x < glyph->w; x++)
		{
			unsigned char a = glyph->samples[y * glyph->w + x ];
			unsigned char b = glyph->samples[(glyph->h - y - 1) * glyph->w + x];
			glyph->samples[y * glyph->w + x ] = b;
			glyph->samples[(glyph->h - y - 1) * glyph->w + x] = a;
		}
	}

	return fz_okay;
}


/*
 * Type 3 fonts...
 */

fz_error
fz_newtype3font(fz_font **fontp, char *name, fz_matrix matrix)
{
	fz_font *font;
	int i;

	font = fz_newfont();
	if (!font)
		return fz_rethrow(-1, "out of memory: font struct");

	font->t3procs = fz_malloc(sizeof(fz_tree*) * 256);
	if (!font->t3procs)
	{
		fz_free(font);
		return fz_rethrow(-1, "out of memory: type3 font charproc array");
	}

	font->t3widths = fz_malloc(sizeof(float) * 256);
	if (!font->t3widths)
	{
		fz_free(font->t3procs);
		fz_free(font);
		return fz_rethrow(-1, "out of memory: type3 font widths array");
	}

	font->t3matrix = matrix;
	for (i = 0; i < 256; i++)
	{
		font->t3procs[i] = nil;
		font->t3widths[i] = 0;
	}

	strlcpy(font->name, name, sizeof(font->name));

	*fontp = font;
	return fz_okay;
}

/* XXX UGLY HACK XXX */
extern fz_colorspace *pdf_devicegray;

fz_error
fz_rendert3glyph(fz_glyph *glyph, fz_font *font, int gid, fz_matrix trm)
{
	fz_error error;
	fz_renderer *gc;
	fz_tree *tree;
	fz_matrix ctm;
	fz_irect bbox;

	/* TODO: make it reentrant */
	static fz_pixmap *pixmap = nil;
	if (pixmap)
	{
		fz_droppixmap(pixmap);
		pixmap = nil;
	}

	if (gid < 0 || gid > 255)
		return fz_throw("assert: glyph out of range");

	tree = font->t3procs[gid];
	if (!tree)
	{
		glyph->w = 0;
		glyph->h = 0;
		return fz_okay;
	}

	ctm = fz_concat(font->t3matrix, trm);
	bbox = fz_roundrect(fz_boundtree(tree, ctm));

	error = fz_newrenderer(&gc, pdf_devicegray, 1, 4096);
	if (error)
		return fz_rethrow(error, "cannot create renderer");
	error = fz_rendertree(&pixmap, gc, tree, ctm, bbox, 0);
	fz_droprenderer(gc);
	if (error)
		return fz_rethrow(error, "cannot render glyph");

	assert(pixmap->n == 1);

	glyph->x = pixmap->x;
	glyph->y = pixmap->y;
	glyph->w = pixmap->w;
	glyph->h = pixmap->h;
	glyph->samples = pixmap->samples;

	return fz_okay;
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

	printf("  bbox [%d %d %d %d]\n",
			font->bbox.x0, font->bbox.y0,
			font->bbox.x1, font->bbox.y1);

	printf("}\n");
}

