/*
 * Benchmarking of loading and drawing pdf pages.
 */

#include "mupdf.h"

#define logbench(...) \
	printf(__VA_ARGS__); \
	fflush(stdout)

/* milli-second timer */
#ifdef _WIN32
#include <windows.h>

typedef struct mstimer {
	LARGE_INTEGER   start;
	LARGE_INTEGER   end;
} mstimer;

void timerstart(mstimer *timer)
{
	assert(timer);
	if (timer)
		QueryPerformanceCounter(&timer->start);
}
void timerstop(mstimer *timer)
{
	assert(timer);
	if (timer)
		QueryPerformanceCounter(&timer->end);
}

double timeinms(mstimer *timer)
{
	LARGE_INTEGER   freq;
	double          time_in_secs;
	assert(timer);
	if (!timer)
		return 0.0;
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
	if (timer)
		gettimeofday(&timer->start, NULL);
}

void timerstop(mstimer *timer)
{
	assert(timer);
	if (timer)
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

pdf_document *openxref(fz_context *ctx, char *filename)
{
	pdf_document *xref = pdf_open_document(ctx, filename);

	fz_try(ctx)
	{
		if (pdf_needs_password(xref))
		{
			logbench("Warning: password protected document\n");
			fz_throw(ctx, "document requires password");
		}

		pdf_count_pages(xref);
	}
	fz_catch(ctx)
	{
		pdf_close_document(xref);
		fz_rethrow(ctx);
	}

	return xref;
}

pdf_page *benchloadpage(fz_context *ctx, pdf_document *xref, int pagenum)
{
	pdf_page *page;
	mstimer timer;

	timerstart(&timer);
	fz_try(ctx) {
		page = pdf_load_page(xref, pagenum - 1);
	}
	fz_catch(ctx) {
		logbench("Error: failed to load page %d\n", pagenum);
		return NULL;
	}
	timerstop(&timer);
	logbench("pageload   %3d: %.2f ms\n", pagenum, timeinms(&timer));

	return page;
}

void benchrenderpage(fz_context *ctx, pdf_document *xref, pdf_page *page, int pagenum)
{
	fz_device *dev;
	fz_pixmap *pix;
	fz_bbox bbox;
	mstimer timer;

	timerstart(&timer);

	bbox = fz_round_rect(pdf_bound_page(xref, page));
	pix = fz_new_pixmap_with_rect(ctx, fz_device_rgb, bbox);
	fz_clear_pixmap_with_value(ctx, pix, 0xFF);
	dev = fz_new_draw_device(ctx, pix);
	fz_try(ctx) {
		pdf_run_page(xref, page, dev, fz_identity, NULL);
		timerstop(&timer);
		logbench("pagerender %3d: %.2f ms\n", pagenum, timeinms(&timer));
	}
	fz_catch(ctx) {
		logbench("Error: pdf_run_page() failed\n");
	}

	fz_drop_pixmap(ctx, pix);
	fz_free_device(dev);
}

void benchfile(char *pdffilename, int loadonly, int pageNo)
{
	pdf_document *xref = NULL;
	mstimer timer;
	int page_count;
	int curpage;

	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx) {
		logbench("Error: fz_new_context() failed\n");
		return;
	}

	logbench("Starting: %s\n", pdffilename);
	timerstart(&timer);
	fz_var(xref);
	fz_try(ctx) {
		xref = openxref(ctx, pdffilename);
	}
	fz_catch(ctx) {
		goto Exit;
	}
	timerstop(&timer);
	logbench("load: %.2f ms\n", timeinms(&timer));

	page_count = pdf_count_pages(xref);
	logbench("page count: %d\n", page_count);

	if (loadonly)
		goto Exit;
	for (curpage = 1; curpage <= page_count; curpage++) {
		pdf_page *page;
		if ((-1 != pageNo) && (pageNo != curpage))
			continue;
		page = benchloadpage(ctx, xref, curpage);
		if (page) {
			benchrenderpage(ctx, xref, page, curpage);
			pdf_free_page(xref, page);
		}
	}

Exit:
	logbench("Finished: %s\n", pdffilename);
	pdf_close_document(xref);
	fz_free_context(ctx);
}

void usage(void)
{
	fprintf(stderr, "usage: pdfbench [-loadonly] [-page N] <pdffile>\n");
	exit(1);
}

void parsecmdargs(int argc, char **argv, int *loadonly, int *pageNo)
{
	int i;

	if (argc < 2)
		usage();

	for (i = 1; i < argc - 1; i++) {
		if (!strcmp(argv[i], "-loadonly")) {
			*loadonly = 1;
		}
		else if (!strcmp(argv[i], "-page") && i + 1 < argc) {
			++i;
			*pageNo = atoi(argv[i]);
		}
		else {
			usage();
		}
	}
}

int main(int argc, char **argv)
{
	int loadonly = 0, pageNo = -1;
	parsecmdargs(argc, argv, &loadonly, &pageNo);
	/* for simplicity assume the file to parse is always the last */
	benchfile(argv[argc-1], loadonly, pageNo);
	return 0;
}
