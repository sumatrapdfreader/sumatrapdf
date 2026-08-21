// Copyright (C) 2004-2021 Artifex Software, Inc.
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

#include <string.h>

/* Load or synthesize ToUnicode map for fonts */

static void
pdf_remap_cmap_range(fz_context *ctx, pdf_cmap *ucs_from_gid,
	unsigned int cpt, unsigned int gid, unsigned int n, pdf_cmap *ucs_from_cpt)
{
	unsigned int k;
	int ucsbuf[PDF_MRANGE_CAP];
	int ucslen;

	for (k = 0; k <= n; ++k)
	{
		ucslen = pdf_lookup_cmap_full(ucs_from_cpt, cpt + k, ucsbuf);
		if (ucslen == 1)
			pdf_map_range_to_range(ctx, ucs_from_gid, gid + k, gid + k, ucsbuf[0]);
		else if (ucslen > 1)
			pdf_map_one_to_many(ctx, ucs_from_gid, gid + k, ucsbuf, ucslen);
	}
}

static pdf_cmap *
pdf_remap_cmap(fz_context *ctx, pdf_cmap *gid_from_cpt, pdf_cmap *ucs_from_cpt)
{
	pdf_cmap *ucs_from_gid;
	unsigned int a, b, x;
	int i;

	ucs_from_gid = pdf_new_cmap(ctx);

	fz_try(ctx)
	{
		if (gid_from_cpt->usecmap)
			ucs_from_gid->usecmap = pdf_remap_cmap(ctx, gid_from_cpt->usecmap, ucs_from_cpt);

		pdf_add_codespace(ctx, ucs_from_gid, 0, 0x7fffffff, 4);

		for (i = 0; i < gid_from_cpt->rlen; ++i)
		{
			a = gid_from_cpt->ranges[i].low;
			b = gid_from_cpt->ranges[i].high;
			x = gid_from_cpt->ranges[i].out;
			pdf_remap_cmap_range(ctx, ucs_from_gid, a, x, b - a, ucs_from_cpt);
		}

		for (i = 0; i < gid_from_cpt->xlen; ++i)
		{
			a = gid_from_cpt->xranges[i].low;
			b = gid_from_cpt->xranges[i].high;
			x = gid_from_cpt->xranges[i].out;
			pdf_remap_cmap_range(ctx, ucs_from_gid, a, x, b - a, ucs_from_cpt);
		}

		/* Font encoding CMaps don't have one-to-many mappings, so we can ignore the mranges. */

		pdf_sort_cmap(ctx, ucs_from_gid);
	}
	fz_catch(ctx)
	{
		pdf_drop_cmap(ctx, ucs_from_gid);
		fz_rethrow(ctx);
	}

	return ucs_from_gid;
}

/* Recover a unicode value from glyph names that don't map via the Adobe Glyph
 * List, mirroring the heuristics pdf.js uses in _simpleFontToUnicode():
 *   "G" + 2 hex digits   -> that code point   (e.g. "G45" -> 'E')
 *   "g" + 4 hex digits   -> that code point
 *   "C"/"c" + 2..3 digits -> that decimal code point
 * Some embedded subset fonts (e.g. MSTT*) name glyphs this way and ship no
 * ToUnicode CMap; without this, their text extracts as U+FFFD and isn't
 * searchable. Returns 0 if no heuristic applies. (issue #3219)
 */
static int
unicode_from_coded_glyph_name(const char *name)
{
	size_t n = strlen(name);
	char *end;
	long code = 0;

	if (name[0] == 'G' && n == 3)
		code = strtol(name + 1, &end, 16);
	else if (name[0] == 'g' && n == 5)
		code = strtol(name + 1, &end, 16);
	else if ((name[0] == 'C' || name[0] == 'c') && n >= 3 && n <= 4)
		code = strtol(name + 1, &end, 10);
	else
		return 0;

	if (*end != 0)
		return 0;
	if (code > 0 && code <= 0xffff)
		return (int)code;
	return 0;
}

/* Distiller Type1 ToUnicode is often identity Latin-1 even for CP1251
 * faces that reuse Latin Encoding names. Treat the font as CP1251 when
 * the name is a known family / has a Cyrillic tag, or the document
 * title/author is Cyrillic. (issue #5873) */
static int
pdf_simple_font_looks_cp1251(fz_context *ctx, pdf_document *doc, pdf_font_desc *font)
{
	const char *name = font->font ? fz_font_name(ctx, font->font) : "";
	char buf[1024];
	int i, k;
	static const char *keys[] = { FZ_META_INFO_TITLE, FZ_META_INFO_AUTHOR, NULL };

	if (strlen(name) > 7 && name[6] == '+')
		name += 7;

	if (fz_strncasecmp(name, "literaturnaya", 13) == 0)
		return 1;
	/* "Academy" / "Academy-Bold", but not "AcademyEngraved". */
	if (fz_strncasecmp(name, "academy", 7) == 0 && (name[7] == 0 || name[7] == '-'))
		return 1;
	for (i = 0; name[i]; i++)
		if (fz_strncasecmp(name + i, "cyr", 3) == 0 || fz_strncasecmp(name + i, "1251", 4) == 0)
			return 1;

	if (!doc)
		return 0;
	for (k = 0; keys[k]; k++)
	{
		if (pdf_lookup_metadata(ctx, doc, keys[k], buf, sizeof buf) <= 0)
			continue;
		/* UTF-8 Cyrillic (U+0400-U+047F) starts with D0/D1. */
		for (i = 0; buf[i]; i++)
			if ((unsigned char)buf[i] == 0xD0 || (unsigned char)buf[i] == 0xD1)
				return 1;
	}
	return 0;
}

void
pdf_load_to_unicode(fz_context *ctx, pdf_document *doc, pdf_font_desc *font,
	const char **strings, char *collection, pdf_obj *cmapstm)
{
	unsigned int cpt;

	if (pdf_is_stream(ctx, cmapstm))
	{
		pdf_cmap *ucs_from_cpt = pdf_load_embedded_cmap(ctx, doc, cmapstm);
		fz_try(ctx)
			font->to_unicode = pdf_remap_cmap(ctx, font->encoding, ucs_from_cpt);
		fz_always(ctx)
			pdf_drop_cmap(ctx, ucs_from_cpt);
		fz_catch(ctx)
			fz_rethrow(ctx);
		font->size += pdf_cmap_size(ctx, font->to_unicode);
	}

	else if (pdf_is_name(ctx, cmapstm))
	{
		pdf_cmap *ucs_from_cpt = pdf_load_system_cmap(ctx, pdf_to_name(ctx, cmapstm));
		fz_try(ctx)
			font->to_unicode = pdf_remap_cmap(ctx, font->encoding, ucs_from_cpt);
		fz_always(ctx)
			pdf_drop_cmap(ctx, ucs_from_cpt);
		fz_catch(ctx)
			fz_rethrow(ctx);
		font->size += pdf_cmap_size(ctx, font->to_unicode);
	}

	else if (collection)
	{
		if (!strcmp(collection, "Adobe-CNS1"))
			font->to_unicode = pdf_load_system_cmap(ctx, "Adobe-CNS1-UCS2");
		else if (!strcmp(collection, "Adobe-GB1"))
			font->to_unicode = pdf_load_system_cmap(ctx, "Adobe-GB1-UCS2");
		else if (!strcmp(collection, "Adobe-Japan1"))
			font->to_unicode = pdf_load_system_cmap(ctx, "Adobe-Japan1-UCS2");
		else if (!strcmp(collection, "Adobe-Korea1"))
			font->to_unicode = pdf_load_system_cmap(ctx, "Adobe-Korea1-UCS2");
	}

	if (strings)
	{
		/* TODO one-to-many mappings */
		int n_high = 0, n_id = 0;

		font->cid_to_ucs = Memento_label(fz_malloc_array(ctx, 256, unsigned short), "cid_to_ucs");
		font->cid_to_ucs_len = 256;
		font->size += 256 * sizeof *font->cid_to_ucs;

		for (cpt = 0; cpt < 256; cpt++)
		{
			if (strings[cpt])
			{
				int ucs = fz_unicode_from_glyph_name(strings[cpt]);
				if (ucs == FZ_REPLACEMENT_CHARACTER)
				{
					int u2 = unicode_from_coded_glyph_name(strings[cpt]);
					if (u2)
						ucs = u2;
				}
				font->cid_to_ucs[cpt] = ucs;
			}
			else
				font->cid_to_ucs[cpt] = FZ_REPLACEMENT_CHARACTER;

			if (cpt >= 0xC0 && font->cid_to_ucs[cpt] != 0 &&
				font->cid_to_ucs[cpt] != FZ_REPLACEMENT_CHARACTER)
			{
				n_high++;
				if (font->cid_to_ucs[cpt] == cpt)
					n_id++;
			}
		}

		/* Encoding names mapped 0xC0-0xFF to U+00C0-U+00FF. Distiller's
		 * ToUnicode does the same, which is wrong for CP1251 Type1 faces. */
		if (n_high >= 32 && n_id * 4 >= n_high * 3 &&
			pdf_simple_font_looks_cp1251(ctx, doc, font))
		{
			if (font->to_unicode)
			{
				font->size -= pdf_cmap_size(ctx, font->to_unicode);
				pdf_drop_cmap(ctx, font->to_unicode);
				font->to_unicode = NULL;
			}
			for (cpt = 0; cpt < 256; cpt++)
				font->cid_to_ucs[cpt] = fz_unicode_from_windows_1251[cpt];
		}
	}

	if (!font->to_unicode && !font->cid_to_ucs)
	{
		/* TODO: synthesize a ToUnicode if it's a freetype font with
		 * cmap and/or post tables or if it has glyph names. */
	}
}
