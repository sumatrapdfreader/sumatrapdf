/*
 * pdfextract -- the ultimate way to extract images and fonts from pdfs
 */

#include "pdftool.h"

static void showusage(void)
{
    fprintf(stderr, "usage: pdfextract [-d password] <file> [object numbers]\n");
    fprintf(stderr, "  -d  \tdecrypt password\n");
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

static void saveimage(fz_obj *obj, int num, int gen)
{
    pdf_image *img = nil;
    fz_obj *ref;
    fz_error error;
    fz_pixmap *pix;
    char name[1024];
    FILE *f;
    int bpc;
    int w;
    int h;
    int n;
    int x;
    int y;

    error = fz_newindirect(&ref, num, gen, xref);
    if (error)
        die(error);

    error = pdf_newstore(&xref->store);
    if (error)
        die(error);

    error = pdf_loadimage(&img, xref, obj);
    if (error)
        die(error);

    n = img->super.n;
    w = img->super.w;
    h = img->super.h;
    bpc = img->bpc;

    error = fz_newpixmap(&pix, 0, 0, w, h, n + 1);
    if (error)
        die(error);

    error = img->super.loadtile(&img->super, pix);
    if (error)
        die(error);

    if (bpc == 1 && n == 0)
    {
        fz_pixmap *temp;

        error = fz_newpixmap(&temp, pix->x, pix->y, pix->w, pix->h, pdf_devicergb->n + 1);
        if (error)
            die(error);

        for (y = 0; y < pix->h; y++)
            for (x = 0; x < pix->w; x++)
            {
                int pixel = y * pix->w + x;
                temp->samples[pixel * temp->n + 0] = 255;
                temp->samples[pixel * temp->n + 1] = pix->samples[pixel];
                temp->samples[pixel * temp->n + 2] = pix->samples[pixel];
                temp->samples[pixel * temp->n + 3] = pix->samples[pixel];
            }

        fz_droppixmap(pix);
        pix = temp;
    }

    if (img->super.cs && strcmp(img->super.cs->name, "DeviceRGB"))
    {
        fz_pixmap *temp;

        error = fz_newpixmap(&temp, pix->x, pix->y, pix->w, pix->h, pdf_devicergb->n + 1);
        if (error)
            die(error);

        fz_convertpixmap(img->super.cs, pix, pdf_devicergb, temp);
        fz_droppixmap(pix);
        pix = temp;
    }

    sprintf(name, "img-%04d.pnm", num);

    f = fopen(name, "wb");
    if (f == NULL)
        die(fz_throw("Error creating image file"));

    fprintf(f, "P6\n%d %d\n%d\n", w, h, 255);

    for (y = 0; y < pix->h; y++)
        for (x = 0; x < pix->w; x++)
        {
            fz_sample *sample = &pix->samples[(y * pix->w + x) * (pdf_devicergb->n + 1)];
            unsigned char r = sample[1];
            unsigned char g = sample[2];
            unsigned char b = sample[3];
            fprintf(f, "%c%c%c", r, g, b);
        }

    if (fclose(f) < 0)
        die(fz_throw("Error closing image file"));

    fz_droppixmap(pix);

    pdf_dropstore(xref->store);
    xref->store = nil;

    fz_dropimage(&img->super);

    fz_dropobj(ref);
}

static void savefont(fz_obj *dict, int num, int gen)
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

    error = fz_newbuffer(&buf, 0);
    if (error)
        die(error);

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
        saveimage(obj, num, gen);
    else if (isfontdesc(obj))
        savefont(obj, num, gen);

    fz_dropobj(obj);
}

int main(int argc, char **argv)
{
    char *password = "";
    int c, o;

    while ((c = fz_getopt(argc, argv, "d:")) != -1)
    {
	switch (c)
	{
	    case 'd': password = fz_optarg; break;
	    default:
		      showusage();
		      break;
	}
    }

    if (fz_optind == argc)
	showusage();

    openxref(argv[fz_optind++], password, 0);

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
}

