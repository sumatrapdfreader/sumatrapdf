// Copyright (C) 2024-2025 Artifex Software, Inc.
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

#include <zlib.h>

#include <limits.h>

typedef struct
{
	fz_document_writer super;
	int count;
	fz_stext_page *page;
	fz_output *out;
	fz_stext_options options;
	int pagenum;
} fz_csv_writer;

static fz_device *
csv_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_csv_writer *wri = (fz_csv_writer*)wri_;
	wri->page = fz_new_stext_page(ctx, mediabox);
	wri->options.flags |= FZ_STEXT_COLLECT_VECTORS;
	wri->options.flags |= FZ_STEXT_ACCURATE_BBOXES;
	wri->options.flags |= FZ_STEXT_SEGMENT;
	wri->options.flags |= FZ_STEXT_TABLE_HUNT;
	return fz_new_stext_device(ctx, wri->page, &wri->options);
}

typedef struct
{
	int leading;
	int spaces;
} space_data;

static void
output_line(fz_context *ctx, fz_output *out, fz_stext_line *line, space_data *sd)
{
	for (; line != NULL; line = line->next)
	{
		fz_stext_char *ch;

		for (ch = line->first_char; ch != NULL; ch = ch->next)
		{
			if (ch->c == ' ')
			{
				if (!sd->leading)
					sd->spaces++;
				continue;
			}
			sd->leading = 0;
			/* Compact all runs of spaces to single ones. */
			if (sd->spaces > 0)
			{
				fz_write_printf(ctx, out, " ");
				sd->spaces = 0;
			}
			if (ch->c == '\"')
			{
				fz_write_printf(ctx, out, "\"\"");
			}
			else
			{
				fz_write_printf(ctx, out, "%C", ch->c);
			}
		}
	}
}

static fz_rect
whitespaceless_bbox(fz_context *ctx, fz_stext_block *block)
{
	fz_rect r = fz_empty_rect;
	fz_stext_line *line;
	fz_stext_char *ch;

	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down)
				r = fz_union_rect(r, whitespaceless_bbox(ctx, block->u.s.down->first_block));
			continue;
		}
		if (block->type != FZ_STEXT_BLOCK_TEXT)
		{
			r = fz_union_rect(r, block->bbox);
			continue;
		}
		for (line = block->u.t.first_line; line != NULL; line = line->next)
		{
			for (ch = line->first_char; ch != NULL; ch = ch->next)
			{
				if (ch->c != ' ')
					r = fz_union_rect(r, fz_rect_from_quad(ch->quad));
			}
		}
	}

	return r;
}

static void
output_td_contents(fz_context *ctx, fz_output *out, fz_stext_block *block, space_data *sd)
{
	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down)
				output_td_contents(ctx, out, block->u.s.down->first_block, sd);
			continue;
		}
		if (block->type == FZ_STEXT_BLOCK_TEXT)
			output_line(ctx, out, block->u.t.first_line, sd);
	}
}

/* We have output up to and including position *pos on entry to this function.
 * We preserve that on output. */
static void
output_td(fz_context *ctx, fz_csv_writer *wri, fz_stext_block *grid, int *pos, fz_stext_block *block)
{
	int x0, x1;
	space_data sd = { 0 };
	fz_rect r = whitespaceless_bbox(ctx, block);

	if (fz_is_empty_rect(r))
		return;

	if (block && grid)
	{

		for (x0 = 0; x0 < grid->u.b.xs->len; x0++)
			if (r.x0 < grid->u.b.xs->list[x0].pos)
				break;
		for (x1 = x0; x1 < grid->u.b.xs->len; x1++)
			if (r.x1 <= grid->u.b.xs->list[x1].pos)
				break;
		x0--;
		x1--;
	}
	else
		x0 = *pos+1, x1 = *pos+1;

	/* Send enough , to get us to the right position. */
	while (*pos < x0)
	{
		if (*pos >= 0)
			fz_write_printf(ctx, wri->out, ",");
		*pos = (*pos)+1;
	}

	fz_write_printf(ctx, wri->out, "\"");
	output_td_contents(ctx, wri->out, block, &sd);
	fz_write_printf(ctx, wri->out, "\"");

	/* Send any extra , to allow for colspans */
	while (*pos < x1)
	{
		fz_write_printf(ctx, wri->out, ",");
		*pos = (*pos)+1;
	}
}

static void
output_tr(fz_context *ctx, fz_csv_writer *wri, fz_stext_block *grid, fz_stext_block *block)
{
	int pos = -1;

	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (!block->u.s.down)
				continue;
			if (block->u.s.down->standard == FZ_STRUCTURE_TD)
				output_td(ctx, wri, grid, &pos, block->u.s.down->first_block);
		}
	}

	if (pos != -1)
		fz_write_printf(ctx, wri->out, "\n");
}

static void
output_table(fz_context *ctx, fz_csv_writer *wri, fz_rect bbox, fz_stext_block *first)
{
	fz_stext_block *block;
	fz_stext_block *grid = NULL;
	int rows = 0;

	fz_try(ctx)
	{
		/* First, walk to find the div positions */
		for (block = first; block != NULL; block = block->next)
		{
			if (block->type == FZ_STEXT_BLOCK_GRID)
			{
				grid = block;
				break;
			}
		}

		/* Then, count the rows */
		for (block = first; block != NULL; block = block->next)
		{
			if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down != NULL && block->u.s.down->standard == FZ_STRUCTURE_TR)
				rows++;
		}

		fz_write_printf(ctx, wri->out, "Table %d,%d,%d,%g,%g,%g,%g\n",
			wri->count++,
			rows,
			wri->pagenum,
			bbox.x0, bbox.y0, bbox.x1, bbox.y1);

		/* Then do the output */
		for (block = first; block != NULL; block = block->next)
		{
			if (block->type == FZ_STEXT_BLOCK_STRUCT)
			{
				if (!block->u.s.down)
					continue;
				if (block->u.s.down->standard == FZ_STRUCTURE_TR)
					output_tr(ctx, wri, grid, block->u.s.down->first_block);
			}
		}
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
output_tables(fz_context *ctx, fz_csv_writer *wri, fz_stext_page *page, fz_stext_block *block)
{
	for (; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (!block->u.s.down)
				continue;
			if (block->u.s.down->standard == FZ_STRUCTURE_TABLE)
				output_table(ctx, wri, block->bbox, block->u.s.down->first_block);
			else
				output_tables(ctx, wri, page, block->u.s.down->first_block);
		}
	}
}

static void
csv_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_csv_writer *wri = (fz_csv_writer*)wri_;

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);

		/* Output UTF-8 BOM */
		fz_write_printf(ctx, wri->out, "%C", 0xFEFF);

		output_tables(ctx, wri, wri->page, wri->page->first_block);
		fz_drop_stext_page(ctx, wri->page);
		wri->page = NULL;
		wri->pagenum++;
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
csv_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_csv_writer *wri = (fz_csv_writer*)wri_;
	fz_close_output(ctx, wri->out);
}

static void
csv_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_csv_writer *wri = (fz_csv_writer*)wri_;
	fz_drop_stext_page(ctx, wri->page);
	fz_drop_output(ctx, wri->out);
}

fz_document_writer *
fz_new_csv_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_csv_writer *wri = NULL;

	fz_var(wri);
	fz_var(out);

	fz_try(ctx)
	{
		wri = fz_new_derived_document_writer(ctx, fz_csv_writer, csv_begin_page, csv_end_page, csv_close_writer, csv_drop_writer);
		fz_parse_stext_options(ctx, &wri->options, options);
		wri->out = out;
	}
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}
	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_csv_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.csv", 0);
	fz_document_writer *wri = NULL;
	fz_try(ctx)
		wri = fz_new_csv_writer_with_output(ctx, out, options);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return wri;
}
