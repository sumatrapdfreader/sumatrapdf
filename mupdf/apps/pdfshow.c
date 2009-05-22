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

void closexref()
{
    pdf_closexref(xref);
    xref = nil;
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

int isimage(fz_obj *obj)
{
    fz_obj *type = fz_dictgets(obj, "Subtype");
    return fz_isname(type) && !strcmp(fz_toname(type), "Image");
}

void showimage(fz_obj *img)
{
    fz_error error;
    fz_obj *obj;
    int mask = 0;
    char *cs = "";
    int bpc;
    int w;
    int h;
    int n;

    obj = fz_dictgets(img, "BitsPerComponent");
    if (!obj)
	die(fz_throw("No bits per component"));
    error = pdf_resolve(&obj, xref);
    if (error)
	die(error);
    bpc = fz_toint(obj);

    obj = fz_dictgets(img, "Width");
    if (!obj)
	die(fz_throw("No width"));
    error = pdf_resolve(&obj, xref);
    if (error)
	die(error);
    w = fz_toint(obj);

    obj = fz_dictgets(img, "Height");
    if (!obj)
	die(fz_throw("No height"));
    error = pdf_resolve(&obj, xref);
    if (error)
	die(error);
    h = fz_toint(obj);

    obj = fz_dictgets(img, "ImageMask");
    if (obj)
    {
	error = pdf_resolve(&obj, xref);
	if (error)
	    die(error);
	mask = fz_tobool(obj);
    }

    obj = fz_dictgets(img, "ColorSpace");
    if (!mask && !obj)
	die(fz_throw("No colorspace"));
    if (obj)
    {
	error = pdf_resolve(&obj, xref);
	if (error)
	    die(error);
	if (fz_isname(obj))
	{
	    cs = fz_toname(obj);

	    if (!strcmp(cs, "DeviceGray"))
		n = 1;
	    else if (!strcmp(cs, "DeviceRGB"))
		n = 3;
	}
	else if (fz_isarray(obj))
	{
	    fz_obj *csarray = obj;

	    obj = fz_arrayget(csarray, 0);
	    if (!obj)
		die(fz_throw("No colorspace"));
	    error = pdf_resolve(&obj, xref);
	    if (error)
		die(error);
	    if (!fz_isname(obj))
		die(fz_throw("Not a colorspace name"));
	    cs = fz_toname(obj);

	    if (!strcmp(cs, "ICCBased"))
	    {
		obj = fz_arrayget(csarray, 1);
		if (!obj)
		    die(fz_throw("No colorspace dict"));
		error = pdf_resolve(&obj, xref);
		if (error)
		    die(error);
		if (!fz_isdict(obj))
		    die(fz_throw("Not a colorspace dict"));

		obj = fz_dictgets(obj, "N");
		if (!obj)
		    die(fz_throw("No number of components"));
		error = pdf_resolve(&obj, xref);
		if (error)
		    die(error);
		if (!fz_isint(obj))
		    die(fz_throw("Not a number of components"));
		n = fz_toint(obj);
	    }
	    else if (!strcmp(cs, "CalGray"))
	    {
		n = 1;
	    }
	    else if (!strcmp(cs, "CalRGB"))
	    {
		n = 3;
	    }
	}
    }

    if (!mask)
    {
	if (n == 1 && bpc == 1)
	    printf("P4\n%d %d\n", w, h);
	else if (n == 1 && bpc == 8)
	    printf("P5\n%d %d\n%d\n", w, h, (1 << bpc) - 1);
	else if (n == 3)
	    printf("P6\n%d %d\n%d\n", w, h, (1 << bpc) - 1);
    }
    else
    {
	if (bpc == 1)
	    printf("P4\n%d %d\n", w, h);
	else if (bpc == 8)
	    printf("P5\n%d %d\n%d\n", w, h, (1 << bpc) - 1);
    }
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

    if (showbinary && showdecode && isimage(obj))
    {
	showimage(obj);
	showstream(num, gen);
    }
    else if (pdf_isstream(xref, num, gen))
    {
	printf("%d %d obj\n", num, gen);
	fz_debugobj(obj);
	printf("stream\n");
	showstream(num, gen);
	printf("endstream\n");
	printf("endobj\n\n");
    }
    else
    {
	printf("%d %d obj\n", num, gen);
	fz_debugobj(obj);
	printf("endobj\n\n");
    }

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

    closexref();
}

