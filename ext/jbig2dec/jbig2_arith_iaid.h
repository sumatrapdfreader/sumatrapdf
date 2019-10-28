/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifndef _JBIG2_ARITH_IAID_H
#define _JBIG2_ARITH_IAID_H

typedef struct _Jbig2ArithIaidCtx Jbig2ArithIaidCtx;

Jbig2ArithIaidCtx *jbig2_arith_iaid_ctx_new(Jbig2Ctx *ctx, int SBSYMCODELEN);

int jbig2_arith_iaid_decode(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *actx, Jbig2ArithState *as, int32_t *p_result);

void jbig2_arith_iaid_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *iax);

#endif /* _JBIG2_ARITH_IAID_H */
