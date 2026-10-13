/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*!
 *     Low-level fast hit-miss transform with auto-generated sels
 *
 *      Dispatcher:
 *             l_int32    fhmtgen_low_1()
 *
 *      Static Low-level:
 *             void       fhmt_1_*()
 */

#include "allheaders.h"

static void  fhmt_1_0(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_1(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_2(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_3(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_4(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_5(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_6(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_7(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_8(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);
static void  fhmt_1_9(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);


/*---------------------------------------------------------------------*
 *                           Fast hmt dispatcher                       *
 *---------------------------------------------------------------------*/
/*!
 *  fhmtgen_low_1()
 *
 *       a dispatcher to appropriate low-level code
 */
l_int32
fhmtgen_low_1(l_uint32  *datad,
              l_int32    w,
              l_int32    h,
              l_int32    wpld,
              l_uint32  *datas,
              l_int32    wpls,
              l_int32    index)
{

    switch (index)
    {
    case 0:
        fhmt_1_0(datad, w, h, wpld, datas, wpls);
        break;
    case 1:
        fhmt_1_1(datad, w, h, wpld, datas, wpls);
        break;
    case 2:
        fhmt_1_2(datad, w, h, wpld, datas, wpls);
        break;
    case 3:
        fhmt_1_3(datad, w, h, wpld, datas, wpls);
        break;
    case 4:
        fhmt_1_4(datad, w, h, wpld, datas, wpls);
        break;
    case 5:
        fhmt_1_5(datad, w, h, wpld, datas, wpls);
        break;
    case 6:
        fhmt_1_6(datad, w, h, wpld, datas, wpls);
        break;
    case 7:
        fhmt_1_7(datad, w, h, wpld, datas, wpls);
        break;
    case 8:
        fhmt_1_8(datad, w, h, wpld, datas, wpls);
        break;
    case 9:
        fhmt_1_9(datad, w, h, wpld, datas, wpls);
        break;
    }

    return 0;
}


/*--------------------------------------------------------------------------*
 *                 Low-level auto-generated static routines                 *
 *--------------------------------------------------------------------------*/
/*
 *  N.B.  In all the low-level routines, the part of the image
 *        that is accessed has been clipped by 32 pixels on
 *        all four sides.  This is done in the higher level
 *        code by redefining w and h smaller and by moving the
 *        start-of-image pointers up to the beginning of this
 *        interior rectangle.
 */
static void
fhmt_1_0(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((~*(sptr - wpls) >> 1) | (~*(sptr - wpls - 1) << 31)) &
                    (~*(sptr - wpls)) &
                    ((~*(sptr - wpls) << 1) | (~*(sptr - wpls + 1) >> 31)) &
                    ((~*(sptr) >> 1) | (~*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((~*(sptr) << 1) | (~*(sptr + 1) >> 31)) &
                    ((~*(sptr + wpls) >> 1) | (~*(sptr + wpls - 1) << 31)) &
                    (~*(sptr + wpls)) &
                    ((~*(sptr + wpls) << 1) | (~*(sptr + wpls + 1) >> 31));
        }
    }
}

static void
fhmt_1_1(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31)) &
                    ((~*(sptr + wpls) >> 1) | (~*(sptr + wpls - 1) << 31)) &
                    (~*(sptr + wpls)) &
                    ((~*(sptr + wpls) << 1) | (~*(sptr + wpls + 1) >> 31));
        }
    }
}

static void
fhmt_1_2(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((~*(sptr - wpls) >> 1) | (~*(sptr - wpls - 1) << 31)) &
                    (~*(sptr - wpls)) &
                    ((~*(sptr - wpls) << 1) | (~*(sptr - wpls + 1) >> 31)) &
                    ((*(sptr) >> 1) | (*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((*(sptr) << 1) | (*(sptr + 1) >> 31));
        }
    }
}

static void
fhmt_1_3(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = (*(sptr - wpls)) &
                    ((~*(sptr - wpls) << 1) | (~*(sptr - wpls + 1) >> 31)) &
                    (*sptr) &
                    ((~*(sptr) << 1) | (~*(sptr + 1) >> 31)) &
                    (*(sptr + wpls)) &
                    ((~*(sptr + wpls) << 1) | (~*(sptr + wpls + 1) >> 31));
        }
    }
}

static void
fhmt_1_4(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;

    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((~*(sptr - wpls) >> 1) | (~*(sptr - wpls - 1) << 31)) &
                    (*(sptr - wpls)) &
                    ((~*(sptr) >> 1) | (~*(sptr - 1) << 31)) &
                    (*sptr) &
                    ((~*(sptr + wpls) >> 1) | (~*(sptr + wpls - 1) << 31)) &
                    (*(sptr + wpls));
        }
    }
}

static void
fhmt_1_5(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2, wpls3, wpls4;
l_int32             wpls5, wpls6;

    wpls2 = 2 * wpls;
    wpls3 = 3 * wpls;
    wpls4 = 4 * wpls;
    wpls5 = 5 * wpls;
    wpls6 = 6 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((~*(sptr - wpls6) << 1) | (~*(sptr - wpls6 + 1) >> 31)) &
                    ((*(sptr - wpls6) << 3) | (*(sptr - wpls6 + 1) >> 29)) &
                    (~*(sptr - wpls2)) &
                    ((*(sptr - wpls2) << 2) | (*(sptr - wpls2 + 1) >> 30)) &
                    ((~*(sptr + wpls2) >> 1) | (~*(sptr + wpls2 - 1) << 31)) &
                    ((*(sptr + wpls2) << 1) | (*(sptr + wpls2 + 1) >> 31)) &
                    ((~*(sptr + wpls6) >> 2) | (~*(sptr + wpls6 - 1) << 30)) &
                    (*(sptr + wpls6));
        }
    }
}

static void
fhmt_1_6(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((~*(sptr - wpls) >> 1) | (~*(sptr - wpls - 1) << 31)) &
                    (~*(sptr - wpls)) &
                    ((~*(sptr - wpls) << 1) | (~*(sptr - wpls + 1) >> 31)) &
                    ((~*(sptr - wpls) << 2) | (~*(sptr - wpls + 1) >> 30)) &
                    ((~*(sptr) >> 1) | (~*(sptr - 1) << 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((~*(sptr + wpls) >> 1) | (~*(sptr + wpls - 1) << 31)) &
                    ((*(sptr + wpls) << 1) | (*(sptr + wpls + 1) >> 31)) &
                    ((*(sptr + wpls) << 2) | (*(sptr + wpls + 1) >> 30)) &
                    ((~*(sptr + wpls2) >> 1) | (~*(sptr + wpls2 - 1) << 31)) &
                    (*(sptr + wpls2)) &
                    ((*(sptr + wpls2) << 1) | (*(sptr + wpls2 + 1) >> 31)) &
                    ((*(sptr + wpls2) << 2) | (*(sptr + wpls2 + 1) >> 30));
        }
    }
}

static void
fhmt_1_7(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((~*(sptr - wpls) >> 2) | (~*(sptr - wpls - 1) << 30)) &
                    ((~*(sptr - wpls) >> 1) | (~*(sptr - wpls - 1) << 31)) &
                    (~*(sptr - wpls)) &
                    ((~*(sptr - wpls) << 1) | (~*(sptr - wpls + 1) >> 31)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((~*(sptr) << 1) | (~*(sptr + 1) >> 31)) &
                    ((*(sptr + wpls) >> 2) | (*(sptr + wpls - 1) << 30)) &
                    ((*(sptr + wpls) >> 1) | (*(sptr + wpls - 1) << 31)) &
                    ((~*(sptr + wpls) << 1) | (~*(sptr + wpls + 1) >> 31)) &
                    ((*(sptr + wpls2) >> 2) | (*(sptr + wpls2 - 1) << 30)) &
                    ((*(sptr + wpls2) >> 1) | (*(sptr + wpls2 - 1) << 31)) &
                    (*(sptr + wpls2)) &
                    ((~*(sptr + wpls2) << 1) | (~*(sptr + wpls2 + 1) >> 31));
        }
    }
}

static void
fhmt_1_8(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((~*(sptr - wpls2) >> 1) | (~*(sptr - wpls2 - 1) << 31)) &
                    (*(sptr - wpls2)) &
                    ((*(sptr - wpls2) << 1) | (*(sptr - wpls2 + 1) >> 31)) &
                    ((*(sptr - wpls2) << 2) | (*(sptr - wpls2 + 1) >> 30)) &
                    ((~*(sptr - wpls) >> 1) | (~*(sptr - wpls - 1) << 31)) &
                    ((*(sptr - wpls) << 1) | (*(sptr - wpls + 1) >> 31)) &
                    ((*(sptr - wpls) << 2) | (*(sptr - wpls + 1) >> 30)) &
                    ((~*(sptr) >> 1) | (~*(sptr - 1) << 31)) &
                    ((*(sptr) << 2) | (*(sptr + 1) >> 30)) &
                    ((~*(sptr + wpls) >> 1) | (~*(sptr + wpls - 1) << 31)) &
                    (~*(sptr + wpls)) &
                    ((~*(sptr + wpls) << 1) | (~*(sptr + wpls + 1) >> 31)) &
                    ((~*(sptr + wpls) << 2) | (~*(sptr + wpls + 1) >> 30));
        }
    }
}

static void
fhmt_1_9(l_uint32  *datad,
         l_int32    w,
         l_int32    h,
         l_int32    wpld,
         l_uint32  *datas,
         l_int32    wpls)
{
l_int32    i;
l_int32    j, pwpls;
l_uint32  *sptr, *dptr;
l_int32             wpls2;

    wpls2 = 2 * wpls;
    pwpls = (l_uint32)(w + 31) / 32;  /* proper wpl of src */

    for (i = 0; i < h; i++) {
        sptr = datas + i * wpls;
        dptr = datad + i * wpld;
        for (j = 0; j < pwpls; j++, sptr++, dptr++) {
            *dptr = ((*(sptr - wpls2) >> 2) | (*(sptr - wpls2 - 1) << 30)) &
                    ((*(sptr - wpls2) >> 1) | (*(sptr - wpls2 - 1) << 31)) &
                    (*(sptr - wpls2)) &
                    ((~*(sptr - wpls2) << 1) | (~*(sptr - wpls2 + 1) >> 31)) &
                    ((*(sptr - wpls) >> 2) | (*(sptr - wpls - 1) << 30)) &
                    ((*(sptr - wpls) >> 1) | (*(sptr - wpls - 1) << 31)) &
                    ((~*(sptr - wpls) << 1) | (~*(sptr - wpls + 1) >> 31)) &
                    ((*(sptr) >> 2) | (*(sptr - 1) << 30)) &
                    ((~*(sptr) << 1) | (~*(sptr + 1) >> 31)) &
                    ((~*(sptr + wpls) >> 2) | (~*(sptr + wpls - 1) << 30)) &
                    ((~*(sptr + wpls) >> 1) | (~*(sptr + wpls - 1) << 31)) &
                    (~*(sptr + wpls)) &
                    ((~*(sptr + wpls) << 1) | (~*(sptr + wpls + 1) >> 31));
        }
    }
}

