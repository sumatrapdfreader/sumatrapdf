// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#ifdef FT_FONT_FORMATS_H
#include FT_FONT_FORMATS_H
#else
#include FT_XFREE86_H
#endif
#include FT_TRUETYPE_TABLES_H

#ifndef FT_SFNT_HEAD
#define FT_SFNT_HEAD ft_sfnt_head
#endif

static int ft_font_file_kind(fz_context *ctx, FT_Face face)
{
	const char *kind;
	fz_ft_lock(ctx);
#ifdef FT_FONT_FORMATS_H
	kind = FT_Get_Font_Format(face);
#else
	kind = FT_Get_X11_Font_Format(face);
#endif
	fz_ft_unlock(ctx);
	if (!strcmp(kind, "TrueType")) return 2;
	if (!strcmp(kind, "Type 1")) return 1;
	if (!strcmp(kind, "CFF")) return 3;
	if (!strcmp(kind, "CID Type 1")) return 1;
	return 0;
}

static int is_ttc(fz_font *font)
{
	if (!font || !font->buffer || font->buffer->len < 4)
		return 0;
	return !memcmp(font->buffer->data, "ttcf", 4);
}

static int is_truetype(fz_context *ctx, FT_Face face)
{
	return ft_font_file_kind(ctx, face) == 2;
}

static int is_postscript(fz_context *ctx, FT_Face face)
{
	int kind = ft_font_file_kind(ctx, face);
	return (kind == 1 || kind == 3);
}

static int is_builtin_font(fz_context *ctx, fz_font *font)
{
	int size;
	unsigned char *data;
	if (!font->buffer)
		return 0;
	fz_buffer_storage(ctx, font->buffer, &data);
	return fz_lookup_base14_font(ctx, pdf_clean_font_name(font->name), &size) == data;
}

static pdf_obj*
pdf_add_font_file(fz_context *ctx, pdf_document *doc, fz_font *font)
{
	fz_buffer *buf = font->buffer;
	pdf_obj *obj = NULL;
	pdf_obj *ref = NULL;
	int drop_buf = 0;

	fz_var(obj);
	fz_var(ref);

	/* Check for substitute fonts */
	if (font->flags.ft_substitute)
		return NULL;

	if (is_ttc(font))
	{
		buf = NULL;
		drop_buf = 1;
		buf = fz_extract_ttf_from_ttc(ctx, font);
	}

	fz_try(ctx)
	{
		size_t len = fz_buffer_storage(ctx, buf, NULL);
		int is_opentype;
		obj = pdf_new_dict(ctx, doc, 3);
		pdf_dict_put_int(ctx, obj, PDF_NAME(Length1), (int)len);
		switch (ft_font_file_kind(ctx, font->ft_face))
		{
		case 1:
			/* TODO: these may not be the correct values, but I doubt it matters */
			pdf_dict_put_int(ctx, obj, PDF_NAME(Length2), len);
			pdf_dict_put_int(ctx, obj, PDF_NAME(Length3), 0);
			break;
		case 2:
			break;
		case 3:
			fz_ft_lock(ctx);
			is_opentype = !!FT_Get_Sfnt_Table(font->ft_face, FT_SFNT_HEAD);
			fz_ft_unlock(ctx);
			if (is_opentype)
				pdf_dict_put(ctx, obj, PDF_NAME(Subtype), PDF_NAME(OpenType));
			else
				pdf_dict_put(ctx, obj, PDF_NAME(Subtype), PDF_NAME(CIDFontType0C));
			break;
		}
		ref = pdf_add_object(ctx, doc, obj);
		pdf_update_stream(ctx, doc, ref, buf, 0);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, obj);
		if (drop_buf)
			fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, ref);
		fz_rethrow(ctx);
	}
	return ref;
}

static void
pdf_add_font_descriptor(fz_context *ctx, pdf_document *doc, pdf_obj *fobj, fz_font *font)
{
	FT_Face face = font->ft_face;
	pdf_obj *fdobj = NULL;
	pdf_obj *fileref;
	fz_rect bbox;

	fdobj = pdf_new_dict(ctx, doc, 10);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, fdobj, PDF_NAME(Type), PDF_NAME(FontDescriptor));
		pdf_dict_put_name(ctx, fdobj, PDF_NAME(FontName), font->name);

		bbox.x0 = font->bbox.x0 * 1000;
		bbox.y0 = font->bbox.y0 * 1000;
		bbox.x1 = font->bbox.x1 * 1000;
		bbox.y1 = font->bbox.y1 * 1000;
		pdf_dict_put_rect(ctx, fdobj, PDF_NAME(FontBBox), bbox);

		pdf_dict_put_int(ctx, fdobj, PDF_NAME(ItalicAngle), 0);
		pdf_dict_put_int(ctx, fdobj, PDF_NAME(Ascent), face->ascender * 1000.0f / face->units_per_EM);
		pdf_dict_put_int(ctx, fdobj, PDF_NAME(Descent), face->descender * 1000.0f / face->units_per_EM);
		pdf_dict_put_int(ctx, fdobj, PDF_NAME(StemV), 80);
		pdf_dict_put_int(ctx, fdobj, PDF_NAME(Flags), PDF_FD_NONSYMBOLIC);

		fileref = pdf_add_font_file(ctx, doc, font);
		if (fileref)
		{
			switch (ft_font_file_kind(ctx, face))
			{
			default:
			case 1: pdf_dict_put_drop(ctx, fdobj, PDF_NAME(FontFile), fileref); break;
			case 2: pdf_dict_put_drop(ctx, fdobj, PDF_NAME(FontFile2), fileref); break;
			case 3: pdf_dict_put_drop(ctx, fdobj, PDF_NAME(FontFile3), fileref); break;
			}
		}

		pdf_dict_put_drop(ctx, fobj, PDF_NAME(FontDescriptor), pdf_add_object(ctx, doc, fdobj));
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, fdobj);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_add_simple_font_widths(fz_context *ctx, pdf_document *doc, pdf_obj *fobj, fz_font *font, const char * const encoding[])
{
	int width_table[256];
	pdf_obj *widths;
	int i, first, last;

	first = 0;
	last = 0;

	for (i = 0; i < 256; ++i)
	{
		int glyph = 0;
		if (encoding[i])
		{
			glyph = fz_encode_character_by_glyph_name(ctx, font, encoding[i]);
		}
		if (glyph > 0)
		{
			if (!first)
				first = i;
			last = i;
			width_table[i] = fz_advance_glyph(ctx, font, glyph, 0) * 1000;
		}
		else
			width_table[i] = 0;
	}

	widths = pdf_new_array(ctx, doc, last - first + 1);
	pdf_dict_put_drop(ctx, fobj, PDF_NAME(Widths), widths);
	for (i = first; i <= last; ++i)
		pdf_array_push_int(ctx, widths, width_table[i]);
	pdf_dict_put_int(ctx, fobj, PDF_NAME(FirstChar), first);
	pdf_dict_put_int(ctx, fobj, PDF_NAME(LastChar), last);
}

static void
pdf_add_cid_system_info(fz_context *ctx, pdf_document *doc, pdf_obj *fobj, const char *reg, const char *ord, int supp)
{
	pdf_obj *csi = pdf_dict_put_dict(ctx, fobj, PDF_NAME(CIDSystemInfo), 3);
	pdf_dict_put_string(ctx, csi, PDF_NAME(Registry), reg, strlen(reg));
	pdf_dict_put_string(ctx, csi, PDF_NAME(Ordering), ord, strlen(ord));
	pdf_dict_put_int(ctx, csi, PDF_NAME(Supplement), supp);
}

/* Different states of starting, same width as last, or consecutive glyph */
enum { FW_START = 0, FW_SAME, FW_DIFFER };

/* ToDo: Ignore the default sized characters */
static void
pdf_add_cid_font_widths(fz_context *ctx, pdf_document *doc, pdf_obj *fobj, fz_font *font)
{
	FT_Face face = font->ft_face;
	pdf_obj *run_obj = NULL;
	pdf_obj *fw;
	int curr_code;
	int curr_size;
	int first_code;
	int state = FW_START;

	fz_var(run_obj);

	fw = pdf_add_new_array(ctx, doc, 10);
	fz_try(ctx)
	{
		curr_code = 0;
		curr_size = fz_advance_glyph(ctx, font, 0, 0) * 1000;
		first_code = 0;

		for (curr_code = 1; curr_code < face->num_glyphs; curr_code++)
		{
			int prev_size = curr_size;

			curr_size = fz_advance_glyph(ctx, font, curr_code, 0) * 1000;

			/* So each time around the loop when we reach here, we have sizes
			 * for curr_code-1 (prev_size) and curr_code (curr_size), neither
			 * of which have been published yet. By the time we reach the end
			 * of the loop we must have disposed of prev_size. */
			switch (state)
			{
			case FW_SAME:
				/* Until now, we've been in a run of identical values, extending
				 * from first_code to curr_code-1. If the current and prev sizes
				 * match, then this now extends from first_code to curr_code and
				 * we don't need to do anything. If not, we need to flush and
				 * restart. */
				if (curr_size != prev_size)
				{
					/* Add three entries. First cid, last cid and width */
					pdf_array_push_int(ctx, fw, first_code);
					pdf_array_push_int(ctx, fw, curr_code-1);
					pdf_array_push_int(ctx, fw, prev_size);
					/* And the new first code is our current code. */
					first_code = curr_code;
					state = FW_START;
				}
				break;
			case FW_DIFFER:
				/* Until now, we've been in a run of differing values, extending
				 * from first_code to curr_code-1 (though prev_size, the size for
				 * curr_code-1 has not yet been pushed). */
				if (curr_size == prev_size)
				{
					/* Same width, so flush the run of differences. */
					pdf_array_push_int(ctx, fw, first_code);
					pdf_array_push(ctx, fw, run_obj);
					pdf_drop_obj(ctx, run_obj);
					run_obj = NULL;
					/* Start a new 'same' entry starting with curr_code-1.
					 * i.e. the prev size is not put in the run. */
					state = FW_SAME;
					first_code = curr_code-1;
				}
				else
				{
					/* Continue our differing run by adding prev size to run_obj. */
					pdf_array_push_int(ctx, run_obj, prev_size);
				}
				break;
			case FW_START:
				/* Starting fresh. Determine our state. */
				if (curr_size == prev_size)
				{
					state = FW_SAME;
				}
				else
				{
					run_obj = pdf_new_array(ctx, doc, 10);
					pdf_array_push_int(ctx, run_obj, prev_size);
					state = FW_DIFFER;
				}
				break;
			}
		}

		/* So curr_code-1 is the last valid char, and curr_size was its size. */
		switch (state)
		{
		case FW_SAME:
			/* We have an unflushed run of same entries. */
			if (first_code != curr_code-1)
			{
				pdf_array_push_int(ctx, fw, first_code);
				pdf_array_push_int(ctx, fw, curr_code-1);
				pdf_array_push_int(ctx, fw, curr_size);
			}
			break;
		case FW_DIFFER:
			/* We have not yet pushed curr_size to the object. */
			pdf_array_push_int(ctx, fw, first_code);
			pdf_array_push_int(ctx, run_obj, curr_size);
			pdf_array_push(ctx, fw, run_obj);
			pdf_drop_obj(ctx, run_obj);
			run_obj = NULL;
			break;
		case FW_START:
			/* Lone wolf! */
			pdf_array_push_int(ctx, fw, curr_code-1);
			pdf_array_push_int(ctx, fw, curr_code-1);
			pdf_array_push_int(ctx, fw, curr_size);
			break;
		}

		if (font->width_table != NULL)
			pdf_dict_put_int(ctx, fobj, PDF_NAME(DW), font->width_default);
		if (pdf_array_len(ctx, fw) > 0)
			pdf_dict_put(ctx, fobj, PDF_NAME(W), fw);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, fw);
		pdf_drop_obj(ctx, run_obj);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* Descendant font construction used for CID font creation from ttf or Adobe type1 */
static pdf_obj*
pdf_add_descendant_cid_font(fz_context *ctx, pdf_document *doc, fz_font *font)
{
	FT_Face face = font->ft_face;
	pdf_obj *fobj, *fref;
	const char *ps_name;

	fobj = pdf_new_dict(ctx, doc, 3);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, fobj, PDF_NAME(Type), PDF_NAME(Font));
		if (is_truetype(ctx, face))
			pdf_dict_put(ctx, fobj, PDF_NAME(Subtype), PDF_NAME(CIDFontType2));
		else
			pdf_dict_put(ctx, fobj, PDF_NAME(Subtype), PDF_NAME(CIDFontType0));

		pdf_add_cid_system_info(ctx, doc, fobj, "Adobe", "Identity", 0);

		fz_ft_lock(ctx);
		ps_name = FT_Get_Postscript_Name(face);
		fz_ft_unlock(ctx);
		if (ps_name)
			pdf_dict_put_name(ctx, fobj, PDF_NAME(BaseFont), ps_name);
		else
			pdf_dict_put_name(ctx, fobj, PDF_NAME(BaseFont), font->name);

		pdf_add_font_descriptor(ctx, doc, fobj, font);

		/* We may have a cid font already with width info in source font and no cmap in the ft face */
		pdf_add_cid_font_widths(ctx, doc, fobj, font);

		fref = pdf_add_object(ctx, doc, fobj);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, fobj);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return fref;
}

static int next_range(int *table, int size, int k)
{
	int n;
	for (n = 1; k + n < size; ++n)
	{
		if ((k & 0xFF00) != ((k+n) & 0xFF00)) /* high byte changes */
			break;
		if (table[k] + n != table[k+n])
			break;
	}
	return n;
}

/* Create the ToUnicode CMap. */
static void
pdf_add_to_unicode(fz_context *ctx, pdf_document *doc, pdf_obj *fobj, fz_font *font)
{
	FT_Face face = font->ft_face;
	fz_buffer *buf = NULL;

	int *table;
	int num_seq = 0;
	int num_chr = 0;
	int n, k;

	/* Populate reverse cmap table */
	{
		FT_ULong ucs;
		FT_UInt gid;

		table = fz_calloc(ctx, face->num_glyphs, sizeof *table);
		fz_ft_lock(ctx);
		ucs = FT_Get_First_Char(face, &gid);
		while (gid > 0)
		{
			if (gid < (FT_ULong)face->num_glyphs && face->num_glyphs > 0)
				table[gid] = ucs;
			ucs = FT_Get_Next_Char(face, ucs, &gid);
		}
		fz_ft_unlock(ctx);
	}

	for (k = 0; k < face->num_glyphs; k += n)
	{
		n = next_range(table, face->num_glyphs, k);
		if (n > 1)
			++num_seq;
		else if (table[k] > 0)
			++num_chr;
	}

	/* No mappings available... */
	if (num_seq + num_chr == 0)
	{
		fz_warn(ctx, "cannot create ToUnicode mapping for %s", font->name);
		fz_free(ctx, table);
		return;
	}

	fz_var(buf);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 0);

		/* Header boiler plate */
		fz_append_string(ctx, buf, "/CIDInit /ProcSet findresource begin\n");
		fz_append_string(ctx, buf, "12 dict begin\n");
		fz_append_string(ctx, buf, "begincmap\n");
		fz_append_string(ctx, buf, "/CIDSystemInfo <</Registry(Adobe)/Ordering(UCS)/Supplement 0>> def\n");
		fz_append_string(ctx, buf, "/CMapName /Adobe-Identity-UCS def\n");
		fz_append_string(ctx, buf, "/CMapType 2 def\n");
		fz_append_string(ctx, buf, "1 begincodespacerange\n");
		fz_append_string(ctx, buf, "<0000> <FFFF>\n");
		fz_append_string(ctx, buf, "endcodespacerange\n");

		/* Note to have a valid CMap, the number of entries in table set can
		 * not exceed 100, so we have to break into multiple tables. Also, note
		 * that to reduce the file size we should be looking for sequential
		 * ranges. Per Adobe technical note #5411, we can't have a range
		 * cross a boundary where the high order byte changes */

		/* First the ranges */
		if (num_seq > 0)
		{
			int count = 0;
			if (num_seq > 100)
			{
				fz_append_string(ctx, buf, "100 beginbfrange\n");
				num_seq -= 100;
			}
			else
				fz_append_printf(ctx, buf, "%d beginbfrange\n", num_seq);
			for (k = 0; k < face->num_glyphs; k += n)
			{
				n = next_range(table, face->num_glyphs, k);
				if (n > 1)
				{
					if (count == 100)
					{
						fz_append_string(ctx, buf, "endbfrange\n");
						if (num_seq > 100)
						{
							fz_append_string(ctx, buf, "100 beginbfrange\n");
							num_seq -= 100;
						}
						else
							fz_append_printf(ctx, buf, "%d beginbfrange\n", num_seq);
						count = 0;
					}
					fz_append_printf(ctx, buf, "<%04x> <%04x> <%04x>\n", k, k+n-1, table[k]);
					++count;
				}
			}
			fz_append_string(ctx, buf, "endbfrange\n");
		}

		/* Then the singles */
		if (num_chr > 0)
		{
			int count = 0;
			if (num_chr > 100)
			{
				fz_append_string(ctx, buf, "100 beginbfchar\n");
				num_chr -= 100;
			}
			else
				fz_append_printf(ctx, buf, "%d beginbfchar\n", num_chr);
			for (k = 0; k < face->num_glyphs; k += n)
			{
				n = next_range(table, face->num_glyphs, k);
				if (n == 1 && table[k] > 0)
				{
					if (count == 100)
					{
						fz_append_string(ctx, buf, "endbfchar\n");
						if (num_chr > 100)
						{
							fz_append_string(ctx, buf, "100 beginbfchar\n");
							num_chr -= 100;
						}
						else
							fz_append_printf(ctx, buf, "%d beginbfchar\n", num_chr);
						count = 0;
					}
					fz_append_printf(ctx, buf, "<%04x> <%04x>\n", k, table[k]);
					++count;
				}
			}
			fz_append_string(ctx, buf, "endbfchar\n");
		}

		/* Trailer boiler plate */
		fz_append_string(ctx, buf, "endcmap\n");
		fz_append_string(ctx, buf, "CMapName currentdict /CMap defineresource pop\n");
		fz_append_string(ctx, buf, "end\nend\n");

		pdf_dict_put_drop(ctx, fobj, PDF_NAME(ToUnicode), pdf_add_stream(ctx, doc, buf, NULL, 0));
	}
	fz_always(ctx)
	{
		fz_free(ctx, table);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

pdf_obj *
pdf_add_cid_font(fz_context *ctx, pdf_document *doc, fz_font *font)
{
	pdf_obj *fobj = NULL;
	pdf_obj *fref = NULL;
	pdf_obj *dfonts = NULL;
	pdf_font_resource_key key;

	fref = pdf_find_font_resource(ctx, doc, PDF_CID_FONT_RESOURCE, 0, font, &key);
	if (fref)
		return fref;

	fobj = pdf_add_new_dict(ctx, doc, 10);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, fobj, PDF_NAME(Type), PDF_NAME(Font));
		pdf_dict_put(ctx, fobj, PDF_NAME(Subtype), PDF_NAME(Type0));
		pdf_dict_put_name(ctx, fobj, PDF_NAME(BaseFont), font->name);
		pdf_dict_put(ctx, fobj, PDF_NAME(Encoding), PDF_NAME(Identity_H));
		pdf_add_to_unicode(ctx, doc, fobj, font);

		dfonts = pdf_dict_put_array(ctx, fobj, PDF_NAME(DescendantFonts), 1);
		pdf_array_push_drop(ctx, dfonts, pdf_add_descendant_cid_font(ctx, doc, font));

		fref = pdf_insert_font_resource(ctx, doc, &key, fobj);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, fobj);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return fref;
}

/* Create simple (8-bit encoding) fonts */

static void
pdf_add_simple_font_encoding_imp(fz_context *ctx, pdf_document *doc, pdf_obj *font, const char *glyph_names[])
{
	pdf_obj *enc, *diff;
	int i, last;

	enc = pdf_dict_put_dict(ctx, font, PDF_NAME(Encoding), 2);
	pdf_dict_put(ctx, enc, PDF_NAME(BaseEncoding), PDF_NAME(WinAnsiEncoding));
	diff = pdf_dict_put_array(ctx, enc, PDF_NAME(Differences), 129);
	last = 0;
	for (i = 128; i < 256; ++i)
	{
		const char *glyph = glyph_names[i];
		if (glyph)
		{
			if (last != i-1)
				pdf_array_push_int(ctx, diff, i);
			last = i;
			pdf_array_push_name(ctx, diff, glyph);
		}
	}
}

static void
pdf_add_simple_font_encoding(fz_context *ctx, pdf_document *doc, pdf_obj *fobj, int encoding)
{
	switch (encoding)
	{
	default:
	case PDF_SIMPLE_ENCODING_LATIN:
		pdf_dict_put(ctx, fobj, PDF_NAME(Encoding), PDF_NAME(WinAnsiEncoding));
		break;
	case PDF_SIMPLE_ENCODING_GREEK:
		pdf_add_simple_font_encoding_imp(ctx, doc, fobj, fz_glyph_name_from_iso8859_7);
		break;
	case PDF_SIMPLE_ENCODING_CYRILLIC:
		pdf_add_simple_font_encoding_imp(ctx, doc, fobj, fz_glyph_name_from_koi8u);
		break;
	}
}

pdf_obj *
pdf_add_simple_font(fz_context *ctx, pdf_document *doc, fz_font *font, int encoding)
{
	FT_Face face = font->ft_face;
	pdf_obj *fobj = NULL;
	pdf_obj *fref = NULL;
	const char **enc;
	pdf_font_resource_key key;

	fref = pdf_find_font_resource(ctx, doc, PDF_SIMPLE_FONT_RESOURCE, encoding, font, &key);
	if (fref)
		return fref;

	switch (encoding)
	{
	default:
	case PDF_SIMPLE_ENCODING_LATIN: enc = fz_glyph_name_from_windows_1252; break;
	case PDF_SIMPLE_ENCODING_GREEK: enc = fz_glyph_name_from_iso8859_7; break;
	case PDF_SIMPLE_ENCODING_CYRILLIC: enc = fz_glyph_name_from_koi8u; break;
	}

	fobj = pdf_add_new_dict(ctx, doc, 10);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, fobj, PDF_NAME(Type), PDF_NAME(Font));
		if (is_truetype(ctx, face))
			pdf_dict_put(ctx, fobj, PDF_NAME(Subtype), PDF_NAME(TrueType));
		else
			pdf_dict_put(ctx, fobj, PDF_NAME(Subtype), PDF_NAME(Type1));

		if (!is_builtin_font(ctx, font))
		{
			const char *ps_name;
			fz_ft_lock(ctx);
			ps_name = FT_Get_Postscript_Name(face);
			fz_ft_unlock(ctx);
			if (!ps_name)
				ps_name = font->name;
			pdf_dict_put_name(ctx, fobj, PDF_NAME(BaseFont), ps_name);
			pdf_add_simple_font_encoding(ctx, doc, fobj, encoding);
			pdf_add_simple_font_widths(ctx, doc, fobj, font, enc);
			pdf_add_font_descriptor(ctx, doc, fobj, font);
		}
		else
		{
			pdf_dict_put_name(ctx, fobj, PDF_NAME(BaseFont), pdf_clean_font_name(font->name));
			pdf_add_simple_font_encoding(ctx, doc, fobj, encoding);
			if (encoding != PDF_SIMPLE_ENCODING_LATIN)
				pdf_add_simple_font_widths(ctx, doc, fobj, font, enc);
		}

		fref = pdf_insert_font_resource(ctx, doc, &key, fobj);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, fobj);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
	return fref;
}

int
pdf_font_writing_supported(fz_context *ctx, fz_font *font)
{
	if (font->ft_face == NULL || font->buffer == NULL || font->buffer->len < 4 || !font->flags.embed || font->flags.never_embed)
		return 0;
	if (is_ttc(font))
		return 1;
	if (is_truetype(ctx, font->ft_face))
		return 1;
	if (is_postscript(ctx, font->ft_face))
		return 1;
	return 0;
}

pdf_obj *
pdf_add_cjk_font(fz_context *ctx, pdf_document *doc, fz_font *fzfont, int script, int wmode, int serif)
{
	pdf_obj *fref, *font, *subfont, *fontdesc;
	pdf_obj *dfonts;
	fz_rect bbox = { -200, -200, 1200, 1200 };
	pdf_font_resource_key key;
	int flags;

	const char *basefont, *encoding, *ordering;
	int supplement;

	switch (script)
	{
	default:
		script = FZ_ADOBE_CNS;
		/* fall through */
	case FZ_ADOBE_CNS: /* traditional chinese */
		basefont = serif ? "Ming" : "Fangti";
		encoding = wmode ? "UniCNS-UTF16-V" : "UniCNS-UTF16-H";
		ordering = "CNS1";
		supplement = 7;
		break;
	case FZ_ADOBE_GB: /* simplified chinese */
		basefont = serif ? "Song" : "Heiti";
		encoding = wmode ? "UniGB-UTF16-V" : "UniGB-UTF16-H";
		ordering = "GB1";
		supplement = 5;
		break;
	case FZ_ADOBE_JAPAN:
		basefont = serif ? "Mincho" : "Gothic";
		encoding = wmode ? "UniJIS-UTF16-V" : "UniJIS-UTF16-H";
		ordering = "Japan1";
		supplement = 6;
		break;
	case FZ_ADOBE_KOREA:
		basefont = serif ? "Batang" : "Dotum";
		encoding = wmode ? "UniKS-UTF16-V" : "UniKS-UTF16-H";
		ordering = "Korea1";
		supplement = 2;
		break;
	}

	flags = PDF_FD_SYMBOLIC;
	if (serif)
		flags |= PDF_FD_SERIF;

	fref = pdf_find_font_resource(ctx, doc, PDF_CJK_FONT_RESOURCE, script, fzfont, &key);
	if (fref)
		return fref;

	font = pdf_add_new_dict(ctx, doc, 5);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, font, PDF_NAME(Type), PDF_NAME(Font));
		pdf_dict_put(ctx, font, PDF_NAME(Subtype), PDF_NAME(Type0));
		pdf_dict_put_name(ctx, font, PDF_NAME(BaseFont), basefont);
		pdf_dict_put_name(ctx, font, PDF_NAME(Encoding), encoding);
		dfonts = pdf_dict_put_array(ctx, font, PDF_NAME(DescendantFonts), 1);
		pdf_array_push_drop(ctx, dfonts, subfont = pdf_add_new_dict(ctx, doc, 5));
		{
			pdf_dict_put(ctx, subfont, PDF_NAME(Type), PDF_NAME(Font));
			pdf_dict_put(ctx, subfont, PDF_NAME(Subtype), PDF_NAME(CIDFontType0));
			pdf_dict_put_name(ctx, subfont, PDF_NAME(BaseFont), basefont);
			pdf_add_cid_system_info(ctx, doc, subfont, "Adobe", ordering, supplement);
			fontdesc = pdf_add_new_dict(ctx, doc, 8);
			pdf_dict_put_drop(ctx, subfont, PDF_NAME(FontDescriptor), fontdesc);
			{
				pdf_dict_put(ctx, fontdesc, PDF_NAME(Type), PDF_NAME(FontDescriptor));
				pdf_dict_put_text_string(ctx, fontdesc, PDF_NAME(FontName), basefont);
				pdf_dict_put_rect(ctx, fontdesc, PDF_NAME(FontBBox), bbox);
				pdf_dict_put_int(ctx, fontdesc, PDF_NAME(Flags), flags);
				pdf_dict_put_int(ctx, fontdesc, PDF_NAME(ItalicAngle), 0);
				pdf_dict_put_int(ctx, fontdesc, PDF_NAME(Ascent), 1000);
				pdf_dict_put_int(ctx, fontdesc, PDF_NAME(Descent), -200);
				pdf_dict_put_int(ctx, fontdesc, PDF_NAME(StemV), 80);
			}
		}

		fref = pdf_insert_font_resource(ctx, doc, &key, font);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, font);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return fref;
}

pdf_obj *
pdf_add_substitute_font(fz_context *ctx, pdf_document *doc, fz_font *font)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "substitute font creation is not implemented yet");
}
