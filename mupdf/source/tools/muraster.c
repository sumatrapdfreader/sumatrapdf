/*
 * muraster -- Convert a document to a raster file.
 *
 * Deliberately simple. Designed to be a basis for what
 * printer customers would need.
 *
 * Therefore; only supports pgm, ppm, pam, pbm, pkm,
 * and then only dependent on the FZ_PLOTTERS_{G,RGB,CMYK}
 * flags.
 * Only supports banding.
 * Supports auto fallback to grey if possible.
 * Supports threading.
 * Supports fallback in low memory cases.
 */

/*
	CONFIGURATION SECTION

	The first bit of configuration for this is actually in
	how the muthreads helper library is built. If muthreads
	does not know how to support threading on your system
	then it will ensure that DISABLE_MUTHREADS is set. All
	the muthreads entrypoints/types will still be defined
	(as dummy types/functions), but attempting to use them
	will return errors.

	Configuration options affecting threading should be
	turned off if DISABLE_MUTHREADS is set.

	Integrators can/should define the following
	MURASTER_CONFIG_ values. If not set, we'll
	attempt to set sensible defaults.
*/

/*
	MURASTER_CONFIG_RENDER_THREADS: The number of render
	threads to use. Typically you would set this to the
	number of CPU cores - 1 (or -2 if background printing
	is used).

	If no threading library exists for your OS set this
	to 0.

	If undefined, we will use a default of
	3 - MURASTER_CONFIG_BGPRINT.
*/
/* #define MURASTER_CONFIG_RENDER_THREADS 3 */

/*
	MURASTER_CONFIG_BGPRINT: 0 or 1. Set to 1 to
	enable background printing. This relies on
	a threading library existing for the OS.

	If undefined, we will use a default of 1.
*/
/* #define MURASTER_CONFIG_BGPRINT 1 */

/*
	MURASTER_CONFIG_X_RESOLUTION: The default X resolution
	in dots per inch. If undefined, taken to be 300dpi.
*/
/* #define MURASTER_CONFIG_X_RESOLUTION 300 */

/*
	MURASTER_CONFIG_Y_RESOLUTION: The default Y resolution
	in dots per inch. If undefined, taken to be 300dpi.
*/
/* #define MURASTER_CONFIG_Y_RESOLUTION 300 */

/*
	MURASTER_CONFIG_WIDTH: The printable page width
	(in inches)
*/
/* #define MURASTER_CONFIG_WIDTH 8.27f */

/*
	MURASTER_CONFIG_HEIGHT: The printable page height
	(in inches)
*/
/* #define MURASTER_CONFIG_HEIGHT 11.69f */

/*
	MURASTER_CONFIG_STORE_SIZE: The maximum size to use
	for the fz_store.

	If undefined, then on Linux we will attempt to guess
	the memory size, and we'll use that for the store
	size. This will be too large, but it should work OK.

	If undefined and NOT linux, then we'll use the default
	store size.
*/
/* #define MURASTER_CONFIG_STORE_SIZE FZ_STORE_DEFAULT */

/*
	MURASTER_CONFIG_MIN_BAND_HEIGHT: The minimum band
	height we will ever use. This might correspond to the
	number of nozzles on an inkjet head.

	By default, we'll use 32.
*/
/* #define MURASTER_CONFIG_MIN_BAND_HEIGHT 32 */

/*
	MURASTER_CONFIG_BAND_MEMORY: The maximum amount of
	memory (in bytes) to use for any given band.

	We will need MURASTER_CONFIG_RENDER_THREADS of these,
	one for each render thread.

	Having this be a multiple of
	MURASTER_CONFIG_MIN_BAND_HEIGHT * MURASTER_CONFIG_MAX_WIDTH * MURASTER_CONFIG_X_RESOLUTION * N
	would be sensible.

	(Where N = 1 for greyscale, 3 for RGB, 4 for CMYK)
*/
/* #define MURASTER_CONFIG_BAND_MEMORY (32*10*300*4*16) */

/*
	MURASTER_CONFIG_GREY_FALLBACK: 0, 1 or 2.

	Set to 1 to fallback to grey rendering if the page
	is definitely grey. Any images in colored color
	spaces will be assumed to be color. This may refuse
	to fallback in some cases when it could have done.

	Set to 2 to fallback to grey rendering if the page
	is definitely grey. Any images in colored color
	spaces will be exhaustively checked. This will
	fallback whenever possible, at the expense of some
	runtime as more processing is required to check.
*/
/* #define MURASTER_CONFIG_GREY_FALLBACK 1 */

/*
	END OF CONFIGURATION SECTION
*/

#include "mupdf/fitz.h"
#include "mupdf/helpers/mu-threads.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _MSC_VER
struct timeval;
struct timezone;
int gettimeofday(struct timeval *tv, struct timezone *tz);
#else
#include <sys/time.h>
#endif

/*
	After this point, we convert the #defines set (or not set)
	above into sensible values we can work with. Don't edit
	these for configuration.
*/

/* Unless we have specifically disabled threading, enable it. */
#ifndef DISABLE_MUTHREADS
#ifndef MURASTER_THREADS
#define MURASTER_THREADS 1
#endif
#endif

/* If we have threading, and we haven't already configured BGPRINT,
 * enable it. */
#if MURASTER_THREADS != 0
#ifndef MURASTER_CONFIG_BGPRINT
#define MURASTER_CONFIG_BGPRINT 1
#endif
#endif

#ifdef MURASTER_CONFIG_X_RESOLUTION
#define X_RESOLUTION MURASTER_CONFIG_X_RESOLUTION
#else
#define X_RESOLUTION 300
#endif

#ifdef MURASTER_CONFIG_Y_RESOLUTION
#define Y_RESOLUTION MURASTER_CONFIG_Y_RESOLUTION
#else
#define Y_RESOLUTION 300
#endif

#ifdef MURASTER_CONFIG_WIDTH
#define PAPER_WIDTH MURASTER_CONFIG_WIDTH
#else
#define PAPER_WIDTH 8.27f
#endif

#ifdef MURASTER_CONFIG_HEIGHT
#define PAPER_HEIGHT MURASTER_CONFIG_HEIGHT
#else
#define PAPER_HEIGHT 11.69f
#endif

#ifdef MURASTER_CONFIG_STORE_SIZE
#define STORE_SIZE MURASTER_CONFIG_STORE_SIZE
#else
#define STORE_SIZE FZ_STORE_SIZE
#endif

#ifdef MURASTER_CONFIG_MIN_BAND_HEIGHT
#define MIN_BAND_HEIGHT MURASTER_CONFIG_MIN_BAND_HEIGHT
#else
#define MIN_BAND_HEIGHT 32
#endif

#ifdef MURASTER_CONFIG_BAND_MEMORY
#define BAND_MEMORY MURASTER_CONFIG_BAND_MEMORY
#else
#if defined(FZ_PLOTTERS_CMYK) || defined(FZ_PLOTTERS_N)
#define BAND_MEMORY (MIN_BAND_HEIGHT * PAPER_WIDTH * X_RESOLUTION * 4 * 16)
#elif defined(FZ_PLOTTERS_RGB)
#define BAND_MEMORY (MIN_BAND_HEIGHT * PAPER_WIDTH * X_RESOLUTION * 3 * 16)
#else
#define BAND_MEMORY (MIN_BAND_HEIGHT * PAPER_WIDTH * X_RESOLUTION * 1 * 16)
#endif
#endif

#ifdef MURASTER_CONFIG_GREY_FALLBACK
#define GREY_FALLBACK MURASTER_CONFIG_GREY_FALLBACK
#else
#ifdef FZ_PLOTTERS_N
#define GREY_FALLBACK 1
#elif defined(FZ_PLOTTERS_G) && (defined(FZ_PLOTTERS_RGB) || defined(FZ_PLOTTERS_CMYK))
#define GREY_FALLBACK 1
#else
#define GREY_FALLBACK 0
#endif
#endif

#if GREY_FALLBACK != 0 && !defined(FZ_PLOTTERS_N) && !defined(FZ_PLOTTERS_G)
#error MURASTER_CONFIG_GREY_FALLBACK requires either FZ_PLOTTERS_N or FZ_PLOTTERS_G
#endif

/* Enable for helpful threading debug */
/* #define DEBUG_THREADS(A) do { printf A; fflush(stdout); } while (0) */
#define DEBUG_THREADS(A) do { } while (0)

enum {
	OUT_PGM,
	OUT_PPM,
	OUT_PAM,
	OUT_PBM,
	OUT_PKM
};

enum {
	CS_GRAY,
	CS_RGB,
	CS_CMYK
};

typedef struct
{
	char *suffix;
	int format;
	int cs;
} suffix_t;

static const suffix_t suffix_table[] =
{
#if FZ_PLOTTERS_G || FZ_PLOTTERS_N
	{ ".pgm", OUT_PGM, CS_GRAY },
#endif
#if FZ_PLOTTERS_RGB || FZ_PLOTTERS_N
	{ ".ppm", OUT_PPM, CS_RGB },
#endif
#if FZ_PLOTTERS_CMYK || FZ_PLOTTERS_N
	{ ".pam", OUT_PAM, CS_CMYK },
#endif
#if FZ_PLOTTERS_G || FZ_PLOTTERS_N
	{ ".pbm", OUT_PBM, CS_GRAY },
#endif
#if FZ_PLOTTERS_CMYK || FZ_PLOTTERS_N
	{ ".pkm", OUT_PKM, CS_CMYK }
#endif
};

#ifndef DISABLE_MUTHREADS

static mu_mutex mutexes[FZ_LOCK_MAX];

static void muraster_lock(void *user, int lock)
{
	mu_lock_mutex(&mutexes[lock]);
}

static void muraster_unlock(void *user, int lock)
{
	mu_unlock_mutex(&mutexes[lock]);
}

static fz_locks_context muraster_locks =
{
	NULL, muraster_lock, muraster_unlock
};

static void fin_muraster_locks(void)
{
	int i;

	for (i = 0; i < FZ_LOCK_MAX; i++)
		mu_destroy_mutex(&mutexes[i]);
}

static fz_locks_context *init_muraster_locks(void)
{
	int i;
	int failed = 0;

	for (i = 0; i < FZ_LOCK_MAX; i++)
		failed |= mu_create_mutex(&mutexes[i]);

	if (failed)
	{
		fin_muraster_locks();
		return NULL;
	}

	return &muraster_locks;
}

#endif

#ifdef MURASTER_CONFIG_RENDER_THREADS
#define NUM_RENDER_THREADS MURASTER_CONFIG_RENDER_THREADS
#elif defined(DISABLE_MUTHREADS)
#define NUM_RENDER_THREADS 0
#else
#define NUM_RENDER_THREADS 3
#endif

#if defined(DISABLE_MUTHREADS) && NUM_RENDER_THREADS != 0
#error "Can't have MURASTER_CONFIG_RENDER_THREADS > 0 without having a threading library!"
#endif

#ifdef MURASTER_CONFIG_BGPRINT
#define BGPRINT MURASTER_CONFIG_BGPRINT
#elif MURASTER_THREADS == 0
#define BGPRINT 0
#else
#define BGPRINT 1
#endif

#if defined(DISABLE_MUTHREADS) && BGPRINT != 0
#error "Can't have MURASTER_CONFIG_BGPRINT > 0 without having a threading library!"
#endif

typedef struct worker_t {
	fz_context *ctx;
	int started;
	int status;
	int num;
	int band_start; /* -1 to shutdown, or offset of band to render */
	fz_display_list *list;
	fz_matrix ctm;
	fz_rect tbounds;
	fz_pixmap *pix;
	fz_bitmap *bit;
	fz_cookie cookie;
	mu_semaphore start;
	mu_semaphore stop;
	mu_thread thread;
} worker_t;

static char *output = NULL;
static fz_output *out = NULL;

static char *format;
static int output_format;
static int output_cs;

static int rotation = -1;
static float x_resolution;
static float y_resolution;
static int width = 0;
static int height = 0;
static int fit = 0;

static float layout_w = FZ_DEFAULT_LAYOUT_W;
static float layout_h = FZ_DEFAULT_LAYOUT_H;
static float layout_em = FZ_DEFAULT_LAYOUT_EM;
static char *layout_css = NULL;
static int layout_use_doc_css = 1;

static int showtime = 0;
static int showmemory = 0;

static int ignore_errors = 0;
static int alphabits_text = 8;
static int alphabits_graphics = 8;

static int min_band_height;
static size_t max_band_memory;

static int errored = 0;
static fz_colorspace *colorspace;
static char *filename;
static int num_workers = 0;
static worker_t *workers;

typedef struct render_details
{
	/* Page */
	fz_page *page;

	/* Display list */
	fz_display_list *list;

	/* Raw bounds */
	fz_rect bounds;

	/* Transformed bounds */
	fz_rect tbounds;

	/* Rounded transformed bounds */
	fz_irect ibounds;

	/* Transform matrix */
	fz_matrix ctm;

	/* How many min band heights are we working in? */
	int band_height_multiple;

	/* What colorspace are we working in? (Adjusted for fallback) */
	int colorspace;

	/* What output format? (Adjusted for fallback) */
	int format;

	/* During the course of the rendering, this keeps track of
	 * how many 'min_band_heights' have been safely rendered. */
	int bands_rendered;

	/* The maximum number of workers we'll try to use. This
	 * will start at the maximum value, and may drop to 0
	 * if we have problems with memory. */
	int num_workers;

	/* The band writer to output the page */
	fz_band_writer *bander;

	/* Number of components in image */
	int n;
} render_details;

enum
{
	RENDER_OK = 0,
	RENDER_RETRY = 1,
	RENDER_FATAL = 2
};

static struct {
	int active;
	int started;
	int solo;
	int status;
	fz_context *ctx;
	mu_thread thread;
	mu_semaphore start;
	mu_semaphore stop;
	int pagenum;
	char *filename;
	render_details render;
	int interptime;
} bgprint;

static struct {
	int count, total;
	int min, max;
	int mininterp, maxinterp;
	int minpage, maxpage;
	char *minfilename;
	char *maxfilename;
} timing;

#define stringify(A) #A

static void usage(void)
{
	fprintf(stderr,
		"muraster version " FZ_VERSION "\n"
		"Usage: muraster [options] file [pages]\n"
		"\t-p -\tpassword\n"
		"\n"
		"\t-o -\toutput file name\n"
		"\t-F -\toutput format (default inferred from output file name)\n"
		"\t\tpam, pbm, pgm, pkm, ppm\n"
		"\n"
		"\t-s -\tshow extra information:\n"
		"\t\tm - show memory use\n"
		"\t\tt - show timings\n"
		"\n"
		"\t-R {auto,0,90,180,270}\n"
		"\t\trotate clockwise (default: auto)\n"
		"\t-r -{,_}\tx and y resolution in dpi (default: " stringify(X_RESOLUTION) "x" stringify(Y_RESOLUTION) ")\n"
		"\t-w -\tprintable width (in inches) (default: " stringify(PAPER_WIDTH) ")\n"
		"\t-h -\tprintable height (in inches) (default: " stringify(PAPER_HEIGHT) "\n"
		"\t-f\tfit file to page if too large\n"
		"\t-B -\tminimum band height (e.g. 32)\n"
		"\t-M -\tmax bandmemory (e.g. 655360)\n"
#ifndef DISABLE_MUTHREADS
		"\t-T -\tnumber of threads to use for rendering\n"
		"\t-P\tparallel interpretation/rendering\n"
#endif
		"\n"
		"\t-W -\tpage width for EPUB layout\n"
		"\t-H -\tpage height for EPUB layout\n"
		"\t-S -\tfont size for EPUB layout\n"
		"\t-U -\tfile name of user stylesheet for EPUB layout\n"
		"\t-X\tdisable document styles for EPUB layout\n"
		"\n"
		"\t-A -\tnumber of bits of antialiasing (0 to 8)\n"
		"\t-A -/-\tnumber of bits of antialiasing (0 to 8) (graphics, text)\n"
		"\n"
		"\tpages\tcomma separated list of page numbers and ranges\n"
		);
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

static int drawband(fz_context *ctx, fz_page *page, fz_display_list *list, fz_matrix ctm, fz_rect tbounds, fz_cookie *cookie, int band_start, fz_pixmap *pix, fz_bitmap **bit)
{
	fz_device *dev = NULL;

	*bit = NULL;

	fz_try(ctx)
	{
		fz_clear_pixmap_with_value(ctx, pix, 255);

		dev = fz_new_draw_device(ctx, fz_identity, pix);
		if (alphabits_graphics == 0)
			fz_enable_device_hints(ctx, dev, FZ_DONT_INTERPOLATE_IMAGES);
		if (list)
			fz_run_display_list(ctx, list, dev, ctm, tbounds, cookie);
		else
			fz_run_page(ctx, page, dev, ctm, cookie);
		fz_close_device(ctx, dev);
		fz_drop_device(ctx, dev);
		dev = NULL;

		if ((output_format == OUT_PBM) || (output_format == OUT_PKM))
			*bit = fz_new_bitmap_from_pixmap_band(ctx, pix, NULL, band_start);
	}
	fz_catch(ctx)
	{
		fz_drop_device(ctx, dev);
		return RENDER_RETRY;
	}
	return RENDER_OK;
}

static int dodrawpage(fz_context *ctx, int pagenum, fz_cookie *cookie, render_details *render)
{
	fz_pixmap *pix = NULL;
	fz_bitmap *bit = NULL;
	int errors_are_fatal = 0;
	fz_irect ibounds = render->ibounds;
	fz_rect tbounds = render->tbounds;
	int total_height = ibounds.y1 - ibounds.y0;
	int start_offset = min_band_height * render->bands_rendered;
	int remaining_start = ibounds.y0 + start_offset;
	int remaining_height = ibounds.y1 - remaining_start;
	int band_height = min_band_height * render->band_height_multiple;
	int bands = (remaining_height + band_height-1) / band_height;
	fz_matrix ctm = render->ctm;
	int band;

	fz_var(pix);
	fz_var(bit);
	fz_var(errors_are_fatal);

	fz_try(ctx)
	{
		/* Set up ibounds and tbounds for a single band_height band.
		 * We will adjust ctm as we go. */
		ibounds.y1 = ibounds.y0 + band_height;
		tbounds.y1 = tbounds.y0 + band_height + 2;
		DEBUG_THREADS(("Using %d Bands\n", bands));
		ctm.f += start_offset;

		if (render->num_workers > 0)
		{
			for (band = 0; band < fz_mini(render->num_workers, bands); band++)
			{
				int band_start = start_offset + band * band_height;
				worker_t *w = &workers[band];
				w->band_start = band_start;
				w->ctm = ctm;
				w->tbounds = tbounds;
				memset(&w->cookie, 0, sizeof(fz_cookie));
				w->list = render->list;
				if (remaining_height < band_height)
					ibounds.y1 = ibounds.y0 + remaining_height;
				remaining_height -= band_height;
				w->pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, NULL, 0);
				fz_set_pixmap_resolution(ctx, w->pix, x_resolution, y_resolution);
				DEBUG_THREADS(("Worker %d, Pre-triggering band %d\n", band, band));
				w->started = 1;
				mu_trigger_semaphore(&w->start);
				ctm.f -= band_height;
			}
			pix = workers[0].pix;
		}
		else
		{
			pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds, NULL, 0);
			fz_set_pixmap_resolution(ctx, pix, x_resolution, y_resolution);
		}

		for (band = 0; band < bands; band++)
		{
			int status;
			int band_start = start_offset + band * band_height;
			int draw_height = total_height - band_start;

			if (draw_height > band_height)
				draw_height = band_height;

			if (render->num_workers > 0)
			{
				worker_t *w = &workers[band % render->num_workers];
				DEBUG_THREADS(("Waiting for worker %d to complete band %d\n", w->num, band));
				mu_wait_semaphore(&w->stop);
				w->started = 0;
				status = w->status;
				pix = w->pix;
				bit = w->bit;
				w->bit = NULL;
				cookie->errors += w->cookie.errors;
			}
			else
				status = drawband(ctx, render->page, render->list, ctm, tbounds, cookie, band_start, pix, &bit);

			if (status != RENDER_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "Render failed");

			render->bands_rendered += render->band_height_multiple;

			if (out)
			{
				/* If we get any errors while outputting the bands, retrying won't help. */
				errors_are_fatal = 1;
				fz_write_band(ctx, render->bander, bit ? bit->stride : pix->stride, draw_height, bit ? bit->samples : pix->samples);
				errors_are_fatal = 0;
			}
			fz_drop_bitmap(ctx, bit);
			bit = NULL;

			if (render->num_workers > 0 && band + render->num_workers < bands)
			{
				worker_t *w = &workers[band % render->num_workers];
				w->band_start = band_start;
				w->ctm = ctm;
				w->tbounds = tbounds;
				memset(&w->cookie, 0, sizeof(fz_cookie));
				DEBUG_THREADS(("Triggering worker %d for band_start= %d\n", w->num, w->band_start));
				w->started = 1;
				mu_trigger_semaphore(&w->start);
			}
			ctm.f -= draw_height;
		}
	}
	fz_always(ctx)
	{
		fz_drop_bitmap(ctx, bit);
		bit = NULL;
		if (render->num_workers > 0)
		{
			int band;
			for (band = 0; band < fz_mini(render->num_workers, bands); band++)
			{
				worker_t *w = &workers[band];
				w->cookie.abort = 1;
				if (w->started)
				{
					mu_wait_semaphore(&w->stop);
					w->started = 0;
				}
				fz_drop_pixmap(ctx, w->pix);
			}
		}
		else
			fz_drop_pixmap(ctx, pix);
	}
	fz_catch(ctx)
	{
		/* Swallow error */
		if (errors_are_fatal)
			return RENDER_FATAL;
		return RENDER_RETRY;
	}
	if (cookie->errors)
		errored = 1;

	return RENDER_OK;
}

/* This functions tries to render a page, falling back repeatedly to try and make it work. */
static int try_render_page(fz_context *ctx, int pagenum, fz_cookie *cookie, int start, int interptime, char *filename, int bg, int solo, render_details *render)
{
	int status;

	if (out && !(bg && solo))
	{
		/* Output any page level headers (for banded formats). Don't do this if
		 * we're running in solo bgprint mode, cos we've already done it once! */
		fz_try(ctx)
		{
			int w = render->ibounds.x1 - render->ibounds.x0;
			int h = render->ibounds.y1 - render->ibounds.y0;
			fz_write_header(ctx, render->bander, w, h, render->n, 0, 0, 0, 0, 0, NULL);
		}
		fz_catch(ctx)
		{
			/* Failure! */
			return RENDER_FATAL;
		}
	}

	while (1)
	{
		status = dodrawpage(ctx, pagenum, cookie, render);
		if (status == RENDER_OK || status == RENDER_FATAL)
			break;

		/* If we are bgprinting, then ask the caller to try us again in solo mode. */
		if (bg && !solo)
		{
			DEBUG_THREADS(("Render failure; trying again in solo mode\n"));
			return RENDER_RETRY; /* Avoids all the cleanup below! */
		}

		/* Try again with fewer threads */
		if (render->num_workers > 1)
		{
			render->num_workers >>= 1;
			DEBUG_THREADS(("Render failure; trying again with %d render threads\n", render->num_workers));
			continue;
		}

		/* Halve the band height, if we still can. */
		if (render->band_height_multiple > 2)
		{
			render->band_height_multiple >>= 1;
			DEBUG_THREADS(("Render failure; trying again with %d band height multiple\n", render->band_height_multiple));
			continue;
		}

		/* If all else fails, ditch the list and try again. */
		if (render->list)
		{
			fz_drop_display_list(ctx, render->list);
			render->list = NULL;
			DEBUG_THREADS(("Render failure; trying again with no list\n"));
			continue;
		}

		/* Give up. */
		DEBUG_THREADS(("Render failure; nothing else to try\n"));
		break;
	}

	fz_drop_page(ctx, render->page);
	fz_drop_display_list(ctx, render->list);
	fz_drop_band_writer(ctx, render->bander);

	if (showtime)
	{
		int end = gettime();
		int diff = end - start;

		if (bg)
		{
			if (diff + interptime < timing.min)
			{
				timing.min = diff + interptime;
				timing.mininterp = interptime;
				timing.minpage = pagenum;
				timing.minfilename = filename;
			}
			if (diff + interptime > timing.max)
			{
				timing.max = diff + interptime;
				timing.maxinterp = interptime;
				timing.maxpage = pagenum;
				timing.maxfilename = filename;
			}
			timing.total += diff + interptime;
			timing.count ++;

			fprintf(stderr, " %dms (interpretation) %dms (rendering) %dms (total)\n", interptime, diff, diff + interptime);
		}
		else
		{
			if (diff < timing.min)
			{
				timing.min = diff;
				timing.minpage = pagenum;
				timing.minfilename = filename;
			}
			if (diff > timing.max)
			{
				timing.max = diff;
				timing.maxpage = pagenum;
				timing.maxfilename = filename;
			}
			timing.total += diff;
			timing.count ++;

			fprintf(stderr, " %dms\n", diff);
		}
	}

	if (showmemory)
	{
		fz_dump_glyph_cache_stats(ctx);
	}

	fz_flush_warnings(ctx);

	return status;
}

static int wait_for_bgprint_to_finish(void)
{
	if (!bgprint.active || !bgprint.started)
		return 0;

	mu_wait_semaphore(&bgprint.stop);
	bgprint.started = 0;
	return bgprint.status;
}

static void
get_page_render_details(fz_context *ctx, fz_page *page, render_details *render)
{
	float page_width, page_height;
	int rot;
	float s_x, s_y;

	render->page = page;
	render->list = NULL;
	render->num_workers = num_workers;

	render->bounds = fz_bound_page(ctx, page);
	page_width = (render->bounds.x1 - render->bounds.x0)/72;
	page_height = (render->bounds.y1 - render->bounds.y0)/72;

	s_x = x_resolution / 72;
	s_y = y_resolution / 72;
	if (rotation == -1)
	{
		/* Automatic rotation. If we fit, use 0. If we don't, and 90 would be 'better' use that. */
		if (page_width <= width && page_height <= height)
		{
			/* Page fits, so use no rotation. */
			rot = 0;
		}
		else if (fit)
		{
			/* Use whichever gives the biggest scale */
			float sx_0 = width / page_width;
			float sy_0 = height / page_height;
			float sx_90 = height / page_width;
			float sy_90 = width / page_height;
			float s_0, s_90;
			s_0 = fz_min(sx_0, sy_0);
			s_90 = fz_min(sx_90, sy_90);
			if (s_0 >= s_90)
			{
				rot = 0;
				if (s_0 < 1)
				{
					s_x *= s_0;
					s_y *= s_0;
				}
			}
			else
			{
				rot = 90;
				if (s_90 < 1)
				{
					s_x *= s_90;
					s_y *= s_90;
				}
			}
		}
		else
		{
			/* Use whichever crops the least area */
			float lost0 = 0;
			float lost90 = 0;

			if (page_width > width)
				lost0 += (page_width - width) * (page_height > height ? height : page_height);
			if (page_height > height)
				lost0 += (page_height - height) * page_width;

			if (page_width > height)
				lost90 += (page_width - height) * (page_height > width ? width : page_height);
			if (page_height > width)
				lost90 += (page_height - width) * page_width;

			rot = (lost0 <= lost90 ? 0 : 90);
		}
	}
	else
	{
		rot = rotation;
	}

	render->ctm = fz_pre_scale(fz_rotate(rot), s_x, s_y);
	render->tbounds = fz_transform_rect(render->bounds, render->ctm);;
	render->ibounds = fz_round_rect(render->tbounds);
}

static void
initialise_banding(fz_context *ctx, render_details *render, int color)
{
	size_t min_band_mem;
	int bpp, h, w, reps;

	render->colorspace = output_cs;
	render->format = output_format;
#if GREY_FALLBACK != 0
	if (color == 0)
	{
		if (render->colorspace == CS_RGB)
		{
			/* Fallback from PPM to PGM */
			render->colorspace = CS_GRAY;
			render->format = OUT_PGM;
		}
		else if (render->colorspace == CS_CMYK)
		{
			render->colorspace = CS_GRAY;
			if (render->format == OUT_PKM)
				render->format = OUT_PBM;
			else
				render->format = OUT_PGM;
		}
	}
#endif

	switch (render->colorspace)
	{
	case CS_GRAY:
		bpp = 1;
		break;
	case CS_RGB:
		bpp = 2;
		break;
	default:
	case CS_CMYK:
		bpp = 3;
		break;
	}

	w = render->ibounds.x1 - render->ibounds.x0;
	min_band_mem = bpp * w * min_band_height;
	reps = (int)(max_band_memory / min_band_mem);
	if (reps < 1)
		reps = 1;

	/* Adjust reps to even out the work between threads */
	if (render->num_workers > 0)
	{
		int runs, num_bands;
		h = render->ibounds.y1 - render->ibounds.y0;
		num_bands = (h + min_band_height - 1) / min_band_height;
		/* num_bands = number of min_band_height bands */
		runs = (num_bands + reps-1) / reps;
		/* runs = number of worker runs of reps min_band_height bands */
		runs = ((runs + render->num_workers - 1) / render->num_workers) * render->num_workers;
		/* runs = number of worker runs rounded up to make use of all our threads */
		reps = (num_bands + runs - 1) / runs;
	}

	render->band_height_multiple = reps;
	render->bands_rendered = 0;

	if (output_format == OUT_PGM || output_format == OUT_PPM)
	{
		render->bander = fz_new_pnm_band_writer(ctx, out);
		render->n = output_format == OUT_PGM ? 1 : 3;
	}
	else if (output_format == OUT_PAM)
	{
		render->bander = fz_new_pam_band_writer(ctx, out);
		render->n = 4;
	}
	else if (output_format == OUT_PBM)
	{
		render->bander = fz_new_pbm_band_writer(ctx, out);
		render->n = 1;
	}
	else if (output_format == OUT_PKM)
	{
		render->bander = fz_new_pkm_band_writer(ctx, out);
		render->n = 4;
	}
}

static void drawpage(fz_context *ctx, fz_document *doc, int pagenum)
{
	fz_page *page;
	fz_display_list *list = NULL;
	fz_device *list_dev = NULL;
	int start;
	fz_cookie cookie = { 0 };
#if GREY_FALLBACK != 0
	fz_device *test_dev = NULL;
	int is_color = 0;
#else
	int is_color = 2;
#endif
	render_details render;
	int status;

	fz_var(list);
	fz_var(list_dev);
	fz_var(test_dev);

	do
	{
		start = (showtime ? gettime() : 0);

		page = fz_load_page(ctx, doc, pagenum - 1);

		/* Calculate Page bounds, transform etc */
		get_page_render_details(ctx, page, &render);

		/* Make the display list, and see if we need color */
		fz_try(ctx)
		{
			list = fz_new_display_list(ctx, render.bounds);
			list_dev = fz_new_list_device(ctx, list);
#if GREY_FALLBACK != 0
			test_dev = fz_new_test_device(ctx, &is_color, 0.01f, 0, list_dev);
			fz_run_page(ctx, page, test_dev, fz_identity, &cookie);
			fz_close_device(ctx, test_dev);
#else
			fz_run_page(ctx, page, list_dev, fz_identity, &cookie);
#endif
			fz_close_device(ctx, list_dev);
		}
		fz_always(ctx)
		{
#if GREY_FALLBACK != 0
			fz_drop_device(ctx, test_dev);
#endif
			fz_drop_device(ctx, list_dev);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			list = NULL;
			/* Just continue with no list. Also, we can't do multiple
			 * threads if we have no list. */
			render.num_workers = 1;
		}
		render.list = list;

#if GREY_FALLBACK != 0
		if (list == NULL)
		{
			/* We need to know about color, but the previous test failed
			 * (presumably) due to the size of the list. Rerun direct
			 * from file. */
			fz_try(ctx)
			{
				test_dev = fz_new_test_device(ctx, &is_color, 0.01f, 0, NULL);
				fz_run_page(ctx, page, test_dev, fz_identity, &cookie);
				fz_close_device(ctx, test_dev);
			}
			fz_always(ctx)
			{
				fz_drop_device(ctx, test_dev);
			}
			fz_catch(ctx)
			{
				/* We failed. Just give up. */
				fz_drop_page(ctx, page);
				fz_rethrow(ctx);
			}
		}
#endif

#if GREY_FALLBACK == 2
		/* If we 'possibly' need color, find out if we 'really' need color. */
		if (is_color == 1)
		{
			/* We know that the device has images or shadings in
			 * colored spaces. We have been told to test exhaustively
			 * so we know whether to use color or grey rendering. */
			is_color = 0;
			fz_try(ctx)
			{
				test_dev = fz_new_test_device(ctx, &is_color, 0.01f, FZ_TEST_OPT_IMAGES | FZ_TEST_OPT_SHADINGS, NULL);
				if (list)
					fz_run_display_list(ctx, list, test_dev, &fz_identity, &fz_infinite_rect, &cookie);
				else
					fz_run_page(ctx, page, test_dev, &fz_identity, &cookie);
				fz_close_device(ctx, test_dev);
			}
			fz_always(ctx)
			{
				fz_drop_device(ctx, test_dev);
			}
			fz_catch(ctx)
			{
				fz_drop_display_list(ctx, list);
				fz_drop_page(ctx, page);
				fz_rethrow(ctx);
			}
		}
#endif

		/* Figure out banding */
		initialise_banding(ctx, &render, is_color);

		if (bgprint.active && showtime)
		{
			int end = gettime();
			start = end - start;
		}

		/* If we're not using bgprint, then no need to wait */
		if (!bgprint.active)
			break;

		/* If we are using it, then wait for it to finish. */
		status = wait_for_bgprint_to_finish();
		if (status == RENDER_OK)
		{
			/* The background bgprint completed successfully. Drop out of the loop,
			 * and carry on with our next page. */
			break;
		}

		/* The bgprint in the background failed! This might have been because
		 * we were using memory etc in the foreground. We'd better ditch
		 * everything we can and try again. */
		fz_drop_display_list(ctx, list);
		fz_drop_page(ctx, page);

		if (status == RENDER_FATAL)
		{
			/* We failed because of not being able to output. No point in retrying. */
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to render page");
		}
		bgprint.started = 1;
		bgprint.solo = 1;
		mu_trigger_semaphore(&bgprint.start);
		status = wait_for_bgprint_to_finish();
		if (status != 0)
		{
			/* Hard failure */
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to render page");
		}
		/* Loop back to reload this page */
	}
	while (1);

	if (showtime)
	{
		fprintf(stderr, "page %s %d", filename, pagenum);
	}
	if (bgprint.active)
	{
		bgprint.started = 1;
		bgprint.solo = 0;
		bgprint.render = render;
		bgprint.filename = filename;
		bgprint.pagenum = pagenum;
		bgprint.interptime = start;
		mu_trigger_semaphore(&bgprint.start);
	}
	else
	{
		if (try_render_page(ctx, pagenum, &cookie, start, 0, filename, 0, 0, &render))
		{
			/* Hard failure */
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to render page");
		}
	}
}

/* Wait for the final page being printed by bgprint to complete,
 * retrying if necessary. */
static void
finish_bgprint(fz_context *ctx)
{
	int status;

	if (!bgprint.active)
		return;

	/* If we are using it, then wait for it to finish. */
	status = wait_for_bgprint_to_finish();
	if (status == RENDER_OK)
	{
		/* The background bgprint completed successfully. */
		return;
	}

	if (status == RENDER_FATAL)
	{
		/* We failed because of not being able to output. No point in retrying. */
		fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to render page");
	}
	bgprint.started = 1;
	bgprint.solo = 1;
	mu_trigger_semaphore(&bgprint.start);
	status = wait_for_bgprint_to_finish();
	if (status != 0)
	{
		/* Hard failure */
		fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to render page");
	}
}

static void drawrange(fz_context *ctx, fz_document *doc, const char *range)
{
	int page, spage, epage, pagecount;

	pagecount = fz_count_pages(ctx, doc);

	while ((range = fz_parse_page_range(ctx, range, &spage, &epage, pagecount)))
	{
		if (spage < epage)
			for (page = spage; page <= epage; page++)
				drawpage(ctx, doc, page);
		else
			for (page = spage; page >= epage; page--)
				drawpage(ctx, doc, page);
	}
}

typedef struct
{
	size_t size;
#if defined(_M_IA64) || defined(_M_AMD64)
	size_t align;
#endif
} trace_header;

typedef struct
{
	size_t current;
	size_t peak;
	size_t total;
} trace_info;

static void *
trace_malloc(void *arg, size_t size)
{
	trace_info *info = (trace_info *) arg;
	trace_header *p;
	if (size == 0)
		return NULL;
	p = malloc(size + sizeof(trace_header));
	if (p == NULL)
		return NULL;
	p[0].size = size;
	info->current += size;
	info->total += size;
	if (info->current > info->peak)
		info->peak = info->current;
	return (void *)&p[1];
}

static void
trace_free(void *arg, void *p_)
{
	trace_info *info = (trace_info *) arg;
	trace_header *p = (trace_header *)p_;

	if (p == NULL)
		return;
	info->current -= p[-1].size;
	free(&p[-1]);
}

static void *
trace_realloc(void *arg, void *p_, size_t size)
{
	trace_info *info = (trace_info *) arg;
	trace_header *p = (trace_header *)p_;
	size_t oldsize;

	if (size == 0)
	{
		trace_free(arg, p_);
		return NULL;
	}
	if (p == NULL)
		return trace_malloc(arg, size);
	oldsize = p[-1].size;
	p = realloc(&p[-1], size + sizeof(trace_header));
	if (p == NULL)
		return NULL;
	info->current += size - oldsize;
	if (size > oldsize)
		info->total += size - oldsize;
	if (info->current > info->peak)
		info->peak = info->current;
	p[0].size = size;
	return &p[1];
}

#ifndef DISABLE_MUTHREADS
static void worker_thread(void *arg)
{
	worker_t *me = (worker_t *)arg;

	do
	{
		DEBUG_THREADS(("Worker %d waiting\n", me->num));
		mu_wait_semaphore(&me->start);
		DEBUG_THREADS(("Worker %d woken for band_start %d\n", me->num, me->band_start));
		me->status = RENDER_OK;
		if (me->band_start >= 0)
			me->status = drawband(me->ctx, NULL, me->list, me->ctm, me->tbounds, &me->cookie, me->band_start, me->pix, &me->bit);
		DEBUG_THREADS(("Worker %d completed band_start %d (status=%d)\n", me->num, me->band_start, me->status));
		mu_trigger_semaphore(&me->stop);
	}
	while (me->band_start >= 0);
}

static void bgprint_worker(void *arg)
{
	fz_cookie cookie = { 0 };
	int pagenum;

	(void)arg;

	do
	{
		DEBUG_THREADS(("BGPrint waiting\n"));
		mu_wait_semaphore(&bgprint.start);
		pagenum = bgprint.pagenum;
		DEBUG_THREADS(("BGPrint woken for pagenum %d\n", pagenum));
		if (pagenum >= 0)
		{
			int start = gettime();
			memset(&cookie, 0, sizeof(cookie));
			bgprint.status = try_render_page(bgprint.ctx, pagenum, &cookie, start, bgprint.interptime, bgprint.filename, 1, bgprint.solo, &bgprint.render);
		}
		DEBUG_THREADS(("BGPrint completed page %d\n", pagenum));
		mu_trigger_semaphore(&bgprint.stop);
	}
	while (pagenum >= 0);
}
#endif

static void
read_resolution(const char *arg)
{
	char *sep = strchr(arg, ',');

	if (sep == NULL)
		sep = strchr(arg, 'x');
	if (sep == NULL)
		sep = strchr(arg, ':');
	if (sep == NULL)
		sep = strchr(arg, ';');

	x_resolution = fz_atoi(arg);
	if (sep && sep[1])
		y_resolution = fz_atoi(arg);
	else
		y_resolution = x_resolution;
}

static int
read_rotation(const char *arg)
{
	int i;

	if (strcmp(arg, "auto"))
	{
		return -1;
	}

	i = fz_atoi(arg);

	i = i % 360;
	if (i % 90 != 0)
	{
		fprintf(stderr, "Ignoring invalid rotation\n");
		i = 0;
	}

	return i;
}

int main(int argc, char **argv)
{
	char *password = "";
	fz_document *doc = NULL;
	int c;
	fz_context *ctx;
	trace_info info = { 0, 0, 0 };
	fz_alloc_context alloc_ctx = { &info, trace_malloc, trace_realloc, trace_free };
	fz_locks_context *locks = NULL;

	fz_var(doc);

	bgprint.active = 0;			/* set by -P */
	min_band_height = MIN_BAND_HEIGHT;
	max_band_memory = BAND_MEMORY;
	width = 0;
	height = 0;
	num_workers = NUM_RENDER_THREADS;
	x_resolution = X_RESOLUTION;
	y_resolution = Y_RESOLUTION;

	while ((c = fz_getopt(argc, argv, "p:o:F:R:r:w:h:fB:M:s:A:iW:H:S:T:U:XvP")) != -1)
	{
		switch (c)
		{
		default: usage(); break;

		case 'p': password = fz_optarg; break;

		case 'o': output = fz_optarg; break;
		case 'F': format = fz_optarg; break;

		case 'R': rotation = read_rotation(fz_optarg); break;
		case 'r': read_resolution(fz_optarg); break;
		case 'w': width = fz_atof(fz_optarg); break;
		case 'h': height = fz_atof(fz_optarg); break;
		case 'f': fit = 1; break;
		case 'B': min_band_height = atoi(fz_optarg); break;
		case 'M': max_band_memory = atoi(fz_optarg); break;

		case 'W': layout_w = fz_atof(fz_optarg); break;
		case 'H': layout_h = fz_atof(fz_optarg); break;
		case 'S': layout_em = fz_atof(fz_optarg); break;
		case 'U': layout_css = fz_optarg; break;
		case 'X': layout_use_doc_css = 0; break;

		case 's':
			if (strchr(fz_optarg, 't')) ++showtime;
			if (strchr(fz_optarg, 'm')) ++showmemory;
			break;

		case 'A':
		{
			char *sep;
			alphabits_graphics = atoi(fz_optarg);
			sep = strchr(fz_optarg, '/');
			if (sep)
				alphabits_text = atoi(sep+1);
			else
				alphabits_text = alphabits_graphics;
			break;
		}
		case 'i': ignore_errors = 1; break;

		case 'T':
#if MURASTER_THREADS != 0
			num_workers = atoi(fz_optarg); break;
#else
			fprintf(stderr, "Threads not enabled in this build\n");
			break;
#endif
		case 'P':
#if MURASTER_THREADS != 0
			bgprint.active = 1; break;
#else
			fprintf(stderr, "Threads not enabled in this build\n");
			break;
#endif
		case 'v': fprintf(stderr, "muraster version %s\n", FZ_VERSION); return 1;
		}
	}

	if (width == 0)
		width = x_resolution * PAPER_WIDTH;

	if (height == 0)
		height = y_resolution * PAPER_HEIGHT;

	if (fz_optind == argc)
		usage();

	if (min_band_height <= 0)
	{
		fprintf(stderr, "Require a positive minimum band height\n");
		exit(1);
	}

#ifndef DISABLE_MUTHREADS
	locks = init_muraster_locks();
	if (locks == NULL)
	{
		fprintf(stderr, "cannot initialise mutexes\n");
		exit(1);
	}
#endif

	ctx = fz_new_context((showmemory == 0 ? NULL : &alloc_ctx), locks, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_set_text_aa_level(ctx, alphabits_text);
	fz_set_graphics_aa_level(ctx, alphabits_graphics);

#ifndef DISABLE_MUTHREADS
	if (bgprint.active)
	{
		int fail = 0;
		bgprint.ctx = fz_clone_context(ctx);
		fail |= mu_create_semaphore(&bgprint.start);
		fail |= mu_create_semaphore(&bgprint.stop);
		fail |= mu_create_thread(&bgprint.thread, bgprint_worker, NULL);
		if (fail)
		{
			fprintf(stderr, "bgprint startup failed\n");
			exit(1);
		}
	}

	if (num_workers > 0)
	{
		int i;
		int fail = 0;
		workers = fz_calloc(ctx, num_workers, sizeof(*workers));
		for (i = 0; i < num_workers; i++)
		{
			workers[i].ctx = fz_clone_context(ctx);
			workers[i].num = i;
			fail |= mu_create_semaphore(&workers[i].start);
			fail |= mu_create_semaphore(&workers[i].stop);
			fail |= mu_create_thread(&workers[i].thread, worker_thread, &workers[i]);
		}
		if (fail)
		{
			fprintf(stderr, "worker startup failed\n");
			exit(1);
		}
	}
#endif /* DISABLE_MUTHREADS */

	if (layout_css)
	{
		fz_buffer *buf = fz_read_file(ctx, layout_css);
		fz_set_user_css(ctx, fz_string_from_buffer(ctx, buf));
		fz_drop_buffer(ctx, buf);
	}

	fz_set_use_document_css(ctx, layout_use_doc_css);

	output_format = suffix_table[0].format;
	output_cs = suffix_table[0].cs;
	if (format)
	{
		int i;

		for (i = 0; i < (int)nelem(suffix_table); i++)
		{
			if (!strcmp(format, suffix_table[i].suffix+1))
			{
				output_format = suffix_table[i].format;
				output_cs = suffix_table[i].cs;
				break;
			}
		}
		if (i == (int)nelem(suffix_table))
		{
			fprintf(stderr, "Unknown output format '%s'\n", format);
			exit(1);
		}
	}
	else if (output)
	{
		char *suffix = output;
		int i;

		for (i = 0; i < (int)nelem(suffix_table); i++)
		{
			char *s = strstr(suffix, suffix_table[i].suffix);

			if (s != NULL)
			{
				suffix = s+1;
				output_format = suffix_table[i].format;
				output_cs = suffix_table[i].cs;
				i = 0;
			}
		}
	}

	switch (output_cs)
	{
	case CS_GRAY:
		colorspace = fz_device_gray(ctx);
		break;
	case CS_RGB:
		colorspace = fz_device_rgb(ctx);
		break;
	case CS_CMYK:
		colorspace = fz_device_cmyk(ctx);
		break;
	}

	if (output && (output[0] != '-' || output[1] != 0) && *output != 0)
	{
		out = fz_new_output_with_path(ctx, output, 0);
	}
	else
		out = fz_stdout(ctx);

	timing.count = 0;
	timing.total = 0;
	timing.min = 1 << 30;
	timing.max = 0;
	timing.mininterp = 1 << 30;
	timing.maxinterp = 0;
	timing.minpage = 0;
	timing.maxpage = 0;
	timing.minfilename = "";
	timing.maxfilename = "";

	fz_try(ctx)
	{
		fz_register_document_handlers(ctx);

		while (fz_optind < argc)
		{
			fz_try(ctx)
			{
				filename = argv[fz_optind++];

				doc = fz_open_document(ctx, filename);

				if (fz_needs_password(ctx, doc))
				{
					if (!fz_authenticate_password(ctx, doc, password))
						fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", filename);
				}

				fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);

				if (fz_optind == argc || !fz_is_page_range(ctx, argv[fz_optind]))
					drawrange(ctx, doc, "1-N");
				if (fz_optind < argc && fz_is_page_range(ctx, argv[fz_optind]))
					drawrange(ctx, doc, argv[fz_optind++]);

				fz_drop_document(ctx, doc);
				doc = NULL;
			}
			fz_catch(ctx)
			{
				if (!ignore_errors)
					fz_rethrow(ctx);

				fz_drop_document(ctx, doc);
				doc = NULL;
				fz_warn(ctx, "ignoring error in '%s'", filename);
			}
		}
		finish_bgprint(ctx);
	}
	fz_catch(ctx)
	{
		fz_drop_document(ctx, doc);
		fprintf(stderr, "error: cannot draw '%s'\n", filename);
		errored = 1;
	}

	if (showtime && timing.count > 0)
	{
		fprintf(stderr, "total %dms / %d pages for an average of %dms\n",
			timing.total, timing.count, timing.total / timing.count);
		fprintf(stderr, "fastest page %d: %dms\n", timing.minpage, timing.min);
		fprintf(stderr, "slowest page %d: %dms\n", timing.maxpage, timing.max);
	}

#ifndef DISABLE_MUTHREADS
	if (num_workers > 0)
	{
		int i;
		for (i = 0; i < num_workers; i++)
		{
			workers[i].band_start = -1;
			mu_trigger_semaphore(&workers[i].start);
			mu_wait_semaphore(&workers[i].stop);
			mu_destroy_semaphore(&workers[i].start);
			mu_destroy_semaphore(&workers[i].stop);
			mu_destroy_thread(&workers[i].thread);
			fz_drop_context(workers[i].ctx);
		}
		fz_free(ctx, workers);
	}

	if (bgprint.active)
	{
		bgprint.pagenum = -1;
		mu_trigger_semaphore(&bgprint.start);
		mu_wait_semaphore(&bgprint.stop);
		mu_destroy_semaphore(&bgprint.start);
		mu_destroy_semaphore(&bgprint.stop);
		mu_destroy_thread(&bgprint.thread);
		fz_drop_context(bgprint.ctx);
	}
#endif /* DISABLE_MUTHREADS */

	fz_close_output(ctx, out);
	fz_drop_output(ctx, out);
	out = NULL;

	fz_drop_context(ctx);
#ifndef DISABLE_MUTHREADS
	fin_muraster_locks();
#endif /* DISABLE_MUTHREADS */

	if (showmemory)
	{
		char buf[100];
		fz_snprintf(buf, sizeof buf, "Memory use total=%zu peak=%zu current=%zu", info.total, info.peak, info.current);
		fprintf(stderr, "%s\n", buf);
	}

	return (errored != 0);
}
