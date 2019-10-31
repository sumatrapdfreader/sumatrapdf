#include "mupdf/fitz.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void usage(void)
{
	fprintf(stderr,
		"Usage: mutool trace [options] file [pages]\n"
		"\t-p -\tpassword\n"
		"\n"
		"\t-W -\tpage width for EPUB layout\n"
		"\t-H -\tpage height for EPUB layout\n"
		"\t-S -\tfont size for EPUB layout\n"
		"\t-U -\tfile name of user stylesheet for EPUB layout\n"
		"\t-X\tdisable document styles for EPUB layout\n"
		"\n"
		"\t-d\tuse display list\n"
		"\n"
		"\tpages\tcomma separated list of page numbers and ranges\n"
		);
	exit(1);
}

static float layout_w = FZ_DEFAULT_LAYOUT_W;
static float layout_h = FZ_DEFAULT_LAYOUT_H;
static float layout_em = FZ_DEFAULT_LAYOUT_EM;
static char *layout_css = NULL;
static int layout_use_doc_css = 1;

static int use_display_list = 0;

static void runpage(fz_context *ctx, fz_document *doc, int number)
{
	fz_page *page = NULL;
	fz_display_list *list = NULL;
	fz_device *dev = NULL;
	fz_rect mediabox;

	fz_var(page);
	fz_var(list);
	fz_var(dev);
	fz_try(ctx)
	{
		page = fz_load_page(ctx, doc, number - 1);
		mediabox = fz_bound_page(ctx, page);
		printf("<page number=\"%d\" mediabox=\"%g %g %g %g\">\n",
				number, mediabox.x0, mediabox.y0, mediabox.x1, mediabox.y1);
		dev = fz_new_trace_device(ctx, fz_stdout(ctx));
		if (use_display_list)
		{
			list = fz_new_display_list_from_page(ctx, page);
			fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
		}
		else
		{
			fz_run_page(ctx, page, dev, fz_identity, NULL);
		}
		printf("</page>\n");
	}
	fz_always(ctx)
	{
		fz_drop_display_list(ctx, list);
		fz_drop_page(ctx, page);
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void runrange(fz_context *ctx, fz_document *doc, int count, const char *range)
{
	int start, end, i;

	while ((range = fz_parse_page_range(ctx, range, &start, &end, count)))
	{
		if (start < end)
			for (i = start; i <= end; ++i)
				runpage(ctx, doc, i);
		else
			for (i = start; i >= end; --i)
				runpage(ctx, doc, i);
	}
}

int mutrace_main(int argc, char **argv)
{
	fz_context *ctx;
	fz_document *doc = NULL;
	char *password = "";
	int i, c, count;

	while ((c = fz_getopt(argc, argv, "p:W:H:S:U:Xd")) != -1)
	{
		switch (c)
		{
		default: usage(); break;
		case 'p': password = fz_optarg; break;

		case 'W': layout_w = fz_atof(fz_optarg); break;
		case 'H': layout_h = fz_atof(fz_optarg); break;
		case 'S': layout_em = fz_atof(fz_optarg); break;
		case 'U': layout_css = fz_optarg; break;
		case 'X': layout_use_doc_css = 0; break;

		case 'd': use_display_list = 1; break;
		}
	}

	if (fz_optind == argc)
		usage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return EXIT_FAILURE;
	}

	fz_try(ctx)
	{
		fz_register_document_handlers(ctx);
		if (layout_css)
		{
			fz_buffer *buf = fz_read_file(ctx, layout_css);
			fz_set_user_css(ctx, fz_string_from_buffer(ctx, buf));
			fz_drop_buffer(ctx, buf);
		}
		fz_set_use_document_css(ctx, layout_use_doc_css);
	}
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot initialize mupdf: %s\n",  fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	fz_var(doc);
	fz_try(ctx)
	{
		for (i = fz_optind; i < argc; ++i)
		{
			doc = fz_open_document(ctx, argv[i]);
			if (fz_needs_password(ctx, doc))
				if (!fz_authenticate_password(ctx, doc, password))
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", argv[i]);
			fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);
			printf("<document filename=\"%s\">\n", argv[i]);
			count = fz_count_pages(ctx, doc);
			if (i+1 < argc && fz_is_page_range(ctx, argv[i+1]))
				runrange(ctx, doc, count, argv[++i]);
			else
				runrange(ctx, doc, count, "1-N");
			printf("</document>\n");
			fz_drop_document(ctx, doc);
			doc = NULL;
		}
	}
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot run document: %s\n", fz_caught_message(ctx));
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	fz_drop_context(ctx);
	return EXIT_SUCCESS;
}
