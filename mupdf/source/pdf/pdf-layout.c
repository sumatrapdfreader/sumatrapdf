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
#include <float.h>
#include <math.h>

#define LINE_LIMIT (100)
#define LINE_HEIGHT (1.2f)
struct line { const char *a, *b; };

struct font_info
{
	fz_context *ctx;
	fz_font *font;
	float fontsize;
};

static float measure_character(struct font_info *info, int c)
{
	fz_font *font;
	int gid = fz_encode_character_with_fallback(info->ctx, info->font, c, 0, 0, &font);
	return fz_advance_glyph(info->ctx, font, gid, 0) * info->fontsize;
}


static int break_lines(struct font_info *info, const char *a, struct line *lines, int maxlines, float width, float *maxwidth)
{
	const char *next, *space = NULL, *b = a;
	int c, n = 0;
	float space_x, x = 0, w = 0;

	if (maxwidth)
		*maxwidth = 0;

	while (*b)
	{
		next = b + fz_chartorune(&c, b);
		if (c == '\r' || c == '\n')
		{
			if (lines && n < maxlines)
			{
				lines[n].a = a;
				lines[n].b = b;
			}
			++n;
			if (maxwidth && *maxwidth < x)
				*maxwidth = x;
			a = next;
			x = 0;
			space = NULL;
		}
		else
		{
			if (c == ' ')
			{
				space = b;
				space_x = x;
			}

			w = measure_character(info, c);
			if (x + w > width)
			{
				if (space)
				{
					if (lines && n < maxlines)
					{
						lines[n].a = a;
						lines[n].b = space;
					}
					++n;
					if (maxwidth && *maxwidth < space_x)
						*maxwidth = space_x;
					a = next = space + 1;
					x = 0;
					space = NULL;
				}
				else
				{
					if (lines && n < maxlines)
					{
						lines[n].a = a;
						lines[n].b = b;
					}
					++n;
					if (maxwidth && *maxwidth < x)
						*maxwidth = x;
					a = b;
					x = w;
					space = NULL;
				}
			}
			else
			{
				x += w;
			}
		}
		b = next;
	}

	if (lines && n < maxlines)
	{
		lines[n].a = a;
		lines[n].b = b;
	}
	++n;
	if (maxwidth && *maxwidth < x)
		*maxwidth = x;
	return n < maxlines ? n : maxlines;
}

static fz_matrix show_string(fz_context *ctx, fz_text *text, fz_font *user_font, fz_matrix trm, const char *s, int len,
	int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language language)
{
	fz_font *font;
	int gid, ucs;
	float adv;
	int i = 0;

	while (i < len)
	{
		i += fz_chartorune(&ucs, s + i);
		gid = fz_encode_character_with_fallback(ctx, user_font, ucs, 0, language, &font);
		fz_show_glyph(ctx, text, font, trm, gid, ucs, wmode, bidi_level, markup_dir, language);
		adv = fz_advance_glyph(ctx, font, gid, wmode);
		if (wmode == 0)
			trm = fz_pre_translate(trm, adv, 0);
		else
			trm = fz_pre_translate(trm, 0, adv);
	}

	return trm;
}

fz_text *pdf_layout_fit_text(fz_context *ctx, fz_font *font, fz_text_language lang, const char *str, fz_rect bounds)
{
	fz_text *text = NULL;
	struct font_info info;
	struct line *lines;
	float width = bounds.x1 - bounds.x0;
	float height = bounds.y1 - bounds.y0;

	lines = fz_malloc_array(ctx, LINE_LIMIT, struct line);

	fz_var(info);
	fz_try(ctx)
	{
		fz_matrix trm;
		int target_line_count;
		int line_count, l;
		float line_len;
		fz_rect tbounds;
		float xadj, yadj;
		fz_text_span *span;

		info.ctx = ctx;
		info.font = font;
		info.fontsize = 1;

		/* Find out how many lines the text requires without any wrapping */
		target_line_count = break_lines(&info, str, lines, LINE_LIMIT, FLT_MAX, &line_len);

		/* Try increasing line counts, which reduces the font size, until the text fits */
		do
		{
			info.fontsize = height / (target_line_count * LINE_HEIGHT);
			line_count = break_lines(&info, str, lines, LINE_LIMIT, width, &line_len);
		} while (line_count > target_line_count++);

		trm = fz_scale(info.fontsize, -info.fontsize);
		trm.e += bounds.x0;
		trm.f += bounds.y1;
		text = fz_new_text(ctx);
		for (l = 0; l < line_count; l++)
		{
			show_string(ctx, text, font, trm, lines[l].a, lines[l].b - lines[l].a, 0, 0, FZ_BIDI_LTR, lang);
			trm = fz_pre_translate(trm, 0.0f, -LINE_HEIGHT);
		}
		tbounds = fz_bound_text(ctx, text, NULL, fz_identity);
		xadj = (bounds.x0 + bounds.x1 - tbounds.x0 - tbounds.x1) / 2.0f;
		yadj = (bounds.y0 + bounds.y1 - tbounds.y0 - tbounds.y1) / 2.0f;
		for (span = text->head; span; span = span->next)
		{
			int i;
			for (i = 0; i < span->len; i++)
			{
				span->items[i].x += xadj;
				span->items[i].y += yadj;
			}
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, lines);
	}
	fz_catch(ctx)
	{
		fz_drop_text(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}
