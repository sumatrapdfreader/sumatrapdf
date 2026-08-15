// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

/* Fax G3/G4 tables */

typedef struct
{
	unsigned short code;
	unsigned short nbits;
} cfe_code;

typedef struct {
	cfe_code termination[64];
	cfe_code makeup[41];
} cf_runs;

/* Define the end-of-line code. */
static const cfe_code cf_run_eol = {1, 12};

/* Define the 2-D run codes. */
static const cfe_code cf2_run_pass = {0x1, 4};
static const cfe_code cf2_run_vertical[7] =
{
	{0x3, 7},
	{0x3, 6},
	{0x3, 3},
	{0x1, 1},
	{0x2, 3},
	{0x2, 6},
	{0x2, 7}
};
static const cfe_code cf2_run_horizontal = {1, 3};

/* White run codes. */
static const cf_runs cf_white_runs =
{
	/* Termination codes */
	{
		{0x35, 8}, {0x7, 6}, {0x7, 4}, {0x8, 4},
		{0xb, 4}, {0xc, 4}, {0xe, 4}, {0xf, 4},
		{0x13, 5}, {0x14, 5}, {0x7, 5}, {0x8, 5},
		{0x8, 6}, {0x3, 6}, {0x34, 6}, {0x35, 6},
		{0x2a, 6}, {0x2b, 6}, {0x27, 7}, {0xc, 7},
		{0x8, 7}, {0x17, 7}, {0x3, 7}, {0x4, 7},
		{0x28, 7}, {0x2b, 7}, {0x13, 7}, {0x24, 7},
		{0x18, 7}, {0x2, 8}, {0x3, 8}, {0x1a, 8},
		{0x1b, 8}, {0x12, 8}, {0x13, 8}, {0x14, 8},
		{0x15, 8}, {0x16, 8}, {0x17, 8}, {0x28, 8},
		{0x29, 8}, {0x2a, 8}, {0x2b, 8}, {0x2c, 8},
		{0x2d, 8}, {0x4, 8}, {0x5, 8}, {0xa, 8},
		{0xb, 8}, {0x52, 8}, {0x53, 8}, {0x54, 8},
		{0x55, 8}, {0x24, 8}, {0x25, 8}, {0x58, 8},
		{0x59, 8}, {0x5a, 8}, {0x5b, 8}, {0x4a, 8},
		{0x4b, 8}, {0x32, 8}, {0x33, 8}, {0x34, 8}
	},

	/* Make-up codes */
	{
		{0, 0} /* dummy */ , {0x1b, 5}, {0x12, 5}, {0x17, 6},
		{0x37, 7}, {0x36, 8}, {0x37, 8}, {0x64, 8},
		{0x65, 8}, {0x68, 8}, {0x67, 8}, {0xcc, 9},
		{0xcd, 9}, {0xd2, 9}, {0xd3, 9}, {0xd4, 9},
		{0xd5, 9}, {0xd6, 9}, {0xd7, 9}, {0xd8, 9},
		{0xd9, 9}, {0xda, 9}, {0xdb, 9}, {0x98, 9},
		{0x99, 9}, {0x9a, 9}, {0x18, 6}, {0x9b, 9},
		{0x8, 11}, {0xc, 11}, {0xd, 11}, {0x12, 12},
		{0x13, 12}, {0x14, 12}, {0x15, 12}, {0x16, 12},
		{0x17, 12}, {0x1c, 12}, {0x1d, 12}, {0x1e, 12},
		{0x1f, 12}
	}
};

/* Black run codes. */
static const cf_runs cf_black_runs =
{
	/* Termination codes */
	{
		{0x37, 10}, {0x2, 3}, {0x3, 2}, {0x2, 2},
		{0x3, 3}, {0x3, 4}, {0x2, 4}, {0x3, 5},
		{0x5, 6}, {0x4, 6}, {0x4, 7}, {0x5, 7},
		{0x7, 7}, {0x4, 8}, {0x7, 8}, {0x18, 9},
		{0x17, 10}, {0x18, 10}, {0x8, 10}, {0x67, 11},
		{0x68, 11}, {0x6c, 11}, {0x37, 11}, {0x28, 11},
		{0x17, 11}, {0x18, 11}, {0xca, 12}, {0xcb, 12},
		{0xcc, 12}, {0xcd, 12}, {0x68, 12}, {0x69, 12},
		{0x6a, 12}, {0x6b, 12}, {0xd2, 12}, {0xd3, 12},
		{0xd4, 12}, {0xd5, 12}, {0xd6, 12}, {0xd7, 12},
		{0x6c, 12}, {0x6d, 12}, {0xda, 12}, {0xdb, 12},
		{0x54, 12}, {0x55, 12}, {0x56, 12}, {0x57, 12},
		{0x64, 12}, {0x65, 12}, {0x52, 12}, {0x53, 12},
		{0x24, 12}, {0x37, 12}, {0x38, 12}, {0x27, 12},
		{0x28, 12}, {0x58, 12}, {0x59, 12}, {0x2b, 12},
		{0x2c, 12}, {0x5a, 12}, {0x66, 12}, {0x67, 12}
	},

	/* Make-up codes. */
	{
		{0, 0} /* dummy */ , {0xf, 10}, {0xc8, 12}, {0xc9, 12},
		{0x5b, 12}, {0x33, 12}, {0x34, 12}, {0x35, 12},
		{0x6c, 13}, {0x6d, 13}, {0x4a, 13}, {0x4b, 13},
		{0x4c, 13}, {0x4d, 13}, {0x72, 13}, {0x73, 13},
		{0x74, 13}, {0x75, 13}, {0x76, 13}, {0x77, 13},
		{0x52, 13}, {0x53, 13}, {0x54, 13}, {0x55, 13},
		{0x5a, 13}, {0x5b, 13}, {0x64, 13}, {0x65, 13},
		{0x8, 11}, {0xc, 11}, {0xd, 11}, {0x12, 12},
		{0x13, 12}, {0x14, 12}, {0x15, 12}, {0x16, 12},
		{0x17, 12}, {0x1c, 12}, {0x1d, 12}, {0x1e, 12},
		{0x1f, 12}
	}
};

static inline int
getbit(const unsigned char *buf, int x)
{
	/* Invert bit to handle BlackIs1=false */
	return ( ( buf[x >> 3] >> ( 7 - (x & 7) ) ) & 1 ) ^ 1;
}

static inline int
find_changing(const unsigned char *line, int x, int w)
{
	int a, b;

	if (!line || x >= w)
		return w;

	if (x == -1)
	{
		a = 0;
		x = 0;
	}
	else
	{
		a = getbit(line, x);
		x++;
	}
	while (x < w)
	{
		b = getbit(line, x);
		if (a != b)
			break;
		x++;
	}

	return x;
}

static inline int
find_changing_color(const unsigned char *line, int x, int w, int color)
{
	if (!line || x >= w)
		return w;
	x = find_changing(line, x, w);
	if (x < w && getbit(line, x) != color)
		x = find_changing(line, x, w);
	return x;
}

static inline int
getrun(const unsigned char *line, int x, int w, int c)
{
	int z = x;
	while (z < w)
	{
		int b = getbit(line, z);
		if (c != b)
			break;
		++z;
	}
	return z - x;
}

static inline void
putcode(fz_context *ctx, fz_buffer *out, const cfe_code *run)
{
	fz_append_bits(ctx, out, run->code, run->nbits);
}

static void
putrun(fz_context *ctx, fz_buffer *out, int run, int c)
{
	const cf_runs *codetable = c ? &cf_black_runs : &cf_white_runs;
	if (run > 63)
	{
		int m = run >> 6;
		while (m > 40)
		{
			putcode(ctx, out, &codetable->makeup[40]);
			m -= 40;
		}
		if (m > 0)
			putcode(ctx, out, &codetable->makeup[m]);
		putcode(ctx, out, &codetable->termination[run & 63]);
	}
	else
	{
		putcode(ctx, out, &codetable->termination[run]);
	}
}

fz_buffer *
fz_compress_ccitt_fax_g4(fz_context *ctx, const unsigned char *src, int columns, int rows, ptrdiff_t stride)
{
	fz_buffer *out = fz_new_buffer(ctx, (stride * rows) >> 3);
	const unsigned char *ref = NULL;

	fz_try(ctx)
	{
		while (rows-- > 0)
		{
			int a0 = -1;
			int c = 0;

			while (a0 < columns)
			{
				int a1 = find_changing(src, a0, columns);
				int b1 = find_changing_color(ref, a0, columns, c^1);
				int b2 = find_changing(ref, b1, columns);
				int diff = b1 - a1;
				if (a0 < 0)
					a0 = 0;

				/* pass mode */
				if (b2 < a1)
				{
					putcode(ctx, out, &cf2_run_pass);
					a0 = b2;
				}

				/* vertical mode */
				else if (diff >= -3 && diff <= 3)
				{
					putcode(ctx, out, &cf2_run_vertical[diff + 3]);
					a0 = a1;
					c = c^1;
				}

				/* horizontal mode */
				else
				{
					int a2 = find_changing(src, a1, columns);
					putcode(ctx, out, &cf2_run_horizontal);
					putrun(ctx, out, a1 - a0, c);
					putrun(ctx, out, a2 - a1, c^1);
					a0 = a2;
				}
			}

			ref = src;
			src += stride;
		}

		putcode(ctx, out, &cf_run_eol);
		putcode(ctx, out, &cf_run_eol);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, out);
		fz_rethrow(ctx);
	}

	return out;
}

fz_buffer *
fz_compress_ccitt_fax_g3(fz_context *ctx, const unsigned char *src, int columns, int rows, ptrdiff_t stride)
{
	fz_buffer *out = fz_new_buffer(ctx, (stride * rows) >> 3);
	int i;

	fz_try(ctx)
	{
		while (rows-- > 0)
		{
			int a0 = 0;
			int c = 0;
			while (a0 < columns)
			{
				int run = getrun(src, a0, columns, c);
				putrun(ctx, out, run, c);
				a0 += run;
				c = c^1;
			}
			src += stride;
		}

		for (i = 0; i < 6; ++i)
			putcode(ctx, out, &cf_run_eol);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, out);
		fz_rethrow(ctx);
	}

	return out;
}
