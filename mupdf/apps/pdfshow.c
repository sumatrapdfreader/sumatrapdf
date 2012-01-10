/*
 * pdfshow -- the ultimate pdf debugging tool
 */

#include "fitz.h"
#include "mupdf.h"

static pdf_xref *xref = NULL;
static fz_context *ctx = NULL;
static int showbinary = 0;
static int showdecode = 1;
static int showcolumn;

static void usage(void)
{
	fprintf(stderr, "usage: pdfshow [options] file.pdf [grepable] [xref] [trailer] [pagetree] [object numbers]\n");
	fprintf(stderr, "\t-b\tprint streams as binary data\n");
	fprintf(stderr, "\t-e\tprint encoded streams (don't decode)\n");
	fprintf(stderr, "\t-p\tpassword\n");
	exit(1);
}

static void showtrailer(void)
{
	if (!xref)
		fz_throw(ctx, "no file specified");
	printf("trailer\n");
	fz_debug_obj(xref->trailer);
	printf("\n");
}

static void showxref(void)
{
	if (!xref)
		fz_throw(ctx, "no file specified");
	pdf_debug_xref(xref);
	printf("\n");
}

static void showpagetree(void)
{
	fz_obj *ref;
	int count;
	int i;

	if (!xref)
		fz_throw(ctx, "no file specified");

	count = pdf_count_pages(xref);
	for (i = 0; i < count; i++)
	{
		ref = xref->page_refs[i];
		printf("page %d = %d %d R\n", i + 1, fz_to_num(ref), fz_to_gen(ref));
	}
	printf("\n");
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
		stm = pdf_open_stream(xref, num, gen);
	else
		stm = pdf_open_raw_stream(xref, num, gen);

	while (1)
	{
		n = fz_read(stm, buf, sizeof buf);
		if (n == 0)
			break;
		if (showbinary)
			fwrite(buf, 1, n, stdout);
		else
			showsafe(buf, n);
	}

	fz_close(stm);
}

static void showobject(int num, int gen)
{
	fz_obj *obj;

	if (!xref)
		fz_throw(ctx, "no file specified");

	obj = pdf_load_object(xref, num, gen);

	if (pdf_is_stream(xref, num, gen))
	{
		if (showbinary)
		{
			showstream(num, gen);
		}
		else
		{
			printf("%d %d obj\n", num, gen);
			fz_debug_obj(obj);
			printf("stream\n");
			showstream(num, gen);
			printf("endstream\n");
			printf("endobj\n\n");
		}
	}
	else
	{
		printf("%d %d obj\n", num, gen);
		fz_debug_obj(obj);
		printf("endobj\n\n");
	}

	fz_drop_obj(obj);
}

static void showgrep(char *filename)
{
	fz_obj *obj;
	int i;

	for (i = 0; i < xref->len; i++)
	{
		if (xref->table[i].type == 'n' || xref->table[i].type == 'o')
		{
			fz_try(ctx)
			{
				obj = pdf_load_object(xref, i, 0);
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "skipping object (%d 0 R)", i);
				continue;
			}

			fz_sort_dict(obj);

			printf("%s:%d: ", filename, i);
			fz_fprint_obj(stdout, obj, 1);

			fz_drop_obj(obj);
		}
	}

	printf("%s:trailer: ", filename);
	fz_fprint_obj(stdout, xref->trailer, 1);
}

#ifdef MUPDF_COMBINED_EXE
int pdfshow_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	char *password = NULL; /* don't throw errors if encrypted */
	char *filename;
	int c;

	while ((c = fz_getopt(argc, argv, "p:be")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'b': showbinary = 1; break;
		case 'e': showdecode = 0; break;
		default: usage(); break;
		}
	}

	if (fz_optind == argc)
		usage();

	filename = argv[fz_optind++];

	ctx = fz_new_context(&fz_alloc_default, 256<<20);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	xref = pdf_open_xref(ctx, filename, password);

	if (fz_optind == argc)
		showtrailer();

	while (fz_optind < argc)
	{
		switch (argv[fz_optind][0])
		{
		case 't': showtrailer(); break;
		case 'x': showxref(); break;
		case 'p': showpagetree(); break;
		case 'g': showgrep(filename); break;
		default: showobject(atoi(argv[fz_optind]), 0); break;
		}
		fz_optind++;
	}

	pdf_free_xref(xref);
	fz_flush_warnings(ctx);
	fz_free_context(ctx);
	return 0;
}
