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

/**
 * Generic region handlers.
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stddef.h>
#include <string.h>             /* memcpy(), memset() */

#ifdef OUTPUT_PBM
#include <stdio.h>
#endif

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_generic.h"
#include "jbig2_image.h"
#include "jbig2_mmr.h"
#include "jbig2_page.h"
#include "jbig2_segment.h"

#if !defined (UINT32_MAX)
#define UINT32_MAX 0xffffffff
#endif

/*
This is an explanation of the unoptimized and optimized generic
region decoder implementations below, wherein we try to explain
all the magic numbers.

The generic region decoders decode the output pixels one row at a
time, top to bottom. Within each row the pixels are decoded left
to right. The input for the arithmetic integer decoder used to
decode each pixel is a context consisting of up to 16 previously
decoded pixels. These pixels are chosen according to a predefined
template placed relative to the location of the pixel to be
decoded (6.2.5.3 figures 3, 4, 5 and 6). There are four different
template that may be used (6.2.5.3). The template to use is
determined by GBTEMPLATE. GBTEMPLATE is set in the symbol
dictionary (6.5.8.1), generic region (7.4.6.4), or when decoding
a halftone region's gray-scale image (annex C.5).

Most of the pixels in each template have fixed locations relative
to the pixel to be decoded. However, all templates have at least
one adaptive pixel. The adaptive pixels have nominal locations,
but these locations may be changed by GBAT. GBAT is set in the
symbol dictionary (7.4.2.1.2), generic region (7.4.6.1), or hard
coded as for halftone patterns (6.7.5).

Adaptive pixels are restricted to fall within a field of
previously decoded pixels relative to the pixel to be decoded
(figure 7). The relative Y-coordinate for these adaptive pixels
may vary between -128 and 0. The relative X-coordinate may vary
between -128 and +127 (however, if the Y-coordinate is 0 the
range of the X-coordinate is further restricted to -128 to -1
since the pixels at locations 0 to +127 have not yet been
decoded). If a template refers to a pixel location that reside
outside of the image boundaries its value is assumed to be 0.

UNOPTIMIZED DECODER

The unoptimized decoders first check the contents of GBAT. If
GBAT specifies that any of the adaptive pixels reside outside the
allowed field the decoding is aborted. Next, each row is
processed top to bottom, left to right, one pixel at a time. For
each pixel a context is created containing the bit values of the
pixels that fall inside the template.

The order these bits are stored in the context is implementation
dependent (6.2.5.3). We store the bit values in the CONTEXT
variable from LSB to MSB, starting with the value of the pixel to
the left of the current pixel, continuing right to left, bottom
to top following the template. Using the CONTEXT created from
these pixel values, the arithmetic integer decoder retrieves the
pixel value, which is then written into the output image.

Example when GBTEMPLATE is 2:

The figure below represents a pixel grid of the output image.
Each pixel is a single bit in the image. The pixel "OO" in the
figure below is about to be decoded. The pixels "??" have not
been decoded yet. The CONTEXT variable is constructed by
combining the bit values from the pixels referred to by the
template, shifted to their corresponding bit position.

     .    .    .    .    .    .    .    .
     .    .    .    .    .    .    .    .
  ...+----+----+----+----+----+----+----+...
     |    |    | X9 | X8 | X7 |    |    |
  ...+----+----+----+----+----+----+----+...
     |    | X6 | X5 | X4 | X3 | A1 |    |
  ...+----+----+----+----+----+----+----+...
     |    | X2 | X1 | OO | ?? | ?? | ?? |
  ...+----+----+----+----+----+----+----+...
     .    .    .    .    .    .    .    .
     .    .    .    .    .    .    .    .

In the table below pixel OO is assumed to be at coordinate (x, y).

Bit 9: Pixel at location (x-1, y-2) (This is fixed pixel X9)
Bit 8: Pixel at location (x  , y-2) (This is fixed pixel X8)
Bit 7: Pixel at location (x+1, y-2) (This is fixed pixel X7)
Bit 6: Pixel at location (x-2, y-1) (This is fixed pixel X6)
Bit 5: Pixel at location (x-1, y-1) (This is fixed pixel X5)
Bit 4: Pixel at location (x  , y-1) (This is fixed pixel X4)
Bit 3: Pixel at location (x+1, y-1) (This is fixed pixel X3)
Bit 2: Pixel at location (x+2, y-1) (This is adaptive pixel A1)
Bit 1: Pixel at location (x-2, y  ) (This is fixed pixel X2)
Bit 0: Pixel at location (x-1, y  ) (This is fixed pixel X1)

The location of adaptive pixel A1 may not always be at the
nominal location (x+2, y-1). It could be at any pixel location to
the left or above OO as specified by GBAT, e.g. at the location
(x-128, y+127).

OPTIMIZED DECODER

The optimized decoders work differently. They strive to avoid
recreating the arithmetic integer decoder context from scratch
for every pixel decoded. Instead they reuse part of the CONTEXT
used to compute the previous pixel (the pixel to left of the one
now being decoded). They also keep two sliding windows of pixel
bit values from the two rows of pixels immediately above the
pixel to be decoded. These are stored in the 32-bit variables
line_m1 (row above the pixel to be decoded) and line_m2 (row
above that of line_m1). These optimized decoders ONLY work for
the nominal adaptive pixel locations since these locations are
hard-coded into the implementation.

The bit ordering in the CONTEXT variable is identical to the
unoptimized case described above.

The optimized decoders decode the output pixels one row at a
time, top to bottom. Within each row the pixels are decoded in
batches of up to eight pixels at a time (except possibly the
right most batch which may be less than eight pixels). The
batches in a row are decoded in sequence from left to right.
Within each such batch the pixels are decoded in sequence from
left to right.

Before decoding the pixels in a row the two sliding windows of
pixel values are reset. The first eight pixels of the row above
the pixel to be decoded is stored in line_m1, while line_m2
stores the first eight pixels of the row above that of line_m1.

The figure below illustrates the situation where the template has
been placed so that the decoded pixel OO is the very first pixel
of a row. It also gives labels to various pixels that we will
refer to below.

             .    .    .    .    .    .    .    .    .    .    .
             |    .    .    .    .    .    .    .    .    .    .
   +    +    +----+----+----+----+----+----+----+----+----+----+...
          X9 | X8 | X7 | m1 | m2 | m3 | m4 | m5 | m6 | m7 |    |
   +    +    +----+----+----+----+----+----+----+----+----+----+...
     X6   X5 | X4 | X3 | A1 | n1 | n2 | n3 | n4 | n5 | n6 | n7 |
   +    +    +----+----+----+----+----+----+----+----+----+----+...
     X2   X1 | OO |    |    |    |    |    |    |    |    |    |
   +    +    +----+----+----+----+----+----+----+----+----+----+...
             |    .    .    .    .    .    .    .    .    .    .
             .    .    .    .    .    .    .    .    .    .    .

The pixels X1, X2, X5, X6 and X9 all reside outside the left edge
of the image. These pixels (like all others outside the image)
can according to 6.2.5.2 be assumed to be 0. line_m1 stores n5
through n1 as well as A1, and X3 through X6. line_m2 stores m6
through m1 as well as X7 through X9. The bits in line_m2 are also
shifted left four bits as seen below.

15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0 | bit position
------------------------------------------------+------------------
 0  0  0  0  0  0 X6 X5 X4 X3 A1 n1 n2 n3 n4 n5 | line_m1
 0  0  0 X9 X8 X7 m1 m2 m3 m4 m5 m6  0  0  0  0 | line_m2

The way line_m1 and line_m2 are stored means we can simply shift
them by the same amount to move the sliding window.

The bit order in line_m1 and line_m2 matches the ordering in the
CONTEXT variable. Each bit for the 'A' and 'X' pixels in line_m1
and line_m2 correspond to the equivalent bits in CONTEXT, only
shifted right by 3 bits. Thus X3 is bit 3 in CONTEXT and bit 6 in
line_m1, etc.

The initial arithmetic integer decoder context is created and
stored in the CONTEXT variable by masking, shifting, and bitwise
ORing the contents of line_m1 and line_m2. The "CONTEXT contents"
row is only shown for clarity, it is not present in the code.

15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0 | bit position
------------------------------------------------+---------------------------
 0  0  0  0  0  0  0  0  0 X6 X5 X4 X3 A1 n1 n2 | line_m1 >> 3
 0  0  0  0  0  0  0  0  0  1  1  1  1  1  0  0 | mask for line_m1 (0x7c)
 0  0  0  0  0  0  0  0  0 X6 X5 X4 X3 A1  0  0 | line_m1 AND mask
------------------------------------------------+---------------------------
 0  0  0  0  0  0 X9 X8 X7 m1 m2 m3 m4 m5 m6  0 | line_m2 >> 3
 0  0  0  0  0  0  1  1  1  0  0  0  0  0  0  0 | mask for line_m2 (0x380)
 0  0  0  0  0  0 X9 X8 X7  0  0  0  0  0  0  0 | line_m2 AND mask
------------------------------------------------+---------------------------
 0  0  0  0  0  0 X9 X8 X7 X6 X5 X4 X3 A1  0  0 | CONTEXT = line_m1 OR line_m2
------------------------------------------------+---------------------------
 0  0  0  0  0  0 X9 X8 X7 X6 X5 X4 X3 A1 X2 X1 | CONTEXT contents

Each batch is normally 8 bits, but at the right edge of the image
we may have fewer pixels to decode. The minor_width is how many
pixels the current batch should decode, with a counter variable
x_minor to keep track of the current pixel being decoded.

In order to process a new batch of pixels, unless we're at the
rightmost batch of pixels, we need to refill the sliding window
variables with eight new bits. Looking at the diagram above we
can see that in order to decode eight pixels starting with O0
we'll need to have bits up to pixel 'n7' for line_m1 and 'm7' for
line_m2 available (A1 and X7 moved right 7 times). To do this
simply and quickly, we shift line_m1 left by 8 bits, and OR in
the next byte from corresponding row. Likewise for line_m2, but
the next byte from the image is also shifted left by 4 bits to
compensate for line_m2 having the four least significant bits
unused.

These new eight bits contain the bit values of the eight pixels
to the right of those already present in line_m1 and line_m2. We
call these new bits m7 through mE, and n6 through nD, as
illustrated below.

23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0 | bit position
------------------------------------------------------------------------+-------------
 0  0  0  0  0  0  0  0  0  0  0  0  0  0 X6 X5 X4 X3 A1 n1 n2 n3 n4 n5 | original line_m1
 0  0  0  0  0  0 X6 X5 X4 X3 A1 n1 n2 n3 n4 n5  0  0  0  0  0  0  0  0 | line_m1 shifted left by 8
 0  0  0  0  0  0 X6 X5 X4 X3 A1 n1 n2 n3 n4 n5 n6 n7 n8 n9 nA nB nC nD | line_m1 with new bits ORed in
------------------------------------------------------------------------+-------------
 0  0  0  0  0  0  0  0  0  0  0 X9 X8 X7 m1 m2 m3 m4 m5 m6  0  0  0  0 | original line_m2
 0  0  0 X9 X8 X7 m1 m2 m3 m4 m5 m6  0  0  0  0  0  0  0  0  0  0  0  0 | line_m2 shifted left by 8
 0  0  0 X9 X8 X7 m1 m2 m3 m4 m5 m6 m7 m8 m9 mA mB mC mD mE  0  0  0  0 | line_m2 with new bits ORed in

             .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .
             |    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .
   +    +    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+...
          X9 | X8 | X7 | m1 | m2 | m3 | m4 | m5 | m6 | m7 | m8 | m9 | mA | mB | mC | mD | mE |    |    |    |
   +    +    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+...
     X6   X5 | X4 | X3 | A1 | n1 | n2 | n3 | n4 | n5 | n6 | n7 | n8 | n9 | nA | nB | nC | nD |    |    |    |
   +    +    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+...
     X2   X1 | OO |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
   +    +    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+...
             |    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .
             .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .

CONTEXT, line_m1 and line_m2 now contain all necessary bits to
decode a full batch of eight pixels.

The first pixel in the batch is decoded using this CONTEXT. After
that, for each following pixel we need to update the CONTEXT
using both the last decoded pixel value and new bits from line_m1
and line_m2.

             .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .
             |    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .
   +    +    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+...
         (X9)|_X8_|_X7_|>m1<| m2 | m3 | m4 | m5 | m6 | m7 | m8 | m9 | mA | mB | mC | mD | mE |    |    |    |
   +    +    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+...
    (X6) _X5_|_X4_|_X3_|_A1_|>n1<| n2 | n3 | n4 | n5 | n6 | n7 | n8 | n9 | nA | nB | nC | nD |    |    |    |
   +    +    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+...
    (X2) _X1_|>OO<| oo |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
   +    +    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+...
             |    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .
             .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .    .

This figure illustrates what happens when the same template is
overlaid on itself shifted one pixel to the right in order to
decode the next pixel. Pixels marked with _  _ are pixels that
are present in both templates' CONTEXTs and can be reused. Pixels
marked with (  ) are pixels from the first template that are no
longer necessary and can be removed from CONTEXT. Pixels marked
with >  < are new pixels that were not part of the original
CONTEXT, and so need to be moved into the CONTEXT at the
appropriate locations. In general the leftmost pixels of each
template row can be forgotten, while new pixels are needed at the
right most location of each row.

The CONTEXT corresponding to the current pixel OO and how it is
masked is shown below. Note how the left most pixel of each row
of the template is NOT propagated to the CONTEXT, these pixels
are X2, X6 and X9. This is done by having the mask being 0 at the
corresponding locations.

 9  8  7  6  5  4  3  2  1  0 | bit position
------------------------------+-------------
X9 X8 X7 X6 X5 X4 X3 A1 X2 X1 | pixel values from CONTEXT
 0  1  1  0  1  1  1  1  0  1 | reused pixel bit value mask (0x1bd)
 0 X8 X7  0 X5 X4 X3 A1  0 X1 | reused pixel values from CONTEXT

Next the CONTEXT is shifted left by one bit to make it reference
the next pixel to be decoded. The pixel bit value we just decoded
is then written into the bit corresponding to X1. The sliding
windows in line_m1 and line_m2 are both shifted (10 - x_minor)
bits to the right to make the needed pixels' bit values appear at
the correct positions to be ORed into CONTEXT. Note that this
shift amount depends on which bit in the batch is currently being
computed, as is given by the x_minor counter. In the example
below we assume that x_minor is 0.

 9  8  7  6  5  4  3  2  1  0 | bit position
------------------------------+--------------
 0 X8 X7  0 X5 X4 X3 A1  0  0 | reused pixels from CONTEXT
X8 X7  0 X5 X4 X3 A1  0  0  0 | reused pixels shifted left 1 bit
------------------------------+--------------
X8 X7  0 X5 X4 X3 A1  0 X1 OO | new CONTEXT with current pixel at LSB
------------------------------+--------------
 0  0 X6 X5 X4 X3 A1 n1 n2 n3 | line_m1 shifted (10 - x_minor) bits to the right
 0  0  0  0  0  0  0  1  0  0 | mask for new adaptive pixel one row above (0x4)
X8 X7  0 X5 X4 X3 A1 n1 X1 OO | new CONTEXT with new adaptive pixel
------------------------------+--------------
X8 X7 m1 m2 m3 m4 m5 m6 m7 m8 | line_m2 with new bits ORed in
 0  0  1  0  0  0  0  0  0  0 | mask for new pixel two rows above (0x80)
X8 X7 m1 X5 X4 X3 A1 n1 X1 OO | new CONTEXT with new pixel

This makes the computation of the new CONTEXT be:

NEWCONTEXT = (CONTEXT & 0x1bd) << 1
NEWCONTEXT |= newbit;
NEWCONTEXT |= (line_m1 >> (10-x_minor)) & 0x4;
NEWCONTEXT |= (line_m2 >> (10-x_minor)) & 0x80;

The optimized decoding functions for GBTEMPLATE 0, 1 and 3 all
work similarly. */

/* return the appropriate context size for the given template */
int
jbig2_generic_stats_size(Jbig2Ctx *ctx, int template)
{
    int stats_size = template == 0 ? 1 << 16 : template == 1 ? 1 << 13 : 1 << 10;

    return stats_size;
}

static int
jbig2_decode_generic_template0(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params, Jbig2ArithState *as,
                               Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    const uint32_t rowstride = image->stride;
    uint32_t x, y;
    byte *line2 = NULL;
    byte *line1 = NULL;
    byte *gbreg_line = (byte *) image->data;

#ifdef OUTPUT_PBM
    printf("P4\n%d %d\n", GBW, GBH);
#endif

    if (GBW <= 0)
        return 0;

    for (y = 0; y < GBH; y++) {
        uint32_t CONTEXT;
        uint32_t line_m1;
        uint32_t line_m2;
        uint32_t padded_width = (GBW + 7) & -8;
        int code = 0;

        line_m1 = line1 ? line1[0] : 0;
        line_m2 = line2 ? line2[0] << 6 : 0;
        CONTEXT = (line_m1 & 0x7f0) | (line_m2 & 0xf800);

        /* 6.2.5.7 3d */
        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int x_minor;
            int minor_width = GBW - x > 8 ? 8 : GBW - x;

            if (line1)
                line_m1 = (line_m1 << 8) | (x + 8 < GBW ? line1[(x >> 3) + 1] : 0);

            if (line2)
                line_m2 = (line_m2 << 8) | (x + 8 < GBW ? line2[(x >> 3) + 1] << 6 : 0);

            /* This is the speed-critical inner loop. */
            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                bool bit;

                bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
                if (code)
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template0 optimized");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0x7bf7) << 1) | bit | ((line_m1 >> (7 - x_minor)) & 0x10) | ((line_m2 >> (7 - x_minor)) & 0x800);
            }
            gbreg_line[x >> 3] = result;
        }
#ifdef OUTPUT_PBM
        fwrite(gbreg_line, 1, rowstride, stdout);
#endif
        line2 = line1;
        line1 = gbreg_line;
        gbreg_line += rowstride;
    }

    return 0;
}

#define pixel_outside_field(x, y) \
    ((y) < -128 || (y) > 0 || (x) < -128 || ((y) < 0 && (x) > 127) || ((y) == 0 && (x) >= 0))

static int
jbig2_decode_generic_template0_unopt(Jbig2Ctx *ctx,
                                     Jbig2Segment *segment,
                                     const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    uint32_t CONTEXT;
    uint32_t x, y;
    bool bit;
    int code = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]) ||
        pixel_outside_field(params->gbat[2], params->gbat[3]) ||
        pixel_outside_field(params->gbat[4], params->gbat[5]) ||
        pixel_outside_field(params->gbat[6], params->gbat[7]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    /* this version is generic and easy to understand, but very slow */

    for (y = 0; y < GBH; y++) {
        for (x = 0; x < GBW; x++) {
            if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                jbig2_image_set_pixel(image, x, y, 0);
                continue;
            }
            CONTEXT = 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y) << 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
            CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
            CONTEXT |= jbig2_image_get_pixel(image, x - 4, y) << 3;
            CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 4;
            CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 1) << 5;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 6;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 7;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 8;
            CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 9;
            CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[2], y + params->gbat[3]) << 10;
            CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[4], y + params->gbat[5]) << 11;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 12;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 2) << 13;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 14;
            CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[6], y + params->gbat[7]) << 15;
            bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
            if (code)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template0 unoptimized");
            jbig2_image_set_pixel(image, x, y, bit);
        }
    }
    return 0;
}

static int
jbig2_decode_generic_template1_unopt(Jbig2Ctx *ctx,
                                     Jbig2Segment *segment,
                                     const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    uint32_t CONTEXT;
    uint32_t x, y;
    bool bit;
    int code = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    /* this version is generic and easy to understand, but very slow */

    for (y = 0; y < GBH; y++) {
        for (x = 0; x < GBW; x++) {
            if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                jbig2_image_set_pixel(image, x, y, 0);
                continue;
            }
            CONTEXT = 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y) << 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
            CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
            CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 3;
            CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 1) << 4;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 5;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 6;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 7;
            CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 8;
            CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 2) << 9;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 10;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 2) << 11;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 12;
            bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
            if (code)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template1 unoptimized");
            jbig2_image_set_pixel(image, x, y, bit);
        }
    }
    return 0;
}

static int
jbig2_decode_generic_template1(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    const uint32_t rowstride = image->stride;
    uint32_t x, y;
    byte *line2 = NULL;
    byte *line1 = NULL;
    byte *gbreg_line = (byte *) image->data;
    int code = 0;

#ifdef OUTPUT_PBM
    printf("P4\n%d %d\n", GBW, GBH);
#endif

    if (GBW <= 0)
        return 0;

    for (y = 0; y < GBH; y++) {
        uint32_t CONTEXT;
        uint32_t line_m1;
        uint32_t line_m2;
        uint32_t padded_width = (GBW + 7) & -8;

        line_m1 = line1 ? line1[0] : 0;
        line_m2 = line2 ? line2[0] << 5 : 0;
        CONTEXT = ((line_m1 >> 1) & 0x1f8) | ((line_m2 >> 1) & 0x1e00);

        /* 6.2.5.7 3d */
        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int x_minor;
            int minor_width = GBW - x > 8 ? 8 : GBW - x;

            if (line1)
                line_m1 = (line_m1 << 8) | (x + 8 < GBW ? line1[(x >> 3) + 1] : 0);

            if (line2)
                line_m2 = (line_m2 << 8) | (x + 8 < GBW ? line2[(x >> 3) + 1] << 5 : 0);

            /* This is the speed-critical inner loop. */
            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                bool bit;

                bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
                if (code)
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template1 optimized");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0xefb) << 1) | bit | ((line_m1 >> (8 - x_minor)) & 0x8) | ((line_m2 >> (8 - x_minor)) & 0x200);
            }
            gbreg_line[x >> 3] = result;
        }
#ifdef OUTPUT_PBM
        fwrite(gbreg_line, 1, rowstride, stdout);
#endif
        line2 = line1;
        line1 = gbreg_line;
        gbreg_line += rowstride;
    }

    return 0;
}

static int
jbig2_decode_generic_template2_unopt(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    uint32_t CONTEXT;
    uint32_t x, y;
    bool bit;
    int code = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    /* this version is generic and easy to understand, but very slow */

    for (y = 0; y < GBH; y++) {
        for (x = 0; x < GBW; x++) {
            if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                jbig2_image_set_pixel(image, x, y, 0);
                continue;
            }
            CONTEXT = 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y) << 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
            CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 2;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 3;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 4;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 5;
            CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 6;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 7;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 2) << 8;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 9;
            bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
            if (code)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template2 unoptimized");
            jbig2_image_set_pixel(image, x, y, bit);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_template2(Jbig2Ctx *ctx,
                                Jbig2Segment *segment,
                                const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    const uint32_t rowstride = image->stride;
    uint32_t x, y;
    byte *line2 = NULL;
    byte *line1 = NULL;
    byte *gbreg_line = (byte *) image->data;
    int code = 0;

#ifdef OUTPUT_PBM
    printf("P4\n%d %d\n", GBW, GBH);
#endif

    if (GBW <= 0)
        return 0;

    for (y = 0; y < GBH; y++) {
        uint32_t CONTEXT;
        uint32_t line_m1;
        uint32_t line_m2;
        uint32_t padded_width = (GBW + 7) & -8;

        line_m1 = line1 ? line1[0] : 0;
        line_m2 = line2 ? line2[0] << 4 : 0;
        CONTEXT = ((line_m1 >> 3) & 0x7c) | ((line_m2 >> 3) & 0x380);

        /* 6.2.5.7 3d */
        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int x_minor;
            int minor_width = GBW - x > 8 ? 8 : GBW - x;

            if (line1)
                line_m1 = (line_m1 << 8) | (x + 8 < GBW ? line1[(x >> 3) + 1] : 0);

            if (line2)
                line_m2 = (line_m2 << 8) | (x + 8 < GBW ? line2[(x >> 3) + 1] << 4 : 0);

            /* This is the speed-critical inner loop. */
            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                bool bit;

                bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
                if (code)
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template2 optimized");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0x1bd) << 1) | bit | ((line_m1 >> (10 - x_minor)) & 0x4) | ((line_m2 >> (10 - x_minor)) & 0x80);
            }
            gbreg_line[x >> 3] = result;
        }
#ifdef OUTPUT_PBM
        fwrite(gbreg_line, 1, rowstride, stdout);
#endif
        line2 = line1;
        line1 = gbreg_line;
        gbreg_line += rowstride;
    }

    return 0;
}

static int
jbig2_decode_generic_template3(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    const uint32_t rowstride = image->stride;
    byte *line1 = NULL;
    byte *gbreg_line = (byte *) image->data;
    uint32_t x, y;
    int code;

#ifdef OUTPUT_PBM
    printf("P4\n%d %d\n", GBW, GBH);
#endif

    if (GBW <= 0)
        return 0;

    for (y = 0; y < GBH; y++) {
        uint32_t CONTEXT;
        uint32_t line_m1;
        uint32_t padded_width = (GBW + 7) & -8;

        line_m1 = line1 ? line1[0] : 0;
        CONTEXT = (line_m1 >> 1) & 0x3f0;

        /* 6.2.5.7 3d */
        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int x_minor;
            int minor_width = GBW - x > 8 ? 8 : GBW - x;

            if (line1)
                line_m1 = (line_m1 << 8) | (x + 8 < GBW ? line1[(x >> 3) + 1] : 0);

            /* This is the speed-critical inner loop. */
            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                bool bit;

                bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
                if (code)
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template3 optimized");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0x1f7) << 1) | bit | ((line_m1 >> (8 - x_minor)) & 0x10);
            }
            gbreg_line[x >> 3] = result;
        }
#ifdef OUTPUT_PBM
        fwrite(gbreg_line, 1, rowstride, stdout);
#endif
        line1 = gbreg_line;
        gbreg_line += rowstride;
    }

    return 0;
}

static int
jbig2_decode_generic_template3_unopt(Jbig2Ctx *ctx,
                                     Jbig2Segment *segment,
                                     const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    uint32_t CONTEXT;
    uint32_t x, y;
    bool bit;
    int code = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    /* this version is generic and easy to understand, but very slow */

    for (y = 0; y < GBH; y++) {
        for (x = 0; x < GBW; x++) {
            if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                jbig2_image_set_pixel(image, x, y, 0);
                continue;
            }
            CONTEXT = 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y) << 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
            CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
            CONTEXT |= jbig2_image_get_pixel(image, x - 4, y) << 3;
            CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 4;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 5;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 6;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 7;
            CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 8;
            CONTEXT |= jbig2_image_get_pixel(image, x - 3, y - 1) << 9;
            bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
            if (code)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template3 unoptimized");
            jbig2_image_set_pixel(image, x, y, bit);
        }
    }
    return 0;
}

static void
copy_prev_row(Jbig2Image *image, int row)
{
    if (!row) {
        /* no previous row */
        memset(image->data, 0, image->stride);
    } else {
        /* duplicate data from the previous row */
        uint8_t *src = image->data + (row - 1) * image->stride;

        memcpy(src + image->stride, src, image->stride);
    }
}

static int
jbig2_decode_generic_template0_TPGDON(Jbig2Ctx *ctx,
                                      Jbig2Segment *segment,
                                      const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    uint32_t CONTEXT;
    uint32_t x, y;
    bool bit;
    int LTP = 0;
    int code = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]) ||
        pixel_outside_field(params->gbat[2], params->gbat[3]) ||
        pixel_outside_field(params->gbat[4], params->gbat[5]) ||
        pixel_outside_field(params->gbat[6], params->gbat[7]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        LTP ^= jbig2_arith_decode(as, &GB_stats[0x9B25], &code);
        if (code)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template0 TPGDON1");
        if (!LTP) {
            for (x = 0; x < GBW; x++) {
                if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                    jbig2_image_set_pixel(image, x, y, 0);
                    continue;
                }
                CONTEXT = jbig2_image_get_pixel(image, x - 1, y);
                CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
                CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
                CONTEXT |= jbig2_image_get_pixel(image, x - 4, y) << 3;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 4;
                CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 1) << 5;
                CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 6;
                CONTEXT |= jbig2_image_get_pixel(image, x, y - 1) << 7;
                CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 8;
                CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 9;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[2], y + params->gbat[3]) << 10;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[4], y + params->gbat[5]) << 11;
                CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 12;
                CONTEXT |= jbig2_image_get_pixel(image, x, y - 2) << 13;
                CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 14;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[6], y + params->gbat[7]) << 15;
                bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
                if (code)
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template0 TPGDON2");
                jbig2_image_set_pixel(image, x, y, bit);
            }
        } else {
            copy_prev_row(image, y);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_template1_TPGDON(Jbig2Ctx *ctx,
                                      Jbig2Segment *segment,
                                      const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    uint32_t CONTEXT;
    uint32_t x, y;
    bool bit;
    int LTP = 0;
    int code = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        LTP ^= jbig2_arith_decode(as, &GB_stats[0x0795], &code);
        if (code)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template1 TPGDON1");
        if (!LTP) {
            for (x = 0; x < GBW; x++) {
                if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                    jbig2_image_set_pixel(image, x, y, 0);
                    continue;
                }
                CONTEXT = jbig2_image_get_pixel(image, x - 1, y);
                CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
                CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 3;
                CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 1) << 4;
                CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 5;
                CONTEXT |= jbig2_image_get_pixel(image, x, y - 1) << 6;
                CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 7;
                CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 8;
                CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 2) << 9;
                CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 10;
                CONTEXT |= jbig2_image_get_pixel(image, x, y - 2) << 11;
                CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 12;
                bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
                if (code)
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template1 TPGDON2");
                jbig2_image_set_pixel(image, x, y, bit);
            }
        } else {
            copy_prev_row(image, y);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_template2_TPGDON(Jbig2Ctx *ctx,
                                      Jbig2Segment *segment,
                                      const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    uint32_t CONTEXT;
    uint32_t x, y;
    bool bit;
    int LTP = 0;
    int code = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        LTP ^= jbig2_arith_decode(as, &GB_stats[0xE5], &code);
        if (code)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template2 TPGDON1");
        if (!LTP) {
            for (x = 0; x < GBW; x++) {
                if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                    jbig2_image_set_pixel(image, x, y, 0);
                    continue;
                }
                CONTEXT = jbig2_image_get_pixel(image, x - 1, y);
                CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 2;
                CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 3;
                CONTEXT |= jbig2_image_get_pixel(image, x, y - 1) << 4;
                CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 5;
                CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 6;
                CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 7;
                CONTEXT |= jbig2_image_get_pixel(image, x, y - 2) << 8;
                CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 9;
                bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
                if (code)
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template2 TPGDON2");
                jbig2_image_set_pixel(image, x, y, bit);
            }
        } else {
            copy_prev_row(image, y);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_template3_TPGDON(Jbig2Ctx *ctx,
                                      Jbig2Segment *segment,
                                      const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const uint32_t GBW = image->width;
    const uint32_t GBH = image->height;
    uint32_t CONTEXT;
    uint32_t x, y;
    bool bit;
    int LTP = 0;
    int code = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        LTP ^= jbig2_arith_decode(as, &GB_stats[0x0195], &code);
        if (code)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template3 TPGDON1");
        if (!LTP) {
            for (x = 0; x < GBW; x++) {
                if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                    jbig2_image_set_pixel(image, x, y, 0);
                    continue;
                }
                CONTEXT = jbig2_image_get_pixel(image, x - 1, y);
                CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
                CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
                CONTEXT |= jbig2_image_get_pixel(image, x - 4, y) << 3;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 4;
                CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 5;
                CONTEXT |= jbig2_image_get_pixel(image, x, y - 1) << 6;
                CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 7;
                CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 8;
                CONTEXT |= jbig2_image_get_pixel(image, x - 3, y - 1) << 9;
                bit = jbig2_arith_decode(as, &GB_stats[CONTEXT], &code);
                if (code)
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to decode arithmetic code when handling generic template3 TPGDON2");
                jbig2_image_set_pixel(image, x, y, bit);
            }
        } else {
            copy_prev_row(image, y);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_region_TPGDON(Jbig2Ctx *ctx,
                                   Jbig2Segment *segment,
                                   const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    switch (params->GBTEMPLATE) {
    case 0:
        return jbig2_decode_generic_template0_TPGDON(ctx, segment, params, as, image, GB_stats);
    case 1:
        return jbig2_decode_generic_template1_TPGDON(ctx, segment, params, as, image, GB_stats);
    case 2:
        return jbig2_decode_generic_template2_TPGDON(ctx, segment, params, as, image, GB_stats);
    case 3:
        return jbig2_decode_generic_template3_TPGDON(ctx, segment, params, as, image, GB_stats);
    }

    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "unsupported GBTEMPLATE (%d)", params->GBTEMPLATE);
}

/**
 * jbig2_decode_generic_region: Decode a generic region.
 * @ctx: The context for allocation and error reporting.
 * @segment: A segment reference for error reporting.
 * @params: Decoding parameter set.
 * @as: Arithmetic decoder state.
 * @image: Where to store the decoded data.
 * @GB_stats: Arithmetic stats.
 *
 * Decodes a generic region, according to section 6.2. The caller should
 * pass an already allocated Jbig2Image object for @image
 *
 * Because this API is based on an arithmetic decoding state, it is
 * not suitable for MMR decoding.
 *
 * Return code: 0 on success.
 **/
int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
                            Jbig2Segment *segment, const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int8_t *gbat = params->gbat;

    if (image->stride * image->height > (1 << 26) && segment->data_length < image->stride * image->height / (1 << 16)) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "region is far larger than data provided (%d << %d), aborting to prevent DOS", segment->data_length, image->stride * image->height);
    }

    if (!params->MMR && params->TPGDON)
        return jbig2_decode_generic_region_TPGDON(ctx, segment, params, as, image, GB_stats);

    if (!params->MMR && params->GBTEMPLATE == 0) {
        if (!params->USESKIP && gbat[0] == +3 && gbat[1] == -1 && gbat[2] == -3 && gbat[3] == -1 && gbat[4] == +2 && gbat[5] == -2 && gbat[6] == -2 && gbat[7] == -2)
            return jbig2_decode_generic_template0(ctx, segment, params, as, image, GB_stats);
        else
            return jbig2_decode_generic_template0_unopt(ctx, segment, params, as, image, GB_stats);
    } else if (!params->MMR && params->GBTEMPLATE == 1) {
        if (!params->USESKIP && gbat[0] == +3 && gbat[1] == -1)
            return jbig2_decode_generic_template1(ctx, segment, params, as, image, GB_stats);
        else
            return jbig2_decode_generic_template1_unopt(ctx, segment, params, as, image, GB_stats);
    }
    else if (!params->MMR && params->GBTEMPLATE == 2) {
        if (!params->USESKIP && gbat[0] == 2 && gbat[1] == -1)
            return jbig2_decode_generic_template2(ctx, segment, params, as, image, GB_stats);
        else
            return jbig2_decode_generic_template2_unopt(ctx, segment, params, as, image, GB_stats);
    } else if (!params->MMR && params->GBTEMPLATE == 3) {
        if (!params->USESKIP && gbat[0] == 2 && gbat[1] == -1)
            return jbig2_decode_generic_template3(ctx, segment, params, as, image, GB_stats);
        else
            return jbig2_decode_generic_template3_unopt(ctx, segment, params, as, image, GB_stats);
    }

    {
        int i;

        for (i = 0; i < 8; i++)
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "gbat[%d] = %d", i, params->gbat[i]);
    }

    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "unsupported generic region (MMR=%d, GBTEMPLATE=%d)", params->MMR, params->GBTEMPLATE);
}

/**
 * Handler for immediate generic region segments
 */
int
jbig2_immediate_generic_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    Jbig2RegionSegmentInfo rsi;
    byte seg_flags;
    int8_t gbat[8];
    int offset;
    uint32_t gbat_bytes = 0;
    Jbig2GenericRegionParams params;
    int code = 0;
    Jbig2Image *image = NULL;
    Jbig2WordStream *ws = NULL;
    Jbig2ArithState *as = NULL;
    Jbig2ArithCx *GB_stats = NULL;
    uint32_t height;
    Jbig2Page *page = &ctx->pages[ctx->current_page];

    /* 7.4.6 */
    if (segment->data_length < 18)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");

    jbig2_get_region_segment_info(&rsi, segment_data);
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "generic region: %u x %u @ (%u, %u), flags = %02x", rsi.width, rsi.height, rsi.x, rsi.y, rsi.flags);

    /* 7.4.6.4 */
    height = rsi.height;
    if (segment->rows != UINT32_MAX) {
        if (segment->rows > rsi.height)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment contains more rows than stated in header");
        height = segment->rows;
    }

    /* 7.4.6.2 */
    seg_flags = segment_data[17];
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "segment flags = %02x", seg_flags);
    if ((seg_flags & 1) && (seg_flags & 6))
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "MMR is 1, but GBTEMPLATE is not 0");

    /* 7.4.6.3 */
    if (!(seg_flags & 1)) {
        gbat_bytes = (seg_flags & 6) ? 2 : 8;
        if (18 + gbat_bytes > segment->data_length)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
        memcpy(gbat, segment_data + 18, gbat_bytes);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "gbat: %d, %d", gbat[0], gbat[1]);
    }

    offset = 18 + gbat_bytes;

    /* Check for T.88 amendment 2 */
    if ((seg_flags >> 5) & 1)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment uses 12 adaptive template pixels (NYI)");

    /* Table 34 */
    params.MMR = seg_flags & 1;
    params.GBTEMPLATE = (seg_flags & 6) >> 1;
    params.TPGDON = (seg_flags & 8) >> 3;
    params.USESKIP = 0;
    memcpy(params.gbat, gbat, gbat_bytes);

    if (page->height == 0xffffffff && page->striped && page->stripe_size > 0) {
        if (rsi.y >= page->end_row + page->stripe_size) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "ignoring %u x %u region at (%u, %u) outside of stripe at row %u covering %u rows, on page of height %u", rsi.width, rsi.height, rsi.x, rsi.y, page->end_row, page->stripe_size, page->image->height);
            return 0;
        }
        if (height > page->end_row + page->stripe_size) {
            height = page->end_row + page->stripe_size;
        }
    } else {
        if (rsi.y >= page->height) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "ignoring %u x %u region at (%u, %u) outside of page of height %u", rsi.width, rsi.height, rsi.x, rsi.y, page->height);
            return 0;
        }
        if (height > page->height - rsi .y) {
            height = page->height - rsi.y;
        }
    }
    if (height == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "nothing remains of region, ignoring");
        return 0;
    }

    image = jbig2_image_new(ctx, rsi.width, height);
    if (image == NULL)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate generic image");
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "allocated %d x %d image buffer for region decode results", rsi.width, height);

    if (params.MMR) {
        code = jbig2_decode_generic_mmr(ctx, segment, &params, segment_data + offset, segment->data_length - offset, image);
        if (code < 0) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode MMR-coded generic region");
            goto cleanup;
        }
    } else {
        int stats_size = jbig2_generic_stats_size(ctx, params.GBTEMPLATE);

        GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
        if (GB_stats == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate arithmetic decoder states when handling immediate generic region");
            goto cleanup;
        }
        memset(GB_stats, 0, stats_size);

        ws = jbig2_word_stream_buf_new(ctx, segment_data + offset, segment->data_length - offset);
        if (ws == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocated word stream when handling immediate generic region");
            goto cleanup;
        }
        as = jbig2_arith_new(ctx, ws);
        if (as == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate arithmetic coding state when handling immediate generic region");
            goto cleanup;
        }
        code = jbig2_decode_generic_region(ctx, segment, &params, as, image, GB_stats);
        if (code < 0) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode immediate generic region");
            goto cleanup;
        }
    }

    code = jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page], image, rsi.x, rsi.y, rsi.op);
    if (code < 0)
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unable to add result to page");

cleanup:
    jbig2_free(ctx->allocator, as);
    jbig2_word_stream_buf_free(ctx, ws);
    jbig2_free(ctx->allocator, GB_stats);
    jbig2_image_release(ctx, image);

    return code;
}
