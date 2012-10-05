#include "fitz-internal.h"

/* This code needs to be kept out of stm_buffer.c to avoid it being
 * pulled into cmapdump.c */

void
fz_free_compressed_buffer(fz_context *ctx, fz_compressed_buffer *buf)
{
	if (!buf)
		return;

	fz_drop_buffer(ctx, buf->buffer);
	fz_free(ctx, buf);
}


fz_stream *
fz_open_image_decomp_stream(fz_context *ctx, fz_compressed_buffer *buffer, int *factor)
{
	fz_stream *chain = fz_open_buffer(ctx, buffer->buffer);
	fz_compression_params *params = &buffer->params;

	switch (params->type)
	{
	case FZ_IMAGE_FAX:
		*factor = 1;
		return fz_open_faxd(chain,
				params->u.fax.k,
				params->u.fax.end_of_line,
				params->u.fax.encoded_byte_align,
				params->u.fax.columns,
				params->u.fax.rows,
				params->u.fax.end_of_block,
				params->u.fax.black_is_1);
	case FZ_IMAGE_JPEG:
		if (*factor > 8)
			*factor = 8;
		return fz_open_resized_dctd(chain, params->u.jpeg.color_transform, *factor);
	case FZ_IMAGE_RLD:
		*factor = 1;
		return fz_open_rld(chain);
	case FZ_IMAGE_FLATE:
		*factor = 1;
		chain = fz_open_flated(chain);
		if (params->u.flate.predictor > 1)
			chain = fz_open_predict(chain, params->u.flate.predictor, params->u.flate.columns, params->u.flate.colors, params->u.flate.bpc);
		return chain;
	case FZ_IMAGE_LZW:
		*factor = 1;
		chain = fz_open_lzwd(chain, params->u.lzw.early_change);
		if (params->u.lzw.predictor > 1)
			chain = fz_open_predict(chain, params->u.lzw.predictor, params->u.lzw.columns, params->u.lzw.colors, params->u.lzw.bpc);
		return chain;
	default:
		*factor = 1;
		break;
	}

	return chain;
}

fz_stream *
fz_open_compressed_buffer(fz_context *ctx, fz_compressed_buffer *buffer)
{
	int factor = 1;

	return fz_open_image_decomp_stream(ctx, buffer, &factor);
}

unsigned int
fz_compressed_buffer_size(fz_compressed_buffer *buffer)
{
	if (!buffer || !buffer->buffer)
		return 0;
	return (unsigned int)buffer->buffer->cap;
}
