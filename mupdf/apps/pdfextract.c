/*
 * pdfextract -- the ultimate way to extract images and fonts from pdfs
 */

#include "fitz.h"
#include "mupdf.h"

static pdf_xref *xref = NULL;
static int dorgb = 0;

void die(fz_error error)
{
	fz_catch(error, "aborting");
	if (xref)
		pdf_freexref(xref);
	exit(1);
}

static void usage(void)
{
	fprintf(stderr, "usage: pdfextract [options] file.pdf [object numbers]\n");
	fprintf(stderr, "\t-p\tpassword\n");
	fprintf(stderr, "\t-r\tconvert images to rgb\n");
	exit(1);
}

static int isimage(fz_obj *obj)
{
	fz_obj *type = fz_dictgets(obj, "Subtype");
	return fz_isname(type) && !strcmp(fz_toname(type), "Image");
}

static int isfontdesc(fz_obj *obj)
{
	fz_obj *type = fz_dictgets(obj, "Type");
	return fz_isname(type) && !strcmp(fz_toname(type), "FontDescriptor");
}

static void saveimage(int num)
{
	fz_error error;
	pdf_image *img;
	fz_pixmap *pix;
	fz_obj *ref;
	char name[1024];

	ref = fz_newindirect(num, 0, xref);

	/* TODO: detect DCTD and save as jpeg */

	error = pdf_loadimage(&img, xref, nil, ref);
	if (error)
		die(error);

	pix = pdf_loadtile(img);

	if (dorgb && img->colorspace && img->colorspace != fz_devicergb)
	{
		fz_pixmap *temp;
		temp = fz_newpixmap(fz_devicergb, pix->x, pix->y, pix->w, pix->h);
		fz_convertpixmap(pix, temp);
		fz_droppixmap(pix);
		pix = temp;
	}

	if (pix->n <= 4)
	{
		sprintf(name, "img-%04d.png", num);
		printf("extracting image %s\n", name);
		fz_writepng(pix, name, 0);
	}
	else
	{
		sprintf(name, "img-%04d.pam", num);
		printf("extracting image %s\n", name);
		fz_writepam(pix, name, 0);
	}

	fz_droppixmap(pix);
	pdf_dropimage(img);

	fz_dropobj(ref);
}

static void savefont(fz_obj *dict, int num)
{
	fz_error error;
	char name[1024];
	char *subtype;
	fz_buffer *buf;
	fz_obj *stream = nil;
	fz_obj *obj;
	char *ext = "";
	FILE *f;
	char *fontname = "font";
	int n;

	obj = fz_dictgets(dict, "FontName");
	if (obj)
		fontname = fz_toname(obj);

	obj = fz_dictgets(dict, "FontFile");
	if (obj)
	{
		stream = obj;
		ext = "pfa";
	}

	obj = fz_dictgets(dict, "FontFile2");
	if (obj)
	{
		stream = obj;
		ext = "ttf";
	}

	obj = fz_dictgets(dict, "FontFile3");
	if (obj)
	{
		stream = obj;

		obj = fz_dictgets(obj, "Subtype");
		if (obj && !fz_isname(obj))
			die(fz_throw("Invalid font descriptor subtype"));

		subtype = fz_toname(obj);
		if (!strcmp(subtype, "Type1C"))
			ext = "cff";
		else if (!strcmp(subtype, "CIDFontType0C"))
			ext = "cid";
		else
			die(fz_throw("Unhandled font type '%s'", subtype));
	}

	if (!stream)
	{
		fz_warn("Unhandled font type");
		return;
	}

	buf = fz_newbuffer(0);

	error = pdf_loadstream(&buf, xref, fz_tonum(stream), fz_togen(stream));
	if (error)
		die(error);

	sprintf(name, "%s-%04d.%s", fontname, num, ext);
	printf("extracting font %s\n", name);

	f = fopen(name, "wb");
	if (f == NULL)
		die(fz_throw("Error creating font file"));

	n = fwrite(buf, 1, buf->len, f);
	if (n < buf->len)
		die(fz_throw("Error writing font file"));

	if (fclose(f) < 0)
		die(fz_throw("Error closing font file"));

	fz_dropbuffer(buf);
}

static void showobject(int num)
{
	fz_error error;
	fz_obj *obj;

	if (!xref)
		die(fz_throw("no file specified"));

	error = pdf_loadobject(&obj, xref, num, 0);
	if (error)
		die(error);

	if (isimage(obj))
		saveimage(num);
	else if (isfontdesc(obj))
		savefont(obj, num);

	fz_dropobj(obj);
}

int main(int argc, char **argv)
{
	fz_error error;
	char *infile;
	char *password = "";
	int c, o;

	while ((c = fz_getopt(argc, argv, "p:r")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'r': dorgb++; break;
		default: usage(); break;
		}
	}

	if (fz_optind == argc)
		usage();

	infile = argv[fz_optind++];
	error = pdf_openxref(&xref, infile, password);
	if (error)
		die(fz_rethrow(error, "cannot open input file '%s'", infile));

	if (fz_optind == argc)
	{
		for (o = 0; o < xref->len; o++)
			showobject(o);
	}
	else
	{
		while (fz_optind < argc)
		{
			showobject(atoi(argv[fz_optind]));
			fz_optind++;
		}
	}

	pdf_freexref(xref);

	return 0;
}
