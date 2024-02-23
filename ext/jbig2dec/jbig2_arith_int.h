/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

/*
    jbig2dec
*/

#ifndef _JBIG2_ARITH_INT_H
#define _JBIG2_ARITH_INT_H

typedef struct _Jbig2ArithIntCtx Jbig2ArithIntCtx;

Jbig2ArithIntCtx *jbig2_arith_int_ctx_new(Jbig2Ctx *ctx);

int jbig2_arith_int_decode(Jbig2Ctx *ctx, Jbig2ArithIntCtx *actx, Jbig2ArithState *as, int32_t *p_result);

void jbig2_arith_int_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIntCtx *iax);

#endif /* _JBIG2_ARITH_INT_H */
