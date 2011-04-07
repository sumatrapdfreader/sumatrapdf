#include "fitz.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

static void fz_finalize_freetype(void);

static fz_font *
fz_new_font(void)
{
	fz_font *font;

	font = fz_malloc(sizeof(fz_font));
	font->refs = 1;
	strcpy(font->name, "<unknown>");

	font->ft_face = NULL;
	font->ft_substitute = 0;
	font->ft_hint = 0;

	font->ft_file = NULL;
	font->ft_data = NULL;
	font->ft_size = 0;

	font->t3matrix = fz_identity;
	font->t3resources = NULL;
	font->t3procs = NULL;
	font->t3widths = NULL;
	font->t3xref = NULL;
	font->t3run = NULL;

	font->bbox.x0 = 0;
	font->bbox.y0 = 0;
	font->bbox.x1 = 1000;
	font->bbox.y1 = 1000;

	font->width_count = 0;
	font->width_table = NULL;

	return font;
}

fz_font *
fz_keep_font(fz_font *font)
{
	font->refs ++;
	return font;
}

void
fz_drop_font(fz_font *font)
{
	int fterr;
	int i;

	if (font && --font->refs == 0)
	{
		if (font->t3procs)
		{
			if (font->t3resources)
				fz_drop_obj(font->t3resources);
			for (i = 0; i < 256; i++)
				if (font->t3procs[i])
					fz_drop_buffer(font->t3procs[i]);
			fz_free(font->t3procs);
			fz_free(font->t3widths);
		}

		if (font->ft_face)
		{
			fterr = FT_Done_Face((FT_Face)font->ft_face);
			if (fterr)
				fz_warn("freetype finalizing face: %s", ft_error_string(fterr));
			fz_finalize_freetype();
		}

		if (font->ft_file)
			fz_free(font->ft_file);
		if (font->ft_data)
			fz_free(font->ft_data);

		if (font->width_table)
			fz_free(font->width_table);

		fz_free(font);
	}
}

void
fz_set_font_bbox(fz_font *font, float xmin, float ymin, float xmax, float ymax)
{
	font->bbox.x0 = xmin;
	font->bbox.y0 = ymin;
	font->bbox.x1 = xmax;
	font->bbox.y1 = ymax;
}

/*
 * Freetype hooks
 */

static FT_Library fz_ftlib = NULL;
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

char *ft_error_string(int err)
{
	const struct ft_error *e;

	for (e = ft_errors; e->str != NULL; e++)
		if (e->err == err)
			return e->str;

	return "Unknown error";
}

static fz_error
fz_init_freetype(void)
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
		return fz_throw("cannot init freetype: %s", ft_error_string(fterr));

	FT_Library_Version(fz_ftlib, &maj, &min, &pat);
	if (maj == 2 && min == 1 && pat < 7)
	{
		fterr = FT_Done_FreeType(fz_ftlib);
		if (fterr)
			fz_warn("freetype finalizing: %s", ft_error_string(fterr));
		return fz_throw("freetype version too old: %d.%d.%d", maj, min, pat);
	}

	fz_ftlib_refs++;
	return fz_okay;
}

static void
fz_finalize_freetype(void)
{
	int fterr;

	if (--fz_ftlib_refs == 0)
	{
		fterr = FT_Done_FreeType(fz_ftlib);
		if (fterr)
			fz_warn("freetype finalizing: %s", ft_error_string(fterr));
		fz_ftlib = NULL;
	}
}

fz_error
fz_new_font_from_file(fz_font **fontp, char *path, int index)
{
	FT_Face face;
	fz_error error;
	fz_font *font;
	int fterr;

	error = fz_init_freetype();
	if (error)
		return fz_rethrow(error, "cannot init freetype library");

	fterr = FT_New_Face(fz_ftlib, path, index, &face);
	if (fterr)
		return fz_throw("freetype: cannot load font: %s", ft_error_string(fterr));

	font = fz_new_font();
	font->ft_face = face;
	font->bbox.x0 = face->bbox.xMin * 1000 / face->units_per_EM;
	font->bbox.y0 = face->bbox.yMin * 1000 / face->units_per_EM;
	font->bbox.x1 = face->bbox.xMax * 1000 / face->units_per_EM;
	font->bbox.y1 = face->bbox.yMax * 1000 / face->units_per_EM;

	*fontp = font;
	return fz_okay;
}

fz_error
fz_new_font_from_memory(fz_font **fontp, unsigned char *data, int len, int index)
{
	FT_Face face;
	fz_error error;
	fz_font *font;
	int fterr;

	error = fz_init_freetype();
	if (error)
		return fz_rethrow(error, "cannot init freetype library");

	fterr = FT_New_Memory_Face(fz_ftlib, data, len, index, &face);
	if (fterr)
		return fz_throw("freetype: cannot load font: %s", ft_error_string(fterr));

	font = fz_new_font();
	font->ft_face = face;
	font->bbox.x0 = face->bbox.xMin * 1000 / face->units_per_EM;
	font->bbox.y0 = face->bbox.yMin * 1000 / face->units_per_EM;
	font->bbox.x1 = face->bbox.xMax * 1000 / face->units_per_EM;
	font->bbox.y1 = face->bbox.yMax * 1000 / face->units_per_EM;

	*fontp = font;
	return fz_okay;
}

static fz_matrix
fz_adjust_ft_glyph_width(fz_font *font, int gid, fz_matrix trm)
{
	/* Fudge the font matrix to stretch the glyph if we've substituted the font. */
	if (font->ft_substitute && gid < font->width_count)
	{
		FT_Error fterr;
		int subw;
		int realw;
		float scale;

		/* TODO: use FT_Get_Advance */
		fterr = FT_Set_Char_Size(font->ft_face, 1000, 1000, 72, 72);
		if (fterr)
			fz_warn("freetype setting character size: %s", ft_error_string(fterr));

		fterr = FT_Load_Glyph(font->ft_face, gid,
			FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
		if (fterr)
			fz_warn("freetype failed to load glyph: %s", ft_error_string(fterr));

		realw = ((FT_Face)font->ft_face)->glyph->metrics.horiAdvance;
		subw = font->width_table[gid];
		if (realw)
			scale = (float) subw / realw;
		else
			scale = 1;

		return fz_concat(fz_scale(scale, 1), trm);
	}

	return trm;
}

fz_pixmap *
fz_render_ft_glyph(fz_font *font, int gid, fz_matrix trm)
{
	FT_Face face = font->ft_face;
	FT_Matrix m;
	FT_Vector v;
	FT_Error fterr;
	fz_pixmap *glyph;
	int y;

	trm = fz_adjust_ft_glyph_width(font, gid, trm);

	/*
	Freetype mutilates complex glyphs if they are loaded
	with FT_Set_Char_Size 1.0. it rounds the coordinates
	before applying transformation. to get more precision in
	freetype, we shift part of the scale in the matrix
	into FT_Set_Char_Size instead
	*/

	m.xx = trm.a * 64; /* should be 65536 */
	m.yx = trm.b * 64;
	m.xy = trm.c * 64;
	m.yy = trm.d * 64;
	v.x = trm.e * 64;
	v.y = trm.f * 64;

	fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); /* should be 64, 64 */
	if (fterr)
		fz_warn("freetype setting character size: %s", ft_error_string(fterr));
	FT_Set_Transform(face, &m, &v);

	if (font->ft_hint)
	{
		/*
		Enable hinting, but keep the huge char size so that
		it is hinted for a character. This will in effect nullify
		the effect of grid fitting. This form of hinting should
		only be used for DynaLab and similar tricky TrueType fonts,
		so that we get the correct outline shape.
		*/
#ifdef GRIDFIT
		/* If you really want grid fitting, enable this code. */
		float scale = fz_matrix_expansion(trm);
		m.xx = trm.a * 65536 / scale;
		m.xy = trm.b * 65536 / scale;
		m.yx = trm.c * 65536 / scale;
		m.yy = trm.d * 65536 / scale;
		v.x = 0;
		v.y = 0;

		fterr = FT_Set_Char_Size(face, 64 * scale, 64 * scale, 72, 72);
		if (fterr)
			fz_warn("freetype setting character size: %s", ft_error_string(fterr));
		FT_Set_Transform(face, &m, &v);
#endif
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP);
		if (fterr)
			fz_warn("freetype load glyph (gid %d): %s", gid, ft_error_string(fterr));
	}
	else
	{
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
		if (fterr)
		{
			fz_warn("freetype load glyph (gid %d): %s", gid, ft_error_string(fterr));
			return NULL;
		}
	}

	fterr = FT_Render_Glyph(face->glyph, fz_get_aa_level() > 0 ? ft_render_mode_normal : ft_render_mode_mono);
	if (fterr)
	{
		fz_warn("freetype render glyph (gid %d): %s", gid, ft_error_string(fterr));
		return NULL;
	}

	glyph = fz_new_pixmap(NULL,
		face->glyph->bitmap_left,
		face->glyph->bitmap_top - face->glyph->bitmap.rows,
		face->glyph->bitmap.width,
		face->glyph->bitmap.rows);

	if (face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
	{
		for (y = 0; y < glyph->h; y++)
		{
			unsigned char *out = glyph->samples + y * glyph->w;
			unsigned char *in = face->glyph->bitmap.buffer + (glyph->h - y - 1) * face->glyph->bitmap.pitch;
			unsigned char bit = 0x80;
			int w = glyph->w;
			while (w--)
			{
				*out++ = (*in & bit) ? 255 : 0;
				bit >>= 1;
				if (bit == 0)
				{
					bit = 0x80;
					in++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < glyph->h; y++)
		{
			memcpy(glyph->samples + y * glyph->w,
				face->glyph->bitmap.buffer + (glyph->h - y - 1) * face->glyph->bitmap.pitch,
				glyph->w);
		}
	}

	return glyph;
}

fz_pixmap *
fz_render_ft_stroked_glyph(fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, fz_stroke_state *state)
{
	FT_Face face = font->ft_face;
	float expansion = fz_matrix_expansion(ctm);
	int linewidth = state->linewidth * expansion * 64 / 2;
	FT_Matrix m;
	FT_Vector v;
	FT_Error fterr;
	FT_Stroker stroker;
	FT_Glyph glyph;
	FT_BitmapGlyph bitmap;
	fz_pixmap *pix;
	int y;

	trm = fz_adjust_ft_glyph_width(font, gid, trm);

	m.xx = trm.a * 64; /* should be 65536 */
	m.yx = trm.b * 64;
	m.xy = trm.c * 64;
	m.yy = trm.d * 64;
	v.x = trm.e * 64;
	v.y = trm.f * 64;

	fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); /* should be 64, 64 */
	if (fterr)
	{
		fz_warn("FT_Set_Char_Size: %s", ft_error_string(fterr));
		return NULL;
	}

	FT_Set_Transform(face, &m, &v);

	fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
	if (fterr)
	{
		fz_warn("FT_Load_Glyph(gid %d): %s", gid, ft_error_string(fterr));
		return NULL;
	}

	fterr = FT_Stroker_New(fz_ftlib, &stroker);
	if (fterr)
	{
		fz_warn("FT_Stroker_New: %s", ft_error_string(fterr));
		return NULL;
	}

	FT_Stroker_Set(stroker, linewidth, state->linecap, state->linejoin, state->miterlimit * 65536);

	fterr = FT_Get_Glyph(face->glyph, &glyph);
	if (fterr)
	{
		fz_warn("FT_Get_Glyph: %s", ft_error_string(fterr));
		FT_Stroker_Done(stroker);
		return NULL;
	}

	fterr = FT_Glyph_Stroke(&glyph, stroker, 1);
	if (fterr)
	{
		fz_warn("FT_Glyph_Stroke: %s", ft_error_string(fterr));
		FT_Done_Glyph(glyph);
		FT_Stroker_Done(stroker);
		return NULL;
	}

	fterr = FT_Glyph_To_Bitmap(&glyph, fz_get_aa_level() > 0 ? ft_render_mode_normal : ft_render_mode_mono, 0, 1);
	if (fterr)
	{
		fz_warn("FT_Glyph_To_Bitmap: %s", ft_error_string(fterr));
		FT_Done_Glyph(glyph);
		FT_Stroker_Done(stroker);
		return NULL;
	}

	bitmap = (FT_BitmapGlyph)glyph;
	pix = fz_new_pixmap(NULL,
		bitmap->left,
		bitmap->top - bitmap->bitmap.rows,
		bitmap->bitmap.width,
		bitmap->bitmap.rows);

	if (face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
	{
		for (y = 0; y < pix->h; y++)
		{
			unsigned char *out = pix->samples + y * pix->w;
			unsigned char *in = bitmap->bitmap.buffer + (pix->h - y - 1) * bitmap->bitmap.pitch;
			unsigned char bit = 0x80;
			int w = bitmap->bitmap.width;
			while (w--)
			{
				*out++ = (*in & bit) ? 255 : 0;
				bit >>= 1;
				if (bit == 0)
				{
					bit = 0x80;
					in++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < pix->h; y++)
		{
			memcpy(pix->samples + y * pix->w,
				bitmap->bitmap.buffer + (pix->h - y - 1) * bitmap->bitmap.pitch,
				pix->w);
		}
	}

	FT_Done_Glyph(glyph);
	FT_Stroker_Done(stroker);

	return pix;
}

/*
 * Type 3 fonts...
 */

fz_font *
fz_new_type3_font(char *name, fz_matrix matrix)
{
	fz_font *font;
	int i;

	font = fz_new_font();
	font->t3procs = fz_calloc(256, sizeof(fz_buffer*));
	font->t3widths = fz_calloc(256, sizeof(float));

	fz_strlcpy(font->name, name, sizeof(font->name));
	font->t3matrix = matrix;
	for (i = 0; i < 256; i++)
	{
		font->t3procs[i] = NULL;
		font->t3widths[i] = 0;
	}

	return font;
}

fz_pixmap *
fz_render_t3_glyph(fz_font *font, int gid, fz_matrix trm)
{
	fz_error error;
	fz_matrix ctm;
	fz_buffer *contents;
	fz_bbox bbox;
	fz_device *dev;
	fz_glyph_cache *cache;
	fz_pixmap *glyph;
	fz_pixmap *result;

	if (gid < 0 || gid > 255)
		return NULL;

	contents = font->t3procs[gid];
	if (!contents)
		return NULL;

	ctm = fz_concat(font->t3matrix, trm);
	dev = fz_new_bbox_device(&bbox);
	error = font->t3run(font->t3xref, font->t3resources, contents, dev, ctm);
	if (error)
		fz_catch(error, "cannot draw type3 glyph");
	fz_free_device(dev);

	glyph = fz_new_pixmap(fz_device_gray, bbox.x0-1, bbox.y0-1, bbox.x1 - bbox.x0 + 1, bbox.y1 - bbox.y0 + 1);
	fz_clear_pixmap(glyph);

	cache = fz_new_glyph_cache();
	dev = fz_new_draw_device(cache, glyph);
	error = font->t3run(font->t3xref, font->t3resources, contents, dev, ctm);
	if (error)
		fz_catch(error, "cannot draw type3 glyph");
	fz_free_device(dev);
	fz_free_glyph_cache(cache);

	result = fz_alpha_from_gray(glyph, 0);
	fz_drop_pixmap(glyph);

	return result;
}

void
fz_debug_font(fz_font *font)
{
	printf("font '%s' {\n", font->name);

	if (font->ft_face)
	{
		printf("\tfreetype face %p\n", font->ft_face);
		if (font->ft_substitute)
			printf("\tsubstitute font\n");
	}

	if (font->t3procs)
	{
		printf("\ttype3 matrix [%g %g %g %g]\n",
			font->t3matrix.a, font->t3matrix.b,
			font->t3matrix.c, font->t3matrix.d);
	}

	printf("\tbbox [%g %g %g %g]\n",
		font->bbox.x0, font->bbox.y0,
		font->bbox.x1, font->bbox.y1);

	printf("}\n");
}
