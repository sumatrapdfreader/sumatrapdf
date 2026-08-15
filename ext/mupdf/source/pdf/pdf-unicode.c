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

/* True when most mapped codes in 0xC0-0xFF are the Latin-1 identity
 * (code -> U+00xx). Distiller emits this for Type1 fonts whose Encoding
 * uses Latin glyph names, including Russian CP1251 Type1 faces. */
static int
to_unicode_is_latin1_identity_high(pdf_cmap *cmap)
{
	unsigned int c;
	int n_mapped = 0;
	int n_identity = 0;

	if (!cmap)
		return 0;

	for (c = 0xC0; c <= 0xFF; c++)
	{
		int u = pdf_lookup_cmap(cmap, c);
		if (u < 0)
			continue;
		n_mapped++;
		if (u == (int)c)
			n_identity++;
	}

	/* Need a solid high-byte map that is mostly identity. */
	return n_mapped >= 32 && n_identity * 4 >= n_mapped * 3;
}

static int
cid_to_ucs_is_latin1_identity_high(const unsigned short *cid_to_ucs, size_t len)
{
	unsigned int c;
	int n_mapped = 0;
	int n_identity = 0;

	if (!cid_to_ucs || len < 256)
		return 0;

	for (c = 0xC0; c <= 0xFF; c++)
	{
		unsigned short u = cid_to_ucs[c];
		if (u == 0 || u == FZ_REPLACEMENT_CHARACTER)
			continue;
		n_mapped++;
		if (u == c)
			n_identity++;
	}

	return n_mapped >= 32 && n_identity * 4 >= n_mapped * 3;
}

static int
utf8_contains_cyrillic(const char *s)
{
	if (!s)
		return 0;
	while (*s)
	{
		int rune;
		int n = fz_chartorune(&rune, s);
		if (n <= 0)
			break;
		/* Cyrillic block + Cyrillic Supplement */
		if ((rune >= 0x0400 && rune <= 0x04FF) || (rune >= 0x0500 && rune <= 0x052F))
			return 1;
		s += n;
	}
	return 0;
}

/* Strip optional PDF subset tag "ABCDEF+". */
static const char *
font_name_without_subset(const char *name)
{
	int i;

	if (!name || strlen(name) <= 7 || name[6] != '+')
		return name;
	for (i = 0; i < 6; i++)
	{
		char c = name[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
			return name;
	}
	return name + 7;
}

/* Family token before style suffix (-Bold, -Italic, …). */
static size_t
font_family_len(const char *name)
{
	const char *p = name;
	while (*p && *p != '-' && *p != ',' && *p != ' ')
		p++;
	return (size_t)(p - name);
}

static int
ascii_contains_ci(const char *hay, const char *needle)
{
	size_t n;

	if (!hay || !needle || !needle[0])
		return 0;
	n = strlen(needle);
	for (; *hay; hay++)
	{
		if (fz_strncasecmp(hay, needle, n) == 0)
			return 1;
	}
	return 0;
}

static int
font_name_suggests_cyrillic(const char *name)
{
	/* Classic ParaType / ParaGraph CP1251 Type1 faces and explicit Cyrillic tags.
	 * Match the family token only so "AcademyEngraved" is not treated as Academy. */
	static const char *families[] = {
		"literaturnaya",
		"academy",
		"schoolbook",
		"petersburg",
		"freeset",
		"pragmatica",
		"newton",
		"baltica",
		"magazine",
		"zhurnalnaya",
		"zhurnal",
		"journal",
		"kudryashev",
		"kudriashov",
		"octava",
		"textbook",
		"certum",
		"pushkin",
		"karolla",
		"hermes",
		"futuris",
		"gazeta",
		"arialcyr",
		"timescyr",
		"courcyr",
		NULL
	};
	const char *base;
	size_t flen;
	int i;

	if (!name || !name[0])
		return 0;

	base = font_name_without_subset(name);

	/* Explicit markers anywhere in the name. */
	if (ascii_contains_ci(base, "cyrillic") || ascii_contains_ci(base, "cyrl") ||
		ascii_contains_ci(base, "paratype") || ascii_contains_ci(base, "cp1251") ||
		ascii_contains_ci(base, "windows-1251"))
		return 1;

	flen = font_family_len(base);
	if (flen == 0)
		return 0;

	for (i = 0; families[i]; i++)
	{
		size_t n = strlen(families[i]);
		if (flen == n && fz_strncasecmp(base, families[i], n) == 0)
			return 1;
	}
	return 0;
}

static int
pdf_doc_suggests_cyrillic(fz_context *ctx, pdf_document *doc)
{
	static const char *info_keys[] = {
		FZ_META_INFO_TITLE,
		FZ_META_INFO_SUBJECT,
		FZ_META_INFO_AUTHOR,
		FZ_META_INFO_KEYWORDS,
		NULL
	};
	pdf_obj *root;
	pdf_obj *lang;
	char buf[2048];
	int i;

	if (!doc)
		return 0;

	root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	lang = pdf_dict_get(ctx, root, PDF_NAME(Lang));
	if (pdf_is_string(ctx, lang))
	{
		const char *l = pdf_to_text_string(ctx, lang);
		/* ru, uk, bg, be, sr, mk, … */
		if (l && l[0] && l[1])
		{
			char a = l[0] | 0x20;
			char b = l[1] | 0x20;
			if ((a == 'r' && b == 'u') || (a == 'u' && b == 'k') ||
				(a == 'b' && b == 'g') || (a == 'b' && b == 'e') ||
				(a == 's' && b == 'r') || (a == 'm' && b == 'k'))
				return 1;
		}
	}

	for (i = 0; info_keys[i]; i++)
	{
		if (pdf_lookup_metadata(ctx, doc, info_keys[i], buf, sizeof buf) > 0 &&
			utf8_contains_cyrillic(buf))
			return 1;
	}
	return 0;
}

/* Rebuild simple-font Unicode as Windows-1251. Prefer cid_to_ucs and drop a
 * misleading identity-Latin ToUnicode so extraction uses the CP1251 table. */
static void
pdf_apply_windows_1251_unicode(fz_context *ctx, pdf_font_desc *font)
{
	unsigned int c;

	if (font->to_unicode)
	{
		font->size -= pdf_cmap_size(ctx, font->to_unicode);
		pdf_drop_cmap(ctx, font->to_unicode);
		font->to_unicode = NULL;
	}

	if (!font->cid_to_ucs)
	{
		font->cid_to_ucs = Memento_label(fz_malloc_array(ctx, 256, unsigned short), "cid_to_ucs");
		font->cid_to_ucs_len = 256;
		font->size += 256 * sizeof *font->cid_to_ucs;
	}
	else if (font->cid_to_ucs_len < 256)
	{
		font->cid_to_ucs = fz_realloc_array(ctx, font->cid_to_ucs, 256, unsigned short);
		font->size += (256 - font->cid_to_ucs_len) * sizeof *font->cid_to_ucs;
		font->cid_to_ucs_len = 256;
	}

	for (c = 0; c < 256; c++)
		font->cid_to_ucs[c] = fz_unicode_from_windows_1251[c];
}

/* Acrobat Distiller (and similar) often emit Type1 ToUnicode CMaps that
 * identity-map 0xC0-0xFF to U+00C0-U+00FF from Latin Encoding glyph names.
 * Russian CP1251 Type1 fonts (Literaturnaya, Academy, …) put Cyrillic glyphs
 * at those codes under the same Latin names, so extract/copy becomes mojibake.
 * When the high-byte map is identity Latin-1 and the font or document looks
 * Cyrillic, reinterpret via Windows-1251. (issue #5873) */
static void
pdf_fix_cyrillic_cp1251_tounicode(fz_context *ctx, pdf_document *doc, pdf_font_desc *font)
{
	int high_is_latin1;
	const char *fname = NULL;
	int cyrillic_hint;

	high_is_latin1 = to_unicode_is_latin1_identity_high(font->to_unicode);
	if (!high_is_latin1)
		high_is_latin1 = cid_to_ucs_is_latin1_identity_high(font->cid_to_ucs, font->cid_to_ucs_len);
	if (!high_is_latin1)
		return;

	if (font->font)
		fname = fz_font_name(ctx, font->font);

	cyrillic_hint = font_name_suggests_cyrillic(fname);
	if (!cyrillic_hint)
		cyrillic_hint = pdf_doc_suggests_cyrillic(ctx, doc);
	if (!cyrillic_hint)
		return;

	pdf_apply_windows_1251_unicode(ctx, font);
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
		}
	}

	if (!font->to_unicode && !font->cid_to_ucs)
	{
		/* TODO: synthesize a ToUnicode if it's a freetype font with
		 * cmap and/or post tables or if it has glyph names. */
	}

	pdf_fix_cyrillic_cp1251_tounicode(ctx, doc, font);
}
