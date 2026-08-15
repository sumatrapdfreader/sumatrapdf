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

#include <openjpeg.h>

#if FZ_ENABLE_JPX

static opj_image_t *
image_from_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	opj_image_cmptparm_t cmptparm[FZ_MAX_COLORS] = { 0 };
	OPJ_INT32 *data[FZ_MAX_COLORS];
	int i;
	opj_image_t *image;
	OPJ_COLOR_SPACE cs;

	if (pix->alpha || pix->s)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "No spots/alpha for JPX encode");

	if (fz_colorspace_is_cmyk(ctx, pix->colorspace))
		cs = OPJ_CLRSPC_CMYK;
	else if (fz_colorspace_is_rgb(ctx, pix->colorspace))
		cs = OPJ_CLRSPC_SRGB;
	else if (fz_colorspace_is_gray(ctx, pix->colorspace))
		cs = OPJ_CLRSPC_GRAY;
	else
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid colorspace for JPX encode");

	/* Create image */
	for (i = 0; i < pix->n; ++i)
	{
		cmptparm[i].prec = 8;
		cmptparm[i].sgnd = 0;
		cmptparm[i].dx = 1;
		cmptparm[i].dy = 1;
		cmptparm[i].w = (OPJ_UINT32)pix->w;
		cmptparm[i].h = (OPJ_UINT32)pix->h;
	}

	image = opj_image_create(pix->n, &cmptparm[0], cs);
	if (image == NULL)
		fz_throw(ctx, FZ_ERROR_LIBRARY, "OPJ image creation failed");

	image->x0 = 0;
	image->y0 = 0;
	image->x1 = pix->w;
	image->y1 = pix->h;

	for (i = 0; i < pix->n; ++i)
		data[i] = image->comps[i].data;

	{
		int w = pix->w;
		int stride = pix->stride;
		int n = pix->n;
		int x, y, k;
		unsigned char *s = pix->samples;
		for (y = pix->h; y > 0; y--)
		{
			unsigned char *s2 = s;
			s += stride;
			for (k = 0; k < n; k++)
			{
				unsigned char *s3 = s2++;
				OPJ_INT32 *d = data[k];
				data[k] += w;
				for (x = w; x > 0; x--)
				{
					*d++ = (*s3);
					s3 += n;
				}
			}
		}
	}

	return image;
}

typedef struct
{
	fz_context *ctx; /* Safe */
	fz_output *out;
} my_stream;

static void
close_stm(void *user_data)
{
	my_stream *stm = (my_stream *)user_data;

	/* Nothing to see here. */
	fz_close_output(stm->ctx, stm->out);
}

static OPJ_SIZE_T
write_stm(void *p_buffer, OPJ_SIZE_T p_nb_bytes, void *p_user_data)
{
	my_stream *stm = (my_stream *)p_user_data;

	fz_try(stm->ctx)
		fz_write_data(stm->ctx, stm->out, p_buffer, p_nb_bytes);
	fz_catch(stm->ctx)
		return (OPJ_SIZE_T)-1;

	return p_nb_bytes;
}

static OPJ_OFF_T skip_stm(OPJ_OFF_T p_nb_bytes, void *p_user_data)
{
	my_stream *stm = (my_stream *)p_user_data;

	(void)stm;

	return -1;
}

static OPJ_BOOL seek_stm(OPJ_OFF_T p_nb_bytes, void *p_user_data)
{
	my_stream *stm = (my_stream *)p_user_data;

	(void)stm;

	return 0;
}

static void
info_callback(const char *msg, void *client_data)
{
#if 0
	fz_context *ctx = (fz_context *)client_data;
	// strlen-1 to trim trailing newline
	fz_warn(ctx, "openjpeg info: %.*s", (int) strlen(msg)-1, msg);
#endif
}

static void
warning_callback(const char *msg, void *client_data)
{
	fz_context *ctx = (fz_context *)client_data;
	// strlen-1 to trim trailing newline
	fz_warn(ctx, "openjpeg warning: %.*s", (int) strlen(msg)-1, msg);
}

static void
error_callback(const char *msg, void *client_data)
{
	fz_context *ctx = (fz_context *)client_data;
	// strlen-1 to trim trailing newline
	fz_warn(ctx, "openjpeg error: %.*s", (int) strlen(msg)-1, msg);
}

void
fz_write_pixmap_as_jpx(fz_context *ctx, fz_output *out, fz_pixmap *pix, int q)
{
	opj_cparameters_t parameters;   /* compression parameters */

	opj_stream_t *l_stream = 00;
	opj_codec_t* l_codec = 00;
	opj_image_t *image = NULL;
	OPJ_BOOL bSuccess;

	my_stream stm;

	fz_var(image);

	fz_opj_lock(ctx);
	fz_try(ctx)
	{
		image = image_from_pixmap(ctx, pix);
		stm.ctx = ctx;
		stm.out = out;

		/* set encoding parameters to default values */
		opj_set_default_encoder_parameters(&parameters);

		/* Decide if MCT should be used */
		/* mct = 1 -> rgb data should be converted to ycc */
		parameters.tcp_mct = (pix->n >= 3) ? 1 : 0;

		parameters.irreversible = 1;

		/* JPEG-2000 codestream */
		l_codec = opj_create_compress(OPJ_CODEC_J2K);
		/* Use OPJ_CODEC_JP2 for JPEG 2000 compressed image data, but that requires seeking. */

		/* catch events using our callbacks and give a local context */
		opj_set_info_handler(l_codec, info_callback, ctx);
		opj_set_warning_handler(l_codec, warning_callback, ctx);
		opj_set_error_handler(l_codec, error_callback, ctx);

		/* We encode using tiles. */
		parameters.cp_tx0 = 0;
		parameters.cp_ty0 = 0;
		parameters.tile_size_on = OPJ_TRUE;
		parameters.cp_tdx = 256;
		parameters.cp_tdy = 256;

		/* Shrink the tile so it's not more than twice the width/height of the image. */
		while (parameters.cp_tdx>>1 >= pix->w)
			parameters.cp_tdx >>= 1;
		while (parameters.cp_tdy>>1 >= pix->h)
			parameters.cp_tdy >>= 1;

		/* The tile size must not be smaller than that given by numresolution. */
		if (parameters.cp_tdx < 1<<(parameters.numresolution-1))
			parameters.cp_tdx = 1<<(parameters.numresolution-1);
		if (parameters.cp_tdy < 1<<(parameters.numresolution-1))
			parameters.cp_tdy = 1<<(parameters.numresolution-1);

		/* FIXME: Calculate layers here? */
		/* My understanding of the suggestion that I've been given, is that we should pick
		 * layers to be the largest integer, such that (1<<layers) * tile_size >= w */
		{
			int layers = 0;
			while (pix->w>>(layers+1) >= parameters.cp_tdx &&
				pix->h>>(layers+1) >= parameters.cp_tdy)
				layers++;
			/* But putting layers into parameters.tcp_numlayers causes a crash... */
		}

		if (q == 100)
		{
			/* Lossless compression requested! */
		}
		else if (pix->w < 2*parameters.cp_tdx && pix->h < 2*parameters.cp_tdy)
		{
			/* We only compress lossily if the image is larger than the tilesize, otherwise work losslessly. */
		}
		else
		{
			/* 20:1 compression is reasonable */
			parameters.tcp_numlayers = 1;
			parameters.tcp_rates[0] = (100-q);
			parameters.cp_disto_alloc = 1;
		}

		if (! opj_setup_encoder(l_codec, &parameters, image))
		{
			opj_destroy_codec(l_codec);
			opj_image_destroy(image);
			fz_throw(ctx, FZ_ERROR_LIBRARY, "OpenJPEG encoder setup failed");
		}

		/* open a byte stream for writing and allocate memory for all tiles */
		l_stream = opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE, OPJ_FALSE);
		if (!l_stream)
		{
			opj_destroy_codec(l_codec);
			opj_image_destroy(image);
			fz_throw(ctx, FZ_ERROR_LIBRARY, "OpenJPEG encoder setup failed (stream creation)");
		}

		opj_stream_set_user_data(l_stream, &stm, close_stm);
		opj_stream_set_user_data_length(l_stream, 0);
		//opj_stream_set_read_function(l_stream, opj_read_from_file);
		opj_stream_set_write_function(l_stream, write_stm);
		opj_stream_set_skip_function(l_stream, skip_stm);
		opj_stream_set_seek_function(l_stream, seek_stm);

		/* encode the image */
		bSuccess = opj_start_compress(l_codec, image, l_stream);
		if (!bSuccess)
		{
			opj_destroy_codec(l_codec);
			opj_image_destroy(image);
			fz_throw(ctx, FZ_ERROR_LIBRARY, "OpenJPEG encode failed");
		}

		bSuccess = bSuccess && opj_encode(l_codec, l_stream);
		bSuccess = bSuccess && opj_end_compress(l_codec, l_stream);

		opj_stream_destroy(l_stream);

		/* free remaining compression structures */
		opj_destroy_codec(l_codec);

		/* free image data */
		opj_image_destroy(image);

		if (!bSuccess)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "Encoding failed");
	}
	fz_always(ctx)
		fz_opj_unlock(ctx);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_save_pixmap_as_jpx(fz_context *ctx, fz_pixmap *pixmap, const char *filename, int q)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, 0);
	fz_try(ctx)
	{
		fz_write_pixmap_as_jpx(ctx, out, pixmap, q);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static fz_buffer *
jpx_from_pixmap(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params, int quality, int drop)
{
	fz_buffer *buf = NULL;
	fz_output *out = NULL;

	fz_var(buf);
	fz_var(out);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_write_pixmap_as_jpx(ctx, out, pix, quality);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		if (drop)
			fz_drop_pixmap(ctx, pix);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
	return buf;
}

fz_buffer *
fz_new_buffer_from_image_as_jpx(fz_context *ctx, fz_image *image, fz_color_params color_params, int quality)
{
	fz_pixmap *pix = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
	return jpx_from_pixmap(ctx, pix, color_params, quality, 1);
}

fz_buffer *
fz_new_buffer_from_pixmap_as_jpx(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params, int quality)
{
	return jpx_from_pixmap(ctx, pix, color_params, quality, 0);
}

#else

void
fz_write_pixmap_as_jpx(fz_context *ctx, fz_output *out, fz_pixmap *pix, int q)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JPX support disabled");
}

#endif
