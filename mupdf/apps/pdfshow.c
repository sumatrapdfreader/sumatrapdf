/*
 * pdfshow -- the ultimate pdf debugging tool
 */

#include "fitz.h"
#include "mupdf.h"

pdf_xref *xref = NULL;

void die(fz_error eo)
{
    fz_catch(eo, "aborting");
    exit(1);
}

void openxref(char *filename, char *password)
{
    fz_error error;
    fz_obj *obj;

    error = pdf_newxref(&xref);
    if (error)
	die(error);

    error = pdf_loadxref(xref, filename);
    if (error)
    {
	fz_catch(error, "trying to repair");
	error = pdf_repairxref(xref, filename);
	if (error)
	    die(error);
    }

    error = pdf_decryptxref(xref);
    if (error)
	die(error);

    if (xref->crypt)
    {
	int okay = pdf_setpassword(xref->crypt, password);
	if (!okay)
	    die(fz_throw("invalid password"));
    }

    /* TODO: move into mupdf lib, see pdfapp_open in pdfapp.c */
    obj = fz_dictgets(xref->trailer, "Root");
    if (!obj)
	die(error);

    error = pdf_loadindirect(&xref->root, xref, obj);
    if (error)
	die(error);

    obj = fz_dictgets(xref->trailer, "Info");
    if (obj)
    {
	error = pdf_loadindirect(&xref->info, xref, obj);
	if (error)
	    die(error);
    }
}

int showbinary = 0;
int showdecode = 0;
int showcolumn;

void showusage(void)
{
    fprintf(stderr, "usage: pdfshow [-bd] [-p password] <file> [xref] [trailer] [object numbers]\n");
    fprintf(stderr, "  -b  \tprint streams as raw binary data\n");
    fprintf(stderr, "  -x  \tdecompress streams\n");
    fprintf(stderr, "  -d  \tdecrypt password\n");
    exit(1);
}

void showtrailer(void)
{
    if (!xref)
	die(fz_throw("no file specified"));
    printf("trailer\n");
    fz_debugobj(xref->trailer);
    printf("\n");
}

void showxref(void)
{
    if (!xref)
	die(fz_throw("no file specified"));
    pdf_debugxref(xref);
    printf("\n");
}

void showsafe(unsigned char *buf, int n)
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

void showstream(int num, int gen)
{
    fz_error error;
    fz_stream *stm;
    unsigned char buf[2048];
    int n;

    showcolumn = 0;

    if (showdecode)
	error = pdf_openstream(&stm, xref, num, gen);
    else
	error = pdf_openrawstream(&stm, xref, num, gen);
    if (error)
	die(error);

    while (1)
    {
	error = fz_read(&n, stm, buf, sizeof buf);
	if (error)
	    die(error);
	if (n == 0)
	    break;
	if (showbinary)
	    fwrite(buf, 1, n, stdout);
	else
	    showsafe(buf, n);
    }

    fz_dropstream(stm);
}

void showobject(int num, int gen)
{
    fz_error error;
    fz_obj *obj;

    if (!xref)
	die(fz_throw("no file specified"));

    error = pdf_loadobject(&obj, xref, num, gen);
    if (error)
	die(error);

    printf("%d %d obj\n", num, gen);
    fz_debugobj(obj);

    if (pdf_isstream(xref, num, gen))
    {
	printf("stream\n");
	showstream(num, gen);
	printf("endstream\n");
    }

    printf("endobj\n\n");

    fz_dropobj(obj);
}

int main(int argc, char **argv)
{
    char *password = "";
    int c;

    while ((c = getopt(argc, argv, "d:bx")) != -1)
    {
	switch (c)
	{
	    case 'd': password = optarg; break;
	    case 'b': showbinary ++; break;
	    case 'x': showdecode ++; break;
	    default:
		      showusage();
		      break;
	}
    }

    if (optind == argc)
	showusage();

    openxref(argv[optind++], password);

    if (optind == argc)
	showtrailer();

    while (optind < argc)
    {
	if (!strcmp(argv[optind], "trailer"))
	    showtrailer();
	else if (!strcmp(argv[optind], "xref"))
	    showxref();
	else
	    showobject(atoi(argv[optind]), 0);
	optind++;
    }
}

