/*
 * pdfextract -- the ultimate way to extract images and fonts from pdfs
 */

#include "pdftool.h"

static void showusage(void)
{
	fprintf(stderr, "usage: pdfextract [-p password] <file> [object numbers]\n");
	fprintf(stderr, "\t-p\tpassword\n");
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

static void saveimage(int num, int gen)
{
	pdf_image *img = nil;
	fz_obj *ref;
	fz_error error;
	fz_pixmap *pix;
	char name[1024];
	FILE *f;
	int x, y;
	unsigned char *samples;

	ref = fz_newindirect(num, gen, xref);

	xref->store = pdf_newstore();

	error = pdf_loadimage(&img, xref, nil, ref);
	if (error)
		die(error);

	pix = pdf_loadtile(img);

	if (img->bpc == 1 && img->n == 0)
	{
		fz_pixmap *temp;

		temp = fz_newpixmap(pdf_devicergb, pix->x, pix->y, pix->w, pix->h);

		for (y = 0; y < pix->h; y++)
			for (x = 0; x < pix->w; x++)
			{
				int pixel = y * pix->w + x;
				temp->samples[pixel * temp->n + 0] = pix->samples[pixel];
				temp->samples[pixel * temp->n + 1] = pix->samples[pixel];
				temp->samples[pixel * temp->n + 2] = pix->samples[pixel];
				temp->samples[pixel * temp->n + 3] = 255;
			}

		fz_droppixmap(pix);
		pix = temp;
	}

	if (img->colorspace && strcmp(img->colorspace->name, "DeviceRGB"))
	{
		fz_pixmap *temp;
		temp = fz_newpixmap(pdf_devicergb, pix->x, pix->y, pix->w, pix->h);
		fz_convertpixmap(pix, temp);
		fz_droppixmap(pix);
		pix = temp;
	}

	sprintf(name, "img-%04d.pnm", num);

	f = fopen(name, "wb");
	if (f == NULL)
		die(fz_throw("Error creating image file"));

	fprintf(f, "P6\n%d %d\n%d\n", img->w, img->h, 255);

	samples = pix->samples;

	for (y = 0; y < pix->h; y++)
		for (x = 0; x < pix->w; x++)
		{
			unsigned char r, g, b;

			r = *(samples++);
			g = *(samples++);
			b = *(samples++);
			samples++;

			fprintf(f, "%c%c%c", r, g, b);
		}

	if (fclose(f) < 0)
		die(fz_throw("Error closing image file"));

	fz_droppixmap(pix);

	pdf_freestore(xref->store);
	xref->store = nil;

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
	unsigned char *p;
	char *fontname = "font";

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

	f = fopen(name, "wb");
	if (f == NULL)
		die(fz_throw("Error creating image file"));

	for (p = buf->rp; p < buf->wp; p ++)
		fprintf(f, "%c", *p);

	if (fclose(f) < 0)
		die(fz_throw("Error closing image file"));

	fz_dropbuffer(buf);
}

static void showobject(int num, int gen)
{
	fz_error error;
	fz_obj *obj;

	if (!xref)
		die(fz_throw("no file specified"));

	error = pdf_loadobject(&obj, xref, num, gen);
	if (error)
		die(error);

	if (isimage(obj))
		saveimage(num, gen);
	else if (isfontdesc(obj))
		savefont(obj, num);

	fz_dropobj(obj);
}

int main(int argc, char **argv)
{
	char *password = "";
	int c, o;

	while ((c = fz_getopt(argc, argv, "p:")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		default:
			showusage();
			break;
		}
	}

	if (fz_optind == argc)
		showusage();

	openxref(argv[fz_optind++], password, 0, 0);

	if (fz_optind == argc)
		for (o = 0; o < xref->len; o++)
			showobject(o, 0);
	else
		while (fz_optind < argc)
	{
		showobject(atoi(argv[fz_optind]), 0);
		fz_optind++;
	}

	closexref();

	return 0;
}

