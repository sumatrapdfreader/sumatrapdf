/*
 * Benchmarking of loading and drawing pdf pages.
 */

#ifdef WIN32
#include <windows.h>
#endif

#include "fitz.h"
#include "mupdf.h"

#define logbench(...) \
	printf(__VA_ARGS__); \
	fflush(stdout)

int pagetobench = -1;
int loadonly = 0;
pdf_xref *src = nil;
pdf_pagetree *srcpages = nil;
fz_renderer *drawgc = nil;
pdf_page *drawpage = nil;

/* milli-second timer */
#ifdef _WIN32
typedef struct mstimer {
    LARGE_INTEGER   start;
    LARGE_INTEGER   end;
} mstimer;

void timerstart(mstimer *timer)
{
    assert(timer);
    if (!timer)
        return;
    QueryPerformanceCounter(&timer->start);
}
void timerstop(mstimer *timer)
{
    assert(timer);
    if (!timer)
        return;
    QueryPerformanceCounter(&timer->end);
}

double timeinms(mstimer *timer)
{
    LARGE_INTEGER   freq;
    double          time_in_secs;
    QueryPerformanceFrequency(&freq);
    time_in_secs = (double)(timer->end.QuadPart-timer->start.QuadPart)/(double)freq.QuadPart;
    return time_in_secs * 1000.0;
}
#else
#include <sys/time.h>
typedef struct mstimer {
    struct timeval    start;
    struct timeval    end;
} mstimer;

void timerstart(mstimer *timer)
{
    assert(timer);
    if (!timer)
        return;
    gettimeofday(&timer->start, NULL);
}

void timerstop(mstimer *timer)
{
    assert(timer);
    if (!timer)
        return;
    gettimeofday(&timer->end, NULL);
}

double timeinms(mstimer *timer)
{
    double timeInMs;
    time_t seconds;
    int    usecs;

    assert(timer);
    if (!timer)
        return 0.0;
    seconds = timer->end.tv_sec - timer->start.tv_sec;
    usecs = timer->end.tv_usec - timer->start.tv_usec;
    if (usecs < 0) {
        --seconds;
        usecs += 1000000;
    }
    timeInMs = (double)seconds*(double)1000.0 + (double)usecs/(double)1000.0;
    return timeInMs;
}
#endif

void closesrc(void)
{
	if (srcpages)
	{
		pdf_droppagetree(srcpages);
		srcpages = nil;
	}

	if (src)
	{
		if (src->store)
		{
			pdf_dropstore(src->store);
			src->store = nil;
		}
		pdf_closexref(src);
		src = nil;
	}
}

fz_error* opensrc(char *filename, char *password)
{
	fz_error *error;

	error = pdf_newxref(&src);
	if (error)
		return error;

	error = pdf_loadxref(src, filename);
	if (error)
	{
		logbench("Warning: pdf_loadxref() failed, trying to repair\n");
		error = pdf_repairxref(src, filename);
		if (error)
		{
			logbench("Error: pdf_repairxref() failed\n");
			return error;
		}
	}

	error = pdf_decryptxref(src);
	if (error)
	{
		logbench("Error: pdf_decryptxref() failed\n");
		return error;
	}

	if (src->crypt)
	{
		int okay = pdf_setpassword(src->crypt, password);
		if (!okay)
		{
			logbench("Warning: pdf_setpassword() failed, incorrect password\n");
			return fz_throw("invalid password");
		}
	}

	error = pdf_loadpagetree(&srcpages, src);
	if (error)
	{
		logbench("Error: pdf_loadpagetree() failed\n");
	}
	return error;
}

fz_error* benchloadpage(int pagenum)
{
	fz_error *error;
	fz_obj *pageobj;
	mstimer timer;
	double timems;

	timerstart(&timer);
	pageobj = pdf_getpageobject(srcpages, pagenum - 1);
	drawpage = nil;
	error = pdf_loadpage(&drawpage, src, pageobj);
	timerstop(&timer);
	if (error)
	{
		logbench("Error: failed to load page %d\n", pagenum);
		return error;
	}
	timems = timeinms(&timer);
	logbench("pageload   %3d: %.2f ms\n", pagenum, timems);
	return fz_okay;
}

fz_error* benchrenderpage(int pagenum)
{
	fz_error *error;
	fz_matrix ctm;
	fz_irect bbox;
	fz_pixmap *pix;
	int w, h;
	mstimer timer;
	double timems;

	timerstart(&timer);
	ctm = fz_identity();
	ctm = fz_concat(ctm, fz_translate(0, -drawpage->mediabox.y1));

	bbox = fz_roundrect(fz_transformaabb(ctm, drawpage->mediabox));
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	error = fz_newpixmap(&pix, bbox.x0, bbox.y0, w, h, 4);
	if (error)
	{
		logbench("Error: fz_newpixmap() failed\n");
		return error;
	}

	memset(pix->samples, 0xff, pix->h * pix->w * pix->n);
	error = fz_rendertreeover(drawgc, pix, drawpage->tree, ctm);
	if (error)
	{
		logbench("Error: rz_rendertreeover() failed\n");
		goto Exit;
	}

	timerstop(&timer);
	timems = timeinms(&timer);

	logbench("pagerender %3d: %.2f ms\n", pagenum, timems);
Exit:
	fz_droppixmap(pix);
	return fz_okay;
}

void freepage(void)
{
	pdf_droppage(drawpage);
	drawpage = nil;
}

void benchfile(char *pdffilename)
{
	fz_error *error;
	mstimer timer;
	double timems;
	int pages;
	int curpage;

	error = fz_newrenderer(&drawgc, pdf_devicergb, 0, 1024 * 512);
	if (error)
	{
		logbench("Error: fz_newrenderer() failed\n");
		goto Exit;
	}

	logbench("Starting: %s\n", pdffilename);
	timerstart(&timer);
	error = opensrc(pdffilename, "");
	timerstop(&timer);
	if (error)
		goto Exit;
	timems = timeinms(&timer);
	logbench("load: %.2f ms\n", timems);

	pages = pdf_getpagecount(srcpages);
	logbench("page count: %d\n", pages);

	if (loadonly)
		goto Exit;
	for (curpage = 1; curpage <= pages; curpage++) {
		if ((-1 != pagetobench) && (pagetobench != curpage))
			continue;
		error = benchloadpage(curpage);
		if (!error)
			benchrenderpage(curpage);
		if (drawpage)
			freepage();
	}

Exit:
	logbench("Finished: %s\n", pdffilename);
	if (drawgc)
		fz_droprenderer(drawgc);
}

void usage(void)
{
	fprintf(stderr, "usage: pdfbench [-loadonly] [-page N] <pdffile>\n");
	exit(1);
}

int isarg(char *arg, char *name)
{
	if ('-' != *arg++)
		return 0;
	/* be liberal, allow '-' and '--' as arg prefix */
	if ('-'== *arg) ++arg;
	return (0 == strcmp(arg, name));
}

void parsecmdargs(int argc, char **argv)
{
	int i;
	char *arg;

	if (argc < 2)
		usage();

	for (i=1; i<argc; i++) {
		arg = argv[i];	
		if (isarg(arg, "loadonly")) {
			loadonly = 1;
		}
		if (isarg(arg, "page")) {
			++i;
			if (i == argc)
				usage();
			pagetobench = atoi(argv[i]);
		}
	}
}

int main(int argc, char **argv)
{
	parsecmdargs(argc, argv);
	/* for simplicity assume the file to parse is always the last */
	benchfile(argv[argc-1]);
	return 0;
}
