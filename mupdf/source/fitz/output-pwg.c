#include "mupdf/fitz.h"

void
fz_output_pwg_file_header(fz_output *out)
{
	static const unsigned char pwgsig[4] = { 'R', 'a', 'S', '2' };

	/* Sync word */
	fz_write(out, pwgsig, 4);
}

static void
output_header(fz_output *out, const fz_pwg_options *pwg, int xres, int yres, int w, int h, int bpp)
{
	static const char zero[64] = { 0 };
	int i;

	/* Page Header: */
	fz_write(out, pwg ? pwg->media_class : zero, 64);
	fz_write(out, pwg ? pwg->media_color : zero, 64);
	fz_write(out, pwg ? pwg->media_type : zero, 64);
	fz_write(out, pwg ? pwg->output_type : zero, 64);
	fz_write_int32be(out, pwg ? pwg->advance_distance : 0);
	fz_write_int32be(out, pwg ? pwg->advance_media : 0);
	fz_write_int32be(out, pwg ? pwg->collate : 0);
	fz_write_int32be(out, pwg ? pwg->cut_media : 0);
	fz_write_int32be(out, pwg ? pwg->duplex : 0);
	fz_write_int32be(out, xres);
	fz_write_int32be(out, yres);
	/* CUPS format says that 284->300 are supposed to be the bbox of the
	 * page in points. PWG says 'Reserved'. */
	for (i=284; i < 300; i += 4)
		fz_write(out, zero, 4);
	fz_write_int32be(out, pwg ? pwg->insert_sheet : 0);
	fz_write_int32be(out, pwg ? pwg->jog : 0);
	fz_write_int32be(out, pwg ? pwg->leading_edge : 0);
	/* CUPS format says that 312->320 are supposed to be the margins of
	 * the lower left hand edge of page in points. PWG says 'Reserved'. */
	for (i=312; i < 320; i += 4)
		fz_write(out, zero, 4);
	fz_write_int32be(out, pwg ? pwg->manual_feed : 0);
	fz_write_int32be(out, pwg ? pwg->media_position : 0);
	fz_write_int32be(out, pwg ? pwg->media_weight : 0);
	fz_write_int32be(out, pwg ? pwg->mirror_print : 0);
	fz_write_int32be(out, pwg ? pwg->negative_print : 0);
	fz_write_int32be(out, pwg ? pwg->num_copies : 0);
	fz_write_int32be(out, pwg ? pwg->orientation : 0);
	fz_write_int32be(out, pwg ? pwg->output_face_up : 0);
	fz_write_int32be(out, w * 72/ xres);	/* Page size in points */
	fz_write_int32be(out, h * 72/ yres);
	fz_write_int32be(out, pwg ? pwg->separations : 0);
	fz_write_int32be(out, pwg ? pwg->tray_switch : 0);
	fz_write_int32be(out, pwg ? pwg->tumble : 0);
	fz_write_int32be(out, w); /* Page image in pixels */
	fz_write_int32be(out, h);
	fz_write_int32be(out, pwg ? pwg->media_type_num : 0);
	fz_write_int32be(out, bpp < 8 ? 1 : 8); /* Bits per color */
	fz_write_int32be(out, bpp); /* Bits per pixel */
	fz_write_int32be(out, (w * bpp + 7)/8); /* Bytes per line */
	fz_write_int32be(out, 0); /* Chunky pixels */
	fz_write_int32be(out, bpp == 1 ? 3 /* Black */ : (bpp == 8 ? 18 /* Sgray */ : 19 /* Srgb */)); /* Colorspace */
	fz_write_int32be(out, pwg ? pwg->compression : 0);
	fz_write_int32be(out, pwg ? pwg->row_count : 0);
	fz_write_int32be(out, pwg ? pwg->row_feed : 0);
	fz_write_int32be(out, pwg ? pwg->row_step : 0);
	fz_write_int32be(out, bpp <= 8 ? 1 : 3); /* Num Colors */
	for (i=424; i < 452; i += 4)
		fz_write(out, zero, 4);
	fz_write_int32be(out, 1); /* TotalPageCount */
	fz_write_int32be(out, 1); /* CrossFeedTransform */
	fz_write_int32be(out, 1); /* FeedTransform */
	fz_write_int32be(out, 0); /* ImageBoxLeft */
	fz_write_int32be(out, 0); /* ImageBoxTop */
	fz_write_int32be(out, w); /* ImageBoxRight */
	fz_write_int32be(out, h); /* ImageBoxBottom */
	for (i=480; i < 1668; i += 4)
		fz_write(out, zero, 4);
	fz_write(out, pwg ? pwg->rendering_intent : zero, 64);
	fz_write(out, pwg ? pwg->page_size_name : zero, 64);
}

void
fz_output_pwg_page(fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg)
{
	unsigned char *sp;
	int y, x, sn, dn, ss;
	fz_context *ctx;

	if (!out || !pixmap)
		return;

	ctx = out->ctx;

	if (pixmap->n != 1 && pixmap->n != 2 && pixmap->n != 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as pwg");

	sn = pixmap->n;
	dn = pixmap->n;
	if (dn == 2 || dn == 4)
		dn--;

	output_header(out, pwg, pixmap->xres, pixmap->yres, pixmap->w, pixmap->h, dn*8);

	/* Now output the actual bitmap, using a packbits like compression */
	sp = pixmap->samples;
	ss = pixmap->w * sn;
	y = 0;
	while (y < pixmap->h)
	{
		int yrep;

		assert(sp == pixmap->samples + y * ss);

		/* Count the number of times this line is repeated */
		for (yrep = 1; yrep < 256 && y+yrep < pixmap->h; yrep++)
		{
			if (memcmp(sp, sp + yrep * ss, ss) != 0)
				break;
		}
		fz_write_byte(out, yrep-1);

		/* Encode the line */
		x = 0;
		while (x < pixmap->w)
		{
			int d;

			assert(sp == pixmap->samples + y * ss + x * sn);

			/* How far do we have to look to find a repeated value? */
			for (d = 1; d < 128 && x+d < pixmap->w; d++)
			{
				if (memcmp(sp + (d-1)*sn, sp + d*sn, sn) == 0)
					break;
			}
			if (d == 1)
			{
				int xrep;

				/* We immediately have a repeat (or we've hit
				 * the end of the line). Count the number of
				 * times this value is repeated. */
				for (xrep = 1; xrep < 128 && x+xrep < pixmap->w; xrep++)
				{
					if (memcmp(sp, sp + xrep*sn, sn) != 0)
						break;
				}
				fz_write_byte(out, xrep-1);
				fz_write(out, sp, dn);
				sp += sn*xrep;
				x += xrep;
			}
			else
			{
				fz_write_byte(out, 257-d);
				x += d;
				while (d > 0)
				{
					fz_write(out, sp, dn);
					sp += sn;
					d--;
				}
			}
		}

		/* Move to the next line */
		sp += ss*(yrep-1);
		y += yrep;
	}
}

void
fz_output_pwg_bitmap_page(fz_output *out, const fz_bitmap *bitmap, const fz_pwg_options *pwg)
{
	unsigned char *sp;
	int y, x, ss;
	int byte_width;

	if (!out || !bitmap)
		return;

	output_header(out, pwg, bitmap->xres, bitmap->yres, bitmap->w, bitmap->h, 1);

	/* Now output the actual bitmap, using a packbits like compression */
	sp = bitmap->samples;
	ss = bitmap->stride;
	byte_width = (bitmap->w+7)/8;
	y = 0;
	while (y < bitmap->h)
	{
		int yrep;

		assert(sp == bitmap->samples + y * ss);

		/* Count the number of times this line is repeated */
		for (yrep = 1; yrep < 256 && y+yrep < bitmap->h; yrep++)
		{
			if (memcmp(sp, sp + yrep * ss, byte_width) != 0)
				break;
		}
		fz_write_byte(out, yrep-1);

		/* Encode the line */
		x = 0;
		while (x < byte_width)
		{
			int d;

			assert(sp == bitmap->samples + y * ss + x);

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
				fz_write_byte(out, xrep-1);
				fz_write(out, sp, 1);
				sp += xrep;
				x += xrep;
			}
			else
			{
				fz_write_byte(out, 257-d);
				fz_write(out, sp, d);
				sp += d;
				x += d;
			}
		}

		/* Move to the next line */
		sp += ss*yrep - byte_width;
		y += yrep;
	}
}

void
fz_output_pwg(fz_output *out, const fz_pixmap *pixmap, const fz_pwg_options *pwg)
{
	fz_output_pwg_file_header(out);
	fz_output_pwg_page(out, pixmap, pwg);
}

void
fz_write_pwg(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pwg_options *pwg)
{
	FILE *fp;
	fz_output *out = NULL;

	fp = fopen(filename, append ? "ab" : "wb");
	if (!fp)
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file '%s': %s", filename, strerror(errno));
	}

	fz_var(out);

	fz_try(ctx)
	{
		out = fz_new_output_with_file(ctx, fp);
		if (!append)
			fz_output_pwg_file_header(out);
		fz_output_pwg_page(out, pixmap, pwg);
	}
	fz_always(ctx)
	{
		fz_close_output(out);
		fclose(fp);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
fz_write_pwg_bitmap(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pwg_options *pwg)
{
	FILE *fp;
	fz_output *out = NULL;

	fp = fopen(filename, append ? "ab" : "wb");
	if (!fp)
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file '%s': %s", filename, strerror(errno));
	}

	fz_var(out);

	fz_try(ctx)
	{
		out = fz_new_output_with_file(ctx, fp);
		if (!append)
			fz_output_pwg_file_header(out);
		fz_output_pwg_bitmap_page(out, bitmap, pwg);
	}
	fz_always(ctx)
	{
		fz_close_output(out);
		fclose(fp);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
