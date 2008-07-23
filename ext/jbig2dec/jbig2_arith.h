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

    $Id: jbig2_arith.h 461 2008-05-07 21:37:02Z giles $
*/

typedef struct _Jbig2ArithState Jbig2ArithState;

/* An arithmetic coding context is stored as a single byte, with the
   index in the low order 7 bits (actually only 6 are used), and the
   MPS in the top bit. */
typedef unsigned char Jbig2ArithCx;

/* allocate and initialize a new arithmetic coding state */
Jbig2ArithState *
jbig2_arith_new (Jbig2Ctx *ctx, Jbig2WordStream *ws);

/* decode a bit */
bool
jbig2_arith_decode (Jbig2ArithState *as, Jbig2ArithCx *pcx);


