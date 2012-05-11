#include "fitz-internal.h"
#include "mupdf-internal.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_XFREE86_H

static void pdf_load_font_descriptor(pdf_font_desc *fontdesc, pdf_document *xref, pdf_obj *dict, char *collection, char *basefont, int has_encoding);

static char *base_font_names[14][7] =
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
	{ "Symbol", NULL },
	{ "ZapfDingbats", NULL }
};

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

static int strcmp_ignore_space(char *a, char *b)
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

static char *clean_font_name(char *fontname)
{
	int i, k;
	for (i = 0; i < 14; i++)
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
	const char *kind = FT_Get_X11_Font_Format(face);
	if (!strcmp(kind, "TrueType"))
		return TRUETYPE;
	if (!strcmp(kind, "Type 1"))
		return TYPE1;
	if (!strcmp(kind, "CFF"))
		return TYPE1;
	if (!strcmp(kind, "CID Type 1"))
		return TYPE1;
	return UNKNOWN;
}

static int ft_is_bold(FT_Face face)
{
	return face->style_flags & FT_STYLE_FLAG_BOLD;
}

static int ft_is_italic(FT_Face face)
{
	return face->style_flags & FT_STYLE_FLAG_ITALIC;
}

static int ft_char_index(FT_Face face, int cid)
{
	int gid = FT_Get_Char_Index(face, cid);
	if (gid == 0)
		gid = FT_Get_Char_Index(face, 0xf000 + cid);

	/* some chinese fonts only ship the similarly looking 0x2026 */
	if (gid == 0 && cid == 0x22ef)
		gid = FT_Get_Char_Index(face, 0x2026);

	return gid;
}

static int ft_cid_to_gid(pdf_font_desc *fontdesc, int cid)
{
	if (fontdesc->to_ttf_cmap)
	{
		cid = pdf_lookup_cmap(fontdesc->to_ttf_cmap, cid);
		return ft_char_index(fontdesc->font->ft_face, cid);
	}

	if (fontdesc->cid_to_gid && cid < fontdesc->cid_to_gid_len && cid >= 0)
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
	int gid = ft_cid_to_gid(fontdesc, cid);
	int fterr;

	fterr = FT_Load_Glyph(fontdesc->font->ft_face, gid,
			FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
	if (fterr)
	{
		fz_warn(ctx, "freetype load glyph (gid %d): %s", gid, ft_error_string(fterr));
		return 0;
	}
	return ((FT_Face)fontdesc->font->ft_face)->glyph->advance.x;
}

static int lookup_mre_code(char *name)
{
	int i;
	for (i = 0; i < 256; i++)
		if (pdf_mac_roman[i] && !strcmp(name, pdf_mac_roman[i]))
			return i;
	return -1;
}

/*
 * Load font files.
 */

static void
pdf_load_builtin_font(fz_context *ctx, pdf_font_desc *fontdesc, char *fontname)
{
	unsigned char *data;
	unsigned int len;

	data = pdf_lookup_builtin_font(fontname, &len);
	if (!data)
	{
#ifdef _WIN32
		/* we use built-in fonts in addition to those installed on windows
		   because the metric for Times-Roman in windows fonts seems wrong
		   and we end up with over-lapping text if this font is used.
		   poppler doesn't have this problem even when using windows fonts
		   so maybe there's a better fix. */
		fz_try(ctx)
		{
			pdf_load_windows_font(ctx, fontdesc, fontname);
		}
		fz_catch(ctx) { }
		if (fontdesc->font)
			return;
#endif
		fz_throw(ctx, "cannot find builtin font: '%s'", fontname);
	}

	fontdesc->font = fz_new_font_from_memory(ctx, data, len, 0, 1);
	/* RJW: "cannot load freetype font from memory" */

	if (!strcmp(fontname, "Symbol") || !strcmp(fontname, "ZapfDingbats"))
		fontdesc->flags |= PDF_FD_SYMBOLIC;
}

static void
pdf_load_substitute_font(fz_context *ctx, pdf_font_desc *fontdesc, int mono, int serif, int bold, int italic)
{
	unsigned char *data;
	unsigned int len;

	data = pdf_lookup_substitute_font(mono, serif, bold, italic, &len);
	if (!data)
		fz_throw(ctx, "cannot find substitute font");

	fontdesc->font = fz_new_font_from_memory(ctx, data, len, 0, 1);
	/* RJW: "cannot load freetype font from memory" */

	fontdesc->font->ft_substitute = 1;
	fontdesc->font->ft_bold = bold && !ft_is_bold(fontdesc->font->ft_face);
	fontdesc->font->ft_italic = italic && !ft_is_italic(fontdesc->font->ft_face);
}

static void
pdf_load_substitute_cjk_font(fz_context *ctx, pdf_font_desc *fontdesc, int ros, int serif)
{
	unsigned char *data;
	unsigned int len;

#ifdef _WIN32
	/* Try to fall back to a reasonable TrueType font that might be installed locally */
	fz_try(ctx)
	{
		pdf_load_similar_cjk_font(ctx, fontdesc, ros, serif);
	}
	fz_catch(ctx)
	{
#ifdef NOCJKFONT
		fz_try(ctx)
		{
			/* If no CJK fallback font is builtin, maybe one has been shipped separately */
			pdf_load_windows_font(ctx, fontdesc, "DroidSansFallback");
		}
		fz_catch(ctx) { }
#endif
	}
	if (fontdesc->font)
	{
		fontdesc->font->ft_substitute = 1;
		return;
	}
#endif

	data = pdf_lookup_substitute_cjk_font(ros, serif, &len);
	if (!data)
		fz_throw(ctx, "cannot find builtin CJK font");

	/* a glyph bbox cache is too big for droid sans fallback (51k glyphs!) */
	fontdesc->font = fz_new_font_from_memory(ctx, data, len, 0, 0);
	/* RJW: "cannot load builtin CJK font" */

	fontdesc->font->ft_substitute = 1;
}

static void
pdf_load_system_font(fz_context *ctx, pdf_font_desc *fontdesc, char *fontname, char *collection, int has_encoding)
{
	int bold = 0;
	int italic = 0;
	int serif = 0;
	int mono = 0;

#ifdef _WIN32
	/* try to find a precise match in Windows' fonts before falling back to a built-in one */
	fz_try(ctx)
	{
		pdf_load_windows_font(ctx, fontdesc, fontname);
	}
	fz_catch(ctx) { }
	if (fontdesc->font)
	{
		/* TODO: this seems to be required at least for MS-Mincho - why? */
		if (collection)
			fontdesc->font->ft_substitute = 1;
		return;
	}
#endif

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
			pdf_load_substitute_cjk_font(ctx, fontdesc, PDF_ROS_CNS, serif);
		else if (!strcmp(collection, "Adobe-GB1"))
			pdf_load_substitute_cjk_font(ctx, fontdesc, PDF_ROS_GB, serif);
		else if (!strcmp(collection, "Adobe-Japan1"))
			pdf_load_substitute_cjk_font(ctx, fontdesc, PDF_ROS_JAPAN, serif);
		else if (!strcmp(collection, "Adobe-Korea1"))
			pdf_load_substitute_cjk_font(ctx, fontdesc, PDF_ROS_KOREA, serif);
		/* SumatraPDF: use a standard font for Adobe-Identity fonts */
		else if (!strcmp(collection, "Adobe-Identity"))
			goto use_standard_font;
		else
			fz_throw(ctx, "unknown cid collection: %s", collection);
		return;
	}
use_standard_font:

	/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=691690 */
	if ((fontdesc->flags & PDF_FD_SYMBOLIC) && !has_encoding)
		fz_throw(ctx, "encoding-less symbolic font '%s' is missing", fontname);

	pdf_load_substitute_font(ctx, fontdesc, mono, serif, bold, italic);
	/* RJW: "cannot load substitute font" */
}

static void
pdf_load_embedded_font(pdf_font_desc *fontdesc, pdf_document *xref, pdf_obj *stmref)
{
	fz_buffer *buf;
	fz_context *ctx = xref->ctx;

	fz_try(ctx)
	{
		buf = pdf_load_stream(xref, pdf_to_num(stmref), pdf_to_gen(stmref));
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot load font stream (%d %d R)", pdf_to_num(stmref), pdf_to_gen(stmref));
	}

	fz_try(ctx)
	{
		fontdesc->font = fz_new_font_from_memory(ctx, buf->data, buf->len, 0, 1);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_throw(ctx, "cannot load embedded font (%d %d R)", pdf_to_num(stmref), pdf_to_gen(stmref));
	}
	fontdesc->size += buf->len;

	/* save the buffer so we can free it later */
	fontdesc->font->ft_data = buf->data;
	fontdesc->font->ft_size = buf->len;
	fz_free(ctx, buf); /* only free the fz_buffer struct, not the contained data */

	fontdesc->is_embedded = 1;
}

/*
 * Create and destroy
 */

pdf_font_desc *
pdf_keep_font(fz_context *ctx, pdf_font_desc *fontdesc)
{
	return (pdf_font_desc *)fz_keep_storable(ctx, &fontdesc->storable);
}

void
pdf_drop_font(fz_context *ctx, pdf_font_desc *fontdesc)
{
	fz_drop_storable(ctx, &fontdesc->storable);
}

static void
pdf_free_font_imp(fz_context *ctx, fz_storable *fontdesc_)
{
	pdf_font_desc *fontdesc = (pdf_font_desc *)fontdesc_;

	/* SumatraPDF: free vertical glyph substitution data (before font!) */
	pdf_ft_free_vsubst(fontdesc);
	if (fontdesc->font)
		fz_drop_font(ctx, fontdesc->font);
	if (fontdesc->encoding)
		pdf_drop_cmap(ctx, fontdesc->encoding);
	if (fontdesc->to_ttf_cmap)
		pdf_drop_cmap(ctx, fontdesc->to_ttf_cmap);
	if (fontdesc->to_unicode)
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
	FZ_INIT_STORABLE(fontdesc, 1, pdf_free_font_imp);
	fontdesc->size = sizeof(pdf_font_desc);

	fontdesc->font = NULL;

	fontdesc->flags = 0;
	fontdesc->italic_angle = 0;
	fontdesc->ascent = 0;
	fontdesc->descent = 0;
	fontdesc->cap_height = 0;
	fontdesc->x_height = 0;
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

	/* SumatraPDF: vertical glyph substitution */
	fontdesc->_vsubst = NULL;

	return fontdesc;
}

/*
 * Simple fonts (Type1 and TrueType)
 */

static pdf_font_desc *
pdf_load_simple_font(pdf_document *xref, pdf_obj *dict)
{
	pdf_obj *descriptor;
	pdf_obj *encoding;
	pdf_obj *widths;
	unsigned short *etable = NULL;
	pdf_font_desc *fontdesc = NULL;
	FT_Face face;
	FT_CharMap cmap;
	int symbolic;
	int kind;

	char *basefont;
	char *fontname;
	char *estrings[256];
	char ebuffer[256][32];
	int i, k, n;
	int fterr;
	fz_context *ctx = xref->ctx;

	fz_var(fontdesc);
	fz_var(etable);

	basefont = pdf_to_name(pdf_dict_gets(dict, "BaseFont"));
	fontname = clean_font_name(basefont);

	/* Load font file */
	fz_try(ctx)
	{
		fontdesc = pdf_new_font_desc(ctx);

		descriptor = pdf_dict_gets(dict, "FontDescriptor");
		/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=691690 */
		fz_try(ctx)
		{
		if (descriptor)
			pdf_load_font_descriptor(fontdesc, xref, descriptor, NULL, basefont, pdf_dict_gets(dict, "Encoding") != NULL);
		else
			pdf_load_builtin_font(ctx, fontdesc, fontname);
		/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=691690 */
		}
		fz_catch(ctx)
		{
			if (!(fontdesc->flags & PDF_FD_SYMBOLIC))
				fz_rethrow(ctx);
			fz_warn(ctx, "using bullet-substitute font for '%s' (%d %d R)", fontname, pdf_to_num(dict), pdf_to_gen(dict));
			pdf_drop_font(ctx, fontdesc);

			fontdesc = pdf_new_font_desc(ctx);
			pdf_load_builtin_font(ctx, fontdesc, "Symbol");
			face = fontdesc->font->ft_face;
			kind = ft_kind(face);
			fontdesc->encoding = pdf_new_identity_cmap(ctx, 0, 1);
			fontdesc->cid_to_gid_len = 256;
			fontdesc->cid_to_gid = fz_malloc_array(ctx, 256, sizeof(unsigned short));
			k = FT_Get_Name_Index(face, "bullet");
			for (i = 0; i < 256; i++)
				fontdesc->cid_to_gid[i] = k;
			goto skip_encoding;
		}

		/* Some chinese documents mistakenly consider WinAnsiEncoding to be codepage 936 */
		/* SumatraPDF: tweak heuristic to determine broken chinese fonts */
		if (descriptor && pdf_is_string(pdf_dict_gets(descriptor, "FontName")) &&
			!pdf_dict_gets(dict, "ToUnicode") &&
			!strcmp(pdf_to_name(pdf_dict_gets(dict, "Encoding")), "WinAnsiEncoding") &&
			pdf_to_int(pdf_dict_gets(descriptor, "Flags")) == 4)
		{
			/* note: without the comma, pdf_load_font_descriptor would prefer /FontName over /BaseFont */
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
				fontdesc = pdf_new_font_desc(ctx);
				pdf_load_font_descriptor(fontdesc, xref, descriptor, "Adobe-GB1", cp936fonts[i+1], 1);
				fontdesc->encoding = pdf_load_system_cmap(ctx, "GBK-EUC-H");
				fontdesc->to_unicode = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");
				fontdesc->to_ttf_cmap = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");
				/* RJW: "cannot load font" */

				face = fontdesc->font->ft_face;
				kind = ft_kind(face);
				goto skip_encoding;
			}
		}

		face = fontdesc->font->ft_face;
		kind = ft_kind(face);

		/* Encoding */

		symbolic = fontdesc->flags & 4;

		if (face->num_charmaps > 0)
			cmap = face->charmaps[0];
		else
			cmap = NULL;

		for (i = 0; i < face->num_charmaps; i++)
		{
			FT_CharMap test = face->charmaps[i];

			if (kind == TYPE1)
			{
				if (test->platform_id == 7)
					cmap = test;
			}

			if (kind == TRUETYPE)
			{
				if (test->platform_id == 1 && test->encoding_id == 0)
					cmap = test;
				if (test->platform_id == 3 && test->encoding_id == 1)
					cmap = test;
				/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1815 */
				if (symbolic && test->platform_id == 3 && test->encoding_id == 0)
					cmap = test;
			}
		}

		if (cmap)
		{
			fterr = FT_Set_Charmap(face, cmap);
			if (fterr)
				fz_warn(ctx, "freetype could not set cmap: %s", ft_error_string(fterr));
		}
		else
			fz_warn(ctx, "freetype could not find any cmaps");

		etable = fz_malloc_array(ctx, 256, sizeof(unsigned short));
		fontdesc->size += 256 * sizeof(unsigned short);
		for (i = 0; i < 256; i++)
		{
			estrings[i] = NULL;
			etable[i] = 0;
		}

		encoding = pdf_dict_gets(dict, "Encoding");
		if (encoding)
		{
			if (pdf_is_name(encoding))
				pdf_load_encoding(estrings, pdf_to_name(encoding));

			if (pdf_is_dict(encoding))
			{
				pdf_obj *base, *diff, *item;

				base = pdf_dict_gets(encoding, "BaseEncoding");
				if (pdf_is_name(base))
					pdf_load_encoding(estrings, pdf_to_name(base));
				else if (!fontdesc->is_embedded && !symbolic)
					pdf_load_encoding(estrings, "StandardEncoding");

				diff = pdf_dict_gets(encoding, "Differences");
				if (pdf_is_array(diff))
				{
					n = pdf_array_len(diff);
					k = 0;
					for (i = 0; i < n; i++)
					{
						item = pdf_array_get(diff, i);
						if (pdf_is_int(item))
							k = pdf_to_int(item);
						/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1942 */
						if (pdf_is_name(item) && 0 <= k && k < 256)
							estrings[k++] = pdf_to_name(item);
					}
				}
			}
		}

		/* start with the builtin encoding */
		for (i = 0; i < 256; i++)
			etable[i] = ft_char_index(face, i);

		/* encode by glyph name where we can */
		fz_lock(ctx, FZ_LOCK_FREETYPE);
		if (kind == TYPE1)
		{
			for (i = 0; i < 256; i++)
			{
				if (estrings[i])
				{
					etable[i] = FT_Get_Name_Index(face, estrings[i]);
					if (etable[i] == 0)
					{
						int aglcode = pdf_lookup_agl(estrings[i]);
						const char **dupnames = pdf_lookup_agl_duplicates(aglcode);
						while (*dupnames)
						{
							etable[i] = FT_Get_Name_Index(face, (char*)*dupnames);
							if (etable[i])
								break;
							dupnames++;
						}
					}
				}
			}
		}

		/* encode by glyph name where we can */
		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1310 */
		if (kind == TRUETYPE || !strcmp(pdf_to_name(pdf_dict_gets(dict, "Subtype")), "TrueType") && symbolic)
		{
			/* Unicode cmap */
			if (!symbolic && face->charmap && face->charmap->platform_id == 3)
			{
				for (i = 0; i < 256; i++)
				{
					if (estrings[i])
					{
						int aglcode = pdf_lookup_agl(estrings[i]);
						if (!aglcode)
							etable[i] = FT_Get_Name_Index(face, estrings[i]);
						else
							etable[i] = ft_char_index(face, aglcode);
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
						k = lookup_mre_code(estrings[i]);
						if (k <= 0)
							etable[i] = FT_Get_Name_Index(face, estrings[i]);
						else
							etable[i] = ft_char_index(face, k);
					}
				}
			}

			/* Symbolic cmap */
			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1618 */
			else if (face->num_charmaps != 1 || face->charmaps[0]->encoding != FT_ENCODING_MS_SYMBOL)
			{
				for (i = 0; i < 256; i++)
				{
					if (estrings[i])
					{
						etable[i] = FT_Get_Name_Index(face, estrings[i]);
						if (etable[i] == 0)
							etable[i] = ft_char_index(face, i);
						/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1872 */
						if (etable[i] == 0 && symbolic)
						{
							int aglcode = pdf_lookup_agl(estrings[i]);
							if (aglcode)
								etable[i] = ft_char_index(face, aglcode);
						}
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
					estrings[i] = (char*) pdf_win_ansi[i]; /* discard const */
				}
			}
		}
		fz_unlock(ctx, FZ_LOCK_FREETYPE);

		/* SumatraPDF: handle symbolic Type 1 fonts with an implicit encoding similar to Adobe Reader */
		if (kind == TYPE1 && symbolic)
			for (i = 0; i < 256; i++)
				if (etable[i] && estrings[i] && !pdf_lookup_agl(estrings[i]))
					estrings[i] = (char *)pdf_standard[i];

		fontdesc->encoding = pdf_new_identity_cmap(ctx, 0, 1);
		fontdesc->size += pdf_cmap_size(ctx, fontdesc->encoding);
		fontdesc->cid_to_gid_len = 256;
		fontdesc->cid_to_gid = etable;

		pdf_load_to_unicode(xref, fontdesc, estrings, NULL, pdf_dict_gets(dict, "ToUnicode"));
		/* RJW: "cannot load to_unicode" */

	skip_encoding:

		/* Widths */

		pdf_set_default_hmtx(ctx, fontdesc, fontdesc->missing_width);

		widths = pdf_dict_gets(dict, "Widths");
		if (widths)
		{
			int first, last;

			first = pdf_to_int(pdf_dict_gets(dict, "FirstChar"));
			last = pdf_to_int(pdf_dict_gets(dict, "LastChar"));

			if (first < 0 || last > 255 || first > last)
				first = last = 0;

			for (i = 0; i < last - first + 1; i++)
			{
				int wid = pdf_to_int(pdf_array_get(widths, i));
				/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1616 */
				if (!wid && i >= pdf_array_len(widths))
				{
					fz_warn(ctx, "font width missing for glyph %d (%d %d R)", i + first, pdf_to_num(dict), pdf_to_gen(dict));
					FT_Set_Char_Size(face, 1000, 1000, 72, 72);
					wid = ft_width(ctx, fontdesc, i + first);
				}
				pdf_add_hmtx(ctx, fontdesc, i + first, i + first, wid);
			}
		}
		else
		{
			fz_lock(ctx, FZ_LOCK_FREETYPE);
			fterr = FT_Set_Char_Size(face, 1000, 1000, 72, 72);
			if (fterr)
				fz_warn(ctx, "freetype set character size: %s", ft_error_string(fterr));
			for (i = 0; i < 256; i++)
			{
				pdf_add_hmtx(ctx, fontdesc, i, i, ft_width(ctx, fontdesc, i));
			}
			fz_unlock(ctx, FZ_LOCK_FREETYPE);
		}

		pdf_end_hmtx(ctx, fontdesc);
	}
	fz_catch(ctx)
	{
		if (fontdesc && etable != fontdesc->cid_to_gid)
			fz_free(ctx, etable);
		pdf_drop_font(ctx, fontdesc);
		fz_throw(ctx, "cannot load simple font (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict));
	}
	return fontdesc;
}

/*
 * CID Fonts
 */

static pdf_font_desc *
load_cid_font(pdf_document *xref, pdf_obj *dict, pdf_obj *encoding, pdf_obj *to_unicode)
{
	pdf_obj *widths;
	pdf_obj *descriptor;
	pdf_font_desc *fontdesc;
	FT_Face face;
	int kind;
	char collection[256];
	char *basefont;
	int i, k, fterr;
	pdf_obj *obj;
	int dw;
	fz_context *ctx = xref->ctx;

	fz_var(fontdesc);

	fz_try(ctx)
	{
		/* Get font name and CID collection */

		basefont = pdf_to_name(pdf_dict_gets(dict, "BaseFont"));

		{
			pdf_obj *cidinfo;
			char tmpstr[64];
			int tmplen;

			cidinfo = pdf_dict_gets(dict, "CIDSystemInfo");
			if (!cidinfo)
				fz_throw(ctx, "cid font is missing info");

			obj = pdf_dict_gets(cidinfo, "Registry");
			tmplen = MIN(sizeof tmpstr - 1, pdf_to_str_len(obj));
			memcpy(tmpstr, pdf_to_str_buf(obj), tmplen);
			tmpstr[tmplen] = '\0';
			fz_strlcpy(collection, tmpstr, sizeof collection);

			fz_strlcat(collection, "-", sizeof collection);

			obj = pdf_dict_gets(cidinfo, "Ordering");
			tmplen = MIN(sizeof tmpstr - 1, pdf_to_str_len(obj));
			memcpy(tmpstr, pdf_to_str_buf(obj), tmplen);
			tmpstr[tmplen] = '\0';
			fz_strlcat(collection, tmpstr, sizeof collection);
		}

		/* Load font file */

		fontdesc = pdf_new_font_desc(ctx);

		descriptor = pdf_dict_gets(dict, "FontDescriptor");
		if (!descriptor)
			fz_throw(ctx, "syntaxerror: missing font descriptor");
		pdf_load_font_descriptor(fontdesc, xref, descriptor, collection, basefont, 1);

		face = fontdesc->font->ft_face;
		kind = ft_kind(face);

		/* Encoding */

		if (pdf_is_name(encoding))
		{
			if (!strcmp(pdf_to_name(encoding), "Identity-H"))
				fontdesc->encoding = pdf_new_identity_cmap(ctx, 0, 2);
			else if (!strcmp(pdf_to_name(encoding), "Identity-V"))
				fontdesc->encoding = pdf_new_identity_cmap(ctx, 1, 2);
			else
				fontdesc->encoding = pdf_load_system_cmap(ctx, pdf_to_name(encoding));
		}
		else if (pdf_is_indirect(encoding))
		{
			fontdesc->encoding = pdf_load_embedded_cmap(xref, encoding);
		}
		else
		{
			fz_throw(ctx, "syntaxerror: font missing encoding");
		}
		fontdesc->size += pdf_cmap_size(ctx, fontdesc->encoding);

		pdf_set_font_wmode(ctx, fontdesc, pdf_cmap_wmode(ctx, fontdesc->encoding));

		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1565 */
		if (kind == TRUETYPE || !strcmp(pdf_to_name(pdf_dict_gets(dict, "Subtype")), "CIDFontType2"))
		{
			pdf_obj *cidtogidmap;

			cidtogidmap = pdf_dict_gets(dict, "CIDToGIDMap");
			if (pdf_is_indirect(cidtogidmap))
			{
				fz_buffer *buf;

				buf = pdf_load_stream(xref, pdf_to_num(cidtogidmap), pdf_to_gen(cidtogidmap));

				fontdesc->cid_to_gid_len = (buf->len) / 2;
				fontdesc->cid_to_gid = fz_malloc_array(ctx, fontdesc->cid_to_gid_len, sizeof(unsigned short));
				fontdesc->size += fontdesc->cid_to_gid_len * sizeof(unsigned short);
				for (i = 0; i < fontdesc->cid_to_gid_len; i++)
					fontdesc->cid_to_gid[i] = (buf->data[i * 2] << 8) + buf->data[i * 2 + 1];

				fz_drop_buffer(ctx, buf);
			}

			/* if truetype font is external, cidtogidmap should not be identity */
			/* so we map from cid to unicode and then map that through the (3 1) */
			/* unicode cmap to get a glyph id */
			else if (fontdesc->font->ft_substitute)
			{
				fterr = FT_Select_Charmap(face, ft_encoding_unicode);
				if (fterr)
				{
					fz_throw(ctx, "fonterror: no unicode cmap when emulating CID font: %s", ft_error_string(fterr));
				}

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
				/* RJW: "cannot load system cmap %s", collection */
			}
		}

		pdf_load_to_unicode(xref, fontdesc, NULL, collection, to_unicode);
		/* RJW: "cannot load to_unicode" */

		/* Horizontal */

		dw = 1000;
		obj = pdf_dict_gets(dict, "DW");
		if (obj)
			dw = pdf_to_int(obj);
		pdf_set_default_hmtx(ctx, fontdesc, dw);

		widths = pdf_dict_gets(dict, "W");
		if (widths)
		{
			int c0, c1, w, n, m;

			n = pdf_array_len(widths);
			for (i = 0; i < n; )
			{
				c0 = pdf_to_int(pdf_array_get(widths, i));
				obj = pdf_array_get(widths, i + 1);
				if (pdf_is_array(obj))
				{
					m = pdf_array_len(obj);
					for (k = 0; k < m; k++)
					{
						w = pdf_to_int(pdf_array_get(obj, k));
						pdf_add_hmtx(ctx, fontdesc, c0 + k, c0 + k, w);
					}
					i += 2;
				}
				else
				{
					c1 = pdf_to_int(obj);
					w = pdf_to_int(pdf_array_get(widths, i + 2));
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

			obj = pdf_dict_gets(dict, "DW2");
			if (obj)
			{
				dw2y = pdf_to_int(pdf_array_get(obj, 0));
				dw2w = pdf_to_int(pdf_array_get(obj, 1));
			}

			pdf_set_default_vmtx(ctx, fontdesc, dw2y, dw2w);

			widths = pdf_dict_gets(dict, "W2");
			if (widths)
			{
				int c0, c1, w, x, y, n;

				n = pdf_array_len(widths);
				for (i = 0; i < n; )
				{
					c0 = pdf_to_int(pdf_array_get(widths, i));
					obj = pdf_array_get(widths, i + 1);
					if (pdf_is_array(obj))
					{
						int m = pdf_array_len(obj);
						for (k = 0; k * 3 < m; k ++)
						{
							w = pdf_to_int(pdf_array_get(obj, k * 3 + 0));
							x = pdf_to_int(pdf_array_get(obj, k * 3 + 1));
							y = pdf_to_int(pdf_array_get(obj, k * 3 + 2));
							pdf_add_vmtx(ctx, fontdesc, c0 + k, c0 + k, x, y, w);
						}
						i += 2;
					}
					else
					{
						c1 = pdf_to_int(obj);
						w = pdf_to_int(pdf_array_get(widths, i + 2));
						x = pdf_to_int(pdf_array_get(widths, i + 3));
						y = pdf_to_int(pdf_array_get(widths, i + 4));
						pdf_add_vmtx(ctx, fontdesc, c0, c1, x, y, w);
						i += 5;
					}
				}
			}

			pdf_end_vmtx(ctx, fontdesc);
		}
	}
	fz_catch(ctx)
	{
		pdf_drop_font(ctx, fontdesc);
		fz_throw(ctx, "cannot load cid font (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict));
	}

	return fontdesc;
}

static pdf_font_desc *
pdf_load_type0_font(pdf_document *xref, pdf_obj *dict)
{
	pdf_obj *dfonts;
	pdf_obj *dfont;
	pdf_obj *subtype;
	pdf_obj *encoding;
	pdf_obj *to_unicode;

	dfonts = pdf_dict_gets(dict, "DescendantFonts");
	if (!dfonts)
		fz_throw(xref->ctx, "cid font is missing descendant fonts");

	dfont = pdf_array_get(dfonts, 0);

	subtype = pdf_dict_gets(dfont, "Subtype");
	encoding = pdf_dict_gets(dict, "Encoding");
	to_unicode = pdf_dict_gets(dict, "ToUnicode");

	if (pdf_is_name(subtype) && !strcmp(pdf_to_name(subtype), "CIDFontType0"))
		return load_cid_font(xref, dfont, encoding, to_unicode);
	else if (pdf_is_name(subtype) && !strcmp(pdf_to_name(subtype), "CIDFontType2"))
		return load_cid_font(xref, dfont, encoding, to_unicode);
	else
		fz_throw(xref->ctx, "syntaxerror: unknown cid font type");
	/* RJW: "cannot load descendant font (%d %d R)", pdf_to_num(dfont), pdf_to_gen(dfont) */
	return NULL; /* Stupid MSVC */
}

/*
 * FontDescriptor
 */

static void
pdf_load_font_descriptor(pdf_font_desc *fontdesc, pdf_document *xref, pdf_obj *dict, char *collection, char *basefont, int has_encoding)
{
	pdf_obj *obj1, *obj2, *obj3, *obj;
	char *fontname;
	char *origname;
	FT_Face face;
	fz_context *ctx = xref->ctx;

	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1666 */
	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1842 */
	if (strchr(basefont, '+') && pdf_is_name(pdf_dict_gets(dict, "FontName")))
		origname = pdf_to_name(pdf_dict_gets(dict, "FontName"));
	else
		origname = basefont;
	fontname = clean_font_name(origname);

	fontdesc->flags = pdf_to_int(pdf_dict_gets(dict, "Flags"));
	fontdesc->italic_angle = pdf_to_real(pdf_dict_gets(dict, "ItalicAngle"));
	fontdesc->ascent = pdf_to_real(pdf_dict_gets(dict, "Ascent"));
	fontdesc->descent = pdf_to_real(pdf_dict_gets(dict, "Descent"));
	fontdesc->cap_height = pdf_to_real(pdf_dict_gets(dict, "CapHeight"));
	fontdesc->x_height = pdf_to_real(pdf_dict_gets(dict, "XHeight"));
	fontdesc->missing_width = pdf_to_real(pdf_dict_gets(dict, "MissingWidth"));

	obj1 = pdf_dict_gets(dict, "FontFile");
	obj2 = pdf_dict_gets(dict, "FontFile2");
	obj3 = pdf_dict_gets(dict, "FontFile3");
	obj = obj1 ? obj1 : obj2 ? obj2 : obj3;

	if (pdf_is_indirect(obj))
	{
		fz_try(ctx)
		{
			pdf_load_embedded_font(fontdesc, xref, obj);
		}
		fz_catch(ctx)
		{
			fz_warn(ctx, "ignored error when loading embedded font; attempting to load system font");
			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1616 */
			if (strlen(fontname) > 7 && fontname[6] == '+')
			{
				fz_try(ctx)
				{
					pdf_load_system_font(ctx, fontdesc, fontname + 7, collection, has_encoding);
				}
				fz_catch(ctx) { }
			}
			if (!fontdesc->font)
			if (origname != fontname)
				pdf_load_builtin_font(ctx, fontdesc, fontname);
			else
				pdf_load_system_font(ctx, fontdesc, fontname, collection, has_encoding);
			/* RJW: "cannot load font descriptor (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict) */
		}
	}
	else
	{
		if (origname != fontname && 0 /* SumatraPDF: prefer system fonts to the built-in ones */)
			pdf_load_builtin_font(ctx, fontdesc, fontname);
		else
			pdf_load_system_font(ctx, fontdesc, fontname, collection, has_encoding);
		/* RJW: "cannot load font descriptor (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict) */
	}

	fz_strlcpy(fontdesc->font->name, fontname, sizeof fontdesc->font->name);

	/* Check for DynaLab fonts that must use hinting */
	face = fontdesc->font->ft_face;
	if (ft_kind(face) == TRUETYPE)
	{
		if (FT_IS_TRICKY(face) || is_dynalab(fontdesc->font->name))
			fontdesc->font->ft_hint = 1;
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
	};

	font->width_count = n + 1;
	font->width_table = fz_malloc_array(ctx, font->width_count, sizeof(int));
	fontdesc->size += font->width_count * sizeof(int);

	for (i = 0; i < fontdesc->hmtx_len; i++)
	{
		for (k = fontdesc->hmtx[i].lo; k <= fontdesc->hmtx[i].hi; k++)
		{
			cid = pdf_lookup_cmap(fontdesc->encoding, k);
			gid = pdf_font_cid_to_gid(ctx, fontdesc, cid);
			/* SumatraPDF: Widths are per cid, so there could be clashes, if two cids
			               map to the same gid (for now, prefer the non-zero width) */
			if (gid >= 0 && gid < font->width_count && fontdesc->hmtx[i].w != 0)
				font->width_table[gid] = fontdesc->hmtx[i].w;
		}
	}
}

pdf_font_desc *
pdf_load_font(pdf_document *xref, pdf_obj *rdb, pdf_obj *dict)
{
	char *subtype;
	pdf_obj *dfonts;
	pdf_obj *charprocs;
	fz_context *ctx = xref->ctx;
	pdf_font_desc *fontdesc;

	if ((fontdesc = pdf_find_item(ctx, pdf_free_font_imp, dict)))
	{
		return fontdesc;
	}

	subtype = pdf_to_name(pdf_dict_gets(dict, "Subtype"));
	dfonts = pdf_dict_gets(dict, "DescendantFonts");
	charprocs = pdf_dict_gets(dict, "CharProcs");

	if (subtype && !strcmp(subtype, "Type0"))
		fontdesc = pdf_load_type0_font(xref, dict);
	else if (subtype && !strcmp(subtype, "Type1"))
		fontdesc = pdf_load_simple_font(xref, dict);
	else if (subtype && !strcmp(subtype, "MMType1"))
		fontdesc = pdf_load_simple_font(xref, dict);
	else if (subtype && !strcmp(subtype, "TrueType"))
		fontdesc = pdf_load_simple_font(xref, dict);
	else if (subtype && !strcmp(subtype, "Type3"))
		fontdesc = pdf_load_type3_font(xref, rdb, dict);
	else if (charprocs)
	{
		fz_warn(ctx, "unknown font format, guessing type3.");
		fontdesc = pdf_load_type3_font(xref, rdb, dict);
	}
	else if (dfonts)
	{
		fz_warn(ctx, "unknown font format, guessing type0.");
		fontdesc = pdf_load_type0_font(xref, dict);
	}
	else
	{
		fz_warn(ctx, "unknown font format, guessing type1 or truetype.");
		fontdesc = pdf_load_simple_font(xref, dict);
	}
	/* RJW: "cannot load font (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict) */

	/* Save the widths to stretch non-CJK substitute fonts */
	if (fontdesc->font->ft_substitute && !fontdesc->to_ttf_cmap)
		pdf_make_width_table(ctx, fontdesc);

	pdf_store_item(ctx, dict, fontdesc, fontdesc->size);

	return fontdesc;
}

void
pdf_print_font(fz_context *ctx, pdf_font_desc *fontdesc)
{
	int i;

	printf("fontdesc {\n");

	if (fontdesc->font->ft_face)
		printf("\tfreetype font\n");
	if (fontdesc->font->t3procs)
		printf("\ttype3 font\n");

	printf("\twmode %d\n", fontdesc->wmode);
	printf("\tDW %d\n", fontdesc->dhmtx.w);

	printf("\tW {\n");
	for (i = 0; i < fontdesc->hmtx_len; i++)
		printf("\t\t<%04x> <%04x> %d\n",
			fontdesc->hmtx[i].lo, fontdesc->hmtx[i].hi, fontdesc->hmtx[i].w);
	printf("\t}\n");

	if (fontdesc->wmode)
	{
		printf("\tDW2 [%d %d]\n", fontdesc->dvmtx.y, fontdesc->dvmtx.w);
		printf("\tW2 {\n");
		for (i = 0; i < fontdesc->vmtx_len; i++)
			printf("\t\t<%04x> <%04x> %d %d %d\n", fontdesc->vmtx[i].lo, fontdesc->vmtx[i].hi,
				fontdesc->vmtx[i].x, fontdesc->vmtx[i].y, fontdesc->vmtx[i].w);
		printf("\t}\n");
	}
}
