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

#ifndef _JBIG2_ARITH_H
#define _JBIG2_ARITH_H

typedef struct _Jbig2ArithState Jbig2ArithState;

/* An arithmetic coding context is stored as a single byte, with the
   index in the low order 7 bits (actually only 6 are used), and the
   MPS in the top bit. */
typedef unsigned char Jbig2ArithCx;

/* allocate and initialize a new arithmetic coding state */
Jbig2ArithState *jbig2_arith_new(Jbig2Ctx *ctx, Jbig2WordStream *ws);

/* decode a bit */
bool jbig2_arith_decode(Jbig2ArithState *as, Jbig2ArithCx *pcx, int *code);

/* returns true if the end of the data stream has been reached (for sanity checks) */
bool jbig2_arith_has_reached_marker(Jbig2ArithState *as);

#endif /* _JBIG2_ARITH_H */
