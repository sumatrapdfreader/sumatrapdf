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
 * \file roplow.c
 * <pre>
 *      Low level dest-only
 *           void            rasteropUniLow()
 *           static void     rasteropUniWordAlignedlLow()
 *           static void     rasteropUniGeneralLow()
 *
 *      Low level src and dest
 *           void            rasteropLow()
 *           static void     rasteropWordAlignedLow()
 *           static void     rasteropVAlignedLow()
 *           static void     rasteropGeneralLow()
 *
 *      Low level in-place full height vertical block transfer
 *           void            rasteropVipLow()
 *
 *      Low level in-place full width horizontal block transfer
 *           void            rasteropHipLow()
 *           static void     shiftDataHorizontalLow()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

    /* Static helpers */
static void rasteropUniWordAlignedLow(l_uint32 *datad, l_int32 dwpl, l_int32 dx,
                                      l_int32 dy, l_int32  dw, l_int32 dh,
                                      l_int32 op);
static void rasteropUniGeneralLow(l_uint32 *datad, l_int32 dwpl, l_int32 dx,
                                  l_int32 dy, l_int32 dw, l_int32  dh,
                                  l_int32 op);
static void rasteropWordAlignedLow(l_uint32 *datad, l_int32 dwpl, l_int32 dx,
                                   l_int32 dy, l_int32 dw, l_int32 dh,
                                   l_int32 op, l_uint32 *datas, l_int32 swpl,
                                   l_int32 sx, l_int32 sy);
static void rasteropVAlignedLow(l_uint32 *datad, l_int32 dwpl, l_int32 dx,
                                l_int32 dy, l_int32 dw, l_int32 dh,
                                l_int32 op, l_uint32 *datas, l_int32 swpl,
                                l_int32 sx, l_int32 sy);
static void rasteropGeneralLow(l_uint32 *datad, l_int32 dwpl, l_int32 dx,
                               l_int32 dy, l_int32 dw, l_int32 dh,
                               l_int32 op, l_uint32 *datas, l_int32 swpl,
                               l_int32 sx, l_int32 sy);
static void shiftDataHorizontalLow(l_uint32 *datad, l_int32 wpld,
                                   l_uint32 *datas, l_int32 wpls,
                                   l_int32 shift);

#define COMBINE_PARTIAL(d, s, m)     ( ((d) & ~(m)) | ((s) & (m)) )

static const l_int32  SHIFT_LEFT  = 0;
static const l_int32  SHIFT_RIGHT = 1;

static const l_uint32 lmask32[] = {0x0,
    0x80000000, 0xc0000000, 0xe0000000, 0xf0000000,
    0xf8000000, 0xfc000000, 0xfe000000, 0xff000000,
    0xff800000, 0xffc00000, 0xffe00000, 0xfff00000,
    0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000,
    0xffff8000, 0xffffc000, 0xffffe000, 0xfffff000,
    0xfffff800, 0xfffffc00, 0xfffffe00, 0xffffff00,
    0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0,
    0xfffffff8, 0xfffffffc, 0xfffffffe, 0xffffffff};

static const l_uint32 rmask32[] = {0x0,
    0x00000001, 0x00000003, 0x00000007, 0x0000000f,
    0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
    0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
    0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
    0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
    0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
    0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
    0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};


/*--------------------------------------------------------------------*
 *                     Low-level dest-only rasterops                  *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropUniLow()
 *
 * \param[in]    datad  ptr to dest image data
 * \param[in]    dpixw  width of dest
 * \param[in]    dpixh  height of dest
 * \param[in]    depth  depth of src and dest
 * \param[in]    dwpl   wpl of dest
 * \param[in]    dx     x val of UL corner of dest rectangle
 * \param[in]    dy     y val of UL corner of dest rectangle
 * \param[in]    dw     width of dest rectangle
 * \param[in]    dh     height of dest rectangle
 * \param[in]    op     op code
 * \return  void
 *
 *  Action: scales width, performs clipping, checks alignment, and
 *          dispatches for the rasterop.
 */
void
rasteropUniLow(l_uint32  *datad,
               l_int32    dpixw,
               l_int32    dpixh,
               l_int32    depth,
               l_int32    dwpl,
               l_int32    dx,
               l_int32    dy,
               l_int32    dw,
               l_int32    dh,
               l_int32    op)
{
l_int32  dhangw, dhangh;

   /* -------------------------------------------------------*
    *            scale horizontal dimensions by depth
    * -------------------------------------------------------*/
    if (depth != 1) {
        dpixw *= depth;
        dx *= depth;
        dw *= depth;
    }

   /* -------------------------------------------------------*
    *            clip rectangle to dest image
    * -------------------------------------------------------*/
       /* first, clip horizontally (dx, dw) */
    if (dx < 0) {
        dw += dx;  /* reduce dw */
        dx = 0;
    }
    dhangw = dx + dw - dpixw;  /* rect ovhang dest to right */
    if (dhangw > 0)
        dw -= dhangw;  /* reduce dw */

       /* then, clip vertically (dy, dh) */
    if (dy < 0) {
        dh += dy;  /* reduce dh */
        dy = 0;
    }
    dhangh = dy + dh - dpixh;  /* rect ovhang dest below */
    if (dhangh > 0)
        dh -= dhangh;  /* reduce dh */

        /* if clipped entirely, quit */
    if ((dw <= 0) || (dh <= 0))
        return;

   /* -------------------------------------------------------*
    *       dispatch to aligned or non-aligned blitters
    * -------------------------------------------------------*/
    if ((dx & 31) == 0)
        rasteropUniWordAlignedLow(datad, dwpl, dx, dy, dw, dh, op);
    else
        rasteropUniGeneralLow(datad, dwpl, dx, dy, dw, dh, op);
}



/*--------------------------------------------------------------------*
 *           Static low-level uni rasterop with word alignment        *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropUniWordAlignedLow()
 *
 * \param[in]    datad  ptr to dest image data
 * \param[in]    dwpl   wpl of dest
 * \param[in]    dx     x val of UL corner of dest rectangle
 * \param[in]    dy     y val of UL corner of dest rectangle
 * \param[in]    dw     width of dest rectangle
 * \param[in]    dh     height of dest rectangle
 * \param[in]    op     op code
 * \return  void
 *
 *  This is called when the dest rect is left aligned
 *  on 32-bit word boundaries.   That is: dx & 31 == 0.
 *
 *  We make an optimized implementation of this because
 *  it is a common case: e.g., operating on a full dest image.
 */
static void
rasteropUniWordAlignedLow(l_uint32  *datad,
                          l_int32    dwpl,
                          l_int32    dx,
                          l_int32    dy,
                          l_int32    dw,
                          l_int32    dh,
                          l_int32    op)
{
l_int32    nfullw;     /* number of full words */
l_uint32  *pfword;     /* ptr to first word */
l_int32    lwbits;     /* number of ovrhang bits in last partial word */
l_uint32   lwmask;     /* mask for last partial word */
l_uint32  *lined;
l_int32    i, j;

    /*--------------------------------------------------------*
     *                Preliminary calculations                *
     *--------------------------------------------------------*/
    nfullw = dw >> 5;
    lwbits = dw & 31;
    if (lwbits)
        lwmask = lmask32[lwbits];
    pfword = datad + dwpl * dy + (dx >> 5);


    /*--------------------------------------------------------*
     *            Now we're ready to do the ops               *
     *--------------------------------------------------------*/
    switch (op)
    {
    case PIX_CLR:
        for (i = 0; i < dh; i++) {
            lined = pfword + i * dwpl;
            for (j = 0; j < nfullw; j++)
                *lined++ = 0x0;
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, 0x0, lwmask);
        }
        break;
    case PIX_SET:
        for (i = 0; i < dh; i++) {
            lined = pfword + i * dwpl;
            for (j = 0; j < nfullw; j++)
                *lined++ = 0xffffffff;
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, 0xffffffff, lwmask);
        }
        break;
    case PIX_NOT(PIX_DST):
        for (i = 0; i < dh; i++) {
            lined = pfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = ~(*lined);
                lined++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, ~(*lined), lwmask);
        }
        break;
    default:
        lept_stderr("Operation %d not permitted here!\n", op);
    }
}


/*--------------------------------------------------------------------*
 *        Static low-level uni rasterop without word alignment        *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropUniGeneralLow()
 *
 * \param[in]    datad  ptr to dest image data
 * \param[in]    dwpl   wpl of dest
 * \param[in]    dx     x val of UL corner of dest rectangle
 * \param[in]    dy     y val of UL corner of dest rectangle
 * \param[in]    dw     width of dest rectangle
 * \param[in]    dh     height of dest rectangle
 * \param[in]    op     op code
 * \return  void
 */
static void
rasteropUniGeneralLow(l_uint32  *datad,
                      l_int32    dwpl,
                      l_int32    dx,
                      l_int32    dy,
                      l_int32    dw,
                      l_int32    dh,
                      l_int32    op)
{
l_int32    dfwpartb;   /* boolean (1, 0) if first dest word is partial */
l_int32    dfwpart2b;  /* boolean (1, 0) if first dest word is doubly partial */
l_uint32   dfwmask;    /* mask for first partial dest word */
l_int32    dfwbits;    /* first word dest bits in ovrhang */
l_uint32  *pdfwpart = NULL;   /* ptr to first partial dest word */
l_int32    dfwfullb;   /* boolean (1, 0) if there exists a full dest word */
l_int32    dnfullw;    /* number of full words in dest */
l_uint32  *pdfwfull = NULL;   /* ptr to first full dest word */
l_int32    dlwpartb;   /* boolean (1, 0) if last dest word is partial */
l_uint32   dlwmask;    /* mask for last partial dest word */
l_int32    dlwbits;    /* last word dest bits in ovrhang */
l_uint32  *pdlwpart = NULL;   /* ptr to last partial dest word */
l_int32    i, j;


    /*--------------------------------------------------------*
     *                Preliminary calculations                *
     *--------------------------------------------------------*/
        /* is the first word partial? */
    dfwmask = 0;
    if ((dx & 31) == 0) {  /* if not */
        dfwpartb = 0;
        dfwbits = 0;
    } else {  /* if so */
        dfwpartb = 1;
        dfwbits = 32 - (dx & 31);
        dfwmask = rmask32[dfwbits];
        pdfwpart = datad + dwpl * dy + (dx >> 5);
    }

        /* is the first word doubly partial? */
    if (dw >= dfwbits) {  /* if not */
        dfwpart2b = 0;
    } else {  /* if so */
        dfwpart2b = 1;
        dfwmask &= lmask32[32 - dfwbits + dw];
    }

        /* is there a full dest word? */
    if (dfwpart2b == 1) {  /* not */
        dfwfullb = 0;
        dnfullw = 0;
    } else {
        dnfullw = (dw - dfwbits) >> 5;
        if (dnfullw == 0) {  /* if not */
            dfwfullb = 0;
        } else {  /* if so */
            dfwfullb = 1;
            if (dfwpartb)
                pdfwfull = pdfwpart + 1;
            else
                pdfwfull = datad + dwpl * dy + (dx >> 5);
        }
    }

        /* is the last word partial? */
    dlwbits = (dx + dw) & 31;
    if (dfwpart2b == 1 || dlwbits == 0) {  /* if not */
        dlwpartb = 0;
    } else {
        dlwpartb = 1;
        dlwmask = lmask32[dlwbits];
        if (dfwpartb)
            pdlwpart = pdfwpart + 1 + dnfullw;
        else
            pdlwpart = datad + dwpl * dy + (dx >> 5) + dnfullw;
    }


    /*--------------------------------------------------------*
     *            Now we're ready to do the ops               *
     *--------------------------------------------------------*/
    switch (op)
    {
    case PIX_CLR:
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart, 0x0, dfwmask);
                pdfwpart += dwpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = 0x0;
                pdfwfull += dwpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart, 0x0, dlwmask);
                pdlwpart += dwpl;
            }
        }
        break;
    case PIX_SET:
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart, 0xffffffff, dfwmask);
                pdfwpart += dwpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = 0xffffffff;
                pdfwfull += dwpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart, 0xffffffff, dlwmask);
                pdlwpart += dwpl;
            }
        }
        break;
    case PIX_NOT(PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart, ~(*pdfwpart), dfwmask);
                pdfwpart += dwpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = ~(*(pdfwfull + j));
                pdfwfull += dwpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart, ~(*pdlwpart), dlwmask);
                pdlwpart += dwpl;
            }
        }
        break;
    default:
        lept_stderr("Operation %d not permitted here!\n", op);
    }
}


/*--------------------------------------------------------------------*
 *                   Low-level src and dest rasterops                 *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropLow()
 *
 * \param[in]    datad  ptr to dest image data
 * \param[in]    dpixw  width of dest
 * \param[in]    dpixh  height of dest
 * \param[in]    depth  depth of src and dest
 * \param[in]    dwpl   wpl of dest
 * \param[in]    dx     x val of UL corner of dest rectangle
 * \param[in]    dy     y val of UL corner of dest rectangle
 * \param[in]    dw     width of dest rectangle
 * \param[in]    dh     height of dest rectangle
 * \param[in]    op     op code
 * \param[in]    datas  ptr to src image data
 * \param[in]    spixw  width of src
 * \param[in]    spixh  height of src
 * \param[in]    swpl   wpl of src
 * \param[in]    sx     x val of UL corner of src rectangle
 * \param[in]    sy     y val of UL corner of src rectangle
 * \return  void
 *
 *  Action: Scales width, performs clipping, checks alignment and
 *          dispatches for the rasterop.
 *
 *  Warning: the two images must have equal depth.  This is not checked.
 */
void
rasteropLow(l_uint32  *datad,
            l_int32    dpixw,
            l_int32    dpixh,
            l_int32    depth,
            l_int32    dwpl,
            l_int32    dx,
            l_int32    dy,
            l_int32    dw,
            l_int32    dh,
            l_int32    op,
            l_uint32  *datas,
            l_int32    spixw,
            l_int32    spixh,
            l_int32    swpl,
            l_int32    sx,
            l_int32    sy)
{
l_int32  dhangw, shangw, dhangh, shangh;

   /* -------------------------------------------------------*
    *            Scale horizontal dimensions by depth        *
    * -------------------------------------------------------*/
    if (depth != 1) {
        dpixw *= depth;
        dx *= depth;
        dw *= depth;
        spixw *= depth;
        sx *= depth;
    }

   /* -------------------------------------------------------*
    *      Clip to max rectangle within both src and dest    *
    * -------------------------------------------------------*/
       /* Clip horizontally (sx, dx, dw) */
    if (dx < 0) {
        sx -= dx;  /* increase sx */
        dw += dx;  /* reduce dw */
        dx = 0;
    }
    if (sx < 0) {
        dx -= sx;  /* increase dx */
        dw += sx;  /* reduce dw */
        sx = 0;
    }
    dhangw = dx + dw - dpixw;  /* rect ovhang dest to right */
    if (dhangw > 0)
        dw -= dhangw;  /* reduce dw */
    shangw = sx + dw - spixw;   /* rect ovhang src to right */
    if (shangw > 0)
        dw -= shangw;  /* reduce dw */

       /* Clip vertically (sy, dy, dh) */
    if (dy < 0) {
        sy -= dy;  /* increase sy */
        dh += dy;  /* reduce dh */
        dy = 0;
    }
    if (sy < 0) {
        dy -= sy;  /* increase dy */
        dh += sy;  /* reduce dh */
        sy = 0;
    }
    dhangh = dy + dh - dpixh;  /* rect ovhang dest below */
    if (dhangh > 0)
        dh -= dhangh;  /* reduce dh */
    shangh = sy + dh - spixh;  /* rect ovhang src below */
    if (shangh > 0)
        dh -= shangh;  /* reduce dh */

        /* If clipped entirely, quit */
    if ((dw <= 0) || (dh <= 0))
        return;

#if 0
    lept_stderr("dx = %d, dy = %d, dw = %d, dh = %d, sx = %d, sy = %d\n",
                dx, dy, dw, dh, sx, sy);
#endif

   /* -------------------------------------------------------*
    *       Dispatch to aligned or non-aligned blitters      *
    * -------------------------------------------------------*/
    if (((dx & 31) == 0) && ((sx & 31) == 0))
        rasteropWordAlignedLow(datad, dwpl, dx, dy, dw, dh, op,
                               datas, swpl, sx, sy);
    else if ((dx & 31) == (sx & 31))
        rasteropVAlignedLow(datad, dwpl, dx, dy, dw, dh, op,
                            datas, swpl, sx, sy);
    else
        rasteropGeneralLow(datad, dwpl, dx, dy, dw, dh, op,
                           datas, swpl, sx, sy);
}


/*--------------------------------------------------------------------*
 *        Static low-level rasterop with vertical word alignment      *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropWordAlignedLow()
 *
 * \param[in]    datad  ptr to dest image data
 * \param[in]    dwpl   wpl of dest
 * \param[in]    dx     x val of UL corner of dest rectangle
 * \param[in]    dy     y val of UL corner of dest rectangle
 * \param[in]    dw     width of dest rectangle
 * \param[in]    dh     height of dest rectangle
 * \param[in]    op     op code
 * \param[in]    datas  ptr to src image data
 * \param[in]    swpl   wpl of src
 * \param[in]    sx     x val of UL corner of src rectangle
 * \param[in]    sy     y val of UL corner of src rectangle
 * \return  void
 *
 *  This is called when both the src and dest rects
 *  are left aligned on 32-bit word boundaries.
 *  That is: dx & 31 == 0 and sx & 31 == 0
 *
 *  We make an optimized implementation of this because
 *  it is a common case: e.g., two images are rasterop'd
 *  starting from their UL corners 0,0.
 */
static void
rasteropWordAlignedLow(l_uint32  *datad,
                       l_int32    dwpl,
                       l_int32    dx,
                       l_int32    dy,
                       l_int32    dw,
                       l_int32    dh,
                       l_int32    op,
                       l_uint32  *datas,
                       l_int32    swpl,
                       l_int32    sx,
                       l_int32    sy)
{
l_int32    nfullw;     /* number of full words */
l_uint32  *psfword;    /* ptr to first src word */
l_uint32  *pdfword;    /* ptr to first dest word */
l_int32    lwbits;     /* number of ovrhang bits in last partial word */
l_uint32   lwmask;     /* mask for last partial word */
l_uint32  *lines, *lined;
l_int32    i, j;


    /*--------------------------------------------------------*
     *                Preliminary calculations                *
     *--------------------------------------------------------*/
    nfullw = dw >> 5;
    lwbits = dw & 31;
    if (lwbits)
        lwmask = lmask32[lwbits];
    psfword = datas + swpl * sy + (sx >> 5);
    pdfword = datad + dwpl * dy + (dx >> 5);

    /*--------------------------------------------------------*
     *            Now we're ready to do the ops               *
     *--------------------------------------------------------*/
    switch (op)
    {
    case PIX_SRC:
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = *lines;
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, *lines, lwmask);
        }
        break;
    case PIX_NOT(PIX_SRC):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = ~(*lines);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, ~(*lines), lwmask);
        }
        break;
    case (PIX_SRC | PIX_DST):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = (*lines | *lined);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, (*lines | *lined), lwmask);
        }
        break;
    case (PIX_SRC & PIX_DST):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = (*lines & *lined);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, (*lines & *lined), lwmask);
        }
        break;
    case (PIX_SRC ^ PIX_DST):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = (*lines ^ *lined);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, (*lines ^ *lined), lwmask);
        }
        break;
    case (PIX_NOT(PIX_SRC) | PIX_DST):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = (~(*lines) | *lined);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, (~(*lines) | *lined), lwmask);
        }
        break;
    case (PIX_NOT(PIX_SRC) & PIX_DST):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = (~(*lines) & *lined);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, (~(*lines) & *lined), lwmask);
        }
        break;
    case (PIX_SRC | PIX_NOT(PIX_DST)):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = (*lines | ~(*lined));
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, (*lines | ~(*lined)), lwmask);
        }
        break;
    case (PIX_SRC & PIX_NOT(PIX_DST)):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = (*lines & ~(*lined));
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, (*lines & ~(*lined)), lwmask);
        }
        break;
    case (PIX_NOT(PIX_SRC | PIX_DST)):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = ~(*lines  | *lined);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, ~(*lines  | *lined), lwmask);
        }
        break;
    case (PIX_NOT(PIX_SRC & PIX_DST)):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = ~(*lines  & *lined);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, ~(*lines  & *lined), lwmask);
        }
        break;
        /* this is three cases: ~(s ^ d), ~s ^ d, s ^ ~d  */
    case (PIX_NOT(PIX_SRC ^ PIX_DST)):
        for (i = 0; i < dh; i++) {
            lines = psfword + i * swpl;
            lined = pdfword + i * dwpl;
            for (j = 0; j < nfullw; j++) {
                *lined = ~(*lines ^ *lined);
                lined++;
                lines++;
            }
            if (lwbits)
                *lined = COMBINE_PARTIAL(*lined, ~(*lines ^ *lined), lwmask);
        }
        break;
    default:
        lept_stderr("Operation %d invalid\n", op);
    }
}



/*--------------------------------------------------------------------*
 *        Static low-level rasterop with vertical word alignment      *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropVAlignedLow()
 *
 * \param[in]    datad  ptr to dest image data
 * \param[in]    dwpl   wpl of dest
 * \param[in]    dx     x val of UL corner of dest rectangle
 * \param[in]    dy     y val of UL corner of dest rectangle
 * \param[in]    dw     width of dest rectangle
 * \param[in]    dh     height of dest rectangle
 * \param[in]    op     op code
 * \param[in]    datas  ptr to src image data
 * \param[in]    swpl   wpl of src
 * \param[in]    sx     x val of UL corner of src rectangle
 * \param[in]    sy     y val of UL corner of src rectangle
 * \return  void
 *
 *  This is called when the left side of the src and dest
 *  rects have the same alignment relative to 32-bit word
 *  boundaries; i.e., dx & 31) == (sx & 31
 */
static void
rasteropVAlignedLow(l_uint32  *datad,
                    l_int32    dwpl,
                    l_int32    dx,
                    l_int32    dy,
                    l_int32    dw,
                    l_int32    dh,
                    l_int32    op,
                    l_uint32  *datas,
                    l_int32    swpl,
                    l_int32    sx,
                    l_int32    sy)
{
l_int32    dfwpartb;   /* boolean (1, 0) if first dest word is partial */
l_int32    dfwpart2b;  /* boolean (1, 0) if first dest word is doubly partial */
l_uint32   dfwmask;    /* mask for first partial dest word */
l_int32    dfwbits;    /* first word dest bits in ovrhang */
l_uint32  *pdfwpart = NULL;   /* ptr to first partial dest word */
l_uint32  *psfwpart = NULL;   /* ptr to first partial src word */
l_int32    dfwfullb;   /* boolean (1, 0) if there exists a full dest word */
l_int32    dnfullw;    /* number of full words in dest */
l_uint32  *pdfwfull = NULL;   /* ptr to first full dest word */
l_uint32  *psfwfull = NULL;   /* ptr to first full src word */
l_int32    dlwpartb;   /* boolean (1, 0) if last dest word is partial */
l_uint32   dlwmask;    /* mask for last partial dest word */
l_int32    dlwbits;    /* last word dest bits in ovrhang */
l_uint32  *pdlwpart = NULL;   /* ptr to last partial dest word */
l_uint32  *pslwpart = NULL;   /* ptr to last partial src word */
l_int32    i, j;


    /*--------------------------------------------------------*
     *                Preliminary calculations                *
     *--------------------------------------------------------*/
        /* is the first word partial? */
    dfwmask = 0;
    if ((dx & 31) == 0) {  /* if not */
        dfwpartb = 0;
        dfwbits = 0;
    } else {  /* if so */
        dfwpartb = 1;
        dfwbits = 32 - (dx & 31);
        dfwmask = rmask32[dfwbits];
        pdfwpart = datad + dwpl * dy + (dx >> 5);
        psfwpart = datas + swpl * sy + (sx >> 5);
    }

        /* is the first word doubly partial? */
    if (dw >= dfwbits) {  /* if not */
        dfwpart2b = 0;
    } else {  /* if so */
        dfwpart2b = 1;
        dfwmask &= lmask32[32 - dfwbits + dw];
    }

        /* is there a full dest word? */
    if (dfwpart2b == 1) {  /* not */
        dfwfullb = 0;
        dnfullw = 0;
    } else {
        dnfullw = (dw - dfwbits) >> 5;
        if (dnfullw == 0) {  /* if not */
            dfwfullb = 0;
        } else {  /* if so */
            dfwfullb = 1;
            if (dfwpartb) {
                pdfwfull = pdfwpart + 1;
                psfwfull = psfwpart + 1;
            } else {
                pdfwfull = datad + dwpl * dy + (dx >> 5);
                psfwfull = datas + swpl * sy + (sx >> 5);
            }
        }
    }

        /* is the last word partial? */
    dlwbits = (dx + dw) & 31;
    if (dfwpart2b == 1 || dlwbits == 0) {  /* if not */
        dlwpartb = 0;
    } else {
        dlwpartb = 1;
        dlwmask = lmask32[dlwbits];
        if (dfwpartb) {
            pdlwpart = pdfwpart + 1 + dnfullw;
            pslwpart = psfwpart + 1 + dnfullw;
        } else {
            pdlwpart = datad + dwpl * dy + (dx >> 5) + dnfullw;
            pslwpart = datas + swpl * sy + (sx >> 5) + dnfullw;
        }
    }


    /*--------------------------------------------------------*
     *            Now we're ready to do the ops               *
     *--------------------------------------------------------*/
    switch (op)
    {
    case PIX_SRC:
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart, *psfwpart, dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = *(psfwfull + j);
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart, *pslwpart, dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case PIX_NOT(PIX_SRC):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart, ~(*psfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = ~(*(psfwfull + j));
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart, ~(*pslwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC | PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    (*psfwpart | *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) |= *(psfwfull + j);
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     (*pslwpart | *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC & PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    (*psfwpart & *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) &= *(psfwfull + j);
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     (*pslwpart & *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC ^ PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    (*psfwpart ^ *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) ^= *(psfwfull + j);
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     (*pslwpart ^ *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_NOT(PIX_SRC) | PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    (~(*psfwpart) | *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) |= ~(*(psfwfull + j));
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     (~(*pslwpart) | *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_NOT(PIX_SRC) & PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    (~(*psfwpart) & *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) &= ~(*(psfwfull + j));
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     (~(*pslwpart) & *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC | PIX_NOT(PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    (*psfwpart | ~(*pdfwpart)), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = *(psfwfull + j) | ~(*(pdfwfull + j));
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     (*pslwpart | ~(*pdlwpart)), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC & PIX_NOT(PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    (*psfwpart & ~(*pdfwpart)), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = *(psfwfull + j) & ~(*(pdfwfull + j));
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     (*pslwpart & ~(*pdlwpart)), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_NOT(PIX_SRC | PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    ~(*psfwpart | *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = ~(*(psfwfull + j) | *(pdfwfull + j));
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     ~(*pslwpart | *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_NOT(PIX_SRC & PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    ~(*psfwpart & *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = ~(*(psfwfull + j) & *(pdfwfull + j));
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     ~(*pslwpart & *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
        /* this is three cases: ~(s ^ d), ~s ^ d, s ^ ~d  */
    case (PIX_NOT(PIX_SRC ^ PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++) {
                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                    ~(*psfwpart ^ *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++)
                    *(pdfwfull + j) = ~(*(psfwfull + j) ^ *(pdfwfull + j));
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                                     ~(*pslwpart ^ *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    default:
        lept_stderr("Operation %x invalid\n", op);
    }
}


/*--------------------------------------------------------------------*
 *     Static low-level rasterop without vertical word alignment      *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropGeneralLow()
 *
 * \param[in]    datad  ptr to dest image data
 * \param[in]    dwpl   wpl of dest
 * \param[in]    dx     x val of UL corner of dest rectangle
 * \param[in]    dy     y val of UL corner of dest rectangle
 * \param[in]    dw     width of dest rectangle
 * \param[in]    dh     height of dest rectangle
 * \param[in]    op     op code
 * \param[in]    datas  ptr to src image data
 * \param[in]    swpl   wpl of src
 * \param[in]    sx     x val of UL corner of src rectangle
 * \param[in]    sy     y val of UL corner of src rectangle
 * \return  void
 *
 *  This is called when the src and dest rects are
 *  do not have the same 32-bit word alignment.
 *
 *  The method is a generalization of rasteropVAlignLow.
 *  There, the src image pieces were directly merged
 *  with the dest.  Here, we shift the source bits
 *  to fill words that are aligned with the dest, and
 *  then use those "source words" exactly in place
 *  of the source words that were used in rasteropVAlignLow.
 *
 *  The critical parameter is thus the shift required
 *  for the src.  Consider the left edge of the rectangle.
 *  The overhang into the src and dest words are found,
 *  and the difference is exactly this shift.  There are
 *  two separate cases, depending on whether the src pixels
 *  are shifted left or right.  If the src overhang is
 *  larger than the dest overhang, the src is shifted to
 *  the right, and a number of pixels equal to the shift are
 *  left over for filling the next dest word, if necessary.
 *
 *  But if the dest overhang is larger than the src overhang,
 *  the src is shifted to the left, and depending on the width of
 *  transferred pixels, it may also be necessary to shift pixels
 *  in from the next src word, in order to fill the dest word.
 *  An interesting case is where the src overhang equals the width,
 *  dw, of the block.  Then all the pixels necessary to fill the first
 *  dest word can be taken from the first src word, up to the last
 *  src pixel in the word, and no pixels from the next src word are
 *  required.  Consider this simple example, where a single pixel from
 *  the src is transferred to the dest:
 *     pix1 = pixCreate(32, 1, 1);
 *     pix2 = pixCreate(32, 1, 1);
 *     pixRasterop(pix1, 30, 0, 1, 1, PIX_SRC, pix2, 31, 0);
 *  Here, the pixel at the right end of the src image (sx = 31)
 *  is shifted one bit to the left (to dx = 30).  The width (1) equals
 *  the src overhang (1), and no pixels from the next word are required.
 *  (This must be true because there is only one src word.)
 */
static void
rasteropGeneralLow(l_uint32  *datad,
                   l_int32    dwpl,
                   l_int32    dx,
                   l_int32    dy,
                   l_int32    dw,
                   l_int32    dh,
                   l_int32    op,
                   l_uint32  *datas,
                   l_int32    swpl,
                   l_int32    sx,
                   l_int32    sy)
{
l_int32    dfwpartb;    /* boolean (1, 0) if first dest word is partial      */
l_int32    dfwpart2b;   /* boolean (1, 0) if 1st dest word is doubly partial */
l_uint32   dfwmask;     /* mask for first partial dest word                  */
l_int32    dfwbits;     /* first word dest bits in overhang; 0-31            */
l_int32    dhang;       /* dest overhang in first partial word,              */
                        /* or 0 if dest is word aligned (same as dfwbits)    */
l_uint32  *pdfwpart = NULL;    /* ptr to first partial dest word                    */
l_uint32  *psfwpart = NULL;    /* ptr to first partial src word                     */
l_int32    dfwfullb;    /* boolean (1, 0) if there exists a full dest word   */
l_int32    dnfullw;     /* number of full words in dest                      */
l_uint32  *pdfwfull = NULL;    /* ptr to first full dest word                       */
l_uint32  *psfwfull = NULL;    /* ptr to first full src word                        */
l_int32    dlwpartb;    /* boolean (1, 0) if last dest word is partial       */
l_uint32   dlwmask;     /* mask for last partial dest word                   */
l_int32    dlwbits;     /* last word dest bits in ovrhang                    */
l_uint32  *pdlwpart = NULL;    /* ptr to last partial dest word                     */
l_uint32  *pslwpart = NULL;    /* ptr to last partial src word                      */
l_uint32   sword;       /* compose src word aligned with the dest words      */
l_int32    sfwbits;     /* first word src bits in overhang (1-32),           */
                        /* or 32 if src is word aligned                      */
l_int32    shang;       /* source overhang in the first partial word,        */
                        /* or 0 if src is word aligned (not same as sfwbits) */
l_int32    sleftshift;  /* bits to shift left for source word to align       */
                        /* with the dest.  Also the number of bits that      */
                        /* get shifted to the right to align with the dest.  */
l_int32    srightshift; /* bits to shift right for source word to align      */
                        /* with dest.  Also, the number of bits that get     */
                        /* shifted left to align with the dest.              */
l_int32    srightmask;  /* mask for selecting sleftshift bits that have      */
                        /* been shifted right by srightshift bits            */
l_int32    sfwshiftdir; /* either SHIFT_LEFT or SHIFT_RIGHT                  */
l_int32    sfwaddb;     /* boolean: do we need an additional sfw right shift? */
l_int32    slwaddb;     /* boolean: do we need an additional slw right shift? */
l_int32    i, j;


    /*--------------------------------------------------------*
     *                Preliminary calculations                *
     *--------------------------------------------------------*/
        /* To get alignment of src with dst (e.g., in the
         * full words) the src must do a left shift of its
         * relative overhang in the current src word,
         * and OR that with a right shift of
         * (31 -  relative overhang) from the next src word.
         * We find the absolute overhangs, the relative overhangs,
         * the required shifts and the src mask */
    if ((sx & 31) == 0)
        shang = 0;
    else
        shang = 32 - (sx & 31);
    if ((dx & 31) == 0)
        dhang = 0;
    else
        dhang = 32 - (dx & 31);
#if 0
    lept_stderr("shang = %d, dhang = %d\n", shang, dhang);
#endif

    if (shang == 0 && dhang == 0) {  /* this should be treated by an
                                        aligned operation, not by
                                        this general rasterop! */
        sleftshift = 0;
        srightshift = 0;
        srightmask = rmask32[0];
    } else {
        if (dhang > shang)
            sleftshift = dhang - shang;
        else
            sleftshift = 32 - (shang - dhang);
        srightshift = 32 - sleftshift;
        srightmask = rmask32[sleftshift];
    }

#if 0
    lept_stderr("sleftshift = %d, srightshift = %d\n", sleftshift, srightshift);
#endif

        /* Is the first dest word partial? */
    dfwmask = 0;
    if ((dx & 31) == 0) {  /* if not */
        dfwpartb = 0;
        dfwbits = 0;
    } else {  /* if so */
        dfwpartb = 1;
        dfwbits = 32 - (dx & 31);
        dfwmask = rmask32[dfwbits];
        pdfwpart = datad + dwpl * dy + (dx >> 5);
        psfwpart = datas + swpl * sy + (sx >> 5);
        sfwbits = 32 - (sx & 31);
        if (dfwbits > sfwbits) {
            sfwshiftdir = SHIFT_LEFT;  /* shift by sleftshift */
                /* Do we have enough bits from the current src word? */
            if (dw <= shang)
                sfwaddb = 0;  /* yes: we have enough bits */
            else
                sfwaddb = 1;  /* no: rshift in next src word by srightshift */
        } else {
            sfwshiftdir = SHIFT_RIGHT;  /* shift by srightshift */
        }
    }

        /* Is the first dest word doubly partial? */
    if (dw >= dfwbits) {  /* if not */
        dfwpart2b = 0;
    } else {  /* if so */
        dfwpart2b = 1;
        dfwmask &= lmask32[32 - dfwbits + dw];
    }

        /* Is there a full dest word? */
    if (dfwpart2b == 1) {  /* not */
        dfwfullb = 0;
        dnfullw = 0;
    } else {
        dnfullw = (dw - dfwbits) >> 5;
        if (dnfullw == 0) {  /* if not */
            dfwfullb = 0;
        } else {  /* if so */
            dfwfullb = 1;
            pdfwfull = datad + dwpl * dy + ((dx + dhang) >> 5);
            psfwfull = datas + swpl * sy + ((sx + dhang) >> 5); /* yes, dhang */
        }
    }

        /* Is the last dest word partial? */
    dlwbits = (dx + dw) & 31;
    if (dfwpart2b == 1 || dlwbits == 0) {  /* if not */
        dlwpartb = 0;
    } else {
        dlwpartb = 1;
        dlwmask = lmask32[dlwbits];
        pdlwpart = datad + dwpl * dy + ((dx + dhang) >> 5) + dnfullw;
        pslwpart = datas + swpl * sy + ((sx + dhang) >> 5) + dnfullw;
        if (dlwbits <= srightshift)   /* must be <= here !!! */
            slwaddb = 0;  /* we got enough bits from current src word */
        else
            slwaddb = 1;  /* must rshift in next src word by srightshift */
    }


    /*--------------------------------------------------------*
     *            Now we're ready to do the ops               *
     *--------------------------------------------------------*/
    switch (op)
    {
    case PIX_SRC:
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart, sword, dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) = sword;
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart, sword, dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case PIX_NOT(PIX_SRC):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart, ~sword, dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) = ~sword;
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart, ~sword, dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC | PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 (sword | *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) |= sword;
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               (sword | *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC & PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 (sword & *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) &= sword;
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               (sword & *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC ^ PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 (sword ^ *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) ^= sword;
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               (sword ^ *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_NOT(PIX_SRC) | PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 (~sword | *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) |= ~sword;
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               (~sword | *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_NOT(PIX_SRC) & PIX_DST):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 (~sword & *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) &= ~sword;
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               (~sword & *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC | PIX_NOT(PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 (sword | ~(*pdfwpart)), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) = sword | ~(*(pdfwfull + j));
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               (sword | ~(*pdlwpart)), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_SRC & PIX_NOT(PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 (sword & ~(*pdfwpart)), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) = sword & ~(*(pdfwfull + j));
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               (sword & ~(*pdlwpart)), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_NOT(PIX_SRC | PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 ~(sword | *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) = ~(sword | *(pdfwfull + j));
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               ~(sword | *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    case (PIX_NOT(PIX_SRC & PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 ~(sword & *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) = ~(sword & *(pdfwfull + j));
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               ~(sword & *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
        /* this is three cases: ~(s ^ d), ~s ^ d, s ^ ~d  */
    case (PIX_NOT(PIX_SRC ^ PIX_DST)):
            /* do the first partial word */
        if (dfwpartb) {
            for (i = 0; i < dh; i++)
            {
                if (sfwshiftdir == SHIFT_LEFT) {
                    sword = *psfwpart << sleftshift;
                    if (sfwaddb)
                        sword = COMBINE_PARTIAL(sword,
                                      *(psfwpart + 1) >> srightshift,
                                       srightmask);
                } else {  /* shift right */
                    sword = *psfwpart >> srightshift;
                }

                *pdfwpart = COMBINE_PARTIAL(*pdfwpart,
                                 ~(sword ^ *pdfwpart), dfwmask);
                pdfwpart += dwpl;
                psfwpart += swpl;
            }
        }

            /* do the full words */
        if (dfwfullb) {
            for (i = 0; i < dh; i++) {
                for (j = 0; j < dnfullw; j++) {
                    sword = COMBINE_PARTIAL(*(psfwfull + j) << sleftshift,
                                   *(psfwfull + j + 1) >> srightshift,
                                   srightmask);
                    *(pdfwfull + j) = ~(sword ^ *(pdfwfull + j));
                }
                pdfwfull += dwpl;
                psfwfull += swpl;
            }
        }

            /* do the last partial word */
        if (dlwpartb) {
            for (i = 0; i < dh; i++) {
                sword = *pslwpart << sleftshift;
                if (slwaddb)
                    sword = COMBINE_PARTIAL(sword,
                                  *(pslwpart + 1) >> srightshift,
                                  srightmask);

                *pdlwpart = COMBINE_PARTIAL(*pdlwpart,
                               ~(sword ^ *pdlwpart), dlwmask);
                pdlwpart += dwpl;
                pslwpart += swpl;
            }
        }
        break;
    default:
        lept_stderr("Operation %x invalid\n", op);
    }
}


/*--------------------------------------------------------------------*
 *        Low level in-place full height vertical block transfer      *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropVipLow()
 *
 * \param[in]    data   ptr to image data
 * \param[in]    pixw   width
 * \param[in]    pixh   height
 * \param[in]    depth  depth
 * \param[in]    wpl    wpl
 * \param[in]    x      x val of UL corner of rectangle
 * \param[in]    w      width of rectangle
 * \param[in]    shift  + shifts data downward in vertical column
 * \return  0 if OK; 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This clears the pixels that are left exposed after the
 *          translation.  You can consider them as pixels that are
 *          shifted in from outside the image.  This can be later
 *          overridden by the incolor parameter in higher-level functions
 *          that call this.  For example, for images with depth > 1,
 *          these pixels are cleared to black; to be white they
 *          must later be SET to white.  See, e.g., pixRasteropVip().
 *      (2) This function scales the width to accommodate any depth,
 *          performs clipping, and then does the in-place rasterop.
 * </pre>
 */
void
rasteropVipLow(l_uint32  *data,
               l_int32    pixw,
               l_int32    pixh,
               l_int32    depth,
               l_int32    wpl,
               l_int32    x,
               l_int32    w,
               l_int32    shift)
{
l_int32    fwpartb;    /* boolean (1, 0) if first word is partial */
l_int32    fwpart2b;   /* boolean (1, 0) if first word is doubly partial */
l_uint32   fwmask;     /* mask for first partial word */
l_int32    fwbits;     /* first word bits in ovrhang */
l_uint32  *pdfwpart = NULL;   /* ptr to first partial dest word */
l_uint32  *psfwpart = NULL;   /* ptr to first partial src word */
l_int32    fwfullb;    /* boolean (1, 0) if there exists a full word */
l_int32    nfullw;     /* number of full words */
l_uint32  *pdfwfull = NULL;   /* ptr to first full dest word */
l_uint32  *psfwfull = NULL;   /* ptr to first full src word */
l_int32    lwpartb;    /* boolean (1, 0) if last word is partial */
l_uint32   lwmask;     /* mask for last partial word */
l_int32    lwbits;     /* last word bits in ovrhang */
l_uint32  *pdlwpart = NULL;   /* ptr to last partial dest word */
l_uint32  *pslwpart = NULL;   /* ptr to last partial src word */
l_int32    dirwpl;     /* directed wpl (-wpl * sign(shift)) */
l_int32    absshift;   /* absolute value of shift; for use in iterator */
l_int32    vlimit;     /* vertical limit value for iterations */
l_int32    i, j;


   /*--------------------------------------------------------*
    *            Scale horizontal dimensions by depth        *
    *--------------------------------------------------------*/
    if (depth != 1) {
        pixw *= depth;
        x *= depth;
        w *= depth;
    }


   /*--------------------------------------------------------*
    *                   Clip horizontally                    *
    *--------------------------------------------------------*/
    if (x < 0) {
        w += x;    /* reduce w */
        x = 0;     /* clip to x = 0 */
    }
    if (x >= pixw || w <= 0)  /* no part of vertical slice is in the image */
        return;

    if (x + w > pixw)
        w = pixw - x;   /* clip to x + w = pixw */

    /*--------------------------------------------------------*
     *                Preliminary calculations                *
     *--------------------------------------------------------*/
        /* is the first word partial? */
    if ((x & 31) == 0) {  /* if not */
        fwpartb = 0;
        fwbits = 0;
    } else {  /* if so */
        fwpartb = 1;
        fwbits = 32 - (x & 31);
        fwmask = rmask32[fwbits];
        if (shift >= 0) { /* go up from bottom */
            pdfwpart = data + wpl * (pixh - 1) + (x >> 5);
            psfwpart = data + wpl * (pixh - 1 - shift) + (x >> 5);
        } else {  /* go down from top */
            pdfwpart = data + (x >> 5);
            psfwpart = data - wpl * shift + (x >> 5);
        }
    }

        /* is the first word doubly partial? */
    if (w >= fwbits) {  /* if not */
        fwpart2b = 0;
    } else {  /* if so */
        fwpart2b = 1;
        fwmask &= lmask32[32 - fwbits + w];
    }

        /* is there a full dest word? */
    if (fwpart2b == 1) {  /* not */
        fwfullb = 0;
        nfullw = 0;
    } else {
        nfullw = (w - fwbits) >> 5;
        if (nfullw == 0) {  /* if not */
            fwfullb = 0;
        } else {  /* if so */
            fwfullb = 1;
            if (fwpartb) {
                pdfwfull = pdfwpart + 1;
                psfwfull = psfwpart + 1;
            } else if (shift >= 0) { /* go up from bottom */
                pdfwfull = data + wpl * (pixh - 1) + (x >> 5);
                psfwfull = data + wpl * (pixh - 1 - shift) + (x >> 5);
            } else {  /* go down from top */
                pdfwfull = data + (x >> 5);
                psfwfull = data - wpl * shift + (x >> 5);
            }
        }
    }

        /* is the last word partial? */
    lwbits = (x + w) & 31;
    if (fwpart2b == 1 || lwbits == 0) {  /* if not */
        lwpartb = 0;
    } else {
        lwpartb = 1;
        lwmask = lmask32[lwbits];
        if (fwpartb) {
            pdlwpart = pdfwpart + 1 + nfullw;
            pslwpart = psfwpart + 1 + nfullw;
        } else if (shift >= 0) { /* go up from bottom */
            pdlwpart = data + wpl * (pixh - 1) + (x >> 5) + nfullw;
            pslwpart = data + wpl * (pixh - 1 - shift) + (x >> 5) + nfullw;
        } else {  /* go down from top */
            pdlwpart = data + (x >> 5) + nfullw;
            pslwpart = data - wpl * shift + (x >> 5) + nfullw;
        }
    }

        /* determine the direction of flow from the shift
         * If the shift >= 0, data flows downard from src
         * to dest, starting at the bottom and working up.
         * If shift < 0, data flows upward from src to
         * dest, starting at the top and working down. */
    dirwpl = (shift >= 0) ? -wpl : wpl;
    absshift = L_ABS(shift);
    vlimit = L_MAX(0, pixh - absshift);


    /*--------------------------------------------------------*
     *            Now we're ready to do the ops               *
     *--------------------------------------------------------*/

        /* Do the first partial word */
    if (fwpartb) {
        for (i = 0; i < vlimit; i++) {
            *pdfwpart = COMBINE_PARTIAL(*pdfwpart, *psfwpart, fwmask);
            pdfwpart += dirwpl;
            psfwpart += dirwpl;
        }

            /* Clear the incoming pixels */
        for (i = vlimit; i < pixh; i++) {
            *pdfwpart = COMBINE_PARTIAL(*pdfwpart, 0x0, fwmask);
            pdfwpart += dirwpl;
        }
    }

        /* Do the full words */
    if (fwfullb) {
        for (i = 0; i < vlimit; i++) {
            for (j = 0; j < nfullw; j++)
                *(pdfwfull + j) = *(psfwfull + j);
            pdfwfull += dirwpl;
            psfwfull += dirwpl;
        }

            /* Clear the incoming pixels */
        for (i = vlimit; i < pixh; i++) {
            for (j = 0; j < nfullw; j++)
                *(pdfwfull + j) = 0x0;
            pdfwfull += dirwpl;
        }
    }

        /* Do the last partial word */
    if (lwpartb) {
        for (i = 0; i < vlimit; i++) {
            *pdlwpart = COMBINE_PARTIAL(*pdlwpart, *pslwpart, lwmask);
            pdlwpart += dirwpl;
            pslwpart += dirwpl;
        }

            /* Clear the incoming pixels */
        for (i = vlimit; i < pixh; i++) {
            *pdlwpart = COMBINE_PARTIAL(*pdlwpart, 0x0, lwmask);
            pdlwpart += dirwpl;
        }
    }
}



/*--------------------------------------------------------------------*
 *       Low level in-place full width horizontal block transfer      *
 *--------------------------------------------------------------------*/
/*!
 * \brief   rasteropHipLow()
 *
 * \param[in]    data   ptr to image data
 * \param[in]    pixh   height
 * \param[in]    depth  depth
 * \param[in]    wpl    wpl
 * \param[in]    y      y val of UL corner of rectangle
 * \param[in]    h      height of rectangle
 * \param[in]    shift  + shifts data to the left in a horizontal column
 * \return  0 if OK; 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This clears the pixels that are left exposed after the rasterop.
 *          Therefore, for Pix with depth > 1, these pixels become black,
 *          and must be subsequently SET if they are to be white.
 *          For example, see pixRasteropHip().
 *      (2) This function performs clipping and calls shiftDataHorizontalLow()
 *          to do the in-place rasterop on each line.
 * </pre>
 */
void
rasteropHipLow(l_uint32  *data,
               l_int32    pixh,
               l_int32    depth,
               l_int32    wpl,
               l_int32    y,
               l_int32    h,
               l_int32    shift)
{
l_int32    i;
l_uint32  *line;

        /* clip band if necessary */
    if (y < 0) {
        h += y;  /* reduce h */
        y = 0;   /* clip to y = 0 */
    }
    if (h <= 0 || y > pixh)  /* no part of horizontal slice is in the image */
        return;

    if (y + h > pixh)
        h = pixh - y;   /* clip to y + h = pixh */

    for (i = y; i < y + h; i++) {
        line = data + i * wpl;
        shiftDataHorizontalLow(line, wpl, line, wpl, shift * depth);
    }
}


/*!
 * \brief   shiftDataHorizontalLow()
 *
 * \param[in]    datad  ptr to beginning of dest line
 * \param[in]    wpld   wpl of dest
 * \param[in]    datas  ptr to beginning of src line
 * \param[in]    wpls   wpl of src
 * \param[in]    shift  horizontal shift of block; >0 is to right
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This can also be used for in-place operation; see, e.g.,
 *          rasteropHipLow().
 *      (2) We are clearing the pixels that are shifted in from
 *          outside the image.  This can be overridden by the
 *          incolor parameter in higher-level functions that call this.
 * </pre>
 */
static void
shiftDataHorizontalLow(l_uint32  *datad,
                       l_int32    wpld,
                       l_uint32  *datas,
                       l_int32    wpls,
                       l_int32    shift)
{
l_int32    j, firstdw, wpl, rshift, lshift;
l_uint32  *lined, *lines;

    lined = datad;
    lines = datas;

    if (shift >= 0) {   /* src shift to right; data flows to
                         * right, starting at right edge and
                         * progressing leftward. */
        firstdw = shift / 32;
        wpl = L_MIN(wpls, wpld - firstdw);
        lined += firstdw + wpl - 1;
        lines += wpl - 1;
        rshift = shift & 31;
        if (rshift == 0) {
            for (j = 0; j < wpl; j++)
                *lined-- = *lines--;

                /* clear out the rest to the left edge */
            for (j = 0; j < firstdw; j++)
                *lined-- = 0;
        } else {
            lshift = 32 - rshift;
            for (j = 1; j < wpl; j++) {
                *lined-- = *(lines - 1) << lshift | *lines >> rshift;
                lines--;
            }
            *lined = *lines >> rshift;  /* partial first */

                /* clear out the rest to the left edge */
            *lined &= ~lmask32[rshift];
            lined--;
            for (j = 0; j < firstdw; j++)
                *lined-- = 0;
        }
    } else {  /* src shift to left; data flows to left, starting
             * at left edge and progressing rightward. */
        firstdw = (-shift) / 32;
        wpl = L_MIN(wpls - firstdw, wpld);
        lines += firstdw;
        lshift = (-shift) & 31;
        if (lshift == 0) {
            for (j = 0; j < wpl; j++)
                *lined++ = *lines++;

                /* clear out the rest to the right edge */
            for (j = 0; j < firstdw; j++)
                *lined++ = 0;
        } else {
            rshift = 32 - lshift;
            for (j = 1; j < wpl; j++) {
                *lined++ = *lines << lshift | *(lines + 1) >> rshift;
                lines++;
            }
            *lined = *lines << lshift;  /* partial last */

                /* clear out the rest to the right edge */
                /* first clear the lshift pixels of this partial word */
            *lined &= ~rmask32[lshift];
            lined++;
                /* then the remaining words to the right edge */
            for (j = 0; j < firstdw; j++)
                *lined++ = 0;
        }
    }
}
