/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/


typedef struct _Jbig2ArithIaidCtx Jbig2ArithIaidCtx;

Jbig2ArithIaidCtx *
jbig2_arith_iaid_ctx_new(Jbig2Ctx *ctx, int SBSYMCODELEN);

int
jbig2_arith_iaid_decode(Jbig2ArithIaidCtx *ctx, Jbig2ArithState *as,
		       int32_t *p_result);

void
jbig2_arith_iaid_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *iax);
