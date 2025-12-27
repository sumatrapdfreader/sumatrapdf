// Copyright (C) 2025 Artifex Software, Inc.
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
 * mutool grep -- command line tool for searching in documents
 */

#include "mupdf/fitz.h"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#else
#include <unistd.h>
#endif

static fz_context *ctx = NULL;
static fz_output *out = NULL;
static int show_page_number = 0;
static int show_file_name = 0;
static int search_backwards = 0;

static const char *mark_open = "";
static const char *mark_close = "";

static int mugrep_usage(void)
{
	fprintf(stderr,
		"usage: mugrep [options] pattern input.pdf [ input2.pdf ... ]\n"
		"\t-p -\tpassword for encrypted PDF files\n"
		"\t-G\tpattern is a regexp\n"
		"\t-a\tignore accents (diacritics)\n"
		"\t-i\tignore case\n"
		"\t-H\tprint filename for each match\n"
		"\t-n\tprint page number for each match\n"
		"advanced options:\n"
		"\t-S\tcomma-separated list of search options\n"
		"\t-O\tcomma-separated list of stext options\n"
		"\t-b\tsearch pages in backwards order\n"
		"\t-[ -\tstart marker\n"
		"\t-] -\tend marker\n"
		"\t-v\tverbose\n"
		"\t-q\tquiet (don't print warnings and errors)\n"
		"\n"
	);
	fputs(fz_search_options_usage, stderr);
	fputs(fz_stext_options_usage, stderr);
	return EXIT_FAILURE;
}

static int
show_match_rec(fz_stext_block *block, fz_stext_line *begin_line, fz_stext_char *begin_ch, fz_stext_line *end_line, fz_stext_char *end_ch, int *last)
{
	fz_stext_line *line;
	fz_stext_char *ch;
	while (block)
	{
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_TEXT:
			for (line = block->u.t.first_line; line; line = line->next)
			{
				if (line == begin_line)
					begin_line = NULL;
				if (!begin_line)
				{
					for (ch = line->first_char; ch; ch = ch->next)
					{
						if (ch == begin_ch)
							fz_write_string(ctx, out, mark_open);
						if (ch->c < 32)
							fz_write_byte(ctx, out, ' ');
						else if (ch->c != 0xad)
							fz_write_rune(ctx, out, ch->c);
						if (ch == end_ch)
							fz_write_string(ctx, out, mark_close);
						*last = ch->c;
					}
					if (!fz_is_unicode_whitespace(*last) && *last != 0xad)
						fz_write_string(ctx, out, " ");
				}
				if (line == end_line)
				{
					return 1;
				}
			}
			break;
		case FZ_STEXT_BLOCK_STRUCT:
			if (block->u.s.down)
			{
				if (show_match_rec(block->u.s.down->first_block, begin_line, begin_ch, end_line, end_ch, last))
					return 1;
			}
			break;
		}
		block = block->next;
	}
	return 0;
}

static void
show_match_snippet(char *file_name, int page_number, fz_stext_position begin, fz_stext_position end)
{
	int last = 0;

	if (show_file_name)
		fz_write_printf(ctx, out, "%s\t", file_name);
	if (show_page_number)
		fz_write_printf(ctx, out, "%d\t", page_number);

	if (begin.page == end.page)
	{
		(void)show_match_rec(begin.page->first_block, begin.line, begin.ch, end.line, end.ch, &last);
	}
	else
	{
		(void)show_match_rec(begin.page->first_block, begin.line, begin.ch, NULL, NULL, &last);
		(void)show_match_rec(end.page->first_block, NULL, NULL, end.line, end.ch, &last);
	}

	fz_write_byte(ctx, out, '\n');
}

static int
mugrep_run(char *filename, fz_document *doc, char *pattern, fz_search_options options, fz_stext_options *stext_options, int verbose)
{
	int page_count = fz_count_pages(ctx, doc);
	fz_search *search = NULL;
	fz_search_result res;
	int found = 0;

	fz_var(search);

	fz_try(ctx)
	{
		search = fz_new_search(ctx);
		fz_search_set_options(ctx, search, options, pattern);

		if (search_backwards)
		{
			fz_stext_page *page = fz_new_stext_page_from_page_number(ctx, doc, page_count - 1, stext_options);
			fz_feed_search(ctx, search, page, page_count - 1);
		}
		else
		{
			fz_stext_page *page = fz_new_stext_page_from_page_number(ctx, doc, 0, stext_options);
			fz_feed_search(ctx, search, page, 0);
		}

		for (;;)
		{
			if (search_backwards)
				res = fz_search_backwards(ctx, search);
			else
				res = fz_search_forwards(ctx, search);
			if (res.reason == FZ_SEARCH_MATCH)
			{
				fz_search_result_details *details = res.u.match.result;

				found++;

				if (verbose)
				{
					printf("MATCH: %d quads (starting on page %d)\n", details->num_quads, details->quads[0].seq+1);
				}

				show_match_snippet(filename, details->quads[0].seq + 1, details->begin, details->end);
			}
			else if (res.reason == FZ_SEARCH_MORE_INPUT)
			{
				if (res.u.more_input.seq_needed < 0 || res.u.more_input.seq_needed == page_count)
				{
					if (verbose)
						printf("FEEDING END\n");
					fz_feed_search(ctx, search, NULL, res.u.more_input.seq_needed);
				}
				else
				{
					int page_num = res.u.more_input.seq_needed;
					if (verbose)
						printf("FEEDING page %d\n", page_num);
					fz_stext_page *page = fz_new_stext_page_from_page_number(ctx, doc, page_num, stext_options);
					fz_feed_search(ctx, search, page, page_num);
				}
			}
			else if (res.reason == FZ_SEARCH_COMPLETE)
				break;
		}
	}
	fz_always(ctx)
		fz_drop_search(ctx, search);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return found;
}

static void silence(void *user, const char *message) {}

int mugrep_main(int argc, char **argv)
{
	fz_document *doc = NULL;
	char *password = NULL; /* don't throw errors if encrypted */
	char *filename;
	char *pattern;
	int result = EXIT_FAILURE;
	int quiet = 0;
	int c;
	fz_search_options options = FZ_SEARCH_EXACT;
	fz_stext_options stext_options = { 0 };
	int verbose = 0;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_register_document_handlers(ctx);

	out = fz_stdout(ctx);

	if (isatty(1))
	{
		mark_open = "\x1b[1m";
		mark_close = "\x1b[0m";
	}

	while ((c = fz_getopt(argc, argv, "Gaip:vO:S:nHb[:]:q")) != -1)
	{
		switch (c)
		{
		case 'O':
			fz_parse_stext_options(ctx, &stext_options, fz_optarg);
			break;
		case 'S':
			options = fz_parse_search_options(fz_optarg);
			break;
		case 'G':
			options |= FZ_SEARCH_REGEXP | FZ_SEARCH_KEEP_LINES | FZ_SEARCH_KEEP_PARAGRAPHS;
			break;
		case 'a':
			options |= FZ_SEARCH_IGNORE_DIACRITICS;
			break;
		case 'i':
			options |= FZ_SEARCH_IGNORE_CASE;
			break;
		case 'p':
			password = fz_optarg;
			break;
		case 'n':
			show_page_number = 1;
			break;
		case 'H':
			show_file_name = 1;
			break;
		case 'b':
			search_backwards = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case '[':
			mark_open = mark_close = fz_optarg;
			break;
		case ']':
			mark_close = fz_optarg;
			break;
		default:
			return mugrep_usage();
		}
	}

	if (quiet > 0)
	{
		fz_set_warning_callback(ctx, silence, NULL);
		fz_set_error_callback(ctx, silence, NULL);
	}

	if (fz_optind == argc)
		return mugrep_usage();

	pattern = argv[fz_optind++];

	fz_var(doc);

	fz_try(ctx)
	{
		while (fz_optind < argc)
		{
			filename = argv[fz_optind++];

			doc = fz_open_document(ctx, filename);
			if (fz_needs_password(ctx, doc))
				if (!fz_authenticate_password(ctx, doc, password))
				{
					fz_warn(ctx, "cannot authenticate password: %s", filename);
					fz_drop_document(ctx, doc);
					doc = NULL;
					continue;
				}

			if (mugrep_run(filename, doc, pattern, options, &stext_options, verbose))
				result = EXIT_SUCCESS;

			fz_drop_document(ctx, doc);
			doc = NULL;
		}
	}
	fz_always(ctx)
	{
		fz_drop_document(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		result = EXIT_FAILURE;
	}

	fz_drop_context(ctx);

	return result;
}
