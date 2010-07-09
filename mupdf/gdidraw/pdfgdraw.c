/*
 * pdfgdraw:
 *   Draw pages to BMP bitmaps.
 *   Benchmark rendering speed.
 */

#include <windows.h>
#include <pdftool.h>
#include <fitz_gdidraw.h>

struct benchmark
{
	int pages;
	long min;
	int minpage;
	long avg;
	long max;
	int maxpage;
};

static char *drawpattern = nil;
static pdf_page *drawpage = nil;
static float drawzoom = 1;
static int drawrotate = 0;
static int drawcount = 0;
static int benchmark = 0;
static int checksum = 0;

static void local_cleanup(void)
{
	if (xref && xref->store)
	{
		pdf_freestore(xref->store);
		xref->store = nil;
	}
}

static void drawusage(void)
{
	fprintf(stderr,
		"usage: pdfgdraw [options] [file.pdf pages ... ]\n"
		"  -p -\tpassword for decryption\n"
		"  -o -\tpattern (%%d for page number) for output file\n"
		"  -r -\tresolution in dpi\n"
		"  -m\tprint benchmark results\n"
		"  -s\tprint MD5 checksum of page pixel data\n"
		"  example:\n"
		"    pdfgdraw -o output%%03d.bmp input.pdf 1-3,5,9-\n");
	exit(1);
}

static void gettime(long *time_)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0)
		abort();

	*time_ = tv.tv_sec * 1000000 + tv.tv_usec;
}

static void drawloadpage(int pagenum, struct benchmark *loadtimes)
{
	fz_error error;
	fz_obj *pageobj;
	long start;
	long end;
	long elapsed;

	fprintf(stdout, "draw %s:%03d ", basename, pagenum);
	if (benchmark && loadtimes)
	{
		fflush(stdout);
		gettime(&start);
	}

	pageobj = pdf_getpageobject(xref, pagenum);
	error = pdf_loadpage(&drawpage, xref, pageobj);
	if (error)
		die(fz_rethrow(error, "cannot load page %d (%d %d R) in PDF file '%s'", pagenum, fz_tonum(pageobj), fz_togen(pageobj), basename));

	if (benchmark && loadtimes)
	{
		gettime(&end);
		elapsed = end - start;

		if (elapsed < loadtimes->min)
		{
			loadtimes->min = elapsed;
			loadtimes->minpage = pagenum;
		}
		if (elapsed > loadtimes->max)
		{
			loadtimes->max = elapsed;
			loadtimes->maxpage = pagenum;
		}
		loadtimes->avg += elapsed;
		loadtimes->pages++;
	}

	if (benchmark)
		fflush(stdout);
}

static void drawfreepage(void)
{
	pdf_freepage(drawpage);
	drawpage = nil;

	flushxref();

	/* Flush resources between pages.
	 * TODO: should check memory usage before deciding to do this.
	 */
	if (xref && xref->store)
	{
		/* pdf_debugstore(xref->store); */
		pdf_agestoreditems(xref->store);
		pdf_evictageditems(xref->store);
		fflush(stdout);
	}
}

static void drawbmp(int pagenum, struct benchmark *loadtimes, struct benchmark *drawtimes)
{
	static int fd = -1;
	fz_error error;
	fz_matrix ctm;
	fz_bbox bbox;
	char name[256];
	int i, w, h;
	long start;
	long end;
	long elapsed;
	fz_md5 digest;
	fz_device *dev;
	HDC hDC, hDCMain;
	RECT rc;
	HBRUSH bgBrush;
	HBITMAP hbmp;
	BITMAPINFO bmi = { 0 };
	int bmpDataLen;
	char *bmpData;

	if (checksum)
		fz_md5init(&digest);

	drawloadpage(pagenum, loadtimes);

	if (benchmark)
		gettime(&start);

	ctm = fz_identity;
	ctm = fz_concat(ctm, fz_translate(-drawpage->mediabox.x0, -drawpage->mediabox.y1));
	ctm = fz_concat(ctm, fz_scale(drawzoom, -drawzoom));
	ctm = fz_concat(ctm, fz_rotate(drawrotate + drawpage->rotate));

	bbox = fz_roundrect(fz_transformrect(ctm, drawpage->mediabox));
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

	if (drawpattern)
	{
		if (strchr(drawpattern, '%') || fd < 0)
		{
			sprintf(name, drawpattern, drawcount++);
			fd = open(name, O_BINARY|O_WRONLY|O_CREAT|O_TRUNC, 0666);
			if (fd < 0)
				die(fz_throw("ioerror: could not create raster file '%s'", name));
		}
	}

	dev = fz_newgdidevice(hDC);

	error = pdf_runcontentstream(dev, ctm, xref, drawpage->resources, drawpage->contents);
	if (error)
		die(fz_rethrow(error, "cannot draw page %d in PDF file '%s'", pagenum, basename));

	fz_freedevice(dev);

	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biHeight = h;
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	bmpDataLen = ((w * 3 + 3) / 4) * 4 * h;
	bmpData = fz_malloc(bmpDataLen);
	if (!GetDIBits(hDC, hbmp, 0, h, bmpData, &bmi, DIB_RGB_COLORS))
		die(fz_rethrow(error, "gdierror: cannot draw page %d in PDF file '%s'", pagenum, basename));

	if (checksum)
		fz_md5update(&digest, bmpData, bmpDataLen);
	if (drawpattern)
	{
		BITMAPFILEHEADER bmpfh = { 0 };
		bmpfh.bfType = MAKEWORD('B', 'M');
		bmpfh.bfOffBits = sizeof(bmpfh) + sizeof(bmi);
		bmpfh.bfSize = bmpfh.bfOffBits + bmpDataLen;

		write(fd, &bmpfh, sizeof(bmpfh));
		write(fd, &bmi, sizeof(bmi));
		write(fd, bmpData, bmpDataLen);
	}

	fz_free(bmpData);
	DeleteDC(hDC);
	ReleaseDC(NULL, hDCMain);
	DeleteObject(hbmp);

	if (checksum)
	{
		unsigned char buf[16];
		fz_md5final(&digest, buf);
		for (i = 0; i < 16; i++)
			fprintf(stdout, "%02x", buf[i]);
		fprintf(stdout, " ");
	}

	if (drawpattern && strchr(drawpattern, '%'))
		close(fd);

	drawfreepage();

	if (benchmark)
	{
		gettime(&end);
		elapsed = end - start;

		if (elapsed < drawtimes->min)
		{
			drawtimes->min = elapsed;
			drawtimes->minpage = pagenum;
		}
		if (elapsed > drawtimes->max)
		{
			drawtimes->max = elapsed;
			drawtimes->maxpage = pagenum;
		}
		drawtimes->avg += elapsed;
		drawtimes->pages++;

		fprintf(stdout, "time %.3fs",
			elapsed / 1000000.0);
	}

	fprintf(stdout, "\n");
}

static void drawpages(char *pagelist)
{
	int page, spage, epage;
	char *spec, *dash;
	struct benchmark loadtimes, drawtimes;

	if (!xref)
		drawusage();

	if (benchmark)
	{
		memset(&loadtimes, 0x00, sizeof (loadtimes));
		loadtimes.min = LONG_MAX;
		memset(&drawtimes, 0x00, sizeof (drawtimes));
		drawtimes.min = LONG_MAX;
	}

	spec = fz_strsep(&pagelist, ",");
	while (spec)
	{
		dash = strchr(spec, '-');

		if (dash == spec)
			spage = epage = 1;
		else
			spage = epage = atoi(spec);

		if (dash)
		{
			if (strlen(dash) > 1)
				epage = atoi(dash + 1);
			else
				epage = pagecount;
		}

		if (spage > epage)
			page = spage, spage = epage, epage = page;

		if (spage < 1)
			spage = 1;
		if (epage > pagecount)
			epage = pagecount;

		for (page = spage; page <= epage; page++)
		{
			drawbmp(page, &loadtimes, &drawtimes);
		}

		spec = fz_strsep(&pagelist, ",");
	}

	if (benchmark)
	{
		if (loadtimes.pages > 0)
		{
			loadtimes.avg /= loadtimes.pages;
			drawtimes.avg /= drawtimes.pages;

			printf("benchmark-load: min: %6.3fs (page % 4d), avg: %6.3fs, max: %6.3fs (page % 4d)\n",
				loadtimes.min / 1000000.0, loadtimes.minpage,
				loadtimes.avg / 1000000.0,
				loadtimes.max / 1000000.0, loadtimes.maxpage);
			printf("benchmark-draw: min: %6.3fs (page % 4d), avg: %6.3fs, max: %6.3fs (page % 4d)\n",
				drawtimes.min / 1000000.0, drawtimes.minpage,
				drawtimes.avg / 1000000.0,
				drawtimes.max / 1000000.0, drawtimes.maxpage);
		}
	}
}

int main(int argc, char **argv)
{
	char *password = "";
	int c;
	enum { NO_FILE_OPENED, NO_PAGES_DRAWN, DREW_PAGES } state;

	fz_accelerate();

	while ((c = fz_getopt(argc, argv, "p:o:r:ms")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'o': drawpattern = fz_optarg; break;
		case 'r': drawzoom = atof(fz_optarg) / 72; break;
		case 'm': benchmark = 1; break;
		case 's': checksum = 1; break;
		default:
			drawusage();
			break;
		}
	}

	if (fz_optind == argc)
		drawusage();

	setcleanup(local_cleanup);

	state = NO_FILE_OPENED;
	while (fz_optind < argc)
	{
		if (strstr(argv[fz_optind], ".pdf") || strstr(argv[fz_optind], ".PDF"))
		{
			if (state == NO_PAGES_DRAWN)
				drawpages("1-");

			closexref();

			openxref(argv[fz_optind], password, 0, 1);
			state = NO_PAGES_DRAWN;
		}
		else
		{
			drawpages(argv[fz_optind]);
			state = DREW_PAGES;
		}
		fz_optind++;
	}

	if (state == NO_PAGES_DRAWN)
		drawpages("1-");

	closexref();

	return 0;
}
