#ifndef MUPDF_FITZ_FILTER_H
#define MUPDF_FITZ_FILTER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/stream.h"

typedef struct fz_jbig2_globals_s fz_jbig2_globals;

typedef struct
{
	int64_t offset;
	size_t length;
} fz_range;

fz_stream *fz_open_null_filter(fz_context *ctx, fz_stream *chain, int len, int64_t offset);
fz_stream *fz_open_range_filter(fz_context *ctx, fz_stream *chain, fz_range *ranges, int nranges);
fz_stream *fz_open_endstream_filter(fz_context *ctx, fz_stream *chain, int len, int64_t offset);
fz_stream *fz_open_concat(fz_context *ctx, int max, int pad);
void fz_concat_push_drop(fz_context *ctx, fz_stream *concat, fz_stream *chain); /* Ownership of chain is passed in */
fz_stream *fz_open_arc4(fz_context *ctx, fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_aesd(fz_context *ctx, fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_a85d(fz_context *ctx, fz_stream *chain);
fz_stream *fz_open_ahxd(fz_context *ctx, fz_stream *chain);
fz_stream *fz_open_rld(fz_context *ctx, fz_stream *chain);
fz_stream *fz_open_dctd(fz_context *ctx, fz_stream *chain, int color_transform, int l2factor, fz_stream *jpegtables);
fz_stream *fz_open_faxd(fz_context *ctx, fz_stream *chain,
	int k, int end_of_line, int encoded_byte_align,
	int columns, int rows, int end_of_block, int black_is_1);
fz_stream *fz_open_flated(fz_context *ctx, fz_stream *chain, int window_bits);
fz_stream *fz_open_lzwd(fz_context *ctx, fz_stream *chain, int early_change, int min_bits, int reverse_bits, int old_tiff);
fz_stream *fz_open_predict(fz_context *ctx, fz_stream *chain, int predictor, int columns, int colors, int bpc);
fz_stream *fz_open_jbig2d(fz_context *ctx, fz_stream *chain, fz_jbig2_globals *globals);

fz_jbig2_globals *fz_load_jbig2_globals(fz_context *ctx, fz_buffer *buf);
fz_jbig2_globals *fz_keep_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals);
void fz_drop_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals);
void fz_drop_jbig2_globals_imp(fz_context *ctx, fz_storable *globals);

/* Extra filters for tiff */
fz_stream *fz_open_sgilog16(fz_context *ctx, fz_stream *chain, int w);
fz_stream *fz_open_sgilog24(fz_context *ctx, fz_stream *chain, int w);
fz_stream *fz_open_sgilog32(fz_context *ctx, fz_stream *chain, int w);
fz_stream *fz_open_thunder(fz_context *ctx, fz_stream *chain, int w);

#endif
