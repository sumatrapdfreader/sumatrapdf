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
#if !defined (UINT32_MAX)
#define UINT32_MAX  0xffffffffu
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

static inline void
template_image_compose_opt(const uint8_t * JBIG2_RESTRICT ss, uint8_t * JBIG2_RESTRICT dd, int early, int late, uint8_t leftmask, uint8_t rightmask, uint32_t bytewidth_, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride, Jbig2ComposeOp op)
{
    int i;
    uint32_t j;
    int bytewidth = (int)bytewidth_;

    if (bytewidth == 1) {
        for (j = 0; j < h; j++) {
            /* Only 1 byte! */
            uint8_t v = (((early ? 0 : ss[0]<<8) | (late ? 0 : ss[1]))>>shift);
            if (op == JBIG2_COMPOSE_OR)
                *dd |= v & leftmask;
            else if (op == JBIG2_COMPOSE_AND)
                *dd &= (v & leftmask) | ~leftmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *dd ^= v & leftmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *dd ^= (~v) & leftmask;
            else /* Replace */
                *dd = (v & leftmask) | (*dd & ~leftmask);
            dd += dstride;
            ss += sstride;
        }
        return;
    }
    bytewidth -= 2;
    if (shift == 0) {
        ss++;
        for (j = 0; j < h; j++) {
            /* Left byte */
            const uint8_t * JBIG2_RESTRICT s = ss;
            uint8_t * JBIG2_RESTRICT d = dd;
            if (op == JBIG2_COMPOSE_OR)
                *d++ |= *s++ & leftmask;
            else if (op == JBIG2_COMPOSE_AND)
                *d++ &= (*s++ & leftmask) | ~leftmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *d++ ^= *s++ & leftmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *d++ ^= (~*s++) & leftmask;
            else /* Replace */
                *d = (*s++ & leftmask) | (*d & ~leftmask), d++;
            /* Central run */
            for (i = bytewidth; i != 0; i--) {
                if (op == JBIG2_COMPOSE_OR)
                    *d++ |= *s++;
                else if (op == JBIG2_COMPOSE_AND)
                    *d++ &= *s++;
                else if (op == JBIG2_COMPOSE_XOR)
                    *d++ ^= *s++;
                else if (op == JBIG2_COMPOSE_XNOR)
                    *d++ ^= ~*s++;
                else /* Replace */
                    *d++ = *s++;
            }
            /* Right byte */
            if (op == JBIG2_COMPOSE_OR)
                *d |= *s & rightmask;
            else if (op == JBIG2_COMPOSE_AND)
                *d &= (*s & rightmask) | ~rightmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *d ^= *s & rightmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *d ^= (~*s) & rightmask;
            else /* Replace */
                *d = (*s & rightmask) | (*d & ~rightmask);
            dd += dstride;
            ss += sstride;
        }
    } else {
        for (j = 0; j < h; j++) {
            /* Left byte */
            const uint8_t * JBIG2_RESTRICT s = ss;
            uint8_t * JBIG2_RESTRICT d = dd;
            uint8_t s0, s1, v;
            s0 = early ? 0 : *s;
            s++;
            s1 = *s++;
            v = ((s0<<8) | s1)>>shift;
            if (op == JBIG2_COMPOSE_OR)
                *d++ |= v & leftmask;
            else if (op == JBIG2_COMPOSE_AND)
                *d++ &= (v & leftmask) | ~leftmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *d++ ^= v & leftmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *d++ ^= (~v) & leftmask;
            else /* Replace */
                *d = (v & leftmask) | (*d & ~leftmask), d++;
            /* Central run */
            for (i = bytewidth; i > 0; i--) {
                s0 = s1; s1 = *s++;
                v = ((s0<<8) | s1)>>shift;
                if (op == JBIG2_COMPOSE_OR)
                    *d++ |= v;
                else if (op == JBIG2_COMPOSE_AND)
                    *d++ &= v;
                else if (op == JBIG2_COMPOSE_XOR)
                    *d++ ^= v;
                else if (op == JBIG2_COMPOSE_XNOR)
                    *d++ ^= ~v;
                else /* Replace */
                    *d++ = v;
            }
            /* Right byte */
            s0 = s1; s1 = (late ? 0 : *s);
            v = (((s0<<8) | s1)>>shift);
            if (op == JBIG2_COMPOSE_OR)
                *d |= v & rightmask;
            else if (op == JBIG2_COMPOSE_AND)
                *d &= (v & rightmask) | ~rightmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *d ^= v & rightmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *d ^= ~v & rightmask;
            else /* Replace */
                *d = (v & rightmask) | (*d & ~rightmask);
            dd += dstride;
            ss += sstride;
        }
    }
}

static void
jbig2_image_compose_opt_OR(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_OR);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_OR);
}

static void
jbig2_image_compose_opt_AND(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_AND);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_AND);
}

static void
jbig2_image_compose_opt_XOR(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_XOR);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_XOR);
}

static void
jbig2_image_compose_opt_XNOR(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_XNOR);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_XNOR);
}

static void
jbig2_image_compose_opt_REPLACE(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_REPLACE);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_REPLACE);
}

/* composite one jbig2_image onto another */
int
jbig2_image_compose(Jbig2Ctx *ctx, Jbig2Image *dst, Jbig2Image *src, int x, int y, Jbig2ComposeOp op)
{
    uint32_t w, h;
    uint32_t shift;
    uint32_t leftbyte;
    uint8_t *ss;
    uint8_t *dd;
    uint8_t leftmask, rightmask;
    int early = x >= 0;
    int late;
    uint32_t bytewidth;
    uint32_t syoffset = 0;

    if (src == NULL)
        return 0;

    if ((UINT32_MAX - src->width  < (x > 0 ? x : -x)) ||
        (UINT32_MAX - src->height < (y > 0 ? y : -y)))
    {
#ifdef JBIG2_DEBUG
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "overflow in compose_image");
#endif
        return 0;
    }

    /* This code takes a src image and combines it onto dst at offset (x,y), with operation op. */

    /* Data is packed msb first within a byte, so with bits numbered: 01234567.
     * Second byte is: 89abcdef. So to combine into a run, we use:
     *       (s[0]<<8) | s[1] == 0123456789abcdef.
     * To read from src into dst at offset 3, we need to read:
     *    read:      0123456789abcdef...
     *    write:  0123456798abcdef...
     * In general, to read from src and write into dst at offset x, we need to shift
     * down by (x&7) bits to allow for bit alignment. So shift = x&7.
     * So the 'central' part of our runs will see us doing:
     *   *d++ op= ((s[0]<<8)|s[1])>>shift;
     * with special cases on the left and right edges of the run to mask.
     * With the left hand edge, we have to be careful not to 'underread' the start of
     * the src image; this is what the early flag is about. Similarly we have to be
     * careful not to read off the right hand edge; this is what the late flag is for.
     */

    /* clip */
    w = src->width;
    h = src->height;
    shift = (x & 7);
    ss = src->data - early;

    if (x < 0) {
        if (w < (uint32_t) -x)
            w = 0;
        else
            w += x;
        ss += (-x-1)>>3;
        x = 0;
    }
    if (y < 0) {
        if (h < (uint32_t) -y)
            h = 0;
        else
            h += y;
        syoffset = -y * src->stride;
        y = 0;
    }
    if ((uint32_t)x + w > dst->width)
    {
        if (dst->width < (uint32_t)x)
            w = 0;
        else
            w = dst->width - x;
    }
    if ((uint32_t)y + h > dst->height)
    {
        if (dst->height < (uint32_t)y)
            h = 0;
        else
            h = dst->height - y;
    }
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
    dd = dst->data + y * dst->stride + leftbyte;
    bytewidth = (((uint32_t) x + w - 1) >> 3) - leftbyte + 1;
    leftmask = 255>>(x&7);
    rightmask = (((x+w)&7) == 0) ? 255 : ~(255>>((x+w)&7));
    if (bytewidth == 1)
        leftmask &= rightmask;
    late = (ss + bytewidth >= src->data + ((src->width+7)>>3));
    ss += syoffset;

    switch(op)
    {
    case JBIG2_COMPOSE_OR:
        jbig2_image_compose_opt_OR(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    case JBIG2_COMPOSE_AND:
        jbig2_image_compose_opt_AND(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    case JBIG2_COMPOSE_XOR:
        jbig2_image_compose_opt_XOR(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    case JBIG2_COMPOSE_XNOR:
        jbig2_image_compose_opt_XNOR(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    case JBIG2_COMPOSE_REPLACE:
        jbig2_image_compose_opt_REPLACE(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
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
