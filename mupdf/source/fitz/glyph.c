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

#include "glyph-imp.h"
#include "pixmap-imp.h"

#include <string.h>

#define RLE_THRESHOLD 256

fz_glyph *
fz_keep_glyph(fz_context *ctx, fz_glyph *glyph)
{
	return fz_keep_storable(ctx, &glyph->storable);
}

void
fz_drop_glyph(fz_context *ctx, fz_glyph *glyph)
{
	fz_drop_storable(ctx, &glyph->storable);
}

static void
fz_drop_glyph_imp(fz_context *ctx, fz_storable *glyph_)
{
	fz_glyph *glyph = (fz_glyph *)glyph_;
	fz_drop_pixmap(ctx, glyph->pixmap);
	fz_free(ctx, glyph);
}

fz_irect
fz_glyph_bbox(fz_context *ctx, fz_glyph *glyph)
{
	fz_irect bbox;
	bbox.x0 = glyph->x;
	bbox.y0 = glyph->y;
	bbox.x1 = glyph->x + glyph->w;
	bbox.y1 = glyph->y + glyph->h;
	return bbox;
}

fz_irect
fz_glyph_bbox_no_ctx(fz_glyph *glyph)
{
	fz_irect bbox;
	bbox.x0 = glyph->x;
	bbox.y0 = glyph->y;
	bbox.x1 = glyph->x + glyph->w;
	bbox.y1 = glyph->y + glyph->h;
	return bbox;
}

int
fz_glyph_width(fz_context *ctx, fz_glyph *glyph)
{
	return glyph->w;
}

int
fz_glyph_height(fz_context *ctx, fz_glyph *glyph)
{
	return glyph->h;
}

#ifndef NDEBUG
#include <stdio.h>

void
fz_dump_glyph(fz_glyph *glyph)
{
	int x, y;

	if (glyph->pixmap)
	{
		printf("pixmap glyph\n");
		return;
	}
	printf("glyph: %dx%d @ (%d,%d)\n", glyph->w, glyph->h, glyph->x, glyph->y);

	for (y = 0; y < glyph->h; y++)
	{
		int offset = ((int *)(glyph->data))[y];
		if (offset >= 0)
		{
			int extend = 0;
			int eol = 0;
			x = glyph->w;
			do
			{
				int v = glyph->data[offset++];
				int len;
				char c;
				switch(v&3)
				{
				case 0: /* extend */
					extend = v>>2;
					len = 0;
					break;
				case 1: /* Transparent pixels */
					len = 1 + (v>>2) + (extend<<6);
					extend = 0;
					c = '.';
					break;
				case 2: /* Solid pixels */
					len = 1 + (v>>3) + (extend<<5);
					extend = 0;
					eol = v & 4;
					c = (eol ? '$' :'#');
					break;
				default: /* Intermediate pixels */
					len = 1 + (v>>3) + (extend<<5);
					extend = 0;
					offset += len;
					eol = v & 4;
					c = (eol ? '!' : '?');
					break;
				}
				x -= len;
				while (len--)
					fputc(c, stdout);
				if (eol)
					break;
			}
			while (x > 0);
		}
		printf("\n");
	}
}
#endif

fz_glyph *
fz_new_glyph_from_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	fz_glyph *glyph = NULL;

	if (pix == NULL)
		return NULL;

	fz_var(glyph);

	fz_try(ctx)
	{
		if (pix->n != 1 || pix->w * pix->h < RLE_THRESHOLD)
		{
			glyph = fz_malloc_struct(ctx, fz_glyph);
			FZ_INIT_STORABLE(glyph, 1, fz_drop_glyph_imp);
			glyph->x = pix->x;
			glyph->y = pix->y;
			glyph->w = pix->w;
			glyph->h = pix->h;
			glyph->size = fz_pixmap_size(ctx, pix);
			glyph->pixmap = fz_keep_pixmap(ctx, pix);
		}
		else
			glyph = fz_new_glyph_from_8bpp_data(ctx, pix->x, pix->y, pix->w, pix->h, pix->samples, pix->stride);
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, pix);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return glyph;
}

fz_glyph *
fz_new_glyph_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span)
{
	fz_glyph *glyph = NULL;
	fz_pixmap *pix = NULL;
	int size, fill, yy;
	unsigned char *orig_sp = sp;

	fz_var(glyph);
	fz_var(pix);

	fz_try(ctx)
	{
		/* We start out by allocating space as large as the pixmap.
		 * If we need more than that give up on using RLE. We can
		 * never hope to beat the pixmap for really small sizes. */
		if (w <= 6 || w * h < RLE_THRESHOLD)
			goto try_pixmap;

		size = h * w;
		fill = h * sizeof(int);
		glyph = Memento_label(fz_malloc(ctx, sizeof(fz_glyph) + size), "fz_glyph(8)");
		FZ_INIT_STORABLE(glyph, 1, fz_drop_glyph_imp);
		glyph->x = x;
		glyph->y = y;
		glyph->w = w;
		glyph->h = h;
		glyph->pixmap = NULL;
		if (h == 0)
		{
			glyph->size = 0;
			break;
		}
		for (yy=0; yy < h; yy++)
		{
			int nonblankfill = fill;
			int nonblankfill_end = fill;
			int linefill = fill;
			int ww = w;
			do
			{
				int code;
				int len = ww;
				int needed;
				unsigned char *ep;
				switch (*sp)
				{
				case 0:
					if (len > 0x1000)
						len = 0x1000;
					ep = sp+len;
					while (++sp != ep && *sp == 0);
					code = 1;
					len -= ep-sp;
					ww -= len;
					needed = fill + 1 + (len > 0x40);
					break;
				case 255:
					if (len > 0x800)
						len = 0x800;
					ep = sp+len;
					while (++sp != ep && *sp == 255);
					code = 2;
					len -= ep-sp;
					ww -= len;
					needed = fill + 1 + (len > 0x20);
					break;
				default:
				{
					unsigned char c;
					if (len > 0x800)
						len = 0x800;
					ep = sp+len;
					while (++sp != ep && (c = *sp) != 255 && c != 0);
					len -= ep-sp;
					ww -= len;
					needed = fill + 1 + len + (len > 0x20);
					code = 3;
				}
				}
				if (needed > size)
					goto try_pixmap;
				if (code == 1)
				{
					if (len > 0x40)
						glyph->data[fill++] = ((len-1)>>6)<<2;
					glyph->data[fill++] = 1 | (((len-1)&63)<<2);
				}
				else
				{
					if (len > 0x20)
						glyph->data[fill++] = ((len-1)>>5)<<2;
					nonblankfill = fill;
					glyph->data[fill++] = code | (((len-1)&31)<<3);
					if (code == 3)
					{
						memcpy(&glyph->data[fill], sp - len, len);
						fill += len;
					}
					nonblankfill_end = fill;
				}
			}
			while (ww > 0);
			if (nonblankfill_end == linefill)
			{
				((int *)(glyph->data))[yy] = -1;
				fill = linefill;
			}
			else
			{
				glyph->data[nonblankfill] |= 4;
				fill = nonblankfill_end;
				((int *)(glyph->data))[yy] = linefill;
			}
			sp += span - w;
		}
		if (fill != size)
		{
			glyph = fz_realloc(ctx, glyph, sizeof(fz_glyph) + fill);
			size = fill;
		}
		glyph->size = size;
		break;

		/* Nasty use of a goto here, but it saves us having to exit
		 * and reenter the try context, and this routine is speed
		 * critical. */
try_pixmap:
		glyph = Memento_label(fz_realloc(ctx, glyph, sizeof(fz_glyph)), "fz_glyph(8r)");
		FZ_INIT_STORABLE(glyph, 1, fz_drop_glyph_imp);
		pix = fz_new_pixmap_from_8bpp_data(ctx, x, y, w, h, orig_sp, span);
		glyph->x = pix->x;
		glyph->y = pix->y;
		glyph->w = pix->w;
		glyph->h = pix->h;
		glyph->size = fz_pixmap_size(ctx, pix);
		glyph->pixmap = pix;
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_free(ctx, glyph);
		fz_rethrow(ctx);
	}

	return glyph;
}

fz_glyph *
fz_new_glyph_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span)
{
	fz_pixmap *pix = NULL;
	fz_glyph *glyph = NULL;
	int size, fill, yy;
	unsigned char *orig_sp = sp;

	fz_var(glyph);
	fz_var(pix);

	fz_try(ctx)
	{
		/* We start out by allocating space as large as the pixmap.
		 * If we need more than that give up on using RLE. We can
		 * never hope to beat the pixmap for really small sizes. */
		if (w <= 6 || w * h < RLE_THRESHOLD)
			goto try_pixmap;

		size = h * w;
		fill = h * sizeof(int);
		glyph = Memento_label(fz_malloc(ctx, sizeof(fz_glyph) + size), "fz_glyph(1)");
		FZ_INIT_STORABLE(glyph, 1, fz_drop_glyph_imp);
		glyph->x = x;
		glyph->y = y;
		glyph->w = w;
		glyph->h = h;
		glyph->pixmap = NULL;
		if (h == 0)
		{
			glyph->size = 0;
			break;
		}
		for (yy=0; yy < h; yy++)
		{
			int nonblankfill = fill;
			int nonblankfill_end = fill;
			int linefill = fill;
			int ww = w;
			int bit = 0x80;
			do
			{
				int len = 0;
				int needed;
				int b = *sp & bit;
				bit >>= 1;
				if (bit == 0)
					bit = 0x80, sp++;
				ww--;
				if (b == 0)
				{
					while (ww > 0 && len < 0xfff && (*sp & bit) == 0)
					{
						bit >>= 1;
						if (bit == 0)
							bit = 0x80, sp++;
						len++;
						ww--;
					}
					needed = fill + (len >= 0x40) + 1;
					if (needed > size)
						goto try_pixmap;
					if (len >= 0x40)
						glyph->data[fill++] = (len>>6)<<2;
					glyph->data[fill++] = 1 | ((len&63)<<2);
				}
				else
				{
					while (ww > 0 && len < 0x7ff && (*sp & bit) != 0)
					{
						bit >>= 1;
						if (bit == 0)
							bit = 0x80, sp++;
						len++;
						ww--;
					}
					needed = fill + (len >= 0x20) + 1;
					if (needed > size)
						goto try_pixmap;
					if (len >= 0x20)
						glyph->data[fill++] = (len>>5)<<2;
					nonblankfill = fill;
					glyph->data[fill++] = 2 | ((len&31)<<3);
					nonblankfill_end = fill;
				}
			}
			while (ww > 0);
			if (nonblankfill_end == linefill)
			{
				((int *)(glyph->data))[yy] = -1;
				fill = linefill;
			}
			else
			{
				glyph->data[nonblankfill] |= 4;
				fill = nonblankfill_end;
				((int *)(glyph->data))[yy] = linefill;
			}
			sp += span - (w>>3);
		}
		if (fill != size)
		{
			glyph = fz_realloc(ctx, glyph, sizeof(fz_glyph) + fill);
			size = fill;
		}
		glyph->size = size;
		break;

		/* Nasty use of a goto here, but it saves us having to exit
		 * and reenter the try context, and this routine is speed
		 * critical. */
try_pixmap:
		glyph = fz_realloc(ctx, glyph, sizeof(fz_glyph));
		FZ_INIT_STORABLE(glyph, 1, fz_drop_glyph_imp);
		pix = fz_new_pixmap_from_1bpp_data(ctx, x, y, w, h, orig_sp, span);
		glyph->x = pix->x;
		glyph->y = pix->y;
		glyph->w = pix->w;
		glyph->h = pix->h;
		glyph->size = fz_pixmap_size(ctx, pix);
		glyph->pixmap = pix;
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_free(ctx, glyph);
		fz_rethrow(ctx);
	}

	return glyph;
}
