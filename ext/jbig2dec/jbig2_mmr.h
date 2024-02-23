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

#ifndef _JBIG2_MMR_H
#define _JBIG2_MMR_H

int
jbig2_decode_generic_mmr(Jbig2Ctx *ctx, Jbig2Segment *segment, const Jbig2GenericRegionParams *params, const byte *data, size_t size, Jbig2Image *image);

int
jbig2_decode_halftone_mmr(Jbig2Ctx *ctx, const Jbig2GenericRegionParams *params, const byte *data, size_t size, Jbig2Image *image, size_t *consumed_bytes);

#endif /* _JBIG2_MMR_H */
