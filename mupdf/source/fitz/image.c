// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#include "context-imp.h"
#include "image-imp.h"
#include "pixmap-imp.h"

#include <string.h>
#include <math.h>
#include <assert.h>

/* TODO: here or public? */
static int
fz_key_storable_needs_reaping(fz_context *ctx, const fz_key_storable *ks)
{
	return ks == NULL ? 0 : (ks->store_key_refs == ks->storable.refs);
}

#define SANE_DPI 72.0f
#define INSANE_DPI 4800.0f

#define SCALABLE_IMAGE_DPI 96

struct fz_compressed_image
{
	fz_image super;
	fz_compressed_buffer *buffer;
};

struct fz_pixmap_image
{
	fz_image super;
	fz_pixmap *tile;
};

typedef struct
{
	int refs;
	fz_image *image;
	int l2factor;
	fz_irect rect;
} fz_image_key;

fz_image *
fz_keep_image(fz_context *ctx, fz_image *image)
{
	return fz_keep_key_storable(ctx, &image->key_storable);
}

fz_image *
fz_keep_image_store_key(fz_context *ctx, fz_image *image)
{
	return fz_keep_key_storable_key(ctx, &image->key_storable);
}

void
fz_drop_image_store_key(fz_context *ctx, fz_image *image)
{
	fz_drop_key_storable_key(ctx, &image->key_storable);
}

static int
fz_make_hash_image_key(fz_context *ctx, fz_store_hash *hash, void *key_)
{
	fz_image_key *key = (fz_image_key *)key_;
	hash->u.pir.ptr = key->image;
	hash->u.pir.i = key->l2factor;
	hash->u.pir.r = key->rect;
	return 1;
}

static void *
fz_keep_image_key(fz_context *ctx, void *key_)
{
	fz_image_key *key = (fz_image_key *)key_;
	return fz_keep_imp(ctx, key, &key->refs);
}

static void
fz_drop_image_key(fz_context *ctx, void *key_)
{
	fz_image_key *key = (fz_image_key *)key_;
	if (fz_drop_imp(ctx, key, &key->refs))
	{
		fz_drop_image_store_key(ctx, key->image);
		fz_free(ctx, key);
	}
}

static int
fz_cmp_image_key(fz_context *ctx, void *k0_, void *k1_)
{
	fz_image_key *k0 = (fz_image_key *)k0_;
	fz_image_key *k1 = (fz_image_key *)k1_;
	return k0->image == k1->image && k0->l2factor == k1->l2factor && k0->rect.x0 == k1->rect.x0 && k0->rect.y0 == k1->rect.y0 && k0->rect.x1 == k1->rect.x1 && k0->rect.y1 == k1->rect.y1;
}

static void
fz_format_image_key(fz_context *ctx, char *s, size_t n, void *key_)
{
	fz_image_key *key = (fz_image_key *)key_;
	fz_snprintf(s, n, "(image %d x %d sf=%d)", key->image->w, key->image->h, key->l2factor);
}

static int
fz_needs_reap_image_key(fz_context *ctx, void *key_)
{
	fz_image_key *key = (fz_image_key *)key_;

	return fz_key_storable_needs_reaping(ctx, &key->image->key_storable);
}

static const fz_store_type fz_image_store_type =
{
	"fz_image",
	fz_make_hash_image_key,
	fz_keep_image_key,
	fz_drop_image_key,
	fz_cmp_image_key,
	fz_format_image_key,
	fz_needs_reap_image_key
};

void
fz_drop_image(fz_context *ctx, fz_image *image)
{
	fz_drop_key_storable(ctx, &image->key_storable);
}

static void
fz_mask_color_key(fz_context *ctx, fz_pixmap *pix, int n, int bpc, const int *colorkey_in, int indexed)
{
	unsigned char *p = pix->samples;
	int w;
	int k, t;
	int h = pix->h;
	size_t stride = pix->stride - pix->w * (size_t)pix->n;
	int colorkey[FZ_MAX_COLORS * 2];
	int scale, shift, max;

	if (pix->w == 0)
		return;

	if (indexed)
	{
		/* no upscaling or downshifting needed for indexed images */
		scale = 1;
		shift = 0;
	}
	else
	{
		switch (bpc)
		{
		case 1: scale = 255; shift = 0; break;
		case 2: scale = 85; shift = 0; break;
		case 4: scale = 17; shift = 0; break;
		default:
		case 8: scale = 1; shift = 0; break;
		case 16: scale = 1; shift = 8; break;
		case 24: scale = 1; shift = 16; break;
		case 32: scale = 1; shift = 24; break;
		}
	}

	switch (bpc)
	{
	case 1: max = 1; break;
	case 2: max = 3; break;
	case 4: max = 15; break;
	default:
	case 8: max = 0xff; break;
	case 16: max = 0xffff; break;
	case 24: max = 0xffffff; break;
	case 32: max = 0xffffffff; break;
	}

	for (k = 0; k < 2 * n; k++)
	{
		colorkey[k] = colorkey_in[k];

		if (colorkey[k] > max)
		{
			if (indexed && bpc == 1)
			{
				if (k == 0)
				{
					fz_warn(ctx, "first color key masking value out of range in 1bpc indexed image, ignoring color key masking");
					return;
				}
				fz_warn(ctx, "later color key masking value out of range in 1bpc indexed image, assumed to be 1");
				colorkey[k] = 1;
			}
			else if (bpc != 1)
			{
				fz_warn(ctx, "color key masking value out of range, masking to valid range");
				colorkey[k] &= max;
			}
		}

		if (colorkey[k] < 0 || colorkey[k] > max)
		{
			fz_warn(ctx, "color key masking value out of range, clamping to valid range");
			colorkey[k] = fz_clampi(colorkey[k], 0, max);
		}

		if (scale > 1)
		{
			/* scale up color key masking value so it can be compared with samples. */
			colorkey[k] *= scale;
		}
		else if (shift > 0)
		{
			/* shifting down color key masking value so it can be compared with samples. */
			colorkey[k] >>= shift;
		}
	}

	while (h--)
	{
		w = pix->w;
		do
		{
			t = 1;
			for (k = 0; k < n; k++)
				if (p[k] < colorkey[k * 2] || p[k] > colorkey[k * 2 + 1])
					t = 0;
			if (t)
				for (k = 0; k < pix->n; k++)
					p[k] = 0;
			p += pix->n;
		}
		while (--w);
		p += stride;
	}
}

static void
fz_unblend_masked_tile(fz_context *ctx, fz_pixmap *tile, fz_image *image, const fz_irect *isa)
{
	fz_pixmap *mask;
	unsigned char *s, *d = tile->samples;
	int n = tile->n;
	int k;
	size_t sstride, dstride = tile->stride - tile->w * (size_t)tile->n;
	int h;
	fz_irect subarea;

	/* We need at least as much of the mask as there was of the tile. */
	if (isa)
		subarea = *isa;
	else
	{
		subarea.x0 = 0;
		subarea.y0 = 0;
		subarea.x1 = tile->w;
		subarea.y1 = tile->h;
	}

	mask = fz_get_pixmap_from_image(ctx, image->mask, &subarea, NULL, NULL, NULL);
	s = mask->samples;
	/* RJW: Urgh, bit of nastiness here. fz_pixmap_from_image will either return
	 * an exact match for the subarea we asked for, or the full image, and the
	 * normal way to know is that the matrix will be updated. That doesn't help
	 * us here. */
	if (image->mask->w == mask->w && image->mask->h == mask->h) {
		subarea.x0 = 0;
		subarea.y0 = 0;
	}
	if (isa)
		s += (isa->x0 - subarea.x0) * (size_t)mask->n + (isa->y0 - subarea.y0) * (size_t)mask->stride;
	sstride = mask->stride - tile->w * (size_t)mask->n;
	h = tile->h;

	if (tile->w != 0)
	{
		while (h--)
		{
			int w = tile->w;
			do
			{
				if (*s == 0)
					for (k = 0; k < image->n; k++)
						d[k] = image->colorkey[k];
				else
					for (k = 0; k < image->n; k++)
						d[k] = fz_clampi(image->colorkey[k] + (d[k] - image->colorkey[k]) * 255 / *s, 0, 255);
				s++;
				d += n;
			}
			while (--w);
			s += sstride;
			d += dstride;
		}
	}

	fz_drop_pixmap(ctx, mask);
}

static void fz_adjust_image_subarea(fz_context *ctx, fz_image *image, fz_irect *subarea, int l2factor)
{
	int f = 1<<l2factor;
	int bpp = image->bpc * image->n;
	int mask;

	switch (bpp)
	{
	case 1: mask = 8*f; break;
	case 2: mask = 4*f; break;
	case 4: mask = 2*f; break;
	default: mask = (bpp & 7) == 0 ? f : 0; break;
	}

	if (mask != 0)
	{
		subarea->x0 &= ~(mask - 1);
		subarea->x1 = (subarea->x1 + mask - 1) & ~(mask - 1);
	}
	else
	{
		/* Awkward case - mask cannot be a power of 2. */
		mask = bpp*f;
		switch (bpp)
		{
		case 3:
		case 5:
		case 7:
		case 9:
		case 11:
		case 13:
		case 15:
		default:
			mask *= 8;
			break;
		case 6:
		case 10:
		case 14:
			mask *= 4;
			break;
		case 12:
			mask *= 2;
			break;
		}
		subarea->x0 = (subarea->x0 / mask) * mask;
		subarea->x1 = ((subarea->x1 + mask - 1) / mask) * mask;
	}

	subarea->y0 &= ~(f - 1);
	if (subarea->x1 > image->w)
		subarea->x1 = image->w;
	subarea->y1 = (subarea->y1 + f - 1) & ~(f - 1);
	if (subarea->y1 > image->h)
		subarea->y1 = image->h;
}

static void fz_compute_image_key(fz_context *ctx, fz_image *image, fz_matrix *ctm,
	fz_image_key *key, const fz_irect *subarea, int l2factor, int *w, int *h, int *dw, int *dh)
{
	key->refs = 1;
	key->image = image;
	key->l2factor = l2factor;

	if (subarea == NULL)
	{
		key->rect.x0 = 0;
		key->rect.y0 = 0;
		key->rect.x1 = image->w;
		key->rect.y1 = image->h;
	}
	else
	{
		key->rect = *subarea;
		ctx->tuning->image_decode(ctx->tuning->image_decode_arg, image->w, image->h, key->l2factor, &key->rect);
		fz_adjust_image_subarea(ctx, image, &key->rect, key->l2factor);
	}

	/* Based on that subarea, recalculate the extents */
	if (ctm)
	{
		float frac_w = (float) (key->rect.x1 - key->rect.x0) / image->w;
		float frac_h = (float) (key->rect.y1 - key->rect.y0) / image->h;
		float a = ctm->a * frac_w;
		float b = ctm->b * frac_w;
		float c = ctm->c * frac_h;
		float d = ctm->d * frac_h;
		*w = sqrtf(a * a + b * b);
		*h = sqrtf(c * c + d * d);
	}
	else
	{
		*w = image->w;
		*h = image->h;
	}

	/* Return the true sizes to the caller */
	if (dw)
		*dw = *w;
	if (dh)
		*dh = *h;
	if (*w > image->w)
		*w = image->w;
	if (*h > image->h)
		*h = image->h;

	if (*w == 0 || *h == 0)
		key->l2factor = 0;
}

typedef struct {
	fz_stream *src;
	size_t l_skip; /* Number of bytes to skip on the left. */
	size_t r_skip; /* Number of bytes to skip on the right. */
	size_t b_skip; /* Number of bytes to skip on the bottom. */
	int lines; /* Number of lines left to copy. */
	size_t stride; /* Number of bytes to read in the image. */
	size_t nskip; /* Number of bytes left to skip on this line. */
	size_t nread; /* Number of bytes left to read on this line. */
} subarea_state;

static int
subarea_next(fz_context *ctx, fz_stream *stm, size_t len)
{
	subarea_state *state = stm->state;
	size_t n;

	stm->wp = stm->rp = NULL;

	while (state->nskip > 0)
	{
		n = fz_skip(ctx, state->src, state->nskip);
		if (n == 0)
			return EOF;
		state->nskip -= n;
	}
	if (state->lines == 0)
		return EOF;
	n = fz_available(ctx, state->src, state->nread);
	if (n > state->nread)
		n = state->nread;
	if (n == 0)
		return EOF;
	stm->rp = state->src->rp;
	stm->wp = stm->rp + n;
	stm->pos += n;
	state->src->rp = stm->wp;
	state->nread -= n;
	if (state->nread == 0)
	{
		state->lines--;
		if (state->lines == 0)
			state->nskip = state->r_skip + state->b_skip;
		else
			state->nskip = state->l_skip + state->r_skip;
		state->nread = state->stride;
	}
	return *stm->rp++;
}

static void
subarea_drop(fz_context *ctx, void *state)
{
	fz_free(ctx, state);
}

static fz_stream *
subarea_stream(fz_context *ctx, fz_stream *stm, fz_image *image, const fz_irect *subarea, int l2factor)
{
	subarea_state *state;
	int f = 1<<l2factor;
	int stream_w = (image->w + f - 1)>>l2factor;
	size_t stream_stride = (stream_w * (size_t)image->n * image->bpc + 7) / 8;
	int l_margin = subarea->x0 >> l2factor;
	int t_margin = subarea->y0 >> l2factor;
	int r_margin = (image->w + f - 1 - subarea->x1) >> l2factor;
	int b_margin = (image->h + f - 1 - subarea->y1) >> l2factor;
	size_t l_skip = (l_margin * (size_t)image->n * image->bpc)/8;
	size_t r_skip = (r_margin * (size_t)image->n * image->bpc + 7)/8;
	size_t t_skip = t_margin * stream_stride;
	size_t b_skip = b_margin * stream_stride;
	int h = (subarea->y1 - subarea->y0 + f - 1) >> l2factor;
	int w = (subarea->x1 - subarea->x0 + f - 1) >> l2factor;
	size_t stride = (w * (size_t)image->n * image->bpc + 7) / 8;

	state = fz_malloc_struct(ctx, subarea_state);
	state->src = stm;
	state->l_skip = l_skip;
	state->r_skip = r_skip;
	state->b_skip = b_skip;
	state->lines = h;
	state->nskip = l_skip+t_skip;
	state->stride = stride;
	state->nread = stride;

	return fz_new_stream(ctx, state, subarea_next, subarea_drop);
}

typedef struct
{
	fz_stream *src;
	int w; /* Width in source pixels. */
	int h; /* Height (remaining) in scanlines. */
	int n; /* Number of components. */
	int f; /* Fill level (how many scanlines we've copied in). */
	size_t r; /* How many samples Remain to be filled in this line. */
	int l2; /* The amount of subsampling we're doing. */
	unsigned char data[1];
} l2sub_state;

static void
subsample_drop(fz_context *ctx, void *state)
{
	fz_free(ctx, state);
}

static int
subsample_next(fz_context *ctx, fz_stream *stm, size_t len)
{
	l2sub_state *state = (l2sub_state *)stm->state;
	size_t fill;

	stm->rp = stm->wp = &state->data[0];
	if (state->h == 0)
		return EOF;

	/* Copy in data */
	do
	{
		if (state->r == 0)
			state->r = state->w * (size_t)state->n;

		while (state->r > 0)
		{
			size_t a;
			a = fz_available(ctx, state->src, state->r);
			if (a == 0)
				return EOF;
			if (a > state->r)
				a = state->r;
			memcpy(&state->data[state->w * (size_t)state->n * (state->f+1) - state->r],
				state->src->rp, a);
			state->src->rp += a;
			state->r -= a;
		}
		state->f++;
		state->h--;
	}
	while (state->h > 0 && state->f != (1<<state->l2));

	/* Perform the subsample */
	fz_subsample_pixblock(state->data, state->w, state->f, state->n, state->l2, state->w * (size_t)state->n);
	state->f = 0;

	/* Update data pointers. */
	fill = ((state->w + (1<<state->l2) - 1)>>state->l2) * (size_t)state->n;
	stm->pos += fill;
	stm->rp = &state->data[0];
	stm->wp = &state->data[fill];

	return *stm->rp++;
}

static fz_stream *
subsample_stream(fz_context *ctx, fz_stream *src, int w, int h, int n, int l2extra)
{
	l2sub_state *state = fz_malloc(ctx, sizeof(l2sub_state) + w*(size_t)(n<<l2extra));

	state->src = src;
	state->w = w;
	state->h = h;
	state->n = n;
	state->f = 0;
	state->r = 0;
	state->l2 = l2extra;

	return fz_new_stream(ctx, state, subsample_next, subsample_drop);
}

/* l2factor is the amount of subsampling that the decoder is going to be
 * doing for us already. (So for JPEG 0,1,2,3 corresponding to 1, 2, 4,
 * 8. For other formats, probably 0.). l2extra is the additional amount
 * of subsampling we should perform here. */
fz_pixmap *
fz_decomp_image_from_stream(fz_context *ctx, fz_stream *stm, fz_compressed_image *cimg, fz_irect *subarea, int indexed, int l2factor, int *l2extra)
{
	fz_image *image = &cimg->super;
	fz_pixmap *tile = NULL;
	size_t stride, len, i;
	unsigned char *samples = NULL;
	int f = 1<<l2factor;
	int w = image->w;
	int h = image->h;
	int matte = image->use_colorkey && image->mask;
	fz_stream *read_stream = stm;
	fz_stream *sstream = NULL;
	fz_stream *l2stream = NULL;
	fz_stream *unpstream = NULL;

	if (matte)
	{
		/* Can't do l2factor decoding */
		if (image->w != image->mask->w || image->h != image->mask->h)
		{
			fz_warn(ctx, "mask must be of same size as image for /Matte");
			matte = 0;
		}
		assert(l2factor == 0);
	}
	if (subarea)
	{
		if (subarea->x0 == 0 && subarea->x1 == image->w &&
			subarea->y0 == 0 && subarea->y1 == image->h)
			subarea = NULL;
		else
		{
			fz_adjust_image_subarea(ctx, image, subarea, l2factor);
			w = (subarea->x1 - subarea->x0);
			h = (subarea->y1 - subarea->y0);
		}
	}
	w = (w + f - 1) >> l2factor;
	h = (h + f - 1) >> l2factor;

	fz_var(tile);
	fz_var(samples);
	fz_var(sstream);
	fz_var(unpstream);
	fz_var(l2stream);

	fz_try(ctx)
	{
		int alpha = (image->colorspace == NULL);
		if (image->use_colorkey)
			alpha = 1;

		if (subarea)
			read_stream = sstream = subarea_stream(ctx, stm, image, subarea, l2factor);
		if (image->bpc != 8 || image->use_colorkey)
			read_stream = unpstream = fz_unpack_stream(ctx, read_stream, image->bpc, w, h, image->n, indexed, image->use_colorkey, 0);
		if (l2extra && *l2extra && !indexed)
		{
			read_stream = l2stream = subsample_stream(ctx, read_stream, w, h, image->n + image->use_colorkey, *l2extra);
			w = (w + (1<<*l2extra) - 1)>>*l2extra;
			h = (h + (1<<*l2extra) - 1)>>*l2extra;
			*l2extra = 0;
		}

		tile = fz_new_pixmap(ctx, image->colorspace, w, h, NULL, alpha);
		if (image->interpolate & FZ_PIXMAP_FLAG_INTERPOLATE)
			tile->flags |= FZ_PIXMAP_FLAG_INTERPOLATE;
		else
			tile->flags &= ~FZ_PIXMAP_FLAG_INTERPOLATE;

		samples = tile->samples;
		stride = tile->stride;

		len = fz_read(ctx, read_stream, samples, h * stride);

		/* Pad truncated images */
		if (len < stride * h)
		{
			fz_warn(ctx, "padding truncated image");
			memset(samples + len, 0, stride * h - len);
		}

		/* Invert 1-bit image masks */
		if (image->imagemask)
		{
			/* 0=opaque and 1=transparent so we need to invert */
			unsigned char *p = samples;
			len = h * stride;
			for (i = 0; i < len; i++)
				p[i] = ~p[i];
		}

		/* color keyed transparency */
		if (image->use_colorkey && !image->mask)
			fz_mask_color_key(ctx, tile, image->n, image->bpc, image->colorkey, indexed);

		if (indexed)
		{
			fz_pixmap *conv;
			fz_decode_indexed_tile(ctx, tile, image->decode, (1 << image->bpc) - 1);
			conv = fz_convert_indexed_pixmap_to_base(ctx, tile);
			fz_drop_pixmap(ctx, tile);
			tile = conv;
		}
		else if (image->use_decode)
		{
			fz_decode_tile(ctx, tile, image->decode);
		}

		/* pre-blended matte color */
		if (matte)
			fz_unblend_masked_tile(ctx, tile, image, subarea);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, sstream);
		fz_drop_stream(ctx, unpstream);
		fz_drop_stream(ctx, l2stream);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, tile);
		fz_rethrow(ctx);
	}

	return tile;
}

void
fz_drop_image_base(fz_context *ctx, fz_image *image)
{
	fz_drop_colorspace(ctx, image->colorspace);
	fz_drop_image(ctx, image->mask);
	fz_free(ctx, image);
}

void
fz_drop_image_imp(fz_context *ctx, fz_storable *image_)
{
	fz_image *image = (fz_image *)image_;

	image->drop_image(ctx, image);
	fz_drop_image_base(ctx, image);
}

static void
drop_compressed_image(fz_context *ctx, fz_image *image_)
{
	fz_compressed_image *image = (fz_compressed_image *)image_;

	fz_drop_compressed_buffer(ctx, image->buffer);
}

static void
drop_pixmap_image(fz_context *ctx, fz_image *image_)
{
	fz_pixmap_image *image = (fz_pixmap_image *)image_;

	fz_drop_pixmap(ctx, image->tile);
}

static fz_pixmap *
compressed_image_get_pixmap(fz_context *ctx, fz_image *image_, fz_irect *subarea, int w, int h, int *l2factor)
{
	fz_compressed_image *image = (fz_compressed_image *)image_;
	int native_l2factor;
	fz_stream *stm;
	int indexed;
	fz_pixmap *tile;
	int can_sub = 0;
	int local_l2factor;

	/* If we are using matte, then the decode code requires both image and tile sizes
	 * to match. The simplest way to ensure this is to do no native l2factor decoding.
	 */
	if (image->super.use_colorkey && image->super.mask)
	{
		local_l2factor = 0;
		l2factor = &local_l2factor;
	}

	/* We need to make a new one. */
	/* First check for ones that we can't decode using streams */
	switch (image->buffer->params.type)
	{
	case FZ_IMAGE_PNG:
		tile = fz_load_png(ctx, image->buffer->buffer->data, image->buffer->buffer->len);
		break;
	case FZ_IMAGE_GIF:
		tile = fz_load_gif(ctx, image->buffer->buffer->data, image->buffer->buffer->len);
		break;
	case FZ_IMAGE_BMP:
		tile = fz_load_bmp(ctx, image->buffer->buffer->data, image->buffer->buffer->len);
		break;
	case FZ_IMAGE_TIFF:
		tile = fz_load_tiff(ctx, image->buffer->buffer->data, image->buffer->buffer->len);
		break;
	case FZ_IMAGE_PNM:
		tile = fz_load_pnm(ctx, image->buffer->buffer->data, image->buffer->buffer->len);
		break;
	case FZ_IMAGE_JXR:
		tile = fz_load_jxr(ctx, image->buffer->buffer->data, image->buffer->buffer->len);
		break;
	case FZ_IMAGE_JPX:
		tile = fz_load_jpx(ctx, image->buffer->buffer->data, image->buffer->buffer->len, image->super.colorspace);
		break;
	case FZ_IMAGE_PSD:
		tile = fz_load_psd(ctx, image->buffer->buffer->data, image->buffer->buffer->len);
		break;
	case FZ_IMAGE_JPEG:
		/* Scan JPEG stream and patch missing height values in header */
		{
			unsigned char *s = image->buffer->buffer->data;
			unsigned char *e = s + image->buffer->buffer->len;
			unsigned char *d;
			for (d = s + 2; s < d && d + 9 < e && d[0] == 0xFF; d += (d[2] << 8 | d[3]) + 2)
			{
				if (d[1] < 0xC0 || (0xC3 < d[1] && d[1] < 0xC9) || 0xCB < d[1])
					continue;
				if ((d[5] == 0 && d[6] == 0) || ((d[5] << 8) | d[6]) > image->super.h)
				{
					d[5] = (image->super.h >> 8) & 0xFF;
					d[6] = image->super.h & 0xFF;
				}
			}
		}
		/* fall through */

	default:
		native_l2factor = l2factor ? *l2factor : 0;
		stm = fz_open_image_decomp_stream_from_buffer(ctx, image->buffer, l2factor);
		fz_try(ctx)
		{
			if (l2factor)
				native_l2factor -= *l2factor;
			indexed = fz_colorspace_is_indexed(ctx, image->super.colorspace);
			can_sub = 1;
			tile = fz_decomp_image_from_stream(ctx, stm, image, subarea, indexed, native_l2factor, l2factor);
		}
		fz_always(ctx)
			fz_drop_stream(ctx, stm);
		fz_catch(ctx)
			fz_rethrow(ctx);

		break;
	}

	if (can_sub == 0 && subarea != NULL)
	{
		subarea->x0 = 0;
		subarea->y0 = 0;
		subarea->x1 = image->super.w;
		subarea->y1 = image->super.h;
	}

	return tile;
}

static fz_pixmap *
pixmap_image_get_pixmap(fz_context *ctx, fz_image *image_, fz_irect *subarea, int w, int h, int *l2factor)
{
	fz_pixmap_image *image = (fz_pixmap_image *)image_;

	/* 'Simple' images created direct from pixmaps will have no buffer
	 * of compressed data. We cannot do any better than just returning
	 * a pointer to the original 'tile'.
	 */
	return fz_keep_pixmap(ctx, image->tile); /* That's all we can give you! */
}

static void
update_ctm_for_subarea(fz_matrix *ctm, const fz_irect *subarea, int w, int h)
{
	fz_matrix m;

	if (ctm == NULL || (subarea->x0 == 0 && subarea->y0 == 0 && subarea->x1 == w && subarea->y1 == h))
		return;

	m.a = (float) (subarea->x1 - subarea->x0) / w;
	m.b = 0;
	m.c = 0;
	m.d = (float) (subarea->y1 - subarea->y0) / h;
	m.e = (float) subarea->x0 / w;
	m.f = (float) subarea->y0 / h;
	*ctm = fz_concat(m, *ctm);
}

void fz_default_image_decode(void *arg, int w, int h, int l2factor, fz_irect *subarea)
{
	(void)arg;

	if ((subarea->x1-subarea->x0)*(subarea->y1-subarea->y0) >= (w*h/10)*9)
	{
		/* Either no subarea specified, or a subarea 90% or more of the
		 * whole area specified. Use the whole image. */
		subarea->x0 = 0;
		subarea->y0 = 0;
		subarea->x1 = w;
		subarea->y1 = h;
	}
	else
	{
		/* Clip to the edges if they are within 1% */
		if (subarea->x0 <= w/100)
			subarea->x0 = 0;
		if (subarea->y0 <= h/100)
			subarea->y0 = 0;
		if (subarea->x1 >= w*99/100)
			subarea->x1 = w;
		if (subarea->y1 >= h*99/100)
			subarea->y1 = h;
	}
}

static fz_pixmap *
fz_find_image_tile(fz_context *ctx, fz_image *image, fz_image_key *key, fz_matrix *ctm)
{
	fz_pixmap *tile;
	do
	{
		tile = fz_find_item(ctx, fz_drop_pixmap_imp, key, &fz_image_store_type);
		if (tile)
		{
			update_ctm_for_subarea(ctm, &key->rect, image->w, image->h);
			return tile;
		}
		key->l2factor--;
	}
	while (key->l2factor >= 0);
	return NULL;
}

fz_pixmap *
fz_get_pixmap_from_image(fz_context *ctx, fz_image *image, const fz_irect *subarea, fz_matrix *ctm, int *dw, int *dh)
{
	fz_pixmap *tile;
	int l2factor, l2factor_remaining;
	fz_image_key key;
	fz_image_key *keyp = NULL;
	int w;
	int h;

	fz_var(keyp);

	if (!image)
		return NULL;

	/* Figure out the extent. */
	if (ctm)
	{
		w = sqrtf(ctm->a * ctm->a + ctm->b * ctm->b);
		h = sqrtf(ctm->c * ctm->c + ctm->d * ctm->d);
	}
	else
	{
		w = image->w;
		h = image->h;
	}

	if (image->scalable)
	{
		/* If the image is scalable, we always want to re-render and never cache. */
		fz_irect subarea_copy;
		if (subarea)
			subarea_copy = *subarea;
		l2factor_remaining = 0;
		if (dw) *dw = w;
		if (dh) *dh = h;
		return image->get_pixmap(ctx, image, subarea ? &subarea_copy : NULL, image->w, image->h, &l2factor_remaining);
	}

	/* Clamp requested image size, since we never want to magnify images here. */
	if (w > image->w)
		w = image->w;
	if (h > image->h)
		h = image->h;

	if (image->decoded)
	{
		/* If the image is already decoded, then we can't offer a subarea,
		 * or l2factor, and we don't want to cache. */
		l2factor_remaining = 0;
		if (dw) *dw = w;
		if (dh) *dh = h;
		return image->get_pixmap(ctx, image, NULL, image->w, image->h, &l2factor_remaining);
	}

	/* What is our ideal factor? We search for the largest factor where
	 * we can subdivide and stay larger than the required size. We add
	 * a fudge factor of +2 here to allow for the possibility of
	 * expansion due to grid fitting. */
	l2factor = 0;
	if (w > 0 && h > 0)
	{
		while (image->w>>(l2factor+1) >= w+2 && image->h>>(l2factor+1) >= h+2 && l2factor < 6)
			l2factor++;
	}

	/* First, look through the store for existing tiles */
	if (subarea)
	{
		fz_compute_image_key(ctx, image, ctm, &key, subarea, l2factor, &w, &h, dw, dh);
		tile = fz_find_image_tile(ctx, image, &key, ctm);
		if (tile)
			return tile;
	}

	/* No subarea given, or no tile for subarea found; try entire image */
	fz_compute_image_key(ctx, image, ctm, &key, NULL, l2factor, &w, &h, dw, dh);
	tile = fz_find_image_tile(ctx, image, &key, ctm);
	if (tile)
		return tile;

	/* Neither subarea nor full image tile found; prepare the subarea key again */
	if (subarea)
		fz_compute_image_key(ctx, image, ctm, &key, subarea, l2factor, &w, &h, dw, dh);

	/* We'll have to decode the image; request the correct amount of downscaling. */
	l2factor_remaining = l2factor;
	tile = image->get_pixmap(ctx, image, &key.rect, w, h, &l2factor_remaining);

	/* Update the ctm to allow for subareas. */
	update_ctm_for_subarea(ctm, &key.rect, image->w, image->h);

	/* l2factor_remaining is updated to the amount of subscaling left to do */
	assert(l2factor_remaining >= 0 && l2factor_remaining <= 6);
	if (l2factor_remaining)
	{
		fz_try(ctx)
			fz_subsample_pixmap(ctx, tile, l2factor_remaining);
		fz_catch(ctx)
		{
			fz_drop_pixmap(ctx, tile);
			fz_rethrow(ctx);
		}
	}

	fz_try(ctx)
	{
		fz_pixmap *existing_tile;

		/* Now we try to cache the pixmap. Any failure here will just result
		 * in us not caching. */
		keyp = fz_malloc_struct(ctx, fz_image_key);
		keyp->refs = 1;
		keyp->image = fz_keep_image_store_key(ctx, image);
		keyp->l2factor = l2factor;
		keyp->rect = key.rect;

		existing_tile = fz_store_item(ctx, keyp, tile, fz_pixmap_size(ctx, tile), &fz_image_store_type);
		if (existing_tile)
		{
			/* We already have a tile. This must have been produced by a
			 * racing thread. We'll throw away ours and use that one. */
			fz_drop_pixmap(ctx, tile);
			tile = existing_tile;
		}
	}
	fz_always(ctx)
	{
		fz_drop_image_key(ctx, keyp);
	}
	fz_catch(ctx)
	{
		/* Do nothing */
		fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
		fz_report_error(ctx);
	}

	return tile;
}

fz_pixmap *
fz_get_unscaled_pixmap_from_image(fz_context *ctx, fz_image *image)
{
	return fz_get_pixmap_from_image(ctx, image, NULL /*subarea*/, NULL /*ctm*/, NULL /*dw*/, NULL /*dh*/);
}

static size_t
pixmap_image_get_size(fz_context *ctx, fz_image *image)
{
	fz_pixmap_image *im = (fz_pixmap_image *)image;

	if (image == NULL)
		return 0;

	return sizeof(fz_pixmap_image) + fz_pixmap_size(ctx, im->tile);
}

size_t fz_image_size(fz_context *ctx, fz_image *im)
{
	if (im == NULL)
		return 0;

	return im->get_size(ctx, im);
}

fz_image *
fz_new_image_from_pixmap(fz_context *ctx, fz_pixmap *pixmap, fz_image *mask)
{
	fz_pixmap_image *image;

	image = fz_new_derived_image(ctx, pixmap->w, pixmap->h, 8, pixmap->colorspace,
				pixmap->xres, pixmap->yres, 0, 0,
				NULL, NULL, mask, fz_pixmap_image,
				pixmap_image_get_pixmap,
				pixmap_image_get_size,
				drop_pixmap_image);
	image->tile = fz_keep_pixmap(ctx, pixmap);
	image->super.decoded = 1;

	return &image->super;
}

fz_image *
fz_new_image_of_size(fz_context *ctx, int w, int h, int bpc, fz_colorspace *colorspace,
		int xres, int yres, int interpolate, int imagemask, float *decode,
		int *colorkey, fz_image *mask, size_t size,
		fz_image_get_pixmap_fn *get_pixmap,
		fz_image_get_size_fn *get_size,
		fz_drop_image_fn *drop)
{
	fz_image *image;
	int i;

	assert(mask == NULL || mask->mask == NULL);
	assert(size >= sizeof(fz_image));

	image = Memento_label(fz_calloc(ctx, 1, size), "fz_image");
	FZ_INIT_KEY_STORABLE(image, 1, fz_drop_image_imp);
	image->drop_image = drop;
	image->get_pixmap = get_pixmap;
	image->get_size = get_size;
	image->w = w;
	image->h = h;
	image->xres = xres;
	image->yres = yres;
	image->bpc = bpc;
	image->n = (colorspace ? fz_colorspace_n(ctx, colorspace) : 1);
	image->colorspace = fz_keep_colorspace(ctx, colorspace);
	image->interpolate = interpolate;
	image->imagemask = imagemask;
	image->use_colorkey = (colorkey != NULL);
	if (colorkey)
		memcpy(image->colorkey, colorkey, sizeof(int)*image->n*2);
	image->use_decode = 0;
	if (decode)
	{
		memcpy(image->decode, decode, sizeof(float)*image->n*2);
	}
	else
	{
		float maxval = fz_colorspace_is_indexed(ctx, colorspace) ? (1 << bpc) - 1 : 1;
		for (i = 0; i < image->n; i++)
		{
			image->decode[2*i] = 0;
			image->decode[2*i+1] = maxval;
		}
	}
	/* ICC spaces have the default decode arrays pickled into them.
	 * For most spaces this is fine, because [ 0 1 0 1 0 1 ] is
	 * idempotent. For Lab, however, we need to adjust it. */
	if (fz_colorspace_is_lab_icc(ctx, colorspace))
	{
		/* Undo the default decode array of [0 100 -128 127 -128 127] */
		image->decode[0] = image->decode[0]/100.0f;
		image->decode[1] = image->decode[1]/100.0f;
		image->decode[2] = (image->decode[2]+128)/255.0f;
		image->decode[3] = (image->decode[3]+128)/255.0f;
		image->decode[4] = (image->decode[4]+128)/255.0f;
		image->decode[5] = (image->decode[5]+128)/255.0f;
	}
	for (i = 0; i < image->n; i++)
	{
		if (image->decode[i * 2] != 0 || image->decode[i * 2 + 1] != 1)
			break;
	}
	if (i != image->n)
		image->use_decode = 1;
	image->mask = fz_keep_image(ctx, mask);

	return image;
}

static size_t
compressed_image_get_size(fz_context *ctx, fz_image *image)
{
	fz_compressed_image *im = (fz_compressed_image *)image;

	if (image == NULL)
		return 0;

	return sizeof(fz_pixmap_image) + (im->buffer && im->buffer->buffer ? im->buffer->buffer->cap : 0);
}

fz_image *
fz_new_image_from_compressed_buffer(fz_context *ctx, int w, int h,
	int bpc, fz_colorspace *colorspace,
	int xres, int yres, int interpolate, int imagemask, float *decode,
	int *colorkey, fz_compressed_buffer *buffer, fz_image *mask)
{
	fz_compressed_image *image;

	fz_try(ctx)
	{
		image = fz_new_derived_image(ctx, w, h, bpc,
					colorspace, xres, yres,
					interpolate, imagemask, decode,
					colorkey, mask, fz_compressed_image,
					compressed_image_get_pixmap,
					compressed_image_get_size,
					drop_compressed_image);
		image->buffer = buffer;
	}
	fz_catch(ctx)
	{
		fz_drop_compressed_buffer(ctx, buffer);
		fz_rethrow(ctx);
	}

	return &image->super;
}

fz_compressed_buffer *fz_compressed_image_buffer(fz_context *ctx, fz_image *image)
{
	if (image == NULL || image->get_pixmap != compressed_image_get_pixmap)
		return NULL;
	return ((fz_compressed_image *)image)->buffer;
}

void fz_set_compressed_image_buffer(fz_context *ctx, fz_compressed_image *image, fz_compressed_buffer *buf)
{
	assert(image != NULL && image->super.get_pixmap == compressed_image_get_pixmap);
	((fz_compressed_image *)image)->buffer = buf; /* Note: compressed buffers are not reference counted */
}

fz_pixmap *fz_pixmap_image_tile(fz_context *ctx, fz_pixmap_image *image)
{
	if (image == NULL || image->super.get_pixmap != pixmap_image_get_pixmap)
		return NULL;
	return ((fz_pixmap_image *)image)->tile;
}

void fz_set_pixmap_image_tile(fz_context *ctx, fz_pixmap_image *image, fz_pixmap *pix)
{
	assert(image != NULL && image->super.get_pixmap == pixmap_image_get_pixmap);
	((fz_pixmap_image *)image)->tile = pix;
}

const char *
fz_image_type_name(int type)
{
	switch (type)
	{
	default:
	case FZ_IMAGE_UNKNOWN: return "unknown";
	case FZ_IMAGE_RAW: return "raw";
	case FZ_IMAGE_FAX: return "fax";
	case FZ_IMAGE_FLATE: return "flate";
	case FZ_IMAGE_LZW: return "lzw";
	case FZ_IMAGE_RLD: return "rld";
	case FZ_IMAGE_BMP: return "bmp";
	case FZ_IMAGE_GIF: return "gif";
	case FZ_IMAGE_JBIG2: return "jbig2";
	case FZ_IMAGE_JPEG: return "jpeg";
	case FZ_IMAGE_JPX: return "jpx";
	case FZ_IMAGE_JXR: return "jxr";
	case FZ_IMAGE_PNG: return "png";
	case FZ_IMAGE_PNM: return "pnm";
	case FZ_IMAGE_TIFF: return "tiff";
	}
}

int
fz_lookup_image_type(const char *type)
{
	if (type == NULL) return FZ_IMAGE_UNKNOWN;
	if (!strcmp(type, "raw")) return FZ_IMAGE_RAW;
	if (!strcmp(type, "fax")) return FZ_IMAGE_FAX;
	if (!strcmp(type, "flate")) return FZ_IMAGE_FLATE;
	if (!strcmp(type, "lzw")) return FZ_IMAGE_LZW;
	if (!strcmp(type, "rld")) return FZ_IMAGE_RLD;
	if (!strcmp(type, "bmp")) return FZ_IMAGE_BMP;
	if (!strcmp(type, "gif")) return FZ_IMAGE_GIF;
	if (!strcmp(type, "jbig2")) return FZ_IMAGE_JBIG2;
	if (!strcmp(type, "jpeg")) return FZ_IMAGE_JPEG;
	if (!strcmp(type, "jpx")) return FZ_IMAGE_JPX;
	if (!strcmp(type, "jxr")) return FZ_IMAGE_JXR;
	if (!strcmp(type, "png")) return FZ_IMAGE_PNG;
	if (!strcmp(type, "pnm")) return FZ_IMAGE_PNM;
	if (!strcmp(type, "tiff")) return FZ_IMAGE_TIFF;
	return FZ_IMAGE_UNKNOWN;
}

int
fz_recognize_image_format(fz_context *ctx, unsigned char p[8])
{
	if (p[0] == 'P' && p[1] >= '1' && p[1] <= '7')
		return FZ_IMAGE_PNM;
	if (p[0] == 'P' && (p[1] == 'F' || p[1] == 'f'))
		return FZ_IMAGE_PNM;
	if (p[0] == 0xff && p[1] == 0x4f)
		return FZ_IMAGE_JPX;
	if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x0c &&
			p[4] == 0x6a && p[5] == 0x50 && p[6] == 0x20 && p[7] == 0x20)
		return FZ_IMAGE_JPX;
	if (p[0] == 0xff && p[1] == 0xd8)
		return FZ_IMAGE_JPEG;
	if (p[0] == 137 && p[1] == 80 && p[2] == 78 && p[3] == 71 &&
			p[4] == 13 && p[5] == 10 && p[6] == 26 && p[7] == 10)
		return FZ_IMAGE_PNG;
	if (p[0] == 'I' && p[1] == 'I' && p[2] == 0xBC)
		return FZ_IMAGE_JXR;
	if (p[0] == 'I' && p[1] == 'I' && p[2] == 42 && p[3] == 0)
		return FZ_IMAGE_TIFF;
	if (p[0] == 'M' && p[1] == 'M' && p[2] == 0 && p[3] == 42)
		return FZ_IMAGE_TIFF;
	if (p[0] == 'G' && p[1] == 'I' && p[2] == 'F')
		return FZ_IMAGE_GIF;
	if (p[0] == 'B' && p[1] == 'M')
		return FZ_IMAGE_BMP;
	if (p[0] == 'B' && p[1] == 'A')
		return FZ_IMAGE_BMP;
	if (p[0] == 0x97 && p[1] == 'J' && p[2] == 'B' && p[3] == '2' &&
		p[4] == '\r' && p[5] == '\n'  && p[6] == 0x1a && p[7] == '\n')
		return FZ_IMAGE_JBIG2;
	if (p[0] == '8' && p[1] == 'B' && p[2] == 'P' && p[3] == 'S')
		return FZ_IMAGE_PSD;
	return FZ_IMAGE_UNKNOWN;
}

fz_image *
fz_new_image_from_buffer(fz_context *ctx, fz_buffer *buffer)
{
	fz_compressed_buffer *bc;
	int w, h, xres, yres;
	fz_colorspace *cspace;
	size_t len = buffer->len;
	unsigned char *buf = buffer->data;
	fz_image *image = NULL;
	int type;
	int bpc;
	uint8_t orientation = 0;

	if (len < 8)
		fz_throw(ctx, FZ_ERROR_FORMAT, "unknown image file format");

	type = fz_recognize_image_format(ctx, buf);
	bpc = 8;
	switch (type)
	{
	case FZ_IMAGE_PNM:
		fz_load_pnm_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		break;
	case FZ_IMAGE_JPX:
		fz_load_jpx_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		break;
	case FZ_IMAGE_JPEG:
		fz_load_jpeg_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace, &orientation);
		break;
	case FZ_IMAGE_PNG:
		fz_load_png_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		break;
	case FZ_IMAGE_PSD:
		fz_load_psd_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		break;
	case FZ_IMAGE_JXR:
		fz_load_jxr_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		break;
	case FZ_IMAGE_TIFF:
		fz_load_tiff_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		break;
	case FZ_IMAGE_GIF:
		fz_load_gif_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		break;
	case FZ_IMAGE_BMP:
		fz_load_bmp_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		break;
	case FZ_IMAGE_JBIG2:
		fz_load_jbig2_info(ctx, buf, len, &w, &h, &xres, &yres, &cspace);
		bpc = 1;
		break;
	default:
		fz_throw(ctx, FZ_ERROR_FORMAT, "unknown image file format");
	}

	fz_try(ctx)
	{
		bc = fz_malloc_struct(ctx, fz_compressed_buffer);
		bc->buffer = fz_keep_buffer(ctx, buffer);
		bc->params.type = type;
		if (type == FZ_IMAGE_JPEG)
		{
			bc->params.u.jpeg.color_transform = -1;
			bc->params.u.jpeg.invert_cmyk = 1;
		}
		image = fz_new_image_from_compressed_buffer(ctx, w, h, bpc, cspace, xres, yres, 0, 0, NULL, NULL, bc, NULL);
		image->orientation = orientation;
	}
	fz_always(ctx)
		fz_drop_colorspace(ctx, cspace);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return image;
}

int
fz_compressed_image_type(fz_context *ctx, fz_image *image)
{
	fz_compressed_image *cim;

	if (image == NULL || image->drop_image != drop_compressed_image)
		return FZ_IMAGE_UNKNOWN;

	cim = (fz_compressed_image *)image;

	return cim->buffer->params.type;
}

fz_image *
fz_new_image_from_file(fz_context *ctx, const char *path)
{
	fz_buffer *buffer;
	fz_image *image = NULL;

	buffer = fz_read_file(ctx, path);
	fz_try(ctx)
		image = fz_new_image_from_buffer(ctx, buffer);
	fz_always(ctx)
		fz_drop_buffer(ctx, buffer);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return image;
}

void
fz_image_resolution(fz_image *image, int *xres, int *yres)
{
	*xres = image->xres;
	*yres = image->yres;
	if (*xres < 0 || *yres < 0 || (*xres == 0 && *yres == 0))
	{
		/* If neither xres or yres is sane, pick a sane value */
		*xres = SANE_DPI; *yres = SANE_DPI;
	}
	else if (*xres == 0)
	{
		*xres = *yres;
	}
	else if (*yres == 0)
	{
		*yres = *xres;
	}

	/* Scale xres and yres up until we get believable values */
	if (*xres < SANE_DPI || *yres < SANE_DPI || *xres > INSANE_DPI || *yres > INSANE_DPI)
	{
		if (*xres < *yres)
		{
			*yres = *yres * SANE_DPI / *xres;
			*xres = SANE_DPI;
		}
		else
		{
			*xres = *xres * SANE_DPI / *yres;
			*yres = SANE_DPI;
		}

		if (*xres == *yres || *xres < SANE_DPI || *yres < SANE_DPI || *xres > INSANE_DPI || *yres > INSANE_DPI)
		{
			*xres = SANE_DPI;
			*yres = SANE_DPI;
		}
	}
}

uint8_t fz_image_orientation(fz_context *ctx, fz_image *image)
{
	return image ? image->orientation : 0;
}

fz_matrix fz_image_orientation_matrix(fz_context *ctx, fz_image *image)
{
	fz_matrix m;

	switch (image ? image->orientation : 0)
	{
	case 0:
	case 1: /* 0 degree rotation */
		m.a =  1; m.b =  0;
		m.c =  0; m.d =  1;
		m.e =  0; m.f =  0;
		break;
	case 2: /* 90 degree ccw */
		m.a =  0; m.b = -1;
		m.c =  1; m.d =  0;
		m.e =  0; m.f =  1;
		break;
	case 3: /* 180 degree ccw */
		m.a = -1; m.b =  0;
		m.c =  0; m.d = -1;
		m.e =  1; m.f =  1;
		break;
	case 4: /* 270 degree ccw */
		m.a =  0; m.b =  1;
		m.c = -1; m.d =  0;
		m.e =  1; m.f =  0;
		break;
	case 5: /* flip on X */
		m.a = -1; m.b = 0;
		m.c =  0; m.d = 1;
		m.e =  1; m.f = 0;
		break;
	case 6: /* flip on X, then rotate ccw by 90 degrees */
		m.a =  0; m.b =  1;
		m.c =  1; m.d =  0;
		m.e =  0; m.f =  0;
		break;
	case 7: /* flip on X, then rotate ccw by 180 degrees */
		m.a =  1; m.b =  0;
		m.c =  0; m.d = -1;
		m.e =  0; m.f =  1;
		break;
	case 8: /* flip on X, then rotate ccw by 270 degrees */
		m.a =  0; m.b = -1;
		m.c = -1; m.d =  0;
		m.e =  1; m.f =  1;
		break;
	}

	return m;
}

typedef struct fz_display_list_image_s
{
	fz_image super;
	fz_matrix transform;
	fz_display_list *list;
} fz_display_list_image;

static fz_pixmap *
display_list_image_get_pixmap(fz_context *ctx, fz_image *image_, fz_irect *subarea, int w, int h, int *l2factor)
{
	fz_display_list_image *image = (fz_display_list_image *)image_;
	fz_matrix ctm;
	fz_device *dev;
	fz_pixmap *pix;

	fz_var(dev);

	if (subarea)
	{
		/* So, the whole image should be scaled to w * h, but we only want the
		 * given subarea of it. */
		int l = (subarea->x0 * w) / image->super.w;
		int t = (subarea->y0 * h) / image->super.h;
		int r = (subarea->x1 * w + image->super.w - 1) / image->super.w;
		int b = (subarea->y1 * h + image->super.h - 1) / image->super.h;

		pix = fz_new_pixmap(ctx, image->super.colorspace, r-l, b-t, NULL, 0);
		pix->x = l;
		pix->y = t;
	}
	else
	{
		pix = fz_new_pixmap(ctx, image->super.colorspace, w, h, NULL, 0);
	}

	/* If we render the display list into pix with the image matrix, we'll get a unit
	 * square result. Therefore scale by w, h. */
	ctm = fz_pre_scale(image->transform, w, h);

	fz_clear_pixmap(ctx, pix); /* clear to transparent */
	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, ctm, pix);
		fz_run_display_list(ctx, image->list, dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
		fz_drop_device(ctx, dev);
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	/* Never do more subsampling, cos we've already given them the right size */
	if (l2factor)
		*l2factor = 0;

	return pix;
}

static void drop_display_list_image(fz_context *ctx, fz_image *image_)
{
	fz_display_list_image *image = (fz_display_list_image *)image_;

	if (image == NULL)
		return;
	fz_drop_display_list(ctx, image->list);
}

static size_t
display_list_image_get_size(fz_context *ctx, fz_image *image_)
{
	fz_display_list_image *image = (fz_display_list_image *)image_;

	if (image == NULL)
		return 0;

	return sizeof(fz_display_list_image) + 4096; /* FIXME */
}

fz_image *fz_new_image_from_display_list(fz_context *ctx, float w, float h, fz_display_list *list)
{
	fz_display_list_image *image;
	int iw, ih;

	iw = w * SCALABLE_IMAGE_DPI / 72;
	ih = h * SCALABLE_IMAGE_DPI / 72;

	image = fz_new_derived_image(ctx, iw, ih, 8, fz_device_rgb(ctx),
				SCALABLE_IMAGE_DPI, SCALABLE_IMAGE_DPI, 0, 0,
				NULL, NULL, NULL, fz_display_list_image,
				display_list_image_get_pixmap,
				display_list_image_get_size,
				drop_display_list_image);
	image->super.scalable = 1;
	image->transform = fz_scale(1 / w, 1 / h);
	image->list = fz_keep_display_list(ctx, list);

	return &image->super;
}
