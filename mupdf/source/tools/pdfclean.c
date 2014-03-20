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

typedef struct globals_s
{
	pdf_document *doc;
	fz_context *ctx;
} globals;

static void usage(void)
{
	fprintf(stderr,
		"usage: mutool clean [options] input.pdf [output.pdf] [pages]\n"
		"\t-p -\tpassword\n"
		"\t-g\tgarbage collect unused objects\n"
		"\t-gg\tin addition to -g compact xref table\n"
		"\t-ggg\tin addition to -gg merge duplicate objects\n"
		"\t-s\tclean content streams\n"
		"\t-d\tdecompress all streams\n"
		"\t-l\tlinearize PDF\n"
		"\t-i\ttoggle decompression of image streams\n"
		"\t-f\ttoggle decompression of font streams\n"
		"\t-a\tascii hex encode binary streams\n"
		"\tpages\tcomma separated list of ranges\n");
	exit(1);
}

static int
string_in_names_list(pdf_obj *p, pdf_obj *names_list)
{
	int n = pdf_array_len(names_list);
	int i;
	char *str = pdf_to_str_buf(p);

	for (i = 0; i < n ; i += 2)
	{
		if (!strcmp(pdf_to_str_buf(pdf_array_get(names_list, i)), str))
			return 1;
	}
	return 0;
}

/*
 * Recreate page tree to only retain specified pages.
 */

static void retainpage(pdf_document *doc, pdf_obj *parent, pdf_obj *kids, int page)
{
	pdf_obj *pageref = pdf_lookup_page_obj(doc, page-1);
	pdf_obj *pageobj = pdf_resolve_indirect(pageref);

	pdf_dict_puts(pageobj, "Parent", parent);

	/* Store page object in new kids array */
	pdf_array_push(kids, pageref);
}

static void retainpages(globals *glo, int argc, char **argv)
{
	pdf_obj *oldroot, *root, *pages, *kids, *countobj, *parent, *olddests;
	pdf_document *doc = glo->doc;
	int argidx = 0;
	pdf_obj *names_list = NULL;
	int pagecount;
	int i;

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
	while (argc - argidx)
	{
		int page, spage, epage;
		char *spec, *dash;
		char *pagelist = argv[argidx];

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

			spage = fz_clampi(spage, 1, pagecount);
			epage = fz_clampi(epage, 1, pagecount);

			if (spage < epage)
				for (page = spage; page <= epage; ++page)
					retainpage(doc, parent, kids, page);
			else
				for (page = spage; page >= epage; --page)
					retainpage(doc, parent, kids, page);

			spec = fz_strsep(&pagelist, ",");
		}

		argidx++;
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
		pdf_obj *names = pdf_new_dict(doc, 1);
		pdf_obj *dests = pdf_new_dict(doc, 1);
		int len = pdf_dict_len(olddests);

		names_list = pdf_new_array(doc, 32);

		for (i = 0; i < len; i++)
		{
			pdf_obj *key = pdf_dict_get_key(olddests, i);
			pdf_obj *val = pdf_dict_get_val(olddests, i);
			pdf_obj *dest = pdf_dict_gets(val, "D");

			dest = pdf_array_get(dest ? dest : val, 0);
			if (pdf_array_contains(pdf_dict_gets(pages, "Kids"), dest))
			{
				pdf_obj *key_str = pdf_new_string(doc, pdf_to_name(key), strlen(pdf_to_name(key)));
				pdf_array_push(names_list, key_str);
				pdf_array_push(names_list, val);
				pdf_drop_obj(key_str);
			}
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

	/* Force the next call to pdf_count_pages to recount */
	glo->doc->page_count = 0;

	/* Edit each pages /Annot list to remove any links that point to
	 * nowhere. */
	pagecount = pdf_count_pages(doc);
	for (i = 0; i < pagecount; i++)
	{
		pdf_obj *pageref = pdf_lookup_page_obj(doc, i);
		pdf_obj *pageobj = pdf_resolve_indirect(pageref);

		pdf_obj *annots = pdf_dict_gets(pageobj, "Annots");

		int len = pdf_array_len(annots);
		int j;

		for (j = 0; j < len; j++)
		{
			pdf_obj *o = pdf_array_get(annots, j);
			pdf_obj *p;

			if (strcmp(pdf_to_name(pdf_dict_gets(o, "Subtype")), "Link"))
				continue;

			p = pdf_dict_gets(o, "A");
			if (strcmp(pdf_to_name(pdf_dict_gets(p, "S")), "GoTo"))
				continue;

			if (string_in_names_list(pdf_dict_gets(p, "D"), names_list))
				continue;

			/* FIXME: Should probably look at Next too */

			/* Remove this annotation */
			pdf_array_delete(annots, j);
			j--;
		}
	}
}

void pdfclean_clean(fz_context *ctx, char *infile, char *outfile, char *password, fz_write_options *opts, char *argv[], int argc)
{
	globals glo = { 0 };

	glo.ctx = ctx;

	fz_try(ctx)
	{
		glo.doc = pdf_open_document_no_run(ctx, infile);
		if (pdf_needs_password(glo.doc))
			if (!pdf_authenticate_password(glo.doc, password))
				fz_throw(glo.ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", infile);

		/* Only retain the specified subset of the pages */
		if (argc)
			retainpages(&glo, argc, argv);

		pdf_write_document(glo.doc, outfile, opts);
	}
	fz_always(ctx)
	{
		pdf_close_document(glo.doc);
	}
	fz_catch(ctx)
	{
		if (opts && opts->errors)
			*opts->errors = *opts->errors+1;
	}
}

int pdfclean_main(int argc, char **argv)
{
	char *infile;
	char *outfile = "out.pdf";
	char *password = "";
	int c;
	fz_write_options opts;
	int errors = 0;
	fz_context *ctx;

	opts.do_incremental = 0;
	opts.do_garbage = 0;
	opts.do_expand = 0;
	opts.do_ascii = 0;
	opts.do_linear = 0;
	opts.continue_on_error = 1;
	opts.errors = &errors;
	opts.do_clean = 0;

	while ((c = fz_getopt(argc, argv, "adfgilp:s")) != -1)
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
		case 's': opts.do_clean ++; break;
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

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_try(ctx)
	{
		pdfclean_clean(ctx, infile, outfile, password, &opts, &argv[fz_optind], argc - fz_optind);
	}
	fz_catch(ctx)
	{
		errors++;
	}
	fz_free_context(ctx);

	return errors == 0;
}
