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

#include <math.h>
#include <assert.h>
#include <limits.h>

#if ARCH_HAS_SSE
#include <emmintrin.h>
#include <smmintrin.h>
#endif

/* Uncomment the following to enable Debugging printfs. */
/* #define DEBUG_PRINT_WORKING */

enum {
	NUM_SKEW_COLS = 4,
	SKEW_COL_OFFSET = 96,
	SKEW_COL_WIDTH = 96,
	SKEW_MAX_DIFF = 16
};

typedef struct
{
	void *src;
	uint32_t src_w;
	uint32_t src_h;
	uint32_t y;
	uint32_t offsets[NUM_SKEW_COLS*2];
	uint16_t *tables[NUM_SKEW_COLS*2];
	uint32_t corr_height;
} fz_skew;

/* Finalise a rescaler instance. */
static void
fz_drop_skew(fz_context *ctx, fz_skew *skew)
{
	if (skew == NULL)
		return;

	fz_free(ctx, skew->tables[0]);
	fz_free(ctx, skew);
}

/* Initialise a skew instance. */
static fz_skew *
fz_new_skew(fz_context *ctx,
	unsigned int src_w,
	unsigned int src_h)
{
	fz_skew *skew = fz_malloc_struct(ctx, fz_skew);
	int i;

	skew->src_w = src_w;
	skew->src_h = src_h;
	skew->y = 0;
	skew->corr_height = src_h-100; /* FIXME */
	fz_try(ctx)
		skew->tables[0] = (uint16_t *)fz_malloc(ctx, skew->src_h * sizeof(uint16_t) * NUM_SKEW_COLS * 2);
	fz_catch(ctx)
	{
		fz_drop_skew(ctx, skew);
		fz_rethrow(ctx);
	}

	for (i = 1; i < NUM_SKEW_COLS * 2; i++)
		skew->tables[i] = skew->tables[0] + i*skew->src_h;

	for (i = 0; i < NUM_SKEW_COLS; i++)
	{
		int offset = skew->src_w * (i+1) / (NUM_SKEW_COLS+1) - SKEW_COL_WIDTH/2;
		skew->offsets[2*i  ] = offset - SKEW_COL_OFFSET;
		skew->offsets[2*i+1] = offset + SKEW_COL_OFFSET;
	}

	return skew;
}

static int sum_c(const uint8_t *data)
{
	int i;
	uint32_t sum = 0;
	for (i = SKEW_COL_WIDTH; i > 0; i--)
		sum += *data++;
	return sum;
}

#if ARCH_HAS_SSE
static inline int sum_sse(const uint8_t *data)
{
	__m128i mm0, mm1, mm2, mm3, mm4, mm5;
	__m128i mm_zero = _mm_set1_epi32(0);

	mm0 = _mm_loadu_si128((const __m128i *)(data   )); // mm0 = ppoonnmmllkkjjiihhggffeeddccbbaa
	mm1 = _mm_loadu_si128((const __m128i *)(data+16)); // mm1 = ppoonnmmllkkjjiihhggffeeddccbbaa
	mm2 = _mm_loadu_si128((const __m128i *)(data+32)); // mm2 = ppoonnmmllkkjjiihhggffeeddccbbaa
	mm3 = _mm_loadu_si128((const __m128i *)(data+48)); // mm3 = ppoonnmmllkkjjiihhggffeeddccbbaa
	mm4 = _mm_loadu_si128((const __m128i *)(data+64)); // mm4 = ppoonnmmllkkjjiihhggffeeddccbbaa
	mm5 = _mm_loadu_si128((const __m128i *)(data+80)); // mm5 = ppoonnmmllkkjjiihhggffeeddccbbaa

	mm0 = _mm_sad_epu8(mm0, mm_zero); // Max value in each half is 8*255
	mm1 = _mm_sad_epu8(mm1, mm_zero);
	mm2 = _mm_sad_epu8(mm2, mm_zero);
	mm3 = _mm_sad_epu8(mm3, mm_zero);
	mm4 = _mm_sad_epu8(mm4, mm_zero);
	mm5 = _mm_sad_epu8(mm5, mm_zero);

	mm0 = _mm_add_epi64(mm0, mm1); // Max value in each half is 2*8*255
	mm2 = _mm_add_epi64(mm2, mm3); // Max value in each half is 2*8*255
	mm4 = _mm_add_epi64(mm4, mm5); // Max value in each half is 2*8*255
	mm0 = _mm_add_epi64(mm0, mm2); // Max value in each half is 4*8*255
	mm0 = _mm_add_epi64(mm0, mm4); // Max value in each half is 6*8*255

	mm0 = _mm_shuffle_epi32(mm0, (2<<2)+0); // Max value in each half is 10*8*255
	mm0 = _mm_hadd_epi32(mm0, mm0); // Max value in bottom bits is 20*8*255 - still fits in an unsigned 16bit word.

	return _mm_extract_epi16(mm0, 0);
}
#endif

/* Process data: */
static void
fz_skew_process(fz_context *ctx, fz_skew *skew, const uint8_t *input)
{
	int i;
	int off = skew->y++;

#if ARCH_HAS_SSE
	for (i = 0; i < NUM_SKEW_COLS * 2; i++)
		skew->tables[i][off] = sum_sse(input + skew->offsets[i]);
#else
	for (i = 0; i < NUM_SKEW_COLS * 2; i++)
		skew->tables[i][off] = sum_c(input + skew->offsets[i]);
#endif

	/* Some debug code; if enabled this writes the summed value back
	 * in to give us a visual indication of where we are looking for
	 * correspondences. */
#if 0
	for (i = 0; i < NUM_SKEW_COLS * 2; i++) {
		int v = (skew->tables[i][off] + (SKEW_COL_WIDTH/2) ) / SKEW_COL_WIDTH;
		memset(input + skew->offsets[i], v, SKEW_COL_WIDTH);
	}
#endif
}

static double
do_detect_skew(fz_context *ctx, fz_skew *skew)
{
	int i, j, h, o, max_at;
	int64_t max_sum, corr_at, corr_sum, avg_sum;
	float ang;
	int64_t avg = SKEW_COL_WIDTH * 255/2;

	if (skew == NULL)
		return 0;

	h = skew->corr_height - 2 * SKEW_MAX_DIFF;

	corr_at = 0;
	corr_sum = 0;
	for (i = 0; i < NUM_SKEW_COLS; i++)
	{
		max_at = 9999;
		max_sum = 0;
		avg_sum = 0;
		for (o = -SKEW_MAX_DIFF; o <= SKEW_MAX_DIFF; o++)
		{
			uint16_t *t0 = skew->tables[2*i] + SKEW_MAX_DIFF;
			uint16_t *t1 = skew->tables[2*i+1] + SKEW_MAX_DIFF + o;
			int64_t sum = 0;
			for (j = 0; j < h; j++)
				sum += ((int64_t)t0[j]-avg) * ((int64_t)t1[j]-avg);
			if (max_sum < sum)
				max_sum = sum, max_at = o;
			avg_sum += sum;
#ifdef DEBUG_PRINT_WORKING
			printf("col %d, offset %d -> %llx\n", i, o, sum);
#endif
		}
		avg_sum /= (SKEW_MAX_DIFF+1)*2;
#ifdef DEBUG_PRINT_WORKING
		ang = (180.0 / 3.1415942) * atan(max_at / (double)(SKEW_COL_OFFSET * 2));
		printf("col %d max: offset %d -> %llx ang=%g\n", i, max_at, max_sum, ang);
#endif
		/* Subtract the average from the maximum; we judge the significance of a
		 * match by how far it exceeds the average. max_sum becomes 'significance'. */
		max_sum -= avg_sum;
#ifdef DEBUG_PRINT_WORKING
		printf("Significance: %llx\n", max_sum - avg_sum);
#endif
		corr_at += max_at * max_sum;
		corr_sum += max_sum;
	}
	ang = (180.0 / 3.1415942) * atan(corr_at / (double)(corr_sum * SKEW_COL_OFFSET * 2));

	if (ang < -45 || ang > 45)
		return 0;

	return ang;
}

double fz_detect_skew(fz_context *ctx, fz_pixmap *pix)
{
	fz_skew *skew = fz_new_skew(ctx, pix->w, pix->h);
	int y;
	uint8_t *ptr;
	ptrdiff_t stride;
	double angle;
	fz_pixmap *pix2 = NULL;

	fz_var(pix2);

	fz_try(ctx)
	{
		if (pix->n != 1)
		{
			pix2 = fz_convert_pixmap(ctx, pix, fz_device_gray(ctx), NULL, NULL, fz_default_color_params, 0);
			ptr = pix2->samples;
			stride = pix2->stride;
		}
		else
		{
			ptr = pix->samples;
			stride = pix->stride;
		}

		for (y = pix->h; y > 0; y--)
		{
			fz_skew_process(ctx, skew, ptr);
			ptr += stride;
		}

		angle = do_detect_skew(ctx, skew);
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, pix2);
		fz_drop_skew(ctx, skew);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return angle;
}
