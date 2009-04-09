/*
    jbig2dec

    Copyright (C) 2001 Artifex Software, Inc.

    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.

    For further licensing information refer to http://artifex.com/ or
    contact Artifex Software, Inc., 7 Mt. Lassen Drive - Suite A-134,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

int
jbig2_decode_generic_mmr(Jbig2Ctx *ctx,
			 Jbig2Segment *segment,
			 const Jbig2GenericRegionParams *params,
			 const byte *data, size_t size,
			 Jbig2Image *image);

