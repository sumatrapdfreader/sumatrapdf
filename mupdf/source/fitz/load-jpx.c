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

#include "pixmap-imp.h"

#include <assert.h>
#include <string.h>

#if FZ_ENABLE_JPX

static void
jpx_ycc_to_rgb(fz_context *ctx, fz_pixmap *pix, int cbsign, int crsign)
{
	int w = pix->w;
	int h = pix->h;
	int stride = pix->stride;
	int x, y;

	for (y = 0; y < h; y++)
	{
		unsigned char * row = &pix->samples[stride * y];
		for (x = 0; x < w; x++)
		{
			int ycc[3];
			ycc[0] = row[x * 3 + 0];
			ycc[1] = row[x * 3 + 1];
			ycc[2] = row[x * 3 + 2];

			/* consciously skip Y */
			if (cbsign)
				ycc[1] -= 128;
			if (crsign)
				ycc[2] -= 128;

			row[x * 3 + 0] = fz_clampi(ycc[0] + 1.402f * ycc[2], 0, 255);
			row[x * 3 + 1] = fz_clampi(ycc[0] - 0.34413f * ycc[1] - 0.71414f * ycc[2], 0, 255);
			row[x * 3 + 2] = fz_clampi(ycc[0] + 1.772f * ycc[1], 0, 255);
		}
	}
}

#include <openjpeg.h>

typedef struct
{
	int width;
	int height;
	fz_colorspace *cs;
	int xres;
	int yres;
} fz_jpxd;

typedef struct
{
	const unsigned char *data;
	OPJ_SIZE_T size;
	OPJ_SIZE_T pos;
} stream_block;

/* OpenJPEG does not provide a safe mechanism to intercept
 * allocations. In the latest version all allocations go
 * though opj_malloc etc, but no context is passed around.
 *
 * In order to ensure that allocations throughout mupdf
 * are done consistently, we implement opj_malloc etc as
 * functions that call down to fz_malloc etc. These
 * require context variables, so we lock and unlock around
 * calls to openjpeg. Any attempt to call through
 * without setting these will be detected.
 *
 * It is therefore vital that any fz_lock/fz_unlock
 * handlers are shared between all the fz_contexts in
 * use at a time.
 */

/* Potentially we can write different versions
 * of get_context and set_context for different
 * threading systems.
 */

static fz_context *opj_secret = NULL;

static void set_opj_context(fz_context *ctx)
{
	opj_secret = ctx;
}

static fz_context *get_opj_context(void)
{
	return opj_secret;
}

void opj_lock(fz_context *ctx)
{
	fz_ft_lock(ctx);

	set_opj_context(ctx);
}

void opj_unlock(fz_context *ctx)
{
	set_opj_context(NULL);

	fz_ft_unlock(ctx);
}

void *opj_malloc(size_t size)
{
	fz_context *ctx = get_opj_context();

	assert(ctx != NULL);

	return Memento_label(fz_malloc_no_throw(ctx, size), "opj_malloc");
}

void *opj_calloc(size_t n, size_t size)
{
	fz_context *ctx = get_opj_context();

	assert(ctx != NULL);

	return fz_calloc_no_throw(ctx, n, size);
}

void *opj_realloc(void *ptr, size_t size)
{
	fz_context *ctx = get_opj_context();

	assert(ctx != NULL);

	return fz_realloc_no_throw(ctx, ptr, size);
}

void opj_free(void *ptr)
{
	fz_context *ctx = get_opj_context();

	assert(ctx != NULL);

	fz_free(ctx, ptr);
}

static void * opj_aligned_malloc_n(size_t alignment, size_t size)
{
	uint8_t *ptr;
	size_t off;

	if (size == 0)
		return NULL;

	size += alignment + sizeof(uint8_t);
	ptr = opj_malloc(size);
	if (ptr == NULL)
		return NULL;
	off = alignment-(((int)(intptr_t)ptr) & (alignment - 1));
	ptr[off-1] = (uint8_t)off;
	return ptr + off;
}

void * opj_aligned_malloc(size_t size)
{
	return opj_aligned_malloc_n(16, size);
}

void * opj_aligned_32_malloc(size_t size)
{
	return opj_aligned_malloc_n(32, size);
}

void opj_aligned_free(void* ptr_)
{
	uint8_t *ptr = (uint8_t *)ptr_;
	uint8_t off;
	if (ptr == NULL)
		return;

	off = ptr[-1];
	opj_free((void *)(((unsigned char *)ptr) - off));
}

#if 0
/* UNUSED currently, and moderately tricky, so deferred until required */
void * opj_aligned_realloc(void *ptr, size_t size)
{
	return opj_realloc(ptr, size);
}
#endif

static void fz_opj_error_callback(const char *msg, void *client_data)
{
	fz_context *ctx = (fz_context *)client_data;
	char buf[200];
	size_t n;
	fz_strlcpy(buf, msg, sizeof buf);
	n = strlen(buf);
	if (buf[n-1] == '\n')
		buf[n-1] = 0;
	fz_warn(ctx, "openjpeg error: %s", buf);
}

static void fz_opj_warning_callback(const char *msg, void *client_data)
{
	fz_context *ctx = (fz_context *)client_data;
	char buf[200];
	size_t n;
	fz_strlcpy(buf, msg, sizeof buf);
	n = strlen(buf);
	if (buf[n-1] == '\n')
		buf[n-1] = 0;
	fz_warn(ctx, "openjpeg warning: %s", buf);
}

static void fz_opj_info_callback(const char *msg, void *client_data)
{
	/* fz_warn("openjpeg info: %s", msg); */
}

static OPJ_SIZE_T fz_opj_stream_read(void * p_buffer, OPJ_SIZE_T p_nb_bytes, void * p_user_data)
{
	stream_block *sb = (stream_block *)p_user_data;
	OPJ_SIZE_T len;

	len = sb->size - sb->pos;
	if (len == 0)
		return (OPJ_SIZE_T)-1; /* End of file! */
	if (len > p_nb_bytes)
		len = p_nb_bytes;
	memcpy(p_buffer, sb->data + sb->pos, len);
	sb->pos += len;
	return len;
}

static OPJ_OFF_T fz_opj_stream_skip(OPJ_OFF_T skip, void * p_user_data)
{
	stream_block *sb = (stream_block *)p_user_data;

	if (skip > (OPJ_OFF_T)(sb->size - sb->pos))
		skip = (OPJ_OFF_T)(sb->size - sb->pos);
	sb->pos += skip;
	return sb->pos;
}

static OPJ_BOOL fz_opj_stream_seek(OPJ_OFF_T seek_pos, void * p_user_data)
{
	stream_block *sb = (stream_block *)p_user_data;

	if (seek_pos > (OPJ_OFF_T)sb->size)
		return OPJ_FALSE;
	sb->pos = seek_pos;
	return OPJ_TRUE;
}

static int32_t
safe_mul32(fz_context *ctx, int32_t a, int32_t b)
{
	int64_t res = ((int64_t)a) * b;
	int32_t res32 = (int32_t)res;

	if ((res32) != res)
		fz_throw(ctx, FZ_ERROR_LIMIT, "Overflow while decoding jpx");
	return res32;
}

static int32_t
safe_mla32(fz_context *ctx, int32_t a, int32_t b, int32_t c)
{
	int64_t res = ((int64_t)a) * b + c;
	int32_t res32 = (int32_t)res;

	if ((res32) != res)
		fz_throw(ctx, FZ_ERROR_LIMIT, "Overflow while decoding jpx");
	return res32;
}

static inline void
template_copy_comp(unsigned char *dst0, int w, int h, int stride, const OPJ_INT32 *src, int32_t ox, int32_t oy, OPJ_UINT32 cdx, OPJ_UINT32 cdy, OPJ_UINT32 cw, OPJ_UINT32 ch, OPJ_UINT32 sgnd, OPJ_UINT32 prec, int comps)
{
	int x, y;

	for (y = ch; oy + cdy <= 0 && y > 0; y--)
	{
		oy += cdy;
		dst0 += cdy * stride;
		src += cw;
	}
	for (; y > 0; y--)
	{
		int32_t dymin = oy;
		int32_t dywid = cdy;
		unsigned char *dst1 = dst0 + ox * comps;
		int32_t oox = ox;
		const OPJ_INT32 *src0 = src;

		if (dymin < 0)
			dywid += dymin, dst1 -= dymin * stride, dymin = 0;
		if (dymin >= h)
			break;
		if (dymin + dywid > h)
			dywid = h - dymin;

		for (x = cw; oox + cdx <= 0 && x > 0; x--)
		{
			oox += cdx;
			dst1 += cdx * comps;
			src0++;
		}
		for (; x > 0; x--)
		{
			OPJ_INT32 v;
			int32_t xx, yy;
			int32_t dxmin = oox;
			int32_t dxwid = cdx;
			unsigned char *dst2;

			v = *src0++;

			if (sgnd)
				v = v + (1 << (prec - 1));
			if (prec > 8)
				v = v >> (prec - 8);
			else if (prec < 8)
				v = v << (8 - prec);

			if (dxmin < 0)
				dxwid += dxmin, dst1 -= dxmin * comps, dxmin = 0;
			if (dxmin >= w)
				break;
			if (dxmin + dxwid > w)
				dxwid = w - dxmin;

			dst2 = dst1;
			for (yy = dywid; yy > 0; yy--)
			{
				unsigned char *dst3 = dst2;
				for (xx = dxwid; xx > 0; xx--)
				{
					*dst3 = v;
					dst3 += comps;
				}
				dst2 += stride;
			}
			dst1 += comps * cdx;
			oox += cdx;
		}
		dst0 += stride * cdy;
		src += cw;
		oy += cdy;
	}
}

static void
copy_jpx_to_pixmap(fz_context *ctx, fz_pixmap *img, opj_image_t *jpx)
{
	unsigned char *dst;
	int stride, comps;
	int w = img->w;
	int h = img->h;
	int k;

	stride = fz_pixmap_stride(ctx, img);
	comps = fz_pixmap_components(ctx, img);
	dst = fz_pixmap_samples(ctx, img);

	for (k = 0; k < comps; k++)
	{
		opj_image_comp_t *comp = &(jpx->comps[k]);
		OPJ_UINT32 cdx = comp->dx;
		OPJ_UINT32 cdy = comp->dy;
		OPJ_UINT32 cw = comp->w;
		OPJ_UINT32 ch = comp->h;
		int32_t oy = safe_mul32(ctx, comp->y0, cdy) - jpx->y0;
		int32_t ox = safe_mul32(ctx, comp->x0, cdx) - jpx->x0;
		unsigned char *dst0 = dst + oy * stride;
		int prec = comp->prec;
		int sgnd = comp->sgnd;

		if (comp->data == NULL)
			fz_throw(ctx, FZ_ERROR_FORMAT, "No data for JP2 image component %d", k);

		if (fz_colorspace_is_indexed(ctx, img->colorspace))
		{
			prec = 8; /* Don't do any scaling! */
			sgnd = 0;
		}

		/* Check that none of the following will overflow. */
		(void)safe_mla32(ctx, ch, cdy, oy);
		(void)safe_mla32(ctx, cw, cdx, ox);

		if (cdx == 1 && cdy == 1)
			template_copy_comp(dst0, w, h, stride, comp->data, ox, oy, 1 /*cdx*/, 1 /*cdy*/, cw, ch, sgnd, prec, comps);
		else
			template_copy_comp(dst0, w, h, stride, comp->data, ox, oy, cdx, cdy, cw, ch, sgnd, prec, comps);
		dst++;
	}
}

static fz_pixmap *
jpx_read_image(fz_context *ctx, fz_jpxd *state, const unsigned char *data, size_t size, fz_colorspace *defcs, int onlymeta)
{
	fz_pixmap *img = NULL;
	opj_dparameters_t params;
	opj_codec_t *codec;
	opj_image_t *jpx;
	opj_stream_t *stream;
	OPJ_CODEC_FORMAT format;
	int a, n, k;
	int w, h;
	stream_block sb;
	OPJ_UINT32 i;

	fz_var(img);

	if (size < 2)
		fz_throw(ctx, FZ_ERROR_FORMAT, "not enough data to determine image format");

	/* Check for SOC marker -- if found we have a bare J2K stream */
	if (data[0] == 0xFF && data[1] == 0x4F)
		format = OPJ_CODEC_J2K;
	else
		format = OPJ_CODEC_JP2;

	opj_set_default_decoder_parameters(&params);
	if (fz_colorspace_is_indexed(ctx, defcs))
		params.flags |= OPJ_DPARAMETERS_IGNORE_PCLR_CMAP_CDEF_FLAG;

	codec = opj_create_decompress(format);
	opj_set_info_handler(codec, fz_opj_info_callback, ctx);
	opj_set_warning_handler(codec, fz_opj_warning_callback, ctx);
	opj_set_error_handler(codec, fz_opj_error_callback, ctx);
	if (!opj_setup_decoder(codec, &params))
	{
		opj_destroy_codec(codec);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "j2k decode failed");
	}

	stream = opj_stream_default_create(OPJ_TRUE);
	sb.data = data;
	sb.pos = 0;
	sb.size = size;

	opj_stream_set_read_function(stream, fz_opj_stream_read);
	opj_stream_set_skip_function(stream, fz_opj_stream_skip);
	opj_stream_set_seek_function(stream, fz_opj_stream_seek);
	opj_stream_set_user_data(stream, &sb, NULL);
	/* Set the length to avoid an assert */
	opj_stream_set_user_data_length(stream, size);

	if (!opj_read_header(stream, codec, &jpx))
	{
		opj_stream_destroy(stream);
		opj_destroy_codec(codec);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to read JPX header");
	}

	if (!opj_decode(codec, stream, jpx))
	{
		opj_stream_destroy(stream);
		opj_destroy_codec(codec);
		opj_image_destroy(jpx);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Failed to decode JPX image");
	}

	opj_stream_destroy(stream);
	opj_destroy_codec(codec);

	/* jpx should never be NULL here, but check anyway */
	if (!jpx)
		fz_throw(ctx, FZ_ERROR_LIBRARY, "opj_decode failed");

	/* Count number of alpha and color channels */
	n = a = 0;
	for (i = 0; i < jpx->numcomps; ++i)
	{
		if (jpx->comps[i].alpha)
			++a;
		else
			++n;
	}

	for (k = 1; k < n + a; k++)
	{
		if (!jpx->comps[k].data)
		{
			opj_image_destroy(jpx);
			fz_throw(ctx, FZ_ERROR_FORMAT, "image components are missing data");
		}
	}

	w = state->width = jpx->x1 - jpx->x0;
	h = state->height = jpx->y1 - jpx->y0;
	state->xres = 72; /* openjpeg does not read the JPEG 2000 resc box */
	state->yres = 72; /* openjpeg does not read the JPEG 2000 resc box */

	if (w < 0 || h < 0)
	{
		opj_image_destroy(jpx);
		fz_throw(ctx, FZ_ERROR_LIMIT, "Unbelievable size for jpx");
	}

	state->cs = NULL;

	if (defcs)
	{
		if (defcs->n == n)
			state->cs = fz_keep_colorspace(ctx, defcs);
		else
			fz_warn(ctx, "jpx file and dict colorspace do not match");
	}

#if FZ_ENABLE_ICC
	if (!state->cs && jpx->icc_profile_buf && jpx->icc_profile_len > 0)
	{
		fz_buffer *cbuf = NULL;
		fz_var(cbuf);

		fz_try(ctx)
		{
			cbuf = fz_new_buffer_from_copied_data(ctx, jpx->icc_profile_buf, jpx->icc_profile_len);
			state->cs = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_NONE, 0, NULL, cbuf);
		}
		fz_always(ctx)
			fz_drop_buffer(ctx, cbuf);
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
			fz_warn(ctx, "ignoring embedded ICC profile in JPX");
		}

		if (state->cs && state->cs->n != n)
		{
			fz_warn(ctx, "invalid number of components in ICC profile, ignoring ICC profile in JPX");
			fz_drop_colorspace(ctx, state->cs);
			state->cs = NULL;
		}
	}
#endif

	if (!state->cs)
	{
		switch (n)
		{
		case 1: state->cs = fz_keep_colorspace(ctx, fz_device_gray(ctx)); break;
		case 3: state->cs = fz_keep_colorspace(ctx, fz_device_rgb(ctx)); break;
		case 4: state->cs = fz_keep_colorspace(ctx, fz_device_cmyk(ctx)); break;
		default:
			{
				opj_image_destroy(jpx);
				fz_throw(ctx, FZ_ERROR_FORMAT, "unsupported number of components: %d", n);
			}
		}
	}

	if (onlymeta)
	{
		opj_image_destroy(jpx);
		return NULL;
	}

	fz_try(ctx)
	{
		a = !!a; /* ignore any superfluous alpha channels */
		img = fz_new_pixmap(ctx, state->cs, w, h, NULL, a);
		fz_clear_pixmap_with_value(ctx, img, 0);
		copy_jpx_to_pixmap(ctx, img, jpx);

		if (jpx->color_space == OPJ_CLRSPC_SYCC && n == 3 && a == 0)
			jpx_ycc_to_rgb(ctx, img, 1, 1);
		if (a)
			fz_premultiply_pixmap(ctx, img);
	}
	fz_always(ctx)
	{
		fz_drop_colorspace(ctx, state->cs);
		opj_image_destroy(jpx);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, img);
		fz_rethrow(ctx);
	}

	return img;
}

fz_pixmap *
fz_load_jpx(fz_context *ctx, const unsigned char *data, size_t size, fz_colorspace *defcs)
{
	fz_jpxd state = { 0 };
	fz_pixmap *pix = NULL;

	fz_try(ctx)
	{
		opj_lock(ctx);
		pix = jpx_read_image(ctx, &state, data, size, defcs, 0);
	}
	fz_always(ctx)
		opj_unlock(ctx);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return pix;
}

void
fz_load_jpx_info(fz_context *ctx, const unsigned char *data, size_t size, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_jpxd state = { 0 };

	fz_try(ctx)
	{
		opj_lock(ctx);
		jpx_read_image(ctx, &state, data, size, NULL, 1);
	}
	fz_always(ctx)
		opj_unlock(ctx);
	fz_catch(ctx)
		fz_rethrow(ctx);

	*cspacep = state.cs;
	*wp = state.width;
	*hp = state.height;
	*xresp = state.xres;
	*yresp = state.yres;
}

#else /* FZ_ENABLE_JPX */

fz_pixmap *
fz_load_jpx(fz_context *ctx, const unsigned char *data, size_t size, fz_colorspace *defcs)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JPX support disabled");
}

void
fz_load_jpx_info(fz_context *ctx, const unsigned char *data, size_t size, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JPX support disabled");
}

#endif
