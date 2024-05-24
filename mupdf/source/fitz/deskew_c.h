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

/* This file is included from deskew.c if C cores are allowed. */

static void
zoom_x(uint8_t * FZ_RESTRICT tmp,
	const uint8_t * FZ_RESTRICT src,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t dst_w,
	uint32_t src_w,
	uint32_t channels,
	const uint8_t * FZ_RESTRICT bg)
{
	if (0)
slow:
	{
		uint32_t i;
		/* Do any where we might index off the edge of the source */
		int pn = index->first_pixel;
		const uint8_t *s0 = &src[pn * (int)channels];

		for (i = 0; i < channels; i++)
		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32_t pixel = WEIGHT_ROUND;
			int pix_num = pn;
			const uint8_t *s = s0++;
			if (pix_num < 0)
			{
				assert(pix_num == -1);
				pixel += bg[i] * *w++;
				s += channels;
				j--;
				pix_num = 0;
			}
			pix_num = (int)src_w - pix_num;
			if (pix_num > (int)j)
				pix_num = j;
			j -= pix_num;
			while (pix_num > 0)
			{
				pixel += *s * *w++;
				s += channels;
				pix_num--;
			}
			if (j > 0)
			{
				assert(j == 1);
				pixel += bg[i] * *w++;
			}
			pixel >>= WEIGHT_SHIFT;
			*tmp++ = CLAMP(pixel, 0, 255);
		}
		index++;
		dst_w--;
	}

	while (dst_w > 0)
	{
		int i;
		const uint8_t *sr;

		/* Jump out of band to do the (rare) slow (edge) pixels */
		if (index->slow)
			goto slow;

		sr = &src[index->first_pixel * channels];

		for (i = channels; i > 0; i--)
		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32_t pixel = WEIGHT_ROUND;
			const uint8_t *s = sr++;
			for (j = index->n; j > 0; j--, s += channels)
			{
				pixel += *s * *w++;
			}
			pixel >>= WEIGHT_SHIFT;
			*tmp++ = CLAMP(pixel, 0, 255);
		}
		index++;
		dst_w--;
	}
}

static void
zoom_x1(uint8_t * FZ_RESTRICT tmp,
	const uint8_t * FZ_RESTRICT src,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t dst_w,
	uint32_t src_w,
	uint32_t channels,
	const uint8_t * FZ_RESTRICT bg)
{
	if (0)
slow:
	{
		/* Do any where we might index off the edge of the source */
		int pix_num = index->first_pixel;
		const uint8_t *s = &src[pix_num];
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
		const uint8_t *s;

		/* Jump out of band to do the (rare) slow (edge) pixels */
		if (index->slow)
			goto slow;

		s = &src[index->first_pixel];

		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
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
zoom_x3(uint8_t * FZ_RESTRICT tmp,
	const uint8_t * FZ_RESTRICT src,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t dst_w,
	uint32_t src_w,
	uint32_t channels,
	const uint8_t * FZ_RESTRICT bg)
{
	if (0)
slow:
	{
		/* Do any where we might index off the edge of the source */
		int			 pix_num = index->first_pixel;
		const uint8_t *s = &src[pix_num * 3];
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
zoom_x4(uint8_t * FZ_RESTRICT tmp,
	const uint8_t * FZ_RESTRICT src,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t dst_w,
	uint32_t src_w,
	uint32_t channels,
	const uint8_t * FZ_RESTRICT bg)
{
	if (0)
slow:
	{
		/* Do any where we might index off the edge of the source */
		int pn = index->first_pixel;
		const uint8_t *s = &src[pn * 4];
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
		const uint8_t *s;

		/* Jump out of band to do the (rare) slow (edge) pixels */
		if (index->slow)
			goto slow;

		s = &src[index->first_pixel * 4];

		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32_t pixel0 = WEIGHT_ROUND;
			int32_t pixel1 = WEIGHT_ROUND;
			int32_t pixel2 = WEIGHT_ROUND;
			int32_t pixel3 = WEIGHT_ROUND;
			for (j = index->n; j > 0; j--)
			{
				int32_t wt = *w++;
				pixel0 += *s++ * wt;
				pixel1 += *s++ * wt;
				pixel2 += *s++ * wt;
				pixel3 += *s++ * wt;
			}
			pixel0 >>= WEIGHT_SHIFT;
			pixel1 >>= WEIGHT_SHIFT;
			pixel2 >>= WEIGHT_SHIFT;
			pixel3 >>= WEIGHT_SHIFT;
			*tmp++ = CLAMP(pixel0, 0, 255);
			*tmp++ = CLAMP(pixel1, 0, 255);
			*tmp++ = CLAMP(pixel2, 0, 255);
			*tmp++ = CLAMP(pixel3, 0, 255);
		}
		index++;
		dst_w--;
	}
}

static void
zoom_y(uint8_t * dst,
	const uint8_t * FZ_RESTRICT tmp,
	const index_t * FZ_RESTRICT index,
	const weight_t * FZ_RESTRICT weights,
	uint32_t width,
	uint32_t channels,
	uint32_t mod,
	int32_t y)
{
	uint32_t stride = width * channels;
	uint32_t offset = 0;

	while (width--)
	{
		uint32_t i;
		uint32_t off = (index->first_pixel + y) * stride + offset;

		offset += channels;
		if (off >= mod)
			off -= mod;

		for (i = 0; i < channels; i++)
		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32_t pixel = WEIGHT_ROUND;
			uint32_t o = off++;

			for (j = index->n; j > 0; j--)
			{
				pixel += tmp[o] * *w++;
				o += stride;
				if (o >= mod)
					o -= mod;
			}
			pixel >>= WEIGHT_SHIFT;
			*dst++ = CLAMP(pixel, 0, 255);
		}
		index++;
	}
}

static void
zoom_y1(uint8_t * dst,
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

	while (width--)
	{
		uint32_t off = (index->first_pixel + y) * stride + offset;

		offset++;
		if (off >= mod)
			off -= mod;

		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32_t pixel0 = WEIGHT_ROUND;

			for (j = index->n; j > 0; j--)
			{
				pixel0 += tmp[off ] * *w++;
				off += stride;
				if (off >= mod)
					off -= mod;
			}
			pixel0 >>= WEIGHT_SHIFT;
			*dst++ = CLAMP(pixel0, 0, 255);
		}
		index++;
	}
}

static void
zoom_y3(uint8_t * dst,
		const uint8_t * FZ_RESTRICT tmp,
		const index_t * FZ_RESTRICT index,
		const weight_t * FZ_RESTRICT weights,
		uint32_t width,
		uint32_t channels,
		uint32_t mod,
		int32_t y)
{
	uint32_t stride = width * 3;
	uint32_t offset = 0;

	while (width--)
	{
		uint32_t off = (index->first_pixel + y) * stride + offset;

		offset += 3;
		if (off >= mod)
			off -= mod;

		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32_t pixel0 = WEIGHT_ROUND;
			int32_t pixel1 = WEIGHT_ROUND;
			int32_t pixel2 = WEIGHT_ROUND;

			for (j = index->n; j > 0; j--)
			{
				int32_t wt = *w++;
				pixel0 += tmp[off ] * wt;
				pixel1 += tmp[off+1] * wt;
				pixel2 += tmp[off+2] * wt;
				off += stride;
				if (off >= mod)
					off -= mod;
			}
			pixel0 >>= WEIGHT_SHIFT;
			pixel1 >>= WEIGHT_SHIFT;
			pixel2 >>= WEIGHT_SHIFT;
			*dst++ = CLAMP(pixel0, 0, 255);
			*dst++ = CLAMP(pixel1, 0, 255);
			*dst++ = CLAMP(pixel2, 0, 255);
		}
		index++;
	}
}

static void
zoom_y4(uint8_t * dst,
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

	while (width--)
	{
		uint32_t off = (index->first_pixel + y) * stride + offset;

		offset += 4;
		if (off >= mod)
			off -= mod;

		{
			const weight_t *w = &weights[index->index];
			uint32_t j = index->n;
			int32_t pixel0 = WEIGHT_ROUND;
			int32_t pixel1 = WEIGHT_ROUND;
			int32_t pixel2 = WEIGHT_ROUND;
			int32_t pixel3 = WEIGHT_ROUND;

			for (j = index->n; j > 0; j--)
			{
				int32_t wt = *w++;
				pixel0 += tmp[off] * wt;
				pixel1 += tmp[off+1] * wt;
				pixel2 += tmp[off+2] * wt;
				pixel3 += tmp[off+3] * wt;
				off += stride;
				if (off >= mod)
					off -= mod;
			}
			pixel0 >>= WEIGHT_SHIFT;
			pixel1 >>= WEIGHT_SHIFT;
			pixel2 >>= WEIGHT_SHIFT;
			pixel3 >>= WEIGHT_SHIFT;
			*dst++ = CLAMP(pixel0, 0, 255);
			*dst++ = CLAMP(pixel1, 0, 255);
			*dst++ = CLAMP(pixel2, 0, 255);
			*dst++ = CLAMP(pixel3, 0, 255);
		}
		index++;
	}
}
