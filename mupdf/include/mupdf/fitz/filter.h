#ifndef MUPDF_FITZ_FILTER_H
#define MUPDF_FITZ_FILTER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"

fz_stream *fz_open_copy(fz_stream *chain);
fz_stream *fz_open_null(fz_stream *chain, int len, int offset);
fz_stream *fz_open_concat(fz_context *ctx, int max, int pad);
void fz_concat_push(fz_stream *concat, fz_stream *chain); /* Ownership of chain is passed in */
fz_stream *fz_open_arc4(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_aesd(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_a85d(fz_stream *chain);
fz_stream *fz_open_ahxd(fz_stream *chain);
fz_stream *fz_open_rld(fz_stream *chain);
fz_stream *fz_open_dctd(fz_stream *chain, int color_transform);
fz_stream *fz_open_resized_dctd(fz_stream *chain, int color_transform, int l2factor);
fz_stream *fz_open_faxd(fz_stream *chain,
	int k, int end_of_line, int encoded_byte_align,
	int columns, int rows, int end_of_block, int black_is_1);
fz_stream *fz_open_flated(fz_stream *chain);
fz_stream *fz_open_lzwd(fz_stream *chain, int early_change);
fz_stream *fz_open_predict(fz_stream *chain, int predictor, int columns, int colors, int bpc);

/* cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2314 */
void fz_dctd_set_common_tables(fz_stream *stm, unsigned char *data, int size);

/* SumatraPDF: reuse JBIG2Globals */
#include "mupdf/fitz/store.h"
typedef struct fz_jbig2_globals_s fz_jbig2_globals;
fz_jbig2_globals *fz_load_jbig2_globals(fz_context *ctx, unsigned char *data, int size);
void fz_free_jbig2_globals_imp(fz_context *ctx, fz_storable *globals);
fz_stream *fz_open_jbig2d(fz_stream *chain, fz_jbig2_globals *globals);

#endif
