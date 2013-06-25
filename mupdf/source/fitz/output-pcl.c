#include "mupdf/fitz.h"

/* Lifted from ghostscript gdevjlm.h */
/*
 * The notion that there is such a thing as a "PCL printer" is a fiction: no
 * two "PCL" printers, even at the same PCL level, have identical command
 * sets. (The H-P documentation isn't fully accurate either; for example,
 * it doesn't reveal that the DeskJet printers implement anything beyond PCL
 * 3.)
 *
 * This file contains feature definitions for a generic monochrome PCL
 * driver (gdevdljm.c), and the specific feature values for all such
 * printers that Ghostscript currently supports.
 */

/* Printer spacing capabilities. Include at most one of these. */
#define PCL_NO_SPACING	0	/* no vertical spacing capability, must be 0 */
#define PCL3_SPACING	1	/* <ESC>*p+<n>Y (PCL 3) */
#define PCL4_SPACING	2	/* <ESC>*b<n>Y (PCL 4) */
#define PCL5_SPACING	4	/* <ESC>*b<n>Y and clear seed row (PCL 5) */
/* The following is only used internally. */
#define PCL_ANY_SPACING \
	(PCL3_SPACING | PCL4_SPACING | PCL5_SPACING)

/* Individual printer properties.  Any subset of these may be included. */
#define PCL_MODE_2_COMPRESSION		8	/* compression mode 2 supported */
						/* (PCL 4) */
#define PCL_MODE_3_COMPRESSION		16	/* compression modes 2 & 3 supported */
						/* (PCL 5) */
#define PCL_END_GRAPHICS_DOES_RESET	32	/* <esc>*rB resets all parameters */
#define PCL_HAS_DUPLEX			64	/* <esc>&l<duplex>S supported */
#define PCL_CAN_SET_PAPER_SIZE		128	/* <esc>&l<sizecode>A supported */
#define PCL_CAN_PRINT_COPIES		256	/* <esc>&l<copies>X supported */
#define HACK__IS_A_LJET4PJL		512
#define HACK__IS_A_OCE9050		1024

/* Shorthands for the most common spacing/compression combinations. */
#define PCL_MODE0 PCL3_SPACING
#define PCL_MODE0NS PCL_NO_SPACING
#define PCL_MODE2 (PCL4_SPACING | PCL_MODE_2_COMPRESSION)
#define PCL_MODE2P (PCL_NO_SPACING | PCL_MODE_2_COMPRESSION)
#define PCL_MODE3 (PCL5_SPACING | PCL_MODE_3_COMPRESSION)
#define PCL_MODE3NS (PCL_NO_SPACING | PCL_MODE_3_COMPRESSION)

#define MIN_SKIP_LINES 7
static const char *const from2to3 = "\033*b3M";
static const char *const from3to2 = "\033*b2M";
static const int penalty_from2to3 = 5; /* strlen(from2to3); */
static const int penalty_from3to2 = 5; /* strlen(from3to2); */

/* H-P DeskJet */
static const fz_pcl_options fz_pcl_options_ljet4 =
{
	(PCL_MODE2 | PCL_END_GRAPHICS_DOES_RESET | PCL_CAN_SET_PAPER_SIZE),
	"\033&k1W\033*b2M",
	"\033&k1W\033*b2M"
};

/* H-P DeskJet 500 */
static const fz_pcl_options fz_pcl_options_dj500 =
{
	(PCL_MODE3 | PCL_END_GRAPHICS_DOES_RESET | PCL_CAN_SET_PAPER_SIZE),
	"\033&k1W",
	"\033&k1W"
};

/* Kyocera FS-600 */
static const fz_pcl_options fz_pcl_options_fs600 =
{
	(PCL_MODE3 | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033*r0F\033&u%dD",
	"\033*r0F\033&u%dD"
};

/* H-P original LaserJet */
/* H-P LaserJet Plus */
static const fz_pcl_options fz_pcl_options_lj =
{
	(PCL_MODE0),
	"\033*b0M",
	"\033*b0M"
};

/* H-P LaserJet IIp, IId */
static const fz_pcl_options fz_pcl_options_lj2 =
{
	(PCL_MODE2P | PCL_CAN_SET_PAPER_SIZE),
	"\033*r0F\033*b2M",
	"\033*r0F\033*b2M"
};

/* H-P LaserJet III* */
static const fz_pcl_options fz_pcl_options_lj3 =
{
	(PCL_MODE3 | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033&l-180u36Z\033*r0F",
	"\033&l-180u36Z\033*r0F"
};

/* H-P LaserJet IIId */
static const fz_pcl_options fz_pcl_options_lj3d =
{
	(PCL_MODE3 | PCL_HAS_DUPLEX | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033&l-180u36Z\033*r0F",
	"\033&l180u36Z\033*r0F"
};

/* H-P LaserJet 4 */
static const fz_pcl_options fz_pcl_options_lj4 =
{
	(PCL_MODE3 | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033&l-180u36Z\033*r0F\033&u%dD",
	"\033&l-180u36Z\033*r0F\033&u%dD"
};

/* H-P LaserJet 4 PL */
static const fz_pcl_options fz_pcl_options_lj4pl =
{
	(PCL_MODE3 | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES | HACK__IS_A_LJET4PJL),
	"\033&l-180u36Z\033*r0F\033&u%dD",
	"\033&l-180u36Z\033*r0F\033&u%dD"
};

/* H-P LaserJet 4d */
static const fz_pcl_options fz_pcl_options_lj4d =
{
	(PCL_MODE3 | PCL_HAS_DUPLEX | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033&l-180u36Z\033*r0F\033&u%dD",
	"\033&l180u36Z\033*r0F\033&u%dD"
};

/* H-P 2563B line printer */
static const fz_pcl_options fz_pcl_options_lp2563b =
{
	(PCL_MODE0NS | PCL_CAN_SET_PAPER_SIZE),
	"\033*b0M",
	"\033*b0M"
};

/* OCE 9050 line printer */
static const fz_pcl_options fz_pcl_options_oce9050 =
{
	(PCL_MODE3NS | PCL_CAN_SET_PAPER_SIZE | HACK__IS_A_OCE9050),
	"\033*b0M",
	"\033*b0M"
};

static void copy_opts(fz_pcl_options *dst, const fz_pcl_options *src)
{
	if (dst)
		*dst = *src;
}

void fz_pcl_preset(fz_context *ctx, fz_pcl_options *opts, const char *preset)
{
	if (preset == NULL || *preset == 0 || !strcmp(preset, "ljet4"))
		copy_opts(opts, &fz_pcl_options_ljet4);
	else if (!strcmp(preset, "dj500"))
		copy_opts(opts, &fz_pcl_options_dj500);
	else if (!strcmp(preset, "fs600"))
		copy_opts(opts, &fz_pcl_options_fs600);
	else if (!strcmp(preset, "lj"))
		copy_opts(opts, &fz_pcl_options_lj);
	else if (!strcmp(preset, "lj2"))
		copy_opts(opts, &fz_pcl_options_lj2);
	else if (!strcmp(preset, "lj3"))
		copy_opts(opts, &fz_pcl_options_lj3);
	else if (!strcmp(preset, "lj3d"))
		copy_opts(opts, &fz_pcl_options_lj3d);
	else if (!strcmp(preset, "lj4"))
		copy_opts(opts, &fz_pcl_options_lj4);
	else if (!strcmp(preset, "lj4pl"))
		copy_opts(opts, &fz_pcl_options_lj4pl);
	else if (!strcmp(preset, "lj4d"))
		copy_opts(opts, &fz_pcl_options_lj4d);
	else if (!strcmp(preset, "lp2563b"))
		copy_opts(opts, &fz_pcl_options_lp2563b);
	else if (!strcmp(preset, "oce9050"))
		copy_opts(opts, &fz_pcl_options_oce9050);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unknown preset '%s'", preset);
}

void fz_pcl_option(fz_context *ctx, fz_pcl_options *opts, const char *option, int val)
{
	if (opts == NULL)
		return;

	if (!strcmp(option, "spacing"))
	{
		switch (val)
		{
		case 0:
			opts->features &= ~PCL_ANY_SPACING;
			break;
		case 1:
			opts->features = (opts->features & ~PCL_ANY_SPACING) | PCL3_SPACING;
			break;
		case 2:
			opts->features = (opts->features & ~PCL_ANY_SPACING) | PCL4_SPACING;
			break;
		case 3:
			opts->features = (opts->features & ~PCL_ANY_SPACING) | PCL5_SPACING;
			break;
		default:
			fz_throw(ctx, FZ_ERROR_GENERIC, "Unsupported PCL spacing %d (0-3 only)", val);
		}
	}
	else if (!strcmp(option, "mode2"))
	{
		if (val == 0)
			opts->features &= ~PCL_MODE_2_COMPRESSION;
		else if (val == 1)
			opts->features |= PCL_MODE_2_COMPRESSION;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 0 or 1 for mode2 value");
	}
	else if (!strcmp(option, "mode3"))
	{
		if (val == 0)
			opts->features &= ~PCL_MODE_3_COMPRESSION;
		else if (val == 1)
			opts->features |= PCL_MODE_3_COMPRESSION;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 0 or 1 for mode3 value");
	}
	else if (!strcmp(option, "eog_reset"))
	{
		if (val == 0)
			opts->features &= ~PCL_END_GRAPHICS_DOES_RESET;
		else if (val == 1)
			opts->features |= PCL_END_GRAPHICS_DOES_RESET;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 0 or 1 for eog_reset value");
	}
	else if (!strcmp(option, "has_duplex"))
	{
		if (val == 0)
			opts->features &= ~PCL_HAS_DUPLEX;
		else if (val == 1)
			opts->features |= PCL_HAS_DUPLEX;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 0 or 1 for has_duplex value");
	}
	else if (!strcmp(option, "has_papersize"))
	{
		if (val == 0)
			opts->features &= ~PCL_CAN_SET_PAPER_SIZE;
		else if (val == 1)
			opts->features |= PCL_CAN_SET_PAPER_SIZE;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 0 or 1 for has_papersize value");
	}
	else if (!strcmp(option, "has_copies"))
	{
		if (val == 0)
			opts->features &= ~PCL_CAN_PRINT_COPIES;
		else if (val == 1)
			opts->features |= PCL_CAN_PRINT_COPIES;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 0 or 1 for has_papersize value");
	}
	else if (!strcmp(option, "is_ljet4pjl"))
	{
		if (val == 0)
			opts->features &= ~HACK__IS_A_LJET4PJL;
		else if (val == 1)
			opts->features |= HACK__IS_A_LJET4PJL;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 0 or 1 for is_ljet4pjl value");
	}
	else if (!strcmp(option, "is_oce9050"))
	{
		if (val == 0)
			opts->features &= ~HACK__IS_A_OCE9050;
		else if (val == 1)
			opts->features |= HACK__IS_A_OCE9050;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 0 or 1 for is_oce9050 value");
	}
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unknown pcl option '%s'", option);
}

static void
make_init(fz_pcl_options *pcl, char *buf, unsigned long len, const char *str, int res)
{
	int paper_source = -1;

	snprintf(buf, len, str, res);

	if (pcl->manual_feed_set && pcl->manual_feed)
		paper_source = 2;
	else if (pcl->media_position_set && pcl->media_position >= 0)
		paper_source = pcl->media_position;
	if (paper_source >= 0)
	{
		char buf2[40];
		snprintf(buf2, sizeof(buf2), "\033&l%dH", paper_source);
		strncat(buf, buf2, len);
	}
}

static void
pcl_header(fz_output *out, fz_pcl_options *pcl, int num_copies, int xres)
{
	char odd_page_init[80];
	char even_page_init[80];

	make_init(pcl, odd_page_init, sizeof(odd_page_init), pcl->odd_page_init, xres);
	make_init(pcl, even_page_init, sizeof(even_page_init), pcl->even_page_init, xres);

	if (pcl->page_count == 0)
	{
		if (pcl->features & HACK__IS_A_LJET4PJL)
			fz_puts(out, "\033%-12345X@PJL\r\n@PJL ENTER LANGUAGE = PCL\r\n");
		fz_puts(out, "\033E"); /* reset printer */
		/* If the printer supports it, set the paper size */
		/* based on the actual requested size. */
		if (pcl->features & PCL_CAN_SET_PAPER_SIZE)
			fz_printf(out, "\033&l%dA", pcl->paper_size);
		/* If printer can duplex, set duplex mode appropriately. */
		if (pcl->features & PCL_HAS_DUPLEX)
		{
			if (pcl->duplex_set)
			{
				if (pcl->duplex)
				{
					if (!pcl->tumble)
						fz_puts(out, "\033&l1S");
					else
						fz_puts(out, "\033&l2S");
				}
				else
					fz_puts(out, "\033&l0S");
			}
			else
			{
				/* default to duplex for this printer */
				fz_puts(out, "\033&l1S");
			}
		}
	}

	/* Put out per-page initialization. */
	/* in duplex mode the sheet is already in process, so there are some
	 * commands which must not be sent to the printer for the 2nd page,
	 * as this commands will cause the printer to eject the sheet with
	 * only the 1st page printed. This commands are:
	 * \033&l%dA (setting paper size)
	 * \033&l%dH (setting paper tray)
	 * in simplex mode we set this parameters for each page,
	 * in duplex mode we set this parameters for each odd page
	 */

	if ((pcl->features & PCL_HAS_DUPLEX) && pcl->duplex_set && pcl->duplex)
	{
		/* We are printing duplex, so change margins as needed */
		if (((pcl->page_count/num_copies)%2) == 0)
		{
			if (pcl->page_count != 0 && (pcl->features & PCL_CAN_SET_PAPER_SIZE))
			{
				fz_printf(out, "\033&l%dA", pcl->paper_size);
			}
			fz_puts(out, "\033&l0o0l0E");
			fz_puts(out, pcl->odd_page_init);
		}
		else
			fz_puts(out, pcl->even_page_init);
	}
	else
	{
		if (pcl->features & PCL_CAN_SET_PAPER_SIZE)
		{
			fz_printf(out, "\033&l%dA", pcl->paper_size);
		}
		fz_puts(out, "\033&l0o0l0E");
		fz_puts(out, pcl->odd_page_init);
	}

	fz_printf(out, "\033&l%dX", num_copies); /* # of copies */

	/* End raster graphics, position cursor at top. */
	fz_puts(out, "\033*rB\033*p0x0Y");

	/* The DeskJet and DeskJet Plus reset everything upon */
	/* receiving \033*rB, so we must reinitialize graphics mode. */
	if (pcl->features & PCL_END_GRAPHICS_DOES_RESET)
	{
		fz_puts(out, pcl->odd_page_init); /* Assume this does the right thing */
		fz_printf(out, "\033&l%dX", num_copies); /* # of copies */
	}

	/* Set resolution. */
	fz_printf(out, "\033*t%dR", xres);
	pcl->page_count++;
}

void
fz_output_pcl(fz_output *out, const fz_pixmap *pixmap, fz_pcl_options *pcl)
{
	//unsigned char *sp;
	//int y, x, sn, dn, ss;
	fz_context *ctx;

	if (!out || !pixmap)
		return;

	ctx = out->ctx;

	if (pixmap->n != 1 && pixmap->n != 2 && pixmap->n != 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as pcl");

	pcl_header(out, pcl, 1, pixmap->xres);

#if 0
	sn = pixmap->n;
	dn = pixmap->n;
	if (dn == 2 || dn == 4)
		dn--;

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
#endif
}

/*
 * Mode 2 Row compression routine for the HP DeskJet & LaserJet IIp.
 * Compresses data from row up to end_row, storing the result
 * starting at compressed.  Returns the number of bytes stored.
 * Runs of K<=127 literal bytes are encoded as K-1 followed by
 * the bytes; runs of 2<=K<=127 identical bytes are encoded as
 * 257-K followed by the byte.
 * In the worst case, the result is N+(N/127)+1 bytes long,
 * where N is the original byte count (end_row - row).
 */
int
mode2compress(unsigned char *out, unsigned char *in, int in_len)
{
	int x;
	int out_len = 0;
	int run;

	for (x = 0; x < in_len; x += run)
	{
		/* How far do we have to look to find a value that isn't repeated? */
		for (run = 1; run < 127 && x+run < in_len; run++)
			if (in[0] != in[run])
				break;
		if (run > 1)
		{
			/* We have a run of matching bytes */
			out[out_len++] = 257-run;
			out[out_len++] = in[0];
		}
		else
		{
			int i;

			/* How many literals do we need to copy? */
			for (run = 1; run < 127 && x+run < in_len; run++)
				if (in[run] == in[run+1])
					break;
			out[out_len++] = run-1;
			for (i = 0; i < run; i++)
				out[out_len++] = in[i];
		}
		in += run;
	}
	return out_len;
}

/*
 * Mode 3 compression routine for the HP LaserJet III family.
 * Compresses bytecount bytes starting at current, storing the result
 * in compressed, comparing against and updating previous.
 * Returns the number of bytes stored.	In the worst case,
 * the number of bytes is bytecount+(bytecount/8)+1.
 */
int
mode3compress(unsigned char *out, const unsigned char *in, unsigned char *prev, int in_len)
{
	unsigned char *compressed = out;
	const unsigned char *cur = in;
	const unsigned char *end = in + in_len;

	while (cur < end) {		/* Detect a maximum run of unchanged bytes. */
		const unsigned char *run = cur;
		const unsigned char *diff;
		const unsigned char *stop;
		int offset, cbyte;

		while (cur < end && *cur == *prev) {
			cur++, prev++;
		}
		if (cur == end)
			break;		/* rest of row is unchanged */
		/* Detect a run of up to 8 changed bytes. */
		/* We know that *cur != *prev. */
		diff = cur;
		stop = (end - cur > 8 ? cur + 8 : end);
		do
		{
			*prev++ = *cur++;
		}
		while (cur < stop && *cur != *prev);
		/* Now [run..diff) are unchanged, and */
		/* [diff..cur) are changed. */
		/* Generate the command byte(s). */
		offset = diff - run;
		cbyte = (cur - diff - 1) << 5;
		if (offset < 31)
			*out++ = cbyte + offset;
		else {
			*out++ = cbyte + 31;
			offset -= 31;
			while (offset >= 255)
				*out++ = 255, offset -= 255;
			*out++ = offset;
		}
		/* Copy the changed data. */
		while (diff < cur)
			*out++ = *diff++;
	}
	return out - compressed;
}

void wind(void)
{}

void
fz_output_pcl_bitmap(fz_output *out, const fz_bitmap *bitmap, fz_pcl_options *pcl)
{
	unsigned char *data, *out_data;
	int y, ss, rmask, line_size;
	fz_context *ctx;
	int num_blank_lines;
	int compression = -1;
	unsigned char *prev_row = NULL;
	unsigned char *out_row_mode_2 = NULL;
	unsigned char *out_row_mode_3 = NULL;
	int out_count;
	int max_mode_2_size;
	int max_mode_3_size;

	if (!out || !bitmap)
		return;

	ctx = out->ctx;

	if (pcl->features & HACK__IS_A_OCE9050)
	{
		/* Enter HPGL/2 mode, begin plot, Initialise (start plot), Enter PCL mode */
		fz_puts(out, "\033%1BBPIN;\033%1A");
	}

	pcl_header(out, pcl, 1, bitmap->xres);

	fz_var(prev_row);
	fz_var(out_row_mode_2);
	fz_var(out_row_mode_3);

	fz_try(ctx)
	{
		num_blank_lines = 0;
		rmask = ~0 << (-bitmap->w & 7);
		line_size = (bitmap->w + 7)/8;
		max_mode_2_size = line_size + (line_size/127) + 1;
		max_mode_3_size = line_size + (line_size/8) + 1;
		prev_row = fz_calloc(ctx, line_size, sizeof(unsigned char));
		out_row_mode_2 = fz_calloc(ctx, max_mode_2_size, sizeof(unsigned char));
		out_row_mode_3 = fz_calloc(ctx, max_mode_3_size, sizeof(unsigned char));

		/* Transfer raster graphics. */
		data = bitmap->samples;
		ss = bitmap->stride;
		for (y = 0; y < bitmap->h; y++, data += ss)
		{
			unsigned char *end_data = data + line_size;

			if ((end_data[-1] & rmask) == 0)
			{
				end_data--;
				while (end_data > data && end_data[-1] == 0)
					end_data--;
			}
			if (end_data == data)
			{
				/* Blank line */
				num_blank_lines++;
				continue;
			}
			wind();

			/* We've reached a non-blank line. */
			/* Put out a spacing command if necessary. */
			if (num_blank_lines == y) {
				/* We're at the top of a page. */
				if (pcl->features & PCL_ANY_SPACING)
				{
					if (num_blank_lines > 0)
						fz_printf(out, "\033*p+%dY", num_blank_lines * bitmap->yres);
					/* Start raster graphics. */
					fz_puts(out, "\033*r1A");
				}
				else if (pcl->features & PCL_MODE_3_COMPRESSION)
				{
					/* Start raster graphics. */
					fz_puts(out, "\033*r1A");
					for (; num_blank_lines; num_blank_lines--)
						fz_puts(out, "\033*b0W");
				}
				else
				{
					/* Start raster graphics. */
					fz_puts(out, "\033*r1A");
					for (; num_blank_lines; num_blank_lines--)
						fz_puts(out, "\033*bW");
				}
			}

			/* Skip blank lines if any */
			else if (num_blank_lines != 0)
			{
				/* Moving down from current position causes head
				 * motion on the DeskJet, so if the number of lines
				 * is small, we're better off printing blanks.
				 *
				 * For Canon LBP4i and some others, <ESC>*b<n>Y
				 * doesn't properly clear the seed row if we are in
				 * compression mode 3.
				 */
				if ((num_blank_lines < MIN_SKIP_LINES && compression != 3) ||
						!(pcl->features & PCL_ANY_SPACING))
				{
					int mode_3ns = ((pcl->features & PCL_MODE_3_COMPRESSION) && !(pcl->features & PCL_ANY_SPACING));
					if (mode_3ns && compression != 2)
					{
						/* Switch to mode 2 */
						fz_puts(out, from3to2);
						compression = 2;
					}
					if (pcl->features & PCL_MODE_3_COMPRESSION)
					{
						/* Must clear the seed row. */
						fz_puts(out, "\033*b1Y");
						num_blank_lines--;
					}
					if (mode_3ns)
					{
						for (; num_blank_lines; num_blank_lines--)
							fz_puts(out, "\033*b0W");
					}
					else
					{
						for (; num_blank_lines; num_blank_lines--)
							fz_puts(out, "\033*bW");
					}
				}
				else if (pcl->features & PCL3_SPACING)
					fz_printf(out, "\033*p+%dY", num_blank_lines * bitmap->yres);
				else
					fz_printf(out, "\033*b%dY", num_blank_lines);
				/* Clear the seed row (only matters for mode 3 compression). */
				memset(prev_row, 0, line_size);
			}
			num_blank_lines = 0;

			/* Choose the best compression mode for this particular line. */
			if (pcl->features & PCL_MODE_3_COMPRESSION)
			{
				/* Compression modes 2 and 3 are both available. Try
				 * both and see which produces the least output data.
				 */
				int count3 = mode3compress(out_row_mode_3, data, prev_row, line_size);
				int count2 = mode2compress(out_row_mode_2, data, line_size);
				int penalty3 = (compression == 3 ? 0 : penalty_from2to3);
				int penalty2 = (compression == 2 ? 0 : penalty_from3to2);

				if (count3 + penalty3 < count2 + penalty2)
				{
					if (compression != 3)
						fz_puts(out, from2to3);
					compression = 3;
					out_data = (unsigned char *)out_row_mode_3;
					out_count = count3;
				}
				else
				{
					if (compression != 2)
						fz_puts(out, from3to2);
					compression = 2;
					out_data = (unsigned char *)out_row_mode_2;
					out_count = count2;
				}
			}
			else if (pcl->features & PCL_MODE_2_COMPRESSION)
			{
				out_data = out_row_mode_2;
				out_count = mode2compress(out_row_mode_2, data, line_size);
			}
			else
			{
				out_data = data;
				out_count = line_size;
			}

			/* Transfer the data */
			fz_printf(out, "\033*b%dW", out_count);
			fz_write(out, out_data, out_count);
		}

		/* end raster graphics and eject page */
		fz_puts(out, "\033*rB\f");

		if (pcl->features & HACK__IS_A_OCE9050)
		{
			/* Pen up, pen select, advance full page, reset */
			fz_puts(out, "\033%1BPUSP0PG;\033E");
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, prev_row);
		fz_free(ctx, out_row_mode_2);
		fz_free(ctx, out_row_mode_3);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
fz_write_pcl(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, fz_pcl_options *pcl)
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
		fz_output_pcl(out, pixmap, pcl);
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
fz_write_pcl_bitmap(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, fz_pcl_options *pcl)
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
		fz_output_pcl_bitmap(out, bitmap, pcl);
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
