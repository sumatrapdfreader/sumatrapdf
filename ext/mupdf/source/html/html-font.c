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
#include "html-imp.h"

#include <string.h>

static fz_font *
fz_load_html_default_font(fz_context *ctx, fz_html_font_set *set, const char *family, int is_bold, int is_italic)
{
	int is_mono = !strcmp(family, "monospace");
	int is_sans = !strcmp(family, "sans-serif");
	const char *real_family = is_mono ? "Courier" : is_sans ? "Helvetica" : "Charis SIL";
	const char *backup_family = is_mono ? "Courier" : is_sans ? "Helvetica" : "Times";
	int idx = (is_mono ? 8 : is_sans ? 4 : 0) + is_bold * 2 + is_italic;
	if (!set->fonts[idx])
	{
		const unsigned char *data;
		int size;

		data = fz_lookup_builtin_font(ctx, real_family, is_bold, is_italic, &size);
		if (!data)
			data = fz_lookup_builtin_font(ctx, backup_family, is_bold, is_italic, &size);
		if (!data)
			fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "cannot load html font: %s", real_family);
		set->fonts[idx] = fz_new_font_from_memory(ctx, NULL, data, size, 0, 1);
		fz_font_flags(set->fonts[idx])->is_serif = !is_sans;
	}
	return set->fonts[idx];
}

void
fz_add_html_font_face(fz_context *ctx, fz_html_font_set *set,
	const char *family, int is_bold, int is_italic, int is_small_caps,
	const char *src, fz_font *font)
{
	fz_html_font_face *custom = fz_malloc_struct(ctx, fz_html_font_face);
	fz_font_flags_t *flags;

	flags = fz_font_flags(font);
	if (is_bold && !flags->is_bold)
		flags->fake_bold = 1;
	if (is_italic && !flags->is_italic)
		flags->fake_italic = 1;

	fz_try(ctx)
	{
		custom->font = fz_keep_font(ctx, font);
		custom->src = fz_strdup(ctx, src);
		custom->family = fz_strdup(ctx, family);
		custom->is_bold = is_bold;
		custom->is_italic = is_italic;
		custom->is_small_caps = is_small_caps;
		custom->next = set->custom;
		set->custom = custom;
	}
	fz_catch(ctx)
	{
		fz_drop_font(ctx, custom->font);
		fz_free(ctx, custom->src);
		fz_free(ctx, custom->family);
		fz_rethrow(ctx);
	}
}

fz_font *
fz_load_html_font(fz_context *ctx, fz_html_font_set *set,
	const char *family, int is_bold, int is_italic, int is_small_caps)
{
	fz_html_font_face *custom;
	const unsigned char *data;
	int best_score = 0;
	fz_font *best_font = NULL;
	fz_font *font;
	int size;

	for (custom = set->custom; custom; custom = custom->next)
	{
		if (!strcmp(family, custom->family))
		{
			int score =
				1 * (is_bold == custom->is_bold) +
				2 * (is_italic == custom->is_italic) +
				4 * (is_small_caps == custom->is_small_caps);
			if (score > best_score)
			{
				best_score = score;
				best_font = custom->font;
			}
		}
	}

	// We found a perfect match!
	if (best_font && best_score == 1 + 2 + 4)
		return best_font;

	// Try to load a perfect match.
	data = fz_lookup_builtin_font(ctx, family, is_bold, is_italic, &size);
	if (!data)
		data = fz_lookup_builtin_font(ctx, family, 0, 0, &size);
	if (data)
	{
		font = fz_new_font_from_memory(ctx, NULL, data, size, 0, 0);
		fz_try(ctx)
			fz_add_html_font_face(ctx, set, family, is_bold, is_italic, 0, "<builtin>", font);
		fz_always(ctx)
			fz_drop_font(ctx, font);
		fz_catch(ctx)
			fz_rethrow(ctx);
		return font;
	}
	else
	{
		font = fz_load_system_font(ctx, family, is_bold, is_italic, 0);
		if (font)
		{
			fz_try(ctx)
				fz_add_html_font_face(ctx, set, family, is_bold, is_italic, 0, "<system>", font);
			fz_always(ctx)
				fz_drop_font(ctx, font);
			fz_catch(ctx)
				fz_rethrow(ctx);
			return font;
		}
	}

	// Use the imperfect match from before.
	if (best_font)
		return best_font;

	// Handle the "default" font aliases.
	if (!strcmp(family, "monospace") || !strcmp(family, "sans-serif") || !strcmp(family, "serif"))
		return fz_load_html_default_font(ctx, set, family, is_bold, is_italic);

	return NULL;
}

fz_html_font_set *fz_new_html_font_set(fz_context *ctx)
{
	return fz_malloc_struct(ctx, fz_html_font_set);
}

void fz_drop_html_font_set(fz_context *ctx, fz_html_font_set *set)
{
	fz_html_font_face *font, *next;
	int i;

	if (!set)
		return;

	font = set->custom;
	while (font)
	{
		next = font->next;
		fz_drop_font(ctx, font->font);
		fz_free(ctx, font->src);
		fz_free(ctx, font->family);
		fz_free(ctx, font);
		font = next;
	}

	for (i = 0; i < (int)nelem(set->fonts); ++i)
		fz_drop_font(ctx, set->fonts[i]);

	fz_free(ctx, set);
}
