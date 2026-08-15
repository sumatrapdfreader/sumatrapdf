// Copyright (C) 2004-2024 Artifex Software, Inc.
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

/* This file is included from deskew.c if NEON cores are allowed. */

#include "arm_neon.h"

static void
zoom_x1_neon(uint8_t * FZ_RESTRICT tmp,
	const uint8_t * FZ_RESTRICT src,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t dst_w,
	uint32_t src_w,
	uint32_t channels,
	const uint8_t * FZ_RESTRICT bg)
{
	int32x4_t round = vdupq_n_s32(WEIGHT_ROUND);

	if (0)
slow:
	{
		/* Do any where we might index off the edge of the source */
		int pix_num = index->first_pixel;
		const uint8_t  *s = &src[pix_num];
		const weight_t *w = &weights[index->index];
		uint32_t j = index->n;
		int32_t pixel0 = WEIGHT_ROUND;
		if (pix_num < 0)
		{
			int32_t wt = *w++;
			assert(pix_num == -1);
			pixel0 += bg[0] * wt;
			s++;
			j--;
			pix_num = 0;
		}
		pix_num = (int)src_w - pix_num;
		if (pix_num > (int)j)
			pix_num = j;
		j -= pix_num;
		while (pix_num > 0)
		{
			pixel0 += *s++ * *w++;
			pix_num--;
		}
		if (j > 0)
		{
			assert(j == 1);
			pixel0 += bg[0] * *w;
		}
		pixel0 >>= WEIGHT_SHIFT;
		*tmp++ = CLAMP(pixel0, 0, 255);
		index++;
		dst_w--;
	}

	while (dst_w > 0)
	{
		const uint8_t  *s;
		uint32_t j;
		const weight_t *w;

		/* Jump out of band to do the (rare) slow (edge) pixels */
		if (index->slow)
			goto slow;

		s = &src[index->first_pixel];
		j = index->n;
		w = &weights[index->index];
		if (j <= 4)
		{
			int32x4_t q_pair_sum;
			int16x4_t wts = vld1_s16(w);
			uint8x8_t pix_bytes = vld1_u8(s);
			int16x4_t pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix_bytes)));
			int32x4_t sum = vmlal_s16(round, pix16, wts);
			int32x2_t pair_sum = vpadd_s32(vget_high_s32(sum), vget_low_s32(sum));
			pair_sum = vpadd_s32(pair_sum, pair_sum);
			q_pair_sum = vcombine_s32(pair_sum, vget_high_s32(q_pair_sum));
			*tmp++ = vget_lane_u8(vreinterpret_u8_u16(vqshrun_n_s32(q_pair_sum, WEIGHT_SHIFT-8)), 1);
		}
		else if (j <= 8)
		{
			int32x4_t q_pair_sum;
			int16x8_t wts = vld1q_s16(w);
			uint8x8_t pix_bytes = vld1_u8(s);
			int16x8_t pix16 = vreinterpretq_s16_u16(vmovl_u8(pix_bytes));
			int32x4_t sum = vmlal_s16(vmlal_s16(round, vget_low_s16(pix16), vget_low_s16(wts)),
									  vget_high_s16(pix16), vget_high_s16(wts));
			int32x2_t pair_sum = vpadd_s32(vget_high_s32(sum), vget_low_s32(sum));
			pair_sum = vpadd_s32(pair_sum, pair_sum);
			q_pair_sum = vcombine_s32(pair_sum, vget_high_s32(q_pair_sum));
			*tmp++ = vget_lane_u8(vreinterpret_u8_u16(vqshrun_n_s32(q_pair_sum, WEIGHT_SHIFT-8)), 1);
		}
		else
		{
			int32_t pixel0 = WEIGHT_ROUND;
			for (j = index->n; j > 0; j--)
			{
				pixel0 += *s++ * *w++;
			}
			pixel0 >>= WEIGHT_SHIFT;
			*tmp++ = CLAMP(pixel0, 0, 255);
		}
		index++;
		dst_w--;
	}
}

static void
zoom_x3_neon(uint8_t * FZ_RESTRICT tmp,
	const uint8_t * FZ_RESTRICT src,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t dst_w,
	uint32_t src_w,
	uint32_t channels,
	const uint8_t * FZ_RESTRICT bg)
{
	int32x4_t round = vdupq_n_s32(WEIGHT_ROUND);

	if (0)
slow:
	{
		/* Do any where we might index off the edge of the source */
		int pix_num = index->first_pixel;
		const uint8_t  *s = &src[pix_num * 3];
		const weight_t *w = &weights[index->index];
		uint32_t j = index->n;
		int32_t pixel0 = WEIGHT_ROUND;
		int32_t pixel1 = WEIGHT_ROUND;
		int32_t pixel2 = WEIGHT_ROUND;
		if (pix_num < 0)
		{
			int32_t wt = *w++;
			assert(pix_num == -1);
			pixel0 += bg[0] * wt;
			pixel1 += bg[1] * wt;
			pixel2 += bg[2] * wt;
			s += 3;
			j--;
			pix_num = 0;
		}
		pix_num = (int)src_w - pix_num;
		if (pix_num > (int)j)
			pix_num = j;
		j -= pix_num;
		while (pix_num > 0)
		{
			int32_t wt = *w++;
			pixel0 += *s++ * wt;
			pixel1 += *s++ * wt;
			pixel2 += *s++ * wt;
			pix_num--;
		}
		if (j > 0)
		{
			int32_t wt = *w++;
			assert(j == 1);
			pixel0 += bg[0] * wt;
			pixel1 += bg[1] * wt;
			pixel2 += bg[2] * wt;
		}
		pixel0 >>= WEIGHT_SHIFT;
		pixel1 >>= WEIGHT_SHIFT;
		pixel2 >>= WEIGHT_SHIFT;
		*tmp++ = CLAMP(pixel0, 0, 255);
		*tmp++ = CLAMP(pixel1, 0, 255);
		*tmp++ = CLAMP(pixel2, 0, 255);
		index++;
		dst_w--;
	}

	while (dst_w > 0)
	{
		const uint8_t  *s;
		int j;
		const weight_t *w;
		uint8x16_t pix_bytes;
		int32x4_t sum;
		uint8x8_t out_pix;

		/* Jump out of band to do the (rare) slow (edge) pixels */
		if (index->slow)
			goto slow;

		s = &src[index->first_pixel * 3];
		j = (int)index->n;
		w = &weights[index->index];

	pix_bytes = vld1q_u8(s);	// pix_bytes = ppoonnmmllkkjjiihhggffeeddccbbaa
		if (j == 4)
		{
			int16x4_t  pix16;
			int16x4_t  vw;
			vw = vdup_n_s16(w[0]);
			pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
			pix_bytes = vextq_u8(pix_bytes, pix_bytes, 3);
			sum = vmlal_s16(round, pix16, vw);
			vw = vdup_n_s16(w[1]);
			pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
			pix_bytes = vextq_u8(pix_bytes, pix_bytes, 3);
			sum = vmlal_s16(sum, pix16, vw);
			vw = vdup_n_s16(w[2]);
			pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
			pix_bytes = vextq_u8(pix_bytes, pix_bytes, 3);
			sum = vmlal_s16(sum, pix16, vw);
			vw = vdup_n_s16(w[3]);
			pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
			sum = vmlal_s16(sum, pix16, vw);
		}
		else
		{
			int off = j & 3;
			int16x4_t vw;
			s += (off ? off : 4) * 3;
			sum = round;
			/* This is a use of Duff's Device. I'm very sorry, but on the other hand, Yay! */
			switch (off)
			{
				do
				{
					int16x4_t  pix16;
					pix_bytes = vld1q_u8(s);	// pix_bytes = ppoonnmmllkkjjiihhggffeeddccbbaa
					s += 4 * 3;
			case 0:
					vw = vdup_n_s16(*w++);
					pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
					pix_bytes = vextq_u8(pix_bytes, pix_bytes, 3);
					sum = vmlal_s16(sum, pix16, vw);
			case 3:
					vw = vdup_n_s16(*w++);
					pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
					pix_bytes = vextq_u8(pix_bytes, pix_bytes, 3);
					sum = vmlal_s16(sum, pix16, vw);
			case 2:
					vw = vdup_n_s16(*w++);
					pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
					pix_bytes = vextq_u8(pix_bytes, pix_bytes, 3);
					sum = vmlal_s16(sum, pix16, vw);
			case 1:
					vw = vdup_n_s16(*w++);
					pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
					sum = vmlal_s16(sum, pix16, vw);
					j -= 4;
				} while (j > 0);
			}
		}
		out_pix = vreinterpret_u8_u16(vqshrun_n_s32(sum, WEIGHT_SHIFT-8));
		*tmp++ = vget_lane_u8(out_pix, 1);
		*tmp++ = vget_lane_u8(out_pix, 3);
		*tmp++ = vget_lane_u8(out_pix, 5);
		index++;
		dst_w--;
	}

	while (dst_w > 0)
	{
		const uint8_t *s;

		/* Jump out of band to do the (rare) slow (edge) pixels */
		if (index->slow)
			goto slow;

		s = &src[index->first_pixel * 3];

		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32_t pixel0 = WEIGHT_ROUND;
			int32_t pixel1 = WEIGHT_ROUND;
			int32_t pixel2 = WEIGHT_ROUND;
			for (j = index->n; j > 0; j--)
			{
				int32_t wt = *w++;
				pixel0 += *s++ * wt;
				pixel1 += *s++ * wt;
				pixel2 += *s++ * wt;
			}
			pixel0 >>= WEIGHT_SHIFT;
			pixel1 >>= WEIGHT_SHIFT;
			pixel2 >>= WEIGHT_SHIFT;
			*tmp++ = CLAMP(pixel0, 0, 255);
			*tmp++ = CLAMP(pixel1, 0, 255);
			*tmp++ = CLAMP(pixel2, 0, 255);
		}
		index++;
		dst_w--;
	}
}

static void
zoom_x4_neon(uint8_t * FZ_RESTRICT tmp,
	const uint8_t  * FZ_RESTRICT src,
	const index_t  * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t dst_w,
	uint32_t src_w,
	uint32_t channels,
	const uint8_t * FZ_RESTRICT bg)
{
	int32x4_t round = vdupq_n_s32(WEIGHT_ROUND);

	if (0)
slow:
	{
		/* Do any where we might index off the edge of the source */
		int pn = index->first_pixel;
		const uint8_t  *s = &src[pn * 4];
		const weight_t *w = &weights[index->index];
		uint32_t j = index->n;
		int32_t pixel0 = WEIGHT_ROUND;
		int32_t pixel1 = WEIGHT_ROUND;
		int32_t pixel2 = WEIGHT_ROUND;
		int32_t pixel3 = WEIGHT_ROUND;
		int pix_num = pn;
		if (pix_num < 0)
		{
			int32_t wt = *w++;
			assert(pix_num == -1);
			pixel0 += bg[0] * wt;
			pixel1 += bg[1] * wt;
			pixel2 += bg[2] * wt;
			pixel3 += bg[3] * wt;
			s += 4;
			j--;
			pix_num = 0;
		}
		pix_num = (int)src_w - pix_num;
		if (pix_num > (int)j)
			pix_num = j;
		j -= pix_num;
		while (pix_num > 0)
		{
			int32_t wt = *w++;
			pixel0 += *s++ * wt;
			pixel1 += *s++ * wt;
			pixel2 += *s++ * wt;
			pixel3 += *s++ * wt;
			pix_num--;
		}
		if (j > 0)
		{
			int32_t wt = *w;
			assert(j == 1);
			pixel0 += bg[0] * wt;
			pixel1 += bg[1] * wt;
			pixel2 += bg[2] * wt;
			pixel3 += bg[3] * wt;
		}
		pixel0 >>= WEIGHT_SHIFT;
		pixel1 >>= WEIGHT_SHIFT;
		pixel2 >>= WEIGHT_SHIFT;
		pixel3 >>= WEIGHT_SHIFT;
		*tmp++ = CLAMP(pixel0, 0, 255);
		*tmp++ = CLAMP(pixel1, 0, 255);
		*tmp++ = CLAMP(pixel2, 0, 255);
		*tmp++ = CLAMP(pixel3, 0, 255);
		index++;
		dst_w--;
	}

	while (dst_w > 0)
	{
		const uint8_t  *s;
		int j;
		const weight_t *w;
		int32x4_t sum;
		uint8x16_t pix_bytes;
		uint8x8_t out_pix;
		//__m128i mm0, mm1, mm4, mw0, mw1;

		/* Jump out of band to do the (rare) slow (edge) pixels */
		if (index->slow)
			goto slow;

		s = &src[index->first_pixel * 4];
		j = (int)index->n;
		w = &weights[index->index];

	pix_bytes = vld1q_u8(s);	// pix_bytes = ppoonnmmllkkjjiihhggffeeddccbbaa
		if (j == 4)
		{
			int16x4_t  pix16;
			int16x4_t  vw;
			vw = vdup_n_s16(w[0]);
			pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
			pix_bytes = vextq_u8(pix_bytes, pix_bytes, 4);
			sum = vmlal_s16(round, pix16, vw);
			vw = vdup_n_s16(w[1]);
			pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
			pix_bytes = vextq_u8(pix_bytes, pix_bytes, 4);
			sum = vmlal_s16(sum, pix16, vw);
			vw = vdup_n_s16(w[2]);
			pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
			pix_bytes = vextq_u8(pix_bytes, pix_bytes, 4);
			sum = vmlal_s16(sum, pix16, vw);
			vw = vdup_n_s16(w[3]);
			pix16 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
			sum = vmlal_s16(sum, pix16, vw);
		}
		else
		{
			int off = j & 3;
			int16x4_t  vw;
			s += (off ? off : 4) * 4;
			/* This is a use of Duff's Device. I'm very sorry, but on the other hand, Yay! */
			sum = round;
			switch (off)
			{
				do
				{
					int16x4_t  pixels;
					pix_bytes = vld1q_u8(s);	// pix_bytes = ppoonnmmllkkjjiihhggffeeddccbbaa
					s += 4 * 4;
			case 0:
					vw = vdup_n_s16(*w++);
					pixels = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
					pix_bytes = vextq_u8(pix_bytes, pix_bytes, 4);
					sum = vmlal_s16(sum, pixels, vw);
			case 3:
					vw = vdup_n_s16(*w++);
					pixels = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
					pix_bytes = vextq_u8(pix_bytes, pix_bytes, 4);
					sum = vmlal_s16(sum, pixels, vw);
			case 2:
					vw = vdup_n_s16(*w++);
					pixels = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
					pix_bytes = vextq_u8(pix_bytes, pix_bytes, 4);
					sum = vmlal_s16(sum, pixels, vw);
			case 1:
					vw = vdup_n_s16(*w++);
					pixels = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vget_low_u8(pix_bytes))));
					sum = vmlal_s16(sum, pixels, vw);
					j -= 4;
				} while (j > 0);
			}
		}
		out_pix = vreinterpret_u8_u16(vqshrun_n_s32(sum, WEIGHT_SHIFT-8));
		*tmp++ = vget_lane_u8(out_pix, 1);
		*tmp++ = vget_lane_u8(out_pix, 3);
		*tmp++ = vget_lane_u8(out_pix, 5);
		*tmp++ = vget_lane_u8(out_pix, 7);
		index++;
		dst_w--;
	}
}

static void
zoom_y1_neon(uint8_t * dst,
	const uint8_t * FZ_RESTRICT tmp,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t width,
	uint32_t channels,
	uint32_t mod,
	int32_t y)
{
	uint32_t stride = width;
	uint32_t offset = 0;
	int32x4_t round = vdupq_n_s32(WEIGHT_ROUND);

	if (0)
slow:
	{
		uint32_t off = (index->first_pixel + y) * stride + offset;

		offset++;
		if (off >= mod)
			off -= mod;

		{
			const weight_t *w = (const weight_t *)&weights[index->index * 4];
			uint32_t j;
			int32_t pixel0 = WEIGHT_ROUND;

			for (j = index->n; j > 0; j--)
			{
				pixel0 += tmp[off] * *w;
				w += 4;
				off += stride;
				if (off >= mod)
					off -= mod;
			}
			pixel0 >>= WEIGHT_SHIFT;
			*dst++ = CLAMP(pixel0, 0, 255);
		}
		index++;
		width--;
	}

	while (width > 0)
	{
		uint32_t off;
		/* The slow flag stops us accessing off the end of the source row.
		* It also tells us how many pixels we can do at once. This usage
		* is different for zoom_y1 than for all other cores. */
		int n = index->slow;
		if (n <= 1)
			goto slow;
		off = (index->first_pixel + y) * stride + offset;
		offset += n;
		if (off >= mod)
			off -= mod;

		{
			const weight_t *w = &weights[index->index * 4];
			uint32_t j = index->n;
			int32x4_t sum;
			uint16x4_t out16;

			if (j == 4)
			{
				uint8x8_t pix0, pix1, pix2, pix3;
				int16x4_t vw0, vw1, vw2, vw3;
				pix0 = vld1_u8(&tmp[off]);
				off += stride;
				if (off >= mod)
					off -= mod;
				vw0 = vld1_s16(w);
				w += 4;
				sum = vmlal_s16(round, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix0))), vw0);
				pix1 = vld1_u8(&tmp[off]);
				off += stride;
				if (off >= mod)
					off -= mod;
				vw1 = vld1_s16(w);
				w += 4;
				sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix1))), vw1);
				pix2 = vld1_u8(&tmp[off]);
				off += stride;
				if (off >= mod)
					off -= mod;
				vw2 = vld1_s16(w);
				w += 4;
				sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix2))), vw2);
				pix3 = vld1_u8(&tmp[off]);
				off += stride;
				if (off >= mod)
					off -= mod;
				vw3 = vld1_s16(w);
				sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix3))), vw3);
			}
			else
			{
				sum = round;
				for ( ; j > 0; j--)
				{
					uint8x8_t pix0;
					int16x4_t vw0;
					pix0 = vld1_u8(&tmp[off]);
					off += stride;
					if (off >= mod)
						off -= mod;
					vw0 = vld1_s16(w);
					w += 4;
					sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix0))), vw0);
				}
			}
			out16 = vqshrun_n_s32(sum, WEIGHT_SHIFT-8);
			*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 1);
			if (n > 1)
			{
				*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 3);
				if (n > 2)
				{
					*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 5);
					if (n > 3)
					{
						*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 7);
					}
				}
			}
		}
		index += n;
		width -= n;
	}
}

static void
zoom_y3_neon(uint8_t * dst,
	const uint8_t * FZ_RESTRICT tmp,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t width,
	uint32_t channels,
	uint32_t mod,
	int32_t y)
{
	uint32_t  stride = width * 3;
	uint32_t  offset = 0;

	while (width--)
	{
		const weight_t *w = &weights[index->index];
		uint32_t		j = index->n;
		int32x4_t	   sum;
		uint16x4_t	  out16;
		uint32_t		off = (index->first_pixel + y) * stride + offset;
		offset += 3;
		if (off >= mod)
			off -= mod;

		if (j == 4)
		{
			const weight_t *w = &weights[index->index];
			uint8x8_t	   pix0, pix1, pix2, pix3;
			int16x4_t	   vw0, vw1, vw2, vw3;
			pix0 = vld1_u8(&tmp[off]);
			off += stride;
			if (off >= mod)
				off -= mod;
			vw0 = vdup_n_s16(*w++);
			sum = vmlal_s16(vdupq_n_s32(WEIGHT_ROUND), vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix0))), vw0);
			pix1 = vld1_u8(&tmp[off]);
			off += stride;
			if (off >= mod)
				off -= mod;
			vw1 = vdup_n_s16(*w++);
			sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix1))), vw1);
			pix2 = vld1_u8(&tmp[off]);
			off += stride;
			if (off >= mod)
				off -= mod;
			vw2 = vdup_n_s16(*w++);
			sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix2))), vw2);
			pix3 = vld1_u8(&tmp[off]);
			off += stride;
			if (off >= mod)
				off -= mod;
			vw3 = vdup_n_s16(*w++);
			sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix3))), vw3);
		}
		else
		{
			sum = vdupq_n_s32(WEIGHT_ROUND);
			do
			{
				uint8x8_t pix0 = vld1_u8(&tmp[off]);
				int16x4_t vw0;
				off += stride;
				if (off >= mod)
					off -= mod;
				vw0 = vdup_n_s16(*w++);
				sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix0))), vw0);
			}
			while (--j);
		}
		out16 = vqshrun_n_s32(sum, WEIGHT_SHIFT-8);
		*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 1);
		*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 3);
		*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 5);
		index++;
	}
}

static void
zoom_y4_neon(uint8_t * dst,
	const uint8_t * FZ_RESTRICT tmp,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t width,
	uint32_t channels,
	uint32_t mod,
	int32_t y)
{
	uint32_t stride = width * 4;
	uint32_t offset = 0;
	int32x4_t round = vdupq_n_s32(WEIGHT_ROUND);

	while (width--)
	{
		uint32_t off = (index->first_pixel + y) * stride + offset;

		offset += 4;
		if (off >= mod)
			off -= mod;

		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32x4_t sum;
			uint16x4_t out16;

			if (j == 4)
			{
				uint8x8_t pix0, pix1, pix2, pix3;
				int16x4_t vw0, vw1, vw2, vw3;
				pix0 = vld1_u8(&tmp[off]);
				off += stride;
				if (off >= mod)
					off -= mod;
				vw0 = vdup_n_s16(*w++);
				sum = vmlal_s16(round, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix0))), vw0);
				pix1 = vld1_u8(&tmp[off]);
				off += stride;
				if (off >= mod)
					off -= mod;
				vw1 = vdup_n_s16(*w++);
				sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix1))), vw1);
				pix2 = vld1_u8(&tmp[off]);
				off += stride;
				if (off >= mod)
					off -= mod;
				vw2 = vdup_n_s16(*w++);
				sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix2))), vw2);
				pix3 = vld1_u8(&tmp[off]);
				off += stride;
				if (off >= mod)
					off -= mod;
				vw3 = vdup_n_s16(*w++);
				sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix3))), vw3);
			}
			else
			{
				sum = round;
				for ( ; j > 0; j--)
				{
					uint8x8_t pix0;
					int16x4_t vw0;
					pix0 = vld1_u8(&tmp[off]);
					off += stride;
					if (off >= mod)
						off -= mod;
					vw0 = vdup_n_s16(*w++);
					sum = vmlal_s16(sum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(pix0))), vw0);
				}
			}
			out16 = vqshrun_n_s32(sum, WEIGHT_SHIFT-8);
			*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 1);
			*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 3);
			*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 5);
			*dst++ = vget_lane_u8(vreinterpret_u8_u16(out16), 7);
		}
		index++;
	}
}
