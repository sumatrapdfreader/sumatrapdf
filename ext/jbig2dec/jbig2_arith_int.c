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


/* Annex A */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memset() */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"

struct _Jbig2ArithIntCtx {
  Jbig2ArithCx IAx[512];
};

Jbig2ArithIntCtx *
jbig2_arith_int_ctx_new(Jbig2Ctx *ctx)
{
  Jbig2ArithIntCtx *result = jbig2_new(ctx, Jbig2ArithIntCtx, 1);

  if (result == NULL)
  {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
          "failed to allocate Jbig2ArithIntCtx in jbig2_arith_int_ctx_new");
  }
  else
  {
      memset(result->IAx, 0, sizeof(result->IAx));
  }

  return result;
}

/* A.2 */
/* Return value: -1 on error, 0 on normal value, 1 on OOB return. */
int
jbig2_arith_int_decode(Jbig2ArithIntCtx *ctx, Jbig2ArithState *as,
		       int32_t *p_result)
{
  Jbig2ArithCx *IAx = ctx->IAx;
  int PREV = 1;
  int S, V;
  int bit;
  int n_tail, offset;
  int i;

  S = jbig2_arith_decode(as, &IAx[PREV]);
  PREV = (PREV << 1) | S;

  bit = jbig2_arith_decode(as, &IAx[PREV]);
  PREV = (PREV << 1) | bit;
  if (bit)
    {
      bit = jbig2_arith_decode(as, &IAx[PREV]);
      PREV = (PREV << 1) | bit;

      if (bit)
	{
	  bit = jbig2_arith_decode(as, &IAx[PREV]);
	  PREV = (PREV << 1) | bit;

	  if (bit)
	    {
	      bit = jbig2_arith_decode(as, &IAx[PREV]);
	      PREV = (PREV << 1) | bit;

	      if (bit)
		{
		  bit = jbig2_arith_decode(as, &IAx[PREV]);
		  PREV = (PREV << 1) | bit;

		  if (bit)
		    {
		      n_tail = 32;
		      offset = 4436;
		    }
		  else
		    {
		      n_tail = 12;
		      offset = 340;
		    }
		}
	      else
		{
		  n_tail = 8;
		  offset = 84;
		}
	    }
	  else
	    {
	      n_tail = 6;
	      offset = 20;
	    }
	}
      else
	{
	  n_tail = 4;
	  offset = 4;
	}
    }
  else
    {
      n_tail = 2;
      offset = 0;
    }

  V = 0;
  for (i = 0; i < n_tail; i++)
    {
      bit = jbig2_arith_decode(as, &IAx[PREV]);
      PREV = ((PREV << 1) & 511) | (PREV & 256) | bit;
      V = (V << 1) | bit;
    }

  V += offset;
  V = S ? -V : V;
  *p_result = V;
  return S && V == 0 ? 1 : 0;
}

void
jbig2_arith_int_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIntCtx *iax)
{
  jbig2_free(ctx->allocator, iax);
}
