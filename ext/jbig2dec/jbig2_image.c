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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>             /* memcpy() */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"

#if !defined (INT32_MAX)
#define INT32_MAX  0x7fffffff
#endif

/* allocate a Jbig2Image structure and its associated bitmap */
Jbig2Image *
jbig2_image_new(Jbig2Ctx *ctx, uint32_t width, uint32_t height)
{
    Jbig2Image *image;
    uint32_t stride;

    if (width == 0 || height == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to create zero sized image");
        return NULL;
    }

    image = jbig2_new(ctx, Jbig2Image, 1);
    if (image == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to allocate image");
        return NULL;
    }

    stride = ((width - 1) >> 3) + 1;    /* generate a byte-aligned stride */

    /* check for integer multiplication overflow */
    if (height > (INT32_MAX / stride)) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "integer multiplication overflow (stride=%u, height=%u)", stride, height);
        jbig2_free(ctx->allocator, image);
        return NULL;
    }
    image->data = jbig2_new(ctx, uint8_t, (size_t) height * stride);
    if (image->data == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to allocate image data buffer (stride=%u, height=%u)", stride, height);
        jbig2_free(ctx->allocator, image);
        return NULL;
    }

    image->width = width;
    image->height = height;
    image->stride = stride;
    image->refcount = 1;

    return image;
}

/* bump the reference count for an image pointer */
Jbig2Image *
jbig2_image_reference(Jbig2Ctx *ctx, Jbig2Image *image)
{
    if (image)
        image->refcount++;
    return image;
}

/* release an image pointer, freeing it it appropriate */
void
jbig2_image_release(Jbig2Ctx *ctx, Jbig2Image *image)
{
    if (image == NULL)
        return;
    image->refcount--;
    if (image->refcount == 0)
        jbig2_image_free(ctx, image);
}

/* free a Jbig2Image structure and its associated memory */
void
jbig2_image_free(Jbig2Ctx *ctx, Jbig2Image *image)
{
    if (image != NULL) {
        jbig2_free(ctx->allocator, image->data);
        jbig2_free(ctx->allocator, image);
    }
}

/* resize a Jbig2Image */
Jbig2Image *
jbig2_image_resize(Jbig2Ctx *ctx, Jbig2Image *image, uint32_t width, uint32_t height, int value)
{
    if (width == image->width) {
        uint8_t *data;

        /* check for integer multiplication overflow */
        if (image->height > (INT32_MAX / image->stride)) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "integer multiplication overflow during resize (stride=%u, height=%u)", image->stride, height);
            return NULL;
        }
        /* use the same stride, just change the length */
        data = jbig2_renew(ctx, image->data, uint8_t, (size_t) height * image->stride);
        if (data == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to reallocate image");
            return NULL;
        }
        image->data = data;
        if (height > image->height) {
            const uint8_t fill = value ? 0xFF : 0x00;
            memset(image->data + (size_t) image->height * image->stride, fill, ((size_t) height - image->height) * image->stride);
        }
        image->height = height;

    } else {
        Jbig2Image *newimage;
        int code;

        /* Unoptimized implementation, but it works. */

        newimage = jbig2_image_new(ctx, width, height);
        if (newimage == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "failed to allocate resized image");
            return NULL;
        }
        jbig2_image_clear(ctx, newimage, value);

        code = jbig2_image_compose(ctx, newimage, image, 0, 0, JBIG2_COMPOSE_REPLACE);
        if (code < 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "failed to compose image buffers when resizing");
            jbig2_image_release(ctx, newimage);
            return NULL;
        }

        /* if refcount > 1 the original image, its pointer must
        be kept, so simply replaces its innards, and throw away
        the empty new image shell. */
        jbig2_free(ctx->allocator, image->data);
        image->width = newimage->width;
        image->height = newimage->height;
        image->stride = newimage->stride;
        image->data = newimage->data;
        jbig2_free(ctx->allocator, newimage);
    }

    return image;
}

/* composite one jbig2_image onto another
   slow but general version */
static int
jbig2_image_compose_unopt(Jbig2Ctx *ctx, Jbig2Image *dst, Jbig2Image *src, int x, int y, Jbig2ComposeOp op)
{
    uint32_t i, j;
    uint32_t sw = src->width;
    uint32_t sh = src->height;
    uint32_t sx = 0;
    uint32_t sy = 0;

    /* clip to the dst image boundaries */
    if (x < 0) {
        sx += -x;
        if (sw < (uint32_t) -x)
            sw = 0;
        else
            sw -= -x;
        x = 0;
    }
    if (y < 0) {
        sy += -y;
        if (sh < (uint32_t) -y)
            sh = 0;
        else
            sh -= -y;
        y = 0;
    }
    if ((uint32_t) x + sw >= dst->width) {
        if (dst->width >= (uint32_t) x)
            sw = dst->width - x;
        else
            sw = 0;
    }
    if ((uint32_t) y + sh >= dst->height) {
        if (dst->height >= (uint32_t) y)
            sh = dst->height - y;
        else
            sh = 0;
    }

    switch (op) {
    case JBIG2_COMPOSE_OR:
        for (j = 0; j < sh; j++) {
            for (i = 0; i < sw; i++) {
                jbig2_image_set_pixel(dst, i + x, j + y, jbig2_image_get_pixel(src, i + sx, j + sy) | jbig2_image_get_pixel(dst, i + x, j + y));
            }
        }
        break;
    case JBIG2_COMPOSE_AND:
        for (j = 0; j < sh; j++) {
            for (i = 0; i < sw; i++) {
                jbig2_image_set_pixel(dst, i + x, j + y, jbig2_image_get_pixel(src, i + sx, j + sy) & jbig2_image_get_pixel(dst, i + x, j + y));
            }
        }
        break;
    case JBIG2_COMPOSE_XOR:
        for (j = 0; j < sh; j++) {
            for (i = 0; i < sw; i++) {
                jbig2_image_set_pixel(dst, i + x, j + y, jbig2_image_get_pixel(src, i + sx, j + sy) ^ jbig2_image_get_pixel(dst, i + x, j + y));
            }
        }
        break;
    case JBIG2_COMPOSE_XNOR:
        for (j = 0; j < sh; j++) {
            for (i = 0; i < sw; i++) {
                jbig2_image_set_pixel(dst, i + x, j + y, (jbig2_image_get_pixel(src, i + sx, j + sy) == jbig2_image_get_pixel(dst, i + x, j + y)));
            }
        }
        break;
    case JBIG2_COMPOSE_REPLACE:
        for (j = 0; j < sh; j++) {
            for (i = 0; i < sw; i++) {
                jbig2_image_set_pixel(dst, i + x, j + y, jbig2_image_get_pixel(src, i + sx, j + sy));
            }
        }
        break;
    }

    return 0;
}

/* composite one jbig2_image onto another */
int
jbig2_image_compose(Jbig2Ctx *ctx, Jbig2Image *dst, Jbig2Image *src, int x, int y, Jbig2ComposeOp op)
{
    uint32_t i, j;
    uint32_t w, h;
    uint32_t leftbyte, rightbyte;
    uint32_t shift;
    uint8_t *s, *ss;
    uint8_t *d, *dd;
    uint8_t mask, rightmask;

    if (src == NULL)
        return 0;

    /* The optimized code for the OR operator below doesn't
       handle the source image partially placed outside the
       destination (above and/or to the left). The affected
       intersection of the destination is computed correctly,
       however the correct subset of the source image is not
       chosen. Instead the upper left corner of the source image
       is always used.

       In the unoptimized version that handles all operators
       (including OR) the correct subset of the source image is
       chosen.

       The workaround is to check whether the x/y coordinates to
       the composition operator are negative and in this case use
       the unoptimized implementation.

       TODO: Fix the optimized OR implementation if possible. */
    if (op != JBIG2_COMPOSE_OR || x < 0 || y < 0) {
        /* hand off the the general routine */
        return jbig2_image_compose_unopt(ctx, dst, src, x, y, op);
    }

    /* optimized code for the prevalent OR operator */

    /* clip */
    w = src->width;
    h = src->height;
    ss = src->data;

    if (x < 0) {
        if (w < (uint32_t) -x)
            w = 0;
        else
            w += x;
        x = 0;
    }
    if (y < 0) {
        if (h < (uint32_t) -y)
            h = 0;
        else
            h += y;
        y = 0;
    }
    w = ((uint32_t) x + w < dst->width) ? w : ((dst->width >= (uint32_t) x) ? dst->width - (uint32_t) x : 0);
    h = ((uint32_t) y + h < dst->height) ? h : ((dst->height >= (uint32_t) y) ? dst->height - (uint32_t) y : 0);
#ifdef JBIG2_DEBUG
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "compositing %dx%d at (%d, %d) after clipping", w, h, x, y);
#endif

    /* check for zero clipping region */
    if ((w <= 0) || (h <= 0)) {
#ifdef JBIG2_DEBUG
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "zero clipping region");
#endif
        return 0;
    }

    leftbyte = (uint32_t) x >> 3;
    rightbyte = ((uint32_t) x + w - 1) >> 3;
    shift = x & 7;

    /* general OR case */
    s = ss;
    d = dd = dst->data + y * dst->stride + leftbyte;
    if (d < dst->data ||
        leftbyte > dst->stride ||
        d - leftbyte + (size_t) h * dst->stride > dst->data + (size_t) dst->height * dst->stride ||
        s - leftbyte + (size_t) (h - 1) * src->stride + rightbyte > src->data + (size_t) src->height * src->stride) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "preventing heap overflow in jbig2_image_compose");
    }
    if (leftbyte == rightbyte) {
        mask = 0x100 - (0x100 >> w);
        for (j = 0; j < h; j++) {
            *d |= (*s & mask) >> shift;
            d += dst->stride;
            s += src->stride;
        }
    } else if (shift == 0) {
        rightmask = (w & 7) ? 0x100 - (1 << (8 - (w & 7))) : 0xFF;
        for (j = 0; j < h; j++) {
            for (i = leftbyte; i < rightbyte; i++)
                *d++ |= *s++;
            *d |= *s & rightmask;
            d = (dd += dst->stride);
            s = (ss += src->stride);
        }
    } else {
        bool overlap = (((w + 7) >> 3) < ((x + w + 7) >> 3) - (x >> 3));

        mask = 0x100 - (1 << shift);
        if (overlap)
            rightmask = (0x100 - (0x100 >> ((x + w) & 7))) >> (8 - shift);
        else
            rightmask = 0x100 - (0x100 >> (w & 7));
        for (j = 0; j < h; j++) {
            *d++ |= (*s & mask) >> shift;
            for (i = leftbyte; i < rightbyte - 1; i++) {
                *d |= ((*s++ & ~mask) << (8 - shift));
                *d++ |= ((*s & mask) >> shift);
            }
            if (overlap)
                *d |= (*s & rightmask) << (8 - shift);
            else
                *d |= ((s[0] & ~mask) << (8 - shift)) | ((s[1] & rightmask) >> shift);
            d = (dd += dst->stride);
            s = (ss += src->stride);
        }
    }

    return 0;
}

/* initialize an image bitmap to a constant value */
void
jbig2_image_clear(Jbig2Ctx *ctx, Jbig2Image *image, int value)
{
    const uint8_t fill = value ? 0xFF : 0x00;

    memset(image->data, fill, image->stride * image->height);
}

/* look up a pixel value in an image.
   returns 0 outside the image frame for the convenience of
   the template code
*/
int
jbig2_image_get_pixel(Jbig2Image *image, int x, int y)
{
    const int w = image->width;
    const int h = image->height;
    const int byte = (x >> 3) + y * image->stride;
    const int bit = 7 - (x & 7);

    if ((x < 0) || (x >= w))
        return 0;
    if ((y < 0) || (y >= h))
        return 0;

    return ((image->data[byte] >> bit) & 1);
}

/* set an individual pixel value in an image */
void
jbig2_image_set_pixel(Jbig2Image *image, int x, int y, bool value)
{
    const int w = image->width;
    const int h = image->height;
    int scratch, mask;
    int bit, byte;

    if ((x < 0) || (x >= w))
        return;
    if ((y < 0) || (y >= h))
        return;

    byte = (x >> 3) + y * image->stride;
    bit = 7 - (x & 7);
    mask = (1 << bit) ^ 0xff;

    scratch = image->data[byte] & mask;
    image->data[byte] = scratch | (value << bit);
}
