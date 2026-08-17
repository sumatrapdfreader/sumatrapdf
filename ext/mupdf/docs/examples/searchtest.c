#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <memory.h>

static void
feed_page(fz_context *ctx, fz_document *doc, fz_search *search, int page_num, fz_stext_options *options)
{
	fz_stext_page *page;

	// printf("FEEDING page %d\n", page_num+1);
	page = fz_new_stext_page_from_page_number(ctx, doc, page_num, options);
	fz_feed_search(ctx, search, page, page_num);
}

static void
search_test(fz_context *ctx, fz_search_options options, const char *needle, int first_page, int backwards)
{
	fz_stext_options stext_options = { FZ_STEXT_DEHYPHENATE };
	fz_document *doc = fz_open_document(ctx, "pdfref17.pdf");
	int n = fz_count_pages(ctx, doc);
	fz_search *search = NULL;
	fz_search_result res;
	fz_stext_line *line;
	fz_stext_char *ch;

	fz_var(search);

	printf("search_test(needle='%s' options=%d first_page=%d backwards=%d)\n", needle, options, first_page, backwards);

	if (first_page < 0)
		first_page = n+first_page;

	fz_try(ctx)
	{
		search = fz_new_search(ctx, needle, options);

		if (first_page != 0)
			feed_page(ctx, doc, search, first_page, &stext_options);
		while (1)
		{
			if (backwards)
				res = fz_search_backwards(ctx, search);
			else
				res = fz_search_forwards(ctx, search);

			switch (res.reason)
			{
			case FZ_SEARCH_MATCH:
				printf("Match(%s): %d quads (starting on page %d)\n", needle, res.u.match->num_quads, res.u.match->quads[0].seq);
				if (0)
				{
					int i;
					for (i = 0; i < res.u.match->num_quads; ++i)
					{
						printf("\tseq=%d q=%g %g %g %g %g %g %g %g\n",
						res.u.match->quads[i].seq,
						res.u.match->quads[i].quad.ul.x,
						res.u.match->quads[i].quad.ul.y,
						res.u.match->quads[i].quad.ur.x,
						res.u.match->quads[i].quad.ur.y,
						res.u.match->quads[i].quad.ll.x,
						res.u.match->quads[i].quad.ll.y,
						res.u.match->quads[i].quad.lr.x,
						res.u.match->quads[i].quad.lr.y);
					}

					line = res.u.match->begin.line;
					ch = res.u.match->begin.ch;
					while (ch != res.u.match->end.ch)
					{
						printf("\tchar %c\n", ch->c);

						ch = ch->next;
						if (ch == NULL)
						{
							line = line->next;
							if (line == NULL)
								break;

							ch = line->first_char;
						}
					}
					if (ch == res.u.match->end.ch)
						printf("\tchar %c\n", ch->c);
				}
				break;
			case FZ_SEARCH_MORE_INPUT:
				if (res.u.seq_needed < 0 || res.u.seq_needed == n)
				{
					printf("No page %d, so END\n", res.u.seq_needed);
					fz_feed_search(ctx, search, NULL, res.u.seq_needed);
				}
				else
				{
					feed_page(ctx, doc, search, res.u.seq_needed, &stext_options);
				}
				break;
			}
			if (res.reason == FZ_SEARCH_COMPLETE)
			{
				printf("COMPLETE\n");
				break;
			}
		}
	}
	fz_always(ctx)
	{
		fz_drop_search(ctx, search);
		fz_drop_document(ctx, doc);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

int main(int argc, const char *argv[])
{
	fz_context *ctx;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (ctx == NULL)
	{
		fprintf(stderr, "Failed to create context");
		return 1;
	}

	fz_try(ctx)
	{
		fz_register_document_handlers(ctx);

		search_test(ctx, FZ_SEARCH_EXACT, "Adobe", 0, 0);
		search_test(ctx, FZ_SEARCH_IGNORE_CASE, "aDoBe", 0, 0);
		search_test(ctx, FZ_SEARCH_IGNORE_DIACRITICS, "A Aacute", 997-1, 0);
		search_test(ctx, FZ_SEARCH_IGNORE_DIACRITICS | FZ_SEARCH_IGNORE_CASE, "a aacute", 997-1, 0);
		search_test(ctx, FZ_SEARCH_EXACT, "MAKES NO WARRANTY", 1, 0);
		search_test(ctx, FZ_SEARCH_REGEXP | FZ_SEARCH_KEEP_PARAGRAPHS, "Ad.be", 0, 0);
		search_test(ctx, FZ_SEARCH_EXACT, "Adobe", -1, 1);
		search_test(ctx, FZ_SEARCH_REGEXP | FZ_SEARCH_KEEP_PARAGRAPHS, "Ad.be", -1, 1);
		/* This one seems broken */
		search_test(ctx, FZ_SEARCH_REGEXP | FZ_SEARCH_KEEP_PARAGRAPHS, "Ad.*?be", 0, 0);

		break;
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
	}

	fz_drop_context(ctx);

	return 0;
}
