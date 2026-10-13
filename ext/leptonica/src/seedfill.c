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
 * \file seedfill.c
 * <pre>
 *
 *      Binary seedfill (source: Luc Vincent)
 *               PIX         *pixSeedfillBinary()
 *               PIX         *pixSeedfillBinaryRestricted()
 *               static void  seedfillBinaryLow()
 *
 *      Applications of binary seedfill to find and fill holes,
 *      remove c.c. touching the border and fill bg from border:
 *               PIX         *pixHolesByFilling()
 *               PIX         *pixFillClosedBorders()
 *               PIX         *pixExtractBorderConnComps()
 *               PIX         *pixRemoveBorderConnComps()
 *               PIX         *pixFillBgFromBorder()
 *
 *      Hole-filling of components to bounding rectangle
 *               PIX         *pixFillHolesToBoundingRect()
 *
 *      Gray seedfill (source: Luc Vincent:fast-hybrid-grayscale-reconstruction)
 *               l_int32      pixSeedfillGray()
 *               l_int32      pixSeedfillGrayInv()
 *               static void  seedfillGrayLow()
 *               static void  seedfillGrayInvLow()

 *
 *      Gray seedfill (source: Luc Vincent: sequential-reconstruction algorithm)
 *               l_int32      pixSeedfillGraySimple()
 *               l_int32      pixSeedfillGrayInvSimple()
 *               static void  seedfillGrayLowSimple()
 *               static void  seedfillGrayInvLowSimple()
 *
 *      Gray seedfill variations
 *               PIX         *pixSeedfillGrayBasin()
 *
 *      Distance function (source: Luc Vincent)
 *               PIX         *pixDistanceFunction()
 *               static void  distanceFunctionLow()
 *
 *      Seed spread (based on distance function)
 *               PIX         *pixSeedspread()
 *               static void  seedspreadLow()
 *
 *      Local extrema:
 *               l_int32      pixLocalExtrema()
 *            static l_int32  pixQualifyLocalMinima()
 *               l_int32      pixSelectedLocalExtrema()
 *               PIX         *pixFindEqualValues()
 *
 *      Selection of minima in mask of connected components
 *               PTA         *pixSelectMinInConnComp()
 *
 *      Removal of seeded connected components from a mask
 *               PIX         *pixRemoveSeededComponents()
 *
 *
 *           ITERATIVE RASTER-ORDER SEEDFILL
 *
 *      The basic method in the Vincent seedfill (aka reconstruction)
 *      algorithm is simple.  We describe here the situation for
 *      binary seedfill.  Pixels are sampled in raster order in
 *      the seed image.  If they are 4-connected to ON pixels
 *      either directly above or to the left, and are not masked
 *      out by the mask image, they are turned on (or remain on).
 *      (Ditto for 8-connected, except you need to check 3 pixels
 *      on the previous line as well as the pixel to the left
 *      on the current line.  This is extra computational work
 *      for relatively little gain, so it is preferable
 *      in most situations to use the 4-connected version.)
 *      The algorithm proceeds from UR to LL of the image, and
 *      then reverses and sweeps up from LL to UR.
 *      These double sweeps are iterated until there is no change.
 *      At this point, the seed has entirely filled the region it
 *      is allowed to, as delimited by the mask image.
 *
 *      The grayscale seedfill is a straightforward generalization
 *      of the binary seedfill, and is described in seedfillLowGray().
 *
 *      For some applications, the filled seed will later be OR'd
 *      with the negative of the mask.   This is used, for example,
 *      when you flood fill into a 4-connected region of OFF pixels
 *      and you want the result after those pixels are turned ON.
 *
 *      Note carefully that the mask we use delineates which pixels
 *      are allowed to be ON as the seed is filled.  We will call this
 *      a "filling mask".  As the seed expands, it is repeatedly
 *      ANDed with the filling mask: s & fm.  The process can equivalently
 *      be formulated using the inverse of the filling mask, which
 *      we will call a "blocking mask": bm = ~fm.   As the seed
 *      expands, the blocking mask is repeatedly used to prevent
 *      the seed from expanding into the blocking mask.  This is done
 *      by set subtracting the blocking mask from the expanded seed:
 *      s - bm.  Set subtraction of the blocking mask is equivalent
 *      to ANDing with the inverse of the blocking mask: s & (~bm).
 *      But from the inverse relation between blocking and filling
 *      masks, this is equal to s & fm, which proves the equivalence.
 *
 *      For efficiency, the pixels can be taken in larger units
 *      for processing, but still in raster order.  It is natural
 *      to take them in 32-bit words.  The outline of the work
 *      to be done for 4-cc (not including special cases for boundary
 *      words, such as the first line or the last word in each line)
 *      is as follows.  Let the filling mask be m.  The
 *      seed is to fill "under" the mask; i.e., limited by an AND
 *      with the mask.  Let the current word be w, the word
 *      in the line above be wa, and the previous word in the
 *      current line be wp.   Let t be a temporary word that
 *      is used in computation.  Note that masking is performed by
 *      w & m.  (If we had instead used a "blocking" mask, we
 *      would perform masking by the set subtraction operation,
 *      w - m, which is defined to be w & ~m.)
 *
 *      The entire operation can be implemented with shifts,
 *      logical operations and tests.  For each word in the seed image
 *      there are two steps.  The first step is to OR the word with
 *      the word above and with the rightmost pixel in wp (call it "x").
 *      Because wp is shifted one pixel to its right, "x" is ORed
 *      to the leftmost pixel of w.  We then clip to the ON pixels in
 *      the mask.  The result is
 *               t  <--  (w | wa | x000... ) & m
 *      We've now finished taking data from above and to the left.
 *      The second step is to allow filling to propagate horizontally
 *      in t, always making sure that it is properly masked at each
 *      step.  So if filling can be done (i.e., t is neither all 0s
 *      nor all 1s), iteratively take:
 *           t  <--  (t | (t >> 1) | (t << 1)) & m
 *      until t stops changing.  Then write t back into w.
 *
 *      Finally, the boundary conditions require we note that in doing
 *      the above steps:
 *          (a) The words in the first row have no wa
 *          (b) The first word in each row has no wp in that row
 *          (c) The last word in each row must be masked so that
 *              pixels don't propagate beyond the right edge of the
 *              actual image.  (This is easily accomplished by
 *              setting the out-of-bound pixels in m to OFF.)
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

struct L_Pixel
{
    l_int32    x;
    l_int32    y;
};
typedef struct L_Pixel  L_PIXEL;

static void seedfillBinaryLow(l_uint32 *datas, l_int32 hs, l_int32 wpls,
                              l_uint32 *datam, l_int32 hm, l_int32 wplm,
                              l_int32 connectivity);
static void seedfillGrayLow(l_uint32 *datas, l_int32 w, l_int32 h,
                            l_int32 wpls, l_uint32 *datam, l_int32 wplm,
                            l_int32 connectivity);
static void seedfillGrayInvLow(l_uint32 *datas, l_int32 w, l_int32 h,
                               l_int32 wpls, l_uint32 *datam, l_int32 wplm,
                               l_int32 connectivity);
static void seedfillGrayLowSimple(l_uint32 *datas, l_int32 w, l_int32 h,
                                  l_int32 wpls, l_uint32 *datam, l_int32 wplm,
                                  l_int32 connectivity);
static void seedfillGrayInvLowSimple(l_uint32 *datas, l_int32 w, l_int32 h,
                                     l_int32 wpls, l_uint32 *datam,
                                     l_int32 wplm, l_int32 connectivity);
static void distanceFunctionLow(l_uint32 *datad, l_int32 w, l_int32 h,
                                l_int32 d, l_int32 wpld, l_int32 connectivity);
static void seedspreadLow(l_uint32 *datad, l_int32 w, l_int32 h, l_int32 wpld,
                          l_uint32 *datat, l_int32 wplt, l_int32 connectivity);


static l_int32 pixQualifyLocalMinima(PIX *pixs, PIX *pixm, l_int32 maxval);

#ifndef  NO_CONSOLE_IO
#define   DEBUG_PRINT_ITERS    0
#endif  /* ~NO_CONSOLE_IO */

  /* Two-way (UL --> LR, LR --> UL) sweep iterations; typically need only 4 */
static const l_int32  MaxIters = 40;


/*-----------------------------------------------------------------------*
 *              Vincent's Iterative Binary Seedfill method               *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixSeedfillBinary()
 *
 * \param[in]    pixd          [optional]; can be null, equal to pixs,
 *                             or different from pixs; 1 bpp
 * \param[in]    pixs          1 bpp seed
 * \param[in]    pixm          1 bpp filling mask
 * \param[in]    connectivity  4 or 8
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) This is for binary seedfill (aka "binary reconstruction").
 *      (2) There are 3 cases:
 *            (a) pixd == null (make a new pixd)
 *            (b) pixd == pixs (in-place)
 *            (c) pixd != pixs
 *      (3) If you know the case, use these patterns for clarity:
 *            (a) pixd = pixSeedfillBinary(NULL, pixs, ...);
 *            (b) pixSeedfillBinary(pixs, pixs, ...);
 *            (c) pixSeedfillBinary(pixd, pixs, ...);
 *      (4) The resulting pixd contains the filled seed.  For some
 *          applications you want to OR it with the inverse of
 *          the filling mask.
 *      (5) The input seed and mask images can be different sizes, but
 *          in typical use the difference, if any, would be only
 *          a few pixels in each direction.  If the sizes differ,
 *          the clipping is handled by the low-level function
 *          seedfillBinaryLow().
 * </pre>
 */
PIX *
pixSeedfillBinary(PIX     *pixd,
                  PIX     *pixs,
                  PIX     *pixm,
                  l_int32  connectivity)
{
l_int32    i, boolval;
l_int32    hd, hm, wpld, wplm;
l_uint32  *datad, *datam;
PIX       *pixt;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, pixd);
    if (!pixm || pixGetDepth(pixm) != 1)
        return (PIX *)ERROR_PTR("pixm undefined or not 1 bpp", __func__, pixd);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not in {4,8}", __func__, pixd);

        /* Prepare pixd as a copy of pixs if not identical */
    if ((pixd = pixCopy(pixd, pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixSetPadBits(pixd, 0);  /* be safe: */
    pixSetPadBits(pixm, 0);  /* avoid using uninitialized memory */

        /* pixt is used to test for completion */
    if ((pixt = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixt not made", __func__, pixd);

    hd = pixGetHeight(pixd);
    hm = pixGetHeight(pixm);  /* included so seedfillBinaryLow() can clip */
    datad = pixGetData(pixd);
    datam = pixGetData(pixm);
    wpld = pixGetWpl(pixd);
    wplm = pixGetWpl(pixm);


    for (i = 0; i < MaxIters; i++) {
        pixCopy(pixt, pixd);
        seedfillBinaryLow(datad, hd, wpld, datam, hm, wplm, connectivity);
        pixEqual(pixd, pixt, &boolval);
        if (boolval == 1) {
#if DEBUG_PRINT_ITERS
            lept_stderr("Binary seed fill converged: %d iters\n", i + 1);
#endif  /* DEBUG_PRINT_ITERS */
            break;
        }
    }

    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixSeedfillBinaryRestricted()
 *
 * \param[in]    pixd          [optional]; can be null, equal to pixs,
 *                             or different from pixs; 1 bpp
 * \param[in]    pixs          1 bpp seed
 * \param[in]    pixm          1 bpp filling mask
 * \param[in]    connectivity  4 or 8
 * \param[in]    xmax          max distance in x direction of fill into mask
 * \param[in]    ymax          max distance in y direction of fill into mask
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) See usage for pixSeedfillBinary(), which has unrestricted fill.
 *          In pixSeedfillBinary(), the filling distance is unrestricted
 *          and can be larger than pixs, depending on the topology of
 *          th mask.
 *      (2) There are occasions where it is useful not to permit the
 *          fill to go more than a certain distance into the mask.
 *          %xmax specifies the maximum horizontal distance allowed
 *          in the fill; %ymax does likewise in the vertical direction.
 *      (3) Operationally, the max "distance" allowed for the fill
 *          is a linear distance from the original seed, independent
 *          of the actual mask topology.
 *      (4) Another formulation of this problem, not implemented,
 *          would use the manhattan distance from the seed, as
 *          determined by a breadth-first search starting at the seed
 *          boundaries and working outward where the mask fg allows.
 *          How this might use the constraints of separate xmax and ymax
 *          is not clear.
 * </pre>
 */
PIX *
pixSeedfillBinaryRestricted(PIX     *pixd,
                            PIX     *pixs,
                            PIX     *pixm,
                            l_int32  connectivity,
                            l_int32  xmax,
                            l_int32  ymax)
{
l_int32  w, h;
PIX     *pix1, *pix2;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, pixd);
    if (!pixm || pixGetDepth(pixm) != 1)
        return (PIX *)ERROR_PTR("pixm undefined or not 1 bpp", __func__, pixd);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not in {4,8}", __func__, pixd);
    if (xmax == 0 && ymax == 0)  /* no filling permitted */
        return pixClone(pixs);
    if (xmax < 0 || ymax < 0) {
        L_ERROR("xmax and ymax must be non-negative", __func__);
        return pixClone(pixs);
    }

        /* Full fill from the seed into the mask. */
    if ((pix1 = pixSeedfillBinary(NULL, pixs, pixm, connectivity)) == NULL)
        return (PIX *)ERROR_PTR("pix1 not made", __func__, pixd);

        /* Dilate the seed.  This gives the maximal region where changes
         * are permitted.  Invert to get the region where pixs is
         * not allowed to change.  */
    pix2 = pixDilateCompBrick(NULL, pixs, 2 * xmax + 1, 2 * ymax + 1);
    pixInvert(pix2, pix2);

        /* Blank the region of pix1 specified by the fg of pix2.
         * This is not yet the final result, because it may have fg pixels
         * that are not accessible from the seed in the restricted distance.
         * For example, such pixels may be connected to the original seed,
         * but through a path that goes outside the permitted region. */
    pixGetDimensions(pixs, &w, &h, NULL);
    pixRasterop(pix1, 0, 0, w, h, PIX_DST & PIX_NOT(PIX_SRC), pix2, 0, 0);

        /* To get the accessible pixels in the restricted region, do
         * a second seedfill from the original seed, using pix1 as
         * a mask.  The result, in pixd, will not have any bad fg
         * pixels that were in pix1. */
    pixd = pixSeedfillBinary(pixd, pixs, pix1, connectivity);

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    return pixd;
}


/*!
 * \brief   seedfillBinaryLow()
 *
 *  Notes:
 *      (1) This is an in-place fill, where the seed image is
 *          filled, clipping to the filling mask, in one full
 *          cycle of UL -> LR and LR -> UL raster scans.
 *      (2) Assume the mask is a filling mask, not a blocking mask.
 *      (3) Assume that the RHS pad bits of the mask
 *          are properly set to 0.
 *      (4) Clip to the smallest dimensions to avoid invalid reads.
 */
static void
seedfillBinaryLow(l_uint32  *datas,
                  l_int32    hs,
                  l_int32    wpls,
                  l_uint32  *datam,
                  l_int32    hm,
                  l_int32    wplm,
                  l_int32    connectivity)
{
l_int32    i, j, h, wpl;
l_uint32   word, mask;
l_uint32   wordabove, wordleft, wordbelow, wordright;
l_uint32   wordprev;  /* test against this in previous iteration */
l_uint32  *lines, *linem;

    h = L_MIN(hs, hm);
    wpl = L_MIN(wpls, wplm);

    switch (connectivity)
    {
    case 4:
            /* UL --> LR scan */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < wpl; j++) {
                word = *(lines + j);
                mask = *(linem + j);

                    /* OR from word above and from word to left; mask */
                if (i > 0) {
                    wordabove = *(lines - wpls + j);
                    word |= wordabove;
                }
                if (j > 0) {
                    wordleft = *(lines + j - 1);
                    word |= wordleft << 31;
                }
                word &= mask;

                    /* No need to fill horizontally? */
                if (!word || !(~word)) {
                    *(lines + j) = word;
                    continue;
                }

                while (1) {
                    wordprev = word;
                    word = (word | (word >> 1) | (word << 1)) & mask;
                    if ((word ^ wordprev) == 0) {
                        *(lines + j) = word;
                        break;
                    }
                }
            }
        }

            /* LR --> UL scan */
        for (i = h - 1; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = wpl - 1; j >= 0; j--) {
                word = *(lines + j);
                mask = *(linem + j);

                    /* OR from word below and from word to right; mask */
                if (i < h - 1) {
                    wordbelow = *(lines + wpls + j);
                    word |= wordbelow;
                }
                if (j < wpl - 1) {
                    wordright = *(lines + j + 1);
                    word |= wordright >> 31;
                }
                word &= mask;

                    /* No need to fill horizontally? */
                if (!word || !(~word)) {
                    *(lines + j) = word;
                    continue;
                }

                while (1) {
                    wordprev = word;
                    word = (word | (word >> 1) | (word << 1)) & mask;
                    if ((word ^ wordprev) == 0) {
                        *(lines + j) = word;
                        break;
                    }
                }
            }
        }
        break;

    case 8:
            /* UL --> LR scan */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < wpl; j++) {
                word = *(lines + j);
                mask = *(linem + j);

                    /* OR from words above and from word to left; mask */
                if (i > 0) {
                    wordabove = *(lines - wpls + j);
                    word |= (wordabove | (wordabove << 1) | (wordabove >> 1));
                    if (j > 0)
                        word |= (*(lines - wpls + j - 1)) << 31;
                    if (j < wpl - 1)
                        word |= (*(lines - wpls + j + 1)) >> 31;
                }
                if (j > 0) {
                    wordleft = *(lines + j - 1);
                    word |= wordleft << 31;
                }
                word &= mask;

                    /* No need to fill horizontally? */
                if (!word || !(~word)) {
                    *(lines + j) = word;
                    continue;
                }

                while (1) {
                    wordprev = word;
                    word = (word | (word >> 1) | (word << 1)) & mask;
                    if ((word ^ wordprev) == 0) {
                        *(lines + j) = word;
                        break;
                    }
                }
            }
        }

            /* LR --> UL scan */
        for (i = h - 1; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = wpl - 1; j >= 0; j--) {
                word = *(lines + j);
                mask = *(linem + j);

                    /* OR from words below and from word to right; mask */
                if (i < h - 1) {
                    wordbelow = *(lines + wpls + j);
                    word |= (wordbelow | (wordbelow << 1) | (wordbelow >> 1));
                    if (j > 0)
                        word |= (*(lines + wpls + j - 1)) << 31;
                    if (j < wpl - 1)
                        word |= (*(lines + wpls + j + 1)) >> 31;
                }
                if (j < wpl - 1) {
                    wordright = *(lines + j + 1);
                    word |= wordright >> 31;
                }
                word &= mask;

                    /* No need to fill horizontally? */
                if (!word || !(~word)) {
                    *(lines + j) = word;
                    continue;
                }

                while (1) {
                    wordprev = word;
                    word = (word | (word >> 1) | (word << 1)) & mask;
                    if ((word ^ wordprev) == 0) {
                        *(lines + j) = word;
                        break;
                    }
                }
            }
        }
        break;

    default:
        L_ERROR("connectivity must be 4 or 8\n", __func__);
    }
}


/*!
 * \brief   pixHolesByFilling()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    connectivity   4 or 8
 * \return  pixd  inverted image of all holes, or NULL on error
 *
 * Action:
 *     1 Start with 1-pixel black border on otherwise white pixd
 *     2 Use the inverted pixs as the filling mask to fill in
 *         all the pixels from the border to the pixs foreground
 *     3 OR the result with pixs to have an image with all
 *         ON pixels except for the holes.
 *     4 Invert the result to get the holes as foreground
 *
 * <pre>
 * Notes:
 *     (1) To get 4-c.c. holes of the 8-c.c. as foreground, use
 *         4-connected filling; to get 8-c.c. holes of the 4-c.c.
 *         as foreground, use 8-connected filling.
 * </pre>
 */
PIX *
pixHolesByFilling(PIX     *pixs,
                  l_int32  connectivity)
{
PIX  *pixsi, *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

    if ((pixd = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    if ((pixsi = pixInvert(NULL, pixs)) == NULL) {
        pixDestroy(&pixd);
        return (PIX *)ERROR_PTR("pixsi not made", __func__, NULL);
    }

    pixSetOrClearBorder(pixd, 1, 1, 1, 1, PIX_SET);
    pixSeedfillBinary(pixd, pixd, pixsi, connectivity);
    pixOr(pixd, pixd, pixs);
    pixInvert(pixd, pixd);
    pixDestroy(&pixsi);
    return pixd;
}


/*!
 * \brief   pixFillClosedBorders()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    connectivity   filling connectivity 4 or 8
 * \return  pixd  all topologically outer closed borders are filled
 *                     as connected comonents, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Start with 1-pixel black border on otherwise white pixd
 *      (2) Subtract input pixs to remove border pixels that were
 *          also on the closed border
 *      (3) Use the inverted pixs as the filling mask to fill in
 *          all the pixels from the outer border to the closed border
 *          on pixs
 *      (4) Invert the result to get the filled component, including
 *          the input border
 *      (5) If the borders are 4-c.c., use 8-c.c. filling, and v.v.
 *      (6) Closed borders within c.c. that represent holes, etc., are filled.
 * </pre>
 */
PIX *
pixFillClosedBorders(PIX     *pixs,
                     l_int32  connectivity)
{
PIX  *pixsi, *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

    if ((pixd = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixSetOrClearBorder(pixd, 1, 1, 1, 1, PIX_SET);
    pixSubtract(pixd, pixd, pixs);
    if ((pixsi = pixInvert(NULL, pixs)) == NULL) {
        pixDestroy(&pixd);
        return (PIX *)ERROR_PTR("pixsi not made", __func__, NULL);
    }

    pixSeedfillBinary(pixd, pixd, pixsi, connectivity);
    pixInvert(pixd, pixd);
    pixDestroy(&pixsi);

    return pixd;
}


/*!
 * \brief   pixExtractBorderConnComps()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    connectivity   filling connectivity 4 or 8
 * \return  pixd  all pixels in the src that are in connected
 *                components touching the border, or NULL on error
 */
PIX *
pixExtractBorderConnComps(PIX     *pixs,
                          l_int32  connectivity)
{
PIX  *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

        /* Start with 1 pixel wide black border as seed in pixd */
    if ((pixd = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixSetOrClearBorder(pixd, 1, 1, 1, 1, PIX_SET);

       /* Fill in pixd from the seed, using pixs as the filling mask.
        * This fills all components from pixs that are touching the border. */
    pixSeedfillBinary(pixd, pixd, pixs, connectivity);

    return pixd;
}


/*!
 * \brief   pixRemoveBorderConnComps()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    connectivity   filling connectivity 4 or 8
 * \return  pixd  all pixels in the src that are not touching the
 *                border or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This removes all fg components touching the border.
 * </pre>
 */
PIX *
pixRemoveBorderConnComps(PIX     *pixs,
                         l_int32  connectivity)
{
PIX  *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

       /* Fill from a 1 pixel wide seed at the border into all components
        * in pixs (the filling mask) that are touching the border */
    pixd = pixExtractBorderConnComps(pixs, connectivity);

       /* Save in pixd only those components in pixs not touching the border */
    pixXor(pixd, pixd, pixs);
    return pixd;
}


/*!
 * \brief   pixFillBgFromBorder()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    connectivity   filling connectivity 4 or 8
 * \return  pixd with the background c.c. touching the border
 *               filled to foreground, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This fills all bg components touching the border to fg.
 *          It is the photometric inverse of pixRemoveBorderConnComps().
 *      (2) Invert the result to get the "holes" left after this fill.
 *          This can be done multiple times, extracting holes within
 *          holes after each pair of fillings.  Specifically, this code
 *          peels away n successive embeddings of components:
 * \code
 *              pix1 = <initial image>
 *              for (i = 0; i < 2 * n; i++) {
 *                   pix2 = pixFillBgFromBorder(pix1, 8);
 *                   pixInvert(pix2, pix2);
 *                   pixDestroy(&pix1);
 *                   pix1 = pix2;
 *              }
 * \endcode
 * </pre>
 */
PIX *
pixFillBgFromBorder(PIX     *pixs,
                    l_int32  connectivity)
{
PIX  *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

       /* Invert to turn bg touching the border to a fg component.
        * Extract this by filling from a 1 pixel wide seed at the border. */
    pixInvert(pixs, pixs);
    pixd = pixExtractBorderConnComps(pixs, connectivity);
    pixInvert(pixs, pixs);  /* restore pixs */

       /* Bit-or the filled bg component with pixs */
    pixOr(pixd, pixd, pixs);
    return pixd;
}


/*-----------------------------------------------------------------------*
 *            Hole-filling of components to bounding rectangle           *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixFillHolesToBoundingRect()
 *
 * \param[in]    pixs         1 bpp
 * \param[in]    minsize      min number of pixels in the hole
 * \param[in]    maxhfract    max hole area as fraction of fg pixels in the cc
 * \param[in]    minfgfract   min fg area as fraction of bounding rectangle
 * \return  pixd   with some holes possibly filled and some c.c. possibly
 *                 expanded to their bounding rects, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does not fill holes that are smaller in area than 'minsize'.
 *          Use %minsize = 0 and %maxhfract = 1.0 to fill all holes.
 *      (2) This does not fill holes with an area larger than
 *          %maxhfract times the fg area of the c.c.
 *          Use 1.0 to fill all holes.
 *      (3) This does not expand the fg of the c.c. to bounding rect if
 *          the fg area is less than %minfgfract times the area of the
 *          bounding rect.  Use 1.0 to skip expanding to the bounding rect.
 *      (4) The decisions are made as follows:
 *           ~ Decide if we are filling the holes; if so, when using
 *             the fg area, include the filled holes.
 *           ~ Decide based on the fg area if we are filling to a bounding rect.
 *             If so, do it.
 *             If not, fill the holes if the condition is satisfied.
 *      (5) The choice of %minsize depends on the resolution.
 *      (6) For solidifying image mask regions on printed materials,
 *          which tend to be rectangular, values for %maxhfract
 *          and %minfgfract around 0.5 are reasonable.
 * </pre>
 */
PIX *
pixFillHolesToBoundingRect(PIX       *pixs,
                           l_int32    minsize,
                           l_float32  maxhfract,
                           l_float32  minfgfract)
{
l_int32    i, x, y, w, h, n, nfg, nh, ntot, area;
l_int32   *tab;
l_float32  hfract;  /* measured hole fraction */
l_float32  fgfract;  /* measured fg fraction */
BOXA      *boxa;
PIX       *pixd, *pixfg, *pixh;
PIXA      *pixa;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    maxhfract = L_MIN(L_MAX(maxhfract, 0.0), 1.0);
    minfgfract = L_MIN(L_MAX(minfgfract, 0.0), 1.0);

    pixd = pixCopy(NULL, pixs);
    boxa = pixConnComp(pixd, &pixa, 8);
    n = boxaGetCount(boxa);
    tab = makePixelSumTab8();
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, &x, &y, &w, &h);
        area = w * h;
        if (area < minsize)
            continue;
        pixfg = pixaGetPix(pixa, i, L_COPY);
        pixh = pixHolesByFilling(pixfg, 4);  /* holes only */
        pixCountPixels(pixfg, &nfg, tab);
        pixCountPixels(pixh, &nh, tab);
        hfract = (l_float32)nh / (l_float32)nfg;
        ntot = nfg;
        if (hfract <= maxhfract)  /* we will fill the holes (at least) */
            ntot = nfg + nh;
        fgfract = (l_float32)ntot / (l_float32)area;
        if (fgfract >= minfgfract) {  /* fill to bounding rect */
            pixSetAll(pixfg);
            pixRasterop(pixd, x, y, w, h, PIX_SRC, pixfg, 0, 0);
        } else if (hfract <= maxhfract) {  /* fill just the holes */
            pixRasterop(pixd, x, y, w, h, PIX_DST | PIX_SRC , pixh, 0, 0);
        }
        pixDestroy(&pixfg);
        pixDestroy(&pixh);
    }
    boxaDestroy(&boxa);
    pixaDestroy(&pixa);
    LEPT_FREE(tab);
    return pixd;
}


/*-----------------------------------------------------------------------*
 *               Vincent's hybrid Grayscale Seedfill method              *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixSeedfillGray()
 *
 * \param[in]    pixs           8 bpp seed; filled in place
 * \param[in]    pixm           8 bpp filling mask
 * \param[in]    connectivity   4 or 8
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place filling operation on the seed, pixs,
 *          where the clipping mask is always above or at the level
 *          of the seed as it is filled.
 *      (2) For details of the operation, see the description in
 *          seedfillGrayLow() and the code there.
 *      (3) As an example of use, see the description in pixHDome().
 *          There, the seed is an image where each pixel is a fixed
 *          amount smaller than the corresponding mask pixel.
 *      (4) Reference paper :
 *            L. Vincent, Morphological grayscale reconstruction in image
 *            analysis: applications and efficient algorithms, IEEE Transactions
 *            on  Image Processing, vol. 2, no. 2, pp. 176-201, 1993.
 * </pre>
 */
l_ok
pixSeedfillGray(PIX     *pixs,
                PIX     *pixm,
                l_int32  connectivity)
{
l_int32    h, w, wpls, wplm;
l_uint32  *datas, *datam;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 8)
        return ERROR_INT("pixm not defined or not 8 bpp", __func__, 1);
    if (connectivity != 4 && connectivity != 8)
        return ERROR_INT("connectivity not in {4,8}", __func__, 1);

        /* Make sure the sizes of seed and mask images are the same */
    if (pixSizesEqual(pixs, pixm) == 0)
        return ERROR_INT("pixs and pixm sizes differ", __func__, 1);

    datas = pixGetData(pixs);
    datam = pixGetData(pixm);
    wpls = pixGetWpl(pixs);
    wplm = pixGetWpl(pixm);
    pixGetDimensions(pixs, &w, &h, NULL);
    seedfillGrayLow(datas, w, h, wpls, datam, wplm, connectivity);

    return 0;
}


/*!
 * \brief   pixSeedfillGrayInv()
 *
 * \param[in]    pixs           8 bpp seed; filled in place
 * \param[in]    pixm           8 bpp filling mask
 * \param[in]    connectivity   4 or 8
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place filling operation on the seed, pixs,
 *          where the clipping mask is always below or at the level
 *          of the seed as it is filled.  Think of filling up a basin
 *          to a particular level, given by the maximum seed value
 *          in the basin.  Outside the filled region, the mask
 *          is above the filling level.
 *      (2) Contrast this with pixSeedfillGray(), where the clipping mask
 *          is always above or at the level of the fill.  An example
 *          of its use is the hdome fill, where the seed is an image
 *          where each pixel is a fixed amount smaller than the
 *          corresponding mask pixel.
 *      (3) The basin fill, pixSeedfillGrayBasin(), is a special case
 *          where the seed pixel values are generated from the mask,
 *          and where the implementation uses pixSeedfillGray() by
 *          inverting both the seed and mask.
 * </pre>
 */
l_ok
pixSeedfillGrayInv(PIX     *pixs,
                   PIX     *pixm,
                   l_int32  connectivity)
{
l_int32    h, w, wpls, wplm;
l_uint32  *datas, *datam;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 8)
        return ERROR_INT("pixm not defined or not 8 bpp", __func__, 1);
    if (connectivity != 4 && connectivity != 8)
        return ERROR_INT("connectivity not in {4,8}", __func__, 1);

        /* Make sure the sizes of seed and mask images are the same */
    if (pixSizesEqual(pixs, pixm) == 0)
        return ERROR_INT("pixs and pixm sizes differ", __func__, 1);

    datas = pixGetData(pixs);
    datam = pixGetData(pixm);
    wpls = pixGetWpl(pixs);
    wplm = pixGetWpl(pixm);
    pixGetDimensions(pixs, &w, &h, NULL);
    seedfillGrayInvLow(datas, w, h, wpls, datam, wplm, connectivity);

    return 0;
}


/*!
 * \brief   seedfillGrayLow()
 *
 *  Notes:
 *      (1) The pixels are numbered as follows:
 *              1  2  3
 *              4  x  5
 *              6  7  8
 *          This low-level filling operation consists of two scans,
 *          raster and anti-raster, covering the entire seed image.
 *          This is followed by a breadth-first propagation operation to
 *          complete the fill.
 *          During the anti-raster scan, every pixel p whose current value
 *          could still be propagated after the anti-raster scan is put into
 *          the FIFO queue.
 *          The propagation step is a breadth-first fill to completion.
 *          Unlike the simple grayscale seedfill pixSeedfillGraySimple(),
 *          where at least two full raster/anti-raster iterations are required
 *          for completion and verification, the hybrid method uses only a
 *          single raster/anti-raster set of scans.
 *      (2) The filling action can be visualized from the following example.
 *          Suppose the mask, which clips the fill, is a sombrero-shaped
 *          surface, where the highest point is 200 and the low pixels
 *          around the rim are 30.  Beyond the rim, the mask goes up a bit.
 *          Suppose the seed, which is filled, consists of a single point
 *          of height 150, located below the max of the mask, with
 *          the rest 0.  Then in the raster scan, nothing happens until
 *          the high seed point is encountered, and then this value is
 *          propagated right and down, until it hits the side of the
 *          sombrero.   The seed can never exceed the mask, so it fills
 *          to the rim, going lower along the mask surface.  When it
 *          passes the rim, the seed continues to fill at the rim
 *          height to the edge of the seed image.  Then on the
 *          anti-raster scan, the seed fills flat inside the
 *          sombrero to the upper and left, and then out from the
 *          rim as before.  The final result has a seed that is
 *          flat outside the rim, and inside it fills the sombrero
 *          but only up to 150.  If the rim height varies, the
 *          filled seed outside the rim will be at the highest
 *          point on the rim, which is a saddle point on the rim.
 *      (3) Reference paper :
 *            L. Vincent, Morphological grayscale reconstruction in image
 *            analysis: applications and efficient algorithms, IEEE Transactions
 *            on  Image Processing, vol. 2, no. 2, pp. 176-201, 1993.
 */
static void
seedfillGrayLow(l_uint32  *datas,
                l_int32    w,
                l_int32    h,
                l_int32    wpls,
                l_uint32  *datam,
                l_int32    wplm,
                l_int32    connectivity)
{
l_uint8    val1, val2, val3, val4, val5, val6, val7, val8;
l_uint8    val, maxval, maskval, boolval;
l_int32    i, j, imax, jmax, queue_size;
l_uint32  *lines, *linem;
L_PIXEL *pixel;
L_QUEUE  *lq_pixel;

    if (connectivity != 4 && connectivity != 8) {
        L_ERROR("connectivity must be 4 or 8\n", __func__);
        return;
    }

    imax = h - 1;
    jmax = w - 1;

        /* In the worst case, most of the pixels could be pushed
         * onto the FIFO queue during anti-raster scan.  However this
         * will rarely happen, and we initialize the queue ptr size to
         * the image perimeter. */
    lq_pixel = lqueueCreate(2 * (w + h));

    switch (connectivity)
    {
    case 4:
            /* UL --> LR scan  (Raster Order)
             * If I : mask image
             *    J : marker image
             * Let p be the currect pixel;
             * J(p) <- (max{J(p) union J(p) neighbors in raster order})
             *          intersection I(p) */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < w; j++) {
                if ((maskval = GET_DATA_BYTE(linem, j)) > 0) {
                    maxval = 0;
                    if (i > 0)
                        maxval = GET_DATA_BYTE(lines - wpls, j);
                    if (j > 0) {
                        val4 = GET_DATA_BYTE(lines, j - 1);
                        maxval = L_MAX(maxval, val4);
                    }
                    val = GET_DATA_BYTE(lines, j);
                    maxval = L_MAX(maxval, val);
                    val = L_MIN(maxval, maskval);
                    SET_DATA_BYTE(lines, j, val);
                }
            }
        }

            /* LR --> UL scan (anti-raster order)
             * Let p be the currect pixel;
             * J(p) <- (max{J(p) union J(p) neighbors in anti-raster order})
             *          intersection I(p) */
        for (i = imax; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = jmax; j >= 0; j--) {
                boolval = FALSE;
                if ((maskval = GET_DATA_BYTE(linem, j)) > 0) {
                    maxval = 0;
                    if (i < imax)
                        maxval = GET_DATA_BYTE(lines + wpls, j);
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        maxval = L_MAX(maxval, val5);
                    }
                    val = GET_DATA_BYTE(lines, j);
                    maxval = L_MAX(maxval, val);
                    val = L_MIN(maxval, maskval);
                    SET_DATA_BYTE(lines, j, val);

                        /*
                         * If there exists a point (q) which belongs to J(p)
                         * neighbors in anti-raster order such that J(q) < J(p)
                         * and J(q) < I(q) then
                         * fifo_add(p) */
                    if (i < imax) {
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        if ((val7 < val) &&
                            (val7 < GET_DATA_BYTE(linem + wplm, j))) {
                            boolval = TRUE;
                        }
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        if (!boolval && (val5 < val) &&
                            (val5 < GET_DATA_BYTE(linem, j + 1))) {
                            boolval = TRUE;
                        }
                    }
                    if (boolval) {
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
            }
        }

            /* Propagation step:
             *        while fifo_empty = false
             *          p <- fifo_first()
             *          for every pixel (q) belong to neighbors of (p)
             *            if J(q) < J(p) and I(q) != J(q)
             *              J(q) <- min(J(p), I(q));
             *              fifo_add(q);
             *            end
             *          end
             *        end */
        queue_size = lqueueGetCount(lq_pixel);
        while (queue_size) {
            pixel = (L_PIXEL *)lqueueRemove(lq_pixel);
            i = pixel->x;
            j = pixel->y;
            LEPT_FREE(pixel);
            lines = datas + i * wpls;
            linem = datam + i * wplm;

            if ((val = GET_DATA_BYTE(lines, j)) > 0) {
                if (i > 0) {
                    val2 = GET_DATA_BYTE(lines - wpls, j);
                    maskval = GET_DATA_BYTE(linem - wplm, j);
                    if (val > val2 && val2 != maskval) {
                        SET_DATA_BYTE(lines - wpls, j, L_MIN(val, maskval));
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i - 1;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }

                }
                if (j > 0) {
                    val4 = GET_DATA_BYTE(lines, j - 1);
                    maskval = GET_DATA_BYTE(linem, j - 1);
                    if (val > val4 && val4 != maskval) {
                        SET_DATA_BYTE(lines, j - 1, L_MIN(val, maskval));
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j - 1;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
                if (i < imax) {
                    val7 = GET_DATA_BYTE(lines + wpls, j);
                    maskval = GET_DATA_BYTE(linem + wplm, j);
                    if (val > val7 && val7 != maskval) {
                        SET_DATA_BYTE(lines + wpls, j, L_MIN(val, maskval));
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i + 1;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
                if (j < jmax) {
                    val5 = GET_DATA_BYTE(lines, j + 1);
                    maskval = GET_DATA_BYTE(linem, j + 1);
                    if (val > val5 && val5 != maskval) {
                        SET_DATA_BYTE(lines, j + 1, L_MIN(val, maskval));
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j + 1;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
            }

            queue_size = lqueueGetCount(lq_pixel);
        }
        break;

    case 8:
            /* UL --> LR scan  (Raster Order)
             * If I : mask image
             *    J : marker image
             * Let p be the currect pixel;
             * J(p) <- (max{J(p) union J(p) neighbors in raster order})
             *          intersection I(p) */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < w; j++) {
                if ((maskval = GET_DATA_BYTE(linem, j)) > 0) {
                    maxval = 0;
                    if (i > 0) {
                        if (j > 0)
                            maxval = GET_DATA_BYTE(lines - wpls, j - 1);
                        if (j < jmax) {
                            val3 = GET_DATA_BYTE(lines - wpls, j + 1);
                            maxval = L_MAX(maxval, val3);
                        }
                        val2 = GET_DATA_BYTE(lines - wpls, j);
                        maxval = L_MAX(maxval, val2);
                    }
                    if (j > 0) {
                        val4 = GET_DATA_BYTE(lines, j - 1);
                        maxval = L_MAX(maxval, val4);
                    }
                    val = GET_DATA_BYTE(lines, j);
                    maxval = L_MAX(maxval, val);
                    val = L_MIN(maxval, maskval);
                    SET_DATA_BYTE(lines, j, val);
                }
            }
        }

            /* LR --> UL scan (anti-raster order)
             * Let p be the currect pixel;
             * J(p) <- (max{J(p) union J(p) neighbors in anti-raster order})
             *          intersection I(p) */
        for (i = imax; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = jmax; j >= 0; j--) {
                boolval = FALSE;
                if ((maskval = GET_DATA_BYTE(linem, j)) > 0) {
                    maxval = 0;
                    if (i < imax) {
                        if (j > 0) {
                            maxval = GET_DATA_BYTE(lines + wpls, j - 1);
                        }
                        if (j < jmax) {
                            val8 = GET_DATA_BYTE(lines + wpls, j + 1);
                            maxval = L_MAX(maxval, val8);
                        }
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        maxval = L_MAX(maxval, val7);
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        maxval = L_MAX(maxval, val5);
                    }
                    val = GET_DATA_BYTE(lines, j);
                    maxval = L_MAX(maxval, val);
                    val = L_MIN(maxval, maskval);
                    SET_DATA_BYTE(lines, j, val);

                        /* If there exists a point (q) which belongs to J(p)
                         * neighbors in anti-raster order such that J(q) < J(p)
                         * and J(q) < I(q) then
                         * fifo_add(p) */
                    if (i < imax) {
                        if (j > 0) {
                            val6 = GET_DATA_BYTE(lines + wpls, j - 1);
                            if ((val6 < val) &&
                                (val6 < GET_DATA_BYTE(linem + wplm, j - 1))) {
                                boolval = TRUE;
                            }
                        }
                        if (j < jmax) {
                            val8 = GET_DATA_BYTE(lines + wpls, j + 1);
                            if (!boolval && (val8 < val) &&
                                (val8 < GET_DATA_BYTE(linem + wplm, j + 1))) {
                                boolval = TRUE;
                            }
                        }
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        if (!boolval && (val7 < val) &&
                            (val7 < GET_DATA_BYTE(linem + wplm, j))) {
                            boolval = TRUE;
                        }
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        if (!boolval && (val5 < val) &&
                            (val5 < GET_DATA_BYTE(linem, j + 1))) {
                            boolval = TRUE;
                        }
                    }
                    if (boolval) {
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
            }
        }

            /* Propagation step:
             *        while fifo_empty = false
             *          p <- fifo_first()
             *          for every pixel (q) belong to neighbors of (p)
             *            if J(q) < J(p) and I(q) != J(q)
             *              J(q) <- min(J(p), I(q));
             *              fifo_add(q);
             *            end
             *          end
             *        end */
        queue_size = lqueueGetCount(lq_pixel);
        while (queue_size) {
            pixel = (L_PIXEL *)lqueueRemove(lq_pixel);
            i = pixel->x;
            j = pixel->y;
            LEPT_FREE(pixel);
            lines = datas + i * wpls;
            linem = datam + i * wplm;

            if ((val = GET_DATA_BYTE(lines, j)) > 0) {
                if (i > 0) {
                    if (j > 0) {
                        val1 = GET_DATA_BYTE(lines - wpls, j - 1);
                        maskval = GET_DATA_BYTE(linem - wplm, j - 1);
                        if (val > val1 && val1 != maskval) {
                            SET_DATA_BYTE(lines - wpls, j - 1,
                                          L_MIN(val, maskval));
                            pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                            pixel->x = i - 1;
                            pixel->y = j - 1;
                            lqueueAdd(lq_pixel, pixel);
                        }
                    }
                    if (j < jmax) {
                        val3 = GET_DATA_BYTE(lines - wpls, j + 1);
                        maskval = GET_DATA_BYTE(linem - wplm, j + 1);
                        if (val > val3 && val3 != maskval) {
                            SET_DATA_BYTE(lines - wpls, j + 1,
                                          L_MIN(val, maskval));
                            pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                            pixel->x = i - 1;
                            pixel->y = j + 1;
                            lqueueAdd(lq_pixel, pixel);
                        }
                    }
                    val2 = GET_DATA_BYTE(lines - wpls, j);
                    maskval = GET_DATA_BYTE(linem - wplm, j);
                    if (val > val2 && val2 != maskval) {
                        SET_DATA_BYTE(lines - wpls, j, L_MIN(val, maskval));
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i - 1;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }

                }
                if (j > 0) {
                    val4 = GET_DATA_BYTE(lines, j - 1);
                    maskval = GET_DATA_BYTE(linem, j - 1);
                    if (val > val4 && val4 != maskval) {
                        SET_DATA_BYTE(lines, j - 1, L_MIN(val, maskval));
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j - 1;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
                if (i < imax) {
                    if (j > 0) {
                        val6 = GET_DATA_BYTE(lines + wpls, j - 1);
                        maskval = GET_DATA_BYTE(linem + wplm, j - 1);
                        if (val > val6 && val6 != maskval) {
                            SET_DATA_BYTE(lines + wpls, j - 1,
                                          L_MIN(val, maskval));
                            pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                            pixel->x = i + 1;
                            pixel->y = j - 1;
                            lqueueAdd(lq_pixel, pixel);
                        }
                    }
                    if (j < jmax) {
                        val8 = GET_DATA_BYTE(lines + wpls, j + 1);
                        maskval = GET_DATA_BYTE(linem + wplm, j + 1);
                        if (val > val8 && val8 != maskval) {
                            SET_DATA_BYTE(lines + wpls, j + 1,
                                          L_MIN(val, maskval));
                            pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                            pixel->x = i + 1;
                            pixel->y = j + 1;
                            lqueueAdd(lq_pixel, pixel);
                        }
                    }
                    val7 = GET_DATA_BYTE(lines + wpls, j);
                    maskval = GET_DATA_BYTE(linem + wplm, j);
                    if (val > val7 && val7 != maskval) {
                        SET_DATA_BYTE(lines + wpls, j, L_MIN(val, maskval));
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i + 1;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
                if (j < jmax) {
                    val5 = GET_DATA_BYTE(lines, j + 1);
                    maskval = GET_DATA_BYTE(linem, j + 1);
                    if (val > val5 && val5 != maskval) {
                        SET_DATA_BYTE(lines, j + 1, L_MIN(val, maskval));
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j + 1;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
            }

            queue_size = lqueueGetCount(lq_pixel);
        }
        break;

    default:
        L_ERROR("shouldn't get here!\n", __func__);
    }

    lqueueDestroy(&lq_pixel, TRUE);
}


/*!
 * \brief   seedfillGrayInvLow()
 *
 *  Notes:
 *      (1) The pixels are numbered as follows:
 *              1  2  3
 *              4  x  5
 *              6  7  8
 *          This low-level filling operation consists of two scans,
 *          raster and anti-raster, covering the entire seed image.
 *          During the anti-raster scan, every pixel p such that its
 *          current value could still be propagated during the next
 *          raster scanning is put into the FIFO-queue.
 *          Next step is the propagation step where where we update
 *          and propagate the values using FIFO structure created in
 *          anti-raster scan.
 *      (2) The "Inv" signifies the fact that in this case, filling
 *          of the seed only takes place when the seed value is
 *          greater than the mask value.  The mask will act to stop
 *          the fill when it is higher than the seed level.  (This is
 *          in contrast to conventional grayscale filling where the
 *          seed always fills below the mask.)
 *      (3) An example of use is a basin, described by the mask (pixm),
 *          where within the basin, the seed pix (pixs) gets filled to the
 *          height of the highest seed pixel that is above its
 *          corresponding max pixel.  Filling occurs while the
 *          propagating seed pixels in pixs are larger than the
 *          corresponding mask values in pixm.
 *      (4) Reference paper :
 *            L. Vincent, Morphological grayscale reconstruction in image
 *            analysis: applications and efficient algorithms, IEEE Transactions
 *            on  Image Processing, vol. 2, no. 2, pp. 176-201, 1993.
 */
static void
seedfillGrayInvLow(l_uint32  *datas,
                   l_int32    w,
                   l_int32    h,
                   l_int32    wpls,
                   l_uint32  *datam,
                   l_int32    wplm,
                   l_int32    connectivity)
{
l_uint8    val1, val2, val3, val4, val5, val6, val7, val8;
l_uint8    val, maxval, maskval, boolval;
l_int32    i, j, imax, jmax, queue_size;
l_uint32  *lines, *linem;
L_PIXEL *pixel;
L_QUEUE  *lq_pixel;

    if (connectivity != 4 && connectivity != 8) {
        L_ERROR("connectivity must be 4 or 8\n", __func__);
        return;
    }

    imax = h - 1;
    jmax = w - 1;

        /* In the worst case, most of the pixels could be pushed
         * onto the FIFO queue during anti-raster scan.  However this
         * will rarely happen, and we initialize the queue ptr size to
         * the image perimeter. */
    lq_pixel = lqueueCreate(2 * (w + h));

    switch (connectivity)
    {
    case 4:
            /* UL --> LR scan  (Raster Order)
             * If I : mask image
             *    J : marker image
             * Let p be the currect pixel;
             * tmp <- max{J(p) union J(p) neighbors in raster order}
             * if (tmp > I(p))
             *   J(p) <- tmp
             * end */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < w; j++) {
                if ((maskval = GET_DATA_BYTE(linem, j)) < 255) {
                    maxval = GET_DATA_BYTE(lines, j);
                    if (i > 0) {
                        val2 = GET_DATA_BYTE(lines - wpls, j);
                        maxval = L_MAX(maxval, val2);
                    }
                    if (j > 0) {
                        val4 = GET_DATA_BYTE(lines, j - 1);
                        maxval = L_MAX(maxval, val4);
                    }
                    if (maxval > maskval)
                        SET_DATA_BYTE(lines, j, maxval);
                }
            }
        }

            /* LR --> UL scan (anti-raster order)
             * If I : mask image
             *    J : marker image
             * Let p be the currect pixel;
             * tmp <- max{J(p) union J(p) neighbors in anti-raster order}
             * if (tmp > I(p))
             *   J(p) <- tmp
             * end */
        for (i = imax; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = jmax; j >= 0; j--) {
                boolval = FALSE;
                if ((maskval = GET_DATA_BYTE(linem, j)) < 255) {
                    val = maxval = GET_DATA_BYTE(lines, j);
                    if (i < imax) {
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        maxval = L_MAX(maxval, val7);
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        maxval = L_MAX(maxval, val5);
                    }
                    if (maxval > maskval)
                        SET_DATA_BYTE(lines, j, maxval);
                    val = GET_DATA_BYTE(lines, j);

                        /*
                         * If there exists a point (q) which belongs to J(p)
                         * neighbors in anti-raster order such that J(q) < J(p)
                         * and J(p) > I(q) then
                         * fifo_add(p) */
                    if (i < imax) {
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        if ((val7 < val) &&
                            (val > GET_DATA_BYTE(linem + wplm, j))) {
                            boolval = TRUE;
                        }
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        if (!boolval && (val5 < val) &&
                            (val > GET_DATA_BYTE(linem, j + 1))) {
                            boolval = TRUE;
                        }
                    }
                    if (boolval) {
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
            }
        }

            /* Propagation step:
             *        while fifo_empty = false
             *          p <- fifo_first()
             *          for every pixel (q) belong to neighbors of (p)
             *            if J(q) < J(p) and J(p) > I(q)
             *              J(q) <- min(J(p), I(q));
             *              fifo_add(q);
             *            end
             *          end
             *        end */
        queue_size = lqueueGetCount(lq_pixel);
        while (queue_size) {
            pixel = (L_PIXEL *)lqueueRemove(lq_pixel);
            i = pixel->x;
            j = pixel->y;
            LEPT_FREE(pixel);
            lines = datas + i * wpls;
            linem = datam + i * wplm;

            if ((val = GET_DATA_BYTE(lines, j)) > 0) {
                if (i > 0) {
                    val2 = GET_DATA_BYTE(lines - wpls, j);
                    maskval = GET_DATA_BYTE(linem - wplm, j);
                    if (val > val2 && val > maskval) {
                        SET_DATA_BYTE(lines - wpls, j, val);
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i - 1;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }

                }
                if (j > 0) {
                    val4 = GET_DATA_BYTE(lines, j - 1);
                    maskval = GET_DATA_BYTE(linem, j - 1);
                    if (val > val4 && val > maskval) {
                        SET_DATA_BYTE(lines, j - 1, val);
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j - 1;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
                if (i < imax) {
                    val7 = GET_DATA_BYTE(lines + wpls, j);
                    maskval = GET_DATA_BYTE(linem + wplm, j);
                    if (val > val7 && val > maskval) {
                        SET_DATA_BYTE(lines + wpls, j, val);
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i + 1;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
                if (j < jmax) {
                    val5 = GET_DATA_BYTE(lines, j + 1);
                    maskval = GET_DATA_BYTE(linem, j + 1);
                    if (val > val5 && val > maskval) {
                        SET_DATA_BYTE(lines, j + 1, val);
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j + 1;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
            }

            queue_size = lqueueGetCount(lq_pixel);
        }
        break;

    case 8:
            /* UL --> LR scan  (Raster Order)
             * If I : mask image
             *    J : marker image
             * Let p be the currect pixel;
             * tmp <- max{J(p) union J(p) neighbors in raster order}
             * if (tmp > I(p))
             *   J(p) <- tmp
             * end */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < w; j++) {
                if ((maskval = GET_DATA_BYTE(linem, j)) < 255) {
                    maxval = GET_DATA_BYTE(lines, j);
                    if (i > 0) {
                        if (j > 0) {
                            val1 = GET_DATA_BYTE(lines - wpls, j - 1);
                            maxval = L_MAX(maxval, val1);
                        }
                        if (j < jmax) {
                            val3 = GET_DATA_BYTE(lines - wpls, j + 1);
                            maxval = L_MAX(maxval, val3);
                        }
                        val2 = GET_DATA_BYTE(lines - wpls, j);
                        maxval = L_MAX(maxval, val2);
                    }
                    if (j > 0) {
                        val4 = GET_DATA_BYTE(lines, j - 1);
                        maxval = L_MAX(maxval, val4);
                    }
                    if (maxval > maskval)
                        SET_DATA_BYTE(lines, j, maxval);
                }
            }
        }

            /* LR --> UL scan (anti-raster order)
             * If I : mask image
             *    J : marker image
             * Let p be the currect pixel;
             * tmp <- max{J(p) union J(p) neighbors in anti-raster order}
             * if (tmp > I(p))
             *   J(p) <- tmp
             * end */
        for (i = imax; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = jmax; j >= 0; j--) {
                boolval = FALSE;
                if ((maskval = GET_DATA_BYTE(linem, j)) < 255) {
                    maxval = GET_DATA_BYTE(lines, j);
                    if (i < imax) {
                        if (j > 0) {
                            val6 = GET_DATA_BYTE(lines + wpls, j - 1);
                            maxval = L_MAX(maxval, val6);
                        }
                        if (j < jmax) {
                            val8 = GET_DATA_BYTE(lines + wpls, j + 1);
                            maxval = L_MAX(maxval, val8);
                        }
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        maxval = L_MAX(maxval, val7);
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        maxval = L_MAX(maxval, val5);
                    }
                    if (maxval > maskval)
                        SET_DATA_BYTE(lines, j, maxval);
                    val = GET_DATA_BYTE(lines, j);

                        /*
                         * If there exists a point (q) which belongs to J(p)
                         * neighbors in anti-raster order such that J(q) < J(p)
                         * and J(p) > I(q) then
                         * fifo_add(p) */
                    if (i < imax) {
                        if (j > 0) {
                            val6 = GET_DATA_BYTE(lines + wpls, j - 1);
                            if ((val6 < val) &&
                                (val > GET_DATA_BYTE(linem + wplm, j - 1))) {
                                boolval = TRUE;
                            }
                        }
                        if (j < jmax) {
                            val8 = GET_DATA_BYTE(lines + wpls, j + 1);
                            if (!boolval && (val8 < val) &&
                                (val > GET_DATA_BYTE(linem + wplm, j + 1))) {
                                boolval = TRUE;
                            }
                        }
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        if (!boolval && (val7 < val) &&
                            (val > GET_DATA_BYTE(linem + wplm, j))) {
                            boolval = TRUE;
                        }
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        if (!boolval && (val5 < val) &&
                            (val > GET_DATA_BYTE(linem, j + 1))) {
                            boolval = TRUE;
                        }
                    }
                    if (boolval) {
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
            }
        }

            /* Propagation step:
             *        while fifo_empty = false
             *          p <- fifo_first()
             *          for every pixel (q) belong to neighbors of (p)
             *            if J(q) < J(p) and J(p) > I(q)
             *              J(q) <- min(J(p), I(q));
             *              fifo_add(q);
             *            end
             *          end
             *        end */
        queue_size = lqueueGetCount(lq_pixel);
        while (queue_size) {
            pixel = (L_PIXEL *)lqueueRemove(lq_pixel);
            i = pixel->x;
            j = pixel->y;
            LEPT_FREE(pixel);
            lines = datas + i * wpls;
            linem = datam + i * wplm;

            if ((val = GET_DATA_BYTE(lines, j)) > 0) {
                if (i > 0) {
                    if (j > 0) {
                        val1 = GET_DATA_BYTE(lines - wpls, j - 1);
                        maskval = GET_DATA_BYTE(linem - wplm, j - 1);
                        if (val > val1 && val > maskval) {
                            SET_DATA_BYTE(lines - wpls, j - 1, val);
                            pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                            pixel->x = i - 1;
                            pixel->y = j - 1;
                            lqueueAdd(lq_pixel, pixel);
                        }
                    }
                    if (j < jmax) {
                        val3 = GET_DATA_BYTE(lines - wpls, j + 1);
                        maskval = GET_DATA_BYTE(linem - wplm, j + 1);
                        if (val > val3 && val > maskval) {
                            SET_DATA_BYTE(lines - wpls, j + 1, val);
                            pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                            pixel->x = i - 1;
                            pixel->y = j + 1;
                            lqueueAdd(lq_pixel, pixel);
                        }
                    }
                    val2 = GET_DATA_BYTE(lines - wpls, j);
                    maskval = GET_DATA_BYTE(linem - wplm, j);
                    if (val > val2 && val > maskval) {
                        SET_DATA_BYTE(lines - wpls, j, val);
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i - 1;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }

                }
                if (j > 0) {
                    val4 = GET_DATA_BYTE(lines, j - 1);
                    maskval = GET_DATA_BYTE(linem, j - 1);
                    if (val > val4 && val > maskval) {
                        SET_DATA_BYTE(lines, j - 1, val);
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j - 1;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
                if (i < imax) {
                    if (j > 0) {
                        val6 = GET_DATA_BYTE(lines + wpls, j - 1);
                        maskval = GET_DATA_BYTE(linem + wplm, j - 1);
                        if (val > val6 && val > maskval) {
                            SET_DATA_BYTE(lines + wpls, j - 1, val);
                            pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                            pixel->x = i + 1;
                            pixel->y = j - 1;
                            lqueueAdd(lq_pixel, pixel);
                        }
                    }
                    if (j < jmax) {
                        val8 = GET_DATA_BYTE(lines + wpls, j + 1);
                        maskval = GET_DATA_BYTE(linem + wplm, j + 1);
                        if (val > val8 && val > maskval) {
                            SET_DATA_BYTE(lines + wpls, j + 1, val);
                            pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                            pixel->x = i + 1;
                            pixel->y = j + 1;
                            lqueueAdd(lq_pixel, pixel);
                        }
                    }
                    val7 = GET_DATA_BYTE(lines + wpls, j);
                    maskval = GET_DATA_BYTE(linem + wplm, j);
                    if (val > val7 && val > maskval) {
                        SET_DATA_BYTE(lines + wpls, j, val);
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i + 1;
                        pixel->y = j;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
                if (j < jmax) {
                    val5 = GET_DATA_BYTE(lines, j + 1);
                    maskval = GET_DATA_BYTE(linem, j + 1);
                    if (val > val5 && val > maskval) {
                        SET_DATA_BYTE(lines, j + 1, val);
                        pixel = (L_PIXEL *)LEPT_CALLOC(1, sizeof(L_PIXEL));
                        pixel->x = i;
                        pixel->y = j + 1;
                        lqueueAdd(lq_pixel, pixel);
                    }
                }
            }

            queue_size = lqueueGetCount(lq_pixel);
        }
        break;

    default:
        L_ERROR("shouldn't get here!\n", __func__);
    }

    lqueueDestroy(&lq_pixel, TRUE);
}


/*-----------------------------------------------------------------------*
 *             Vincent's Iterative Grayscale Seedfill method             *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixSeedfillGraySimple()
 *
 * \param[in]    pixs           8 bpp seed; filled in place
 * \param[in]    pixm           8 bpp filling mask
 * \param[in]    connectivity   4 or 8
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place filling operation on the seed, pixs,
 *          where the clipping mask is always above or at the level
 *          of the seed as it is filled.
 *      (2) For details of the operation, see the description in
 *          seedfillGrayLowSimple() and the code there.
 *      (3) As an example of use, see the description in pixHDome().
 *          There, the seed is an image where each pixel is a fixed
 *          amount smaller than the corresponding mask pixel.
 *      (4) Reference paper :
 *            L. Vincent, Morphological grayscale reconstruction in image
 *            analysis: applications and efficient algorithms, IEEE Transactions
 *            on  Image Processing, vol. 2, no. 2, pp. 176-201, 1993.
 * </pre>
 */
l_ok
pixSeedfillGraySimple(PIX     *pixs,
                      PIX     *pixm,
                      l_int32  connectivity)
{
l_int32    i, h, w, wpls, wplm, boolval;
l_uint32  *datas, *datam;
PIX       *pixt;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 8)
        return ERROR_INT("pixm not defined or not 8 bpp", __func__, 1);
    if (connectivity != 4 && connectivity != 8)
        return ERROR_INT("connectivity not in {4,8}", __func__, 1);

        /* Make sure the sizes of seed and mask images are the same */
    if (pixSizesEqual(pixs, pixm) == 0)
        return ERROR_INT("pixs and pixm sizes differ", __func__, 1);

        /* This is used to test for completion */
    if ((pixt = pixCreateTemplate(pixs)) == NULL)
        return ERROR_INT("pixt not made", __func__, 1);

    datas = pixGetData(pixs);
    datam = pixGetData(pixm);
    wpls = pixGetWpl(pixs);
    wplm = pixGetWpl(pixm);
    pixGetDimensions(pixs, &w, &h, NULL);
    for (i = 0; i < MaxIters; i++) {
        pixCopy(pixt, pixs);
        seedfillGrayLowSimple(datas, w, h, wpls, datam, wplm, connectivity);
        pixEqual(pixs, pixt, &boolval);
        if (boolval == 1) {
#if DEBUG_PRINT_ITERS
            L_INFO("Gray seed fill converged: %d iters\n", __func__, i + 1);
#endif  /* DEBUG_PRINT_ITERS */
            break;
        }
    }

    pixDestroy(&pixt);
    return 0;
}


/*!
 * \brief   pixSeedfillGrayInvSimple()
 *
 * \param[in]    pixs           8 bpp seed; filled in place
 * \param[in]    pixm           8 bpp filling mask
 * \param[in]    connectivity   4 or 8
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place filling operation on the seed, pixs,
 *          where the clipping mask is always below or at the level
 *          of the seed as it is filled.  Think of filling up a basin
 *          to a particular level, given by the maximum seed value
 *          in the basin.  Outside the filled region, the mask
 *          is above the filling level.
 *      (2) Contrast this with pixSeedfillGraySimple(), where the clipping mask
 *          is always above or at the level of the fill.  An example
 *          of its use is the hdome fill, where the seed is an image
 *          where each pixel is a fixed amount smaller than the
 *          corresponding mask pixel.
 * </pre>
 */
l_ok
pixSeedfillGrayInvSimple(PIX     *pixs,
                         PIX     *pixm,
                         l_int32  connectivity)
{
l_int32    i, h, w, wpls, wplm, boolval;
l_uint32  *datas, *datam;
PIX       *pixt;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 8)
        return ERROR_INT("pixm not defined or not 8 bpp", __func__, 1);
    if (connectivity != 4 && connectivity != 8)
        return ERROR_INT("connectivity not in {4,8}", __func__, 1);

        /* Make sure the sizes of seed and mask images are the same */
    if (pixSizesEqual(pixs, pixm) == 0)
        return ERROR_INT("pixs and pixm sizes differ", __func__, 1);

        /* This is used to test for completion */
    if ((pixt = pixCreateTemplate(pixs)) == NULL)
        return ERROR_INT("pixt not made", __func__, 1);

    datas = pixGetData(pixs);
    datam = pixGetData(pixm);
    wpls = pixGetWpl(pixs);
    wplm = pixGetWpl(pixm);
    pixGetDimensions(pixs, &w, &h, NULL);
    for (i = 0; i < MaxIters; i++) {
        pixCopy(pixt, pixs);
        seedfillGrayInvLowSimple(datas, w, h, wpls, datam, wplm, connectivity);
        pixEqual(pixs, pixt, &boolval);
        if (boolval == 1) {
#if DEBUG_PRINT_ITERS
            L_INFO("Gray seed fill converged: %d iters\n", __func__, i + 1);
#endif  /* DEBUG_PRINT_ITERS */
            break;
        }
    }

    pixDestroy(&pixt);
    return 0;
}


/*!
 * \brief   seedfillGrayLowSimple()
 *
 *  Notes:
 *      (1) The pixels are numbered as follows:
 *              1  2  3
 *              4  x  5
 *              6  7  8
 *          This low-level filling operation consists of two scans,
 *          raster and anti-raster, covering the entire seed image.
 *          The caller typically iterates until the filling is
 *          complete.
 *      (2) The filling action can be visualized from the following example.
 *          Suppose the mask, which clips the fill, is a sombrero-shaped
 *          surface, where the highest point is 200 and the low pixels
 *          around the rim are 30.  Beyond the rim, the mask goes up a bit.
 *          Suppose the seed, which is filled, consists of a single point
 *          of height 150, located below the max of the mask, with
 *          the rest 0.  Then in the raster scan, nothing happens until
 *          the high seed point is encountered, and then this value is
 *          propagated right and down, until it hits the side of the
 *          sombrero.   The seed can never exceed the mask, so it fills
 *          to the rim, going lower along the mask surface.  When it
 *          passes the rim, the seed continues to fill at the rim
 *          height to the edge of the seed image.  Then on the
 *          anti-raster scan, the seed fills flat inside the
 *          sombrero to the upper and left, and then out from the
 *          rim as before.  The final result has a seed that is
 *          flat outside the rim, and inside it fills the sombrero
 *          but only up to 150.  If the rim height varies, the
 *          filled seed outside the rim will be at the highest
 *          point on the rim, which is a saddle point on the rim.
 */
static void
seedfillGrayLowSimple(l_uint32  *datas,
                      l_int32    w,
                      l_int32    h,
                      l_int32    wpls,
                      l_uint32  *datam,
                      l_int32    wplm,
                      l_int32    connectivity)
{
l_uint8    val2, val3, val4, val5, val7, val8;
l_uint8    val, maxval, maskval;
l_int32    i, j, imax, jmax;
l_uint32  *lines, *linem;

    imax = h - 1;
    jmax = w - 1;

    switch (connectivity)
    {
    case 4:
            /* UL --> LR scan */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < w; j++) {
                if ((maskval = GET_DATA_BYTE(linem, j)) > 0) {
                    maxval = 0;
                    if (i > 0)
                        maxval = GET_DATA_BYTE(lines - wpls, j);
                    if (j > 0) {
                        val4 = GET_DATA_BYTE(lines, j - 1);
                        maxval = L_MAX(maxval, val4);
                    }
                    val = GET_DATA_BYTE(lines, j);
                    maxval = L_MAX(maxval, val);
                    val = L_MIN(maxval, maskval);
                    SET_DATA_BYTE(lines, j, val);
                }
            }
        }

            /* LR --> UL scan */
        for (i = imax; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = jmax; j >= 0; j--) {
                if ((maskval = GET_DATA_BYTE(linem, j)) > 0) {
                    maxval = 0;
                    if (i < imax)
                        maxval = GET_DATA_BYTE(lines + wpls, j);
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        maxval = L_MAX(maxval, val5);
                    }
                    val = GET_DATA_BYTE(lines, j);
                    maxval = L_MAX(maxval, val);
                    val = L_MIN(maxval, maskval);
                    SET_DATA_BYTE(lines, j, val);
                }
            }
        }
        break;

    case 8:
            /* UL --> LR scan */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < w; j++) {
                if ((maskval = GET_DATA_BYTE(linem, j)) > 0) {
                    maxval = 0;
                    if (i > 0) {
                        if (j > 0)
                            maxval = GET_DATA_BYTE(lines - wpls, j - 1);
                        if (j < jmax) {
                            val2 = GET_DATA_BYTE(lines - wpls, j + 1);
                            maxval = L_MAX(maxval, val2);
                        }
                        val3 = GET_DATA_BYTE(lines - wpls, j);
                        maxval = L_MAX(maxval, val3);
                    }
                    if (j > 0) {
                        val4 = GET_DATA_BYTE(lines, j - 1);
                        maxval = L_MAX(maxval, val4);
                    }
                    val = GET_DATA_BYTE(lines, j);
                    maxval = L_MAX(maxval, val);
                    val = L_MIN(maxval, maskval);
                    SET_DATA_BYTE(lines, j, val);
                }
            }
        }

            /* LR --> UL scan */
        for (i = imax; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = jmax; j >= 0; j--) {
                if ((maskval = GET_DATA_BYTE(linem, j)) > 0) {
                    maxval = 0;
                    if (i < imax) {
                        if (j > 0)
                            maxval = GET_DATA_BYTE(lines + wpls, j - 1);
                        if (j < jmax) {
                            val8 = GET_DATA_BYTE(lines + wpls, j + 1);
                            maxval = L_MAX(maxval, val8);
                        }
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        maxval = L_MAX(maxval, val7);
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        maxval = L_MAX(maxval, val5);
                    }
                    val = GET_DATA_BYTE(lines, j);
                    maxval = L_MAX(maxval, val);
                    val = L_MIN(maxval, maskval);
                    SET_DATA_BYTE(lines, j, val);
                }
            }
        }
        break;

    default:
        L_ERROR("connectivity must be 4 or 8\n", __func__);
    }
}


/*!
 * \brief   seedfillGrayInvLowSimple()
 *
 *  Notes:
 *      (1) The pixels are numbered as follows:
 *              1  2  3
 *              4  x  5
 *              6  7  8
 *          This low-level filling operation consists of two scans,
 *          raster and anti-raster, covering the entire seed image.
 *          The caller typically iterates until the filling is
 *          complete.
 *      (2) The "Inv" signifies the fact that in this case, filling
 *          of the seed only takes place when the seed value is
 *          greater than the mask value.  The mask will act to stop
 *          the fill when it is higher than the seed level.  (This is
 *          in contrast to conventional grayscale filling where the
 *          seed always fills below the mask.)
 *      (3) An example of use is a basin, described by the mask (pixm),
 *          where within the basin, the seed pix (pixs) gets filled to the
 *          height of the highest seed pixel that is above its
 *          corresponding max pixel.  Filling occurs while the
 *          propagating seed pixels in pixs are larger than the
 *          corresponding mask values in pixm.
 */
static void
seedfillGrayInvLowSimple(l_uint32  *datas,
                         l_int32    w,
                         l_int32    h,
                         l_int32    wpls,
                         l_uint32  *datam,
                         l_int32    wplm,
                         l_int32    connectivity)
{
l_uint8    val1, val2, val3, val4, val5, val6, val7, val8;
l_uint8    maxval, maskval;
l_int32    i, j, imax, jmax;
l_uint32  *lines, *linem;

    imax = h - 1;
    jmax = w - 1;

    switch (connectivity)
    {
    case 4:
            /* UL --> LR scan */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < w; j++) {
                if ((maskval = GET_DATA_BYTE(linem, j)) < 255) {
                    maxval = GET_DATA_BYTE(lines, j);
                    if (i > 0) {
                        val2 = GET_DATA_BYTE(lines - wpls, j);
                        maxval = L_MAX(maxval, val2);
                    }
                    if (j > 0) {
                        val4 = GET_DATA_BYTE(lines, j - 1);
                        maxval = L_MAX(maxval, val4);
                    }
                    if (maxval > maskval)
                        SET_DATA_BYTE(lines, j, maxval);
                }
            }
        }

            /* LR --> UL scan */
        for (i = imax; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = jmax; j >= 0; j--) {
                if ((maskval = GET_DATA_BYTE(linem, j)) < 255) {
                    maxval = GET_DATA_BYTE(lines, j);
                    if (i < imax) {
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        maxval = L_MAX(maxval, val7);
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        maxval = L_MAX(maxval, val5);
                    }
                    if (maxval > maskval)
                        SET_DATA_BYTE(lines, j, maxval);
                }
            }
        }
        break;

    case 8:
            /* UL --> LR scan */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < w; j++) {
                if ((maskval = GET_DATA_BYTE(linem, j)) < 255) {
                    maxval = GET_DATA_BYTE(lines, j);
                    if (i > 0) {
                        if (j > 0) {
                            val1 = GET_DATA_BYTE(lines - wpls, j - 1);
                            maxval = L_MAX(maxval, val1);
                        }
                        if (j < jmax) {
                            val2 = GET_DATA_BYTE(lines - wpls, j + 1);
                            maxval = L_MAX(maxval, val2);
                        }
                        val3 = GET_DATA_BYTE(lines - wpls, j);
                        maxval = L_MAX(maxval, val3);
                    }
                    if (j > 0) {
                        val4 = GET_DATA_BYTE(lines, j - 1);
                        maxval = L_MAX(maxval, val4);
                    }
                    if (maxval > maskval)
                        SET_DATA_BYTE(lines, j, maxval);
                }
            }
        }

            /* LR --> UL scan */
        for (i = imax; i >= 0; i--) {
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = jmax; j >= 0; j--) {
                if ((maskval = GET_DATA_BYTE(linem, j)) < 255) {
                    maxval = GET_DATA_BYTE(lines, j);
                    if (i < imax) {
                        if (j > 0) {
                            val6 = GET_DATA_BYTE(lines + wpls, j - 1);
                            maxval = L_MAX(maxval, val6);
                        }
                        if (j < jmax) {
                            val8 = GET_DATA_BYTE(lines + wpls, j + 1);
                            maxval = L_MAX(maxval, val8);
                        }
                        val7 = GET_DATA_BYTE(lines + wpls, j);
                        maxval = L_MAX(maxval, val7);
                    }
                    if (j < jmax) {
                        val5 = GET_DATA_BYTE(lines, j + 1);
                        maxval = L_MAX(maxval, val5);
                    }
                    if (maxval > maskval)
                        SET_DATA_BYTE(lines, j, maxval);
                }
            }
        }
        break;

    default:
        L_ERROR("connectivity must be 4 or 8\n", __func__);
    }
}


/*-----------------------------------------------------------------------*
 *                         Gray seedfill variations                      *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixSeedfillGrayBasin()
 *
 * \param[in]    pixb           binary mask giving seed locations
 * \param[in]    pixm           8 bpp basin-type filling mask
 * \param[in]    delta          amount of seed value above mask
 * \param[in]    connectivity   4 or 8
 * \return  pixd filled seed if OK, NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This fills from a seed within basins defined by a filling mask.
 *          The seed value(s) are greater than the corresponding
 *          filling mask value, and the result has the bottoms of
 *          the basins raised by the initial seed value.
 *      (2) The seed has value 255 except where pixb has fg (1), which
 *          are the seed 'locations'.  At the seed locations, the seed
 *          value is the corresponding value of the mask pixel in pixm
 *          plus %delta.  If %delta == 0, we return a copy of pixm.
 *      (3) The actual filling is done using the standard grayscale filling
 *          operation on the inverse of the mask and using the inverse
 *          of the seed image.  After filling, we return the inverse of
 *          the filled seed.
 *      (4) As an example of use: pixm can describe a grayscale image
 *          of text, where the (dark) text pixels are basins of
 *          low values; pixb can identify the local minima in pixm (say, at
 *          the bottom of the basins); and delta is the amount that we wish
 *          to raise (lighten) the basins.  We construct the seed
 *          (a.k.a marker) image from pixb, pixm and %delta.
 * </pre>
 */
PIX *
pixSeedfillGrayBasin(PIX     *pixb,
                     PIX     *pixm,
                     l_int32  delta,
                     l_int32  connectivity)
{
PIX  *pixbi, *pixmi, *pixsd;

    if (!pixb || pixGetDepth(pixb) != 1)
        return (PIX *)ERROR_PTR("pixb undefined or not 1 bpp", __func__, NULL);
    if (!pixm || pixGetDepth(pixm) != 8)
        return (PIX *)ERROR_PTR("pixm undefined or not 8 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not in {4,8}", __func__, NULL);

    if (delta <= 0) {
        L_WARNING("delta <= 0; returning a copy of pixm\n", __func__);
        return pixCopy(NULL, pixm);
    }

        /* Add delta to every pixel in pixm */
    pixsd = pixCopy(NULL, pixm);
    pixAddConstantGray(pixsd, delta);

        /* Prepare the seed.  Write 255 in all pixels of
         * ([pixm] + delta) where pixb is 0. */
    pixbi = pixInvert(NULL, pixb);
    pixSetMasked(pixsd, pixbi, 255);

        /* Fill the inverse seed, using the inverse clipping mask */
    pixmi = pixInvert(NULL, pixm);
    pixInvert(pixsd, pixsd);
    pixSeedfillGray(pixsd, pixmi, connectivity);

        /* Re-invert the filled seed */
    pixInvert(pixsd, pixsd);

    pixDestroy(&pixbi);
    pixDestroy(&pixmi);
    return pixsd;
}


/*-----------------------------------------------------------------------*
 *                   Vincent's Distance Function method                  *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixDistanceFunction()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    connectivity   4 or 8
 * \param[in]    outdepth       8 or 16 bits for pixd
 * \param[in]    boundcond      L_BOUNDARY_BG, L_BOUNDARY_FG
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This computes the distance of each pixel from the nearest
 *          background pixel.  All bg pixels therefore have a distance of 0,
 *          and the fg pixel distances increase linearly from 1 at the
 *          boundary.  It can also be used to compute the distance of
 *          each pixel from the nearest fg pixel, by inverting the input
 *          image before calling this function.  Then all fg pixels have
 *          a distance 0 and the bg pixel distances increase linearly
 *          from 1 at the boundary.
 *      (2) The algorithm, described in Leptonica on the page on seed
 *          filling and connected components, is due to Luc Vincent.
 *          In brief, we generate an 8 or 16 bpp image, initialized
 *          with the fg pixels of the input pix set to 1 and the
 *          1-boundary pixels (i.e., the boundary pixels of width 1 on
 *          the four sides set as either:
 *            * L_BOUNDARY_BG: 0
 *            * L_BOUNDARY_FG:  max
 *          where max = 0xff for 8 bpp and 0xffff for 16 bpp.
 *          Then do raster/anti-raster sweeps over all pixels interior
 *          to the 1-boundary, where the value of each new pixel is
 *          taken to be 1 more than the minimum of the previously-seen
 *          connected pixels (using either 4 or 8 connectivity).
 *          Finally, set the 1-boundary pixels using the mirrored method;
 *          this removes the max values there.
 *      (3) Using L_BOUNDARY_BG clamps the distance to 0 at the
 *          boundary.  Using L_BOUNDARY_FG allows the distance
 *          at the image boundary to "float".
 *      (4) For 4-connected, one could initialize only the left and top
 *          1-boundary pixels, and go all the way to the right
 *          and bottom; then coming back reset left and top.  But we
 *          instead use a method that works for both 4- and 8-connected.
 * </pre>
 */
PIX *
pixDistanceFunction(PIX     *pixs,
                    l_int32  connectivity,
                    l_int32  outdepth,
                    l_int32  boundcond)
{
l_int32    w, h, wpld;
l_uint32  *datad;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("!pixs or pixs not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    if (outdepth != 8 && outdepth != 16)
        return (PIX *)ERROR_PTR("outdepth not 8 or 16 bpp", __func__, NULL);
    if (boundcond != L_BOUNDARY_BG && boundcond != L_BOUNDARY_FG)
        return (PIX *)ERROR_PTR("invalid boundcond", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if ((pixd = pixCreate(w, h, outdepth)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* Initialize the fg pixels to 1 and the bg pixels to 0 */
    pixSetMasked(pixd, pixs, 1);

    if (boundcond == L_BOUNDARY_BG) {
        distanceFunctionLow(datad, w, h, outdepth, wpld, connectivity);
    } else {  /* L_BOUNDARY_FG: set boundary pixels to max val */
        pixRasterop(pixd, 0, 0, w, 1, PIX_SET, NULL, 0, 0);   /* top */
        pixRasterop(pixd, 0, h - 1, w, 1, PIX_SET, NULL, 0, 0);   /* bot */
        pixRasterop(pixd, 0, 0, 1, h, PIX_SET, NULL, 0, 0);   /* left */
        pixRasterop(pixd, w - 1, 0, 1, h, PIX_SET, NULL, 0, 0);   /* right */

        distanceFunctionLow(datad, w, h, outdepth, wpld, connectivity);

            /* Set each boundary pixel equal to the pixel next to it */
        pixSetMirroredBorder(pixd, 1, 1, 1, 1);
    }

    return pixd;
}


/*!
 * \brief   distanceFunctionLow()
 */
static void
distanceFunctionLow(l_uint32  *datad,
                    l_int32    w,
                    l_int32    h,
                    l_int32    d,
                    l_int32    wpld,
                    l_int32    connectivity)
{
l_int32    val1, val2, val3, val4, val5, val6, val7, val8, minval, val;
l_int32    i, j, imax, jmax;
l_uint32  *lined;

        /* One raster scan followed by one anti-raster scan.
         * This does not re-set the 1-boundary of pixels that
         * were initialized to either 0 or maxval. */
    imax = h - 1;
    jmax = w - 1;
    switch (connectivity)
    {
    case 4:
        if (d == 8) {
                /* UL --> LR scan */
            for (i = 1; i < imax; i++) {
                lined = datad + i * wpld;
                for (j = 1; j < jmax; j++) {
                    if ((val = GET_DATA_BYTE(lined, j)) > 0) {
                        val2 = GET_DATA_BYTE(lined - wpld, j);
                        val4 = GET_DATA_BYTE(lined, j - 1);
                        minval = L_MIN(val2, val4);
                        minval = L_MIN(minval, 254);
                        SET_DATA_BYTE(lined, j, minval + 1);
                    }
                }
            }

                /* LR --> UL scan */
            for (i = imax - 1; i > 0; i--) {
                lined = datad + i * wpld;
                for (j = jmax - 1; j > 0; j--) {
                    if ((val = GET_DATA_BYTE(lined, j)) > 0) {
                        val7 = GET_DATA_BYTE(lined + wpld, j);
                        val5 = GET_DATA_BYTE(lined, j + 1);
                        minval = L_MIN(val5, val7);
                        minval = L_MIN(minval + 1, val);
                        SET_DATA_BYTE(lined, j, minval);
                    }
                }
            }
        } else {  /* d == 16 */
                /* UL --> LR scan */
            for (i = 1; i < imax; i++) {
                lined = datad + i * wpld;
                for (j = 1; j < jmax; j++) {
                    if ((val = GET_DATA_TWO_BYTES(lined, j)) > 0) {
                        val2 = GET_DATA_TWO_BYTES(lined - wpld, j);
                        val4 = GET_DATA_TWO_BYTES(lined, j - 1);
                        minval = L_MIN(val2, val4);
                        minval = L_MIN(minval, 0xfffe);
                        SET_DATA_TWO_BYTES(lined, j, minval + 1);
                    }
                }
            }

                /* LR --> UL scan */
            for (i = imax - 1; i > 0; i--) {
                lined = datad + i * wpld;
                for (j = jmax - 1; j > 0; j--) {
                    if ((val = GET_DATA_TWO_BYTES(lined, j)) > 0) {
                        val7 = GET_DATA_TWO_BYTES(lined + wpld, j);
                        val5 = GET_DATA_TWO_BYTES(lined, j + 1);
                        minval = L_MIN(val5, val7);
                        minval = L_MIN(minval + 1, val);
                        SET_DATA_TWO_BYTES(lined, j, minval);
                    }
                }
            }
        }
        break;

    case 8:
        if (d == 8) {
                /* UL --> LR scan */
            for (i = 1; i < imax; i++) {
                lined = datad + i * wpld;
                for (j = 1; j < jmax; j++) {
                    if ((val = GET_DATA_BYTE(lined, j)) > 0) {
                        val1 = GET_DATA_BYTE(lined - wpld, j - 1);
                        val2 = GET_DATA_BYTE(lined - wpld, j);
                        val3 = GET_DATA_BYTE(lined - wpld, j + 1);
                        val4 = GET_DATA_BYTE(lined, j - 1);
                        minval = L_MIN(val1, val2);
                        minval = L_MIN(minval, val3);
                        minval = L_MIN(minval, val4);
                        minval = L_MIN(minval, 254);
                        SET_DATA_BYTE(lined, j, minval + 1);
                    }
                }
            }

                /* LR --> UL scan */
            for (i = imax - 1; i > 0; i--) {
                lined = datad + i * wpld;
                for (j = jmax - 1; j > 0; j--) {
                    if ((val = GET_DATA_BYTE(lined, j)) > 0) {
                        val8 = GET_DATA_BYTE(lined + wpld, j + 1);
                        val7 = GET_DATA_BYTE(lined + wpld, j);
                        val6 = GET_DATA_BYTE(lined + wpld, j - 1);
                        val5 = GET_DATA_BYTE(lined, j + 1);
                        minval = L_MIN(val8, val7);
                        minval = L_MIN(minval, val6);
                        minval = L_MIN(minval, val5);
                        minval = L_MIN(minval + 1, val);
                        SET_DATA_BYTE(lined, j, minval);
                    }
                }
            }
        } else {  /* d == 16 */
                /* UL --> LR scan */
            for (i = 1; i < imax; i++) {
                lined = datad + i * wpld;
                for (j = 1; j < jmax; j++) {
                    if ((val = GET_DATA_TWO_BYTES(lined, j)) > 0) {
                        val1 = GET_DATA_TWO_BYTES(lined - wpld, j - 1);
                        val2 = GET_DATA_TWO_BYTES(lined - wpld, j);
                        val3 = GET_DATA_TWO_BYTES(lined - wpld, j + 1);
                        val4 = GET_DATA_TWO_BYTES(lined, j - 1);
                        minval = L_MIN(val1, val2);
                        minval = L_MIN(minval, val3);
                        minval = L_MIN(minval, val4);
                        minval = L_MIN(minval, 0xfffe);
                        SET_DATA_TWO_BYTES(lined, j, minval + 1);
                    }
                }
            }

                /* LR --> UL scan */
            for (i = imax - 1; i > 0; i--) {
                lined = datad + i * wpld;
                for (j = jmax - 1; j > 0; j--) {
                    if ((val = GET_DATA_TWO_BYTES(lined, j)) > 0) {
                        val8 = GET_DATA_TWO_BYTES(lined + wpld, j + 1);
                        val7 = GET_DATA_TWO_BYTES(lined + wpld, j);
                        val6 = GET_DATA_TWO_BYTES(lined + wpld, j - 1);
                        val5 = GET_DATA_TWO_BYTES(lined, j + 1);
                        minval = L_MIN(val8, val7);
                        minval = L_MIN(minval, val6);
                        minval = L_MIN(minval, val5);
                        minval = L_MIN(minval + 1, val);
                        SET_DATA_TWO_BYTES(lined, j, minval);
                    }
                }
            }
        }
        break;

    default:
        L_ERROR("connectivity must be 4 or 8\n", __func__);
    }
}


/*-----------------------------------------------------------------------*
 *                Seed spread (based on distance function)               *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixSeedspread()
 *
 * \param[in]    pixs           8 bpp
 * \param[in]    connectivity   4 or 8
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The raster/anti-raster method for implementing this filling
 *          operation was suggested by Ray Smith.
 *      (2) This takes an arbitrary set of nonzero pixels in pixs, which
 *          can be sparse, and spreads (extrapolates) the values to
 *          fill all the pixels in pixd with the nonzero value it is
 *          closest to in pixs.  This is similar (though not completely
 *          equivalent) to doing a Voronoi tiling of the image, with a
 *          tile surrounding each pixel that has a nonzero value.
 *          All pixels within a tile are then closer to its "central"
 *          pixel than to any others.  Then assign the value of the
 *          "central" pixel to each pixel in the tile.
 *      (3) This is implemented by computing a distance function in parallel
 *          with the fill.  The distance function uses free boundary
 *          conditions (assumed maxval outside), and it controls the
 *          propagation of the pixels in pixd away from the nonzero
 *          (seed) values.  This is done in 2 traversals (raster/antiraster).
 *          In the raster direction, whenever the distance function
 *          is nonzero, the spread pixel takes on the value of its
 *          predecessor that has the minimum distance value.  In the
 *          antiraster direction, whenever the distance function is nonzero
 *          and its value is replaced by a smaller value, the spread
 *          pixel takes the value of the predecessor with the minimum
 *          distance value.
 *      (4) At boundaries where a pixel is equidistant from two
 *          nearest nonzero (seed) pixels, the decision of which value
 *          to use is arbitrary (greedy in search for minimum distance).
 *          This can give rise to strange-looking results, particularly
 *          for 4-connectivity where the L1 distance is computed from
 *          steps in N,S,E and W directions (no diagonals).
 * </pre>
 */
PIX *
pixSeedspread(PIX     *pixs,
              l_int32  connectivity)
{
l_int32    w, h, wplt, wplg;
l_uint32  *datat, *datag;
PIX       *pixm, *pixt, *pixg, *pixd;

    if (!pixs || pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("!pixs or pixs not 8 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

        /* Add a 4 byte border to pixs.  This simplifies the computation. */
    pixg = pixAddBorder(pixs, 4, 0);
    pixGetDimensions(pixg, &w, &h, NULL);

        /* Initialize distance function pixt.  Threshold pixs to get
         * a 0 at the seed points where the pixs pixel is nonzero, and
         * a 1 at all points that need to be filled.  Use this as a
         * mask to set a 1 in pixt at all non-seed points.  Also, set all
         * pixt pixels in an interior boundary of width 1 to the
         * maximum value.   For debugging, to view the distance function,
         * use pixConvert16To8(pixt, L_LS_BYTE) on small images.  */
    pixm = pixThresholdToBinary(pixg, 1);
    pixt = pixCreate(w, h, 16);
    pixSetMasked(pixt, pixm, 1);
    pixRasterop(pixt, 0, 0, w, 1, PIX_SET, NULL, 0, 0);   /* top */
    pixRasterop(pixt, 0, h - 1, w, 1, PIX_SET, NULL, 0, 0);   /* bot */
    pixRasterop(pixt, 0, 0, 1, h, PIX_SET, NULL, 0, 0);   /* left */
    pixRasterop(pixt, w - 1, 0, 1, h, PIX_SET, NULL, 0, 0);   /* right */
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);

        /* Do the interpolation and remove the border. */
    datag = pixGetData(pixg);
    wplg = pixGetWpl(pixg);
    seedspreadLow(datag, w, h, wplg, datat, wplt, connectivity);
    pixd = pixRemoveBorder(pixg, 4);

    pixDestroy(&pixm);
    pixDestroy(&pixg);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   seedspreadLow()
 *
 *    See pixSeedspread() for a brief description of the algorithm here.
 */
static void
seedspreadLow(l_uint32  *datad,
              l_int32    w,
              l_int32    h,
              l_int32    wpld,
              l_uint32  *datat,
              l_int32    wplt,
              l_int32    connectivity)
{
l_int32    val1t, val2t, val3t, val4t, val5t, val6t, val7t, val8t;
l_int32    i, j, imax, jmax, minval, valt, vald;
l_uint32  *linet, *lined;

        /* One raster scan followed by one anti-raster scan.
         * pixt is initialized to have 0 on pixels where the
         * input is specified in pixd, and to have 1 on all
         * other pixels.  We only change pixels in pixt and pixd
         * that are non-zero in pixt. */
    imax = h - 1;
    jmax = w - 1;
    switch (connectivity)
    {
    case 4:
            /* UL --> LR scan */
        for (i = 1; i < h; i++) {
            linet = datat + i * wplt;
            lined = datad + i * wpld;
            for (j = 1; j < jmax; j++) {
                if ((valt = GET_DATA_TWO_BYTES(linet, j)) > 0) {
                    val2t = GET_DATA_TWO_BYTES(linet - wplt, j);
                    val4t = GET_DATA_TWO_BYTES(linet, j - 1);
                    minval = L_MIN(val2t, val4t);
                    minval = L_MIN(minval, 0xfffe);
                    SET_DATA_TWO_BYTES(linet, j, minval + 1);
                    if (val2t < val4t)
                        vald = GET_DATA_BYTE(lined - wpld, j);
                    else
                        vald = GET_DATA_BYTE(lined, j - 1);
                    SET_DATA_BYTE(lined, j, vald);
                }
            }
        }

            /* LR --> UL scan */
        for (i = imax - 1; i > 0; i--) {
            linet = datat + i * wplt;
            lined = datad + i * wpld;
            for (j = jmax - 1; j > 0; j--) {
                if ((valt = GET_DATA_TWO_BYTES(linet, j)) > 0) {
                    val7t = GET_DATA_TWO_BYTES(linet + wplt, j);
                    val5t = GET_DATA_TWO_BYTES(linet, j + 1);
                    minval = L_MIN(val5t, val7t);
                    minval = L_MIN(minval + 1, valt);
                    if (valt > minval) {  /* replace */
                        SET_DATA_TWO_BYTES(linet, j, minval);
                        if (val5t < val7t)
                            vald = GET_DATA_BYTE(lined, j + 1);
                        else
                            vald = GET_DATA_BYTE(lined + wplt, j);
                        SET_DATA_BYTE(lined, j, vald);
                    }
                }
            }
        }
        break;
    case 8:
            /* UL --> LR scan */
        for (i = 1; i < h; i++) {
            linet = datat + i * wplt;
            lined = datad + i * wpld;
            for (j = 1; j < jmax; j++) {
                if ((valt = GET_DATA_TWO_BYTES(linet, j)) > 0) {
                    val1t = GET_DATA_TWO_BYTES(linet - wplt, j - 1);
                    val2t = GET_DATA_TWO_BYTES(linet - wplt, j);
                    val3t = GET_DATA_TWO_BYTES(linet - wplt, j + 1);
                    val4t = GET_DATA_TWO_BYTES(linet, j - 1);
                    minval = L_MIN(val1t, val2t);
                    minval = L_MIN(minval, val3t);
                    minval = L_MIN(minval, val4t);
                    minval = L_MIN(minval, 0xfffe);
                    SET_DATA_TWO_BYTES(linet, j, minval + 1);
                    if (minval == val1t)
                        vald = GET_DATA_BYTE(lined - wpld, j - 1);
                    else if (minval == val2t)
                        vald = GET_DATA_BYTE(lined - wpld, j);
                    else if (minval == val3t)
                        vald = GET_DATA_BYTE(lined - wpld, j + 1);
                    else  /* minval == val4t */
                        vald = GET_DATA_BYTE(lined, j - 1);
                    SET_DATA_BYTE(lined, j, vald);
                }
            }
        }

            /* LR --> UL scan */
        for (i = imax - 1; i > 0; i--) {
            linet = datat + i * wplt;
            lined = datad + i * wpld;
            for (j = jmax - 1; j > 0; j--) {
                if ((valt = GET_DATA_TWO_BYTES(linet, j)) > 0) {
                    val8t = GET_DATA_TWO_BYTES(linet + wplt, j + 1);
                    val7t = GET_DATA_TWO_BYTES(linet + wplt, j);
                    val6t = GET_DATA_TWO_BYTES(linet + wplt, j - 1);
                    val5t = GET_DATA_TWO_BYTES(linet, j + 1);
                    minval = L_MIN(val8t, val7t);
                    minval = L_MIN(minval, val6t);
                    minval = L_MIN(minval, val5t);
                    minval = L_MIN(minval + 1, valt);
                    if (valt > minval) {  /* replace */
                        SET_DATA_TWO_BYTES(linet, j, minval);
                        if (minval == val5t + 1)
                            vald = GET_DATA_BYTE(lined, j + 1);
                        else if (minval == val6t + 1)
                            vald = GET_DATA_BYTE(lined + wpld, j - 1);
                        else if (minval == val7t + 1)
                            vald = GET_DATA_BYTE(lined + wpld, j);
                        else  /* minval == val8t + 1 */
                            vald = GET_DATA_BYTE(lined + wpld, j + 1);
                        SET_DATA_BYTE(lined, j, vald);
                    }
                }
            }
        }
        break;
    default:
        L_ERROR("connectivity must be 4 or 8\n", __func__);
        break;
    }
}


/*-----------------------------------------------------------------------*
 *                              Local extrema                            *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixLocalExtrema()
 *
 * \param[in]    pixs       8 bpp
 * \param[in]    maxmin     max allowed for the min in a 3x3 neighborhood;
 *                          use 0 for default which is to have no upper bound
 * \param[in]    minmax     min allowed for the max in a 3x3 neighborhood;
 *                          use 0 for default which is to have no lower bound
 * \param[out]   ppixmin    [optional] mask of local minima
 * \param[out]   ppixmax    [optional] mask of local maxima
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This gives the actual local minima and maxima.
 *          A local minimum is a pixel whose surrounding pixels all
 *          have values at least as large, and likewise for a local
 *          maximum.  For the local minima, %maxmin is the upper
 *          bound for the value of pixs.  Likewise, for the local maxima,
 *          %minmax is the lower bound for the value of pixs.
 *      (2) The minima are found by starting with the erosion-and-equality
 *          approach of pixSelectedLocalExtrema().  This is followed
 *          by a qualification step, where each c.c. in the resulting
 *          minimum mask is extracted, the pixels bordering it are
 *          located, and they are queried.  If all of those pixels
 *          are larger than the value of that minimum, it is a true
 *          minimum and its c.c. is saved; otherwise the c.c. is
 *          rejected.  Note that if a bordering pixel has the
 *          same value as the minimum, it must then have a
 *          neighbor that is smaller, so the component is not a
 *          true minimum.
 *      (3) The maxima are found by inverting the image and looking
 *          for the minima there.
 *      (4) The generated masks can be used as markers for
 *          further operations.
 * </pre>
 */
l_ok
pixLocalExtrema(PIX     *pixs,
                l_int32  maxmin,
                l_int32  minmax,
                PIX    **ppixmin,
                PIX    **ppixmax)
{
PIX  *pixmin, *pixmax, *pixt1, *pixt2;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!ppixmin && !ppixmax)
        return ERROR_INT("neither &pixmin, &pixmax are defined", __func__, 1);
    if (maxmin <= 0) maxmin = 254;
    if (minmax <= 0) minmax = 1;

    if (ppixmin) {
        pixt1 = pixErodeGray(pixs, 3, 3);
        pixmin = pixFindEqualValues(pixs, pixt1);
        pixDestroy(&pixt1);
        pixQualifyLocalMinima(pixs, pixmin, maxmin);
        *ppixmin = pixmin;
    }

    if (ppixmax) {
        pixt1 = pixInvert(NULL, pixs);
        pixt2 = pixErodeGray(pixt1, 3, 3);
        pixmax = pixFindEqualValues(pixt1, pixt2);
        pixDestroy(&pixt2);
        pixQualifyLocalMinima(pixt1, pixmax, 255 - minmax);
        *ppixmax = pixmax;
        pixDestroy(&pixt1);
    }

    return 0;
}


/*!
 * \brief   pixQualifyLocalMinima()
 *
 * \param[in]    pixs     8 bpp image from which pixm has been extracted
 * \param[in]    pixm     1 bpp mask of values equal to min in 3x3 neighborhood
 * \param[in]    maxval   max allowed for the min in a 3x3 neighborhood;
 *                        use 0 for default which is to have no upper bound
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function acts in-place to remove all c.c. in pixm
 *          that are not true local minima in pixs.  As seen in
 *          pixLocalExtrema(), the input pixm are found by selecting those
 *          pixels of pixs whose values do not change with a 3x3
 *          grayscale erosion.  Here, we require that for each c.c.
 *          in pixm, all pixels in pixs that correspond to the exterior
 *          boundary pixels of the c.c. have values that are greater
 *          than the value within the c.c.
 *      (2) The maximum allowed value for each local minimum can be
 *          bounded with %maxval.  Use 0 for default, which is to have
 *          no upper bound (equivalent to maxval == 254).
 * </pre>
 */
static l_int32
pixQualifyLocalMinima(PIX     *pixs,
                      PIX     *pixm,
                      l_int32  maxval)
{
l_int32    n, i, j, k, x, y, w, h, xc, yc, wc, hc, xon, yon;
l_int32    vals, wpls, wplc, ismin;
l_uint32   val;
l_uint32  *datas, *datac, *lines, *linec;
BOXA      *boxa;
PIX       *pix1, *pix2, *pix3;
PIXA      *pixa;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not defined or not 1 bpp", __func__, 1);
    if (maxval <= 0) maxval = 254;

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    boxa = pixConnComp(pixm, &pixa, 8);
    n = pixaGetCount(pixa);
    for (k = 0; k < n; k++) {
        boxaGetBoxGeometry(boxa, k, &xc, &yc, &wc, &hc);
        pix1 = pixaGetPix(pixa, k, L_COPY);
        pix2 = pixAddBorder(pix1, 1, 0);
        pix3 = pixDilateBrick(NULL, pix2, 3, 3);
        pixXor(pix3, pix3, pix2);  /* exterior boundary pixels */
        datac = pixGetData(pix3);
        wplc = pixGetWpl(pix3);
        nextOnPixelInRaster(pix1, 0, 0, &xon, &yon);
        pixGetPixel(pixs, xc + xon, yc + yon, &val);
        if (val > maxval) {  /* too large; erase */
            pixRasterop(pixm, xc, yc, wc, hc, PIX_XOR, pix1, 0, 0);
            pixDestroy(&pix1);
            pixDestroy(&pix2);
            pixDestroy(&pix3);
            continue;
        }
        ismin = TRUE;

            /* Check all values in pixs that correspond to the exterior
             * boundary pixels of the c.c. in pixm.  Verify that the
             * value in the c.c. is always less. */
        for (i = 0, y = yc - 1; i < hc + 2 && y >= 0 && y < h; i++, y++) {
            lines = datas + y * wpls;
            linec = datac + i * wplc;
            for (j = 0, x = xc - 1; j < wc + 2 && x >= 0 && x < w; j++, x++) {
                if (GET_DATA_BIT(linec, j)) {
                    vals = GET_DATA_BYTE(lines, x);
                    if (vals <= val) {  /* not a minimum! */
                        ismin = FALSE;
                        break;
                    }
                }
            }
            if (!ismin)
                break;
        }
        if (!ismin)  /* erase it */
            pixRasterop(pixm, xc, yc, wc, hc, PIX_XOR, pix1, 0, 0);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
    }

    boxaDestroy(&boxa);
    pixaDestroy(&pixa);
    return 0;
}


/*!
 * \brief   pixSelectedLocalExtrema()
 *
 * \param[in]    pixs       8 bpp
 * \param[in]    mindist    -1 for keeping all pixels; >= 0 specifies distance
 * \param[out]   ppixmin    mask of local minima
 * \param[out]   ppixmax    mask of local maxima
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This selects those local 3x3 minima that are at least a
 *          specified distance from the nearest local 3x3 maxima, and v.v.
 *          for the selected set of local 3x3 maxima.
 *          The local 3x3 minima is the set of pixels whose value equals
 *          the value after a 3x3 brick erosion, and the local 3x3 maxima
 *          is the set of pixels whose value equals the value after
 *          a 3x3 brick dilation.
 *      (2) mindist is the minimum distance allowed between
 *          local 3x3 minima and local 3x3 maxima, in an 8-connected sense.
 *          mindist == 1 keeps all pixels found in step 1.
 *          mindist == 0 removes all pixels from each mask that are
 *          both a local 3x3 minimum and a local 3x3 maximum.
 *          mindist == 1 removes any local 3x3 minimum pixel that touches a
 *          local 3x3 maximum pixel, and likewise for the local maxima.
 *          To make the decision, visualize each local 3x3 minimum pixel
 *          as being surrounded by a square of size (2 * mindist + 1)
 *          on each side, such that no local 3x3 maximum pixel is within
 *          that square; and v.v.
 *      (3) The generated masks can be used as markers for further operations.
 * </pre>
 */
l_ok
pixSelectedLocalExtrema(PIX     *pixs,
                        l_int32  mindist,
                        PIX    **ppixmin,
                        PIX    **ppixmax)
{
PIX  *pixmin, *pixmax, *pixt, *pixtmin, *pixtmax;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!ppixmin || !ppixmax)
        return ERROR_INT("&pixmin and &pixmax not both defined", __func__, 1);

    pixt = pixErodeGray(pixs, 3, 3);
    pixmin = pixFindEqualValues(pixs, pixt);
    pixDestroy(&pixt);
    pixt = pixDilateGray(pixs, 3, 3);
    pixmax = pixFindEqualValues(pixs, pixt);
    pixDestroy(&pixt);

        /* Remove all points that are within the prescribed distance
         * from each other. */
    if (mindist < 0) {  /* remove no points */
        *ppixmin = pixmin;
        *ppixmax = pixmax;
    } else if (mindist == 0) {  /* remove points belonging to both sets */
        pixt = pixAnd(NULL, pixmin, pixmax);
        *ppixmin = pixSubtract(pixmin, pixmin, pixt);
        *ppixmax = pixSubtract(pixmax, pixmax, pixt);
        pixDestroy(&pixt);
    } else {
        pixtmin = pixDilateBrick(NULL, pixmin,
                                 2 * mindist + 1, 2 * mindist + 1);
        pixtmax = pixDilateBrick(NULL, pixmax,
                                 2 * mindist + 1, 2 * mindist + 1);
        *ppixmin = pixSubtract(pixmin, pixmin, pixtmax);
        *ppixmax = pixSubtract(pixmax, pixmax, pixtmin);
        pixDestroy(&pixtmin);
        pixDestroy(&pixtmax);
    }
    return 0;
}


/*!
 * \brief   pixFindEqualValues()
 *
 * \param[in]    pixs1    8 bpp
 * \param[in]    pixs2    8 bpp
 * \return  pixd 1 bpp mask, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The two images are aligned at the UL corner, and the returned
 *          image has ON pixels where the pixels in pixs1 and pixs2
 *          have equal values.
 * </pre>
 */
PIX *
pixFindEqualValues(PIX  *pixs1,
                   PIX  *pixs2)
{
l_int32    w1, h1, w2, h2, w, h;
l_int32    i, j, val1, val2, wpls1, wpls2, wpld;
l_uint32  *datas1, *datas2, *datad, *lines1, *lines2, *lined;
PIX       *pixd;

    if (!pixs1 || pixGetDepth(pixs1) != 8)
        return (PIX *)ERROR_PTR("pixs1 undefined or not 8 bpp", __func__, NULL);
    if (!pixs2 || pixGetDepth(pixs2) != 8)
        return (PIX *)ERROR_PTR("pixs2 undefined or not 8 bpp", __func__, NULL);
    pixGetDimensions(pixs1, &w1, &h1, NULL);
    pixGetDimensions(pixs2, &w2, &h2, NULL);
    w = L_MIN(w1, w2);
    h = L_MIN(h1, h2);
    pixd = pixCreate(w, h, 1);
    datas1 = pixGetData(pixs1);
    datas2 = pixGetData(pixs2);
    datad = pixGetData(pixd);
    wpls1 = pixGetWpl(pixs1);
    wpls2 = pixGetWpl(pixs2);
    wpld = pixGetWpl(pixd);

    for (i = 0; i < h; i++) {
        lines1 = datas1 + i * wpls1;
        lines2 = datas2 + i * wpls2;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            val1 = GET_DATA_BYTE(lines1, j);
            val2 = GET_DATA_BYTE(lines2, j);
            if (val1 == val2)
                SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*-----------------------------------------------------------------------*
 *             Selection of minima in mask connected components          *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixSelectMinInConnComp()
 *
 * \param[in]    pixs    8 bpp
 * \param[in]    pixm    1 bpp
 * \param[out]   ppta    pta of min pixel locations
 * \param[out]   pnav    [optional] numa of minima values
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) For each 8 connected component in pixm, this finds
 *          a pixel in pixs that has the lowest value, and saves
 *          it in a Pta.  If several pixels in pixs have the same
 *          minimum value, it picks the first one found.
 *      (2) For a mask pixm of true local minima, all pixels in each
 *          connected component have the same value in pixs, so it is
 *          fastest to select one of them using a special seedfill
 *          operation.  Not yet implemented.
 * </pre>
 */
l_ok
pixSelectMinInConnComp(PIX    *pixs,
                       PIX    *pixm,
                       PTA   **ppta,
                       NUMA  **pnav)
{
l_int32    bx, by, bw, bh, i, j, c, n;
l_int32    xs, ys, minx, miny, wpls, wplt, val, minval;
l_uint32  *datas, *datat, *lines, *linet;
BOXA      *boxa;
NUMA      *nav;
PIX       *pixt, *pixs2, *pixm2;
PIXA      *pixa;
PTA       *pta;

    if (!ppta)
        return ERROR_INT("&pta not defined", __func__, 1);
    *ppta = NULL;
    if (pnav) *pnav = NULL;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs undefined or not 8 bpp", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm undefined or not 1 bpp", __func__, 1);

        /* Crop to the min size if necessary */
    if (pixCropToMatch(pixs, pixm, &pixs2, &pixm2)) {
        pixDestroy(&pixs2);
        pixDestroy(&pixm2);
        return ERROR_INT("cropping failure", __func__, 1);
    }

        /* Find value and location of min value pixel in each component */
    boxa = pixConnComp(pixm2, &pixa, 8);
    n = boxaGetCount(boxa);
    pta = ptaCreate(n);
    *ppta = pta;
    nav = numaCreate(n);
    datas = pixGetData(pixs2);
    wpls = pixGetWpl(pixs2);
    for (c = 0; c < n; c++) {
        pixt = pixaGetPix(pixa, c, L_CLONE);
        boxaGetBoxGeometry(boxa, c, &bx, &by, &bw, &bh);
        if (bw == 1 && bh == 1) {
            ptaAddPt(pta, bx, by);
            numaAddNumber(nav, GET_DATA_BYTE(datas + by * wpls, bx));
            pixDestroy(&pixt);
            continue;
        }
        datat = pixGetData(pixt);
        wplt = pixGetWpl(pixt);
        minx = miny = 1000000;
        minval = 256;
        for (i = 0; i < bh; i++) {
            ys = by + i;
            lines = datas + ys * wpls;
            linet = datat + i * wplt;
            for (j = 0; j < bw; j++) {
                xs = bx + j;
                if (GET_DATA_BIT(linet, j)) {
                    val = GET_DATA_BYTE(lines, xs);
                    if (val < minval) {
                        minval = val;
                        minx = xs;
                        miny = ys;
                    }
                }
            }
        }
        ptaAddPt(pta, minx, miny);
        numaAddNumber(nav, GET_DATA_BYTE(datas + miny * wpls, minx));
        pixDestroy(&pixt);
    }

    boxaDestroy(&boxa);
    pixaDestroy(&pixa);
    if (pnav)
        *pnav = nav;
    else
        numaDestroy(&nav);
    pixDestroy(&pixs2);
    pixDestroy(&pixm2);
    return 0;
}


/*-----------------------------------------------------------------------*
 *            Removal of seeded connected components from a mask         *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixRemoveSeededComponents()
 *
 * \param[in]    pixd          [optional]; can be null or equal to pixm; 1 bpp
 * \param[in]    pixs          1 bpp seed
 * \param[in]    pixm          1 bpp filling mask
 * \param[in]    connectivity  4 or 8
 * \param[in]    bordersize    amount of border clearing
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This removes each component in pixm for which there is
 *          at least one seed in pixs.  If pixd == NULL, this returns
 *          the result in a new pixd.  Otherwise, it is an in-place
 *          operation on pixm.  In no situation is pixs altered,
 *          because we do the filling with a copy of pixs.
 *      (2) If bordersize > 0, it also clears all pixels within a
 *          distance %bordersize of the edge of pixd.  This is here
 *          because pixLocalExtrema() typically finds local minima
 *          at the border.  Use %bordersize >= 2 to remove these.
 * </pre>
 */
PIX *
pixRemoveSeededComponents(PIX     *pixd,
                          PIX     *pixs,
                          PIX     *pixm,
                          l_int32  connectivity,
                          l_int32  bordersize)
{
PIX  *pixt;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, pixd);
    if (!pixm || pixGetDepth(pixm) != 1)
        return (PIX *)ERROR_PTR("pixm undefined or not 1 bpp", __func__, pixd);
    if (pixd && pixd != pixm)
        return (PIX *)ERROR_PTR("operation not inplace", __func__, pixd);

    pixt = pixCopy(NULL, pixs);
    pixSeedfillBinary(pixt, pixt, pixm, connectivity);
    pixd = pixXor(pixd, pixm, pixt);
    if (bordersize > 0)
        pixSetOrClearBorder(pixd, bordersize, bordersize, bordersize,
                            bordersize, PIX_CLR);
    pixDestroy(&pixt);
    return pixd;
}
