/*
 * pdfshow -- the ultimate pdf debugging tool
 */

#include "mupdf/pdf.h"

static FILE *out = NULL;

static pdf_document *doc = NULL;
static fz_context *ctx = NULL;
static int showbinary = 0;
static int showdecode = 1;
static int showcolumn;

static void usage(void)
{
	fprintf(stderr, "usage: mutool show [options] file.pdf [grep] [xref] [trailer] [pages] [object numbers]\n");
	fprintf(stderr, "\t-b\tprint streams as binary data\n");
	fprintf(stderr, "\t-e\tprint encoded streams (don't decode)\n");
	fprintf(stderr, "\t-p\tpassword\n");
	fprintf(stderr, "\t-o\toutput file\n");
	exit(1);
}

static void showtrailer(void)
{
	if (!doc)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no file specified");
	fprintf(out, "trailer\n");
	pdf_fprint_obj(out, pdf_trailer(doc), 0);
	fprintf(out, "\n");
}

static void showencrypt(void)
{
	pdf_obj *encrypt;

	if (!doc)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no file specified");
	encrypt = pdf_dict_gets(pdf_trailer(doc), "Encrypt");
	if (!encrypt)
		fz_throw(ctx, FZ_ERROR_GENERIC, "document not encrypted");
	fprintf(out, "encryption dictionary\n");
	pdf_fprint_obj(out, pdf_resolve_indirect(encrypt), 0);
	fprintf(out, "\n");
}

static void showxref(void)
{
	if (!doc)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no file specified");
	pdf_print_xref(doc);
	fprintf(out, "\n");
}

static void showpagetree(void)
{
	pdf_obj *ref;
	int count;
	int i;

	if (!doc)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no file specified");

	count = pdf_count_pages(doc);
	for (i = 0; i < count; i++)
	{
		ref = pdf_lookup_page_obj(doc, i);
		fprintf(out, "page %d = %d %d R\n", i + 1, pdf_to_num(ref), pdf_to_gen(ref));
	}
	fprintf(out, "\n");
}

static void showsafe(unsigned char *buf, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		if (buf[i] == '\r' || buf[i] == '\n') {
			putchar('\n');
			showcolumn = 0;
		}
		else if (buf[i] < 32 || buf[i] > 126) {
			putchar('.');
			showcolumn ++;
		}
		else {
			putchar(buf[i]);
			showcolumn ++;
		}
		if (showcolumn == 79) {
			putchar('\n');
			showcolumn = 0;
		}
	}
}

static void showstream(int num, int gen)
{
	fz_stream *stm;
	unsigned char buf[2048];
	int n;

	showcolumn = 0;

	if (showdecode)
		stm = pdf_open_stream(doc, num, gen);
	else
		stm = pdf_open_raw_stream(doc, num, gen);

	while (1)
	{
		n = fz_read(stm, buf, sizeof buf);
		if (n == 0)
			break;
		if (showbinary)
			fwrite(buf, 1, n, out);
		else
			showsafe(buf, n);
	}

	fz_close(stm);
}

static void showobject(int num, int gen)
{
	pdf_obj *obj;

	if (!doc)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no file specified");

	obj = pdf_load_object(doc, num, gen);

	if (pdf_is_stream(doc, num, gen))
	{
		if (showbinary)
		{
			showstream(num, gen);
		}
		else
		{
			fprintf(out, "%d %d obj\n", num, gen);
			pdf_fprint_obj(out, obj, 0);
			fprintf(out, "stream\n");
			showstream(num, gen);
			fprintf(out, "endstream\n");
			fprintf(out, "endobj\n\n");
		}
	}
	else
	{
		fprintf(out, "%d %d obj\n", num, gen);
		pdf_fprint_obj(out, obj, 0);
		fprintf(out, "endobj\n\n");
	}

	pdf_drop_obj(obj);
}

static void showgrep(char *filename)
{
	pdf_obj *obj;
	int i, len;

	len = pdf_count_objects(doc);
	for (i = 0; i < len; i++)
	{
		pdf_xref_entry *entry = pdf_get_xref_entry(doc, i);
		if (entry->type == 'n' || entry->type == 'o')
		{
			fz_try(ctx)
			{
				obj = pdf_load_object(doc, i, 0);
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "skipping object (%d 0 R)", i);
				continue;
			}

			pdf_sort_dict(obj);

			fprintf(out, "%s:%d: ", filename, i);
			pdf_fprint_obj(out, obj, 1);

			pdf_drop_obj(obj);
		}
	}

	fprintf(out, "%s:trailer: ", filename);
	pdf_fprint_obj(out, pdf_trailer(doc), 1);
}

int pdfshow_main(int argc, char **argv)
{
	char *password = NULL; /* don't throw errors if encrypted */
	char *filename = NULL;
	char *output = NULL;
	int c;

	while ((c = fz_getopt(argc, argv, "p:o:be")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'b': showbinary = 1; break;
		case 'e': showdecode = 0; break;
		case 'o': output = fz_optarg; break;
		default: usage(); break;
		}
	}

	if (fz_optind == argc)
		usage();

	filename = argv[fz_optind++];

	out = stdout;
	if (output)
	{
		out = fopen(output, "wb");
		if (!out)
		{
			fprintf(stderr, "cannot open output file: '%s'\n", output);
			exit(1);
		}
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_var(doc);
	fz_try(ctx)
	{
		doc = pdf_open_document_no_run(ctx, filename);
		if (pdf_needs_password(doc))
			if (!pdf_authenticate_password(doc, password))
				fz_warn(ctx, "cannot authenticate password: %s", filename);

		if (fz_optind == argc)
			showtrailer();

		while (fz_optind < argc)
		{
			switch (argv[fz_optind][0])
			{
			case 't': showtrailer(); break;
			case 'e': showencrypt(); break;
			case 'x': showxref(); break;
			case 'p': showpagetree(); break;
			case 'g': showgrep(filename); break;
			default: showobject(atoi(argv[fz_optind]), 0); break;
			}
			fz_optind++;
		}
	}
	fz_catch(ctx)
	{
	}

	if (out != stdout)
		fclose(out);

	pdf_close_document(doc);
	fz_free_context(ctx);
	return 0;
}
