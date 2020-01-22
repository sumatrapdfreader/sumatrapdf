#include "mupdf/fitz.h"

#include <assert.h>
#include <string.h>

typedef struct {
	fz_band_writer super;
	fz_pwg_options pwg;
} pwg_band_writer;

/*
	Output the file header to a pwg stream, ready for pages to follow it.
*/
void
fz_write_pwg_file_header(fz_context *ctx, fz_output *out)
{
	static const unsigned char pwgsig[4] = { 'R', 'a', 'S', '2' };

	/* Sync word */
	fz_write_data(ctx, out, pwgsig, 4);
}

static void
pwg_page_header(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg,
		int xres, int yres, int w, int h, int bpp)
{
	static const char zero[64] = { 0 };
	int i;

	/* Page Header: */
	fz_write_data(ctx, out, pwg ? pwg->media_class : zero, 64);
	fz_write_data(ctx, out, pwg ? pwg->media_color : zero, 64);
	fz_write_data(ctx, out, pwg ? pwg->media_type : zero, 64);
	fz_write_data(ctx, out, pwg ? pwg->output_type : zero, 64);
	fz_write_int32_be(ctx, out, pwg ? pwg->advance_distance : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->advance_media : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->collate : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->cut_media : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->duplex : 0);
	fz_write_int32_be(ctx, out, xres);
	fz_write_int32_be(ctx, out, yres);
	/* CUPS format says that 284->300 are supposed to be the bbox of the
	 * page in points. PWG says 'Reserved'. */
	for (i=284; i < 300; i += 4)
		fz_write_data(ctx, out, zero, 4);
	fz_write_int32_be(ctx, out, pwg ? pwg->insert_sheet : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->jog : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->leading_edge : 0);
	/* CUPS format says that 312->320 are supposed to be the margins of
	 * the lower left hand edge of page in points. PWG says 'Reserved'. */
	for (i=312; i < 320; i += 4)
		fz_write_data(ctx, out, zero, 4);
	fz_write_int32_be(ctx, out, pwg ? pwg->manual_feed : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->media_position : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->media_weight : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->mirror_print : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->negative_print : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->num_copies : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->orientation : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->output_face_up : 0);
	fz_write_int32_be(ctx, out, w * 72/ xres);	/* Page size in points */
	fz_write_int32_be(ctx, out, h * 72/ yres);
	fz_write_int32_be(ctx, out, pwg ? pwg->separations : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->tray_switch : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->tumble : 0);
	fz_write_int32_be(ctx, out, w); /* Page image in pixels */
	fz_write_int32_be(ctx, out, h);
	fz_write_int32_be(ctx, out, pwg ? pwg->media_type_num : 0);
	fz_write_int32_be(ctx, out, bpp < 8 ? 1 : 8); /* Bits per color */
	fz_write_int32_be(ctx, out, bpp); /* Bits per pixel */
	fz_write_int32_be(ctx, out, (w * bpp + 7)/8); /* Bytes per line */
	fz_write_int32_be(ctx, out, 0); /* Chunky pixels */
	switch (bpp)
	{
	case 1: fz_write_int32_be(ctx, out, 3); /* Black */ break;
	case 8: fz_write_int32_be(ctx, out, 18); /* Sgray */ break;
	case 24: fz_write_int32_be(ctx, out, 19); /* Srgb */ break;
	case 32: fz_write_int32_be(ctx, out, 6); /* Cmyk */ break;
	default: fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap bpp must be 1, 8, 24 or 32 to write as pwg");
	}
	fz_write_int32_be(ctx, out, pwg ? pwg->compression : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->row_count : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->row_feed : 0);
	fz_write_int32_be(ctx, out, pwg ? pwg->row_step : 0);
	fz_write_int32_be(ctx, out, bpp <= 8 ? 1 : (bpp>>8)); /* Num Colors */
	for (i=424; i < 452; i += 4)
		fz_write_data(ctx, out, zero, 4);
	fz_write_int32_be(ctx, out, 1); /* TotalPageCount */
	fz_write_int32_be(ctx, out, 1); /* CrossFeedTransform */
	fz_write_int32_be(ctx, out, 1); /* FeedTransform */
	fz_write_int32_be(ctx, out, 0); /* ImageBoxLeft */
	fz_write_int32_be(ctx, out, 0); /* ImageBoxTop */
	fz_write_int32_be(ctx, out, w); /* ImageBoxRight */
	fz_write_int32_be(ctx, out, h); /* ImageBoxBottom */
	for (i=480; i < 1668; i += 4)
		fz_write_data(ctx, out, zero, 4);
	fz_write_data(ctx, out, pwg ? pwg->rendering_intent : zero, 64);
	fz_write_data(ctx, out, pwg ? pwg->page_size_name : zero, 64);
}

/*
	Output a page to a pwg stream to follow a header, or other pages.
*/
void
fz_write_pixmap_as_pwg_page(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg)
{
	fz_band_writer *writer = fz_new_pwg_band_writer(ctx, out, pwg);

	fz_try(ctx)
	{
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, pixmap->xres, pixmap->yres, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
	}
	fz_always(ctx)
		fz_drop_band_writer(ctx, writer);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
	Output a bitmap page to a pwg stream to follow a header, or other pages.
*/
void
fz_write_bitmap_as_pwg_page(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg)
{
	fz_band_writer *writer = fz_new_mono_pwg_band_writer(ctx, out, pwg);

	fz_try(ctx)
	{
		fz_write_header(ctx, writer, bitmap->w, bitmap->h, bitmap->n, 0, bitmap->xres, bitmap->yres, 0, NULL, NULL);
		fz_write_band(ctx, writer, bitmap->stride, bitmap->h, bitmap->samples);
	}
	fz_always(ctx)
		fz_drop_band_writer(ctx, writer);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_write_pixmap_as_pwg(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg)
{
	fz_write_pwg_file_header(ctx, out);
	fz_write_pixmap_as_pwg_page(ctx, out, pixmap, pwg);
}

void
fz_write_bitmap_as_pwg(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg)
{
	fz_write_pwg_file_header(ctx, out);
	fz_write_bitmap_as_pwg_page(ctx, out, bitmap, pwg);
}

/*
	Save a pixmap as a pwg

	filename: The filename to save as (including extension).

	append: If non-zero, then append a new page to existing file.

	pwg: NULL, or a pointer to an options structure (initialised to zero
	before being filled in, for future expansion).
*/
void
fz_save_pixmap_as_pwg(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pwg_options *pwg)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, append);
	fz_try(ctx)
	{
		if (!append)
			fz_write_pwg_file_header(ctx, out);
		fz_write_pixmap_as_pwg_page(ctx, out, pixmap, pwg);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
	Save a bitmap as a pwg

	filename: The filename to save as (including extension).

	append: If non-zero, then append a new page to existing file.

	pwg: NULL, or a pointer to an options structure (initialised to zero
	before being filled in, for future expansion).
*/
void
fz_save_bitmap_as_pwg(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pwg_options *pwg)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, append);
	fz_try(ctx)
	{
		if (!append)
			fz_write_pwg_file_header(ctx, out);
		fz_write_bitmap_as_pwg_page(ctx, out, bitmap, pwg);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pwg_write_mono_header(fz_context *ctx, fz_band_writer *writer_, fz_colorspace *cs)
{
	pwg_band_writer *writer = (pwg_band_writer *)writer_;

	pwg_page_header(ctx, writer->super.out, &writer->pwg,
		writer->super.xres, writer->super.yres, writer->super.w, writer->super.h, 1);
}

static void
pwg_write_mono_band(fz_context *ctx, fz_band_writer *writer_, int stride, int band_start, int band_height, const unsigned char *samples)
{
	pwg_band_writer *writer = (pwg_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	const unsigned char *sp;
	int y, x;
	int byte_width;

	/* Now output the actual bitmap, using a packbits like compression */
	sp = samples;
	byte_width = (w+7)/8;
	y = 0;
	while (y < band_height)
	{
		int yrep;

		assert(sp == samples + y * stride);

		/* Count the number of times this line is repeated */
		for (yrep = 1; yrep < 256 && y+yrep < h; yrep++)
		{
			if (memcmp(sp, sp + yrep * stride, byte_width) != 0)
				break;
		}
		fz_write_byte(ctx, out, yrep-1);

		/* Encode the line */
		x = 0;
		while (x < byte_width)
		{
			int d;

			assert(sp == samples + y * stride + x);

			/* How far do we have to look to find a repeated value? */
			for (d = 1; d < 128 && x+d < byte_width; d++)
			{
				if (sp[d-1] == sp[d])
					break;
			}
			if (d == 1)
			{
				int xrep;

				/* We immediately have a repeat (or we've hit
				 * the end of the line). Count the number of
				 * times this value is repeated. */
				for (xrep = 1; xrep < 128 && x+xrep < byte_width; xrep++)
				{
					if (sp[0] != sp[xrep])
						break;
				}
				fz_write_byte(ctx, out, xrep-1);
				fz_write_data(ctx, out, sp, 1);
				sp += xrep;
				x += xrep;
			}
			else
			{
				fz_write_byte(ctx, out, 257-d);
				fz_write_data(ctx, out, sp, d);
				sp += d;
				x += d;
			}
		}

		/* Move to the next line */
		sp += stride*yrep - byte_width;
		y += yrep;
	}
}

/*
	Generate a new band writer for
	PWG format images.
*/
fz_band_writer *fz_new_mono_pwg_band_writer(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg)
{
	pwg_band_writer *writer = fz_new_band_writer(ctx, pwg_band_writer, out);

	writer->super.header = pwg_write_mono_header;
	writer->super.band = pwg_write_mono_band;
	if (pwg)
		writer->pwg = *pwg;
	else
		memset(&writer->pwg, 0, sizeof(writer->pwg));

	return &writer->super;
}

static void
pwg_write_header(fz_context *ctx, fz_band_writer *writer_, fz_colorspace *cs)
{
	pwg_band_writer *writer = (pwg_band_writer *)writer_;
	int n = writer->super.n;

	if (writer->super.s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PWG band writer cannot cope with spot colors");
	if (writer->super.alpha != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PWG band writer cannot cope with alpha");
	if (n != 1 && n != 3 && n != 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale, rgb or cmyk to write as pwg");

	pwg_page_header(ctx, writer->super.out, &writer->pwg,
			writer->super.xres, writer->super.yres, writer->super.w, writer->super.h, n*8);
}

static void
pwg_write_band(fz_context *ctx, fz_band_writer *writer_, int stride, int band_start, int band_height, const unsigned char *samples)
{
	pwg_band_writer *writer = (pwg_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	const unsigned char *sp = samples;
	int n = writer->super.n;
	int ss = w * n;
	int y, x;

	/* Now output the actual bitmap, using a packbits like compression */
	y = 0;
	while (y < h)
	{
		int yrep;

		assert(sp == samples + y * stride);

		/* Count the number of times this line is repeated */
		for (yrep = 1; yrep < 256 && y+yrep < h; yrep++)
		{
			if (memcmp(sp, sp + yrep * stride, ss) != 0)
				break;
		}
		fz_write_byte(ctx, out, yrep-1);

		/* Encode the line */
		x = 0;
		while (x < w)
		{
			int d;

			assert(sp == samples + y * stride + x * n);

			/* How far do we have to look to find a repeated value? */
			for (d = 1; d < 128 && x+d < w; d++)
			{
				if (memcmp(sp + (d-1)*n, sp + d*n, n) == 0)
					break;
			}
			if (d == 1)
			{
				int xrep;

				/* We immediately have a repeat (or we've hit
				 * the end of the line). Count the number of
				 * times this value is repeated. */
				for (xrep = 1; xrep < 128 && x+xrep < w; xrep++)
				{
					if (memcmp(sp, sp + xrep*n, n) != 0)
						break;
				}
				fz_write_byte(ctx, out, xrep-1);
				fz_write_data(ctx, out, sp, n);
				sp += n*xrep;
				x += xrep;
			}
			else
			{
				fz_write_byte(ctx, out, 257-d);
				x += d;
				while (d > 0)
				{
					fz_write_data(ctx, out, sp, n);
					sp += n;
					d--;
				}
			}
		}

		/* Move to the next line */
		sp += stride*(yrep-1);
		y += yrep;
	}
}

/*
	Generate a new band writer for
	contone PWG format images.
*/
fz_band_writer *fz_new_pwg_band_writer(fz_context *ctx, fz_output *out, const fz_pwg_options *pwg)
{
	pwg_band_writer *writer = fz_new_band_writer(ctx, pwg_band_writer, out);

	writer->super.header = pwg_write_header;
	writer->super.band = pwg_write_band;
	if (pwg)
		writer->pwg = *pwg;
	else
		memset(&writer->pwg, 0, sizeof(writer->pwg));

	return &writer->super;
}

/* High-level document writer interface */

const char *fz_pwg_write_options_usage =
	"PWG output options:\n"
	"\tmedia_class=<string>: set the media_class field\n"
	"\tmedia_color=<string>: set the media_color field\n"
	"\tmedia_type=<string>: set the media_type field\n"
	"\toutput_type=<string>: set the output_type field\n"
	"\trendering_intent=<string>: set the rendering_intent field\n"
	"\tpage_size_name=<string>: set the page_size_name field\n"
	"\tadvance_distance=<int>: set the advance_distance field\n"
	"\tadvance_media=<int>: set the advance_media field\n"
	"\tcollate=<int>: set the collate field\n"
	"\tcut_media=<int>: set the cut_media field\n"
	"\tduplex=<int>: set the duplex field\n"
	"\tinsert_sheet=<int>: set the insert_sheet field\n"
	"\tjog=<int>: set the jog field\n"
	"\tleading_edge=<int>: set the leading_edge field\n"
	"\tmanual_feed=<int>: set the manual_feed field\n"
	"\tmedia_position=<int>: set the media_position field\n"
	"\tmedia_weight=<int>: set the media_weight field\n"
	"\tmirror_print=<int>: set the mirror_print field\n"
	"\tnegative_print=<int>: set the negative_print field\n"
	"\tnum_copies=<int>: set the num_copies field\n"
	"\torientation=<int>: set the orientation field\n"
	"\toutput_face_up=<int>: set the output_face_up field\n"
	"\tpage_size_x=<int>: set the page_size_x field\n"
	"\tpage_size_y=<int>: set the page_size_y field\n"
	"\tseparations=<int>: set the separations field\n"
	"\ttray_switch=<int>: set the tray_switch field\n"
	"\ttumble=<int>: set the tumble field\n"
	"\tmedia_type_num=<int>: set the media_type_num field\n"
	"\tcompression=<int>: set the compression field\n"
	"\trow_count=<int>: set the row_count field\n"
	"\trow_feed=<int>: set the row_feed field\n"
	"\trow_step=<int>: set the row_step field\n"
	"\n";

static void
warn_if_long(fz_context *ctx, const char *str, size_t ret)
{
	if (ret > 0)
		fz_warn(ctx, "Option %s is too long, truncated.", str);
}

fz_pwg_options *
fz_parse_pwg_options(fz_context *ctx, fz_pwg_options *opts, const char *args)
{
	const char *val;

	memset(opts, 0, sizeof *opts);

	if (fz_has_option(ctx, args, "media_class", &val))
		warn_if_long(ctx, "media_class", fz_copy_option(ctx, val, opts->media_class, 64));
	if (fz_has_option(ctx, args, "media_color", &val))
		warn_if_long(ctx, "media_color", fz_copy_option(ctx, val, opts->media_color, 64));
	if (fz_has_option(ctx, args, "media_type", &val))
		warn_if_long(ctx, "media_type", fz_copy_option(ctx, val, opts->media_type, 64));
	if (fz_has_option(ctx, args, "output_type", &val))
		warn_if_long(ctx, "output_type", fz_copy_option(ctx, val, opts->output_type, 64));
	if (fz_has_option(ctx, args, "rendering_intent", &val))
		warn_if_long(ctx, "rendering_intent", fz_copy_option(ctx, val, opts->rendering_intent, 64));
	if (fz_has_option(ctx, args, "page_size_name", &val))
		warn_if_long(ctx, "page_size_name", fz_copy_option(ctx, val, opts->page_size_name, 64));
	if (fz_has_option(ctx, args, "advance_distance", &val))
		opts->advance_distance = fz_atoi(val);
	if (fz_has_option(ctx, args, "advance_media", &val))
		opts->advance_media = fz_atoi(val);
	if (fz_has_option(ctx, args, "collate", &val))
		opts->collate = fz_atoi(val);
	if (fz_has_option(ctx, args, "cut_media", &val))
		opts->cut_media = fz_atoi(val);
	if (fz_has_option(ctx, args, "duplex", &val))
		opts->duplex = fz_atoi(val);
	if (fz_has_option(ctx, args, "insert_sheet", &val))
		opts->insert_sheet = fz_atoi(val);
	if (fz_has_option(ctx, args, "jog", &val))
		opts->jog = fz_atoi(val);
	if (fz_has_option(ctx, args, "leading_edge", &val))
		opts->leading_edge = fz_atoi(val);
	if (fz_has_option(ctx, args, "manual_feed", &val))
		opts->manual_feed = fz_atoi(val);
	if (fz_has_option(ctx, args, "media_position", &val))
		opts->media_position = fz_atoi(val);
	if (fz_has_option(ctx, args, "media_weight", &val))
		opts->media_weight = fz_atoi(val);
	if (fz_has_option(ctx, args, "mirror_print", &val))
		opts->mirror_print = fz_atoi(val);
	if (fz_has_option(ctx, args, "negative_print", &val))
		opts->negative_print = fz_atoi(val);
	if (fz_has_option(ctx, args, "num_copies", &val))
		opts->num_copies = fz_atoi(val);
	if (fz_has_option(ctx, args, "orientation", &val))
		opts->orientation = fz_atoi(val);
	if (fz_has_option(ctx, args, "output_face_up", &val))
		opts->output_face_up = fz_atoi(val);
	if (fz_has_option(ctx, args, "page_size_x", &val))
		opts->PageSize[0] = fz_atoi(val);
	if (fz_has_option(ctx, args, "page_size_y", &val))
		opts->PageSize[1] = fz_atoi(val);
	if (fz_has_option(ctx, args, "separations", &val))
		opts->separations = fz_atoi(val);
	if (fz_has_option(ctx, args, "tray_switch", &val))
		opts->tray_switch = fz_atoi(val);
	if (fz_has_option(ctx, args, "tumble", &val))
		opts->tumble = fz_atoi(val);
	if (fz_has_option(ctx, args, "media_type_num", &val))
		opts->media_type_num = fz_atoi(val);
	if (fz_has_option(ctx, args, "compression", &val))
		opts->compression = fz_atoi(val);
	if (fz_has_option(ctx, args, "row_count", &val))
		opts->row_count = fz_atoi(val);
	if (fz_has_option(ctx, args, "row_feed", &val))
		opts->row_feed = fz_atoi(val);
	if (fz_has_option(ctx, args, "row_step", &val))
		opts->row_step = fz_atoi(val);

	return opts;
}

typedef struct fz_pwg_writer_s fz_pwg_writer;

struct fz_pwg_writer_s
{
	fz_document_writer super;
	fz_draw_options draw;
	fz_pwg_options pwg;
	int mono;
	fz_pixmap *pixmap;
	fz_output *out;
};

static fz_device *
pwg_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_pwg_writer *wri = (fz_pwg_writer*)wri_;
	return fz_new_draw_device_with_options(ctx, &wri->draw, mediabox, &wri->pixmap);
}

static void
pwg_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_pwg_writer *wri = (fz_pwg_writer*)wri_;
	fz_bitmap *bitmap = NULL;

	fz_var(bitmap);

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		if (wri->mono)
		{
			bitmap = fz_new_bitmap_from_pixmap(ctx, wri->pixmap, NULL);
			fz_write_bitmap_as_pwg_page(ctx, wri->out, bitmap, &wri->pwg);
		}
		else
		{
			fz_write_pixmap_as_pwg_page(ctx, wri->out, wri->pixmap, &wri->pwg);
		}
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_bitmap(ctx, bitmap);
		fz_drop_pixmap(ctx, wri->pixmap);
		wri->pixmap = NULL;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
pwg_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pwg_writer *wri = (fz_pwg_writer*)wri_;
	fz_close_output(ctx, wri->out);
}

static void
pwg_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pwg_writer *wri = (fz_pwg_writer*)wri_;
	fz_drop_pixmap(ctx, wri->pixmap);
	fz_drop_output(ctx, wri->out);
}

fz_document_writer *
fz_new_pwg_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_pwg_writer *wri = fz_new_derived_document_writer(ctx, fz_pwg_writer, pwg_begin_page, pwg_end_page, pwg_close_writer, pwg_drop_writer);
	const char *val;

	fz_try(ctx)
	{
		fz_parse_draw_options(ctx, &wri->draw, options);
		fz_parse_pwg_options(ctx, &wri->pwg, options);
		if (fz_has_option(ctx, options, "colorspace", &val))
			if (fz_option_eq(val, "mono"))
				wri->mono = 1;
		wri->out = out;
		fz_write_pwg_file_header(ctx, wri->out);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_pwg_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.pwg", 0);
	fz_document_writer *wri = NULL;
	fz_try(ctx)
		wri = fz_new_pwg_writer_with_output(ctx, out, options);
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return wri;
}
