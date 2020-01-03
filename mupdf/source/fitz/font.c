#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"
#include "fitz-imp.h"
#include "draw-imp.h"

#include <ft2build.h>
#include "hb.h"
#include "hb-ft.h"

#include <assert.h>

#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_MODULE_H
#include FT_STROKER_H
#include FT_SYSTEM_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H

#define MAX_BBOX_TABLE_SIZE 4096
#define MAX_ADVANCE_CACHE 4096

#ifndef FT_SFNT_OS2
#define FT_SFNT_OS2 ft_sfnt_os2
#endif

/* 20 degrees */
#define SHEAR 0.36397f

int ft_char_index(void *face, int cid)
{
	int gid = FT_Get_Char_Index(face, cid);
	if (gid == 0)
		gid = FT_Get_Char_Index(face, 0xf000 + cid);

	/* some chinese fonts only ship the similarly looking 0x2026 */
	if (gid == 0 && cid == 0x22ef)
		gid = FT_Get_Char_Index(face, 0x2026);

	return gid;
}

int ft_name_index(void *face, const char *name)
{
	int code = FT_Get_Name_Index(face, (char*)name);
	if (code == 0)
	{
		int unicode = fz_unicode_from_glyph_name(name);
		if (unicode)
		{
			const char **dupnames = fz_duplicate_glyph_names_from_unicode(unicode);
			while (*dupnames)
			{
				code = FT_Get_Name_Index(face, (char*)*dupnames);
				if (code)
					break;
				dupnames++;
			}
			if (code == 0)
			{
				char buf[10];
				sprintf(buf, "uni%04X", unicode);
				code = FT_Get_Name_Index(face, buf);
			}
		}
	}
	return code;
}

static void fz_drop_freetype(fz_context *ctx);

static fz_font *
fz_new_font(fz_context *ctx, const char *name, int use_glyph_bbox, int glyph_count)
{
	fz_font *font;
	int i;

	font = fz_malloc_struct(ctx, fz_font);
	font->refs = 1;

	if (name)
		fz_strlcpy(font->name, name, sizeof font->name);
	else
		fz_strlcpy(font->name, "(null)", sizeof font->name);

	font->ft_face = NULL;
	font->flags.ft_substitute = 0;
	font->flags.fake_bold = 0;
	font->flags.fake_italic = 0;
	font->flags.has_opentype = 0;

	font->t3matrix = fz_identity;
	font->t3resources = NULL;
	font->t3procs = NULL;
	font->t3lists = NULL;
	font->t3widths = NULL;
	font->t3flags = NULL;
	font->t3doc = NULL;
	font->t3run = NULL;

	font->bbox.x0 = 0;
	font->bbox.y0 = 0;
	font->bbox.x1 = 1;
	font->bbox.y1 = 1;

	font->glyph_count = glyph_count;

	if (use_glyph_bbox && glyph_count <= MAX_BBOX_TABLE_SIZE)
	{
		fz_try(ctx)
			font->bbox_table = Memento_label(fz_malloc_array(ctx, glyph_count, fz_rect), "font_bbox_table");
		fz_catch(ctx)
		{
			fz_free(ctx, font);
			fz_rethrow(ctx);
		}
		for (i = 0; i < glyph_count; i++)
			font->bbox_table[i] = fz_infinite_rect;
	}
	else
	{
		font->bbox_table = NULL;
	}

	font->width_count = 0;
	font->width_table = NULL;

	return font;
}

/*
	Add a reference to an existing fz_font.

	font: The font to add a reference to.

	Returns the same font.
*/
fz_font *
fz_keep_font(fz_context *ctx, fz_font *font)
{
	return fz_keep_imp(ctx, font, &font->refs);
}

static void
free_resources(fz_context *ctx, fz_font *font)
{
	int i;

	if (font->t3resources)
	{
		font->t3freeres(ctx, font->t3doc, font->t3resources);
		font->t3resources = NULL;
	}

	if (font->t3procs)
	{
		for (i = 0; i < 256; i++)
			fz_drop_buffer(ctx, font->t3procs[i]);
	}
	fz_free(ctx, font->t3procs);
	font->t3procs = NULL;
}

/*
	Internal function to remove the
	references to a document held by a Type3 font. This is
	called during document destruction to ensure that Type3
	fonts clean up properly.

	Without this call being made, Type3 fonts can be left
	holding pdf_obj references for the sake of interpretation
	operations that will never come. These references
	cannot be freed after the document, hence this function
	forces them to be freed earlier in the process.

	font: The font to decouple.

	t3doc: The document to which the font may refer.
*/
void fz_decouple_type3_font(fz_context *ctx, fz_font *font, void *t3doc)
{
	if (!font || !t3doc || font->t3doc == NULL)
		return;

	if (font->t3doc != t3doc)
		fz_throw(ctx, FZ_ERROR_GENERIC, "can't decouple type3 font from a different doc");

	font->t3doc = NULL;
	free_resources(ctx, font);
}

/*
	Drop a reference to a fz_font, destroying the
	font when the last reference is dropped.

	font: The font to drop a reference to.
*/
void
fz_drop_font(fz_context *ctx, fz_font *font)
{
	int fterr;
	int i;

	if (!fz_drop_imp(ctx, font, &font->refs))
		return;

	if (font->t3lists)
	{
		free_resources(ctx, font);
		for (i = 0; i < 256; i++)
			fz_drop_display_list(ctx, font->t3lists[i]);
		fz_free(ctx, font->t3procs);
		fz_free(ctx, font->t3lists);
		fz_free(ctx, font->t3widths);
		fz_free(ctx, font->t3flags);
	}

	if (font->ft_face)
	{
		fz_lock(ctx, FZ_LOCK_FREETYPE);
		fterr = FT_Done_Face((FT_Face)font->ft_face);
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		if (fterr)
			fz_warn(ctx, "FT_Done_Face(%s): %s", font->name, ft_error_string(fterr));
		fz_drop_freetype(ctx);
	}

	for (i = 0; i < 256; ++i)
		fz_free(ctx, font->encoding_cache[i]);

	fz_drop_buffer(ctx, font->buffer);
	fz_free(ctx, font->bbox_table);
	fz_free(ctx, font->width_table);
	fz_free(ctx, font->advance_cache);
	if (font->shaper_data.destroy && font->shaper_data.shaper_handle)
	{
		font->shaper_data.destroy(ctx, font->shaper_data.shaper_handle);
	}
	fz_free(ctx, font);
}

/*
	Set the font bbox.

	font: The font to set the bbox for.

	xmin, ymin, xmax, ymax: The bounding box.
*/
void
fz_set_font_bbox(fz_context *ctx, fz_font *font, float xmin, float ymin, float xmax, float ymax)
{
	if (xmin >= xmax || ymin >= ymax)
	{
		/* Invalid bbox supplied. */
		if (font->t3procs)
		{
			/* For type3 fonts we use the union of all the glyphs' bboxes. */
			font->bbox = fz_empty_rect;
		}
		else
		{
			/* For other fonts it would be prohibitively slow to measure the true one, so make one up. */
			font->bbox = fz_unit_rect;
		}
		font->flags.invalid_bbox = 1;
	}
	else
	{
		font->bbox.x0 = xmin;
		font->bbox.y0 = ymin;
		font->bbox.x1 = xmax;
		font->bbox.y1 = ymax;
	}
}

float fz_font_ascender(fz_context *ctx, fz_font *font)
{
	if (font->t3procs)
		return font->bbox.y1;
	else
	{
		FT_Face face = font->ft_face;
		if (face->ascender == 0)
			return 0.8f;
		return (float)face->ascender / face->units_per_EM;
	}
}

float fz_font_descender(fz_context *ctx, fz_font *font)
{
	if (font->t3procs)
		return font->bbox.y0;
	else
	{
		FT_Face face = font->ft_face;
		if (face->descender == 0)
			return -0.2f;
		return (float)face->descender / face->units_per_EM;
	}
}

/*
 * Freetype hooks
 */

struct fz_font_context_s
{
	int ctx_refs;
	FT_Library ftlib;
	struct FT_MemoryRec_ ftmemory;
	int ftlib_refs;
	fz_load_system_font_fn *load_font;
	fz_load_system_cjk_font_fn *load_cjk_font;
	fz_load_system_fallback_font_fn *load_fallback_font;

	/* Cached fallback fonts */
	fz_font *base14[14];
	fz_font *cjk[4];
	struct { fz_font *serif, *sans; } fallback[256];
	fz_font *symbol1, *symbol2, *math, *music;
	fz_font *emoji;
};

#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s) { (e), (s) },
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST { 0, NULL }

struct ft_error
{
	int err;
	char *str;
};

static void *ft_alloc(FT_Memory memory, long size)
{
	fz_context *ctx = (fz_context *) memory->user;
	return Memento_label(fz_malloc_no_throw(ctx, size), "ft_alloc");
}

static void ft_free(FT_Memory memory, void *block)
{
	fz_context *ctx = (fz_context *) memory->user;
	fz_free(ctx, block);
}

static void *ft_realloc(FT_Memory memory, long cur_size, long new_size, void *block)
{
	fz_context *ctx = (fz_context *) memory->user;
	void *newblock = NULL;
	if (new_size == 0)
	{
		fz_free(ctx, block);
		return newblock;
	}
	if (block == NULL)
		return ft_alloc(memory, new_size);
	return fz_realloc_no_throw(ctx, block, new_size);
}

void fz_new_font_context(fz_context *ctx)
{
	ctx->font = fz_malloc_struct(ctx, fz_font_context);
	ctx->font->ctx_refs = 1;
	ctx->font->ftlib = NULL;
	ctx->font->ftlib_refs = 0;
	ctx->font->load_font = NULL;
	ctx->font->ftmemory.user = ctx;
	ctx->font->ftmemory.alloc = ft_alloc;
	ctx->font->ftmemory.free = ft_free;
	ctx->font->ftmemory.realloc = ft_realloc;
}

fz_font_context *
fz_keep_font_context(fz_context *ctx)
{
	if (!ctx)
		return NULL;
	return fz_keep_imp(ctx, ctx->font, &ctx->font->ctx_refs);
}

void fz_drop_font_context(fz_context *ctx)
{
	if (!ctx)
		return;

	if (fz_drop_imp(ctx, ctx->font, &ctx->font->ctx_refs))
	{
		int i;

		for (i = 0; i < (int)nelem(ctx->font->base14); ++i)
			fz_drop_font(ctx, ctx->font->base14[i]);
		for (i = 0; i < (int)nelem(ctx->font->cjk); ++i)
			fz_drop_font(ctx, ctx->font->cjk[i]);
		for (i = 0; i < (int)nelem(ctx->font->fallback); ++i)
		{
			fz_drop_font(ctx, ctx->font->fallback[i].serif);
			fz_drop_font(ctx, ctx->font->fallback[i].sans);
		}
		fz_drop_font(ctx, ctx->font->symbol1);
		fz_drop_font(ctx, ctx->font->symbol2);
		fz_drop_font(ctx, ctx->font->math);
		fz_drop_font(ctx, ctx->font->music);
		fz_drop_font(ctx, ctx->font->emoji);
		fz_free(ctx, ctx->font);
		ctx->font = NULL;
	}
}

/*
	Install functions to allow
	MuPDF to request fonts from the system.

	Only one set of hooks can be in use at a time.
*/
void fz_install_load_system_font_funcs(fz_context *ctx,
		fz_load_system_font_fn *f,
		fz_load_system_cjk_font_fn *f_cjk,
		fz_load_system_fallback_font_fn *f_back)
{
	ctx->font->load_font = f;
	ctx->font->load_cjk_font = f_cjk;
	ctx->font->load_fallback_font = f_back;
}

/* fz_load_*_font returns NULL if no font could be loaded (also on error) */
/*
	Attempt to load a given font from the
	system.

	name: The name of the desired font.

	bold: 1 if bold desired, 0 otherwise.

	italic: 1 if italic desired, 0 otherwise.

	needs_exact_metrics: 1 if an exact metrical match is required,
	0 otherwise.

	Returns a new font handle, or NULL if no matching font was found
	(or on error).
*/
fz_font *fz_load_system_font(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics)
{
	fz_font *font = NULL;

	if (ctx->font->load_font)
	{
		fz_try(ctx)
			font = ctx->font->load_font(ctx, name, bold, italic, needs_exact_metrics);
		fz_catch(ctx)
			font = NULL;
	}

	return font;
}

/*
	Attempt to load a given font from
	the system.

	name: The name of the desired font.

	ordering: The ordering to load the font from (e.g. FZ_ADOBE_KOREA)

	serif: 1 if serif desired, 0 otherwise.

	Returns a new font handle, or NULL if no matching font was found
	(or on error).
*/
fz_font *fz_load_system_cjk_font(fz_context *ctx, const char *name, int ros, int serif)
{
	fz_font *font = NULL;

	if (ctx->font->load_cjk_font)
	{
		fz_try(ctx)
			font = ctx->font->load_cjk_font(ctx, name, ros, serif);
		fz_catch(ctx)
			font = NULL;
	}

	return font;
}

fz_font *fz_load_system_fallback_font(fz_context *ctx, int script, int language, int serif, int bold, int italic)
{
	fz_font *font = NULL;

	if (ctx->font->load_fallback_font)
	{
		fz_try(ctx)
			font = ctx->font->load_fallback_font(ctx, script, language, serif, bold, italic);
		fz_catch(ctx)
			font = NULL;
	}

	return font;
}

/*
	Try to load a fallback font for the
	given combination of font attributes. Whether a font is
	present or not will depend on the configuration in which
	MuPDF is built.

	script: The script desired (e.g. UCDN_SCRIPT_KATAKANA).

	language: The language desired (e.g. FZ_LANG_ja).

	serif: 1 if serif desired, 0 otherwise.

	bold: 1 if bold desired, 0 otherwise.

	italic: 1 if italic desired, 0 otherwise.

	Returns a new font handle, or NULL if not available.
*/
fz_font *fz_load_fallback_font(fz_context *ctx, int script, int language, int serif, int bold, int italic)
{
	fz_font **fontp;
	const unsigned char *data;
	int index;
	int subfont;
	int size;

	if (script < 0 || script >= (int)nelem(ctx->font->fallback))
		return NULL;

	/* TODO: bold and italic */

	index = script;
	if (script == UCDN_SCRIPT_HAN)
	{
		switch (language)
		{
		case FZ_LANG_ja: index = UCDN_LAST_SCRIPT + 1; break;
		case FZ_LANG_ko: index = UCDN_LAST_SCRIPT + 2; break;
		case FZ_LANG_zh_Hans: index = UCDN_LAST_SCRIPT + 3; break;
		case FZ_LANG_zh_Hant: index = UCDN_LAST_SCRIPT + 4; break;
		}
	}
	if (script == UCDN_SCRIPT_ARABIC)
	{
		if (language == FZ_LANG_ur || language == FZ_LANG_urd)
			index = UCDN_LAST_SCRIPT + 5;
	}

	if (serif)
		fontp = &ctx->font->fallback[index].serif;
	else
		fontp = &ctx->font->fallback[index].sans;

	if (!*fontp)
	{
		*fontp = fz_load_system_fallback_font(ctx, script, language, serif, bold, italic);
		if (!*fontp)
		{
			data = fz_lookup_noto_font(ctx, script, language, &size, &subfont);
			if (data)
				*fontp = fz_new_font_from_memory(ctx, NULL, data, size, subfont, 0);
		}
	}

	return *fontp;
}

static fz_font *fz_load_fallback_math_font(fz_context *ctx)
{
	const unsigned char *data;
	int size;
	if (!ctx->font->math)
	{
		data = fz_lookup_noto_math_font(ctx, &size);
		if (data)
			ctx->font->math = fz_new_font_from_memory(ctx, NULL, data, size, 0, 0);
	}
	return ctx->font->math;
}

static fz_font *fz_load_fallback_music_font(fz_context *ctx)
{
	const unsigned char *data;
	int size;
	if (!ctx->font->music)
	{
		data = fz_lookup_noto_music_font(ctx, &size);
		if (data)
			ctx->font->music = fz_new_font_from_memory(ctx, NULL, data, size, 0, 0);
	}
	return ctx->font->music;
}

static fz_font *fz_load_fallback_symbol1_font(fz_context *ctx)
{
	const unsigned char *data;
	int size;
	if (!ctx->font->symbol1)
	{
		data = fz_lookup_noto_symbol1_font(ctx, &size);
		if (data)
			ctx->font->symbol1 = fz_new_font_from_memory(ctx, NULL, data, size, 0, 0);
	}
	return ctx->font->symbol1;
}

static fz_font *fz_load_fallback_symbol2_font(fz_context *ctx)
{
	const unsigned char *data;
	int size;
	if (!ctx->font->symbol2)
	{
		data = fz_lookup_noto_symbol2_font(ctx, &size);
		if (data)
			ctx->font->symbol2 = fz_new_font_from_memory(ctx, NULL, data, size, 0, 0);
	}
	return ctx->font->symbol2;
}

static fz_font *fz_load_fallback_emoji_font(fz_context *ctx)
{
	const unsigned char *data;
	int size;
	if (!ctx->font->emoji)
	{
		data = fz_lookup_noto_emoji_font(ctx, &size);
		if (data)
			ctx->font->emoji = fz_new_font_from_memory(ctx, NULL, data, size, 0, 0);
	}
	return ctx->font->emoji;
}

static const struct ft_error ft_errors[] =
{
#include FT_ERRORS_H
};

/*
	map an FT error number to a
	static string.

	err: The error number to lookup.

	Returns a pointer to a static textual representation
	of a freetype error.
*/
const char *ft_error_string(int err)
{
	const struct ft_error *e;

	for (e = ft_errors; e->str; e++)
		if (e->err == err)
			return e->str;

	return "Unknown error";
}

static void
fz_keep_freetype(fz_context *ctx)
{
	int fterr;
	int maj, min, pat;
	fz_font_context *fct = ctx->font;

	fz_lock(ctx, FZ_LOCK_FREETYPE);
	if (fct->ftlib)
	{
		fct->ftlib_refs++;
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		return;
	}

	fterr = FT_New_Library(&fct->ftmemory, &fct->ftlib);
	if (fterr)
	{
		const char *mess = ft_error_string(fterr);
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot init freetype: %s", mess);
	}

	FT_Add_Default_Modules(fct->ftlib);

	FT_Library_Version(fct->ftlib, &maj, &min, &pat);
	if (maj == 2 && min == 1 && pat < 7)
	{
		fterr = FT_Done_Library(fct->ftlib);
		if (fterr)
			fz_warn(ctx, "FT_Done_Library(): %s", ft_error_string(fterr));
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		fz_throw(ctx, FZ_ERROR_GENERIC, "freetype version too old: %d.%d.%d", maj, min, pat);
	}

	fct->ftlib_refs++;
	fz_unlock(ctx, FZ_LOCK_FREETYPE);
}

static void
fz_drop_freetype(fz_context *ctx)
{
	int fterr;
	fz_font_context *fct = ctx->font;

	fz_lock(ctx, FZ_LOCK_FREETYPE);
	if (--fct->ftlib_refs == 0)
	{
		fterr = FT_Done_Library(fct->ftlib);
		if (fterr)
			fz_warn(ctx, "FT_Done_Library(): %s", ft_error_string(fterr));
		fct->ftlib = NULL;
	}
	fz_unlock(ctx, FZ_LOCK_FREETYPE);
}

/*
	Create a new font from a font
	file in a fz_buffer.

	name: Name of font (leave NULL to use name from font).

	buffer: Buffer to load from.

	index: Which font from the file to load (0 for default).

	use_glyph_box: 1 if we should use the glyph bbox, 0 otherwise.

	Returns new font handle, or throws exception on error.
*/
fz_font *
fz_new_font_from_buffer(fz_context *ctx, const char *name, fz_buffer *buffer, int index, int use_glyph_bbox)
{
	FT_Face face;
	TT_OS2 *os2;
	fz_font *font;
	int fterr;
	FT_ULong tag, size, i, n;
	char namebuf[sizeof(font->name)];

	fz_keep_freetype(ctx);

	fz_lock(ctx, FZ_LOCK_FREETYPE);
	fterr = FT_New_Memory_Face(ctx->font->ftlib, buffer->data, (FT_Long)buffer->len, index, &face);
	fz_unlock(ctx, FZ_LOCK_FREETYPE);
	if (fterr)
	{
		fz_drop_freetype(ctx);
		fz_throw(ctx, FZ_ERROR_GENERIC, "FT_New_Memory_Face(%s): %s", name, ft_error_string(fterr));
	}

	if (!name)
	{
		if (!face->family_name)
		{
			name = face->style_name;
		}
		else if (!face->style_name)
		{
			name = face->family_name;
		}
		else if (strstr(face->style_name, face->family_name) == face->style_name)
		{
			name = face->style_name;
		}
		else
		{
			fz_strlcpy(namebuf, face->family_name, sizeof(namebuf));
			fz_strlcat(namebuf, " ", sizeof(namebuf));
			fz_strlcat(namebuf, face->style_name, sizeof(namebuf));
			name = namebuf;
		}
	}

	fz_try(ctx)
		font = fz_new_font(ctx, name, use_glyph_bbox, face->num_glyphs);
	fz_catch(ctx)
	{
		fz_lock(ctx, FZ_LOCK_FREETYPE);
		fterr = FT_Done_Face(face);
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		if (fterr)
			fz_warn(ctx, "FT_Done_Face(%s): %s", name, ft_error_string(fterr));
		fz_drop_freetype(ctx);
		fz_rethrow(ctx);
	}

	font->ft_face = face;
	fz_set_font_bbox(ctx, font,
		(float) face->bbox.xMin / face->units_per_EM,
		(float) face->bbox.yMin / face->units_per_EM,
		(float) face->bbox.xMax / face->units_per_EM,
		(float) face->bbox.yMax / face->units_per_EM);

	font->flags.is_mono = !!(face->face_flags & FT_FACE_FLAG_FIXED_WIDTH);
	font->flags.is_serif = 1;
	font->flags.is_bold = !!(face->style_flags & FT_STYLE_FLAG_BOLD);
	font->flags.is_italic = !!(face->style_flags & FT_STYLE_FLAG_ITALIC);

	if (FT_IS_SFNT(face))
	{
		os2 = FT_Get_Sfnt_Table(face, FT_SFNT_OS2);
		if (os2)
			font->flags.is_serif = !(os2->sFamilyClass & 2048); /* Class 8 is sans-serif */

		FT_Sfnt_Table_Info(face, 0, NULL, &n);
		for (i = 0; i < n; ++i)
		{
			FT_Sfnt_Table_Info(face, i, &tag, &size);
			if (tag == TTAG_GDEF || tag == TTAG_GPOS || tag == TTAG_GSUB)
				font->flags.has_opentype = 1;
		}
	}

	if (name)
	{
		if (!font->flags.is_bold)
		{
			if (strstr(name, "Semibold")) font->flags.is_bold = 1;
			if (strstr(name, "Bold")) font->flags.is_bold = 1;
		}
		if (!font->flags.is_italic)
		{
			if (strstr(name, "Italic")) font->flags.is_italic = 1;
			if (strstr(name, "Oblique")) font->flags.is_italic = 1;
		}
	}

	font->buffer = fz_keep_buffer(ctx, buffer);

	return font;
}

/*
	Create a new font from a font
	file in memory.

	name: Name of font (leave NULL to use name from font).

	data: Pointer to the font file data.

	len: Length of the font file data.

	index: Which font from the file to load (0 for default).

	use_glyph_box: 1 if we should use the glyph bbox, 0 otherwise.

	Returns new font handle, or throws exception on error.
*/
fz_font *
fz_new_font_from_memory(fz_context *ctx, const char *name, const unsigned char *data, int len, int index, int use_glyph_bbox)
{
	fz_buffer *buffer = fz_new_buffer_from_shared_data(ctx, data, len);
	fz_font *font = NULL;
	fz_try(ctx)
		font = fz_new_font_from_buffer(ctx, name, buffer, index, use_glyph_bbox);
	fz_always(ctx)
		fz_drop_buffer(ctx, buffer);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return font;
}

/*
	Create a new font from a font
	file.

	name: Name of font (leave NULL to use name from font).

	path: File path to load from.

	index: Which font from the file to load (0 for default).

	use_glyph_box: 1 if we should use the glyph bbox, 0 otherwise.

	Returns new font handle, or throws exception on error.
*/
fz_font *
fz_new_font_from_file(fz_context *ctx, const char *name, const char *path, int index, int use_glyph_bbox)
{
	fz_buffer *buffer = fz_read_file(ctx, path);
	fz_font *font = NULL;
	fz_try(ctx)
		font = fz_new_font_from_buffer(ctx, name, buffer, index, use_glyph_bbox);
	fz_always(ctx)
		fz_drop_buffer(ctx, buffer);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return font;
}

static int
find_base14_index(const char *name)
{
	if (!strcmp(name, "Courier")) return 0;
	if (!strcmp(name, "Courier-Oblique")) return 1;
	if (!strcmp(name, "Courier-Bold")) return 2;
	if (!strcmp(name, "Courier-BoldOblique")) return 3;
	if (!strcmp(name, "Helvetica")) return 4;
	if (!strcmp(name, "Helvetica-Oblique")) return 5;
	if (!strcmp(name, "Helvetica-Bold")) return 6;
	if (!strcmp(name, "Helvetica-BoldOblique")) return 7;
	if (!strcmp(name, "Times-Roman")) return 8;
	if (!strcmp(name, "Times-Italic")) return 9;
	if (!strcmp(name, "Times-Bold")) return 10;
	if (!strcmp(name, "Times-BoldItalic")) return 11;
	if (!strcmp(name, "Symbol")) return 12;
	if (!strcmp(name, "ZapfDingbats")) return 13;
	return -1;
}

/* Create a new font from one of the built-in fonts. */
fz_font *
fz_new_base14_font(fz_context *ctx, const char *name)
{
	const unsigned char *data;
	int size;
	int x = find_base14_index(name);
	if (x >= 0)
	{
		if (ctx->font->base14[x])
			return fz_keep_font(ctx, ctx->font->base14[x]);
		data = fz_lookup_base14_font(ctx, name, &size);
		if (data)
		{
			ctx->font->base14[x] = fz_new_font_from_memory(ctx, name, data, size, 0, 1);
			ctx->font->base14[x]->flags.is_serif = (name[0] == 'T'); /* Times-Roman */
			return fz_keep_font(ctx, ctx->font->base14[x]);
		}
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find builtin font with name '%s'", name);
}

fz_font *
fz_new_cjk_font(fz_context *ctx, int ordering)
{
	const unsigned char *data;
	int size, index;
	if (ordering >= 0 && ordering < (int)nelem(ctx->font->cjk))
	{
		if (ctx->font->cjk[ordering])
			return fz_keep_font(ctx, ctx->font->cjk[ordering]);
		data = fz_lookup_cjk_font(ctx, ordering, &size, &index);
		if (data)
		{
			ctx->font->cjk[ordering] = fz_new_font_from_memory(ctx, NULL, data, size, index, 0);
			return fz_keep_font(ctx, ctx->font->cjk[ordering]);
		}
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find builtin CJK font");
}

fz_font *
fz_new_builtin_font(fz_context *ctx, const char *name, int is_bold, int is_italic)
{
	const unsigned char *data;
	int size;
	data = fz_lookup_builtin_font(ctx, name, is_bold, is_italic, &size);
	if (!data)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find builtin font with name '%s'", name);
	return fz_new_font_from_memory(ctx, NULL, data, size, 0, 0);
}

static fz_matrix *
fz_adjust_ft_glyph_width(fz_context *ctx, fz_font *font, int gid, fz_matrix *trm)
{
	/* Fudge the font matrix to stretch the glyph if we've substituted the font. */
	if (font->flags.ft_stretch && font->width_table /* && font->wmode == 0 */)
	{
		FT_Error fterr;
		FT_Fixed adv = 0;
		float subw;
		float realw;

		fz_lock(ctx, FZ_LOCK_FREETYPE);
		fterr = FT_Get_Advance(font->ft_face, gid, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_IGNORE_TRANSFORM, &adv);
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		if (fterr && fterr != FT_Err_Invalid_Argument)
			fz_warn(ctx, "FT_Get_Advance(%s,%d): %s", font->name, gid, ft_error_string(fterr));

		realw = adv * 1000.0f / ((FT_Face)font->ft_face)->units_per_EM;
		if (gid < font->width_count)
			subw = font->width_table[gid];
		else
			subw = font->width_default;

		/* Sanity check scaling in case of broken metrics. */
		if (realw > 0 && subw > 0)
			*trm = fz_pre_scale(*trm, subw / realw, 1);
	}

	return trm;
}

static fz_glyph *
glyph_from_ft_bitmap(fz_context *ctx, int left, int top, FT_Bitmap *bitmap)
{
	(void)Memento_label(bitmap->buffer, "ft_bitmap");
	if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO)
		return fz_new_glyph_from_1bpp_data(ctx, left, top - bitmap->rows, bitmap->width, bitmap->rows, bitmap->buffer + (bitmap->rows-1)*bitmap->pitch, -bitmap->pitch);
	else
		return fz_new_glyph_from_8bpp_data(ctx, left, top - bitmap->rows, bitmap->width, bitmap->rows, bitmap->buffer + (bitmap->rows-1)*bitmap->pitch, -bitmap->pitch);
}

static fz_pixmap *
pixmap_from_ft_bitmap(fz_context *ctx, int left, int top, FT_Bitmap *bitmap)
{
	(void)Memento_label(bitmap->buffer, "ft_bitmap");
	if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO)
		return fz_new_pixmap_from_1bpp_data(ctx, left, top - bitmap->rows, bitmap->width, bitmap->rows, bitmap->buffer + (bitmap->rows-1)*bitmap->pitch, -bitmap->pitch);
	else
		return fz_new_pixmap_from_8bpp_data(ctx, left, top - bitmap->rows, bitmap->width, bitmap->rows, bitmap->buffer + (bitmap->rows-1)*bitmap->pitch, -bitmap->pitch);
}

/* Takes the freetype lock, and returns with it held */
static FT_GlyphSlot
do_ft_render_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, int aa)
{
	FT_Face face = font->ft_face;
	FT_Matrix m;
	FT_Vector v;
	FT_Error fterr;

	float strength = fz_matrix_expansion(trm) * 0.02f;

	fz_adjust_ft_glyph_width(ctx, font, gid, &trm);

	if (font->flags.fake_italic)
		trm = fz_pre_shear(trm, SHEAR, 0);

	fz_lock(ctx, FZ_LOCK_FREETYPE);

	if (aa == 0)
	{
		/* enable grid fitting for non-antialiased rendering */
		float scale = fz_matrix_expansion(trm);
		m.xx = trm.a * 65536 / scale;
		m.yx = trm.b * 65536 / scale;
		m.xy = trm.c * 65536 / scale;
		m.yy = trm.d * 65536 / scale;
		v.x = 0;
		v.y = 0;

		fterr = FT_Set_Char_Size(face, 64 * scale, 64 * scale, 72, 72);
		if (fterr)
			fz_warn(ctx, "FT_Set_Char_Size(%s,%d,72): %s", font->name, (int)(64*scale), ft_error_string(fterr));
		FT_Set_Transform(face, &m, &v);
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_MONO);
		if (fterr)
		{
			fz_warn(ctx, "FT_Load_Glyph(%s,%d,FT_LOAD_TARGET_MONO): %s", font->name, gid, ft_error_string(fterr));
			goto retry_unhinted;
		}
	}
	else
	{
retry_unhinted:
		/*
		 * Freetype mutilates complex glyphs if they are loaded with
		 * FT_Set_Char_Size 1.0. It rounds the coordinates before applying
		 * transformation. To get more precision in freetype, we shift part of
		 * the scale in the matrix into FT_Set_Char_Size instead.
		 */

		/* Check for overflow; FreeType matrices use 16.16 fixed-point numbers */
		if (trm.a < -512 || trm.a > 512) return NULL;
		if (trm.b < -512 || trm.b > 512) return NULL;
		if (trm.c < -512 || trm.c > 512) return NULL;
		if (trm.d < -512 || trm.d > 512) return NULL;

		m.xx = trm.a * 64; /* should be 65536 */
		m.yx = trm.b * 64;
		m.xy = trm.c * 64;
		m.yy = trm.d * 64;
		v.x = trm.e * 64;
		v.y = trm.f * 64;

		fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); /* should be 64, 64 */
		if (fterr)
			fz_warn(ctx, "FT_Set_Char_Size(%s,65536,72): %s", font->name, ft_error_string(fterr));
		FT_Set_Transform(face, &m, &v);
		fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
		if (fterr)
		{
			fz_warn(ctx, "FT_Load_Glyph(%s,%d,FT_LOAD_NO_HINTING): %s", font->name, gid, ft_error_string(fterr));
			return NULL;
		}
	}

	if (font->flags.fake_bold)
	{
		FT_Outline_Embolden(&face->glyph->outline, strength * 64);
		FT_Outline_Translate(&face->glyph->outline, -strength * 32, -strength * 32);
	}

	fterr = FT_Render_Glyph(face->glyph, aa > 0 ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);
	if (fterr)
	{
		if (aa > 0)
			fz_warn(ctx, "FT_Render_Glyph(%s,%d,FT_RENDER_MODE_NORMAL): %s", font->name, gid, ft_error_string(fterr));
		else
			fz_warn(ctx, "FT_Render_Glyph(%s,%d,FT_RENDER_MODE_MONO): %s", font->name, gid, ft_error_string(fterr));
		return NULL;
	}
	return face->glyph;
}

fz_pixmap *
fz_render_ft_glyph_pixmap(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, int aa)
{
	FT_GlyphSlot slot = do_ft_render_glyph(ctx, font, gid, trm, aa);
	fz_pixmap *pixmap = NULL;

	if (slot == NULL)
	{
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		return NULL;
	}

	fz_try(ctx)
	{
		pixmap = pixmap_from_ft_bitmap(ctx, slot->bitmap_left, slot->bitmap_top, &slot->bitmap);
	}
	fz_always(ctx)
	{
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return pixmap;
}

/* The glyph cache lock is always taken when this is called. */
fz_glyph *
fz_render_ft_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, int aa)
{
	FT_GlyphSlot slot = do_ft_render_glyph(ctx, font, gid, trm, aa);
	fz_glyph *glyph = NULL;

	if (slot == NULL)
	{
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		return NULL;
	}

	fz_try(ctx)
	{
		glyph = glyph_from_ft_bitmap(ctx, slot->bitmap_left, slot->bitmap_top, &slot->bitmap);
	}
	fz_always(ctx)
	{
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return glyph;
}

/* Takes the freetype lock, and returns with it held */
static FT_Glyph
do_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, const fz_stroke_state *state, int aa)
{
	FT_Face face = font->ft_face;
	float expansion = fz_matrix_expansion(ctm);
	int linewidth = state->linewidth * expansion * 64 / 2;
	FT_Matrix m;
	FT_Vector v;
	FT_Error fterr;
	FT_Stroker stroker;
	FT_Glyph glyph;
	FT_Stroker_LineJoin line_join;
	FT_Stroker_LineCap line_cap;

	fz_adjust_ft_glyph_width(ctx, font, gid, &trm);

	if (font->flags.fake_italic)
		trm = fz_pre_shear(trm, SHEAR, 0);

	m.xx = trm.a * 64; /* should be 65536 */
	m.yx = trm.b * 64;
	m.xy = trm.c * 64;
	m.yy = trm.d * 64;
	v.x = trm.e * 64;
	v.y = trm.f * 64;

	fz_lock(ctx, FZ_LOCK_FREETYPE);
	fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); /* should be 64, 64 */
	if (fterr)
	{
		fz_warn(ctx, "FT_Set_Char_Size(%s,65536,72): %s", font->name, ft_error_string(fterr));
		return NULL;
	}

	FT_Set_Transform(face, &m, &v);

	fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
	if (fterr)
	{
		fz_warn(ctx, "FT_Load_Glyph(%s,%d,FT_LOAD_NO_HINTING): %s", font->name, gid, ft_error_string(fterr));
		return NULL;
	}

	fterr = FT_Stroker_New(ctx->font->ftlib, &stroker);
	if (fterr)
	{
		fz_warn(ctx, "FT_Stroker_New(): %s", ft_error_string(fterr));
		return NULL;
	}

	line_join =
		state->linejoin == FZ_LINEJOIN_MITER ? FT_STROKER_LINEJOIN_MITER_FIXED :
		state->linejoin == FZ_LINEJOIN_ROUND ? FT_STROKER_LINEJOIN_ROUND :
		state->linejoin == FZ_LINEJOIN_BEVEL ? FT_STROKER_LINEJOIN_BEVEL :
		FT_STROKER_LINEJOIN_MITER_VARIABLE;
	line_cap =
		state->start_cap == FZ_LINECAP_BUTT ? FT_STROKER_LINECAP_BUTT :
		state->start_cap == FZ_LINECAP_ROUND ? FT_STROKER_LINECAP_ROUND :
		state->start_cap == FZ_LINECAP_SQUARE ? FT_STROKER_LINECAP_SQUARE :
		state->start_cap == FZ_LINECAP_TRIANGLE ? FT_STROKER_LINECAP_BUTT :
		FT_STROKER_LINECAP_BUTT;

	FT_Stroker_Set(stroker, linewidth, line_cap, line_join, state->miterlimit * 65536);

	fterr = FT_Get_Glyph(face->glyph, &glyph);
	if (fterr)
	{
		fz_warn(ctx, "FT_Get_Glyph(): %s", ft_error_string(fterr));
		FT_Stroker_Done(stroker);
		return NULL;
	}

	fterr = FT_Glyph_Stroke(&glyph, stroker, 1);
	if (fterr)
	{
		fz_warn(ctx, "FT_Glyph_Stroke(): %s", ft_error_string(fterr));
		FT_Done_Glyph(glyph);
		FT_Stroker_Done(stroker);
		return NULL;
	}

	FT_Stroker_Done(stroker);

	fterr = FT_Glyph_To_Bitmap(&glyph, aa > 0 ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO, 0, 1);
	if (fterr)
	{
		fz_warn(ctx, "FT_Glyph_To_Bitmap(): %s", ft_error_string(fterr));
		FT_Done_Glyph(glyph);
		return NULL;
	}
	return glyph;
}

fz_pixmap *
fz_render_ft_stroked_glyph_pixmap(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, const fz_stroke_state *state, int aa)
{
	FT_Glyph glyph = do_render_ft_stroked_glyph(ctx, font, gid, trm, ctm, state, aa);
	FT_BitmapGlyph bitmap = (FT_BitmapGlyph)glyph;
	fz_pixmap *pixmap = NULL;

	if (bitmap == NULL)
	{
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		return NULL;
	}

	fz_try(ctx)
	{
		pixmap = pixmap_from_ft_bitmap(ctx, bitmap->left, bitmap->top, &bitmap->bitmap);
	}
	fz_always(ctx)
	{
		FT_Done_Glyph(glyph);
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return pixmap;
}

fz_glyph *
fz_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, const fz_stroke_state *state, int aa)
{
	FT_Glyph glyph = do_render_ft_stroked_glyph(ctx, font, gid, trm, ctm, state, aa);
	FT_BitmapGlyph bitmap = (FT_BitmapGlyph)glyph;
	fz_glyph *result = NULL;

	if (bitmap == NULL)
	{
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		return NULL;
	}

	fz_try(ctx)
	{
		result = glyph_from_ft_bitmap(ctx, bitmap->left, bitmap->top, &bitmap->bitmap);
	}
	fz_always(ctx)
	{
		FT_Done_Glyph(glyph);
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return result;
}

static fz_rect *
fz_bound_ft_glyph(fz_context *ctx, fz_font *font, int gid)
{
	FT_Face face = font->ft_face;
	FT_Error fterr;
	FT_BBox cbox;
	FT_Matrix m;
	FT_Vector v;
	fz_rect *bounds = &font->bbox_table[gid];

	// TODO: refactor loading into fz_load_ft_glyph
	// TODO: cache results

	const int scale = face->units_per_EM;
	const float recip = 1.0f / scale;
	const float strength = 0.02f;
	fz_matrix trm = fz_identity;

	fz_adjust_ft_glyph_width(ctx, font, gid, &trm);

	if (font->flags.fake_italic)
		trm = fz_pre_shear(trm, SHEAR, 0);

	m.xx = trm.a * 65536;
	m.yx = trm.b * 65536;
	m.xy = trm.c * 65536;
	m.yy = trm.d * 65536;
	v.x = trm.e * 65536;
	v.y = trm.f * 65536;

	fz_lock(ctx, FZ_LOCK_FREETYPE);
	/* Set the char size to scale=face->units_per_EM to effectively give
	 * us unscaled results. This avoids quantisation. We then apply the
	 * scale ourselves below. */
	fterr = FT_Set_Char_Size(face, scale, scale, 72, 72);
	if (fterr)
		fz_warn(ctx, "FT_Set_Char_Size(%s,%d,72): %s", font->name, scale, ft_error_string(fterr));
	FT_Set_Transform(face, &m, &v);

	fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
	if (fterr)
	{
		fz_warn(ctx, "FT_Load_Glyph(%s,%d,FT_LOAD_NO_HINTING): %s", font->name, gid, ft_error_string(fterr));
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		bounds->x0 = bounds->x1 = trm.e;
		bounds->y0 = bounds->y1 = trm.f;
		return bounds;
	}

	if (font->flags.fake_bold)
	{
		FT_Outline_Embolden(&face->glyph->outline, strength * scale);
		FT_Outline_Translate(&face->glyph->outline, -strength * 0.5f * scale, -strength * 0.5f * scale);
	}

	FT_Outline_Get_CBox(&face->glyph->outline, &cbox);
	fz_unlock(ctx, FZ_LOCK_FREETYPE);
	bounds->x0 = cbox.xMin * recip;
	bounds->y0 = cbox.yMin * recip;
	bounds->x1 = cbox.xMax * recip;
	bounds->y1 = cbox.yMax * recip;

	if (fz_is_empty_rect(*bounds))
	{
		bounds->x0 = bounds->x1 = trm.e;
		bounds->y0 = bounds->y1 = trm.f;
	}

	return bounds;
}

/* Turn FT_Outline into a fz_path */

struct closure {
	fz_context *ctx;
	fz_path *path;
	fz_matrix trm;
};

static int move_to(const FT_Vector *p, void *cc_)
{
	struct closure *cc = (struct closure *)cc_;
	fz_context *ctx = cc->ctx;
	fz_path *path = cc->path;
	fz_point pt;

	pt = fz_transform_point_xy(p->x, p->y, cc->trm);
	fz_moveto(ctx, path, pt.x, pt.y);
	return 0;
}

static int line_to(const FT_Vector *p, void *cc_)
{
	struct closure *cc = (struct closure *)cc_;
	fz_context *ctx = cc->ctx;
	fz_path *path = cc->path;
	fz_point pt;

	pt = fz_transform_point_xy(p->x, p->y, cc->trm);
	fz_lineto(ctx, path, pt.x, pt.y);
	return 0;
}

static int conic_to(const FT_Vector *c, const FT_Vector *p, void *cc_)
{
	struct closure *cc = (struct closure *)cc_;
	fz_context *ctx = cc->ctx;
	fz_path *path = cc->path;
	fz_point ct, pt;

	ct = fz_transform_point_xy(c->x, c->y, cc->trm);
	pt = fz_transform_point_xy(p->x, p->y, cc->trm);

	fz_quadto(ctx, path, ct.x, ct.y, pt.x, pt.y);
	return 0;
}

static int cubic_to(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *p, void *cc_)
{
	struct closure *cc = (struct closure *)cc_;
	fz_context *ctx = cc->ctx;
	fz_path *path = cc->path;
	fz_point c1t, c2t, pt;

	c1t = fz_transform_point_xy(c1->x, c1->y, cc->trm);
	c2t = fz_transform_point_xy(c2->x, c2->y, cc->trm);
	pt = fz_transform_point_xy(p->x, p->y, cc->trm);

	fz_curveto(ctx, path, c1t.x, c1t.y, c2t.x, c2t.y, pt.x, pt.y);
	return 0;
}

static const FT_Outline_Funcs outline_funcs = {
	move_to, line_to, conic_to, cubic_to, 0, 0
};

fz_path *
fz_outline_ft_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm)
{
	struct closure cc;
	FT_Face face = font->ft_face;
	int fterr;

	const int scale = face->units_per_EM;
	const float recip = 1.0f / scale;
	const float strength = 0.02f;

	fz_adjust_ft_glyph_width(ctx, font, gid, &trm);

	if (font->flags.fake_italic)
		trm = fz_pre_shear(trm, SHEAR, 0);

	fz_lock(ctx, FZ_LOCK_FREETYPE);

	fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_SCALE | FT_LOAD_IGNORE_TRANSFORM);
	if (fterr)
	{
		fz_warn(ctx, "FT_Load_Glyph(%s,%d,FT_LOAD_NO_SCALE|FT_LOAD_IGNORE_TRANSFORM): %s", font->name, gid, ft_error_string(fterr));
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		return NULL;
	}

	if (font->flags.fake_bold)
	{
		FT_Outline_Embolden(&face->glyph->outline, strength * scale);
		FT_Outline_Translate(&face->glyph->outline, -strength * 0.5f * scale, -strength * 0.5f * scale);
	}

	cc.path = NULL;
	fz_try(ctx)
	{
		cc.ctx = ctx;
		cc.path = fz_new_path(ctx);
		cc.trm = fz_concat(fz_scale(recip, recip), trm);
		fz_moveto(ctx, cc.path, cc.trm.e, cc.trm.f);
		FT_Outline_Decompose(&face->glyph->outline, &outline_funcs, &cc);
		fz_closepath(ctx, cc.path);
	}
	fz_always(ctx)
	{
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "freetype cannot decompose outline");
		fz_free(ctx, cc.path);
		return NULL;
	}

	return cc.path;
}

/*
 * Type 3 fonts...
 */

/*
	Create a new (empty) type3 font.

	name: Name of font (or NULL).

	matrix: Font matrix.

	Returns a new font handle, or throws exception on
	allocation failure.
*/
fz_font *
fz_new_type3_font(fz_context *ctx, const char *name, fz_matrix matrix)
{
	fz_font *font;

	font = fz_new_font(ctx, name, 1, 256);
	fz_try(ctx)
	{
		font->t3procs = fz_calloc(ctx, 256, sizeof(fz_buffer*));
		font->t3lists = fz_calloc(ctx, 256, sizeof(fz_display_list*));
		font->t3widths = fz_calloc(ctx, 256, sizeof(float));
		font->t3flags = fz_calloc(ctx, 256, sizeof(unsigned short));
	}
	fz_catch(ctx)
	{
		fz_drop_font(ctx, font);
		fz_rethrow(ctx);
	}

	font->t3matrix = matrix;

	return font;
}

static void
fz_bound_t3_glyph(fz_context *ctx, fz_font *font, int gid)
{
	fz_display_list *list;
	fz_device *dev;

	list = font->t3lists[gid];
	if (!list)
	{
		font->bbox_table[gid] = fz_empty_rect;
		return;
	}

	dev = fz_new_bbox_device(ctx, &font->bbox_table[gid]);
	fz_try(ctx)
	{
		fz_run_display_list(ctx, list, dev, font->t3matrix, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	/* Update font bbox with glyph's computed bbox if the font bbox is invalid */
	if (font->flags.invalid_bbox)
		font->bbox = fz_union_rect(font->bbox, font->bbox_table[gid]);
}

void
fz_prepare_t3_glyph(fz_context *ctx, fz_font *font, int gid)
{
	fz_buffer *contents;
	fz_device *dev;
	fz_rect d1_rect;

	contents = font->t3procs[gid];
	if (!contents)
		return;

	/* We've not already loaded this one! */
	assert(font->t3lists[gid] == NULL);

	font->t3lists[gid] = fz_new_display_list(ctx, font->bbox);

	dev = fz_new_list_device(ctx, font->t3lists[gid]);
	dev->flags = FZ_DEVFLAG_FILLCOLOR_UNDEFINED |
			FZ_DEVFLAG_STROKECOLOR_UNDEFINED |
			FZ_DEVFLAG_STARTCAP_UNDEFINED |
			FZ_DEVFLAG_DASHCAP_UNDEFINED |
			FZ_DEVFLAG_ENDCAP_UNDEFINED |
			FZ_DEVFLAG_LINEJOIN_UNDEFINED |
			FZ_DEVFLAG_MITERLIMIT_UNDEFINED |
			FZ_DEVFLAG_LINEWIDTH_UNDEFINED;

	/* Avoid cycles in glyph content streams referring to the glyph itself.
	 * Remember to restore the content stream below, regardless of exceptions
	 * or a successful run of the glyph. */
	font->t3procs[gid] = NULL;

	fz_try(ctx)
	{
		font->t3run(ctx, font->t3doc, font->t3resources, contents, dev, fz_identity, NULL, NULL);
		fz_close_device(ctx, dev);
		font->t3flags[gid] = dev->flags;
		d1_rect = dev->d1_rect;
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		font->t3procs[gid] = contents;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
	if (fz_display_list_is_empty(ctx, font->t3lists[gid]))
	{
		/* If empty, no need for a huge bbox, especially as the logic
		 * in the 'else if' can make it huge. */
		font->bbox_table[gid].x0 = font->bbox.x0;
		font->bbox_table[gid].y0 = font->bbox.y0;
		font->bbox_table[gid].x1 = font->bbox.x0 + .00001f;
		font->bbox_table[gid].y1 = font->bbox.y0 + .00001f;
	}
	else if (font->t3flags[gid] & FZ_DEVFLAG_BBOX_DEFINED)
	{
		assert(font->bbox_table != NULL);
		assert(font->glyph_count > gid);
		font->bbox_table[gid] = fz_transform_rect(d1_rect, font->t3matrix);

		if (font->flags.invalid_bbox || !fz_contains_rect(font->bbox, d1_rect))
		{
			/* Either the font bbox is invalid, or the d1_rect returned is
			 * incompatible with it. Either way, don't trust the d1 rect
			 * and calculate it from the contents. */
			fz_bound_t3_glyph(ctx, font, gid);
		}
	}
	else
	{
		/* No bbox has been defined for this glyph, so compute it. */
		fz_bound_t3_glyph(ctx, font, gid);
	}
}

/*
	Run a glyph from a Type3 font to
	a given device.

	font: The font to find the glyph in.

	gid: The glyph to run.

	trm: The transform to apply.

	dev: The device to render onto.
*/
void
fz_run_t3_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_device *dev)
{
	fz_display_list *list;
	fz_matrix ctm;

	list = font->t3lists[gid];
	if (!list)
		return;

	ctm = fz_concat(font->t3matrix, trm);
	fz_run_display_list(ctx, list, dev, ctm, fz_infinite_rect, NULL);
}

fz_pixmap *
fz_render_t3_glyph_pixmap(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_colorspace *model, const fz_irect *scissor, int aa)
{
	fz_display_list *list;
	fz_rect bounds;
	fz_irect bbox;
	fz_device *dev = NULL;
	fz_pixmap *glyph;
	fz_pixmap *result = NULL;

	if (gid < 0 || gid > 255)
		return NULL;

	list = font->t3lists[gid];
	if (!list)
		return NULL;

	if (font->t3flags[gid] & FZ_DEVFLAG_MASK)
	{
		if (font->t3flags[gid] & FZ_DEVFLAG_COLOR)
			fz_warn(ctx, "type3 glyph claims to be both masked and colored");
		model = NULL;
	}
	else if (font->t3flags[gid] & FZ_DEVFLAG_COLOR)
	{
		if (!model)
			fz_warn(ctx, "colored type3 glyph wanted in masked context");
	}
	else
	{
		fz_warn(ctx, "type3 glyph doesn't specify masked or colored");
		model = NULL; /* Treat as masked */
	}

	bounds = fz_expand_rect(fz_bound_glyph(ctx, font, gid, trm), 1);
	bbox = fz_irect_from_rect(bounds);
	bbox = fz_intersect_irect(bbox, *scissor);

	/* Glyphs must always have alpha */
	glyph = fz_new_pixmap_with_bbox(ctx, model, bbox, NULL/* FIXME */, 1);

	fz_var(dev);
	fz_try(ctx)
	{
		fz_clear_pixmap(ctx, glyph);
		dev = fz_new_draw_device_type3(ctx, fz_identity, glyph);
		fz_run_t3_glyph(ctx, font, gid, trm, dev);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, glyph);
		fz_rethrow(ctx);
	}

	if (!model)
	{
		fz_try(ctx)
		{
			result = fz_alpha_from_gray(ctx, glyph);
		}
		fz_always(ctx)
		{
			fz_drop_pixmap(ctx, glyph);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}
	else
		result = glyph;

	return result;
}

fz_glyph *
fz_render_t3_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_colorspace *model, const fz_irect *scissor, int aa)
{
	fz_pixmap *pixmap = fz_render_t3_glyph_pixmap(ctx, font, gid, trm, model, scissor, aa);
	return fz_new_glyph_from_pixmap(ctx, pixmap);
}

void
fz_render_t3_glyph_direct(fz_context *ctx, fz_device *dev, fz_font *font, int gid, fz_matrix trm, void *gstate, fz_default_colorspaces *def_cs)
{
	fz_matrix ctm;
	void *contents;

	if (gid < 0 || gid > 255)
		return;

	contents = font->t3procs[gid];
	if (!contents)
		return;

	if (font->t3flags[gid] & FZ_DEVFLAG_MASK)
	{
		if (font->t3flags[gid] & FZ_DEVFLAG_COLOR)
			fz_warn(ctx, "type3 glyph claims to be both masked and colored");
	}
	else if (font->t3flags[gid] & FZ_DEVFLAG_COLOR)
	{
	}
	else
	{
		fz_warn(ctx, "type3 glyph doesn't specify masked or colored");
	}

	/* Avoid cycles in glyph content streams referring to the glyph itself.
	 * Remember to restore the content stream below, regardless of exceptions
	 * or a successful run of the glyph. */
	font->t3procs[gid] = NULL;

	fz_try(ctx)
	{
		ctm = fz_concat(font->t3matrix, trm);
		font->t3run(ctx, font->t3doc, font->t3resources, contents, dev, ctm, gstate, def_cs);
	}
	fz_always(ctx)
		font->t3procs[gid] = contents;
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
	Return a bbox for a given glyph in a font.

	font: The font to look for the glyph in.

	gid: The glyph to bound.

	trm: The matrix to apply to the glyph before bounding.

	r: Pointer to a fz_rect to use for storage.

	Returns r, after filling it in with the bounds of the given glyph.
*/
fz_rect
fz_bound_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm)
{
	fz_rect rect;
	if (font->bbox_table && gid < font->glyph_count)
	{
		/* If the bbox is infinite or empty, distrust it */
		if (fz_is_infinite_rect(font->bbox_table[gid]) || fz_is_empty_rect(font->bbox_table[gid]))
		{
			/* Get the real size from the glyph */
			if (font->ft_face)
				fz_bound_ft_glyph(ctx, font, gid);
			else if (font->t3lists)
				fz_bound_t3_glyph(ctx, font, gid);
			else
				/* If we can't get a real size, fall back to the font
				 * bbox. */
				font->bbox_table[gid] = font->bbox;
			/* If the real size came back as empty, then store it as
			 * a very small rectangle to avoid us calling this same
			 * check every time. */
			if (fz_is_empty_rect(font->bbox_table[gid]))
			{
				font->bbox_table[gid].x0 = 0;
				font->bbox_table[gid].y0 = 0;
				font->bbox_table[gid].x1 = 0.0000001f;
				font->bbox_table[gid].y1 = 0.0000001f;
			}
		}
		rect = font->bbox_table[gid];
	}
	else
	{
		/* fall back to font bbox */
		rect = font->bbox;
	}
	return fz_transform_rect(rect, trm);
}

fz_path *
fz_outline_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix ctm)
{
	if (!font->ft_face)
		return NULL;
	return fz_outline_ft_glyph(ctx, font, gid, ctm);
}

/*
	Determine if a given glyph in a font
	is cacheable. Certain glyphs in a type 3 font cannot safely
	be cached, as their appearance depends on the enclosing
	graphic state.

	font: The font to look for the glyph in.

	gif: The glyph to query.

	Returns non-zero if cacheable, 0 if not.
*/
int fz_glyph_cacheable(fz_context *ctx, fz_font *font, int gid)
{
	if (!font->t3procs || !font->t3flags || gid < 0 || gid >= font->glyph_count)
		return 1;
	return (font->t3flags[gid] & FZ_DEVFLAG_UNCACHEABLE) == 0;
}

static float
fz_advance_ft_glyph(fz_context *ctx, fz_font *font, int gid, int wmode)
{
	FT_Error fterr;
	FT_Fixed adv = 0;
	int mask;

	/* PDF and substitute font widths. */
	if (font->flags.ft_stretch)
	{
		if (font->width_table)
		{
			if (gid < font->width_count)
				return font->width_table[gid] / 1000.0f;
			return font->width_default / 1000.0f;
		}
	}

	mask = FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_IGNORE_TRANSFORM;
	if (wmode)
		mask |= FT_LOAD_VERTICAL_LAYOUT;
	fz_lock(ctx, FZ_LOCK_FREETYPE);
	fterr = FT_Get_Advance(font->ft_face, gid, mask, &adv);
	fz_unlock(ctx, FZ_LOCK_FREETYPE);
	if (fterr && fterr != FT_Err_Invalid_Argument)
	{
		fz_warn(ctx, "FT_Get_Advance(%s,%d): %s", font->name, gid, ft_error_string(fterr));
		if (font->width_table)
		{
			if (gid < font->width_count)
				return font->width_table[gid] / 1000.0f;
			return font->width_default / 1000.0f;
		}
	}
	return (float) adv / ((FT_Face)font->ft_face)->units_per_EM;
}

static float
fz_advance_t3_glyph(fz_context *ctx, fz_font *font, int gid)
{
	if (gid < 0 || gid > 255)
		return 0;
	return font->t3widths[gid];
}

/*
	Find the name of a glyph

	font: The font to look for the glyph in.

	glyph: The glyph id to look for.

	buf: Pointer to a buffer for the name to be inserted into.

	size: The size of the buffer.

	If a font contains a name table, then the name of the glyph
	will be returned in the supplied buffer. Otherwise a name
	is synthesised. The name will be truncated to fit in
	the buffer.
*/
void
fz_get_glyph_name(fz_context *ctx, fz_font *font, int glyph, char *buf, int size)
{
	FT_Face face = font->ft_face;
	if (face)
	{
		if (FT_HAS_GLYPH_NAMES(face))
		{
			int fterr = FT_Get_Glyph_Name(face, glyph, buf, size);
			if (fterr)
				fz_warn(ctx, "FT_Get_Glyph_Name(%s,%d): %s", font->name, glyph, ft_error_string(fterr));
		}
		else
			fz_snprintf(buf, size, "%d", glyph);
	}
	else
	{
		fz_snprintf(buf, size, "%d", glyph);
	}
}

/*
	Return the advance for a given glyph.

	font: The font to look for the glyph in.

	glyph: The glyph to find the advance for.

	wmode: 1 for vertical mode, 0 for horizontal.

	Returns the advance for the glyph.
*/
float
fz_advance_glyph(fz_context *ctx, fz_font *font, int gid, int wmode)
{
	if (font->ft_face)
	{
		if (wmode)
			return fz_advance_ft_glyph(ctx, font, gid, 1);
		if (gid >= 0 && gid < font->glyph_count && gid < MAX_ADVANCE_CACHE)
		{
			if (!font->advance_cache)
			{
				int i;
				font->advance_cache = Memento_label(fz_malloc_array(ctx, font->glyph_count, float), "font_advance_cache");
				for (i = 0; i < font->glyph_count; ++i)
					font->advance_cache[i] = fz_advance_ft_glyph(ctx, font, i, 0);
			}
			return font->advance_cache[gid];
		}

		return fz_advance_ft_glyph(ctx, font, gid, 0);
	}
	if (font->t3procs)
		return fz_advance_t3_glyph(ctx, font, gid);
	return 0;
}

/*
	Find the glyph id for a given unicode
	character within a font.

	font: The font to look for the unicode character in.

	unicode: The unicode character to encode.

	Returns the glyph id for the given unicode value, or 0 if
	unknown.
*/
int
fz_encode_character(fz_context *ctx, fz_font *font, int ucs)
{
	if (font->ft_face)
	{
		if (ucs >= 0 && ucs < 0x10000)
		{
			int pg = ucs >> 8;
			int ix = ucs & 0xFF;
			if (!font->encoding_cache[pg])
			{
				int i;
				font->encoding_cache[pg] = fz_malloc_array(ctx, 256, uint16_t);
				for (i = 0; i < 256; ++i)
					font->encoding_cache[pg][i] = FT_Get_Char_Index(font->ft_face, (pg << 8) + i);
			}
			return font->encoding_cache[pg][ix];
		}
		return FT_Get_Char_Index(font->ft_face, ucs);
	}
	return ucs;
}

/* Encode character, preferring small-caps variant if available. */
int
fz_encode_character_sc(fz_context *ctx, fz_font *font, int unicode)
{
	if (font->ft_face)
	{
		int cat = ucdn_get_general_category(unicode);
		if (cat == UCDN_GENERAL_CATEGORY_LL || cat == UCDN_GENERAL_CATEGORY_LT)
		{
			int glyph;
			const char *name;
			char buf[20];

			name = fz_glyph_name_from_unicode_sc(unicode);
			if (name)
			{
				glyph = FT_Get_Name_Index(font->ft_face, (char*)name);
				if (glyph > 0)
					return glyph;
			}

			sprintf(buf, "uni%04X.sc", unicode);
			glyph = FT_Get_Name_Index(font->ft_face, buf);
			if (glyph > 0)
				return glyph;
		}
	}
	return fz_encode_character(ctx, font, unicode);
}

int
fz_encode_character_by_glyph_name(fz_context *ctx, fz_font *font, const char *glyphname)
{
	int glyph = 0;
	if (font->ft_face)
	{
		glyph = ft_name_index(font->ft_face, glyphname);
		if (glyph == 0)
			glyph = ft_char_index(font->ft_face, fz_unicode_from_glyph_name(glyphname));
	}
	// TODO: type3 fonts (not needed for now)
	return glyph;
}

/*
	Find the glyph id for
	a given unicode character within a font, falling back to
	an alternative if not found.

	font: The font to look for the unicode character in.

	unicode: The unicode character to encode.

	script: The script in use.

	language: The language in use.

	out_font: The font handle in which the given glyph represents
	the requested unicode character. The caller does not own the
	reference it is passed, so should call fz_keep_font if it is
	not simply to be used immediately.

	Returns the glyph id for the given unicode value in the supplied
	font (and sets *out_font to font) if it is present. Otherwise
	an alternative fallback font (based on script/language) is
	searched for. If the glyph is found therein, *out_font is set
	to this reference, and the glyph reference is returned. If it
	cannot be found anywhere, the function returns 0.
*/
/* FIXME: This should take language too eventually, to allow for fonts where we can select different
 * languages using opentype features. */
int
fz_encode_character_with_fallback(fz_context *ctx, fz_font *user_font, int unicode, int script, int language, fz_font **out_font)
{
	fz_font *font;
	int gid;

	gid = fz_encode_character(ctx, user_font, unicode);
	if (gid > 0)
		return *out_font = user_font, gid;

	if (script == 0)
		script = ucdn_get_script(unicode);

	/* Fix for ideographic/halfwidth/fullwidth punctuation forms. */
	if ((unicode >= 0x3000 && unicode <= 0x303F) || (unicode >= 0xFF00 && unicode <= 0xFFEF))
	{
		if (script != UCDN_SCRIPT_HANGUL &&
				script != UCDN_SCRIPT_HIRAGANA &&
				script != UCDN_SCRIPT_KATAKANA &&
				script != UCDN_SCRIPT_BOPOMOFO)
			script = UCDN_SCRIPT_HAN;
	}

	font = fz_load_fallback_font(ctx, script, language, user_font->flags.is_serif, user_font->flags.is_bold, user_font->flags.is_italic);
	if (font)
	{
		gid = fz_encode_character(ctx, font, unicode);
		if (gid > 0)
			return *out_font = font, gid;
	}

#ifndef TOFU_CJK_LANG
	if (script == UCDN_SCRIPT_HAN)
	{
		font = fz_load_fallback_font(ctx, script, FZ_LANG_zh_Hant, user_font->flags.is_serif, user_font->flags.is_bold, user_font->flags.is_italic);
		if (font)
		{
			gid = fz_encode_character(ctx, font, unicode);
			if (gid > 0)
				return *out_font = font, gid;
		}
		font = fz_load_fallback_font(ctx, script, FZ_LANG_ja, user_font->flags.is_serif, user_font->flags.is_bold, user_font->flags.is_italic);
		if (font)
		{
			gid = fz_encode_character(ctx, font, unicode);
			if (gid > 0)
				return *out_font = font, gid;
		}
		font = fz_load_fallback_font(ctx, script, FZ_LANG_ko, user_font->flags.is_serif, user_font->flags.is_bold, user_font->flags.is_italic);
		if (font)
		{
			gid = fz_encode_character(ctx, font, unicode);
			if (gid > 0)
				return *out_font = font, gid;
		}
		font = fz_load_fallback_font(ctx, script, FZ_LANG_zh_Hans, user_font->flags.is_serif, user_font->flags.is_bold, user_font->flags.is_italic);
		if (font)
		{
			gid = fz_encode_character(ctx, font, unicode);
			if (gid > 0)
				return *out_font = font, gid;
		}
	}
#endif

	font = fz_load_fallback_math_font(ctx);
	if (font)
	{
		gid = fz_encode_character(ctx, font, unicode);
		if (gid > 0)
			return *out_font = font, gid;
	}

	font = fz_load_fallback_music_font(ctx);
	if (font)
	{
		gid = fz_encode_character(ctx, font, unicode);
		if (gid > 0)
			return *out_font = font, gid;
	}

	font = fz_load_fallback_symbol1_font(ctx);
	if (font)
	{
		gid = fz_encode_character(ctx, font, unicode);
		if (gid > 0)
			return *out_font = font, gid;
	}

	font = fz_load_fallback_symbol2_font(ctx);
	if (font)
	{
		gid = fz_encode_character(ctx, font, unicode);
		if (gid > 0)
			return *out_font = font, gid;
	}

	font = fz_load_fallback_emoji_font(ctx);
	if (font)
	{
		gid = fz_encode_character(ctx, font, unicode);
		if (gid > 0)
			return *out_font = font, gid;
	}

	font = fz_new_base14_font(ctx, "Symbol");
	if (font)
	{
		fz_drop_font(ctx, font); /* it's cached in the font context, return a borrowed pointer */
		gid = fz_encode_character(ctx, font, unicode);
		if (gid > 0)
			return *out_font = font, gid;
	}

	return *out_font = user_font, 0;
}

int fz_font_is_bold(fz_context *ctx, fz_font *font)
{
	return font ? font->flags.is_bold : 0;
}

int fz_font_is_italic(fz_context *ctx, fz_font *font)
{
	return font ? font->flags.is_italic : 0;
}

int fz_font_is_serif(fz_context *ctx, fz_font *font)
{
	return font ? font->flags.is_serif : 0;
}

int fz_font_is_monospaced(fz_context *ctx, fz_font *font)
{
	return font ? font->flags.is_mono : 0;
}

/*
	Retrieve a pointer to the name of the font.

	font: The font to query.

	Returns a pointer to an internal copy of the font name.
	Will never be NULL, but may be the empty string.
*/
const char *fz_font_name(fz_context *ctx, fz_font *font)
{
	return font ? font->name : "";
}

/*
	Retrieve the Type3 procs
	for a font.

	font: The font to query

	Returns the t3_procs pointer. Will be NULL for a
	non type-3 font.
*/
fz_buffer **fz_font_t3_procs(fz_context *ctx, fz_font *font)
{
	return font ? font->t3procs : NULL;
}

/*
	Retrieve a pointer to the font bbox.

	font: The font to query.

	Returns a pointer to the font bbox (or NULL if the
	font is NULL).
*/
fz_rect fz_font_bbox(fz_context *ctx, fz_font *font)
{
	return font->bbox;
}

/*
	Retrieve the FT_Face handle
	for the font.

	font: The font to query

	Returns the FT_Face handle for the font, or NULL
	if not a freetype handled font. (Cast to void *
	to avoid nasty header exposure).
*/
void *fz_font_ft_face(fz_context *ctx, fz_font *font)
{
	return font ? font->ft_face : NULL;
}

/*
	Retrieve a pointer to the font flags
	for a given font. These can then be updated as required.

	font: The font to query

	Returns a pointer to the flags structure (or NULL, if
	the font is NULL).
*/
fz_font_flags_t *fz_font_flags(fz_font *font)
{
	return font ? &font->flags : NULL;
}

/*
	Retrieve a pointer to the shaper data
	structure for the given font.

	font: The font to query.

	Returns a pointer to the shaper data structure (or NULL if
	font is NULL).
*/
fz_shaper_data_t *fz_font_shaper_data(fz_context *ctx, fz_font *font)
{
	return font ? &font->shaper_data : NULL;
}

void fz_font_digest(fz_context *ctx, fz_font *font, unsigned char digest[16])
{
	if (!font->buffer)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no font file for digest");
	if (!font->has_digest)
	{
		fz_md5_buffer(ctx, font->buffer, font->digest);
		font->has_digest = 1;
	}
	memcpy(digest, font->digest, 16);
}
