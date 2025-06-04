// Copyright (C) 2004-2025 Artifex Software, Inc.
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

//#define DEBUG_DESKEWER

//int errprintf_nomem(const char *string, ...);
//#define printf errprintf_nomem

/* We up the mallocs by a small amount to allow for SSE
 * reading overruns. */
#define SSE_SLOP 16

/* Some notes on the theory.
 *
 * It is "well known" that a rotation (of between -90 and 90
 * degrees at least) can be performed as a process of 3 shears,
 * one on X, one on Y, one on X. For small angles (such as those
 * found when deskewing scanned pages), we can do it in just 2;
 * one on X, then one on Y.
 *
 * The standard rotation matrix is:
 *
 *  R = ( cos(t)   -sin(t) )
 *       ( sin(t)    cos(t) )
 *
 * We can use the following decomposition (thanks to Michael
 * Vrhel for pointing this out):
 *
 *     = ( 1        0        ) ( cos(t)  -sin(t) )
 *       ( tan(t)   1/cos(t) ) ( 0       1       )
 *
 * (noting that tan(t) = sin(t)/cos(t), and sin(t).sin(t)+cos(t)cos(t) = 1)
 *
 *     = ( 1 0        ) ( 1       0 ) ( 1  -tan(t) ) ( cos(t) 0 )
 *       ( 0 1/cos(t) ) ( sin(t)  1 ) ( 0  1       ) ( 0      1 )
 *
 *     = scale(Y)   .   sheer(Y)   .   sheer(X)   .   scale(X)
 *
 * The process we use to do sub-pixel capable shears allows us to
 * incorporate 1 dimensional scales for free. If the expected user of
 * this API is a scanner driver correcting for small errors in document
 * alignment, let's consider that they may also want to expand/reduce
 * at the same time.
 *
 * I'm not sure whether pre or post scales will be most useful, so
 * let's do the maths allowing for both, as it'll end up as no
 * more work.
 *
 * Our transformation pipeline can then be naively expressed as:
 *
 * (u') = (post_x 0     ) (1 0) (1 0) (1 c) (r 0) (pre_x 0    ) (u)
 * (v')   (0      post_y) (0 R) (b 1) (0 1) (0 1) (0     pre_y) (v)
 *
 * where b = sin(t), c = -tan(t), R = 1/cos(t), r = cos(t).
 *
 * Rearranging gives us that:
 *
 * (u') = (post_x 0       ) (1 0) (1 c) (r.pre_x 0    ) (u)
 * (v')   (0      post_y.R) (b 1) (0 1) (0       pre_y) (v)
 *
 * So, let X = post_x, Y = post_y.R, x = r.pre_x, y = pre_y and
 * we have:
 *
 * (u') = (X 0) (1 0) (1 c) (x 0) (u)
 * (v')   (0 Y) (b 1) (0 1) (0 y) (v)
 *
 * We need each step in the operation to be of the form:
 *
 *  (C D) for X and (1 0) for Y
 *  (0 1)           (E F)
 *
 * for some constants C, D, E and F to work with our 1-d scale/shear
 * mechanism. So, rearrange the pipeline:
 *
 *  P = (X 0) (1 0) (1 c) (x 0)
 *      (0 Y) (b 1) (0 1) (0 y)
 *
 *    = (X  0) (x cy)
 *      (bY Y) (0 y )
 *
 *    = (1    0) (X 0) (1 0) (x cy)
 *      (bY/X Y) (0 1) (0 y) (0 1 )
 *
 *    = (1    0) (1 0) (X 0) (x cy)
 *      (bY/X Y) (0 y) (0 1) (0 1 )
 *
 *    = (1    0 ) (xX cyX) = (xX   cyX      ) = (xX  cyX     )
 *      (bY/X yY) (0  1  )   (bxY  bcyY + yY)   (bxY yY(1+bc))
 *
 * The first scale/shear involves us taking an input line of data,
 * and scaling it into temporary storage. Every line of data supplied
 * has the same scale done to it, but at a different X offset. We
 * will therefore have to generate sets of weights for several different
 * subpixel positions, and pick the appropriate one. 4 or 8 should
 * be enough?
 *
 * For the second scale/shear, we don't run down individual scan-columns.
 * Instead, we use a block of lines (the output from the first phase) and
 * traverse through it diagonally, copying data out - effectively
 * producing 1 pixel output for each of the 'scan columns'.
 *
 * This gives us a block of scanlines that we can apply the third
 * shear to.
 *
 * Let us now consider the output position of each of the corners of
 * our src_w * src_h input rectangle under this transformation. For
 * simplicity of notation, let's call these just 'w' and 'h'.
 *
 * We know that by the properties of 2x2 matrices, (0, 0) maps to
 * (0, 0) and (w, h) maps the the sum of where (w,0) (0,h) map to,
 * so we only need calculate the image of 2 corners.
 *
 *    (1    0 ) (xX cyX) (w 0)
 *    (bY/X Yy) (0  1  ) (0 h)
 *
 *  = (1    0 ) (wxX chyX)
 *    (bY/X Yy) (0   h   )
 *
 *  = (wxX      chyX       )
 *    (bwxY     cbhYy + hYy)
 *
 *  = (C E)
 *    (D F)
 *
 * where C = wxX
 *       D = bwxY
 *       E = chyX
 *       F = cbhYy + hYy = hYy (1 + cb)
 *
 * Some ASCII art to illustrate the images after the different
 * stages:
 *
 * For angles 0..90:
 *
 *   *--x => *----x   =>    __.x
 *   |  |     \    \      *'   \
 *   o--+      o----+      \  __.+
 *                          o'
 *
 * For angles 0 to -90:
 *
 *   *--x =>   *----x =>   *.__
 *   |  |     /    /      /    'x
 *   o--+    o----+      o.__  /
 *                           '+
 *
 * How wide does temporary buffer 1 (where the results of the first
 * scale/shear are stored) need to be?
 *
 * From the above diagrams we can see that for angles 0..90 it needs
 * to be wide enough to stretch horizontally from * to +. i.e. wxX + chyX
 *
 * Similarly for angles 0..-90, it needs to be wide enough to stretch
 * horizontally from o to x. i.e. wxX - chyX
 *
 * Given the properties of tan, we can see that it's wxX + |c|hyX in
 * all cases.
 *
 *
 * How tall is the image after the first scale/shear?
 *
 * Well, the height doesn't change, so src_h.
 *
 *
 * How tall does temporary buffer 1 (where the results of the first
 * scale/shear are stored) need to be?
 *
 * Consider the second stage. One of the scanlines produced will be
 * made by reading in a line of pixels from (0, 0) to (wxX+|c|hyX, i)
 * and outputting a horizontal line of pixels in the result. What is
 * the value i?
 *
 * We know:
 *
 *  (1    0 ) (wxX+|c|hyX) = (wx+|c|hy)
 *  (bY/X Yy) (i         )   (0       )
 *
 *  So: bY/X*(wxX+|c|hyX)) + iYy = 0
 *      bY  *(wx +|c|hy )) + iYy = 0
 *
 *      i = -b*(wx+|c|hy)/y     (for non-zero Y and y)
 *        = -b*(tmp1_width)/Xy  (for non-zero X, Y and y)
 *
 * (Sanity check: when t=0, b = 0, no lines needed. As the x
 * scale increases, we need more lines. Seems sane.)
 *
 *
 * How wide is the image after the second scale/shear?
 *
 * Well, the shear on Y doesn't change the width, so it's still
 * wxX + |c|hyX.
 *
 *
 * How tall is the entire image after second scale/shear?
 *
 * For angles 0 to 90, it's the difference in Y between o and x.
 * i.e. cbhYy + hYy - bwxY
 *
 * For angles 0 to -90, it's the difference in Y between + and *.
 * i.e. cbhYy + hYy + bwxY
 *
 * So cbhYy + hYy + |b|wxY in all cases.
 *  = Y*((1+cb)*hy + |b|wx)
 *
 *
 * What is the size of the final image?
 *
 *  W = wxX + |c|hyX
 *  H = cbhYy + hYy + bwxY
 *
 */


/*
Border handling:

We offer 3 styles of border handling, options 0,1,2.

0: Increase the size of the output image from that of the input image. No pixels are dropped, and new border pixels are generated with the given backdrop color.
This is the default, and the safest way to deskew as no pixels are lost.

1: Keep the size of the page content constant (modulo pre and post scales). Some pixels are dropped, and new border pixels are generated with the given backdrop color.

2: Decrease the size of the output image from that of the input image. Pixels are dropped. No new border pixels should be generated. If the centre of the input is
correct, then this should exactly return the correct unskewed original page.

*/

#define WEIGHT_SHIFT 12
#define WEIGHT_SCALE (1<<WEIGHT_SHIFT)
#define WEIGHT_ROUND (1<<(WEIGHT_SHIFT-1))

/* Auxiliary structures. */
typedef struct
{
	uint32_t index;		/* index of first element in list of */
				/* contributors */
	uint16_t n;		/* number of contributors */
				/* (not multiplied by stride) */
	uint8_t  slow;		/* Flag */
	int32_t  first_pixel;	/* offset of first value in source data */
	int32_t  last_pixel;	/* last pixel number */
} index_t;

#if ARCH_HAS_NEON
typedef int16_t weight_t;
#else
typedef int32_t weight_t;
#endif

typedef void (zoom_y_fn)(uint8_t        *            dst,
			const uint8_t  * __restrict tmp,
			const index_t  * __restrict index,
			const weight_t * __restrict weights,
			uint32_t                    width,
			uint32_t                    channels,
			uint32_t                    mod,
			int32_t                     y);
typedef void (zoom_x_fn)(uint8_t        * __restrict tmp,
			const uint8_t  * __restrict src,
			const index_t  * __restrict index,
			const weight_t * __restrict weights,
			uint32_t                    dst_w,
			uint32_t                    src_w,
			uint32_t                    channels,
			const uint8_t  * __restrict bg);

#define CLAMP(v, mn, mx)\
	(v < mn ? mn : v > mx ? mx : v)

/* Include the cores */
#include "deskew_c.h"

#if ARCH_HAS_SSE
#include "deskew_sse.h"
#endif
#if ARCH_HAS_NEON
#include "deskew_neon.h"
#endif

enum {
	SUB_PIX_X = 4,
	SUB_PIX_Y = 4,
};

typedef struct
{
	uint32_t      channels;
	uint32_t      tmp_w;
	uint32_t      tmp_h;
	uint32_t      src_w;
	uint32_t      src_h;
	uint32_t      dst_w;
	uint32_t      dst_h;
	uint32_t      dst_w_borderless;
	uint32_t      dst_h_borderless;
	uint32_t      skip_l;
	uint32_t      skip_t;
	index_t      *index_x[SUB_PIX_X];
	weight_t     *weights_x[SUB_PIX_X];
	index_t      *index_y[SUB_PIX_Y];
	weight_t     *weights_y[SUB_PIX_Y];
	uint32_t      max_y_weights[SUB_PIX_Y];
	uint8_t       bg[FZ_MAX_COLORS];

	double        pre_x_scale;
	double        pre_y_scale;
	double        post_x_scale;
	double        post_y_scale;
	double        beta;
	double        gamma;
	double        chyX;
	double        extent1;
	double        w1;
	double        h2;
	double        diagonal_h;
	double        t_degrees;

	uint32_t      pre_fill_y;
	double        start_y;

	zoom_x_fn    *zoom_x;
	zoom_y_fn    *zoom_y;
} fz_deskewer;

static void
fill_bg(uint8_t *dst, const uint8_t *fill, uint32_t chan, uint32_t len)
{
	while (len--)
	{
		memcpy(dst, fill, chan);
		dst += chan;
	}
}

#define B (1.0f / 3.0f)
#define C (1.0f / 3.0f)
static double
Mitchell_filter(double t)
{
	double t2 = t * t;

	if (t < 0)
		t = -t;

	if (t < 1)
		return ((12 - 9 * B - 6 * C) * (t * t2) +
			(-18 + 12 * B + 6 * C) * t2 +
			(6 - 2 * B)) / 6;
	else if (t < 2)
		return ((-1 * B - 6 * C) * (t * t2) +
			(6 * B + 30 * C) * t2 +
			(-12 * B - 48 * C) * t +
			(8 * B + 24 * C)) / 6;
	else
		return 0;
}

#define FILTER_WIDTH 2

static void
make_x_weights(fz_context *ctx,
		index_t **indexp,
		weight_t **weightsp,
		uint32_t src_w,
		uint32_t dst_w,
		double factor,
		uint32_t offset_f,
		uint32_t offset_n,
		int sse_slow)
{
	double squeeze;
	index_t  *index;
	weight_t *weights;
	uint32_t i;
	double offset = ((double)offset_f)/offset_n;
	uint32_t idx;
	uint32_t max_weights;

	if (factor <= 1)
	{
		squeeze = 1;
		max_weights = 1 + FILTER_WIDTH * 2;
	}
	else
	{
		squeeze = factor;
		max_weights = (uint32_t)ceil(1 + squeeze * FILTER_WIDTH * 2);
		if (max_weights > 10)
		{
			max_weights = 10;
			squeeze = ((double)max_weights) / (FILTER_WIDTH * 2);
		}
	}

	max_weights = (max_weights + 3) & ~3;

	weights = (weight_t *)fz_malloc_aligned(ctx, sizeof(*weights) * max_weights * dst_w + SSE_SLOP, sizeof(weight_t) * 4);
	memset(weights, 0, sizeof(*weights) * max_weights * dst_w);
	fz_try(ctx)
		index = (index_t *)fz_malloc(ctx, sizeof(*index) * dst_w + SSE_SLOP);
	fz_catch(ctx)
	{
		fz_free_aligned(ctx, weights);
		fz_rethrow(ctx);
	}
	*indexp   = index;
	*weightsp = weights;

	idx = 0;
	for (i = 0; i < dst_w; i++)
	{
		/* i is in 0..w (i.e. dst space).
		 * centre, left, right are in 0..src_w (i.e. src_space)
		 */
		double centre = (i+0.5f)*factor - offset;
		int32_t left = (int32_t)ceil(centre - squeeze*FILTER_WIDTH);
		int32_t right = (int32_t)floor(centre + squeeze*FILTER_WIDTH);
		int32_t j, k;

		if ((centre - left) >= squeeze * FILTER_WIDTH)
			left++;
		if ((right - centre) >= squeeze * FILTER_WIDTH)
			right--;

		/* When we're calculating the second set of X weights, the subpixel adjustment can cause us to
		 * read too far to the right. Adjust for this hackily here. */
		if (left > (int32_t)src_w) {
			right -= left - src_w;
			centre -= left - src_w;
			left = src_w;
		}

		assert(right-left+1 <= (int)max_weights && right >= 0 && left <= (int32_t)src_w);
		index->index = idx;
		j = left;
		if (j < 0)
		{
			left = -1;
			weights[idx] = 0;
			for (; j < 0; j++)
			{
				double f = (centre - j) / squeeze;
				weights[idx] += (int32_t)(Mitchell_filter(f) * WEIGHT_SCALE / squeeze);
			}
			idx++;
		}
		k = right;
		if (k > (int32_t)src_w)
			k = (int32_t)src_w;
		for (; j <= k; j++)
		{
			double f = (centre - j) / squeeze;
			weights[idx++] = (int32_t)(Mitchell_filter(f) * WEIGHT_SCALE / squeeze);
		}
		for (; j < right; j++)
		{
			double f = (centre - j) / squeeze;
			weights[idx-1] += (int32_t)(Mitchell_filter(f) * WEIGHT_SCALE / squeeze);
		}
		index->first_pixel = left;
		index->last_pixel  = k;
		index->n           = k-left+1;
		index->slow        = left < 0 || k >= (int32_t)src_w;
		if (left + sse_slow > (int)src_w)
			index->slow = 1;
		idx = (idx + 3) & ~3;
		index++;
	}
}

/* The calculations here are different.
 * We move from offset...offset+h1 in w steps.
 * At each point, we calculate the weights vertically
 * with a scale factor of dst_h/src_h.
 */
static void
make_y_weights(fz_context *ctx,
		index_t **indexp,
		weight_t **weightsp,
		uint32_t *max_weightsp,
		uint32_t dst_w,
		double factor,
		double factor2,
		uint32_t offset_f,
		uint32_t offset_n,
		uint32_t h)
{
	double squeeze;
	index_t  *index;
	weight_t *weights;
	uint32_t i;
	double   offset = ((double)offset_f)/offset_n;
	uint32_t idx;
	uint32_t max_weights;

	if (factor >= 1)
	{
		squeeze = 1;
		max_weights = 1 + FILTER_WIDTH * 2;
	}
	else
	{
		squeeze = 1/factor;
		max_weights = (uint32_t)ceil(squeeze * FILTER_WIDTH * 2);
		if (max_weights > 10)
		{
			max_weights = 10;
			squeeze = ((double)max_weights) / (FILTER_WIDTH * 2);
		}
	}

	max_weights = (max_weights + 3) & ~3;

	/* Ensure that we never try to access before 0 */
	offset += (double)FILTER_WIDTH/squeeze;

	weights = (weight_t *)fz_malloc_aligned(ctx, sizeof(*weights) * max_weights * dst_w, sizeof(weight_t) * 4);
	fz_try(ctx)
		index = (index_t *)fz_malloc(ctx, sizeof(*index) * dst_w);
	fz_catch(ctx)
	{
		fz_free_aligned(ctx, weights);
		fz_rethrow(ctx);
	}
	*indexp       = index;
	*weightsp     = weights;
	*max_weightsp = max_weights;

	if (factor2 < 0)
		offset -= (dst_w-1) * factor2;

	idx = 0;
	for (i = 0; i < dst_w; i++)
	{
		/* i is in 0..dst_w (i.e. dst space).
		 * centre, left, right are in 0..src_h (i.e. src_space)
		 */
		double centre = (i+0.5f)*factor2 + offset;
		int32_t left = (int32_t)ceil(centre - squeeze*FILTER_WIDTH);
		int32_t right = (int32_t)floor(centre + squeeze*FILTER_WIDTH);
		int32_t j;

		if ((centre - left) >= squeeze * FILTER_WIDTH)
			left++;
		if ((right - centre) >= squeeze * FILTER_WIDTH)
			right--;

		assert(right-left+1 <= (int)max_weights);
		index->index       = idx;
		for (j = left; j <= right; j++)
		{
			double f = (centre - j) / squeeze;
			weights[idx++] = (int32_t)(Mitchell_filter(f) * WEIGHT_SCALE / squeeze);
		}
		index->last_pixel  = right;
		index->n           = right-left+1;
		if (left < 0)
			left += h;
		index->first_pixel = left;
		index->slow        = 0;
		index++;
		idx = (idx + 3) & ~3;
	}
}

#ifdef DEBUG_DESKEWER
static void
dump_weights(const index_t *index,
		const weight_t *weights,
		uint32_t w,
		const char *str)
{
	uint32_t i;

	printf("%s weights:\n", str);
	for (i = 0; i < w; i++)
	{
		uint32_t j;
		int32_t  sum = 0;
		uint32_t n = index[i].n;
		uint32_t idx = index[i].index;
		printf(" %d: %d->%d:", i, index[i].first_pixel, index[i].last_pixel);
		for (j = 0; j < n; j++)
		{
			sum += weights[idx];
			printf(" %x", weights[idx++]);
		}
		printf(" (%x)\n", sum);
	}
}
#endif

static void
rejig_for_zoom_y1(fz_context *ctx, fz_deskewer *deskewer)
{
	int i, k;
	uint32_t j, z;

	for (i = 0; i < SUB_PIX_Y; i++)
	{
		uint32_t  num_w = deskewer->dst_w;
		weight_t *new_weights = fz_malloc_aligned(ctx, num_w *  deskewer->max_y_weights[i] *
							sizeof(weight_t) * 4, sizeof(weight_t) * 4);
		index_t  *index;
		weight_t *weights;

		index = deskewer->index_y[i];
		weights = deskewer->weights_y[i];
		for (j = 0; j < num_w; j++)
		{
			weight_t *neww = new_weights + (index->index * 4);
			uint32_t n = index[0].n;
			for (z = 0; z < n; z++)
				neww[4 * z] = weights[index->index + z];
			for (k = 1; k < 4 && j + k < num_w; k++)
			{
				if (index[0].n != index[k].n)
					break;
				if (index[0].first_pixel != index[k].first_pixel)
					break;
				for (z = 0; z < n; z++)
					neww[k + 4 * z] = weights[index[k].index + z];
			}
			/* So, we can do j to j+k-1 (inclusive) all in one hit. */
			index->slow = k;
			index++;
		}
		fz_free_aligned(ctx, deskewer->weights_y[i]);
		deskewer->weights_y[i] = new_weights;
	}
}

static void
fz_drop_deskewer(fz_context *ctx, fz_deskewer *deskewer)
{
	int i;

	if (!deskewer)
		return;

	for (i = 0; i < SUB_PIX_X; i++)
	{
		fz_free(ctx, deskewer->index_x[i]);
		fz_free_aligned(ctx, deskewer->weights_x[i]);
	}
	for (i = 0; i < SUB_PIX_Y; i++)
	{
		fz_free(ctx, deskewer->index_y[i]);
		fz_free_aligned(ctx, deskewer->weights_y[i]);
	}

	fz_free(ctx, deskewer);
}

static fz_deskewer *
fz_new_deskewer(fz_context *ctx,
		unsigned int src_w, unsigned int src_h,
		unsigned int *dst_w, unsigned int *dst_h,
		double t_degrees, int border,
		double pre_x_scale, double pre_y_scale,
		double post_x_scale, double post_y_scale,
		unsigned char *bg,
		unsigned int channels)
{
	fz_deskewer *deskewer;
	double w1, h2, extent1, beta, gamma, chyX, one_plus_cb;
	double diagonal_h, h2_div_Y;
	int i;

#define SIMD_SWITCH_CONDITION 1

#if ARCH_HAS_SSE
#define SIMD_SWITCH(A,B,C) (SIMD_SWITCH_CONDITION ? (A) : (C))
#elif ARCH_HAS_NEON
#define SIMD_SWITCH(A,B,C) (SIMD_SWITCH_CONDITION ? (B) : (C))
#else
#define SIMD_SWITCH(A,B,C) (C)
#endif

	deskewer = fz_malloc_struct(ctx, fz_deskewer);

	deskewer->channels    = channels;

	/* Rotation coefficients */
	gamma = -tan(t_degrees * M_PI / 180);
	beta  = sin(t_degrees * M_PI / 180);

	/* After the first shear/scale, the image will be w1 x src_h in size. */
	chyX = gamma * src_h * pre_y_scale * post_x_scale;
	extent1 = pre_x_scale * post_x_scale * src_w;
	w1 = extent1 + fabs(chyX);
	diagonal_h = fabs(beta) * w1 / pre_y_scale / post_x_scale;

	/* After the second shear/scale, the image will be w1 x h2 in size. */

	/* Calculate the size of the destination buffer */
	one_plus_cb = 1 + gamma * beta;
	h2_div_Y = src_h * pre_y_scale * one_plus_cb + fabs(src_w * pre_x_scale * beta);
	h2 = post_y_scale * h2_div_Y;

	deskewer->tmp_h      = (uint32_t)ceil(diagonal_h) + FILTER_WIDTH*2 + 1;
	if (deskewer->tmp_h == 0) /* Allow for the zero degree case */
		deskewer->tmp_h = 1;
	deskewer->tmp_w      = (uint32_t)ceil(extent1)+1; /* +1 allows for subpixel positioning */
	deskewer->src_w       = src_w;
	deskewer->src_h       = src_h;
	deskewer->dst_w       = (uint32_t)ceil(w1);
	deskewer->dst_h       = (uint32_t)ceil(h2);
	deskewer->pre_x_scale = pre_x_scale;
	deskewer->pre_y_scale = pre_y_scale;
	deskewer->post_x_scale= post_x_scale;
	deskewer->post_y_scale= post_y_scale;
	deskewer->beta        = beta;
	deskewer->gamma       = gamma;
	deskewer->chyX        = chyX;
	deskewer->extent1     = extent1;
	deskewer->w1          = w1;
	deskewer->diagonal_h  = diagonal_h;
	deskewer->t_degrees   = t_degrees;
	deskewer->h2          = h2;
	deskewer->skip_l      = 0;
	deskewer->skip_t      = 0;
	switch(border)
	{
	case 2:
	{
		int w = (int)(src_w * pre_x_scale * post_x_scale + 0.5);
		int h = (int)(src_h * pre_y_scale * post_y_scale + 0.5);
		deskewer->skip_l = (deskewer->dst_w - w);
		deskewer->skip_t = (deskewer->dst_h - h);
		*dst_w = w - deskewer->skip_l;
		*dst_h = h - deskewer->skip_t;
		break;
	}
	case 1:
	{
		int expected_w = (int)(src_w * pre_x_scale * post_x_scale + 0.5);
		int expected_h = (int)(src_h * pre_y_scale * post_y_scale + 0.5);
		deskewer->skip_l = (deskewer->dst_w - expected_w + 1) >> 1;
		deskewer->skip_t = (deskewer->dst_h - expected_h + 1) >> 1;
		*dst_w = expected_w;
		*dst_h = expected_h;
		break;
	}
	default:
	case 0:
		*dst_w = deskewer->dst_w;
		*dst_h = deskewer->dst_h;
		break;
	}
	deskewer->dst_w_borderless = *dst_w;
	deskewer->dst_h_borderless = *dst_h;

	fz_try(ctx)
	{
		memcpy(deskewer->bg, bg, channels);

		/*
		 * Once we have scaled/sheared lines into the tmp1, we have
		 * data such as:
		 *
		 *    -ve angles     +ve angles
		 *      +------+  or +-----+
		 *     /      /       \     \
		 *    /      /         \     \
		 *   /      /           \     \
		 *  /      /             \     \
		 * +-----+                +-----+
		 *
		 * We then need to copy data out into the output. This is done by
		 * reading a series of parallel diagonal lines out. The first
		 * one is as shown here:
		 *
		 *            _.   or    ._            ----
		 *        _.-'             '-._              } pre fill region
		 *     _.+-----+         +-----+._     ----
		 *  .-' /     /           \     \ '-.  ____  } lines of data required before we can start
		 *     /     /             \     \
		 *    /     /               \     \
		 *   /     /                 \     \
		 *  +-----+                   +-----+
		 *
		 * We can see that we need to fill the temporary buffer with some
		 * empty lines to start with.
		 *
		 * The total Y extent of the diagonal line has been calculated as
		 * diagonal_h already. The length of the horizontal edge of the data
		 * region is extent1, and the remainder of the width is |c|hyX.
		 *
		 * Thus we need diagonal_h * extent1 / (extent1 + |c|hyX) in the pre
		 * fill region. We can generate our first line out once we have
		 * h1 lines in.
		 */

		deskewer->pre_fill_y = ((uint32_t)ceil(diagonal_h*extent1/w1) + FILTER_WIDTH*2);
		deskewer->start_y = diagonal_h - deskewer->pre_fill_y;

		/* Each line we scale into the destination starts at a different
		 * X position. We hold that as x2.
		 *
		 * We calculated earlier where the corners of our source rectangle
		 * were mapped to to give us the smallest dst_w x dst_h.
		 *   dst_w = X * (wx + hy|c|)
		 *   dst_h = Y * (hy(1+cb) + |b|wx)
		 */

		for (i = 0; i < SUB_PIX_X; i++)
		{
			make_x_weights(ctx,
					&deskewer->index_x[i],
					&deskewer->weights_x[i],
					src_w,
					deskewer->tmp_w,
					(double)src_w / extent1,
					i, SUB_PIX_X,
					(16+channels-1)/channels);
#ifdef DEBUG_DESKEWER
			{
				char text[16];
				sprintf(text, "X[%d]", i);
				dump_weights(deskewer->index_x[i],
						deskewer->weights_x[i],
						deskewer->tmp_w,
						text);
			}
#endif
		}

		for (i = 0; i < SUB_PIX_Y; i++)
		{
			make_y_weights(ctx,
				&deskewer->index_y[i],
				&deskewer->weights_y[i],
				&deskewer->max_y_weights[i],
				deskewer->dst_w,
				post_y_scale * pre_y_scale,
				(t_degrees >= 0 ? diagonal_h : -diagonal_h) / w1,
				i, SUB_PIX_Y,
				deskewer->tmp_h);
#ifdef DEBUG_DESKEWER
			{
				char text[16];
				sprintf(text, "Y[%d]", i);
				dump_weights(deskewer->index_y[i],
						deskewer->weights_y[i],
						deskewer->dst_w,
						text);
			}
#endif
		}

		switch (channels)
		{
		case 1:
			deskewer->zoom_x = SIMD_SWITCH(zoom_x1_sse, zoom_x1_neon, zoom_x1);
			deskewer->zoom_y = SIMD_SWITCH(zoom_y1_sse, zoom_y1_neon, zoom_y1);
			break;
		case 3:
			deskewer->zoom_x = SIMD_SWITCH(zoom_x3_sse, zoom_x3_neon, zoom_x3);
			deskewer->zoom_y = SIMD_SWITCH(zoom_y3_sse, zoom_y3_neon, zoom_y3);
			break;
		case 4:
			deskewer->zoom_x = SIMD_SWITCH(zoom_x4_sse, zoom_x4_neon, zoom_x4);
			deskewer->zoom_y = SIMD_SWITCH(zoom_y4_sse, zoom_y4_neon, zoom_y4);
			break;
		default:
			deskewer->zoom_x = zoom_x;
			deskewer->zoom_y = zoom_y;
			break;
		}

		if (channels == 1)
			rejig_for_zoom_y1(ctx, deskewer);
	}
	fz_catch(ctx)
	{
		fz_drop_deskewer(ctx, deskewer);
		fz_rethrow(ctx);
	}

	return deskewer;
}

/*
  Our overall forward transform is:

  (u') = (1    0 ) (xX cyX) (u)
  (v')   (bY/X yY) (0  1  ) (v)

  (right hand one is the X shear, left hand one is the Y shear).

  (u') = (xX   cyX     ) (u)
  (v')   (bxY  yY(1+bc)) (v)

  So, inverse...

  (u) = 1/det . (yY(1+bc)  -cyX) (u')
  (v)           (-bxY       xX ) (v')

  where det = (xyXY.(1+bc) - cyX.bxY) = xyXY(1+bc - bc) = xyXY

  So inverse...

   (u) = ((1+bc)/xX   -c/xY) (u')
   (v)   (-b/yX        1/yY) (v')

  Sanity check:

   ((1+bc)/xX  -c/xY) (xX   cyX     ) = ((1+bc) - bc     cy(1+bc)/x - cy(1+bc)/x) = (1 0)
   (-b/yX       1/yY) (bxY  yY(1+bc))   (-bx/y + bx/y    -bc + (1+bc)           )   (0 1)
*/

typedef struct fz_deskewer_bander_s {
	fz_deskewer  *deskewer;
	double        diag_y;
	double        diag_dy;
	uint32_t      in_y;
	uint32_t      out_y;
	uint32_t      tmp_width;
	uint8_t      *tmp;
	uint32_t      tmp_size;
	double        tmp_x;
	double        tmp_dx;
	unsigned int  dst_x0;
	unsigned int  dst_x1;
	unsigned int  dst_y0;
} fz_deskewer_bander;

static fz_deskewer_bander *
fz_new_deskewer_band(fz_context *ctx,
			fz_deskewer *deskewer,
			unsigned int src_y0,
			unsigned int dst_y0)
{
	fz_deskewer_bander *bander = fz_malloc_struct(ctx, fz_deskewer_bander);
	bander->deskewer = deskewer;

	bander->tmp_width = deskewer->dst_w_borderless;
	bander->tmp_size = bander->tmp_width * deskewer->channels * deskewer->tmp_h;
	bander->dst_x0 = deskewer->skip_l;
	bander->dst_x1 = deskewer->dst_w_borderless + deskewer->skip_l;
	bander->dst_y0 = dst_y0;

	fz_try(ctx)
		bander->tmp = (uint8_t *)fz_malloc(ctx, bander->tmp_size + SSE_SLOP);
	fz_catch(ctx)
	{
		fz_free(ctx, bander);
		fz_rethrow(ctx);
	}

	bander->dst_y0 += deskewer->skip_t;

	/* Each line we scale into tmp starts at a different x position.
	 * We hold that as x. Over src_h lines, we want to move from
	 * 0 to chy (or chy to 0). */
	bander->tmp_x = (deskewer->chyX >= 0 ? deskewer->chyX : 0);
	bander->tmp_dx = -deskewer->chyX/deskewer->src_h;
	/* Do a half step */
	bander->tmp_x += bander->tmp_dx/2;
	bander->tmp_x += src_y0 * bander->tmp_dx;

	bander->diag_y = FILTER_WIDTH-(double)deskewer->pre_fill_y;
	bander->diag_dy = (deskewer->src_h + deskewer->diagonal_h * (2 * deskewer->extent1 / deskewer->w1 - 1)) / deskewer->h2;
	/* Do a half step on y */
	bander->diag_y += bander->diag_dy/2;
	bander->diag_y += bander->dst_y0 * bander->diag_dy;

	/* diag_y = Where we start to read the diagonal line from */
	/* in_y = All lines < this have been written into tmp. */
	/* out_y = All lines smaller than this have been written out */

	/* The first source line we have is src_y0. After the X skew this will still be src_y0.
	 * The Y skew will mean that this line extends across a range of output scanlines.
	 * Find the range of scanlines that we touch. */
	bander->in_y = src_y0;
	/* We need to fill the lines up to and including tmp_y with the background color. */
	{
		int first_y = (bander->in_y - deskewer->pre_fill_y + deskewer->tmp_h) % deskewer->tmp_h;
		int count = deskewer->pre_fill_y;
		if (first_y + count > (int)deskewer->tmp_h)
		{
			int s = deskewer->tmp_h - first_y;
			fill_bg(bander->tmp + first_y * bander->tmp_width * deskewer->channels, deskewer->bg, deskewer->channels, bander->tmp_width * s);
			first_y = 0;
			count -= s;
		}
		fill_bg(bander->tmp + first_y * bander->tmp_width * deskewer->channels, deskewer->bg, deskewer->channels, bander->tmp_width * count);
	}

	bander->out_y = dst_y0;

	return bander;
}

static int
fz_deskewer_band_pull(fz_deskewer_bander *bander,
			unsigned char *dst)
{
	/* If we have enough data to make a new y2 line, do so. */
	double y = bander->diag_y + 1.0/(2 * SUB_PIX_Y);
	int iy = (int)floor(y);
	int which_y = (int)floor((y - iy) * SUB_PIX_Y);
	fz_deskewer *deskewer = bander->deskewer;

	/* Ensure we have enough input lines to generate an output one. */
	while (1)
	{
		/* Which source line do we need to have to generate the next destination line? */
		int need_y = deskewer->index_y[which_y][deskewer->t_degrees >= 0 ? bander->dst_x1 - 1 : bander->dst_x0].last_pixel + iy;

		/* Have we got enough lines? */
		if ((int)bander->in_y >= need_y)
			break; /* Yes, break out of the loop to process it. */

		/* No. Can we ask the caller for one? */
		if (bander->in_y < deskewer->src_h)
			/* We are still in the range where we can ask for one. */
			return 0;

		/* Fill in a line with background pixels. */
		fill_bg(&bander->tmp[bander->tmp_width * deskewer->channels * (bander->in_y % deskewer->tmp_h)],
			deskewer->bg, deskewer->channels, bander->tmp_width);
		bander->in_y++;
	}

	deskewer->zoom_y(dst,
			bander->tmp,
			&deskewer->index_y[which_y][bander->dst_x0],
			deskewer->weights_y[which_y],
			bander->tmp_width,
			deskewer->channels,
			bander->tmp_size,
			(iy + deskewer->tmp_h) % deskewer->tmp_h);
	bander->out_y++;
	bander->diag_y += bander->diag_dy;

	return 1;
}

static void
fz_deskewer_band_push(fz_deskewer_bander *bander,
			const unsigned char *src)
{
	fz_deskewer *deskewer = bander->deskewer;
	uint8_t *line = &bander->tmp[bander->tmp_width * deskewer->channels * (bander->in_y % deskewer->tmp_h)];
	double x = bander->tmp_x + 1.0 / (SUB_PIX_X * 2);
	int xi = (int)floor(x);
	int which = (int)floor((x - xi) * SUB_PIX_X);

	{
		int r = (int)bander->dst_x1;
		if (r > xi)
			r = xi;
		if ((int)bander->dst_x0 < r)
			fill_bg(line,
				deskewer->bg,
				deskewer->channels,
				r - (int)bander->dst_x0);
	}
	{
		int l = xi + deskewer->tmp_w - 1; /* Undo the +1 earlier */
		if (l < (int)bander->dst_x0)
			l = (int)bander->dst_x0;
		if (l < (int)bander->dst_x1)
			fill_bg(&line[(l - bander->dst_x0) * deskewer->channels],
				deskewer->bg,
				deskewer->channels,
				(int)bander->dst_x1 - l);
	}
	{
		int l = xi;
		int r = xi + deskewer->tmp_w;
		if (l < (int)bander->dst_x0)
			l = (int)bander->dst_x0;
		if (r > (int)bander->dst_x1)
			r = (int)bander->dst_x1;
		if (l < r)
		{
#if 0
			memcpy(&line[(l - (int)bander->dst_x0) * deskewer->channels],
				src,
				(r-l) * deskewer->channels);
#else
			deskewer->zoom_x(&line[(l - (int)bander->dst_x0) * deskewer->channels],
					src,
					&deskewer->index_x[which][l - xi],
					deskewer->weights_x[which],
					r-l,
					deskewer->src_w,
					deskewer->channels,
					deskewer->bg);
#endif
		}
	}
	bander->in_y++;
	bander->tmp_x += bander->tmp_dx;
}

static void
fz_drop_deskewer_band(fz_context *ctx, fz_deskewer_bander *bander)
{
	if (bander == NULL)
		return;

	fz_free(ctx, bander->tmp);
	fz_free(ctx, bander);
}

static void
fz_deskewer_band(fz_context *ctx,
		fz_deskewer *deskewer,
		const unsigned char *src,
		int src_stride,
		unsigned int src_y0,
		unsigned int src_y1,
		unsigned char *dst,
		int dst_stride,
		unsigned int dst_y0,
		unsigned int dst_y1)
{
	fz_deskewer_bander *bander;
	int row = 0;

	bander = fz_new_deskewer_band(ctx, deskewer, src_y0, dst_y0);

	while (dst_y0 < dst_y1)
	{
		if (fz_deskewer_band_pull(bander, dst + dst_stride * row) == 1)
		{
			row++;
			dst_y0++;
			continue;
		}
		fz_deskewer_band_push(bander, src + src_stride * row);
	}

	fz_drop_deskewer_band(ctx, bander);
}

fz_pixmap *fz_deskew_pixmap(fz_context *ctx,
			fz_pixmap *src,
			double degrees,
			int border)
{
	uint8_t bg[FZ_MAX_COLORS] = { 0 };
	uint8_t bg_rgb[FZ_MAX_COLORS] = { 255, 255, 255 };
	unsigned int dst_w, dst_h;
	fz_deskewer *deskewer = fz_new_deskewer(ctx, src->w, src->h, &dst_w, &dst_h, degrees, border, 1, 1, 1, 1, src->n - src->alpha > 3 ? bg : bg_rgb, src->n);
	fz_pixmap *dst = NULL;

	fz_var(dst);

	fz_try(ctx)
	{
		dst = fz_new_pixmap(ctx, src->colorspace, dst_w, dst_h, NULL, src->alpha);

		fz_deskewer_band(ctx, deskewer, src->samples, src->stride, 0, src->h, dst->samples, dst->stride, 0, dst->h);
	}
	fz_always(ctx)
	{
		fz_drop_deskewer(ctx, deskewer);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, dst);
		fz_rethrow(ctx);
	}

	return dst;

}
