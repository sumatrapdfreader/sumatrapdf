/*
    jbig2dec

    Copyright (C) 2001 Artifex Software, Inc.

    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id: jbig2_arith_int.h 461 2008-05-07 21:37:02Z giles $
*/

typedef struct _Jbig2ArithIntCtx Jbig2ArithIntCtx;

Jbig2ArithIntCtx *
jbig2_arith_int_ctx_new(Jbig2Ctx *ctx);

int
jbig2_arith_int_decode(Jbig2ArithIntCtx *ctx, Jbig2ArithState *as,
		       int32_t *p_result);

void
jbig2_arith_int_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIntCtx *iax);
