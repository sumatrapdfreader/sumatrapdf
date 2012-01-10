#include "fitz.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

static void fz_finalize_freetype(fz_context *ctx);

static fz_font *
fz_new_font(fz_context *ctx, char *name)
{
	fz_font *font;

	font = fz_malloc_struct(ctx, fz_font);
	font->refs = 1;

	if (name)
		fz_strlcpy(font->name, name, sizeof font->name);
	else
		fz_strlcpy(font->name, "(null)", sizeof font->name);

	font->ft_face = NULL;
	font->ft_substitute = 0;
	font->ft_bold = 0;
	font->ft_italic = 0;
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
fz_drop_font(fz_context *ctx, fz_font *font)
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
					fz_drop_buffer(ctx, font->t3procs[i]);
			fz_free(ctx, font->t3procs);
			fz_free(ctx, font->t3widths);
		}

		if (font->ft_face)
		{
			fterr = FT_Done_Face((FT_Face)font->ft_face);
			if (fterr)
				fz_warn(ctx, "freetype finalizing face: %s", ft_error_string(fterr));
			fz_finalize_freetype(ctx);
		}

		if (font->ft_file)
			fz_free(ctx, font->ft_file);
		if (font->ft_data)
			fz_free(ctx, font->ft_data);

		if (font->width_table)
			fz_free(ctx, font->width_table);

		fz_free(ctx, font);
	}
}

void
fz_set_font_bbox(fz_font *font, float xmin, float ymin, float xmax, float ymax)
{
	/* SumatraPDF: the font bbox is assumed to be premultiplied with 1000 */
	font->bbox.x0 = xmin * 1000;
	font->bbox.y0 = ymin * 1000;
	font->bbox.x1 = xmax * 1000;
	font->bbox.y1 = ymax * 1000;
}

/*
 * Freetype hooks
 */

struct fz_font_context_s {
	FT_Library ftlib;
	int ftlib_refs;
};

#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s)	{ (e), (s) },
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST	{ 0, NULL }

struct ft_error
{
	int err;
	char *str;
};

void fz_new_font_context(fz_context *ctx)
{
	ctx->font = fz_malloc_struct(ctx, fz_font_context);
	ctx->font->ftlib = NULL;
	ctx->font->ftlib_refs = 0;
}

void fz_free_font_context(fz_context *ctx)
{
	if (!ctx->font)
		return;
	/* assert(!ctx->ftlib); */
	/* assert(ctx->ftlib_refs == 0); */
	fz_free(ctx, ctx->font);
}

static const struct ft_error ft_errors[] =
{
#include FT_ERRORS_H
};

char *ft_error_string(int err)
{
	const struct ft_error *e;

	for (e = ft_errors; e->str; e++)
		if (e->err == err)
			return e->str;

	return "Unknown error";
}

static void
fz_init_freetype(fz_context *ctx)
{
	int fterr;
	int maj, min, pat;
	fz_font_context *fct = ctx->font;

	if (fct->ftlib)
	{
		fct->ftlib_refs++;
		return;
	}

	fterr = FT_Init_FreeType(&fct->ftlib);
	if (fterr)
		fz_throw(ctx, "cannot init freetype: %s", ft_error_string(fterr));

	FT_Library_Version(fct->ftlib, &maj, &min, &pat);
	if (maj == 2 && min == 1 && pat < 7)
	{
		fterr = FT_Done_FreeType(fct->ftlib);
		if (fterr)
			fz_warn(ctx, "freetype finalizing: %s", ft_error_string(fterr));
		fz_throw(ctx, "freetype version too old: %d.%d.%d", maj, min, pat);
	}

	fct->ftlib_refs++;
}

static void
fz_finalize_freetype(fz_context *ctx)
{
	int fterr;
	fz_font_context *fct = ctx->font;

	if (--fct->ftlib_refs == 0)
	{
		fterr = FT_Done_FreeType(fct->ftlib);
		if (fterr)
			fz_warn(ctx, "freetype finalizing: %s", ft_error_string(fterr));
		fct->ftlib = NULL;
	}
}

/* SumatraPDF: some Chinese fonts seem to wrongly use pre-devided units */
static void
fz_check_font_dimensions(FT_Face face)
{
	/* prevent broken fonts from causing a division by zero */
	if (face->units_per_EM == 0)
		face->units_per_EM = 1000;

	if (face->bbox.xMin == 0 && face->bbox.yMin == 0 &&
		face->bbox.xMax == 1 && face->bbox.yMax == 1 &&
		face->ascender == 1 && face->descender == 0)
	{
		face->bbox.xMax = face->units_per_EM;
		face->bbox.yMax = face->units_per_EM;
		face->ascender = face->units_per_EM;
	}

	/* use default values for fonts with an empty glyph bbox */
	if (face->bbox.xMin == 0 && face->bbox.yMin == 0 &&
		face->bbox.xMax == 0 && face->bbox.yMax == 0 &&
		face->ascender == 0 && face->descender == 0)
	{
		face->bbox.xMax = face->units_per_EM;
		face->bbox.yMax = face->units_per_EM;
		face->ascender = 0.8f * face->units_per_EM;
		face->descender = -0.2f * face->units_per_EM;
	}
}

fz_font *
fz_new_font_from_file(fz_context *ctx, char *path, int index)
{
	FT_Face face;
	fz_font *font;
	int fterr;

	fz_init_freetype(ctx);

	fterr = FT_New_Face(ctx->font->ftlib, path, index, &face);
	if (fterr)
	{
		fz_finalize_freetype(ctx);
		fz_throw(ctx, "freetype: cannot load font: %s", ft_error_string(fterr));
	}
	fz_check_font_dimensions(face);

	font = fz_new_font(ctx, face->family_name);
	font->ft_face = face;
	font->bbox.x0 = face->bbox.xMin * 1000 / face->units_per_EM;
	font->bbox.y0 = face->bbox.yMin * 1000 / face->units_per_EM;
	font->bbox.x1 = face->bbox.xMax * 1000 / face->units_per_EM;
	font->bbox.y1 = face->bbox.yMax * 1000 / face->units_per_EM;

	return font;
}

fz_font *
fz_new_font_from_memory(fz_context *ctx, unsigned char *data, int len, int index)
{
	FT_Face face;
	fz_font *font;
	int fterr;

	fz_init_freetype(ctx);

	fterr = FT_New_Memory_Face(ctx->font->ftlib, data, len, index, &face);
	if (fterr)
	{
		fz_finalize_freetype(ctx);
		fz_throw(ctx, "freetype: cannot load font: %s", ft_error_string(fterr));
	}
	fz_check_font_dimensions(face);

	font = fz_new_font(ctx, face->family_name);
	font->ft_face = face;
	font->bbox.x0 = face->bbox.xMin * 1000 / face->units_per_EM;
	font->bbox.y0 = face->bbox.yMin * 1000 / face->units_per_EM;
	font->bbox.x1 = face->bbox.xMax * 1000 / face->units_per_EM;
	font->bbox.y1 = face->bbox.yMax * 1000 / face->units_per_EM;

	return font;
}

static fz_matrix
fz_adjust_ft_glyph_width(fz_context *ctx, fz_font *font, int gid, fz_matrix trm)
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
			fz_warn(ctx, "freetype setting character size: %s", ft_error_string(fterr));

		fterr = FT_Load_Glyph(font->ft_face, gid,
			FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
		if (fterr)
			fz_warn(ctx, "freetype failed to load glyph: %s", ft_error_string(fterr));

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

static fz_pixmap *
fz_copy_ft_bitmap(fz_context *ctx, int left, int top, FT_Bitmap *bitmap)
{
	fz_pixmap *pixmap;
	int y;

	pixmap = fz_new_pixmap(ctx, NULL, bitmap->width, bitmap->rows);
	pixmap->x = left;
	pixmap->y = top - bitmap->rows;

	if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO)
	{
		for (y = 0; y < pixmap->h; y++)
		{
			unsigned char *out = pixmap->samples + y * pixmap->w;
			unsigned char *in = bitmap->buffer + (pixmap->h - y - 1) * bitmap->pitch;
			unsigned char bit = 0x80;
			int w = pixmap->w;
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
		for (y = 0; y < pixmap->h; y++)
		{
			memcpy(pixmap->samples + y * pixmap->w,
				bitmap->buffer + (pixmap->h - y - 1) * bitmap->pitch,
				pixmap->w);
		}
	}

	return pixmap;
}

fz_pixmap *
fz_render_ft_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm)
{
	FT_Face face = font->ft_face;
	FT_Matrix m;
	FT_Vector v;
	FT_Error fterr;

	trm = fz_adjust_ft_glyph_width(ctx, font, gid, trm);

	if (font->ft_italic)
		trm = fz_concat(fz_shear(0.3f, 0), trm);

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
		fz_warn(ctx, "freetype setting character size: %s", ft_error_string(fterr));
	FT_Set_Transform(face, &m, &v);

	if (fz_get_aa_level(ctx) == 0)
	{
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
			fz_warn(ctx, "freetype setting character size: %s", ft_error_string(fterr));
		FT_Set_Transform(face, &m, &v);
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_MONO);
		if (fterr)
			fz_warn(ctx, "freetype load glyph (gid %d): %s", gid, ft_error_string(fterr));
	}
	else if (font->ft_hint)
	{
		/*
		Enable hinting, but keep the huge char size so that
		it is hinted for a character. This will in effect nullify
		the effect of grid fitting. This form of hinting should
		only be used for DynaLab and similar tricky TrueType fonts,
		so that we get the correct outline shape.
		*/
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP);
		if (fterr)
			fz_warn(ctx, "freetype load glyph (gid %d): %s", gid, ft_error_string(fterr));
	}
	else
	{
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
		if (fterr)
		{
			fz_warn(ctx, "freetype load glyph (gid %d): %s", gid, ft_error_string(fterr));
			return NULL;
		}
	}

	if (font->ft_bold)
	{
		float strength = fz_matrix_expansion(trm) * 0.04f;
		FT_Outline_Embolden(&face->glyph->outline, strength * 64);
		FT_Outline_Translate(&face->glyph->outline, -strength * 32, -strength * 32);
	}

	fterr = FT_Render_Glyph(face->glyph, fz_get_aa_level(ctx) > 0 ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);
	if (fterr)
	{
		fz_warn(ctx, "freetype render glyph (gid %d): %s", gid, ft_error_string(fterr));
		return NULL;
	}

	return fz_copy_ft_bitmap(ctx, face->glyph->bitmap_left, face->glyph->bitmap_top, &face->glyph->bitmap);
}

fz_pixmap *
fz_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, fz_stroke_state *state)
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
	fz_pixmap *pixmap;
	FT_Stroker_LineJoin line_join; /* SumatraPDF: use proper line join */

	trm = fz_adjust_ft_glyph_width(ctx, font, gid, trm);

	if (font->ft_italic)
		trm = fz_concat(fz_shear(0.3f, 0), trm);

	m.xx = trm.a * 64; /* should be 65536 */
	m.yx = trm.b * 64;
	m.xy = trm.c * 64;
	m.yy = trm.d * 64;
	v.x = trm.e * 64;
	v.y = trm.f * 64;

	fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); /* should be 64, 64 */
	if (fterr)
	{
		fz_warn(ctx, "FT_Set_Char_Size: %s", ft_error_string(fterr));
		return NULL;
	}

	FT_Set_Transform(face, &m, &v);

	fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
	if (fterr)
	{
		fz_warn(ctx, "FT_Load_Glyph(gid %d): %s", gid, ft_error_string(fterr));
		return NULL;
	}

	fterr = FT_Stroker_New(ctx->font->ftlib, &stroker);
	if (fterr)
	{
		fz_warn(ctx, "FT_Stroker_New: %s", ft_error_string(fterr));
		return NULL;
	}

	/* SumatraPDF: use proper line join */
	line_join = state->linejoin == 0 ? FT_STROKER_LINEJOIN_MITER_FIXED :
	            state->linejoin == 1 ? FT_STROKER_LINEJOIN_ROUND :
	            state->linejoin == 2 ? FT_STROKER_LINEJOIN_BEVEL :
	                                   FT_STROKER_LINEJOIN_MITER_VARIABLE;
	FT_Stroker_Set(stroker, linewidth, state->start_cap, line_join, state->miterlimit * 65536);

	fterr = FT_Get_Glyph(face->glyph, &glyph);
	if (fterr)
	{
		fz_warn(ctx, "FT_Get_Glyph: %s", ft_error_string(fterr));
		FT_Stroker_Done(stroker);
		return NULL;
	}

	fterr = FT_Glyph_Stroke(&glyph, stroker, 1);
	if (fterr)
	{
		fz_warn(ctx, "FT_Glyph_Stroke: %s", ft_error_string(fterr));
		FT_Done_Glyph(glyph);
		FT_Stroker_Done(stroker);
		return NULL;
	}

	FT_Stroker_Done(stroker);

	fterr = FT_Glyph_To_Bitmap(&glyph, fz_get_aa_level(ctx) > 0 ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO, 0, 1);
	if (fterr)
	{
		fz_warn(ctx, "FT_Glyph_To_Bitmap: %s", ft_error_string(fterr));
		FT_Done_Glyph(glyph);
		return NULL;
	}

	bitmap = (FT_BitmapGlyph)glyph;
	pixmap = fz_copy_ft_bitmap(ctx, bitmap->left, bitmap->top, &bitmap->bitmap);
	FT_Done_Glyph(glyph);

	return pixmap;
}

/*
 * Type 3 fonts...
 */

fz_font *
fz_new_type3_font(fz_context *ctx, char *name, fz_matrix matrix)
{
	fz_font *font;
	int i;

	font = fz_new_font(ctx, name);
	font->t3procs = fz_malloc_array(ctx, 256, sizeof(fz_buffer*));
	font->t3widths = fz_malloc_array(ctx, 256, sizeof(float));

	font->t3matrix = matrix;
	for (i = 0; i < 256; i++)
	{
		font->t3procs[i] = NULL;
		font->t3widths[i] = 0;
	}

	return font;
}

fz_pixmap *
fz_render_t3_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_colorspace *model)
{
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
	dev = fz_new_bbox_device(ctx, &bbox);
	font->t3run(font->t3xref, font->t3resources, contents, dev, ctm);
	/* RJW: "cannot draw type3 glyph" */

	if (dev->flags & FZ_CHARPROC_MASK)
	{
		if (dev->flags & FZ_CHARPROC_COLOR)
			fz_warn(ctx, "type3 glyph claims to be both masked and colored");
		model = NULL;
	}
	else if (dev->flags & FZ_CHARPROC_COLOR)
	{
		if (!model)
			fz_warn(ctx, "colored type3 glyph wanted in masked context");
	}
	else
	{
		fz_warn(ctx, "type3 glyph doesn't specify masked or colored");
		model = NULL; /* Treat as masked */
	}

	fz_free_device(dev);

	bbox.x0--;
	bbox.y0--;
	bbox.x1++;
	bbox.y1++;

	glyph = fz_new_pixmap_with_rect(ctx, model ? model : fz_device_gray, bbox);
	fz_clear_pixmap(glyph);

	cache = fz_new_glyph_cache(ctx);
	dev = fz_new_draw_device_type3(ctx, cache, glyph);
	font->t3run(font->t3xref, font->t3resources, contents, dev, ctm);
	/* RJW: "cannot draw type3 glyph" */
	fz_free_device(dev);
	fz_free_glyph_cache(ctx, cache);

	if (!model)
	{
		result = fz_alpha_from_gray(ctx, glyph, 0);
		fz_drop_pixmap(ctx, glyph);
	}
	else
		result = glyph;

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
