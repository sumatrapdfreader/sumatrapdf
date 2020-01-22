#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include "../fitz/fitz-imp.h"

#include <assert.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#ifdef FT_FONT_FORMATS_H
#include FT_FONT_FORMATS_H
#else
#include FT_XFREE86_H
#endif
#include FT_TRUETYPE_TABLES_H

#ifndef FT_SFNT_HEAD
#define FT_SFNT_HEAD ft_sfnt_head
#endif

void
pdf_load_encoding(const char **estrings, const char *encoding)
{
	const char * const *bstrings = NULL;
	int i;

	if (!strcmp(encoding, "StandardEncoding"))
		bstrings = fz_glyph_name_from_adobe_standard;
	if (!strcmp(encoding, "MacRomanEncoding"))
		bstrings = fz_glyph_name_from_mac_roman;
	if (!strcmp(encoding, "MacExpertEncoding"))
		bstrings = fz_glyph_name_from_mac_expert;
	if (!strcmp(encoding, "WinAnsiEncoding"))
		bstrings = fz_glyph_name_from_win_ansi;

	if (bstrings)
		for (i = 0; i < 256; i++)
			estrings[i] = bstrings[i];
}

static void pdf_load_font_descriptor(fz_context *ctx, pdf_document *doc, pdf_font_desc *fontdesc, pdf_obj *dict,
	const char *collection, const char *basefont, int iscidfont);

static const char *base_font_names[][10] =
{
	{ "Courier", "CourierNew", "CourierNewPSMT", NULL },
	{ "Courier-Bold", "CourierNew,Bold", "Courier,Bold",
		"CourierNewPS-BoldMT", "CourierNew-Bold", NULL },
	{ "Courier-Oblique", "CourierNew,Italic", "Courier,Italic",
		"CourierNewPS-ItalicMT", "CourierNew-Italic", NULL },
	{ "Courier-BoldOblique", "CourierNew,BoldItalic", "Courier,BoldItalic",
		"CourierNewPS-BoldItalicMT", "CourierNew-BoldItalic", NULL },
	{ "Helvetica", "ArialMT", "Arial", NULL },
	{ "Helvetica-Bold", "Arial-BoldMT", "Arial,Bold", "Arial-Bold",
		"Helvetica,Bold", NULL },
	{ "Helvetica-Oblique", "Arial-ItalicMT", "Arial,Italic", "Arial-Italic",
		"Helvetica,Italic", "Helvetica-Italic", NULL },
	{ "Helvetica-BoldOblique", "Arial-BoldItalicMT",
		"Arial,BoldItalic", "Arial-BoldItalic",
		"Helvetica,BoldItalic", "Helvetica-BoldItalic", NULL },
	{ "Times-Roman", "TimesNewRomanPSMT", "TimesNewRoman",
		"TimesNewRomanPS", NULL },
	{ "Times-Bold", "TimesNewRomanPS-BoldMT", "TimesNewRoman,Bold",
		"TimesNewRomanPS-Bold", "TimesNewRoman-Bold", NULL },
	{ "Times-Italic", "TimesNewRomanPS-ItalicMT", "TimesNewRoman,Italic",
		"TimesNewRomanPS-Italic", "TimesNewRoman-Italic", NULL },
	{ "Times-BoldItalic", "TimesNewRomanPS-BoldItalicMT",
		"TimesNewRoman,BoldItalic", "TimesNewRomanPS-BoldItalic",
		"TimesNewRoman-BoldItalic", NULL },
	{ "Symbol", "Symbol,Italic", "Symbol,Bold", "Symbol,BoldItalic",
		"SymbolMT", "SymbolMT,Italic", "SymbolMT,Bold", "SymbolMT,BoldItalic", NULL },
	{ "ZapfDingbats", NULL }
};

const unsigned char *
pdf_lookup_substitute_font(fz_context *ctx, int mono, int serif, int bold, int italic, int *len)
{
	if (mono) {
		if (bold) {
			if (italic) return fz_lookup_base14_font(ctx, "Courier-BoldOblique", len);
			else return fz_lookup_base14_font(ctx, "Courier-Bold", len);
		} else {
			if (italic) return fz_lookup_base14_font(ctx, "Courier-Oblique", len);
			else return fz_lookup_base14_font(ctx, "Courier", len);
		}
	} else if (serif) {
		if (bold) {
			if (italic) return fz_lookup_base14_font(ctx, "Times-BoldItalic", len);
			else return fz_lookup_base14_font(ctx, "Times-Bold", len);
		} else {
			if (italic) return fz_lookup_base14_font(ctx, "Times-Italic", len);
			else return fz_lookup_base14_font(ctx, "Times-Roman", len);
		}
	} else {
		if (bold) {
			if (italic) return fz_lookup_base14_font(ctx, "Helvetica-BoldOblique", len);
			else return fz_lookup_base14_font(ctx, "Helvetica-Bold", len);
		} else {
			if (italic) return fz_lookup_base14_font(ctx, "Helvetica-Oblique", len);
			else return fz_lookup_base14_font(ctx, "Helvetica", len);
		}
	}
}

static int is_dynalab(char *name)
{
	if (strstr(name, "HuaTian"))
		return 1;
	if (strstr(name, "MingLi"))
		return 1;
	if ((strstr(name, "DF") == name) || strstr(name, "+DF"))
		return 1;
	if ((strstr(name, "DLC") == name) || strstr(name, "+DLC"))
		return 1;
	return 0;
}

static int strcmp_ignore_space(const char *a, const char *b)
{
	while (1)
	{
		while (*a == ' ')
			a++;
		while (*b == ' ')
			b++;
		if (*a != *b)
			return 1;
		if (*a == 0)
			return *a != *b;
		if (*b == 0)
			return *a != *b;
		a++;
		b++;
	}
}

const char *pdf_clean_font_name(const char *fontname)
{
	int i, k;
	for (i = 0; i < (int)nelem(base_font_names); i++)
		for (k = 0; base_font_names[i][k]; k++)
			if (!strcmp_ignore_space(base_font_names[i][k], fontname))
				return base_font_names[i][0];
	return fontname;
}

/*
 * FreeType and Rendering glue
 */

enum { UNKNOWN, TYPE1, TRUETYPE };

static int ft_kind(FT_Face face)
{
#ifdef FT_FONT_FORMATS_H
	const char *kind = FT_Get_Font_Format(face);
#else
	const char *kind = FT_Get_X11_Font_Format(face);
#endif
	if (!strcmp(kind, "TrueType")) return TRUETYPE;
	if (!strcmp(kind, "Type 1")) return TYPE1;
	if (!strcmp(kind, "CFF")) return TYPE1;
	if (!strcmp(kind, "CID Type 1")) return TYPE1;
	return UNKNOWN;
}

static int ft_cid_to_gid(pdf_font_desc *fontdesc, int cid)
{
	if (fontdesc->to_ttf_cmap)
	{
		cid = pdf_lookup_cmap(fontdesc->to_ttf_cmap, cid);

		/* vertical presentation forms */
		if (fontdesc->font->flags.ft_substitute && fontdesc->wmode)
		{
			switch (cid)
			{
			case 0x0021: cid = 0xFE15; break; /* ! */
			case 0x0028: cid = 0xFE35; break; /* ( */
			case 0x0029: cid = 0xFE36; break; /* ) */
			case 0x002C: cid = 0xFE10; break; /* , */
			case 0x003A: cid = 0xFE13; break; /* : */
			case 0x003B: cid = 0xFE14; break; /* ; */
			case 0x003F: cid = 0xFE16; break; /* ? */
			case 0x005B: cid = 0xFE47; break; /* [ */
			case 0x005D: cid = 0xFE48; break; /* ] */
			case 0x005F: cid = 0xFE33; break; /* _ */
			case 0x007B: cid = 0xFE37; break; /* { */
			case 0x007D: cid = 0xFE38; break; /* } */
			case 0x2013: cid = 0xFE32; break; /* EN DASH */
			case 0x2014: cid = 0xFE31; break; /* EM DASH */
			case 0x2025: cid = 0xFE30; break; /* TWO DOT LEADER */
			case 0x2026: cid = 0xFE19; break; /* HORIZONTAL ELLIPSIS */
			case 0x3001: cid = 0xFE11; break; /* IDEOGRAPHIC COMMA */
			case 0x3002: cid = 0xFE12; break; /* IDEOGRAPHIC FULL STOP */
			case 0x3008: cid = 0xFE3F; break; /* OPENING ANGLE BRACKET */
			case 0x3009: cid = 0xFE40; break; /* CLOSING ANGLE BRACKET */
			case 0x300A: cid = 0xFE3D; break; /* LEFT DOUBLE ANGLE BRACKET */
			case 0x300B: cid = 0xFE3E; break; /* RIGHT DOUBLE ANGLE BRACKET */
			case 0x300C: cid = 0xFE41; break; /* LEFT CORNER BRACKET */
			case 0x300D: cid = 0xFE42; break; /* RIGHT CORNER BRACKET */
			case 0x300E: cid = 0xFE43; break; /* LEFT WHITE CORNER BRACKET */
			case 0x300F: cid = 0xFE44; break; /* RIGHT WHITE CORNER BRACKET */
			case 0x3010: cid = 0xFE3B; break; /* LEFT BLACK LENTICULAR BRACKET */
			case 0x3011: cid = 0xFE3C; break; /* RIGHT BLACK LENTICULAR BRACKET */
			case 0x3014: cid = 0xFE39; break; /* LEFT TORTOISE SHELL BRACKET */
			case 0x3015: cid = 0xFE3A; break; /* RIGHT TORTOISE SHELL BRACKET */
			case 0x3016: cid = 0xFE17; break; /* LEFT WHITE LENTICULAR BRACKET */
			case 0x3017: cid = 0xFE18; break; /* RIGHT WHITE LENTICULAR BRACKET */

			case 0xFF01: cid = 0xFE15; break; /* FULLWIDTH EXCLAMATION MARK */
			case 0xFF08: cid = 0xFE35; break; /* FULLWIDTH LEFT PARENTHESIS */
			case 0xFF09: cid = 0xFE36; break; /* FULLWIDTH RIGHT PARENTHESIS */
			case 0xFF0C: cid = 0xFE10; break; /* FULLWIDTH COMMA */
			case 0xFF1A: cid = 0xFE13; break; /* FULLWIDTH COLON */
			case 0xFF1B: cid = 0xFE14; break; /* FULLWIDTH SEMICOLON */
			case 0xFF1F: cid = 0xFE16; break; /* FULLWIDTH QUESTION MARK */
			case 0xFF3B: cid = 0xFE47; break; /* FULLWIDTH LEFT SQUARE BRACKET */
			case 0xFF3D: cid = 0xFE48; break; /* FULLWIDTH RIGHT SQUARE BRACKET */
			case 0xFF3F: cid = 0xFE33; break; /* FULLWIDTH LOW LINE */
			case 0xFF5B: cid = 0xFE37; break; /* FULLWIDTH LEFT CURLY BRACKET */
			case 0xFF5D: cid = 0xFE38; break; /* FULLWIDTH RIGHT CURLY BRACKET */

			case 0x30FC: cid = 0xFE31; break; /* KATAKANA-HIRAGANA PROLONGED SOUND MARK */
			case 0xFF0D: cid = 0xFE31; break; /* FULLWIDTH HYPHEN-MINUS */
			}
		}

		return ft_char_index(fontdesc->font->ft_face, cid);
	}

	if (fontdesc->cid_to_gid && (size_t)cid < fontdesc->cid_to_gid_len && cid >= 0)
		return fontdesc->cid_to_gid[cid];

	return cid;
}

int
pdf_font_cid_to_gid(fz_context *ctx, pdf_font_desc *fontdesc, int cid)
{
	if (fontdesc->font->ft_face)
		return ft_cid_to_gid(fontdesc, cid);
	return cid;
}

static int ft_width(fz_context *ctx, pdf_font_desc *fontdesc, int cid)
{
	int mask = FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM;
	int gid = ft_cid_to_gid(fontdesc, cid);
	FT_Fixed adv = 0;
	int fterr;
	FT_Face face = fontdesc->font->ft_face;
	FT_UShort units_per_EM;

	fterr = FT_Get_Advance(face, gid, mask, &adv);
	if (fterr && fterr != FT_Err_Invalid_Argument)
		fz_warn(ctx, "FT_Get_Advance(%d): %s", gid, ft_error_string(fterr));

	units_per_EM = face->units_per_EM;
	if (units_per_EM == 0)
		units_per_EM = 2048;

	return adv * 1000 / units_per_EM;
}

static const struct { int code; const char *name; } mre_diff_table[] =
{
	{ 173, "notequal" },
	{ 176, "infinity" },
	{ 178, "lessequal" },
	{ 179, "greaterequal" },
	{ 182, "partialdiff" },
	{ 183, "summation" },
	{ 184, "product" },
	{ 185, "pi" },
	{ 186, "integral" },
	{ 189, "Omega" },
	{ 195, "radical" },
	{ 197, "approxequal" },
	{ 198, "Delta" },
	{ 215, "lozenge" },
	{ 219, "Euro" },
	{ 240, "apple" },
};

static int lookup_mre_code(const char *name)
{
	int i;
	for (i = 0; i < (int)nelem(mre_diff_table); ++i)
		if (!strcmp(name, mre_diff_table[i].name))
			return mre_diff_table[i].code;
	for (i = 0; i < 256; i++)
		if (fz_glyph_name_from_mac_roman[i] && !strcmp(name, fz_glyph_name_from_mac_roman[i]))
			return i;
	return -1;
}

static int ft_find_glyph_by_unicode_name(FT_Face face, const char *name)
{
	int unicode, glyph;

	/* Prefer exact unicode match if available. */
	unicode = fz_unicode_from_glyph_name_strict(name);
	if (unicode > 0)
	{
		glyph = ft_char_index(face, unicode);
		if (glyph > 0)
			return glyph;
	}

	/* Fall back to font glyph name if we can. */
	glyph = ft_name_index(face, name);
	if (glyph > 0)
		return glyph;

	/* Fuzzy unicode match as last attempt. */
	unicode = fz_unicode_from_glyph_name(name);
	if (unicode > 0)
		return ft_char_index(face, unicode);

	/* Failed. */
	return 0;
}

/*
 * Load font files.
 */

static void
pdf_load_builtin_font(fz_context *ctx, pdf_font_desc *fontdesc, const char *fontname, int has_descriptor)
{
	FT_Face face;
	const char *clean_name = pdf_clean_font_name(fontname);
	if (clean_name == fontname)
		clean_name = "Times-Roman";

	fontdesc->font = fz_load_system_font(ctx, fontname, 0, 0, !has_descriptor);
	if (!fontdesc->font)
	{
		const unsigned char *data;
		int len;

		data = fz_lookup_base14_font(ctx, clean_name, &len);
		if (!data)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot find builtin font: '%s'", fontname);

		fontdesc->font = fz_new_font_from_memory(ctx, fontname, data, len, 0, 1);
		fontdesc->font->flags.is_serif = !!strstr(clean_name, "Times");
	}

	if (!strcmp(clean_name, "Symbol") || !strcmp(clean_name, "ZapfDingbats"))
		fontdesc->flags |= PDF_FD_SYMBOLIC;

	face = fontdesc->font->ft_face;
	fontdesc->ascent = 1000.0f * face->ascender / face->units_per_EM;
	fontdesc->descent = 1000.0f * face->descender / face->units_per_EM;
}

static void
pdf_load_substitute_font(fz_context *ctx, pdf_font_desc *fontdesc, const char *fontname, int mono, int serif, int bold, int italic)
{
	fontdesc->font = fz_load_system_font(ctx, fontname, bold, italic, 0);
	if (!fontdesc->font)
	{
		const unsigned char *data;
		int len;

		data = pdf_lookup_substitute_font(ctx, mono, serif, bold, italic, &len);
		if (!data)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot find substitute font");

		fontdesc->font = fz_new_font_from_memory(ctx, fontname, data, len, 0, 1);
		fontdesc->font->flags.fake_bold = bold && !fontdesc->font->flags.is_bold;
		fontdesc->font->flags.fake_italic = italic && !fontdesc->font->flags.is_italic;

		fontdesc->font->flags.is_mono = mono;
		fontdesc->font->flags.is_serif = serif;
		fontdesc->font->flags.is_bold = bold;
		fontdesc->font->flags.is_italic = italic;
	}

	fontdesc->font->flags.ft_substitute = 1;
	fontdesc->font->flags.ft_stretch = 1;
}

static void
pdf_load_substitute_cjk_font(fz_context *ctx, pdf_font_desc *fontdesc, const char *fontname, int ros, int serif)
{
	fontdesc->font = fz_load_system_cjk_font(ctx, fontname, ros, serif);
	if (!fontdesc->font)
	{
		const unsigned char *data;
		int size;
		int subfont;

		data = fz_lookup_cjk_font(ctx, ros, &size, &subfont);
		if (!data)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot find builtin CJK font");

		/* A glyph bbox cache is too big for CJK fonts. */
		fontdesc->font = fz_new_font_from_memory(ctx, fontname, data, size, subfont, 0);
	}

	fontdesc->font->flags.ft_substitute = 1;
	fontdesc->font->flags.ft_stretch = 0;
}

static void
pdf_load_system_font(fz_context *ctx, pdf_font_desc *fontdesc, const char *fontname, const char *collection)
{
	int bold = 0;
	int italic = 0;
	int serif = 0;
	int mono = 0;

	if (strstr(fontname, "Bold"))
		bold = 1;
	if (strstr(fontname, "Italic"))
		italic = 1;
	if (strstr(fontname, "Oblique"))
		italic = 1;

	if (fontdesc->flags & PDF_FD_FIXED_PITCH)
		mono = 1;
	if (fontdesc->flags & PDF_FD_SERIF)
		serif = 1;
	if (fontdesc->flags & PDF_FD_ITALIC)
		italic = 1;
	if (fontdesc->flags & PDF_FD_FORCE_BOLD)
		bold = 1;

	if (collection)
	{
		if (!strcmp(collection, "Adobe-CNS1"))
			pdf_load_substitute_cjk_font(ctx, fontdesc, fontname, FZ_ADOBE_CNS, serif);
		else if (!strcmp(collection, "Adobe-GB1"))
			pdf_load_substitute_cjk_font(ctx, fontdesc, fontname, FZ_ADOBE_GB, serif);
		else if (!strcmp(collection, "Adobe-Japan1"))
			pdf_load_substitute_cjk_font(ctx, fontdesc, fontname, FZ_ADOBE_JAPAN, serif);
		else if (!strcmp(collection, "Adobe-Korea1"))
			pdf_load_substitute_cjk_font(ctx, fontdesc, fontname, FZ_ADOBE_KOREA, serif);
		else
		{
			if (strcmp(collection, "Adobe-Identity") != 0)
				fz_warn(ctx, "unknown cid collection: %s", collection);
			pdf_load_substitute_font(ctx, fontdesc, fontname, mono, serif, bold, italic);
		}
	}
	else
	{
		pdf_load_substitute_font(ctx, fontdesc, fontname, mono, serif, bold, italic);
	}
}

static void
pdf_load_embedded_font(fz_context *ctx, pdf_document *doc, pdf_font_desc *fontdesc, const char *fontname, pdf_obj *stmref)
{
	fz_buffer *buf;

	buf = pdf_load_stream(ctx, stmref);
	fz_try(ctx)
		fontdesc->font = fz_new_font_from_buffer(ctx, fontname, buf, 0, 1);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);

	fontdesc->size += fz_buffer_storage(ctx, buf, NULL);
	fontdesc->is_embedded = 1;
}

/*
 * Create and destroy
 */

pdf_font_desc *
pdf_keep_font(fz_context *ctx, pdf_font_desc *fontdesc)
{
	return fz_keep_storable(ctx, &fontdesc->storable);
}

void
pdf_drop_font(fz_context *ctx, pdf_font_desc *fontdesc)
{
	fz_drop_storable(ctx, &fontdesc->storable);
}

static void
pdf_drop_font_imp(fz_context *ctx, fz_storable *fontdesc_)
{
	pdf_font_desc *fontdesc = (pdf_font_desc *)fontdesc_;

	fz_drop_font(ctx, fontdesc->font);
	pdf_drop_cmap(ctx, fontdesc->encoding);
	pdf_drop_cmap(ctx, fontdesc->to_ttf_cmap);
	pdf_drop_cmap(ctx, fontdesc->to_unicode);
	fz_free(ctx, fontdesc->cid_to_gid);
	fz_free(ctx, fontdesc->cid_to_ucs);
	fz_free(ctx, fontdesc->hmtx);
	fz_free(ctx, fontdesc->vmtx);
	fz_free(ctx, fontdesc);
}

pdf_font_desc *
pdf_new_font_desc(fz_context *ctx)
{
	pdf_font_desc *fontdesc;

	fontdesc = fz_malloc_struct(ctx, pdf_font_desc);
	FZ_INIT_STORABLE(fontdesc, 1, pdf_drop_font_imp);
	fontdesc->size = sizeof(pdf_font_desc);

	fontdesc->font = NULL;

	fontdesc->flags = 0;
	fontdesc->italic_angle = 0;
	fontdesc->ascent = 800;
	fontdesc->descent = -200;
	fontdesc->cap_height = 800;
	fontdesc->x_height = 500;
	fontdesc->missing_width = 0;

	fontdesc->encoding = NULL;
	fontdesc->to_ttf_cmap = NULL;
	fontdesc->cid_to_gid_len = 0;
	fontdesc->cid_to_gid = NULL;

	fontdesc->to_unicode = NULL;
	fontdesc->cid_to_ucs_len = 0;
	fontdesc->cid_to_ucs = NULL;

	fontdesc->wmode = 0;

	fontdesc->hmtx_cap = 0;
	fontdesc->vmtx_cap = 0;
	fontdesc->hmtx_len = 0;
	fontdesc->vmtx_len = 0;
	fontdesc->hmtx = NULL;
	fontdesc->vmtx = NULL;

	fontdesc->dhmtx.lo = 0x0000;
	fontdesc->dhmtx.hi = 0xFFFF;
	fontdesc->dhmtx.w = 1000;

	fontdesc->dvmtx.lo = 0x0000;
	fontdesc->dvmtx.hi = 0xFFFF;
	fontdesc->dvmtx.x = 0;
	fontdesc->dvmtx.y = 880;
	fontdesc->dvmtx.w = -1000;

	fontdesc->is_embedded = 0;

	return fontdesc;
}

/*
 * Simple fonts (Type1 and TrueType)
 */

static FT_CharMap
select_type1_cmap(FT_Face face)
{
	int i;
	for (i = 0; i < face->num_charmaps; i++)
		if (face->charmaps[i]->platform_id == 7)
			return face->charmaps[i];
	if (face->num_charmaps > 0)
		return face->charmaps[0];
	return NULL;
}

static FT_CharMap
select_truetype_cmap(FT_Face face, int symbolic)
{
	int i;

	/* First look for a Microsoft symbolic cmap, if applicable */
	if (symbolic)
	{
		for (i = 0; i < face->num_charmaps; i++)
			if (face->charmaps[i]->platform_id == 3 && face->charmaps[i]->encoding_id == 0)
				return face->charmaps[i];
	}

	/* Then look for a Microsoft Unicode cmap */
	for (i = 0; i < face->num_charmaps; i++)
		if (face->charmaps[i]->platform_id == 3 && face->charmaps[i]->encoding_id == 1)
			if (FT_Get_CMap_Format(face->charmaps[i]) != -1)
				return face->charmaps[i];

	/* Finally look for an Apple MacRoman cmap */
	for (i = 0; i < face->num_charmaps; i++)
		if (face->charmaps[i]->platform_id == 1 && face->charmaps[i]->encoding_id == 0)
			if (FT_Get_CMap_Format(face->charmaps[i]) != -1)
				return face->charmaps[i];

	if (face->num_charmaps > 0)
		if (FT_Get_CMap_Format(face->charmaps[0]) != -1)
			return face->charmaps[0];
	return NULL;
}

static FT_CharMap
select_unknown_cmap(FT_Face face)
{
	if (face->num_charmaps > 0)
		return face->charmaps[0];
	return NULL;
}

static pdf_font_desc *
pdf_load_simple_font(fz_context *ctx, pdf_document *doc, pdf_obj *dict)
{
	const char *basefont;
	pdf_obj *descriptor;
	pdf_obj *encoding;
	pdf_obj *widths;
	unsigned short *etable = NULL;
	pdf_font_desc *fontdesc = NULL;
	pdf_obj *subtype;
	FT_Face face;
	FT_CharMap cmap;
	int symbolic;
	int kind;
	int glyph;

	const char *estrings[256];
	char ebuffer[256][32];
	int i, k, n;
	int fterr;
	int has_lock = 0;

	fz_var(fontdesc);
	fz_var(etable);
	fz_var(has_lock);

	/* Load font file */
	fz_try(ctx)
	{
		fontdesc = pdf_new_font_desc(ctx);

		basefont = pdf_to_name(ctx, pdf_dict_get(ctx, dict, PDF_NAME(BaseFont)));

		descriptor = pdf_dict_get(ctx, dict, PDF_NAME(FontDescriptor));
		if (descriptor)
			pdf_load_font_descriptor(ctx, doc, fontdesc, descriptor, NULL, basefont, 0);
		else
			pdf_load_builtin_font(ctx, fontdesc, basefont, 0);

		/* Some chinese documents mistakenly consider WinAnsiEncoding to be codepage 936 */
		if (descriptor && pdf_is_string(ctx, pdf_dict_get(ctx, descriptor, PDF_NAME(FontName))) &&
			!pdf_dict_get(ctx, dict, PDF_NAME(ToUnicode)) &&
			pdf_name_eq(ctx, pdf_dict_get(ctx, dict, PDF_NAME(Encoding)), PDF_NAME(WinAnsiEncoding)) &&
			pdf_dict_get_int(ctx, descriptor, PDF_NAME(Flags)) == 4)
		{
			char *cp936fonts[] = {
				"\xCB\xCE\xCC\xE5", "SimSun,Regular",
				"\xBA\xDA\xCC\xE5", "SimHei,Regular",
				"\xBF\xAC\xCC\xE5_GB2312", "SimKai,Regular",
				"\xB7\xC2\xCB\xCE_GB2312", "SimFang,Regular",
				"\xC1\xA5\xCA\xE9", "SimLi,Regular",
				NULL
			};
			for (i = 0; cp936fonts[i]; i += 2)
				if (!strcmp(basefont, cp936fonts[i]))
					break;
			if (cp936fonts[i])
			{
				fz_warn(ctx, "workaround for S22PDF lying about chinese font encodings");
				pdf_drop_font(ctx, fontdesc);
				fontdesc = NULL;
				fontdesc = pdf_new_font_desc(ctx);
				pdf_load_font_descriptor(ctx, doc, fontdesc, descriptor, "Adobe-GB1", cp936fonts[i+1], 0);
				fontdesc->encoding = pdf_load_system_cmap(ctx, "GBK-EUC-H");
				fontdesc->to_unicode = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");
				fontdesc->to_ttf_cmap = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");

				goto skip_encoding;
			}
		}

		face = fontdesc->font->ft_face;
		kind = ft_kind(face);

		/* Encoding */

		symbolic = fontdesc->flags & 4;

		if (kind == TYPE1)
			cmap = select_type1_cmap(face);
		else if (kind == TRUETYPE)
			cmap = select_truetype_cmap(face, symbolic);
		else
			cmap = select_unknown_cmap(face);

		if (cmap)
		{
			fterr = FT_Set_Charmap(face, cmap);
			if (fterr)
				fz_warn(ctx, "freetype could not set cmap: %s", ft_error_string(fterr));
		}
		else
			fz_warn(ctx, "freetype could not find any cmaps");

		/* FIXME: etable may leak on error. */
		etable = Memento_label(fz_malloc_array(ctx, 256, unsigned short), "cid_to_gid");
		fontdesc->size += 256 * sizeof(unsigned short);
		for (i = 0; i < 256; i++)
		{
			estrings[i] = NULL;
			etable[i] = 0;
		}

		encoding = pdf_dict_get(ctx, dict, PDF_NAME(Encoding));
		if (encoding)
		{
			if (pdf_is_name(ctx, encoding))
				pdf_load_encoding(estrings, pdf_to_name(ctx, encoding));

			if (pdf_is_dict(ctx, encoding))
			{
				pdf_obj *base, *diff, *item;

				base = pdf_dict_get(ctx, encoding, PDF_NAME(BaseEncoding));
				if (pdf_is_name(ctx, base))
					pdf_load_encoding(estrings, pdf_to_name(ctx, base));
				else if (!fontdesc->is_embedded && !symbolic)
					pdf_load_encoding(estrings, "StandardEncoding");

				diff = pdf_dict_get(ctx, encoding, PDF_NAME(Differences));
				if (pdf_is_array(ctx, diff))
				{
					n = pdf_array_len(ctx, diff);
					k = 0;
					for (i = 0; i < n; i++)
					{
						item = pdf_array_get(ctx, diff, i);
						if (pdf_is_int(ctx, item))
							k = pdf_to_int(ctx, item);
						if (pdf_is_name(ctx, item) && k >= 0 && k < (int)nelem(estrings))
							estrings[k++] = pdf_to_name(ctx, item);
					}
				}
			}
		}
		else if (!fontdesc->is_embedded && !symbolic)
			pdf_load_encoding(estrings, "StandardEncoding");

		/* start with the builtin encoding */
		for (i = 0; i < 256; i++)
			etable[i] = ft_char_index(face, i);

		fz_lock(ctx, FZ_LOCK_FREETYPE);
		has_lock = 1;

		/* built-in and substitute fonts may be a different type than what the document expects */
		subtype = pdf_dict_get(ctx, dict, PDF_NAME(Subtype));
		if (pdf_name_eq(ctx, subtype, PDF_NAME(Type1)))
			kind = TYPE1;
		else if (pdf_name_eq(ctx, subtype, PDF_NAME(MMType1)))
			kind = TYPE1;
		else if (pdf_name_eq(ctx, subtype, PDF_NAME(TrueType)))
			kind = TRUETYPE;
		else if (pdf_name_eq(ctx, subtype, PDF_NAME(CIDFontType0)))
			kind = TYPE1;
		else if (pdf_name_eq(ctx, subtype, PDF_NAME(CIDFontType2)))
			kind = TRUETYPE;

		/* encode by glyph name where we can */
		if (kind == TYPE1)
		{
			for (i = 0; i < 256; i++)
			{
				if (estrings[i])
				{
					glyph = ft_name_index(face, estrings[i]);
					if (glyph > 0)
						etable[i] = glyph;
				}
			}
		}

		/* encode by glyph name where we can */
		if (kind == TRUETYPE)
		{
			/* Unicode cmap */
			if (!symbolic && face->charmap && face->charmap->platform_id == 3)
			{
				for (i = 0; i < 256; i++)
				{
					if (estrings[i])
					{
						glyph = ft_find_glyph_by_unicode_name(face, estrings[i]);
						if (glyph > 0)
							etable[i] = glyph;
					}
				}
			}

			/* MacRoman cmap */
			else if (!symbolic && face->charmap && face->charmap->platform_id == 1)
			{
				for (i = 0; i < 256; i++)
				{
					if (estrings[i])
					{
						int mrcode = lookup_mre_code(estrings[i]);
						glyph = 0;
						if (mrcode > 0)
							glyph = ft_char_index(face, mrcode);
						if (glyph == 0)
							glyph = ft_name_index(face, estrings[i]);
						if (glyph > 0)
							etable[i] = glyph;
					}
				}
			}

			/* Symbolic cmap */
			else if (!face->charmap || face->charmap->encoding != FT_ENCODING_MS_SYMBOL)
			{
				for (i = 0; i < 256; i++)
				{
					if (estrings[i])
					{
						glyph = ft_name_index(face, estrings[i]);
						if (glyph > 0)
							etable[i] = glyph;
					}
				}
			}
		}

		/* try to reverse the glyph names from the builtin encoding */
		for (i = 0; i < 256; i++)
		{
			if (etable[i] && !estrings[i])
			{
				if (FT_HAS_GLYPH_NAMES(face))
				{
					fterr = FT_Get_Glyph_Name(face, etable[i], ebuffer[i], 32);
					if (fterr)
						fz_warn(ctx, "freetype get glyph name (gid %d): %s", etable[i], ft_error_string(fterr));
					if (ebuffer[i][0])
						estrings[i] = ebuffer[i];
				}
				else
				{
					estrings[i] = (char*) fz_glyph_name_from_win_ansi[i]; /* discard const */
				}
			}
		}

		/* symbolic Type 1 fonts with an implicit encoding and non-standard glyph names */
		if (kind == TYPE1 && symbolic)
		{
			for (i = 0; i < 256; i++)
				if (etable[i] && estrings[i] && !fz_unicode_from_glyph_name(estrings[i]))
					estrings[i] = (char*) fz_glyph_name_from_adobe_standard[i];
		}

		fz_unlock(ctx, FZ_LOCK_FREETYPE);
		has_lock = 0;

		fontdesc->encoding = pdf_new_identity_cmap(ctx, 0, 1);
		fontdesc->size += pdf_cmap_size(ctx, fontdesc->encoding);
		fontdesc->cid_to_gid_len = 256;
		fontdesc->cid_to_gid = etable;

		fz_try(ctx)
		{
			pdf_load_to_unicode(ctx, doc, fontdesc, estrings, NULL, pdf_dict_get(ctx, dict, PDF_NAME(ToUnicode)));
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_warn(ctx, "cannot load ToUnicode CMap");
		}

	skip_encoding:

		/* Widths */

		pdf_set_default_hmtx(ctx, fontdesc, fontdesc->missing_width);

		widths = pdf_dict_get(ctx, dict, PDF_NAME(Widths));
		if (widths)
		{
			int first, last;

			first = pdf_dict_get_int(ctx, dict, PDF_NAME(FirstChar));
			last = pdf_dict_get_int(ctx, dict, PDF_NAME(LastChar));

			if (first < 0 || last > 255 || first > last)
				first = last = 0;

			for (i = 0; i < last - first + 1; i++)
			{
				int wid = pdf_array_get_int(ctx, widths, i);
				pdf_add_hmtx(ctx, fontdesc, i + first, i + first, wid);
			}
		}
		else
		{
			for (i = 0; i < 256; i++)
				pdf_add_hmtx(ctx, fontdesc, i, i, ft_width(ctx, fontdesc, i));
		}

		pdf_end_hmtx(ctx, fontdesc);
	}
	fz_catch(ctx)
	{
		if (has_lock)
			fz_unlock(ctx, FZ_LOCK_FREETYPE);
		if (fontdesc && etable != fontdesc->cid_to_gid)
			fz_free(ctx, etable);
		pdf_drop_font(ctx, fontdesc);
		fz_rethrow(ctx);
	}
	return fontdesc;
}

static int
hail_mary_make_hash_key(fz_context *ctx, fz_store_hash *hash, void *key_)
{
	hash->u.pi.i = 0;
	hash->u.pi.ptr = NULL;
	return 1;
}

static void *
hail_mary_keep_key(fz_context *ctx, void *key)
{
	return key;
}

static void
hail_mary_drop_key(fz_context *ctx, void *key)
{
}

static int
hail_mary_cmp_key(fz_context *ctx, void *k0, void *k1)
{
	return k0 == k1;
}

static void
hail_mary_format_key(fz_context *ctx, char *s, size_t n, void *key_)
{
	fz_strlcpy(s, "(hail mary font)", n);
}

static int hail_mary_store_key; /* Dummy */

static const fz_store_type hail_mary_store_type =
{
	hail_mary_make_hash_key,
	hail_mary_keep_key,
	hail_mary_drop_key,
	hail_mary_cmp_key,
	hail_mary_format_key,
	NULL
};

pdf_font_desc *
pdf_load_hail_mary_font(fz_context *ctx, pdf_document *doc)
{
	pdf_font_desc *fontdesc;
	pdf_font_desc *existing;

	if ((fontdesc = fz_find_item(ctx, pdf_drop_font_imp, &hail_mary_store_key, &hail_mary_store_type)) != NULL)
	{
		return fontdesc;
	}

	/* FIXME: Get someone with a clue about fonts to fix this */
	fontdesc = pdf_load_simple_font(ctx, doc, NULL);

	existing = fz_store_item(ctx, &hail_mary_store_key, fontdesc, fontdesc->size, &hail_mary_store_type);
	assert(existing == NULL);
	(void)existing; /* Silence warning in release builds */

	return fontdesc;
}

/*
 * CID Fonts
 */

static pdf_font_desc *
load_cid_font(fz_context *ctx, pdf_document *doc, pdf_obj *dict, pdf_obj *encoding, pdf_obj *to_unicode)
{
	pdf_obj *widths;
	pdf_obj *descriptor;
	pdf_font_desc *fontdesc = NULL;
	fz_buffer *buf = NULL;
	pdf_cmap *cmap;
	FT_Face face;
	char collection[256];
	const char *basefont;
	int i, k, fterr;
	pdf_obj *cidtogidmap;
	pdf_obj *obj;
	int dw;

	fz_var(fontdesc);
	fz_var(buf);

	fz_try(ctx)
	{
		/* Get font name and CID collection */

		basefont = pdf_to_name(ctx, pdf_dict_get(ctx, dict, PDF_NAME(BaseFont)));

		{
			pdf_obj *cidinfo;
			const char *reg, *ord;

			cidinfo = pdf_dict_get(ctx, dict, PDF_NAME(CIDSystemInfo));
			if (!cidinfo)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "cid font is missing info");

			reg = pdf_dict_get_string(ctx, cidinfo, PDF_NAME(Registry), NULL);
			ord = pdf_dict_get_string(ctx, cidinfo, PDF_NAME(Ordering), NULL);
			fz_snprintf(collection, sizeof collection, "%s-%s", reg, ord);
		}

		/* Encoding */

		if (pdf_is_name(ctx, encoding))
		{
			cmap = pdf_load_system_cmap(ctx, pdf_to_name(ctx, encoding));
		}
		else if (pdf_is_indirect(ctx, encoding))
		{
			cmap = pdf_load_embedded_cmap(ctx, doc, encoding);
		}
		else
		{
			fz_throw(ctx, FZ_ERROR_SYNTAX, "font missing encoding");
		}

		/* Load font file */

		fontdesc = pdf_new_font_desc(ctx);

		fontdesc->encoding = cmap;
		fontdesc->size += pdf_cmap_size(ctx, fontdesc->encoding);

		pdf_set_font_wmode(ctx, fontdesc, pdf_cmap_wmode(ctx, fontdesc->encoding));

		descriptor = pdf_dict_get(ctx, dict, PDF_NAME(FontDescriptor));
		if (!descriptor)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "missing font descriptor");
		pdf_load_font_descriptor(ctx, doc, fontdesc, descriptor, collection, basefont, 1);

		face = fontdesc->font->ft_face;

		/* Apply encoding */

		cidtogidmap = pdf_dict_get(ctx, dict, PDF_NAME(CIDToGIDMap));
		if (pdf_is_stream(ctx, cidtogidmap))
		{
			size_t z, len;
			unsigned char *data;

			buf = pdf_load_stream(ctx, cidtogidmap);

			len = fz_buffer_storage(ctx, buf, &data);
			fontdesc->cid_to_gid_len = len / 2;
			fontdesc->cid_to_gid = Memento_label(fz_malloc_array(ctx, fontdesc->cid_to_gid_len, unsigned short), "cid_to_gid_map");
			fontdesc->size += fontdesc->cid_to_gid_len * sizeof(unsigned short);
			for (z = 0; z < fontdesc->cid_to_gid_len; z++)
				fontdesc->cid_to_gid[z] = (data[z * 2] << 8) + data[z * 2 + 1];
		}
		else if (cidtogidmap && !pdf_name_eq(ctx, PDF_NAME(Identity), cidtogidmap))
		{
			fz_warn(ctx, "ignoring unknown CIDToGIDMap entry");
		}

		/* if font is external, cidtogidmap should not be identity */
		/* so we map from cid to unicode and then map that through the (3 1) */
		/* unicode cmap to get a glyph id */
		else if (fontdesc->font->flags.ft_substitute)
		{
			fterr = FT_Select_Charmap(face, ft_encoding_unicode);
			if (fterr)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "no unicode cmap when emulating CID font: %s", ft_error_string(fterr));

			if (!strcmp(collection, "Adobe-CNS1"))
				fontdesc->to_ttf_cmap = pdf_load_system_cmap(ctx, "Adobe-CNS1-UCS2");
			else if (!strcmp(collection, "Adobe-GB1"))
				fontdesc->to_ttf_cmap = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");
			else if (!strcmp(collection, "Adobe-Japan1"))
				fontdesc->to_ttf_cmap = pdf_load_system_cmap(ctx, "Adobe-Japan1-UCS2");
			else if (!strcmp(collection, "Adobe-Japan2"))
				fontdesc->to_ttf_cmap = pdf_load_system_cmap(ctx, "Adobe-Japan2-UCS2");
			else if (!strcmp(collection, "Adobe-Korea1"))
				fontdesc->to_ttf_cmap = pdf_load_system_cmap(ctx, "Adobe-Korea1-UCS2");
		}

		pdf_load_to_unicode(ctx, doc, fontdesc, NULL, collection, to_unicode);

		/* If we have an identity encoding, we're supposed to use the glyph ids directly.
		 * If we only have a substitute font, that won't work.
		 * Make a last ditch attempt by using
		 * the ToUnicode table if it exists to map via the substitute font's cmap. */
		if (strstr(fontdesc->encoding->cmap_name, "Identity-") && fontdesc->font->flags.ft_substitute)
		{
			fz_warn(ctx, "non-embedded font using identity encoding: %s", basefont);
			if (fontdesc->to_unicode && !fontdesc->to_ttf_cmap)
				fontdesc->to_ttf_cmap = pdf_keep_cmap(ctx, fontdesc->to_unicode);
		}

		/* Horizontal */

		dw = 1000;
		obj = pdf_dict_get(ctx, dict, PDF_NAME(DW));
		if (obj)
			dw = pdf_to_int(ctx, obj);
		pdf_set_default_hmtx(ctx, fontdesc, dw);

		widths = pdf_dict_get(ctx, dict, PDF_NAME(W));
		if (widths)
		{
			int c0, c1, w, n, m;

			n = pdf_array_len(ctx, widths);
			for (i = 0; i < n; )
			{
				c0 = pdf_array_get_int(ctx, widths, i);
				obj = pdf_array_get(ctx, widths, i + 1);
				if (pdf_is_array(ctx, obj))
				{
					m = pdf_array_len(ctx, obj);
					for (k = 0; k < m; k++)
					{
						w = pdf_array_get_int(ctx, obj, k);
						pdf_add_hmtx(ctx, fontdesc, c0 + k, c0 + k, w);
					}
					i += 2;
				}
				else
				{
					c1 = pdf_to_int(ctx, obj);
					w = pdf_array_get_int(ctx, widths, i + 2);
					pdf_add_hmtx(ctx, fontdesc, c0, c1, w);
					i += 3;
				}
			}
		}

		pdf_end_hmtx(ctx, fontdesc);

		/* Vertical */

		if (pdf_cmap_wmode(ctx, fontdesc->encoding) == 1)
		{
			int dw2y = 880;
			int dw2w = -1000;

			obj = pdf_dict_get(ctx, dict, PDF_NAME(DW2));
			if (obj)
			{
				dw2y = pdf_array_get_int(ctx, obj, 0);
				dw2w = pdf_array_get_int(ctx, obj, 1);
			}

			pdf_set_default_vmtx(ctx, fontdesc, dw2y, dw2w);

			widths = pdf_dict_get(ctx, dict, PDF_NAME(W2));
			if (widths)
			{
				int c0, c1, w, x, y, n;

				n = pdf_array_len(ctx, widths);
				for (i = 0; i < n; )
				{
					c0 = pdf_array_get_int(ctx, widths, i);
					obj = pdf_array_get(ctx, widths, i + 1);
					if (pdf_is_array(ctx, obj))
					{
						int m = pdf_array_len(ctx, obj);
						for (k = 0; k * 3 < m; k ++)
						{
							w = pdf_array_get_int(ctx, obj, k * 3 + 0);
							x = pdf_array_get_int(ctx, obj, k * 3 + 1);
							y = pdf_array_get_int(ctx, obj, k * 3 + 2);
							pdf_add_vmtx(ctx, fontdesc, c0 + k, c0 + k, x, y, w);
						}
						i += 2;
					}
					else
					{
						c1 = pdf_to_int(ctx, obj);
						w = pdf_array_get_int(ctx, widths, i + 2);
						x = pdf_array_get_int(ctx, widths, i + 3);
						y = pdf_array_get_int(ctx, widths, i + 4);
						pdf_add_vmtx(ctx, fontdesc, c0, c1, x, y, w);
						i += 5;
					}
				}
			}

			pdf_end_vmtx(ctx, fontdesc);
		}
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
	{
		pdf_drop_font(ctx, fontdesc);
		fz_rethrow(ctx);
	}

	return fontdesc;
}

static pdf_font_desc *
pdf_load_type0_font(fz_context *ctx, pdf_document *doc, pdf_obj *dict)
{
	pdf_obj *dfonts;
	pdf_obj *dfont;
	pdf_obj *subtype;
	pdf_obj *encoding;
	pdf_obj *to_unicode;

	dfonts = pdf_dict_get(ctx, dict, PDF_NAME(DescendantFonts));
	if (!dfonts)
		fz_throw(ctx, FZ_ERROR_SYNTAX, "cid font is missing descendant fonts");

	dfont = pdf_array_get(ctx, dfonts, 0);

	subtype = pdf_dict_get(ctx, dfont, PDF_NAME(Subtype));
	encoding = pdf_dict_get(ctx, dict, PDF_NAME(Encoding));
	to_unicode = pdf_dict_get(ctx, dict, PDF_NAME(ToUnicode));

	if (pdf_is_name(ctx, subtype) && pdf_name_eq(ctx, subtype, PDF_NAME(CIDFontType0)))
		return load_cid_font(ctx, doc, dfont, encoding, to_unicode);
	if (pdf_is_name(ctx, subtype) && pdf_name_eq(ctx, subtype, PDF_NAME(CIDFontType2)))
		return load_cid_font(ctx, doc, dfont, encoding, to_unicode);
	fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown cid font type");
}

/*
 * FontDescriptor
 */

static void
pdf_load_font_descriptor(fz_context *ctx, pdf_document *doc, pdf_font_desc *fontdesc, pdf_obj *dict,
	const char *collection, const char *basefont, int iscidfont)
{
	pdf_obj *obj1, *obj2, *obj3, *obj;
	const char *fontname;
	FT_Face face;

	/* Prefer BaseFont; don't bother with FontName */
	fontname = basefont;

	fontdesc->flags = pdf_dict_get_int(ctx, dict, PDF_NAME(Flags));
	fontdesc->italic_angle = pdf_dict_get_real(ctx, dict, PDF_NAME(ItalicAngle));
	fontdesc->ascent = pdf_dict_get_real(ctx, dict, PDF_NAME(Ascent));
	fontdesc->descent = pdf_dict_get_real(ctx, dict, PDF_NAME(Descent));
	fontdesc->cap_height = pdf_dict_get_real(ctx, dict, PDF_NAME(CapHeight));
	fontdesc->x_height = pdf_dict_get_real(ctx, dict, PDF_NAME(XHeight));
	fontdesc->missing_width = pdf_dict_get_real(ctx, dict, PDF_NAME(MissingWidth));

	obj1 = pdf_dict_get(ctx, dict, PDF_NAME(FontFile));
	obj2 = pdf_dict_get(ctx, dict, PDF_NAME(FontFile2));
	obj3 = pdf_dict_get(ctx, dict, PDF_NAME(FontFile3));
	obj = obj1 ? obj1 : obj2 ? obj2 : obj3;

	if (pdf_is_indirect(ctx, obj))
	{
		fz_try(ctx)
		{
			pdf_load_embedded_font(ctx, doc, fontdesc, fontname, obj);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_warn(ctx, "ignored error when loading embedded font; attempting to load system font");
			if (!iscidfont && fontname != pdf_clean_font_name(fontname))
				pdf_load_builtin_font(ctx, fontdesc, fontname, 1);
			else
				pdf_load_system_font(ctx, fontdesc, fontname, collection);
		}
	}
	else
	{
		/* sumatrapdf: https://github.com/sumatrapdfreader/sumatrapdf/commit/0c8b5a04ad9ee13584f88e3f055a83b82d65a6ca */
		if (!iscidfont && fontname != pdf_clean_font_name(fontname))
			pdf_load_builtin_font(ctx, fontdesc, fontname, 0);
		else
			pdf_load_system_font(ctx, fontdesc, fontname, collection);
	}

	/* Check for DynaLab fonts that must use hinting */
	face = fontdesc->font->ft_face;
	if (ft_kind(face) == TRUETYPE)
	{
		/* FreeType's own 'tricky' font detection needs a bit of help */
		if (is_dynalab(fontdesc->font->name))
			face->face_flags |= FT_FACE_FLAG_TRICKY;

		if (fontdesc->ascent == 0.0f)
			fontdesc->ascent = 1000.0f * face->ascender / face->units_per_EM;

		if (fontdesc->descent == 0.0f)
			fontdesc->descent = 1000.0f * face->descender / face->units_per_EM;
	}
}

static void
pdf_make_width_table(fz_context *ctx, pdf_font_desc *fontdesc)
{
	fz_font *font = fontdesc->font;
	int i, k, n, cid, gid;

	n = 0;
	for (i = 0; i < fontdesc->hmtx_len; i++)
	{
		for (k = fontdesc->hmtx[i].lo; k <= fontdesc->hmtx[i].hi; k++)
		{
			cid = pdf_lookup_cmap(fontdesc->encoding, k);
			gid = pdf_font_cid_to_gid(ctx, fontdesc, cid);
			if (gid > n)
				n = gid;
		}
	}

	font->width_count = n + 1;
	font->width_table = Memento_label(fz_malloc_array(ctx, font->width_count, short), "font_widths");
	fontdesc->size += font->width_count * sizeof(short);

	font->width_default = fontdesc->dhmtx.w;
	for (i = 0; i < font->width_count; i++)
		font->width_table[i] = -1;

	for (i = 0; i < fontdesc->hmtx_len; i++)
	{
		for (k = fontdesc->hmtx[i].lo; k <= fontdesc->hmtx[i].hi; k++)
		{
			cid = pdf_lookup_cmap(fontdesc->encoding, k);
			gid = pdf_font_cid_to_gid(ctx, fontdesc, cid);
			if (gid >= 0 && gid < font->width_count)
				font->width_table[gid] = fz_maxi(fontdesc->hmtx[i].w, font->width_table[gid]);
		}
	}

	for (i = 0; i < font->width_count; i++)
		if (font->width_table[i] == -1)
			font->width_table[i] = font->width_default;
}

pdf_font_desc *
pdf_load_font(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *dict)
{
	pdf_obj *subtype;
	pdf_obj *dfonts;
	pdf_obj *charprocs;
	pdf_font_desc *fontdesc = NULL;
	int type3 = 0;

	if (pdf_obj_marked(ctx, dict))
		fz_throw(ctx, FZ_ERROR_SYNTAX, "Recursive Type3 font definition.");

	if ((fontdesc = pdf_find_item(ctx, pdf_drop_font_imp, dict)) != NULL)
	{
		return fontdesc;
	}

	subtype = pdf_dict_get(ctx, dict, PDF_NAME(Subtype));
	dfonts = pdf_dict_get(ctx, dict, PDF_NAME(DescendantFonts));
	charprocs = pdf_dict_get(ctx, dict, PDF_NAME(CharProcs));

	if (pdf_name_eq(ctx, subtype, PDF_NAME(Type0)))
		fontdesc = pdf_load_type0_font(ctx, doc, dict);
	else if (pdf_name_eq(ctx, subtype, PDF_NAME(Type1)))
		fontdesc = pdf_load_simple_font(ctx, doc, dict);
	else if (pdf_name_eq(ctx, subtype, PDF_NAME(MMType1)))
		fontdesc = pdf_load_simple_font(ctx, doc, dict);
	else if (pdf_name_eq(ctx, subtype, PDF_NAME(TrueType)))
		fontdesc = pdf_load_simple_font(ctx, doc, dict);
	else if (pdf_name_eq(ctx, subtype, PDF_NAME(Type3)))
	{
		fontdesc = pdf_load_type3_font(ctx, doc, rdb, dict);
		type3 = 1;
	}
	else if (charprocs)
	{
		fz_warn(ctx, "unknown font format, guessing type3.");
		fontdesc = pdf_load_type3_font(ctx, doc, rdb, dict);
		type3 = 1;
	}
	else if (dfonts)
	{
		fz_warn(ctx, "unknown font format, guessing type0.");
		fontdesc = pdf_load_type0_font(ctx, doc, dict);
	}
	else
	{
		fz_warn(ctx, "unknown font format, guessing type1 or truetype.");
		fontdesc = pdf_load_simple_font(ctx, doc, dict);
	}

	pdf_mark_obj(ctx, dict);
	fz_try(ctx)
	{
		/* Create glyph width table for stretching substitute fonts and text extraction. */
		pdf_make_width_table(ctx, fontdesc);

		/* Load CharProcs */
		if (type3)
			pdf_load_type3_glyphs(ctx, doc, fontdesc);

		pdf_store_item(ctx, dict, fontdesc, fontdesc->size);
	}
	fz_always(ctx)
		pdf_unmark_obj(ctx, dict);
	fz_catch(ctx)
	{
		pdf_drop_font(ctx, fontdesc);
		fz_rethrow(ctx);
	}

	return fontdesc;
}

void
pdf_print_font(fz_context *ctx, fz_output *out, pdf_font_desc *fontdesc)
{
	int i;

	fz_write_printf(ctx, out, "fontdesc {\n");

	if (fontdesc->font->ft_face)
		fz_write_printf(ctx, out, "\tfreetype font\n");
	if (fontdesc->font->t3procs)
		fz_write_printf(ctx, out, "\ttype3 font\n");

	fz_write_printf(ctx, out, "\twmode %d\n", fontdesc->wmode);
	fz_write_printf(ctx, out, "\tDW %d\n", fontdesc->dhmtx.w);

	fz_write_printf(ctx, out, "\tW {\n");
	for (i = 0; i < fontdesc->hmtx_len; i++)
		fz_write_printf(ctx, out, "\t\t<%04x> <%04x> %d\n",
			fontdesc->hmtx[i].lo, fontdesc->hmtx[i].hi, fontdesc->hmtx[i].w);
	fz_write_printf(ctx, out, "\t}\n");

	if (fontdesc->wmode)
	{
		fz_write_printf(ctx, out, "\tDW2 [%d %d]\n", fontdesc->dvmtx.y, fontdesc->dvmtx.w);
		fz_write_printf(ctx, out, "\tW2 {\n");
		for (i = 0; i < fontdesc->vmtx_len; i++)
			fz_write_printf(ctx, out, "\t\t<%04x> <%04x> %d %d %d\n", fontdesc->vmtx[i].lo, fontdesc->vmtx[i].hi,
				fontdesc->vmtx[i].x, fontdesc->vmtx[i].y, fontdesc->vmtx[i].w);
		fz_write_printf(ctx, out, "\t}\n");
	}
}
