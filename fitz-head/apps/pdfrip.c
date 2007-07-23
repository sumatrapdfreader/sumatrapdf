#include <fitz.h>
#include <mupdf.h>

int showtree = 0;
float zoom = 1.0;
char *namefmt = nil;
fz_renderer *gc;
int nbands = 1;
int verbose = 0;
int textonly = 0;

void usage()
{
	fprintf(stderr,
"usage: pdfrip [options] file.pdf pageranges\n"
"  -b -\trender page in N bands (default 1)\n"
"  -d -\tpassword for decryption\n"
"  -g  \tshow display tree -- debug\n"
"  -o -\toutput filename format (default out-%%03d.ppm)\n"
"  -t  \tprint text on stdout instead of rendering\n"
"  -v  \tverbose\n"
"  -z -\tzoom factor (default 1.0 = 72 dpi)\n"
	);
	exit(1);
}

/*
 * Draw page
 */

void showpage(pdf_xref *xref, fz_obj *pageobj, int pagenum)
{
	fz_error *error;
	pdf_page *page;
	char namebuf[256];
	char buf[128];
	fz_pixmap *pix;
	fz_matrix ctm;
	fz_irect bbox;
	int fd;
	int x, y;
	int w, h;
	int b, bh;

	if (verbose)
		printf("page %d\n", pagenum);

	sprintf(namebuf, namefmt, pagenum);

	error = pdf_loadpage(&page, xref, pageobj);
	if (error)
		fz_abort(error);

	if (showtree)
	{
		fz_debugobj(pageobj);
		printf("\n");

		printf("page\n");
		printf("  mediabox [ %g %g %g %g ]\n",
			page->mediabox.x0, page->mediabox.y0,
			page->mediabox.x1, page->mediabox.y1);
		printf("  rotate %d\n", page->rotate);

		printf("  resources\n");
		fz_debugobj(page->resources);
		printf("\n");

		printf("tree\n");
		fz_debugtree(page->tree);
		printf("endtree\n");
	}

	ctm = fz_concat(fz_translate(0, -page->mediabox.y1),
					fz_scale(zoom, -zoom));

	if (textonly)
	{
		pdf_textline *line;

		error = pdf_loadtextfromtree(&line, page->tree, ctm);
		if (error)
			fz_abort(error);
		pdf_debugtextline(line);
		pdf_droptextline(line);
		pdf_droppage(page);

		printf("\n\f\n");

		return;
	}

	bbox = fz_roundrect(page->mediabox);
	bbox.x0 = bbox.x0 * zoom;
	bbox.y0 = bbox.y0 * zoom;
	bbox.x1 = bbox.x1 * zoom;
	bbox.y1 = bbox.y1 * zoom;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;
	bh = h / nbands;

	fd = open(namebuf, O_BINARY|O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd < 0)
		fz_abort(fz_throw("open %s failed: %s", namebuf, strerror(errno)));
	sprintf(buf, "P6\n%d %d\n255\n", w, bh * nbands);
	write(fd, buf, strlen(buf));

	error = fz_newpixmap(&pix, bbox.x0, bbox.y0, w, bh, 4);
	if (error)
		fz_abort(error);

	for (b = 0; b < nbands; b++)
	{
		if (verbose)
			printf("render band %d of %d\n", b + 1, nbands);

		memset(pix->samples, 0xff, pix->w * pix->h * 4);

		error = fz_rendertreeover(gc, pix, page->tree, ctm);
		if (error)
			fz_abort(error);

		for (y = 0; y < pix->h; y++)
		{
			unsigned char *src = pix->samples + y * pix->w * 4;
			unsigned char *dst = src;

			for (x = 0; x < pix->w; x++)
			{
				dst[x * 3 + 0] = src[x * 4 + 1];
				dst[x * 3 + 1] = src[x * 4 + 2];
				dst[x * 3 + 2] = src[x * 4 + 3];
			}

			write(fd, dst, pix->w * 3);
		}

		pix->y += bh;
	}

	fz_droppixmap(pix);

	close(fd);

	pdf_droppage(page);
}

int main(int argc, char **argv)
{
	fz_error *error;
	char *filename;
	pdf_xref *xref;
	pdf_pagetree *pages;
	int c;

	char *password = "";

	fz_cpudetect();
	fz_accelerate();

	while ((c = getopt(argc, argv, "Vgtvz:d:o:b:")) != -1)
	{
		switch (c)
		{
		case 'g': ++showtree; break;
		case 't': ++textonly; break;
		case 'v': ++verbose; break;
		case 'd': password = optarg; break;
		case 'z': zoom = atof(optarg); break;
		case 'o': namefmt = optarg; break;
		case 'b': nbands = atoi(optarg); break;
		default: usage();
		}
	}

	if (argc - optind == 0)
		usage();

	filename = argv[optind++];

	if (!namefmt)
	{
#if 1
		namefmt = "out-%03d.ppm";
#else
		char namebuf[256];
		char *s;
		s = strrchr(filename, '/');
		if (!s)
			s = filename;
		else
			s ++;
		strcpy(namebuf, s);
		s = strrchr(namebuf, '.');
		if (s)
			strcpy(s, "-%03d.ppm");
		else
			strcat(namebuf, "-%03d.ppm");
		namefmt = namebuf;
#endif
	}

	if (verbose)
		printf("loading pdf: '%s'\n", filename);

	error = pdf_newxref(&xref);
	if (error)
		fz_abort(error);

	error = pdf_loadxref(xref, filename);
	if (error)
		fz_abort(error);

	error = pdf_decryptxref(xref);
	if (error)
		fz_abort(error);

	if (xref->crypt)
	{
		error = pdf_setpassword(xref->crypt, password);
		if (error)
			fz_abort(error);
	}

	error = pdf_loadpagetree(&pages, xref);
	if (error)
		fz_abort(error);

	if (verbose)
		pdf_debugpagetree(pages);

	if (optind == argc)
	{
		printf("%d pages\n", pdf_getpagecount(pages));
		return 0;
	}

	if (textonly)
	{
		puts("Content-Type: text/plain; charset=UTF-8");
		puts("");
	}

	error = fz_newrenderer(&gc, pdf_devicergb, 0, 1024 * 512);
	if (error)
		fz_abort(error);

	for ( ; optind < argc; optind++)
	{
		int spage, epage, page;
		char *spec = argv[optind];
		char *dash = strchr(spec, '-');

		if (dash == spec)
			spage = epage = 1;
		else
			spage = epage = atoi(spec);

		if (dash)
		{
			if (strlen(dash) > 1)
				epage = atoi(dash+1);
			else
				epage = pdf_getpagecount(pages);
		}

		if (spage > epage)
			page = spage, spage = epage, epage = page;

		for (page = spage; page <= epage; page++)
		{
			if (page < 1 || page > pdf_getpagecount(pages))
				fprintf(stderr, "page out of bounds: %d\n", page);
			else
			{
				showpage(xref, pdf_getpageobject(pages, page - 1), page);
			}
		}
	}

	fz_droprenderer(gc);

	pdf_dropstore(xref->store);
	xref->store = nil;

	pdf_closexref(xref);

	return 0;
}

