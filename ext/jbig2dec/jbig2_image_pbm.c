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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stdio.h>
#include <ctype.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"

/* take an image structure and write it to a file in pbm format */

int jbig2_image_write_pbm_file(Jbig2Image *image, char *filename)
{
    FILE *out;
    int	error;

    if ((out = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "unable to open '%s' for writing", filename);
        return 1;
    }

    error = jbig2_image_write_pbm(image, out);

    fclose(out);
    return (error);
}

/* write out an image struct as a pbm stream to an open file pointer */

int jbig2_image_write_pbm(Jbig2Image *image, FILE *out)
{
        /* pbm header */
        fprintf(out, "P4\n%d %d\n", image->width, image->height);

        /* pbm format pads to a byte boundary, so we can
           just write out the whole data buffer
           NB: this assumes minimal stride for the width */
        fwrite(image->data, 1, image->height*image->stride, out);

        /* success */
	return 0;
}

/* take an image from a file in pbm format */
Jbig2Image *jbig2_image_read_pbm_file(Jbig2Ctx *ctx, char *filename)
{
    FILE *in;
    Jbig2Image *image;

    if ((in = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open '%s' for reading\n", filename);
		return NULL;
    }

    image = jbig2_image_read_pbm(ctx, in);

    fclose(in);

    return (image);
}

/* FIXME: should handle multi-image files */
Jbig2Image *jbig2_image_read_pbm(Jbig2Ctx *ctx, FILE *in)
{
    int i, dim[2];
    int done;
    Jbig2Image *image;
    int c;
    char buf[32];

    /* look for 'P4' magic */
    while ((c = fgetc(in)) != 'P') {
        if (feof(in)) return NULL;
    }
    if ((c = fgetc(in)) != '4') {
        fprintf(stderr, "not a binary pbm file.\n");
        return NULL;
    }
    /* read size. we must find two decimal numbers representing
       the image dimensions. 'done' will index whether we're
       looking for the width or the height and 'i' will be our
       array index for copying strings into our buffer */
    done = 0;
    i = 0;
    while (done < 2) {
        c = fgetc(in);
        /* skip whitespace */
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        /* skip comments */
        if (c == '#') {
            while ((c = fgetc(in)) != '\n');
            continue;
        }
        /* report unexpected eof */
        if (c == EOF) {
           fprintf(stderr, "end-of-file parsing pbm header\n");
           return NULL;
        }
        if (isdigit(c)) {
            buf[i++] = c;
            while (isdigit(c = fgetc(in))) {
                if (i >= 32) {
                    fprintf(stderr, "pbm parsing error\n");
                    return NULL;
                }
                buf[i++] = c;
            }
            buf[i] = '\0';
            if (sscanf(buf, "%d", &dim[done]) != 1) {
                fprintf(stderr, "couldn't read pbm image dimensions\n");
                return NULL;
            }
            i = 0;
            done++;
        }
    }
    /* allocate image structure */
    image = jbig2_image_new(ctx, dim[0], dim[1]);
    if (image == NULL) {
        fprintf(stderr, "could not allocate %dx%d image for pbm file\n", dim[0], dim[1]);
        return NULL;
    }
    /* the pbm data is byte-aligned, so we can
       do a simple block read */
    fread(image->data, 1, image->height*image->stride, in);
    if (feof(in)) {
        fprintf(stderr, "unexpected end of pbm file.\n");
        jbig2_image_release(ctx, image);
        return NULL;
    }
    /* success */
    return image;
}
