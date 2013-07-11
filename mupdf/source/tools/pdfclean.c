/*
 * PDF cleaning tool: general purpose pdf syntax washer.
 *
 * Rewrite PDF with pretty printed objects.
 * Garbage collect unreachable objects.
 * Inflate compressed streams.
 * Create subset documents.
 *
 * TODO: linearize document for fast web view
 */

#include "mupdf/pdf.h"

static pdf_document *doc = NULL;
static fz_context *ctx = NULL;

static void usage(void)
{
	fprintf(stderr,
		"usage: mutool clean [options] input.pdf [output.pdf] [pages]\n"
		"\t-p -\tpassword\n"
		"\t-g\tgarbage collect unused objects\n"
		"\t-gg\tin addition to -g compact xref table\n"
		"\t-ggg\tin addition to -gg merge duplicate objects\n"
		"\t-d\tdecompress all streams\n"
		"\t-l\tlinearize PDF\n"
		"\t-i\ttoggle decompression of image streams\n"
		"\t-f\ttoggle decompression of font streams\n"
		"\t-a\tascii hex encode binary streams\n"
		"\tpages\tcomma separated list of ranges\n");
	exit(1);
}

/*
 * Recreate page tree to only retain specified pages.
 */

static void retainpages(int argc, char **argv)
{
	pdf_obj *oldroot, *root, *pages, *kids, *countobj, *parent, *olddests;

	/* Keep only pages/type and (reduced) dest entries to avoid
	 * references to unretained pages */
	oldroot = pdf_dict_gets(pdf_trailer(doc), "Root");
	pages = pdf_dict_gets(oldroot, "Pages");
	olddests = pdf_load_name_tree(doc, "Dests");

	root = pdf_new_dict(doc, 2);
	pdf_dict_puts(root, "Type", pdf_dict_gets(oldroot, "Type"));
	pdf_dict_puts(root, "Pages", pdf_dict_gets(oldroot, "Pages"));

	pdf_update_object(doc, pdf_to_num(oldroot), root);

	pdf_drop_obj(root);

	/* Create a new kids array with only the pages we want to keep */
	parent = pdf_new_indirect(doc, pdf_to_num(pages), pdf_to_gen(pages));
	kids = pdf_new_array(doc, 1);

	/* Retain pages specified */
	while (argc - fz_optind)
	{
		int page, spage, epage, pagecount;
		char *spec, *dash;
		char *pagelist = argv[fz_optind];

		pagecount = pdf_count_pages(doc);
		spec = fz_strsep(&pagelist, ",");
		while (spec)
		{
			dash = strchr(spec, '-');

			if (dash == spec)
				spage = epage = pagecount;
			else
				spage = epage = atoi(spec);

			if (dash)
			{
				if (strlen(dash) > 1)
					epage = atoi(dash + 1);
				else
					epage = pagecount;
			}

			if (spage > epage)
				page = spage, spage = epage, epage = page;

			spage = fz_clampi(spage, 1, pagecount);
			epage = fz_clampi(epage, 1, pagecount);

			for (page = spage; page <= epage; page++)
			{
				pdf_obj *pageref = pdf_lookup_page_obj(doc, page-1);
				pdf_obj *pageobj = pdf_resolve_indirect(pageref);

				pdf_dict_puts(pageobj, "Parent", parent);

				/* Store page object in new kids array */
				pdf_array_push(kids, pageref);
			}

			spec = fz_strsep(&pagelist, ",");
		}

		fz_optind++;
	}

	pdf_drop_obj(parent);

	/* Update page count and kids array */
	countobj = pdf_new_int(doc, pdf_array_len(kids));
	pdf_dict_puts(pages, "Count", countobj);
	pdf_drop_obj(countobj);
	pdf_dict_puts(pages, "Kids", kids);
	pdf_drop_obj(kids);

	/* Also preserve the (partial) Dests name tree */
	if (olddests)
	{
		int i;
		pdf_obj *names = pdf_new_dict(doc, 1);
		pdf_obj *dests = pdf_new_dict(doc, 1);
		pdf_obj *names_list = pdf_new_array(doc, 32);
		int len = pdf_dict_len(olddests);

		for (i = 0; i < len; i++)
		{
			pdf_obj *key = pdf_dict_get_key(olddests, i);
			pdf_obj *val = pdf_dict_get_val(olddests, i);
			pdf_obj *key_str = pdf_new_string(doc, pdf_to_name(key), strlen(pdf_to_name(key)));
			pdf_obj *dest = pdf_dict_gets(val, "D");

			dest = pdf_array_get(dest ? dest : val, 0);
			if (pdf_array_contains(pdf_dict_gets(pages, "Kids"), dest))
			{
				pdf_array_push(names_list, key_str);
				pdf_array_push(names_list, val);
			}
			pdf_drop_obj(key_str);
		}

		root = pdf_dict_gets(pdf_trailer(doc), "Root");
		pdf_dict_puts(dests, "Names", names_list);
		pdf_dict_puts(names, "Dests", dests);
		pdf_dict_puts(root, "Names", names);

		pdf_drop_obj(names);
		pdf_drop_obj(dests);
		pdf_drop_obj(names_list);
		pdf_drop_obj(olddests);
	}
}

int pdfclean_main(int argc, char **argv)
{
	char *infile;
	char *outfile = "out.pdf";
	char *password = "";
	int c;
	int subset;
	fz_write_options opts;
	int write_failed = 0;
	int errors = 0;

	opts.do_incremental = 0;
	opts.do_garbage = 0;
	opts.do_expand = 0;
	opts.do_ascii = 0;
	opts.do_linear = 0;
	opts.continue_on_error = 1;
	opts.errors = &errors;

	while ((c = fz_getopt(argc, argv, "adfgilp:")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'g': opts.do_garbage ++; break;
		case 'd': opts.do_expand ^= fz_expand_all; break;
		case 'f': opts.do_expand ^= fz_expand_fonts; break;
		case 'i': opts.do_expand ^= fz_expand_images; break;
		case 'l': opts.do_linear ++; break;
		case 'a': opts.do_ascii ++; break;
		default: usage(); break;
		}
	}

	if (argc - fz_optind < 1)
		usage();

	infile = argv[fz_optind++];

	if (argc - fz_optind > 0 &&
		(strstr(argv[fz_optind], ".pdf") || strstr(argv[fz_optind], ".PDF")))
	{
		outfile = argv[fz_optind++];
	}

	subset = 0;
	if (argc - fz_optind > 0)
		subset = 1;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_try(ctx)
	{
		doc = pdf_open_document_no_run(ctx, infile);
		if (pdf_needs_password(doc))
			if (!pdf_authenticate_password(doc, password))
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", infile);

		/* Only retain the specified subset of the pages */
		if (subset)
			retainpages(argc, argv);

		pdf_write_document(doc, outfile, &opts);
	}
	fz_always(ctx)
	{
		pdf_close_document(doc);
	}
	fz_catch(ctx)
	{
		write_failed = 1;
	}

	fz_free_context(ctx);

	if (errors)
		write_failed = 1;
	return write_failed ? 1 : 0;
}
