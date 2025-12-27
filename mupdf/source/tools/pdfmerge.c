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

/*
 * PDF merge tool: Tool for merging pdf content.
 *
 * Simple test bed to work with merging pages from multiple PDFs into a single PDF.
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int usage(void)
{
	fprintf(stderr,
		"usage: mutool merge [-o output.pdf] [-O options] input.pdf [pages] [input2.pdf] [pages2] ...\n"
		"\t-o -\tname of PDF file to create\n"
		"\t-O -\tcomma separated list of output options\n"
		"\tinput.pdf\tname of input file from which to copy pages\n"
		"\tpages\tcomma separated list of page numbers and ranges\n\n"
		);
	fputs(fz_pdf_write_options_usage, stderr);
	return 1;
}

static pdf_document *doc_des = NULL;
static pdf_document *doc_src = NULL;
int output_page_count = 0;

static void page_merge(fz_context *ctx, int page_from, pdf_graft_map *graft_map)
{
	pdf_page *page_src = NULL, *page_des = NULL;
	fz_link *link, *head = NULL;

	pdf_graft_mapped_page(ctx, graft_map, -1, doc_src, page_from - 1);

	fz_var(page_src);
	fz_var(page_des);
	fz_var(head);

	fz_try(ctx)
	{
		page_src = pdf_load_page(ctx, doc_src, page_from - 1);
		page_des = pdf_load_page(ctx, doc_des, pdf_count_pages(ctx, doc_des) - 1);
		head = fz_load_links(ctx, (fz_page*)page_src);
		for (link = head; link; link = link->next)
		{
			// TODO: copy internal links to renumbered pages
			if (fz_is_external_link(ctx, link->uri))
				fz_create_link(ctx, (fz_page*)page_des, link->rect, link->uri);
		}
	}
	fz_always(ctx)
	{
		fz_drop_page(ctx, (fz_page*)page_src);
		fz_drop_page(ctx, (fz_page*)page_des);
		fz_drop_link(ctx, head);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
	While we are processing, it_src tracks the current position we are copying from.

	items is the list of things we have stepped through to get to the current position.
	A prefix of these items may have already been copied across. copied_to_depth is
	the length of that prefix. 0 < = copied_to_depth <= len.
*/
typedef struct
{
	fz_context *ctx;
	fz_outline_iterator *it_dst;
	fz_outline_iterator *it_src;
	const char *range;
	int page_count;
	int max;
	int len;
	fz_outline_item *items;
	int copied_to_depth;
	int page_output_base;
} cor_state;

/* Given a range, and a page in the range 1 to count, return the position
 * which the page occupies in the output range (or 0 for not in range).
 * So page 12 within 10-20 would return 3.
 */
static int
position_in_range(fz_context *ctx, const char *range, int count, int page)
{
	int start, end;
	int n = 0;

	while ((range = fz_parse_page_range(ctx, range, &start, &end, count)))
	{
		if (start < end)
		{
			if (start <= page && page <= end)
				return n + page - start + 1;
			n += end - start + 1;
		}
		else
		{
			if (end <= page && page <= start)
				return n + page - end + 1;
			n += start - end + 1;
		}
	}

	return 0;
}

static void
copy_item(cor_state *cor)
{
	fz_context *ctx = cor->ctx;

	while (cor->copied_to_depth < cor->len)
	{
		/* All items copied in a run get the same uri - that of the last one. */
		fz_outline_item item = cor->items[cor->copied_to_depth];
		item.uri = cor->items[cor->len-1].uri;
		fz_outline_iterator_insert(ctx, cor->it_dst, &item);
		cor->copied_to_depth++;
		fz_outline_iterator_prev(ctx, cor->it_dst);
		fz_outline_iterator_down(ctx, cor->it_dst);
	}
}

static char *
rewrite_page(fz_context *ctx, const char *uri, int n)
{
	const char *p;

	if (uri == NULL)
		return NULL;

	if (strncmp(uri, "#page=", 6) != 0)
		return fz_strdup(ctx, uri);
	p = strchr(uri+6, '&');
	if (p == NULL)
		return fz_asprintf(ctx, "#page=%d", n);

	return fz_asprintf(ctx, "#page=%d%s", n, p);
}

static void
do_copy_outline_range(cor_state *cor)
{
	fz_context *ctx = cor->ctx;

	do
	{
		int has_children;
		float x, y;
		fz_outline_item *item = fz_outline_iterator_item(ctx, cor->it_src);
		int page_num = fz_page_number_from_location(ctx, (fz_document *)doc_src, fz_resolve_link(ctx, (fz_document *)doc_src, item->uri, &x, &y));
		int page_in_range = position_in_range(ctx, cor->range, cor->page_count, page_num+1);
		int new_page_number = page_in_range + cor->page_output_base;

		if (cor->len == cor->max)
		{
			int newmax = cor->max ? cor->max * 2 : 8;
			cor->items = fz_realloc_array(ctx, cor->items, newmax, fz_outline_item);
			cor->max = newmax;
		}
		cor->len++;
		cor->items[cor->len-1].title = NULL;
		cor->items[cor->len-1].uri = NULL;
		cor->items[cor->len-1].is_open = item->is_open;
		cor->items[cor->len-1].title = item->title ? fz_strdup(ctx, item->title) : NULL;
		cor->items[cor->len-1].uri = rewrite_page(ctx, item->uri, new_page_number);

		if (page_in_range != 0)
			copy_item(cor);

		has_children = fz_outline_iterator_down(ctx, cor->it_src);
		if (has_children == 0)
			do_copy_outline_range(cor);
		if (has_children >= 0)
			fz_outline_iterator_up(ctx, cor->it_src);

		cor->len--;
		if (cor->copied_to_depth > cor->len)
		{
			cor->copied_to_depth = cor->len;
			fz_outline_iterator_up(ctx, cor->it_dst);
		}
		fz_outline_iterator_next(ctx, cor->it_dst);
		fz_free(ctx, cor->items[cor->len].title);
		fz_free(ctx, cor->items[cor->len].uri);
	}
	while (fz_outline_iterator_next(ctx, cor->it_src) == 0);
}

static void
copy_outline_range(fz_context *ctx, fz_outline_iterator *it_dst, fz_outline_iterator *it_src, const char *range, int page_count, int page_output_base)
{
	cor_state cor;

	cor.ctx = ctx;
	cor.it_dst = it_dst;
	cor.it_src = it_src;
	cor.max = 0;
	cor.len = 0;
	cor.copied_to_depth = 0;
	cor.range = range;
	cor.items = NULL;
	cor.page_count = page_count;
	cor.page_output_base = page_output_base;

	fz_try(ctx)
		do_copy_outline_range(&cor);
	fz_always(ctx)
	{
		int i;

		for (i = 0; i < cor.len; i++)
		{
			fz_free(ctx, cor.items[i].title);
			fz_free(ctx, cor.items[i].uri);
		}
		fz_free(ctx, cor.items);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}


static void merge_range(fz_context *ctx, const char *range)
{
	int start, end, i, count;
	pdf_graft_map *graft_map;
	const char *r;
	fz_outline_iterator *it_src = NULL;
	fz_outline_iterator *it_dst = NULL;
	int pages_merged = 0;

	count = pdf_count_pages(ctx, doc_src);
	graft_map = pdf_new_graft_map(ctx, doc_des);

	fz_var(it_src);
	fz_var(it_dst);

	fz_try(ctx)
	{
		r = range;
		while ((r = fz_parse_page_range(ctx, r, &start, &end, count)))
		{
			if (start < end)
				for (i = start; i <= end; ++i)
				{
					page_merge(ctx, i, graft_map);
					pages_merged++;
				}
			else
				for (i = start; i >= end; --i)
				{
					page_merge(ctx, i, graft_map);
					pages_merged++;
				}
		}

		it_src = fz_new_outline_iterator(ctx, (fz_document *)doc_src);
		if (it_src == NULL)
			break; /* Should never happen */
		it_dst = fz_new_outline_iterator(ctx, (fz_document *)doc_des);
		if (it_dst == NULL)
			break; /* Should never happen */

		/* Run to the end of it_dst. */
		if (fz_outline_iterator_item(ctx, it_dst) != NULL)
		{
			while (fz_outline_iterator_next(ctx, it_dst) == 0);
		}

		if (fz_outline_iterator_item(ctx, it_src) != NULL)
			copy_outline_range(ctx, it_dst, it_src, range, count, output_page_count);

		output_page_count += pages_merged;
	}
	fz_always(ctx)
	{
		fz_drop_outline_iterator(ctx, it_src);
		fz_drop_outline_iterator(ctx, it_dst);
		pdf_drop_graft_map(ctx, graft_map);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

int pdfmerge_main(int argc, char **argv)
{
	pdf_write_options opts = pdf_default_write_options;
	char *output = "out.pdf";
	char *flags = "";
	char *input;
	int c;
	fz_context *ctx;

	while ((c = fz_getopt(argc, argv, "o:O:")) != -1)
	{
		switch (c)
		{
		case 'o': output = fz_optpath(fz_optarg); break;
		case 'O': flags = fz_optarg; break;
		default: return usage();
		}
	}

	if (fz_optind == argc)
		return usage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "error: Cannot initialize MuPDF context.\n");
		exit(1);
	}

	pdf_parse_write_options(ctx, &opts, flags);

	fz_try(ctx)
	{
		doc_des = pdf_create_document(ctx);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fz_log_error(ctx, "Cannot create destination document.");
		fz_flush_warnings(ctx);
		fz_drop_context(ctx);
		exit(1);
	}

	/* Step through the source files */
	while (fz_optind < argc)
	{
		doc_src = NULL;
		input = argv[fz_optind++];

		fz_var(doc_src);

		fz_try(ctx)
		{
			doc_src = pdf_open_document(ctx, input);
			if (fz_optind == argc || !fz_is_page_range(ctx, argv[fz_optind]))
				merge_range(ctx, "1-N");
			else
				merge_range(ctx, argv[fz_optind++]);
		}
		fz_always(ctx)
			pdf_drop_document(ctx, doc_src);
		fz_catch(ctx)
		{
			fz_report_error(ctx);
			fz_log_error_printf(ctx, "Cannot merge document '%s'.", input);
		}
	}

	if (fz_optind == argc)
	{
		fz_try(ctx)
			pdf_save_document(ctx, doc_des, output, &opts);
		fz_catch(ctx)
		{
			fz_report_error(ctx);
			fz_log_error_printf(ctx, "Cannot save output file: '%s'.", output);
		}
	}

	pdf_drop_document(ctx, doc_des);
	fz_flush_warnings(ctx);
	fz_drop_context(ctx);
	return 0;
}
