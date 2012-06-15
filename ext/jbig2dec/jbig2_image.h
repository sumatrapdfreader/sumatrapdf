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



#ifndef _JBIG2_IMAGE_H
#define _JBIG2_IMAGE_H

int jbig2_image_get_pixel(Jbig2Image *image, int x, int y);
int jbig2_image_set_pixel(Jbig2Image *image, int x, int y, bool value);

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
