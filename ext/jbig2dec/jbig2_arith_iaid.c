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

/* Annex A.3 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stddef.h>
#include <string.h>             /* memset() */

#ifdef VERBOSE
#include <stdio.h>              /* for debug printing only */
#endif

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_iaid.h"

struct _Jbig2ArithIaidCtx {
    int SBSYMCODELEN;
    Jbig2ArithCx *IAIDx;
};

Jbig2ArithIaidCtx *
jbig2_arith_iaid_ctx_new(Jbig2Ctx *ctx, int SBSYMCODELEN)
{
    Jbig2ArithIaidCtx *result = jbig2_new(ctx, Jbig2ArithIaidCtx, 1);
    int ctx_size = 1 << SBSYMCODELEN;

    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to allocate IAID arithmetic coding state");
        return NULL;
    }

    result->SBSYMCODELEN = SBSYMCODELEN;
    result->IAIDx = jbig2_new(ctx, Jbig2ArithCx, ctx_size);
    if (result->IAIDx == NULL)
    {
        jbig2_free(ctx->allocator, result);
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to allocate symbol ID in IAID arithmetic coding state");
        return NULL;
    }

    memset(result->IAIDx, 0, ctx_size);
    return result;
}

/* A.3 */
/* Return value: -1 on error, 0 on normal value */
int
jbig2_arith_iaid_decode(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *actx, Jbig2ArithState *as, int32_t *p_result)
{
    Jbig2ArithCx *IAIDx = actx->IAIDx;
    int SBSYMCODELEN = actx->SBSYMCODELEN;
    int PREV = 1;
    int D;
    int i;

    /* A.3 (2) */
    for (i = 0; i < SBSYMCODELEN; i++) {
        D = jbig2_arith_decode(as, &IAIDx[PREV]);
        if (D < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to decode IAIDx code");
#ifdef VERBOSE
        fprintf(stderr, "IAID%x: D = %d\n", PREV, D);
#endif
        PREV = (PREV << 1) | D;
    }
    /* A.3 (3) */
    PREV -= 1 << SBSYMCODELEN;
#ifdef VERBOSE
    fprintf(stderr, "IAID result: %d\n", PREV);
#endif
    *p_result = PREV;
    return 0;
}

void
jbig2_arith_iaid_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *iax)
{
    if (iax != NULL) {
        jbig2_free(ctx->allocator, iax->IAIDx);
        jbig2_free(ctx->allocator, iax);
    }
}
