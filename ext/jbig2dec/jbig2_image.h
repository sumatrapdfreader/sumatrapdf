/*
    jbig2dec
    
    Copyright (C) 2001-2002 Artifex Software, Inc.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id: jbig2_image.h 461 2008-05-07 21:37:02Z giles $
*/


#ifndef _JBIG2_IMAGE_H
#define _JBIG2_IMAGE_H

int jbig2_image_get_pixel(Jbig2Image *image, int x, int y);
int jbig2_image_set_pixel(Jbig2Image *image, int x, int y, int value);

/* routines for dumping the image data in various formats */
/* FIXME: should these be in the client instead? */

#include <stdio.h>

int jbig2_image_write_pbm_file(Jbig2Image *image, char *filename);
int jbig2_image_write_pbm(Jbig2Image *image, FILE *out);
Jbig2Image *jbig2_image_read_pbm_file(Jbig2Ctx *ctx, char *filename);
Jbig2Image *jbig2_image_read_pbm(Jbig2Ctx *ctx, FILE *in);

#ifdef HAVE_LIBPNG
int jbig2_image_write_png_file(Jbig2Image *image, char *filename);
int jbig2_image_write_png(Jbig2Image *image, FILE *out);
#endif

#endif /* _JBIG2_IMAGE_H */
