#include "fitz.h"
#include "muxps.h"

/* SumatraPDF: add support for GDI+ draw device */
#ifdef _WIN32
#include <windows.h>
#define GDI_PLUS_BMP_RENDERER
#else
#include <sys/time.h>
#endif

char *output = NULL;
float resolution = 72;

int showxml = 0;
int showtext = 0;
int showtime = 0;
int showmd5 = 0;
int showoutline = 0;
int savealpha = 0;
int uselist = 1;

fz_colorspace *colorspace;
char *filename;
fz_context *ctx;

struct {
	int count, total;
	int min, max;
	int minpage, maxpage;
} timing;

static void usage(void)
{
	fprintf(stderr,
		"usage: xpsdraw [options] input.xps [pages]\n"
		"\t-o -\toutput filename (%%d for page number)\n"
#ifdef GDI_PLUS_BMP_RENDERER
		"\t\tsupported formats: pgm, ppm, pam, png, bmp\n"
#else
		"\t\tsupported formats: pgm, ppm, pam, png\n"
#endif
		"\t-r -\tresolution in dpi (default: 72)\n"
		"\t-a\tsave alpha channel (only pam and png)\n"
		"\t-g\trender in grayscale\n"
		"\t-m\tshow timing information\n"
		"\t-t\tshow text (-tt for xml)\n"
		"\t-x\tshow display list\n"
		"\t-d\tdisable use of display list\n"
		"\t-5\tshow md5 checksums\n"
		"\t-l\tprint outline\n"
		"\tpages\tcomma separated list of ranges\n");
	exit(1);
}

static int gettime(void)
{
	static struct timeval first;
	static int once = 1;
	struct timeval now;
	if (once)
	{
		gettimeofday(&first, NULL);
		once = 0;
	}
	gettimeofday(&now, NULL);
	return (now.tv_sec - first.tv_sec) * 1000 + (now.tv_usec - first.tv_usec) / 1000;
}

static int isrange(char *s)
{
	while (*s)
	{
		if ((*s < '0' || *s > '9') && *s != '-' && *s != ',')
			return 0;
		s++;
	}
	return 1;
}

#ifdef GDI_PLUS_BMP_RENDERER
static void drawbmp(xps_document *doc, xps_page *page, fz_display_list *list, int pagenum)
{
	float zoom;
	fz_matrix ctm;
	fz_bbox bbox;
	fz_rect rect;

	int w, h;
	fz_device *dev;
	HDC hDC, hDCMain;
	RECT rc;
	HBRUSH bgBrush;
	HBITMAP hbmp;
	BITMAPINFO bmi = { 0 };
	int bmpDataLen;
	char *bmpData;

	rect = xps_bound_page(doc, page);
	zoom = resolution / 72;
	ctm = fz_scale(zoom, zoom);
	bbox = fz_round_rect(fz_transform_rect(ctm, rect));

	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	hDCMain = GetDC(NULL);
	hDC = CreateCompatibleDC(hDCMain);
	hbmp = CreateCompatibleBitmap(hDCMain, w, h);
	DeleteObject(SelectObject(hDC, hbmp));

	SetRect(&rc, 0, 0, w, h);
	bgBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
	FillRect(hDC, &rc, bgBrush);
	DeleteObject(bgBrush);

	dev = fz_new_gdiplus_device(doc->ctx, hDC, bbox);
	if (list)
		fz_run_display_list(list, dev, ctm, bbox, NULL);
	else
		xps_run_page(doc, page, dev, ctm, NULL);
	fz_free_device(dev);

	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biHeight = h;
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	bmpDataLen = ((w * 3 + 3) / 4) * 4 * h;
	bmpData = fz_malloc(doc->ctx, bmpDataLen);
	if (!GetDIBits(hDC, hbmp, 0, h, bmpData, &bmi, DIB_RGB_COLORS))
		fz_throw(doc->ctx, "gdierror: cannot draw page %d in PDF file '%s'", pagenum, filename);

	DeleteDC(hDC);
	ReleaseDC(NULL, hDCMain);
	DeleteObject(hbmp);

	if (output)
	{
		char buf[512];
		int fd;
		BITMAPFILEHEADER bmpfh = { 0 };

		sprintf(buf, output, pagenum);
		fd = open(buf, O_BINARY|O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if (fd < 0)
			fz_throw(doc->ctx, "ioerror: could not create raster file '%s'", buf);

		bmpfh.bfType = MAKEWORD('B', 'M');
		bmpfh.bfOffBits = sizeof(bmpfh) + sizeof(bmi);
		bmpfh.bfSize = bmpfh.bfOffBits + bmpDataLen;

		write(fd, &bmpfh, sizeof(bmpfh));
		write(fd, &bmi, sizeof(bmi));
		write(fd, bmpData, bmpDataLen);

		close(fd);
	}

	if (showmd5)
	{
		fz_md5 md5;
		unsigned char digest[16];
		int i;

		fz_md5_init(&md5);
		fz_md5_update(&md5, bmpData, bmpDataLen);
		fz_md5_final(&md5, digest);

		printf(" ");
		for (i = 0; i < 16; i++)
			printf("%02x", digest[i]);
	}

	fz_free(doc->ctx, bmpData);
}
#endif

static void drawpage(xps_document *doc, int pagenum)
{
	xps_page *page;
	fz_display_list *list;
	fz_device *dev;
	int start;

	if (showtime)
	{
		start = gettime();
	}

	page = xps_load_page(doc, pagenum - 1);

	list = NULL;

	if (uselist)
	{
		list = fz_new_display_list(doc->ctx);
		dev = fz_new_list_device(doc->ctx, list);
		xps_run_page(doc, page, dev, fz_identity, NULL);
		fz_free_device(dev);
	}

	if (showxml)
	{
		dev = fz_new_trace_device(doc->ctx);
		printf("<page number=\"%d\">\n", pagenum);
		if (list)
			fz_run_display_list(list, dev, fz_identity, fz_infinite_bbox, NULL);
		else
			xps_run_page(doc, page, dev, fz_identity, NULL);
		printf("</page>\n");
		fz_free_device(dev);
	}

	if (showtext)
	{
		fz_text_span *text = fz_new_text_span(doc->ctx);
		dev = fz_new_text_device(doc->ctx, text);
		if (list)
			fz_run_display_list(list, dev, fz_identity, fz_infinite_bbox, NULL);
		else
			xps_run_page(doc, page, dev, fz_identity, NULL);
		fz_free_device(dev);
		printf("[Page %d]\n", pagenum);
		if (showtext > 1)
			fz_debug_text_span_xml(text);
		else
			fz_debug_text_span(text);
		printf("\n");
		fz_free_text_span(doc->ctx, text);
	}

	if (showmd5 || showtime)
		printf("page %s %d", filename, pagenum);

#ifdef GDI_PLUS_BMP_RENDERER
	if (output && strstr(output, ".bmp"))
		drawbmp(doc, page, list, pagenum);
	else
#endif
	if (output || showmd5 || showtime)
	{
		float zoom;
		fz_matrix ctm;
		fz_rect rect;
		fz_bbox bbox;
		fz_pixmap *pix;

		rect = xps_bound_page(doc, page);
		zoom = resolution / 72;
		ctm = fz_scale(zoom, zoom);
		bbox = fz_round_rect(fz_transform_rect(ctm, rect));

		/* TODO: banded rendering and multi-page ppm */

		pix = fz_new_pixmap_with_rect(doc->ctx, colorspace, bbox);

		if (savealpha)
			fz_clear_pixmap(doc->ctx, pix);
		else
			fz_clear_pixmap_with_value(doc->ctx, pix, 255);

		dev = fz_new_draw_device(doc->ctx, pix);
		if (list)
			fz_run_display_list(list, dev, ctm, bbox, NULL);
		else
			xps_run_page(doc, page, dev, ctm, NULL);
		fz_free_device(dev);

		if (output)
		{
			char buf[512];
			sprintf(buf, output, pagenum);
			if (strstr(output, ".pgm") || strstr(output, ".ppm") || strstr(output, ".pnm"))
				fz_write_pnm(doc->ctx, pix, buf);
			else if (strstr(output, ".pam"))
				fz_write_pam(doc->ctx, pix, buf, savealpha);
			else if (strstr(output, ".png"))
				fz_write_png(doc->ctx, pix, buf, savealpha);
		}

		if (showmd5)
		{
			fz_md5 md5;
			unsigned char digest[16];
			int i;

			fz_md5_init(&md5);
			fz_md5_update(&md5, pix->samples, pix->w * pix->h * pix->n);
			fz_md5_final(&md5, digest);

			printf(" ");
			for (i = 0; i < 16; i++)
				printf("%02x", digest[i]);
		}

		fz_drop_pixmap(doc->ctx, pix);
	}

	if (list)
		fz_free_display_list(doc->ctx, list);

	if (showtime)
	{
		int end = gettime();
		int diff = end - start;

		if (diff < timing.min)
		{
			timing.min = diff;
			timing.minpage = pagenum;
		}
		if (diff > timing.max)
		{
			timing.max = diff;
			timing.maxpage = pagenum;
		}
		timing.total += diff;
		timing.count ++;

		printf(" %dms", diff);
	}

	if (showmd5 || showtime)
		printf("\n");
}

static void drawrange(xps_document *doc, char *range)
{
	int page, spage, epage;
	char *spec, *dash;

	spec = fz_strsep(&range, ",");
	while (spec)
	{
		dash = strchr(spec, '-');

		if (dash == spec)
			spage = epage = xps_count_pages(doc);
		else
			spage = epage = atoi(spec);

		if (dash)
		{
			if (strlen(dash) > 1)
				epage = atoi(dash + 1);
			else
				epage = xps_count_pages(doc);
		}

		spage = CLAMP(spage, 1, xps_count_pages(doc));
		epage = CLAMP(epage, 1, xps_count_pages(doc));

		if (spage < epage)
			for (page = spage; page <= epage; page++)
				drawpage(doc, page);
		else
			for (page = spage; page >= epage; page--)
				drawpage(doc, page);

		spec = fz_strsep(&range, ",");
	}
}

static void drawoutline(xps_document *doc)
{
	fz_outline *outline = xps_load_outline(doc);
	if (showoutline > 1)
		fz_debug_outline_xml(doc->ctx, outline, 0);
	else
		fz_debug_outline(doc->ctx, outline, 0);
	fz_free_outline(doc->ctx, outline);
}

#ifdef MUPDF_COMBINED_EXE
int xpsdraw_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	int grayscale = 0;
	xps_document *doc = NULL;
	int c;

	fz_var(doc);

	while ((c = fz_getopt(argc, argv, "o:p:r:adglmtx5")) != -1)
	{
		switch (c)
		{
		case 'o': output = fz_optarg; break;
		case 'r': resolution = atof(fz_optarg); break;
		case 'a': savealpha = 1; break;
		case 'l': showoutline++; break;
		case 'm': showtime++; break;
		case 't': showtext++; break;
		case 'x': showxml++; break;
		case '5': showmd5++; break;
		case 'g': grayscale++; break;
		case 'd': uselist = 0; break;
		default: usage(); break;
		}
	}

	if (fz_optind == argc)
		usage();

	if (!showtext && !showxml && !showtime && !showmd5 && !showoutline && !output)
	{
		printf("nothing to do\n");
		exit(0);
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	colorspace = fz_device_rgb;
	if (grayscale)
		colorspace = fz_device_gray;
	if (output && strstr(output, ".pgm"))
		colorspace = fz_device_gray;
	if (output && strstr(output, ".ppm"))
		colorspace = fz_device_rgb;

	timing.count = 0;
	timing.total = 0;
	timing.min = 1 << 30;
	timing.max = 0;
	timing.minpage = 0;
	timing.maxpage = 0;

	if (showxml)
		printf("<?xml version=\"1.0\"?>\n");

	while (fz_optind < argc)
	{
		filename = argv[fz_optind++];

		fz_try(ctx)
		{
			doc = xps_open_document(ctx, filename);

			if (showxml)
				printf("<document name=\"%s\">\n", filename);

			if (showoutline)
				drawoutline(doc);

			if (showtext || showxml || showtime || showmd5 || output)
			{
				if (fz_optind == argc || !isrange(argv[fz_optind]))
					drawrange(doc, "1-");
				if (fz_optind < argc && isrange(argv[fz_optind]))
					drawrange(doc, argv[fz_optind++]);
			}

			if (showxml)
				printf("</document>\n");

			xps_close_document(doc);
		}
		fz_catch(ctx)
		{
			xps_close_document(doc);
		}
	}

	if (showtime)
	{
		printf("total %dms / %d pages for an average of %dms\n",
			timing.total, timing.count, timing.total / timing.count);
		printf("fastest page %d: %dms\n", timing.minpage, timing.min);
		printf("slowest page %d: %dms\n", timing.maxpage, timing.max);
	}

	fz_free_context(ctx);

	return 0;
}
