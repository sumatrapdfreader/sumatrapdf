/*
 * mudraw -- command line tool for drawing pdf/xps/cbz documents
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h" /* for pdf output */

#ifdef _MSC_VER
#define main main_utf8
#endif
/* SumatraPDF: add support for GDI+ draw device */
#ifdef _WIN32
#include <windows.h>
#define GDI_PLUS_BMP_RENDERER
#else
#include <sys/time.h>
#endif

enum { TEXT_PLAIN = 1, TEXT_HTML = 2, TEXT_XML = 3 };

enum { OUT_PNG, OUT_PPM, OUT_PNM, OUT_PAM, OUT_PGM, OUT_PBM, OUT_SVG, OUT_PWG, OUT_PCL, OUT_PDF, OUT_TGA, OUT_BMP };

enum { CS_INVALID, CS_UNSET, CS_MONO, CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA };

typedef struct
{
	char *suffix;
	int format;
} suffix_t;

static const suffix_t suffix_table[] =
{
	{ ".png", OUT_PNG },
	{ ".pgm", OUT_PGM },
	{ ".ppm", OUT_PPM },
	{ ".pnm", OUT_PNM },
	{ ".pam", OUT_PAM },
	{ ".pbm", OUT_PBM },
	{ ".svg", OUT_SVG },
	{ ".pwg", OUT_PWG },
	{ ".pcl", OUT_PCL },
	{ ".pdf", OUT_PDF },
	{ ".tga", OUT_TGA },
#ifdef GDI_PLUS_BMP_RENDERER
	{ ".bmp", OUT_BMP },
#endif
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
	int permitted_cs[6];
} format_cs_table_t;

static const format_cs_table_t format_cs_table[] =
{
	{ OUT_PNG, CS_RGB, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA } },
	{ OUT_PPM, CS_RGB, { CS_GRAY, CS_RGB } },
	{ OUT_PNM, CS_GRAY, { CS_GRAY, CS_RGB } },
	{ OUT_PAM, CS_RGB_ALPHA, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA, CS_CMYK, CS_CMYK_ALPHA } },
	{ OUT_PGM, CS_GRAY, { CS_GRAY, CS_RGB } },
	{ OUT_PBM, CS_MONO, { CS_MONO } },
	{ OUT_SVG, CS_RGB, { CS_RGB } },
	{ OUT_PWG, CS_RGB, { CS_MONO, CS_GRAY, CS_RGB, CS_CMYK } },
	{ OUT_PCL, CS_MONO, { CS_MONO } },
	{ OUT_PDF, CS_RGB, { CS_RGB } },
	{ OUT_TGA, CS_RGB, { CS_GRAY, CS_GRAY_ALPHA, CS_RGB, CS_RGB_ALPHA } },
#ifdef GDI_PLUS_BMP_RENDERER
	{ OUT_BMP, CS_RGB, { CS_RGB } },
#endif
};

/*
	A useful bit of bash script to call this to generate mjs files:
	for f in tests_private/pdf/forms/v1.3/ *.pdf ; do g=${f%.*} ; echo $g ; ../mupdf.git/win32/debug/mudraw.exe -j $g.mjs $g.pdf ; done

	Remove the space from "/ *.pdf" before running - can't leave that
	in here, as it causes a warning about a possibly malformed comment.
*/

static char *output = NULL;
static char *format = NULL;
static float resolution = 72;
static int res_specified = 0;
static float rotation = 0;

static int showxml = 0;
static int showtext = 0;
static int showtime = 0;
static int showmd5 = 0;
static pdf_document *pdfout = NULL;
static int showoutline = 0;
static int uselist = 1;
static int alphabits = 8;
static float gamma_value = 1;
static int invert = 0;
static int width = 0;
static int height = 0;
static int fit = 0;
static int errored = 0;
static int ignore_errors = 0;
static int output_format;
static int append = 0;
static int out_cs = CS_UNSET;
static int bandheight = 0;
static int memtrace_current = 0;
static int memtrace_peak = 0;
static int memtrace_total = 0;
static int showmemory = 0;
static fz_text_sheet *sheet = NULL;
static fz_colorspace *colorspace;
static char *filename;
static int files = 0;
fz_output *out = NULL;

static struct {
	int count, total;
	int min, max;
	int minpage, maxpage;
	char *minfilename;
	char *maxfilename;
} timing;

static void usage(void)
{
	fprintf(stderr,
		"usage: mudraw [options] input [pages]\n"
		"\t-o -\toutput filename (%%d for page number)\n"
		"\t-F -\toutput format (if no -F, -o will be examined)\n"
		"\t\tsupported formats: png, tga, pnm, pam, pwg, pcl, svg, pdf\n"
		"\t-p -\tpassword\n"
		"\t-r -\tresolution in dpi (default: 72)\n"
		"\t-w -\twidth (in pixels) (maximum width if -r is specified)\n"
		"\t-h -\theight (in pixels) (maximum height if -r is specified)\n"
		"\t-f -\tfit width and/or height exactly (ignore aspect)\n"
		"\t-c -\tcolorspace {mono,gray,grayalpha,rgb,rgba,cmyk,cmykalpha}\n"
		"\t-b -\tnumber of bits of antialiasing (0 to 8)\n"
		"\t-B -\tmaximum bandheight (pgm, ppm, pam output only)\n"
		"\t-g\trender in grayscale (equivalent to: -c gray)\n"
		"\t-m\tshow timing information\n"
		"\t-M\tshow memory use summary\n"
		"\t-t\tshow text (-tt for xml, -ttt for more verbose xml)\n"
		"\t-x\tshow display list\n"
		"\t-d\tdisable use of display list\n"
		"\t-5\tshow md5 checksums\n"
		"\t-R -\trotate clockwise by given number of degrees\n"
		"\t-G -\tgamma correct output\n"
		"\t-I\tinvert output\n"
		"\t-l\tprint outline\n"
		"\t-i\tignore errors and continue with the next file\n"
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
static void drawbmp(fz_context *ctx, fz_document *doc, fz_page *page, fz_display_list *list, int pagenum, fz_cookie *cookie)
{
	float zoom;
	fz_matrix ctm;
	fz_irect ibounds;
	fz_rect bounds, tbounds;

	int w, h;
	fz_device *dev;
	HDC dc, dc_main;
	RECT rc;
	HBRUSH bg_brush;
	HBITMAP hbmp;
	BITMAPINFO bmi = { 0 };
	int bmp_data_len;
	unsigned char *bmp_data;

	fz_bound_page(doc, page, &bounds);
	zoom = resolution / 72;
	fz_pre_scale(fz_rotate(&ctm, rotation), zoom, zoom);
	tbounds = bounds;
	fz_round_rect(&ibounds, fz_transform_rect(&tbounds, &ctm));

	w = width;
	h = height;
	if (res_specified)
	{
		fz_round_rect(&ibounds, &tbounds);
		if (w && ibounds.x1 - ibounds.x0 <= w)
			w = 0;
		if (h && ibounds.y1 - ibounds.y0 <= h)
			h = 0;
	}
	if (w || h)
	{
		float scalex = w / (tbounds.x1 - tbounds.x0);
		float scaley = h / (tbounds.y1 - tbounds.y0);
		fz_matrix scale_mat;
		if (w == 0)
			scalex = fit ? 1.0f : scaley;
		if (h == 0)
			scaley = fit ? 1.0f : scalex;
		if (!fit)
			scalex = scaley = min(scalex, scaley);
		fz_concat(&ctm, &ctm, fz_scale(&scale_mat, scalex, scaley));
		tbounds = bounds;
		fz_transform_rect(&tbounds, &ctm);
	}
	fz_round_rect(&ibounds, &tbounds);
	fz_rect_from_irect(&tbounds, &ibounds);

	w = ibounds.x1 - ibounds.x0;
	h = ibounds.y1 - ibounds.y0;

	dc_main = GetDC(NULL);
	hbmp = CreateCompatibleBitmap(dc_main, w, h);
	if (!hbmp)
		fz_throw(ctx, FZ_ERROR_GENERIC, "failed to create a %d x %d bitmap for page %d", w, h, pagenum);
	dc = CreateCompatibleDC(dc_main);
	DeleteObject(SelectObject(dc, hbmp));

	SetRect(&rc, 0, 0, w, h);
	bg_brush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
	FillRect(dc, &rc, bg_brush);
	DeleteObject(bg_brush);

	dev = fz_new_gdiplus_device(ctx, dc, &tbounds);
	if (list)
		fz_run_display_list(list, dev, &ctm, &tbounds, cookie);
	else
		fz_run_page(doc, page, dev, &ctm, cookie);
	fz_free_device(dev);

	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = output_format == OUT_TGA ? -h : h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = output_format == OUT_TGA ? 32 : 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	bmp_data_len = output_format == OUT_TGA ? w * h * 4 : ((w * 3 + 3) / 4) * 4 * h;
	bmp_data = fz_malloc(ctx, bmp_data_len);
	if (!GetDIBits(dc, hbmp, 0, h, bmp_data, &bmi, DIB_RGB_COLORS))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot draw page %d", pagenum, filename);

	DeleteDC(dc);
	ReleaseDC(NULL, dc_main);
	DeleteObject(hbmp);

	if (output)
	{
		char buf[512];
		FILE *f;

		sprintf(buf, output, pagenum);
		f = fopen(buf, "wb");
		if (!f)
			fz_throw(ctx, FZ_ERROR_GENERIC, "could not create raster file '%s'", buf);

		if (output_format == OUT_TGA)
		{
			fz_pixmap *pix = fz_new_pixmap_with_data(ctx, fz_device_bgr(ctx), w, h, bmp_data);
			fz_write_tga(ctx, pix, buf, 0);
			fz_drop_pixmap(ctx, pix);
		}
		else
		{
			BITMAPFILEHEADER bmpfh = { 0 };
			static const int one = 1;
			if (!*(char *)&one)
				fz_throw(ctx, FZ_ERROR_GENERIC, "rendering to BMP is not supported on big-endian architectures");

			bmpfh.bfType = MAKEWORD('B', 'M');
			bmpfh.bfOffBits = sizeof(bmpfh) + sizeof(bmi);
			bmpfh.bfSize = bmpfh.bfOffBits + bmp_data_len;

			fwrite(&bmpfh, sizeof(bmpfh), 1, f);
			fwrite(&bmi, sizeof(bmi), 1, f);
			fwrite(bmp_data, 1, bmp_data_len, f);
		}

		fclose(f);
	}

	if (showmd5)
	{
		fz_pixmap *pix = fz_new_pixmap_with_data(ctx, fz_device_bgr(ctx), bmp_data_len / 4 / h, h, bmp_data);
		unsigned char digest[16];
		int i;

		fz_md5_pixmap(pix, digest);
		printf(" ");
		for (i = 0; i < 16; i++)
			printf("%02x", digest[i]);

		fz_drop_pixmap(ctx, pix);
	}

	fz_free(ctx, bmp_data);
}
#endif

static void drawpage(fz_context *ctx, fz_document *doc, int pagenum)
{
	fz_page *page;
	fz_display_list *list = NULL;
	fz_device *dev = NULL;
	int start;
	fz_cookie cookie = { 0 };

	fz_var(list);
	fz_var(dev);

	if (showtime)
	{
		start = gettime();
	}

	fz_try(ctx)
	{
		page = fz_load_page(doc, pagenum - 1);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot load page %d in file '%s'", pagenum, filename);
	}

	if (uselist)
	{
		fz_try(ctx)
		{
			list = fz_new_display_list(ctx);
			dev = fz_new_list_device(ctx, list);
			fz_run_page(doc, page, dev, &fz_identity, &cookie);
		}
		fz_always(ctx)
		{
			fz_free_device(dev);
			dev = NULL;
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_free_page(doc, page);
			fz_rethrow_message(ctx, "cannot draw page %d in file '%s'", pagenum, filename);
		}
	}

	if (showxml)
	{
		fz_try(ctx)
		{
			dev = fz_new_trace_device(ctx);
			if (list)
				fz_run_display_list(list, dev, &fz_identity, &fz_infinite_rect, &cookie);
			else
				fz_run_page(doc, page, dev, &fz_identity, &cookie);
		}
		fz_always(ctx)
		{
			fz_free_device(dev);
			dev = NULL;
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_free_page(doc, page);
			fz_rethrow(ctx);
		}
	}

	if (showtext)
	{
		fz_text_page *text = NULL;

		fz_var(text);

		fz_try(ctx)
		{
			text = fz_new_text_page(ctx);
			dev = fz_new_text_device(ctx, sheet, text);
			if (showtext == TEXT_HTML)
				fz_disable_device_hints(dev, FZ_IGNORE_IMAGE);
			if (list)
				fz_run_display_list(list, dev, &fz_identity, &fz_infinite_rect, &cookie);
			else
				fz_run_page(doc, page, dev, &fz_identity, &cookie);
			fz_free_device(dev);
			dev = NULL;
			if (showtext == TEXT_XML)
			{
				fz_print_text_page_xml(ctx, out, text);
			}
			else if (showtext == TEXT_HTML)
			{
				fz_analyze_text(ctx, sheet, text);
				fz_print_text_page_html(ctx, out, text);
			}
			else if (showtext == TEXT_PLAIN)
			{
				fz_print_text_page(ctx, out, text);
				fz_printf(out, "\f\n");
			}
		}
		fz_always(ctx)
		{
			fz_free_device(dev);
			dev = NULL;
			fz_free_text_page(ctx, text);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_free_page(doc, page);
			fz_rethrow(ctx);
		}
	}

	if (showmd5 || showtime)
		printf("page %s %d", filename, pagenum);

	if (pdfout)
	{
		fz_matrix ctm;
		fz_rect bounds, tbounds;
		pdf_page *newpage;

		fz_bound_page(doc, page, &bounds);
		fz_rotate(&ctm, rotation);
		tbounds = bounds;
		fz_transform_rect(&tbounds, &ctm);

		newpage = pdf_create_page(pdfout, bounds, 72, 0);

		fz_try(ctx)
		{
			dev = pdf_page_write(pdfout, newpage);
			if (list)
				fz_run_display_list(list, dev, &ctm, &tbounds, &cookie);
			else
				fz_run_page(doc, page, dev, &ctm, &cookie);
			fz_free_device(dev);
			dev = NULL;
		}
		fz_always(ctx)
		{
			fz_free_device(dev);
			dev = NULL;
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_free_page(doc, page);
			fz_rethrow(ctx);
		}
		pdf_insert_page(pdfout, newpage, INT_MAX);
		pdf_free_page(pdfout, newpage);
	}

	if (output && output_format == OUT_SVG)
	{
		float zoom;
		fz_matrix ctm;
		fz_rect bounds, tbounds;
		char buf[512];
		FILE *file;
		fz_output *out;

		if (!strcmp(output, "-"))
			file = stdout;
		else
		{
			sprintf(buf, output, pagenum);
			file = fopen(buf, "wb");
			if (file == NULL)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file '%s': %s", buf, strerror(errno));
		}

		out = fz_new_output_with_file(ctx, file);

		fz_bound_page(doc, page, &bounds);
		zoom = resolution / 72;
		fz_pre_rotate(fz_scale(&ctm, zoom, zoom), rotation);
		tbounds = bounds;
		fz_transform_rect(&tbounds, &ctm);

		fz_try(ctx)
		{
			dev = fz_new_svg_device(ctx, out, tbounds.x1-tbounds.x0, tbounds.y1-tbounds.y0);
			if (list)
				fz_run_display_list(list, dev, &ctm, &tbounds, &cookie);
			else
				fz_run_page(doc, page, dev, &ctm, &cookie);
			fz_free_device(dev);
			dev = NULL;
		}
		fz_always(ctx)
		{
			fz_free_device(dev);
			dev = NULL;
			fz_close_output(out);
			if (file != stdout)
				fclose(file);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_free_page(doc, page);
			fz_rethrow(ctx);
		}
	}

#ifdef GDI_PLUS_BMP_RENDERER
	// hack: use -G0 to "enable GDI+" when saving as TGA
	if (output_format == OUT_BMP || output_format == OUT_TGA && !gamma_value)
		drawbmp(ctx, doc, page, list, pagenum, &cookie);
	else
#endif
	if ((output && output_format != OUT_SVG && !pdfout)|| showmd5 || showtime)
	{
		float zoom;
		fz_matrix ctm;
		fz_rect bounds, tbounds;
		fz_irect ibounds;
		fz_pixmap *pix = NULL;
		int w, h;
		fz_output *output_file = NULL;
		fz_png_output_context *poc = NULL;

		fz_var(pix);
		fz_var(poc);

		fz_bound_page(doc, page, &bounds);
		zoom = resolution / 72;
		fz_pre_scale(fz_rotate(&ctm, rotation), zoom, zoom);
		tbounds = bounds;
		fz_round_rect(&ibounds, fz_transform_rect(&tbounds, &ctm));

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
			fz_scale(&scale_mat, scalex, scaley);
			fz_concat(&ctm, &ctm, &scale_mat);
			tbounds = bounds;
			fz_transform_rect(&tbounds, &ctm);
		}
		fz_round_rect(&ibounds, &tbounds);
		fz_rect_from_irect(&tbounds, &ibounds);

		/* TODO: banded rendering and multi-page ppm */
		fz_try(ctx)
		{
			int savealpha = (out_cs == CS_GRAY_ALPHA || out_cs == CS_RGB_ALPHA || out_cs == CS_CMYK_ALPHA);
			fz_irect band_ibounds = ibounds;
			int band, bands = 1;
			char filename_buf[512];
			int totalheight = ibounds.y1 - ibounds.y0;
			int drawheight = totalheight;

			if (bandheight != 0)
			{
				/* Banded rendering; we'll only render to a
				 * given height at a time. */
				drawheight = bandheight;
				if (totalheight > bandheight)
					band_ibounds.y1 = band_ibounds.y0 + bandheight;
				bands = (totalheight + bandheight-1)/bandheight;
				tbounds.y1 = tbounds.y0 + bandheight + 2;
			}

			pix = fz_new_pixmap_with_bbox(ctx, colorspace, &band_ibounds);
			fz_pixmap_set_resolution(pix, resolution);

			if (output)
			{
				if (!strcmp(output, "-"))
					output_file = fz_new_output_with_file(ctx, stdout);
				else
				{
					sprintf(filename_buf, output, pagenum);
					output_file = fz_new_output_to_filename(ctx, filename_buf);
				}

				if (output_format == OUT_PGM || output_format == OUT_PPM || output_format == OUT_PNM)
					fz_output_pnm_header(output_file, pix->w, totalheight, pix->n);
				else if (output_format == OUT_PAM)
					fz_output_pam_header(output_file, pix->w, totalheight, pix->n, savealpha);
				else if (output_format == OUT_PNG)
					poc = fz_output_png_header(output_file, pix->w, totalheight, pix->n, savealpha);
			}

			for (band = 0; band < bands; band++)
			{
				if (savealpha)
					fz_clear_pixmap(ctx, pix);
				else
					fz_clear_pixmap_with_value(ctx, pix, 255);

				dev = fz_new_draw_device(ctx, pix);
				if (alphabits == 0)
					fz_enable_device_hints(dev, FZ_DONT_INTERPOLATE_IMAGES);
				if (list)
					fz_run_display_list(list, dev, &ctm, &tbounds, &cookie);
				else
					fz_run_page(doc, page, dev, &ctm, &cookie);
				fz_free_device(dev);
				dev = NULL;

				if (invert)
					fz_invert_pixmap(ctx, pix);
				if (gamma_value != 1)
					fz_gamma_pixmap(ctx, pix, gamma_value);

				if (savealpha)
					fz_unmultiply_pixmap(ctx, pix);

				if (output)
				{
					if (output_format == OUT_PGM || output_format == OUT_PPM || output_format == OUT_PNM)
						fz_output_pnm_band(output_file, pix->w, totalheight, pix->n, band, drawheight, pix->samples);
					else if (output_format == OUT_PAM)
						fz_output_pam_band(output_file, pix->w, totalheight, pix->n, band, drawheight, pix->samples, savealpha);
					else if (output_format == OUT_PNG)
						fz_output_png_band(output_file, pix->w, totalheight, pix->n, band, drawheight, pix->samples, savealpha, poc);
					else if (output_format == OUT_PWG)
					{
						if (strstr(output, "%d") != NULL)
							append = 0;
						if (out_cs == CS_MONO)
						{
							fz_bitmap *bit = fz_halftone_pixmap(ctx, pix, NULL);
							fz_write_pwg_bitmap(ctx, bit, filename_buf, append, NULL);
							fz_drop_bitmap(ctx, bit);
						}
						else
							fz_write_pwg(ctx, pix, filename_buf, append, NULL);
						append = 1;
					}
					else if (output_format == OUT_PCL)
					{
						fz_pcl_options options;

						fz_pcl_preset(ctx, &options, "ljet4");

						if (strstr(output, "%d") != NULL)
							append = 0;
						if (out_cs == CS_MONO)
						{
							fz_bitmap *bit = fz_halftone_pixmap(ctx, pix, NULL);
							fz_write_pcl_bitmap(ctx, bit, filename_buf, append, &options);
							fz_drop_bitmap(ctx, bit);
						}
						else
							fz_write_pcl(ctx, pix, filename_buf, append, &options);
						append = 1;
					}
					else if (output_format == OUT_PBM) {
						fz_bitmap *bit = fz_halftone_pixmap(ctx, pix, NULL);
						fz_write_pbm(ctx, bit, filename_buf);
						fz_drop_bitmap(ctx, bit);
					}
					else if (output_format == OUT_TGA)
					{
						fz_write_tga(ctx, pix, filename_buf, savealpha);
					}
				}
				ctm.f -= drawheight;
			}

			if (showmd5)
			{
				unsigned char digest[16];
				int i;

				fz_md5_pixmap(pix, digest);
				printf(" ");
				for (i = 0; i < 16; i++)
					printf("%02x", digest[i]);
			}
		}
		fz_always(ctx)
		{
			if (output)
			{
				if (output_format == OUT_PNG)
					fz_output_png_trailer(output_file, poc);
			}

			fz_free_device(dev);
			dev = NULL;
			fz_drop_pixmap(ctx, pix);
			if (output_file)
				fz_close_output(output_file);
		}
		fz_catch(ctx)
		{
			fz_drop_display_list(ctx, list);
			fz_free_page(doc, page);
			fz_rethrow(ctx);
		}
	}

	if (list)
		fz_drop_display_list(ctx, list);

	fz_free_page(doc, page);

	if (showtime)
	{
		int end = gettime();
		int diff = end - start;

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

		printf(" %dms", diff);
	}

	if (showmd5 || showtime)
		printf("\n");

	if (showmemory)
	{
		fz_dump_glyph_cache_stats(ctx);
	}

	fz_flush_warnings(ctx);

	if (cookie.errors)
		errored = 1;
}

static void drawrange(fz_context *ctx, fz_document *doc, char *range)
{
	int page, spage, epage, pagecount;
	char *spec, *dash;

	pagecount = fz_count_pages(doc);
	spec = fz_strsep(&range, ",");
	while (spec)
	{
		dash = strchr(spec, '-');

		if (dash == spec)
			spage = epage = pagecount;
		else
			spage = epage = atoi(spec);

		if (dash)
		{
			if (strlen(dash) > 1)
				epage = atoi(dash + 1);
			else
				epage = pagecount;
		}

		spage = fz_clampi(spage, 1, pagecount);
		epage = fz_clampi(epage, 1, pagecount);

		if (spage < epage)
			for (page = spage; page <= epage; page++)
				drawpage(ctx, doc, page);
		else
			for (page = spage; page >= epage; page--)
				drawpage(ctx, doc, page);

		spec = fz_strsep(&range, ",");
	}
}

static void drawoutline(fz_context *ctx, fz_document *doc)
{
	fz_outline *outline = fz_load_outline(doc);
	fz_output *out = NULL;

	fz_var(out);
	fz_try(ctx)
	{
		out = fz_new_output_with_file(ctx, stdout);
		if (showoutline > 1)
			fz_print_outline_xml(ctx, out, outline);
		else
			fz_print_outline(ctx, out, outline);
	}
	fz_always(ctx)
	{
		fz_close_output(out);
		fz_free_outline(ctx, outline);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static int
parse_colorspace(const char *name)
{
	int i;

	for (i = 0; i < nelem(cs_name_table); i++)
	{
		if (!strcmp(name, cs_name_table[i].name))
			return cs_name_table[i].colorspace;
	}
	fprintf(stderr, "Unknown colorspace \"%s\"\n", name);
	exit(1);
}

static void *
trace_malloc(void *arg, unsigned int size)
{
	int *p;
	if (size == 0)
		return NULL;
	p = malloc(size + sizeof(unsigned int));
	if (p == NULL)
		return NULL;
	p[0] = size;
	memtrace_current += size;
	memtrace_total += size;
	if (memtrace_current > memtrace_peak)
		memtrace_peak = memtrace_current;
	return (void *)&p[1];
}

static void
trace_free(void *arg, void *p_)
{
	int *p = (int *)p_;

	if (p == NULL)
		return;
	memtrace_current -= p[-1];
	free(&p[-1]);
}

static void *
trace_realloc(void *arg, void *p_, unsigned int size)
{
	int *p = (int *)p_;
	unsigned int oldsize;

	if (size == 0)
	{
		trace_free(arg, p_);
		return NULL;
	}
	if (p == NULL)
		return trace_malloc(arg, size);
	oldsize = p[-1];
	p = realloc(&p[-1], size + sizeof(unsigned int));
	if (p == NULL)
		return NULL;
	memtrace_current += size - oldsize;
	if (size > oldsize)
		memtrace_total += size - oldsize;
	if (memtrace_current > memtrace_peak)
		memtrace_peak = memtrace_current;
	p[0] = size;
	return &p[1];
}

int main(int argc, char **argv)
{
	char *password = "";
	fz_document *doc = NULL;
	int c;
	fz_context *ctx;
	fz_alloc_context alloc_ctx = { NULL, trace_malloc, trace_realloc, trace_free };

	fz_var(doc);

	while ((c = fz_getopt(argc, argv, "lo:F:p:r:R:b:c:dgmtx5G:Iw:h:fiMB:")) != -1)
	{
		switch (c)
		{
		case 'o': output = fz_optarg; break;
		case 'F': format = fz_optarg; break;
		case 'p': password = fz_optarg; break;
		case 'r': resolution = atof(fz_optarg); res_specified = 1; break;
		case 'R': rotation = atof(fz_optarg); break;
		case 'b': alphabits = atoi(fz_optarg); break;
		case 'B': bandheight = atoi(fz_optarg); break;
		case 'l': showoutline++; break;
		case 'm': showtime++; break;
		case 'M': showmemory++; break;
		case 't': showtext++; break;
		case 'x': showxml++; break;
		case '5': showmd5++; break;
		case 'g': out_cs = CS_GRAY; break;
		case 'd': uselist = 0; break;
		case 'c': out_cs = parse_colorspace(fz_optarg); break;
		case 'G': gamma_value = atof(fz_optarg); break;
		case 'w': width = atof(fz_optarg); break;
		case 'h': height = atof(fz_optarg); break;
		case 'f': fit = 1; break;
		case 'I': invert++; break;
		case 'i': ignore_errors = 1; break;
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

	ctx = fz_new_context((showmemory == 0 ? NULL : &alloc_ctx), NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_set_aa_level(ctx, alphabits);

	/* SumatraPDF: use locally installed fonts */
	pdf_install_load_system_font_funcs(ctx);

	/* Determine output type */
	if (bandheight < 0)
	{
		fprintf(stderr, "Bandheight must be > 0\n");
		exit(1);
	}

	output_format = OUT_PNG;
	if (format)
	{
		int i;

		for (i = 0; i < nelem(suffix_table); i++)
		{
			if (!strcmp(format, suffix_table[i].suffix+1))
			{
				output_format = suffix_table[i].format;
				break;
			}
		}
		if (i == nelem(suffix_table))
		{
			fprintf(stderr, "Unknown output format '%s'\n", format);
			exit(1);
		}
	}
	else if (output)
	{
		char *suffix = output;
		int i;

		for (i = 0; i < nelem(suffix_table); i++)
		{
			char *s = strstr(suffix, suffix_table[i].suffix);

			if (s != NULL)
			{
				suffix = s+1;
				output_format = suffix_table[i].format;
				i = 0;
			}
		}
	}

	if (bandheight)
	{
		if (output_format != OUT_PAM && output_format != OUT_PGM && output_format != OUT_PPM && output_format != OUT_PNM && output_format != OUT_PNG)
		{
			fprintf(stderr, "Banded operation only possible with PAM, PGM, PPM, PNM and PNG outputs\n");
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

		for (i = 0; i < nelem(format_cs_table); i++)
		{
			if (format_cs_table[i].format == output_format)
			{
				if (out_cs == CS_UNSET)
					out_cs = format_cs_table[i].default_cs;
				for (j = 0; j < nelem(format_cs_table[i].permitted_cs); j++)
				{
					if (format_cs_table[i].permitted_cs[j] == out_cs)
						break;
				}
				if (j == nelem(format_cs_table[i].permitted_cs))
				{
					fprintf(stderr, "Unsupported colorspace for this format\n");
					exit(1);
				}
			}
		}
	}

	switch (out_cs)
	{
	case CS_MONO:
	case CS_GRAY:
	case CS_GRAY_ALPHA:
		colorspace = fz_device_gray(ctx);
		break;
	case CS_RGB:
	case CS_RGB_ALPHA:
		colorspace = fz_device_rgb(ctx);
		break;
	case CS_CMYK:
	case CS_CMYK_ALPHA:
		colorspace = fz_device_cmyk(ctx);
		break;
	default:
		fprintf(stderr, "Unknown colorspace!\n");
		exit(1);
		break;
	}

	if (output_format == OUT_PDF)
	{
		pdfout = pdf_create_document(ctx);
	}

	timing.count = 0;
	timing.total = 0;
	timing.min = 1 << 30;
	timing.max = 0;
	timing.minpage = 0;
	timing.maxpage = 0;
	timing.minfilename = "";
	timing.maxfilename = "";

	if (showxml || showtext)
		out = fz_new_output_with_file(ctx, stdout);

	if (showxml || showtext == TEXT_XML)
		fz_printf(out, "<?xml version=\"1.0\"?>\n");

	if (showtext)
		sheet = fz_new_text_sheet(ctx);

	if (showtext == TEXT_HTML)
	{
		fz_printf(out, "<style>\n");
		fz_printf(out, "body{background-color:gray;margin:12pt;}\n");
		fz_printf(out, "div.page{background-color:white;margin:6pt;padding:6pt;}\n");
		fz_printf(out, "div.block{border:1px solid gray;margin:6pt;padding:6pt;}\n");
		fz_printf(out, "div.metaline{display:table;width:100%%}\n");
		fz_printf(out, "div.line{display:table-row;padding:6pt}\n");
		fz_printf(out, "div.cell{display:table-cell;padding-left:6pt;padding-right:6pt}\n");
		fz_printf(out, "p{margin:0pt;padding:0pt;}\n");
		fz_printf(out, "</style>\n");
		fz_printf(out, "<body>\n");
	}

	fz_try(ctx)
	{
		fz_register_document_handlers(ctx);

		while (fz_optind < argc)
		{
			fz_try(ctx)
			{
				filename = argv[fz_optind++];
				files++;

				fz_try(ctx)
				{
					doc = fz_open_document(ctx, filename);
				}
				fz_catch(ctx)
				{
					fz_rethrow_message(ctx, "cannot open document: %s", filename);
				}

				if (fz_needs_password(doc))
				{
					if (!fz_authenticate_password(doc, password))
						fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", filename);
				}

				if (showxml || showtext == TEXT_XML)
					fz_printf(out, "<document name=\"%s\">\n", filename);

				if (showoutline)
					drawoutline(ctx, doc);

				if (showtext || showxml || showtime || showmd5 || output)
				{
					if (fz_optind == argc || !isrange(argv[fz_optind]))
						drawrange(ctx, doc, "1-");
					if (fz_optind < argc && isrange(argv[fz_optind]))
						drawrange(ctx, doc, argv[fz_optind++]);
				}

				if (showxml || showtext == TEXT_XML)
					fz_printf(out, "</document>\n");

				fz_close_document(doc);
				doc = NULL;
			}
			fz_catch(ctx)
			{
				if (!ignore_errors)
					fz_rethrow(ctx);

				fz_close_document(doc);
				doc = NULL;
				fz_warn(ctx, "ignoring error in '%s'", filename);
			}
		}
	}
	fz_catch(ctx)
	{
		fz_close_document(doc);
		fprintf(stderr, "error: cannot draw '%s'\n", filename);
		errored = 1;
	}

	if (pdfout)
	{
		fz_write_options opts = { 0 };

		pdf_write_document(pdfout, output, &opts);
		pdf_close_document(pdfout);
	}

	if (showtext == TEXT_HTML)
	{
		fz_printf(out, "</body>\n");
		fz_printf(out, "<style>\n");
		fz_print_text_sheet(ctx, out, sheet);
		fz_printf(out, "</style>\n");
	}

	if (showtext)
		fz_free_text_sheet(ctx, sheet);

	if (showxml || showtext)
	{
		fz_close_output(out);
		out = NULL;
	}

	if (showtime && timing.count > 0)
	{
		if (files == 1)
		{
			printf("total %dms / %d pages for an average of %dms\n",
				timing.total, timing.count, timing.total / timing.count);
			printf("fastest page %d: %dms\n", timing.minpage, timing.min);
			printf("slowest page %d: %dms\n", timing.maxpage, timing.max);
		}
		else
		{
			printf("total %dms / %d pages for an average of %dms in %d files\n",
				timing.total, timing.count, timing.total / timing.count, files);
			printf("fastest page %d: %dms (%s)\n", timing.minpage, timing.min, timing.minfilename);
			printf("slowest page %d: %dms (%s)\n", timing.maxpage, timing.max, timing.maxfilename);
		}
	}

	fz_free_context(ctx);

	if (showmemory)
	{
		printf("Total memory use = %d bytes\n", memtrace_total);
		printf("Peak memory use = %d bytes\n", memtrace_peak);
		printf("Current memory use = %d bytes\n", memtrace_current);
	}

	return (errored != 0);
}

#ifdef _MSC_VER
int wmain(int argc, wchar_t *wargv[])
{
	char **argv = fz_argv_from_wargv(argc, wargv);
	int ret = main(argc, argv);
	fz_free_argv(argc, argv);
	return ret;
}
#endif
