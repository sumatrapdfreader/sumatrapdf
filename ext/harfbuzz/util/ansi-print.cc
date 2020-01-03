/*
 * Copyright © 2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ansi-print.hh"

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for isatty() */
#endif

#if defined (_MSC_VER) && (_MSC_VER < 1800)
static inline long int
lround (double x)
{
  if (x >= 0)
    return floor (x + 0.5);
  else
    return ceil (x - 0.5);
}
#endif

#define ESC_E (char)27

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define CELL_W 8
#define CELL_H (2 * CELL_W)

struct color_diff_t
{
  int dot (const color_diff_t &o)
  { return v[0]*o.v[0] + v[1]*o.v[1] + v[2]*o.v[2] + v[3]*o.v[3]; }

  int v[4];
};

struct color_t
{
  static color_t from_ansi (unsigned int x)
  {
    color_t c = {(0xFFu<<24) | ((0xFFu*(x&1))<<16) | ((0xFFu*((x >> 1)&1))<<8) | (0xFFu*((x >> 2)&1))};
    return c;
  }
  unsigned int to_ansi ()
  {
    return ((v >> 23) & 1) | ((v >> 14)&2) | ((v >> 5)&4);
  }

  color_diff_t diff (const color_t &o)
  {
    color_diff_t d;
    for (unsigned int i = 0; i < 4; i++)
      d.v[i] = (int) ((v >> (i*8))&0xFF) - (int) ((o.v >> (i*8))&0xFF);
    return d;
  }

  uint32_t v;
};

struct image_t
{
  public:

  image_t (unsigned int width_,
	   unsigned int height_,
	   const uint32_t *data_,
	   unsigned int stride_) :
		width (width_),
		height (height_),
		own_data (false),
		data ((color_t *) data_),
		stride (stride_) {}
  image_t (unsigned int width_,
	   unsigned int height_) :
		width (width_),
		height (height_),
		own_data (true),
		data ((color_t *) malloc (sizeof (data[0]) * width * height)),
		stride (width) {}
  ~image_t ()
  { if (own_data) free (data); }

  color_t &operator () (unsigned int x, unsigned int y)
  { return data[x + y * stride]; }

  color_t operator () (unsigned int x, unsigned int y) const
  { return data[x + y * stride]; }

  void
  copy_sub_image (const image_t &s,
		  unsigned int x, unsigned int y,
		  unsigned int w, unsigned int h)
  {
    assert (x < width);
    assert (y < height);
    for (unsigned int row = 0; row < h; row++) {
      color_t *p = data + x + MIN (y + row, height - 1) * stride;
      color_t *q = s.data + row * s.stride;
      if (x + w <= width)
	for (unsigned int col = 0; col < w; col++)
	  *q++ = *p++;
      else {
	unsigned int limit = width - x;
	for (unsigned int col = 0; col < limit; col++)
	  *q++ = *p++;
	p--;
	for (unsigned int col = limit; col < w; col++)
	  *q++ = *p;
      }
    }
  }

  const unsigned int width;
  const unsigned int height;

  private:
  bool own_data;
  color_t * const data;
  const unsigned int stride;
};

struct biimage_t
{
  public:

  biimage_t (unsigned int width, unsigned int height) :
		width (width),
		height (height),
		bg (0), fg (0), unicolor (true),
		data ((uint8_t *) malloc (sizeof (data[0]) * width * height)) {}
  ~biimage_t ()
  { free (data); }

  void set (const image_t &image)
  {
    assert (image.width == width);
    assert (image.height == height);
    int freq[8] = {0};
    for (unsigned int y = 0; y < height; y++)
      for (unsigned int x = 0; x < width; x++) {
	color_t c = image (x, y);
	freq[c.to_ansi ()]++;
      }
    bg = 0;
    for (unsigned int i = 1; i < 8; i++)
      if (freq[bg] < freq[i])
	bg = i;
    fg = 0;
    for (unsigned int i = 1; i < 8; i++)
      if (i != bg && freq[fg] < freq[i])
	fg = i;
    if (fg == bg || freq[fg] == 0) {
      fg = bg;
      unicolor = true;
    }
    else
      unicolor = false;

    /* Set the data... */

    if (unicolor) {
      memset (data, 0, sizeof (data[0]) * width * height);
      return;
    }

    color_t bgc = color_t::from_ansi (bg);
    color_t fgc = color_t::from_ansi (fg);
    color_diff_t diff = fgc.diff (bgc);
    int dd = diff.dot (diff);
    for (unsigned int y = 0; y < height; y++)
      for (unsigned int x = 0; x < width; x++) {
	int d = diff.dot (image (x, y).diff (bgc));
	(*this)(x, y) = d < 0 ? 0 : d > dd ? 255 : lround (d * 255. / dd);
      }
  }

  uint8_t &operator () (unsigned int x, unsigned int y)
  { return data[x + y * width]; }

  uint8_t operator () (unsigned int x, unsigned int y) const
  { return data[x + y * width]; }

  const unsigned int width;
  const unsigned int height;
  unsigned int bg;
  unsigned int fg;
  bool unicolor;

  private:
  uint8_t * const data;
};

static const char *
block_best (const biimage_t &bi, bool *inverse)
{
  assert (bi.width  <= CELL_W);
  assert (bi.height <= CELL_H);

  unsigned int score = (unsigned int) -1;
  unsigned int row_sum[CELL_H] = {0};
  unsigned int col_sum[CELL_W] = {0};
  unsigned int row_sum_i[CELL_H] = {0};
  unsigned int col_sum_i[CELL_W] = {0};
  unsigned int quad[2][2] = {{0}};
  unsigned int quad_i[2][2] = {{0}};
  unsigned int total = 0;
  unsigned int total_i = 0;
  for (unsigned int y = 0; y < bi.height; y++)
    for (unsigned int x = 0; x < bi.width; x++) {
      unsigned int c = bi (x, y);
      unsigned int c_i = 255 - c;
      row_sum[y] += c;
      row_sum_i[y] += c_i;
      col_sum[x] += c;
      col_sum_i[x] += c_i;
      quad[2 * y / bi.height][2 * x / bi.width] += c;
      quad_i[2 * y / bi.height][2 * x / bi.width] += c_i;
      total += c;
      total_i += c_i;
    }

  /* Make the sums cummulative */
  for (unsigned int i = 1; i < bi.height; i++) {
    row_sum[i] += row_sum[i - 1];
    row_sum_i[i] += row_sum_i[i - 1];
  }
  for (unsigned int i = 1; i < bi.width;  i++) {
    col_sum[i] += col_sum[i - 1];
    col_sum_i[i] += col_sum_i[i - 1];
  }

  const char *best_c = " ";

  /* Maybe empty is better! */
  if (total < score) {
    score = total;
    *inverse = false;
    best_c = " ";
  }
  /* Maybe full is better! */
  if (total_i < score) {
    score = total_i;
    *inverse = true;
    best_c = " ";
  }

  /* Find best lower line */
  if (1) {
    unsigned int best_s = (unsigned int) -1;
    bool best_inv = false;
    int best_i = 0;
    for (unsigned int i = 0; i < bi.height - 1; i++)
    {
      unsigned int s;
      s = row_sum[i] + total_i - row_sum_i[i];
      if (s < best_s) {
	best_s = s;
	best_i = i;
	best_inv = false;
      }
      s = row_sum_i[i] + total - row_sum[i];
      if (s < best_s) {
	best_s = s;
	best_i = i;
	best_inv = true;
      }
    }
    if (best_s < score) {
      static const char *lower[7] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇"};
      unsigned int which = lround ((double) ((best_i + 1) * 8) / bi.height);
      if (1 <= which && which <= 7) {
	score = best_s;
	*inverse = best_inv;
	best_c = lower[7 - which];
      }
    }
  }

  /* Find best left line */
  if (1) {
    unsigned int best_s = (unsigned int) -1;
    bool best_inv = false;
    int best_i = 0;
    for (unsigned int i = 0; i < bi.width - 1; i++)
    {
      unsigned int s;
      s = col_sum[i] + total_i - col_sum_i[i];
      if (s < best_s) {
	best_s = s;
	best_i = i;
	best_inv = true;
      }
      s = col_sum_i[i] + total - col_sum[i];
      if (s < best_s) {
	best_s = s;
	best_i = i;
	best_inv = false;
      }
    }
    if (best_s < score) {
      static const char *left [7] = {"▏", "▎", "▍", "▌", "▋", "▊", "▉"};
      unsigned int which = lround ((double) ((best_i + 1) * 8) / bi.width);
      if (1 <= which && which <= 7) {
	score = best_s;
	*inverse = best_inv;
	best_c = left[which - 1];
      }
    }
  }

  /* Find best quadrant */
  if (1) {
    unsigned int q = 0;
    unsigned int qs = 0;
    for (unsigned int i = 0; i < 2; i++)
      for (unsigned int j = 0; j < 2; j++)
	if (quad[i][j] > quad_i[i][j]) {
	  q += 1 << (2 * i + j);
	  qs += quad_i[i][j];
	} else
	  qs += quad[i][j];
    if (qs < score) {
      const char *c = nullptr;
      bool inv = false;
      switch (q) {
	case 1:  c = "▟"; inv = true;  break;
	case 2:  c = "▙"; inv = true;  break;
	case 4:  c = "▖"; inv = false; break;
	case 8:  c = "▗"; inv = false; break;
	case 9:  c = "▚"; inv = false; break;
	case 6:  c = "▞"; inv = false; break;
	case 7:  c = "▜"; inv = true;  break;
	case 11: c = "▜"; inv = true;  break;
	case 13: c = "▙"; inv = true;  break;
	case 14: c = "▟"; inv = true;  break;
      }
      if (c) {
	score = qs;
	*inverse = inv;
	best_c = c;
      }
    }
  }

  return best_c;
}

void
ansi_print_image_rgb24 (const uint32_t *data,
			unsigned int width,
			unsigned int height,
			unsigned int stride)
{
  image_t image (width, height, data, stride);

  unsigned int rows = (height + CELL_H - 1) / CELL_H;
  unsigned int cols = (width +  CELL_W - 1) / CELL_W;
  image_t cell (CELL_W, CELL_H);
  biimage_t bi (CELL_W, CELL_H);
  unsigned int last_bg = -1, last_fg = -1;
  for (unsigned int row = 0; row < rows; row++) {
    for (unsigned int col = 0; col < cols; col++) {
      image.copy_sub_image (cell, col * CELL_W, row * CELL_H, CELL_W, CELL_H);
      bi.set (cell);
      if (bi.unicolor) {
	if (last_bg != bi.bg) {
	  printf ("%c[%dm", ESC_E, 40 + bi.bg);
	  last_bg = bi.bg;
	}
	printf (" ");
      } else {
	/* Figure out the closest character to the biimage */
	bool inverse = false;
	const char *c = block_best (bi, &inverse);
	if (inverse) {
	  if (last_bg != bi.fg || last_fg != bi.bg) {
	    printf ("%c[%d;%dm", ESC_E, 30 + bi.bg, 40 + bi.fg);
	    last_bg = bi.fg;
	    last_fg = bi.bg;
	  }
	} else {
	  if (last_bg != bi.bg || last_fg != bi.fg) {
	    printf ("%c[%d;%dm", ESC_E, 40 + bi.bg, 30 + bi.fg);
	    last_bg = bi.bg;
	    last_fg = bi.fg;
	  }
	}
	printf ("%s", c);
      }
    }
    printf ("%c[0m\n", ESC_E); /* Reset */
    last_bg = last_fg = -1;
  }
}
