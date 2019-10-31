/*
 * mudraw -- command line tool for drawing and converting documents
 */

#include "mupdf/fitz.h"

#if FZ_ENABLE_PDF
#include "mupdf/pdf.h" /* for pdf output */
#endif

#ifndef DISABLE_MUTHREADS
#include "mupdf/helpers/mu-threads.h"
#endif

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

/* Allow for windows stdout being made binary */
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

/* Enable for helpful threading debug */
/* #define DEBUG_THREADS(A) do { printf A; fflush(stdout); } while (0) */
#define DEBUG_THREADS(A) do { } while (0)

enum {
	OUT_NONE,
	OUT_PNG, OUT_PNM, OUT_PGM, OUT_PPM, OUT_PAM,
	OUT_PBM, OUT_PKM, OUT_PWG, OUT_PCL, OUT_PS, OUT_PSD,
	OUT_TEXT, OUT_HTML, OUT_XHTML, OUT_STEXT, OUT_PCLM,
	OUT_TRACE, OUT_SVG,
#if FZ_ENABLE_PDF
	OUT_PDF,
#endif
};

enum { CS_INVALID, CS_UNSET, CS_MONO, CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA, CS_ICC };

enum { SPOTS_NONE, SPOTS_OVERPRINT_SIM, SPOTS_FULL };

typedef struct
{
	char *suffix;
	int format;
	int spots;
} suffix_t;

static const suffix_t suffix_table[] =
{
	{ ".png", OUT_PNG, 0 },
	{ ".pgm", OUT_PGM, 0 },
	{ ".ppm", OUT_PPM, 0 },
	{ ".pnm", OUT_PNM, 0 },
	{ ".pam", OUT_PAM, 0 },
	{ ".pbm", OUT_PBM, 0 },
	{ ".pkm", OUT_PKM, 0 },
	{ ".svg", OUT_SVG, 0 },
	{ ".pwg", OUT_PWG, 0 },
	{ ".pclm", OUT_PCLM, 0 },
	{ ".pcl", OUT_PCL, 0 },
#if FZ_ENABLE_PDF
	{ ".pdf", OUT_PDF, 0 },
#endif
	{ ".psd", OUT_PSD, 1 },
	{ ".ps", OUT_PS, 0 },

	{ ".txt", OUT_TEXT, 0 },
	{ ".text", OUT_TEXT, 0 },
	{ ".html", OUT_HTML, 0 },
	{ ".xhtml", OUT_XHTML, 0 },
	{ ".stext", OUT_STEXT, 0 },

	{ ".trace", OUT_TRACE, 0 },
};

typedef struct
{
	char *name;
	int colorspace;
} cs_name_t;

static const cs_name_t cs_name_table[] =
{
	{ "m", CS_MONO },
	{ "mono", CS_MONO },
	{ "g", CS_GRAY },
	{ "gray", CS_GRAY },
	{ "grey", CS_GRAY },
	{ "ga", CS_GRAY_ALPHA },
	{ "grayalpha", CS_GRAY_ALPHA },
	{ "greyalpha", CS_GRAY_ALPHA },
	{ "rgb", CS_RGB },
	{ "rgba", CS_RGB_ALPHA },
	{ "rgbalpha", CS_RGB_ALPHA },
	{ "cmyk", CS_CMYK },
	{ "cmyka", CS_CMYK_ALPHA },
	{ "cmykalpha", CS_CMYK_ALPHA },
};

typedef struct
{
	int format;
	int default_cs;
	int permitted_cs[7];
} format_cs_table_t;

static const format_cs_table_t format_cs_table[] =
{
	{ OUT_PNG, CS_RGB, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_ICC } },
	{ OUT_PPM, CS_RGB, { CS_GRAY, CS_RGB } },
	{ OUT_PNM, CS_GRAY, { CS_GRAY, CS_RGB } },
	{ OUT_PAM, CS_RGB_ALPHA, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA } },
	{ OUT_PGM, CS_GRAY, { CS_GRAY, CS_RGB } },
	{ OUT_PBM, CS_MONO, { CS_MONO } },
	{ OUT_PKM, CS_CMYK, { CS_CMYK } },
	{ OUT_PWG, CS_RGB, { CS_MONO, CS_GRAY, CS_RGB, CS_CMYK } },
	{ OUT_PCL, CS_MONO, { CS_MONO, CS_RGB } },
	{ OUT_PCLM, CS_RGB, { CS_RGB, CS_GRAY } },
	{ OUT_PS, CS_RGB, { CS_GRAY, CS_RGB, CS_CMYK } },
	{ OUT_PSD, CS_CMYK, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA, CS_ICC } },

	{ OUT_TRACE, CS_RGB, { CS_RGB } },
	{ OUT_SVG, CS_RGB, { CS_RGB } },
#if FZ_ENABLE_PDF
	{ OUT_PDF, CS_RGB, { CS_RGB } },
#endif

	{ OUT_TEXT, CS_RGB, { CS_RGB } },
	{ OUT_HTML, CS_RGB, { CS_RGB } },
	{ OUT_XHTML, CS_RGB, { CS_RGB } },
	{ OUT_STEXT, CS_RGB, { CS_RGB } },
};

/*
	In the presence of pthreads or Windows threads, we can offer
	a multi-threaded option. In the absence, of such, we degrade
	nicely.
*/
#ifndef DISABLE_MUTHREADS

static mu_mutex mutexes[FZ_LOCK_MAX];

static void mudraw_lock(void *user, int lock)
{
	mu_lock_mutex(&mutexes[lock]);
}

static void mudraw_unlock(void *user, int lock)
{
	mu_unlock_mutex(&mutexes[lock]);
}

static fz_locks_context mudraw_locks =
{
	NULL, mudraw_lock, mudraw_unlock
};

static void fin_mudraw_locks(void)
{
	int i;

	for (i = 0; i < FZ_LOCK_MAX; i++)
		mu_destroy_mutex(&mutexes[i]);
}

static fz_locks_context *init_mudraw_locks(void)
{
	int i;
	int failed = 0;

	for (i = 0; i < FZ_LOCK_MAX; i++)
		failed |= mu_create_mutex(&mutexes[i]);

	if (failed)
	{
		fin_mudraw_locks();
		return NULL;
	}

	return &mudraw_locks;
}

#endif

typedef struct worker_t {
	fz_context *ctx;
	int num;
	int band; /* -1 to shutdown, or band to render */
	fz_display_list *list;
	fz_matrix ctm;
	fz_rect tbounds;
	fz_pixmap *pix;
	fz_bitmap *bit;
	fz_cookie cookie;
#ifndef DISABLE_MUTHREADS
	mu_semaphore start;
	mu_semaphore stop;
	mu_thread thread;
#endif
} worker_t;

static char *output = NULL;
static fz_output *out = NULL;
static int output_pagenum = 0;
static int output_file_per_page = 0;

static char *format = NULL;
static int output_format = OUT_NONE;

static float rotation = 0;
static float resolution = 72;
static int res_specified = 0;
static int width = 0;
static int height = 0;
static int fit = 0;

static float layout_w = FZ_DEFAULT_LAYOUT_W;
static float layout_h = FZ_DEFAULT_LAYOUT_H;
static float layout_em = FZ_DEFAULT_LAYOUT_EM;
static char *layout_css = NULL;
static int layout_use_doc_css = 1;
static float min_line_width = 0.0f;

static int showfeatures = 0;
static int showtime = 0;
static int showmemory = 0;
static int showmd5 = 0;

#if FZ_ENABLE_PDF
static pdf_document *pdfout = NULL;
#endif

static int no_icc = 0;
static int ignore_errors = 0;
static int uselist = 1;
static int alphabits_text = 8;
static int alphabits_graphics = 8;

static int out_cs = CS_UNSET;
static const char *proof_filename = NULL;
fz_colorspace *proof_cs = NULL;
static const char *icc_filename = NULL;
static float gamma_value = 1;
static int invert = 0;
static int band_height = 0;
static int lowmemory = 0;

static int quiet = 0;
static int errored = 0;
static fz_colorspace *colorspace = NULL;
static fz_colorspace *oi = NULL;
#if FZ_ENABLE_SPOT_RENDERING
static int spots = SPOTS_OVERPRINT_SIM;
#else
static int spots = SPOTS_NONE;
#endif
static int alpha;
static char *filename;
static int files = 0;
static int num_workers = 0;
static worker_t *workers;
static fz_band_writer *bander = NULL;

static const char *layer_config = NULL;

static struct {
	int active;
	int started;
	fz_context *ctx;
#ifndef DISABLE_MUTHREADS
	mu_thread thread;
	mu_semaphore start;
	mu_semaphore stop;
#endif
	int pagenum;
	char *filename;
	fz_display_list *list;
	fz_page *page;
	int interptime;
	fz_separations *seps;
} bgprint;

static struct {
	int count, total;
	int min, max;
	int mininterp, maxinterp;
	int minpage, maxpage;
	char *minfilename;
	char *maxfilename;
} timing;

static void usage(void)
{
	fprintf(stderr,
		"mudraw version " FZ_VERSION "\n"
		"Usage: mudraw [options] file [pages]\n"
		"\t-p -\tpassword\n"
		"\n"
		"\t-o -\toutput file name (%%d for page number)\n"
		"\t-F -\toutput format (default inferred from output file name)\n"
		"\t\traster: png, pnm, pam, pbm, pkm, pwg, pcl, ps\n"
		"\t\tvector: svg, pdf, trace\n"
		"\t\ttext: txt, html, stext\n"
		"\n"
		"\t-q\tbe quiet (don't print progress messages)\n"
		"\t-s -\tshow extra information:\n"
		"\t\tm - show memory use\n"
		"\t\tt - show timings\n"
		"\t\tf - show page features\n"
		"\t\t5 - show md5 checksum of rendered image\n"
		"\n"
		"\t-R -\trotate clockwise (default: 0 degrees)\n"
		"\t-r -\tresolution in dpi (default: 72)\n"
		"\t-w -\twidth (in pixels) (maximum width if -r is specified)\n"
		"\t-h -\theight (in pixels) (maximum height if -r is specified)\n"
		"\t-f -\tfit width and/or height exactly; ignore original aspect ratio\n"
		"\t-B -\tmaximum band_height (pXm, pcl, pclm, ps, psd and png output only)\n"
#ifndef DISABLE_MUTHREADS
		"\t-T -\tnumber of threads to use for rendering (banded mode only)\n"
#else
		"\t-T -\tnumber of threads to use for rendering (disabled in this non-threading build)\n"
#endif
		"\n"
		"\t-W -\tpage width for EPUB layout\n"
		"\t-H -\tpage height for EPUB layout\n"
		"\t-S -\tfont size for EPUB layout\n"
		"\t-U -\tfile name of user stylesheet for EPUB layout\n"
		"\t-X\tdisable document styles for EPUB layout\n"
		"\n"
		"\t-c -\tcolorspace (mono, gray, grayalpha, rgb, rgba, cmyk, cmykalpha, filename of ICC profile)\n"
		"\t-e -\tproof icc profile (filename of ICC profile)\n"
		"\t-G -\tapply gamma correction\n"
		"\t-I\tinvert colors\n"
		"\n"
		"\t-A -\tnumber of bits of antialiasing (0 to 8)\n"
		"\t-A -/-\tnumber of bits of antialiasing (0 to 8) (graphics, text)\n"
		"\t-l -\tminimum stroked line width (in pixels)\n"
		"\t-D\tdisable use of display list\n"
		"\t-i\tignore errors\n"
		"\t-L\tlow memory mode (avoid caching, clear objects after each page)\n"
#ifndef DISABLE_MUTHREADS
		"\t-P\tparallel interpretation/rendering\n"
#else
		"\t-P\tparallel interpretation/rendering (disabled in this non-threading build)\n"
#endif
		"\t-N\tdisable ICC workflow (\"N\"o color management)\n"
		"\t-O -\tControl spot/overprint rendering\n"
#if FZ_ENABLE_SPOT_RENDERING
		"\t\t 0 = No spot rendering\n"
		"\t\t 1 = Overprint simulation (default)\n"
		"\t\t 2 = Full spot rendering\n"
#else
		"\t\t 0 = No spot rendering (default)\n"
		"\t\t 1 = Overprint simulation (Disabled in this build)\n"
		"\t\t 2 = Full spot rendering (Disabled in this build)\n"
#endif
		"\n"
		"\t-y l\tList the layer configs to stderr\n"
		"\t-y -\tSelect layer config (by number)\n"
		"\t-y -{,-}*\tSelect layer config (by number), and toggle the listed entries\n"
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

static int has_percent_d(char *s)
{
	/* find '%[0-9]*d' */
	while (*s)
	{
		if (*s++ == '%')
		{
			while (*s >= '0' && *s <= '9')
				++s;
			if (*s == 'd')
				return 1;
		}
	}
	return 0;
}

/* Output file level (as opposed to page level) headers */
static void
file_level_headers(fz_context *ctx)
{
	if (output_format == OUT_STEXT || output_format == OUT_TRACE)
		fz_write_printf(ctx, out, "<?xml version=\"1.0\"?>\n");

	if (output_format == OUT_HTML)
		fz_print_stext_header_as_html(ctx, out);
	if (output_format == OUT_XHTML)
		fz_print_stext_header_as_xhtml(ctx, out);

	if (output_format == OUT_STEXT || output_format == OUT_TRACE)
		fz_write_printf(ctx, out, "<document name=\"%s\">\n", filename);

	if (output_format == OUT_PS)
		fz_write_ps_file_header(ctx, out);

	if (output_format == OUT_PWG)
		fz_write_pwg_file_header(ctx, out);

	if (output_format == OUT_PCLM)
	{
		fz_pclm_options opts = { 0 };
		fz_parse_pclm_options(ctx, &opts, "compression=flate");
		bander = fz_new_pclm_band_writer(ctx, out, &opts);
	}
}

static void
file_level_trailers(fz_context *ctx)
{
	if (output_format == OUT_STEXT || output_format == OUT_TRACE)
		fz_write_printf(ctx, out, "</document>\n");

	if (output_format == OUT_HTML)
		fz_print_stext_trailer_as_html(ctx, out);
	if (output_format == OUT_XHTML)
		fz_print_stext_trailer_as_xhtml(ctx, out);

	if (output_format == OUT_PS)
		fz_write_ps_file_trailer(ctx, out, output_pagenum);

	if (output_format == OUT_PCLM)
		fz_drop_band_writer(ctx, bander);
}

static void drawband(fz_context *ctx, fz_page *page, fz_display_list *list, fz_matrix ctm, fz_rect tbounds, fz_cookie *cookie, int band_start, fz_pixmap *pix, fz_bitmap **bit)
{
	fz_device *dev = NULL;

	fz_var(dev);

	*bit = NULL;

	fz_try(ctx)
	{
		if (pix->alpha)
			fz_clear_pixmap(ctx, pix);
		else
			fz_clear_pixmap_with_value(ctx, pix, 255);

		dev = fz_new_draw_device_with_proof(ctx, fz_identity, pix, proof_cs);
		if (lowmemory)
			fz_enable_device_hints(ctx, dev, FZ_NO_CACHE);
		if (alphabits_graphics == 0)
			fz_enable_device_hints(ctx, dev, FZ_DONT_INTERPOLATE_IMAGES);
		if (list)
			fz_run_display_list(ctx, list, dev, ctm, tbounds, cookie);
		else
			fz_run_page(ctx, page, dev, ctm, cookie);
		fz_close_device(ctx, dev);
		fz_drop_device(ctx, dev);
		dev = NULL;

		if (invert)
			fz_invert_pixmap(ctx, pix);
		if (gamma_value != 1)
			fz_gamma_pixmap(ctx, pix, gamma_value);

		if (((output_format == OUT_PCL || output_format == OUT_PWG) && out_cs == CS_MONO) || (output_format == OUT_PBM) || (output_format == OUT_PKM))
			*bit = fz_new_bitmap_from_pixmap_band(ctx, pix, NULL, band_start);
	}
	fz_catch(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_rethrow(ctx);
	}
}

static void dodrawpage(fz_context *ctx, fz_page *page, fz_display_list *list, int pagenum, fz_cookie *cookie, int start, int interptime, char *filename, int bg, fz_separations *seps)
{
	fz_rect mediabox;
	fz_device *dev = NULL;

	fz_var(dev);

	if (output_file_per_page)
		file_level_headers(ctx);

	fz_try(ctx)
	{
		if (list)
			mediabox = fz_bound_display_list(ctx, list);
		else
			mediabox = fz_bound_page(ctx, page);
	}
	fz_catch(ctx)
	{
		fz_drop_display_list(ctx, list);
		fz_drop_separations(ctx, seps);
		fz_drop_page(ctx, page);
		fz_rethrow(ctx);
	}

	if (output_format == OUT_TRACE)
	{
		fz_try(ctx)
		{
			fz_write_printf(ctx, out, "<page mediabox=\"%g %g %g %g\">\n",
					mediabox.x0, mediabox.y0, mediabox.x1, mediabox.y1);
			dev = fz_new_trace_device(ctx, out);
			if (lowmemory)
				fz_enable_device_hints(ctx, dev, FZ_NO_CACHE);
			if (list)
				fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, cookie);
			else
				fz_run_page(ctx, page, dev, fz_identity, cookie);
			fz_write_printf(ctx, out, "</page>\n");
			fz_close_device(ctx, dev);
		}
		fz_always(ctx)
		{
			fz_drop_device(ctx, dev);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_drop_separations(ctx, seps);
			fz_drop_page(ctx, page);
			fz_rethrow(ctx);
		}
	}

	else if (output_format == OUT_TEXT || output_format == OUT_HTML || output_format == OUT_XHTML || output_format == OUT_STEXT)
	{
		fz_stext_page *text = NULL;
		float zoom;
		fz_matrix ctm;

		zoom = resolution / 72;
		ctm = fz_pre_scale(fz_rotate(rotation), zoom, zoom);

		fz_var(text);

		fz_try(ctx)
		{
			fz_stext_options stext_options;

			stext_options.flags = (output_format == OUT_HTML || output_format == OUT_XHTML) ? FZ_STEXT_PRESERVE_IMAGES : 0;
			text = fz_new_stext_page(ctx, mediabox);
			dev = fz_new_stext_device(ctx,  text, &stext_options);
			if (lowmemory)
				fz_enable_device_hints(ctx, dev, FZ_NO_CACHE);
			if (list)
				fz_run_display_list(ctx, list, dev, ctm, fz_infinite_rect, cookie);
			else
				fz_run_page(ctx, page, dev, ctm, cookie);
			fz_close_device(ctx, dev);
			fz_drop_device(ctx, dev);
			dev = NULL;
			if (output_format == OUT_STEXT)
			{
				fz_print_stext_page_as_xml(ctx, out, text, pagenum);
			}
			else if (output_format == OUT_HTML)
			{
				fz_print_stext_page_as_html(ctx, out, text, pagenum);
			}
			else if (output_format == OUT_XHTML)
			{
				fz_print_stext_page_as_xhtml(ctx, out, text, pagenum);
			}
			else if (output_format == OUT_TEXT)
			{
				fz_print_stext_page_as_text(ctx, out, text);
				fz_write_printf(ctx, out, "\f\n");
			}
		}
		fz_always(ctx)
		{
			fz_drop_device(ctx, dev);
			fz_drop_stext_page(ctx, text);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_drop_separations(ctx, seps);
			fz_drop_page(ctx, page);
			fz_rethrow(ctx);
		}
	}

#if FZ_ENABLE_PDF
	else if (output_format == OUT_PDF)
	{
		fz_buffer *contents = NULL;
		pdf_obj *resources = NULL;

		fz_var(contents);
		fz_var(resources);

		fz_try(ctx)
		{
			pdf_obj *page_obj;

			dev = pdf_page_write(ctx, pdfout, mediabox, &resources, &contents);
			if (list)
				fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, cookie);
			else
				fz_run_page(ctx, page, dev, fz_identity, cookie);
			fz_close_device(ctx, dev);
			fz_drop_device(ctx, dev);
			dev = NULL;

			page_obj = pdf_add_page(ctx, pdfout, mediabox, rotation, resources, contents);
			pdf_insert_page(ctx, pdfout, -1, page_obj);
			pdf_drop_obj(ctx, page_obj);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(ctx, resources);
			fz_drop_buffer(ctx, contents);
			fz_drop_device(ctx, dev);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_drop_separations(ctx, seps);
			fz_drop_page(ctx, page);
			fz_rethrow(ctx);
		}
	}
#endif

	else if (output_format == OUT_SVG)
	{
		float zoom;
		fz_matrix ctm;
		fz_rect tbounds;
		char buf[512];
		fz_output *out = NULL;

		fz_var(out);

		zoom = resolution / 72;
		ctm = fz_pre_rotate(fz_scale(zoom, zoom), rotation);
		tbounds = fz_transform_rect(mediabox, ctm);

		fz_try(ctx)
		{
			if (!output || !strcmp(output, "-"))
				out = fz_stdout(ctx);
			else
			{
				fz_snprintf(buf, sizeof(buf), output, pagenum);
				out = fz_new_output_with_path(ctx, buf, 0);
			}

			dev = fz_new_svg_device(ctx, out, tbounds.x1-tbounds.x0, tbounds.y1-tbounds.y0, FZ_SVG_TEXT_AS_PATH, 1);
			if (lowmemory)
				fz_enable_device_hints(ctx, dev, FZ_NO_CACHE);
			if (list)
				fz_run_display_list(ctx, list, dev, ctm, tbounds, cookie);
			else
				fz_run_page(ctx, page, dev, ctm, cookie);
			fz_close_device(ctx, dev);
			fz_close_output(ctx, out);
		}
		fz_always(ctx)
		{
			fz_drop_device(ctx, dev);
			fz_drop_output(ctx, out);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_drop_separations(ctx, seps);
			fz_drop_page(ctx, page);
			fz_rethrow(ctx);
		}
	}
	else
	{
		float zoom;
		fz_matrix ctm;
		fz_rect tbounds;
		fz_irect ibounds;
		fz_pixmap *pix = NULL;
		int w, h;
		fz_bitmap *bit = NULL;

		fz_var(pix);
		fz_var(bander);
		fz_var(bit);

		zoom = resolution / 72;
		ctm = fz_pre_scale(fz_rotate(rotation), zoom, zoom);

		tbounds = fz_transform_rect(mediabox, ctm);
		ibounds = fz_round_rect(tbounds);

		/* Make local copies of our width/height */
		w = width;
		h = height;

		/* If a resolution is specified, check to see whether w/h are
		 * exceeded; if not, unset them. */
		if (res_specified)
		{
			int t;
			t = ibounds.x1 - ibounds.x0;
			if (w && t <= w)
				w = 0;
			t = ibounds.y1 - ibounds.y0;
			if (h && t <= h)
				h = 0;
		}

		/* Now w or h will be 0 unless they need to be enforced. */
		if (w || h)
		{
			float scalex = w / (tbounds.x1 - tbounds.x0);
			float scaley = h / (tbounds.y1 - tbounds.y0);
			fz_matrix scale_mat;

			if (fit)
			{
				if (w == 0)
					scalex = 1.0f;
				if (h == 0)
					scaley = 1.0f;
			}
			else
			{
				if (w == 0)
					scalex = scaley;
				if (h == 0)
					scaley = scalex;
			}
			if (!fit)
			{
				if (scalex > scaley)
					scalex = scaley;
				else
					scaley = scalex;
			}
			scale_mat = fz_scale(scalex, scaley);
			ctm = fz_concat(ctm, scale_mat);
			tbounds = fz_transform_rect(mediabox, ctm);
		}
		ibounds = fz_round_rect(tbounds);
		tbounds = fz_rect_from_irect(ibounds);

		fz_try(ctx)
		{
			fz_irect band_ibounds = ibounds;
			int band, bands = 1;
			int totalheight = ibounds.y1 - ibounds.y0;
			int drawheight = totalheight;

			if (band_height != 0)
			{
				/* Banded rendering; we'll only render to a
				 * given height at a time. */
				drawheight = band_height;
				if (totalheight > band_height)
					band_ibounds.y1 = band_ibounds.y0 + band_height;
				bands = (totalheight + band_height-1)/band_height;
				tbounds.y1 = tbounds.y0 + band_height + 2;
				DEBUG_THREADS(("Using %d Bands\n", bands));
			}

			if (num_workers > 0)
			{
				for (band = 0; band < fz_mini(num_workers, bands); band++)
				{
					workers[band].band = band;
					workers[band].ctm = ctm;
					workers[band].tbounds = tbounds;
					memset(&workers[band].cookie, 0, sizeof(fz_cookie));
					workers[band].list = list;
					workers[band].pix = fz_new_pixmap_with_bbox(ctx, colorspace, band_ibounds, seps, alpha);
					fz_set_pixmap_resolution(ctx, workers[band].pix, resolution, resolution);
#ifndef DISABLE_MUTHREADS
					DEBUG_THREADS(("Worker %d, Pre-triggering band %d\n", band, band));
					mu_trigger_semaphore(&workers[band].start);
#endif
					ctm.f -= drawheight;
				}
				pix = workers[0].pix;
			}
			else
			{
				pix = fz_new_pixmap_with_bbox(ctx, colorspace, band_ibounds, seps, alpha);
				fz_set_pixmap_resolution(ctx, pix, resolution, resolution);
			}

			/* Output any page level headers (for banded formats) */
			if (output)
			{
				if (output_format == OUT_PGM || output_format == OUT_PPM || output_format == OUT_PNM)
					bander = fz_new_pnm_band_writer(ctx, out);
				else if (output_format == OUT_PAM)
					bander = fz_new_pam_band_writer(ctx, out);
				else if (output_format == OUT_PNG)
					bander = fz_new_png_band_writer(ctx, out);
				else if (output_format == OUT_PBM)
					bander = fz_new_pbm_band_writer(ctx, out);
				else if (output_format == OUT_PKM)
					bander = fz_new_pkm_band_writer(ctx, out);
				else if (output_format == OUT_PS)
					bander = fz_new_ps_band_writer(ctx, out);
				else if (output_format == OUT_PSD)
					bander = fz_new_psd_band_writer(ctx, out);
				else if (output_format == OUT_PWG)
				{
					if (out_cs == CS_MONO)
						bander = fz_new_mono_pwg_band_writer(ctx, out, NULL);
					else
						bander = fz_new_pwg_band_writer(ctx, out, NULL);
				}
				else if (output_format == OUT_PCL)
				{
					if (out_cs == CS_MONO)
						bander = fz_new_mono_pcl_band_writer(ctx, out, NULL);
					else
						bander = fz_new_color_pcl_band_writer(ctx, out, NULL);
				}
				if (bander)
				{
					fz_write_header(ctx, bander, pix->w, totalheight, pix->n, pix->alpha, pix->xres, pix->yres, output_pagenum++, pix->colorspace, pix->seps);
				}
			}

			for (band = 0; band < bands; band++)
			{
				if (num_workers > 0)
				{
					worker_t *w = &workers[band % num_workers];
#ifndef DISABLE_MUTHREADS
					DEBUG_THREADS(("Waiting for worker %d to complete band %d\n", w->num, band));
					mu_wait_semaphore(&w->stop);
#endif
					pix = w->pix;
					bit = w->bit;
					w->bit = NULL;
					cookie->errors += w->cookie.errors;
				}
				else
					drawband(ctx, page, list, ctm, tbounds, cookie, band * band_height, pix, &bit);

				if (output)
				{
					if (bander)
						fz_write_band(ctx, bander, bit ? bit->stride : pix->stride, drawheight, bit ? bit->samples : pix->samples);
					fz_drop_bitmap(ctx, bit);
					bit = NULL;
				}

				if (num_workers > 0 && band + num_workers < bands)
				{
					worker_t *w = &workers[band % num_workers];
					w->band = band + num_workers;
					w->ctm = ctm;
					w->tbounds = tbounds;
					memset(&w->cookie, 0, sizeof(fz_cookie));
#ifndef DISABLE_MUTHREADS
					DEBUG_THREADS(("Triggering worker %d for band %d\n", w->num, w->band));
					mu_trigger_semaphore(&w->start);
#endif
				}
				ctm.f -= drawheight;
			}

			/* FIXME */
			if (showmd5)
			{
				unsigned char digest[16];
				int i;

				fz_md5_pixmap(ctx, pix, digest);
				fprintf(stderr, " ");
				for (i = 0; i < 16; i++)
					fprintf(stderr, "%02x", digest[i]);
			}
		}
		fz_always(ctx)
		{
			if (output_format != OUT_PCLM)
				fz_drop_band_writer(ctx, bander);
			fz_drop_bitmap(ctx, bit);
			bit = NULL;
			if (num_workers > 0)
			{
				int band;
				for (band = 0; band < num_workers; band++)
					fz_drop_pixmap(ctx, workers[band].pix);
			}
			else
				fz_drop_pixmap(ctx, pix);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_drop_separations(ctx, seps);
			fz_drop_page(ctx, page);
			fz_rethrow(ctx);
		}
	}

	fz_drop_display_list(ctx, list);

	if (output_file_per_page)
		file_level_trailers(ctx);

	fz_drop_separations(ctx, seps);

	fz_drop_page(ctx, page);

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
			timing.count ++;

			fprintf(stderr, " %dms (interpretation) %dms (rendering) %dms (total)", interptime, diff, diff + interptime);
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

			fprintf(stderr, " %dms", diff);
		}
	}

	if (!quiet || showfeatures || showtime || showmd5)
		fprintf(stderr, "\n");

	if (lowmemory)
		fz_empty_store(ctx);

	if (showmemory)
		fz_dump_glyph_cache_stats(ctx);

	fz_flush_warnings(ctx);

	if (cookie->errors)
		errored = 1;
}

static void bgprint_flush(void)
{
	if (!bgprint.active || !bgprint.started)
		return;

#ifndef DISABLE_MUTHREADS
	mu_wait_semaphore(&bgprint.stop);
#endif
	bgprint.started = 0;
}

static void drawpage(fz_context *ctx, fz_document *doc, int pagenum)
{
	fz_page *page;
	fz_display_list *list = NULL;
	fz_device *dev = NULL;
	int start;
	fz_cookie cookie = { 0 };
	fz_separations *seps = NULL;
	const char *features = "";

	fz_var(list);
	fz_var(dev);
	fz_var(seps);

	start = (showtime ? gettime() : 0);

	page = fz_load_page(ctx, doc, pagenum - 1);

	if (spots != SPOTS_NONE)
	{
		fz_try(ctx)
		{
			seps = fz_page_separations(ctx, page);
			if (seps)
			{
				int i, n = fz_count_separations(ctx, seps);
				if (spots == SPOTS_FULL)
					for (i = 0; i < n; i++)
						fz_set_separation_behavior(ctx, seps, i, FZ_SEPARATION_SPOT);
				else
					for (i = 0; i < n; i++)
						fz_set_separation_behavior(ctx, seps, i, FZ_SEPARATION_COMPOSITE);
			}
			else if (fz_page_uses_overprint(ctx, page))
			{
				/* This page uses overprint, so we need an empty
				 * sep object to force the overprint simulation on. */
				seps = fz_new_separations(ctx, 0);
			}
			else if (oi && fz_colorspace_n(ctx, oi) != fz_colorspace_n(ctx, colorspace))
			{
				/* We have an output intent, and it's incompatible
				 * with the colorspace our device needs. Force the
				 * overprint simulation on, because this ensures that
				 * we 'simulate' the output intent too. */
				seps = fz_new_separations(ctx, 0);
			}
		}
		fz_catch(ctx)
		{
			fz_drop_page(ctx, page);
			fz_rethrow(ctx);
		}
	}

	if (uselist)
	{
		fz_try(ctx)
		{
			list = fz_new_display_list(ctx, fz_bound_page(ctx, page));
			dev = fz_new_list_device(ctx, list);
			if (lowmemory)
				fz_enable_device_hints(ctx, dev, FZ_NO_CACHE);
			fz_run_page(ctx, page, dev, fz_identity, &cookie);
			fz_close_device(ctx, dev);
		}
		fz_always(ctx)
		{
			fz_drop_device(ctx, dev);
			dev = NULL;
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_drop_separations(ctx, seps);
			fz_drop_page(ctx, page);
			fz_rethrow(ctx);
		}

		if (bgprint.active && showtime)
		{
			int end = gettime();
			start = end - start;
		}
	}

	if (showfeatures)
	{
		int iscolor;
		dev = fz_new_test_device(ctx, &iscolor, 0.02f, 0, NULL);
		if (lowmemory)
			fz_enable_device_hints(ctx, dev, FZ_NO_CACHE);
		fz_try(ctx)
		{
			if (list)
				fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
			else
				fz_run_page(ctx, page, dev, fz_identity, &cookie);
			fz_close_device(ctx, dev);
		}
		fz_always(ctx)
		{
			fz_drop_device(ctx, dev);
			dev = NULL;
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_drop_separations(ctx, seps);
			fz_drop_page(ctx, page);
			fz_rethrow(ctx);
		}
		features = iscolor ? " color" : " grayscale";
	}

	if (output_file_per_page)
	{
		char text_buffer[512];

		bgprint_flush();
		if (out)
		{
			fz_close_output(ctx, out);
			fz_drop_output(ctx, out);
		}
		fz_snprintf(text_buffer, sizeof(text_buffer), output, pagenum);
		out = fz_new_output_with_path(ctx, text_buffer, 0);
	}

	if (bgprint.active)
	{
		bgprint_flush();
		if (bgprint.active)
		{
			if (!quiet || showfeatures || showtime || showmd5)
				fprintf(stderr, "page %s %d%s", filename, pagenum, features);
		}

		bgprint.started = 1;
		bgprint.page = page;
		bgprint.list = list;
		bgprint.seps = seps;
		bgprint.filename = filename;
		bgprint.pagenum = pagenum;
		bgprint.interptime = start;
#ifndef DISABLE_MUTHREADS
		mu_trigger_semaphore(&bgprint.start);
#else
		fz_drop_display_list(ctx, list);
		fz_drop_page(ctx, page);
#endif
	}
	else
	{
		if (!quiet || showfeatures || showtime || showmd5)
			fprintf(stderr, "page %s %d%s", filename, pagenum, features);
		dodrawpage(ctx, page, list, pagenum, &cookie, start, 0, filename, 0, seps);
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
			{
				fz_try(ctx)
					drawpage(ctx, doc, page);
				fz_catch(ctx)
				{
					if (ignore_errors)
						fz_warn(ctx, "ignoring error on page %d in '%s'", page, filename);
					else
						fz_rethrow(ctx);
				}
			}
		else
			for (page = spage; page >= epage; page--)
			{
				fz_try(ctx)
					drawpage(ctx, doc, page);
				fz_catch(ctx)
				{
					if (ignore_errors)
						fz_warn(ctx, "ignoring error on page %d in '%s'", page, filename);
					else
						fz_rethrow(ctx);
				}
			}
	}
}

static int
parse_colorspace(const char *name)
{
	int i;

	for (i = 0; i < (int)nelem(cs_name_table); i++)
	{
		if (!strcmp(name, cs_name_table[i].name))
			return cs_name_table[i].colorspace;
	}

	/* Assume ICC. We will error out later if not the case. */
	icc_filename = name;
	return CS_ICC;
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
		DEBUG_THREADS(("Worker %d woken for band %d\n", me->num, me->band));
		if (me->band >= 0)
			drawband(me->ctx, NULL, me->list, me->ctm, me->tbounds, &me->cookie, me->band * band_height, me->pix, &me->bit);
		DEBUG_THREADS(("Worker %d completed band %d\n", me->num, me->band));
		mu_trigger_semaphore(&me->stop);
	}
	while (me->band >= 0);
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
			dodrawpage(bgprint.ctx, bgprint.page, bgprint.list, pagenum, &cookie, start, bgprint.interptime, bgprint.filename, 1, bgprint.seps);
		}
		DEBUG_THREADS(("BGPrint completed page %d\n", pagenum));
		mu_trigger_semaphore(&bgprint.stop);
	}
	while (pagenum >= 0);
}
#endif

static inline int iswhite(int ch)
{
	return
		ch == '\011' || ch == '\012' ||
		ch == '\014' || ch == '\015' || ch == '\040';
}

static void apply_layer_config(fz_context *ctx, fz_document *doc, const char *lc)
{
#if FZ_ENABLE_PDF
	pdf_document *pdoc = pdf_specifics(ctx, doc);
	int config;
	int n, j;
	pdf_layer_config info;

	if (!pdoc)
	{
		fz_warn(ctx, "Only PDF files have layers");
		return;
	}

	while (iswhite(*lc))
		lc++;

	if (*lc == 0 || *lc == 'l')
	{
		int num_configs = pdf_count_layer_configs(ctx, pdoc);

		fprintf(stderr, "Layer configs:\n");
		for (config = 0; config < num_configs; config++)
		{
			fprintf(stderr, " %s%d:", config < 10 ? " " : "", config);
			pdf_layer_config_info(ctx, pdoc, config, &info);
			if (info.name)
				fprintf(stderr, " Name=\"%s\"", info.name);
			if (info.creator)
				fprintf(stderr, " Creator=\"%s\"", info.creator);
			fprintf(stderr, "\n");
		}
		return;
	}

	/* Read the config number */
	if (*lc < '0' || *lc > '9')
	{
		fprintf(stderr, "cannot find number expected for -y\n");
		return;
	}
	config = fz_atoi(lc);
	pdf_select_layer_config(ctx, pdoc, config);

	while (*lc)
	{
		int item;

		/* Skip over the last number we read (in the fz_atoi) */
		while (*lc >= '0' && *lc <= '9')
			lc++;
		while (iswhite(*lc))
			lc++;
		if (*lc != ',')
			break;
		lc++;
		while (iswhite(*lc))
			lc++;
		if (*lc < '0' || *lc > '9')
		{
			fprintf(stderr, "Expected a number for UI item to toggle\n");
			return;
		}
		item = fz_atoi(lc);
		pdf_toggle_layer_config_ui(ctx, pdoc, item);
	}

	/* Now list the final state of the config */
	fprintf(stderr, "Layer Config %d:\n", config);
	pdf_layer_config_info(ctx, pdoc, config, &info);
	if (info.name)
		fprintf(stderr, " Name=\"%s\"", info.name);
	if (info.creator)
		fprintf(stderr, " Creator=\"%s\"", info.creator);
	fprintf(stderr, "\n");
	n = pdf_count_layer_config_ui(ctx, pdoc);
	for (j = 0; j < n; j++)
	{
		pdf_layer_config_ui ui;

		pdf_layer_config_ui_info(ctx, pdoc, j, &ui);
		fprintf(stderr, "%s%d: ", j < 10 ? " " : "", j);
		while (ui.depth > 0)
		{
			ui.depth--;
			fprintf(stderr, "  ");
		}
		if (ui.type == PDF_LAYER_UI_CHECKBOX)
			fprintf(stderr, " [%c] ", ui.selected ? 'x' : ' ');
		else if (ui.type == PDF_LAYER_UI_RADIOBOX)
			fprintf(stderr, " (%c) ", ui.selected ? 'x' : ' ');
		if (ui.text)
			fprintf(stderr, "%s", ui.text);
		if (ui.type != PDF_LAYER_UI_LABEL && ui.locked)
			fprintf(stderr, " <locked>");
		fprintf(stderr, "\n");
	}
#endif
}

#ifdef MUDRAW_STANDALONE
int main(int argc, char **argv)
#else
int mudraw_main(int argc, char **argv)
#endif
{
	char *password = "";
	fz_document *doc = NULL;
	int c;
	fz_context *ctx;
	trace_info info = { 0, 0, 0 };
	fz_alloc_context alloc_ctx = { &info, trace_malloc, trace_realloc, trace_free };
	fz_locks_context *locks = NULL;

	fz_var(doc);

	while ((c = fz_getopt(argc, argv, "qp:o:F:R:r:w:h:fB:c:e:G:Is:A:DiW:H:S:T:U:XLvPl:y:NO:")) != -1)
	{
		switch (c)
		{
		default: usage(); break;

		case 'q': quiet = 1; break;

		case 'p': password = fz_optarg; break;

		case 'o': output = fz_optarg; break;
		case 'F': format = fz_optarg; break;

		case 'R': rotation = fz_atof(fz_optarg); break;
		case 'r': resolution = fz_atof(fz_optarg); res_specified = 1; break;
		case 'w': width = fz_atof(fz_optarg); break;
		case 'h': height = fz_atof(fz_optarg); break;
		case 'f': fit = 1; break;
		case 'B': band_height = atoi(fz_optarg); break;

		case 'c': out_cs = parse_colorspace(fz_optarg); break;
		case 'e': proof_filename = fz_optarg; break;
		case 'G': gamma_value = fz_atof(fz_optarg); break;
		case 'I': invert++; break;

		case 'W': layout_w = fz_atof(fz_optarg); break;
		case 'H': layout_h = fz_atof(fz_optarg); break;
		case 'S': layout_em = fz_atof(fz_optarg); break;
		case 'U': layout_css = fz_optarg; break;
		case 'X': layout_use_doc_css = 0; break;

		case 'O': spots = fz_atof(fz_optarg);
#ifndef FZ_ENABLE_SPOT_RENDERING
			fprintf(stderr, "Spot rendering/Overprint/Overprint simulation not enabled in this build\n");
			spots = SPOTS_NONE;
#endif
			break;

		case 's':
			if (strchr(fz_optarg, 't')) ++showtime;
			if (strchr(fz_optarg, 'm')) ++showmemory;
			if (strchr(fz_optarg, 'f')) ++showfeatures;
			if (strchr(fz_optarg, '5')) ++showmd5;
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
		case 'D': uselist = 0; break;
		case 'l': min_line_width = fz_atof(fz_optarg); break;
		case 'i': ignore_errors = 1; break;
		case 'N': no_icc = 1; break;

		case 'T':
#ifndef DISABLE_MUTHREADS
			num_workers = atoi(fz_optarg); break;
#else
			fprintf(stderr, "Threads not enabled in this build\n");
			break;
#endif
		case 'L': lowmemory = 1; break;
		case 'P':
#ifndef DISABLE_MUTHREADS
			bgprint.active = 1; break;
#else
			fprintf(stderr, "Threads not enabled in this build\n");
			break;
#endif
		case 'y': layer_config = fz_optarg; break;

		case 'v': fprintf(stderr, "mudraw version %s\n", FZ_VERSION); return 1;
		}
	}

	if (fz_optind == argc)
		usage();

	if (num_workers > 0)
	{
		if (uselist == 0)
		{
			fprintf(stderr, "cannot use multiple threads without using display list\n");
			exit(1);
		}

		if (band_height == 0)
		{
			fprintf(stderr, "Using multiple threads without banding is pointless\n");
		}
	}

	if (bgprint.active)
	{
		if (uselist == 0)
		{
			fprintf(stderr, "cannot bgprint without using display list\n");
			exit(1);
		}
	}

#ifndef DISABLE_MUTHREADS
	locks = init_mudraw_locks();
	if (locks == NULL)
	{
		fprintf(stderr, "mutex initialisation failed\n");
		exit(1);
	}
#endif

	ctx = fz_new_context((showmemory == 0 ? NULL : &alloc_ctx), locks, (lowmemory ? 1 : FZ_STORE_DEFAULT));
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_try(ctx)
	{
		if (proof_filename)
		{
			fz_buffer *proof_buffer = fz_read_file(ctx, proof_filename);
			proof_cs = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_NONE, 0, NULL, proof_buffer);
			fz_drop_buffer(ctx, proof_buffer);
		}

		fz_set_text_aa_level(ctx, alphabits_text);
		fz_set_graphics_aa_level(ctx, alphabits_graphics);
		fz_set_graphics_min_line_width(ctx, min_line_width);
		if (no_icc)
			fz_disable_icc(ctx);
		else
			fz_enable_icc(ctx);

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

		/* Determine output type */
		if (band_height < 0)
		{
			fprintf(stderr, "Bandheight must be > 0\n");
			exit(1);
		}

		output_format = OUT_PNG;
		if (format)
		{
			int i;

			for (i = 0; i < (int)nelem(suffix_table); i++)
			{
				if (!strcmp(format, suffix_table[i].suffix+1))
				{
					output_format = suffix_table[i].format;
					if (spots == SPOTS_FULL && suffix_table[i].spots == 0)
					{
						fprintf(stderr, "Output format '%s' does not support spot rendering.\nDoing overprint simulation instead.\n", suffix_table[i].suffix+1);
						spots = SPOTS_OVERPRINT_SIM;
					}
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
					if (spots == SPOTS_FULL && suffix_table[i].spots == 0)
					{
						fprintf(stderr, "Output format '%s' does not support spot rendering.\nDoing overprint simulation instead.\n", suffix_table[i].suffix+1);
						spots = SPOTS_OVERPRINT_SIM;
					}
					i = 0;
				}
			}
		}

		if (band_height)
		{
			if (output_format != OUT_PAM && output_format != OUT_PGM && output_format != OUT_PPM && output_format != OUT_PNM && output_format != OUT_PNG && output_format != OUT_PBM && output_format != OUT_PKM && output_format != OUT_PCL && output_format != OUT_PCLM && output_format != OUT_PS && output_format != OUT_PSD)
			{
				fprintf(stderr, "Banded operation only possible with PxM, PCL, PCLM, PS, PSD, and PNG outputs\n");
				exit(1);
			}
			if (showmd5)
			{
				fprintf(stderr, "Banded operation not compatible with MD5\n");
				exit(1);
			}
		}

		{
			int i, j;

			for (i = 0; i < (int)nelem(format_cs_table); i++)
			{
				if (format_cs_table[i].format == output_format)
				{
					if (out_cs == CS_UNSET)
						out_cs = format_cs_table[i].default_cs;
					for (j = 0; j < (int)nelem(format_cs_table[i].permitted_cs); j++)
					{
						if (format_cs_table[i].permitted_cs[j] == out_cs)
							break;
					}
					if (j == (int)nelem(format_cs_table[i].permitted_cs))
					{
						fprintf(stderr, "Unsupported colorspace for this format\n");
						exit(1);
					}
				}
			}
		}

		alpha = 1;
		switch (out_cs)
		{
			case CS_MONO:
			case CS_GRAY:
			case CS_GRAY_ALPHA:
				colorspace = fz_device_gray(ctx);
				alpha = (out_cs == CS_GRAY_ALPHA);
				break;
			case CS_RGB:
			case CS_RGB_ALPHA:
				colorspace = fz_device_rgb(ctx);
				alpha = (out_cs == CS_RGB_ALPHA);
				break;
			case CS_CMYK:
			case CS_CMYK_ALPHA:
				colorspace = fz_device_cmyk(ctx);
				alpha = (out_cs == CS_CMYK_ALPHA);
				break;
			case CS_ICC:
				fz_try(ctx)
				{
					fz_buffer *icc_buffer = fz_read_file(ctx, icc_filename);
					colorspace = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_NONE, 0, NULL, icc_buffer);
					fz_drop_buffer(ctx, icc_buffer);
				}
				fz_catch(ctx)
				{
					fprintf(stderr, "Invalid ICC destination color space\n");
					exit(1);
				}
				if (colorspace == NULL)
				{
					fprintf(stderr, "Invalid ICC destination color space\n");
					exit(1);
				}
				alpha = 0;
				break;
			default:
				fprintf(stderr, "Unknown colorspace!\n");
				exit(1);
				break;
		}

		if (out_cs != CS_ICC)
			colorspace = fz_keep_colorspace(ctx, colorspace);
		else
		{
			int i, j, okay;

			/* Check to make sure this icc profile is ok with the output format */
			okay = 0;
			for (i = 0; i < (int)nelem(format_cs_table); i++)
			{
				if (format_cs_table[i].format == output_format)
				{
					for (j = 0; j < (int)nelem(format_cs_table[i].permitted_cs); j++)
					{
						switch (format_cs_table[i].permitted_cs[j])
						{
							case CS_MONO:
							case CS_GRAY:
							case CS_GRAY_ALPHA:
								if (fz_colorspace_is_gray(ctx, colorspace))
									okay = 1;
								break;
							case CS_RGB:
							case CS_RGB_ALPHA:
								if (fz_colorspace_is_rgb(ctx, colorspace))
									okay = 1;
								break;
							case CS_CMYK:
							case CS_CMYK_ALPHA:
								if (fz_colorspace_is_cmyk(ctx, colorspace))
									okay = 1;
								break;
						}
					}
				}
			}

			if (!okay)
			{
				fprintf(stderr, "ICC profile uses a colorspace that cannot be used for this format\n");
				exit(1);
			}
		}

#if FZ_ENABLE_PDF
		if (output_format == OUT_PDF)
		{
			pdfout = pdf_create_document(ctx);
		}
		else
#endif
			if (output_format == OUT_SVG)
			{
				/* SVG files are always opened for each page. Do not open "output". */
			}
			else if (output && (output[0] != '-' || output[1] != 0) && *output != 0)
			{
				if (has_percent_d(output))
					output_file_per_page = 1;
				else
					out = fz_new_output_with_path(ctx, output, 0);
			}
			else
			{
				quiet = 1; /* automatically be quiet if printing to stdout */
#ifdef _WIN32
				/* Windows specific code to make stdout binary. */
				if (output_format != OUT_TEXT && output_format != OUT_STEXT && output_format != OUT_HTML && output_format != OUT_XHTML && output_format != OUT_TRACE)
					setmode(fileno(stdout), O_BINARY);
#endif
				out = fz_stdout(ctx);
			}

		filename = argv[fz_optind];
		if (!output_file_per_page)
			file_level_headers(ctx);

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
		if (showtime && bgprint.active)
			timing.total = gettime();

		fz_try(ctx)
		{
			fz_register_document_handlers(ctx);

			while (fz_optind < argc)
			{
				fz_try(ctx)
				{
					filename = argv[fz_optind++];
					files++;

					doc = fz_open_document(ctx, filename);

					if (fz_needs_password(ctx, doc))
					{
						if (!fz_authenticate_password(ctx, doc, password))
							fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", filename);
					}

					/* Once document is open check for output intent colorspace */
					oi = fz_document_output_intent(ctx, doc);
					if (oi)
					{
						/* See if we had explicitly set a profile to render */
						if (out_cs != CS_ICC)
						{
							/* In this case, we want to render to the output intent
							 * color space if the number of channels is the same */
							if (fz_colorspace_n(ctx, oi) == fz_colorspace_n(ctx, colorspace))
							{
								fz_drop_colorspace(ctx, colorspace);
								colorspace = fz_keep_colorspace(ctx, oi);
							}
						}
					}

					fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);

					if (layer_config)
						apply_layer_config(ctx, doc, layer_config);

					if (fz_optind == argc || !fz_is_page_range(ctx, argv[fz_optind]))
						drawrange(ctx, doc, "1-N");
					if (fz_optind < argc && fz_is_page_range(ctx, argv[fz_optind]))
						drawrange(ctx, doc, argv[fz_optind++]);

					bgprint_flush();
				}
				fz_always(ctx)
				{
					fz_drop_document(ctx, doc);
					doc = NULL;
				}
				fz_catch(ctx)
				{
					if (!ignore_errors)
						fz_rethrow(ctx);

					bgprint_flush();
					fz_warn(ctx, "ignoring error in '%s'", filename);
				}
			}
		}
		fz_catch(ctx)
		{
			bgprint_flush();
			fz_drop_document(ctx, doc);
			fprintf(stderr, "error: cannot draw '%s'\n", filename);
			errored = 1;
		}

		if (!output_file_per_page)
			file_level_trailers(ctx);

#if FZ_ENABLE_PDF
		if (output_format == OUT_PDF)
		{
			if (!output)
				output = "out.pdf";
			pdf_save_document(ctx, pdfout, output, NULL);
			pdf_drop_document(ctx, pdfout);
		}
		else
#endif
		{
			fz_close_output(ctx, out);
			fz_drop_output(ctx, out);
			out = NULL;
		}

		if (showtime && timing.count > 0)
		{
			if (bgprint.active)
				timing.total = gettime() - timing.total;

			if (files == 1)
			{
				fprintf(stderr, "total %dms / %d pages for an average of %dms\n",
						timing.total, timing.count, timing.total / timing.count);
				if (bgprint.active)
				{
					fprintf(stderr, "fastest page %d: %dms (interpretation) %dms (rendering) %dms(total)\n",
							timing.minpage, timing.mininterp, timing.min - timing.mininterp, timing.min);
					fprintf(stderr, "slowest page %d: %dms (interpretation) %dms (rendering) %dms(total)\n",
							timing.maxpage, timing.maxinterp, timing.max - timing.maxinterp, timing.max);
				}
				else
				{
					fprintf(stderr, "fastest page %d: %dms\n", timing.minpage, timing.min);
					fprintf(stderr, "slowest page %d: %dms\n", timing.maxpage, timing.max);
				}
			}
			else
			{
				fprintf(stderr, "total %dms / %d pages for an average of %dms in %d files\n",
						timing.total, timing.count, timing.total / timing.count, files);
				fprintf(stderr, "fastest page %d: %dms (%s)\n", timing.minpage, timing.min, timing.minfilename);
				fprintf(stderr, "slowest page %d: %dms (%s)\n", timing.maxpage, timing.max, timing.maxfilename);
			}
		}

#ifndef DISABLE_MUTHREADS
		if (num_workers > 0)
		{
			int i;
			for (i = 0; i < num_workers; i++)
			{
				workers[i].band = -1;
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
	}
	fz_always(ctx)
	{
		fz_drop_colorspace(ctx, colorspace);
		fz_drop_colorspace(ctx, proof_cs);
	}
	fz_catch(ctx)
	{
	}

	fz_drop_context(ctx);

#ifndef DISABLE_MUTHREADS
	fin_mudraw_locks();
#endif /* DISABLE_MUTHREADS */

	if (showmemory)
	{
		char buf[100];
		fz_snprintf(buf, sizeof buf, "Memory use total=%zu peak=%zu current=%zu", info.total, info.peak, info.current);
		fprintf(stderr, "%s\n", buf);
	}

	return (errored != 0);
}
