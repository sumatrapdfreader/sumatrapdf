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
 * \file colorquant2.c
 * <pre>
 *
 *  Modified median cut color quantization
 *
 *      High level
 *          PIX              *pixMedianCutQuant()
 *          PIX              *pixMedianCutQuantGeneral()
 *          PIX              *pixMedianCutQuantMixed()
 *          PIX              *pixFewColorsMedianCutQuantMixed()
 *
 *      Median cut indexed histogram
 *          l_int32          *pixMedianCutHisto()
 *
 *      Static helpers
 *          static PIXCMAP   *pixcmapGenerateFromHisto()
 *          static PIX       *pixQuantizeWithColormap()
 *          static void       getColorIndexMedianCut()
 *          static L_BOX3D   *pixGetColorRegion()
 *          static l_int32    medianCutApply()
 *          static PIXCMAP   *pixcmapGenerateFromMedianCuts()
 *          static l_int32    vboxGetAverageColor()
 *          static l_int32    vboxGetCount()
 *          static l_int32    vboxGetVolume()
 *          static L_BOX3D   *box3dCreate();
 *          static L_BOX3D   *box3dCopy();
 *
 *   Paul Heckbert published the median cut algorithm, "Color Image
 *   Quantization for Frame Buffer Display," in Proc. SIGGRAPH '82,
 *   Boston, July 1982, pp. 297-307.  See:
 *   http://delivery.acm.org/10.1145/810000/801294/p297-heckbert.pdf
 *
 *   Median cut starts with either the full color space or the occupied
 *   region of color space.  If you're not dithering, the occupied region
 *   can be used, but with dithering, pixels can end up in any place
 *   in the color space, so you must represent the entire color space in
 *   the final colormap.
 *
 *   Color components are quantized to typically 5 or 6 significant
 *   bits (for each of r, g and b).   Call a 3D region of color
 *   space a 'vbox'.  Any color in this quantized space is represented
 *   by an element of a linear histogram array, indexed by rgb value.
 *   The initial region is then divided into two regions that have roughly
 *   equal pixel occupancy (hence the name "median cut").  Subdivision
 *   continues until the requisite number of vboxes has been generated.
 *
 *   But the devil is in the details of the subdivision process.
 *   Here are some choices that you must make:
 *     (1) Along which axis to subdivide?
 *     (2) Which box to put the bin with the median pixel?
 *     (3) How to order the boxes for subdivision?
 *     (4) How to adequately handle boxes with very small numbers of pixels?
 *     (5) How to prevent a little-represented but highly visible color
 *         from being masked out by other colors in its vbox.
 *
 *   Taking these in order:
 *     (1) Heckbert suggests using either the largest vbox side, or the vbox
 *         side with the largest variance in pixel occupancy.  We choose
 *         to divide based on the largest vbox side.
 *     (2) Suppose you've chosen a side.  Then you have a histogram
 *         of pixel occupancy in 2D slices of the vbox.  One of those
 *         slices includes the median pixel.  Suppose there are L bins
 *         to the left (smaller index) and R bins to the right.  Then
 *         this slice (or bin) should be assigned to the box containing
 *         the smaller of L and R.  This both shortens the larger
 *         of the subdivided dimensions and helps a low-count color
 *         far from the subdivision boundary to better express itself.
 *     (2a) One can also ask if the boundary should be moved even
 *         farther into the longer side.  This is feasible if we have
 *         a method for doing extra subdivisions on the high count
 *         vboxes.  And we do (see (3)).
 *     (3) To make sure that the boxes are subdivided toward equal
 *         occupancy, use an occupancy-sorted priority queue, rather
 *         than a simple queue.
 *     (4) With a priority queue, boxes with small number of pixels
 *         won't be repeatedly subdivided.  This is good.
 *     (5) Use of a priority queue allows tricks such as in (2a) to let
 *         small occupancy clusters be better expressed.  In addition,
 *         rather than splitting near the median, small occupancy colors
 *         are best reproduced by cutting half-way into the longer side.
 *
 *   However, serious problems can arise with dithering if a priority
 *   queue is used based on population alone.  If the picture has
 *   large regions of nearly constant color, some vboxes can be very
 *   large and have a sizeable population (but not big enough to get to
 *   the head of the queue).  If one of these large, occupied vboxes
 *   is near in color to a nearly constant color region of the
 *   image, dithering can inject pixels from the large vbox into
 *   the nearly uniform region.  These pixels can be very far away
 *   in color, and the oscillations are highly visible.  To prevent
 *   this, we can take either or both of these actions:
 *
 *     (1) Subdivide a fraction (< 1.0) based on population, and
 *         do the rest of the subdivision based on the product of
 *         the vbox volume and its population.  By using the product,
 *         we avoid further subdivision of nearly empty vboxes, and
 *         directly target large vboxes with significant population.
 *
 *     (2) Threshold the excess color transferred in dithering to
 *         neighboring pixels.
 *
 *   Doing either of these will stop the most annoying oscillations
 *   in dithering.  Furthermore, by doing (1), we also improve the
 *   rendering of regions of nearly constant color, both with and
 *   without dithering.  It turns out that the image quality is
 *   not sensitive to the value of the parameter in (1); values
 *   between 0.3 and 0.9 give very good results.
 *
 *   Here's the lesson: subdivide the color space into vboxes such
 *   that (1) the most populated vboxes that can be further
 *   subdivided (i.e., that occupy more than one quantum volume
 *   in color space) all have approximately the same population,
 *   and (2) all large vboxes have no significant population.
 *   If these conditions are met, the quantization will be excellent.
 *
 *   Once the subdivision has been made, the colormap is generated,
 *   with one color for each vbox and using the average color in the vbox.
 *   At the same time, the histogram array is converted to an inverse
 *   colormap table, storing the colormap index in every cell in the
 *   vbox.  Finally, using both the colormap and the inverse colormap,
 *   a colormapped pix is quickly generated from the original rgb pix.
 *
 *   In the present implementation, subdivided regions of colorspace
 *   that are not occupied are retained, but not further subdivided.
 *   This is required for our inverse colormap lookup table for
 *   dithering, because dithered pixels may fall into these unoccupied
 *   regions.  For such empty regions, we use the center as the rgb
 *   colormap value.
 *
 *   This variation on median cut can be referred to as "Modified Median
 *   Cut" quantization, or MMCQ.  Overall, the undithered MMCQ gives
 *   comparable results to the two-pass Octcube Quantizer (OQ).
 *   Comparing the two methods on the test24.jpg painting, we see:
 *
 *     (1) For rendering spot color (the various reds and pinks in
 *         the image), MMCQ is not as good as OQ.
 *
 *     (2) For rendering majority color regions, MMCQ does a better
 *         job of avoiding posterization.  That is, it does better
 *         dividing the color space up in the most heavily populated regions.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

    /* Median cut 3-d volume element.  Sort on first element, which
     * can be the number of pixels, the volume or a combination
     * of these.   */
struct L_Box3d
{
    l_float32        sortparam;  /* parameter on which to sort the vbox */
    l_int32          npix;       /* number of pixels in the vbox        */
    l_int32          vol;        /* quantized volume of vbox            */
    l_int32          r1;         /* min r index in the vbox             */
    l_int32          r2;         /* max r index in the vbox             */
    l_int32          g1;         /* min g index in the vbox             */
    l_int32          g2;         /* max g index in the vbox             */
    l_int32          b1;         /* min b index in the vbox             */
    l_int32          b2;         /* max b index in the vbox             */
};
typedef struct L_Box3d  L_BOX3D;

    /* Static median cut helper functions */
static PIXCMAP *pixcmapGenerateFromHisto(PIX *pixs, l_int32 depth,
                                         l_int32 *histo, l_int32 histosize,
                                         l_int32 sigbits);
static PIX *pixQuantizeWithColormap(PIX *pixs, l_int32 ditherflag,
                                    l_int32 outdepth,
                                    PIXCMAP *cmap, l_int32 *indexmap,
                                    l_int32 mapsize, l_int32 sigbits);
static void getColorIndexMedianCut(l_uint32 pixel, l_int32 rshift,
                                   l_uint32 mask, l_int32 sigbits,
                                   l_int32 *pindex);
static L_BOX3D *pixGetColorRegion(PIX *pixs, l_int32 sigbits,
                                  l_int32 subsample);
static l_int32 medianCutApply(l_int32 *histo, l_int32 sigbits,
                              L_BOX3D *vbox, L_BOX3D **pvbox1,
                              L_BOX3D **pvbox2);
static PIXCMAP *pixcmapGenerateFromMedianCuts(L_HEAP *lh, l_int32 *histo,
                                              l_int32 sigbits);
static l_int32 vboxGetAverageColor(L_BOX3D *vbox, l_int32 *histo,
                                   l_int32 sigbits, l_int32 index,
                                   l_int32  *prval, l_int32 *pgval,
                                   l_int32  *pbval);
static l_int32 vboxGetCount(L_BOX3D *vbox, l_int32 *histo, l_int32 sigbits);
static l_int32 vboxGetVolume(L_BOX3D *vbox);
static L_BOX3D *box3dCreate(l_int32 r1, l_int32 r2, l_int32 g1,
                            l_int32 g2, l_int32 b1, l_int32 b2);
static L_BOX3D *box3dCopy(L_BOX3D *vbox);


    /* 5 significant bits for each component is generally satisfactory */
static const l_int32  DefaultSigBits = 5;
static const l_int32  MaxItersAllowed = 5000;  /* prevents infinite looping */

    /* Specify fraction of vboxes made that are sorted on population alone.
     * The remaining vboxes are sorted on (population * vbox-volume).  */
static const l_float32  FractByPopulation = 0.85f;

    /* To get the max value of 'dif' in the dithering color transfer,
     * divide DifCap by 8. */
static const l_int32  DifCap = 100;


#ifndef   NO_CONSOLE_IO
#define   DEBUG_MC_COLORS       0
#define   DEBUG_SPLIT_AXES      0
#endif   /* ~NO_CONSOLE_IO */

/*------------------------------------------------------------------------*
 *                                 High level                             *
 *------------------------------------------------------------------------*/
/*!
 * \brief   pixMedianCutQuant()
 *
 * \param[in]    pixs         32 bpp; rgb color
 * \param[in]    ditherflag   1 for dither; 0 for no dither
 * \return  pixd 8 bit with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Simple interface.  See pixMedianCutQuantGeneral() for
 *          use of defaulted parameters.
 * </pre>
 */
PIX *
pixMedianCutQuant(PIX     *pixs,
                  l_int32  ditherflag)
{
    return pixMedianCutQuantGeneral(pixs, ditherflag,
                                    0, 256, DefaultSigBits, 1, 1);
}


/*!
 * \brief   pixMedianCutQuantGeneral()
 *
 * \param[in]    pixs         32 bpp; rgb color
 * \param[in]    ditherflag   1 for dither; 0 for no dither
 * \param[in]    outdepth     output depth; valid: 0, 1, 2, 4, 8
 * \param[in]    maxcolors    between 2 and 256
 * \param[in]    sigbits      valid: 5 or 6; use 0 for default
 * \param[in]    maxsub       max subsampling, integer; use 0 for default;
 *                            1 for no subsampling
 * \param[in]    checkbw      1 to check if color content is very small,
 *                            0 to assume there is sufficient color
 * \return  pixd   8 bit with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) %maxcolors must be in the range [2 ... 256].
 *      (2) Use %outdepth = 0 to have the output depth computed as the
 *          minimum required to hold the actual colors found, given
 *          the %maxcolors constraint.
 *      (3) Use %outdepth = 1, 2, 4 or 8 to specify the output depth.
 *          In that case, %maxcolors must not exceed 2^(outdepth).
 *      (4) If there are fewer quantized colors in the image than %maxcolors,
 *          the colormap is simply generated from those colors.
 *      (5) %maxsub is the maximum allowed subsampling to be used in the
 *          computation of the color histogram and region of occupied
 *          color space.  The subsampling is chosen internally for
 *          efficiency, based on the image size, but this parameter
 *          limits it.  Use %maxsub = 0 for the internal default, which is the
 *          maximum allowed subsampling.  Use %maxsub = 1 to prevent
 *          subsampling.  In general use %maxsub >= 1 to specify the
 *          maximum subsampling to be allowed, where the actual subsampling
 *          will be the minimum of this value and the internally
 *          determined default value.
 *      (6) %sigbits can be 5 or 6.  There are 2^24 colors in the color space.
 *              sigbits     # of volume elems    # of colors in a volume elem
 *              --------------------------------------------------------------
 *                 5              2^15                  2^9 = 512
 *                 6              2^18                  2^6 = 64
 *          Volume in color space is measured in the number of volume elements.
 *      (7) If the image appears gray because either most of the pixels
 *          are gray or most of the pixels are essentially black or white,
 *          the image is trivially quantized with a grayscale colormap.  The
 *          reason is that median cut divides the color space into rectangular
 *          regions, and it does a very poor job if all the pixels are
 *          near the diagonal of the color space cube.
 * </pre>
 */
PIX *
pixMedianCutQuantGeneral(PIX     *pixs,
                         l_int32  ditherflag,
                         l_int32  outdepth,
                         l_int32  maxcolors,
                         l_int32  sigbits,
                         l_int32  maxsub,
                         l_int32  checkbw)
{
l_int32    i, subsample, histosize, smalln, ncolors, niters, popcolors;
l_int32    w, h, minside, factor, index, rval, gval, bval;
l_int32   *histo;
l_float32  maxprod, prod, norm, pixfract, colorfract;
L_BOX3D   *vbox, *vbox1, *vbox2;
L_HEAP    *lh, *lhs;
PIX       *pixd;
PIXCMAP   *cmap;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (maxcolors < 2 || maxcolors > 256)
        return (PIX *)ERROR_PTR("maxcolors not in [2...256]", __func__, NULL);
    if (outdepth != 0 && outdepth != 1 && outdepth != 2 && outdepth != 4 &&
        outdepth != 8)
        return (PIX *)ERROR_PTR("outdepth not in {0,1,2,4,8}", __func__, NULL);
    if (outdepth > 0 && (maxcolors > (1 << outdepth)))
        return (PIX *)ERROR_PTR("maxcolors > 2^(outdepth)", __func__, NULL);
    if (sigbits == 0)
        sigbits = DefaultSigBits;
    else if (sigbits < 5 || sigbits > 6)
        return (PIX *)ERROR_PTR("sigbits not 5 or 6", __func__, NULL);
    if (maxsub <= 0)
        maxsub = 10;  /* default will prevail for 10^7 pixels or less */

        /* Determine if the image has sufficient color content.
         * If pixfract << 1, most pixels are close to black or white.
         * If colorfract << 1, the pixels that are not near
         *   black or white have very little color.
         * If with little color, quantize with a grayscale colormap. */
    pixGetDimensions(pixs, &w, &h, NULL);
    if (checkbw) {
        minside = L_MIN(w, h);
        factor = L_MAX(1, minside / 400);
        pixColorFraction(pixs, 20, 244, 20, factor, &pixfract, &colorfract);
        if (pixfract * colorfract < 0.00025) {
            L_INFO("\n  Pixel fraction neither white nor black = %6.3f"
                   "\n  Color fraction of those pixels = %6.3f"
                   "\n  Quantizing in gray\n",
                   __func__, pixfract, colorfract);
            return pixConvertTo8(pixs, 1);
        }
    }

        /* Compute the color space histogram.  Default sampling
         * is about 10^5 sampled pixels.  */
    if (maxsub == 1) {
        subsample = 1;
    } else {
        subsample = (l_int32)(sqrt((l_float64)(w * h) / 100000.));
        subsample = L_MAX(1, L_MIN(maxsub, subsample));
    }
    histo = pixMedianCutHisto(pixs, sigbits, subsample);
    histosize = 1 << (3 * sigbits);

        /* See if the number of quantized colors is less than maxcolors */
    ncolors = 0;
    smalln = TRUE;
    for (i = 0; i < histosize; i++) {
        if (histo[i])
            ncolors++;
        if (ncolors > maxcolors) {
            smalln = FALSE;
            break;
        }
    }
    if (smalln) {  /* finish up now */
        if (outdepth == 0) {
            if (ncolors <= 2)
                outdepth = 1;
            else if (ncolors <= 4)
                outdepth = 2;
            else if (ncolors <= 16)
                outdepth = 4;
            else
                outdepth = 8;
        }
        cmap = pixcmapGenerateFromHisto(pixs, outdepth,
                                        histo, histosize, sigbits);
        pixd = pixQuantizeWithColormap(pixs, ditherflag, outdepth, cmap,
                                       histo, histosize, sigbits);
        LEPT_FREE(histo);
        return pixd;
    }

        /* Initial vbox: minimum region in colorspace occupied by pixels */
    if (ditherflag || subsample > 1)  /* use full color space */
        vbox = box3dCreate(0, (1 << sigbits) - 1,
                           0, (1 << sigbits) - 1,
                           0, (1 << sigbits) - 1);
    else
        vbox = pixGetColorRegion(pixs, sigbits, subsample);
    vbox->npix = vboxGetCount(vbox, histo, sigbits);
    vbox->vol = vboxGetVolume(vbox);

        /* For a fraction 'popcolors' of the desired 'maxcolors',
         * generate median cuts based on population, putting
         * everything on a priority queue sorted by population. */
    lh = lheapCreate(0, L_SORT_DECREASING);
    lheapAdd(lh, vbox);
    ncolors = 1;
    niters = 0;
    popcolors = (l_int32)(FractByPopulation * maxcolors);
    while (1) {
        vbox = (L_BOX3D *)lheapRemove(lh);
        if (vboxGetCount(vbox, histo, sigbits) == 0)  { /* just put it back */
            lheapAdd(lh, vbox);
            continue;
        }
        medianCutApply(histo, sigbits, vbox, &vbox1, &vbox2);
        if (!vbox1) {
            L_WARNING("vbox1 not defined; shouldn't happen!\n", __func__);
            break;
        }
        if (vbox1->vol > 1)
            vbox1->sortparam = vbox1->npix;
        LEPT_FREE(vbox);
        lheapAdd(lh, vbox1);
        if (vbox2) {  /* vbox2 can be NULL */
            if (vbox2->vol > 1)
                vbox2->sortparam = vbox2->npix;
            lheapAdd(lh, vbox2);
            ncolors++;
        }
        if (ncolors >= popcolors)
            break;
        if (niters++ > MaxItersAllowed) {
            L_WARNING("infinite loop; perhaps too few pixels!\n", __func__);
            break;
        }
    }

        /* Re-sort by the product of pixel occupancy times the size
         * in color space.  Normalize to the largest product to avoid
         * integer overflow. */
    maxprod = 0.0;
    for (i = 0; i < lh->n; i++) {
        if ((vbox = (L_BOX3D *)lheapGetElement(lh, i)) == NULL)
            continue;
        prod = (l_float32)vbox->npix * (l_float32)vbox->vol;
        if (prod > maxprod) maxprod = prod;
    }
    norm = (maxprod == 0) ? 1.0 : 1000000.0 / maxprod;
    lhs = lheapCreate(0, L_SORT_DECREASING);
    while ((vbox = (L_BOX3D *)lheapRemove(lh))) {
        vbox->sortparam = norm * vbox->npix * vbox->vol;
        lheapAdd(lhs, vbox);
    }
    lheapDestroy(&lh, TRUE);

        /* For the remaining (maxcolors - popcolors), generate the
         * median cuts using the (npix * vol) sorting. */
    while (1) {
        vbox = (L_BOX3D *)lheapRemove(lhs);
        if (vboxGetCount(vbox, histo, sigbits) == 0)  { /* just put it back */
            lheapAdd(lhs, vbox);
            continue;
        }
        medianCutApply(histo, sigbits, vbox, &vbox1, &vbox2);
        if (!vbox1) {
            L_WARNING("vbox1 not defined; shouldn't happen!\n", __func__);
            break;
        }
        if (vbox1->vol > 1)
            vbox1->sortparam = norm * vbox1->npix * vbox1->vol;
        LEPT_FREE(vbox);
        lheapAdd(lhs, vbox1);
        if (vbox2) {  /* vbox2 can be NULL */
            if (vbox2->vol > 1)
                vbox2->sortparam = norm * vbox2->npix * vbox2->vol;
            lheapAdd(lhs, vbox2);
            ncolors++;
        }
        if (ncolors >= maxcolors)
            break;
        if (niters++ > MaxItersAllowed) {
            L_WARNING("infinite loop; perhaps too few pixels!\n", __func__);
            break;
        }
    }

        /* Re-sort by pixel occupancy.  This is not necessary,
         * but it makes a more useful listing.  */
    lh = lheapCreate(0, L_SORT_DECREASING);
    while ((vbox = (L_BOX3D *)lheapRemove(lhs))) {
        vbox->sortparam = vbox->npix;
/*        vbox->sortparam = vbox->npix * vbox->vol; */
        lheapAdd(lh, vbox);
    }
    lheapDestroy(&lhs, TRUE);

        /* Generate colormap from median cuts and quantize pixd */
    cmap = pixcmapGenerateFromMedianCuts(lh, histo, sigbits);
    if (outdepth == 0) {
        ncolors = pixcmapGetCount(cmap);
        if (ncolors <= 2)
            outdepth = 1;
        else if (ncolors <= 4)
            outdepth = 2;
        else if (ncolors <= 16)
            outdepth = 4;
        else
            outdepth = 8;
    }
    pixd = pixQuantizeWithColormap(pixs, ditherflag, outdepth, cmap,
                                   histo, histosize, sigbits);

        /* Force darkest color to black if each component <= 4 */
    pixcmapGetRankIntensity(cmap, 0.0, &index);
    pixcmapGetColor(cmap, index, &rval, &gval, &bval);
    if (rval < 5 && gval < 5 && bval < 5)
        pixcmapResetColor(cmap, index, 0, 0, 0);

        /* Force lightest color to white if each component >= 252 */
    pixcmapGetRankIntensity(cmap, 1.0, &index);
    pixcmapGetColor(cmap, index, &rval, &gval, &bval);
    if (rval > 251 && gval > 251 && bval > 251)
        pixcmapResetColor(cmap, index, 255, 255, 255);

    lheapDestroy(&lh, TRUE);
    LEPT_FREE(histo);
    return pixd;
}


/*!
 * \brief   pixMedianCutQuantMixed()
 *
 * \param[in]    pixs          32 bpp; rgb color
 * \param[in]    ncolor        maximum number of colors assigned to
 *                             pixels with significant color
 * \param[in]    ngray          number of gray colors to be used; must be >= 2
 * \param[in]    darkthresh    threshold near black; if the lightest component
 *                             is below this, the pixel is not considered to
 *                             be gray or color; uses 0 for default
 * \param[in]    lightthresh   threshold near white; if the darkest component
 *                             is above this, the pixel is not considered to
 *                             be gray or color; use 0 for default
 * \param[in]    diffthresh    thresh for the max difference between component
 *                             values; for differences below this, the pixel
 *                             is considered to be gray; use 0 for default
 * \return  pixd   8 bpp cmapped, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) ncolor + ngray must not exceed 255.
 *      (2) The method makes use of pixMedianCutQuantGeneral() with
 *          minimal addition.
 *          (a) Preprocess the image, setting all pixels with little color
 *              to black, and populating an auxiliary 8 bpp image with the
 *              expected colormap values corresponding to the set of
 *              quantized gray values.
 *          (b) Color quantize the altered input image to n + 1 colors.
 *          (c) Augment the colormap with the gray indices, and
 *              substitute the gray quantized values from the auxiliary
 *              image for those in the color quantized output that had
 *              been quantized as black.
 *      (3) Median cut color quantization is relatively poor for grayscale
 *          images with many colors, when compared to octcube quantization.
 *          Thus, for images with both gray and color, it is important
 *          to quantize the gray pixels by another method.  Here, we
 *          are conservative in detecting color, preferring to use
 *          a few extra bits to encode colorful pixels that push them
 *          to gray.  This is particularly reasonable with this function,
 *          because it handles the gray and color pixels separately,
 *          using median cut color quantization for the color pixels
 *          and equal-bin grayscale quantization for the non-color pixels.
 * </pre>
 */
PIX *
pixMedianCutQuantMixed(PIX     *pixs,
                       l_int32  ncolor,
                       l_int32  ngray,
                       l_int32  darkthresh,
                       l_int32  lightthresh,
                       l_int32  diffthresh)
{
l_int32    i, j, w, h, wplc, wplg, wpld, nc, unused, iscolor, factor, minside;
l_int32    rval, gval, bval, minval, maxval, val, grayval;
l_float32  pixfract, colorfract;
l_int32   *lut;
l_uint32  *datac, *datag, *datad, *linec, *lineg, *lined;
PIX       *pixc, *pixg, *pixd;
PIXCMAP   *cmap;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (ngray < 2)
        return (PIX *)ERROR_PTR("ngray < 2", __func__, NULL);
    if (ncolor + ngray > 255)
        return (PIX *)ERROR_PTR("ncolor + ngray > 255", __func__, NULL);
    if (darkthresh <= 0) darkthresh = 20;
    if (lightthresh <= 0) lightthresh = 244;
    if (diffthresh <= 0) diffthresh = 20;

        /* First check if this should be quantized in gray.
         * Use a more sensitive parameter for detecting color than with
         * pixMedianCutQuantGeneral(), because this function can handle
         * gray pixels well.  */
    pixGetDimensions(pixs, &w, &h, NULL);
    minside = L_MIN(w, h);
    factor = L_MAX(1, minside / 400);
    pixColorFraction(pixs, darkthresh, lightthresh, diffthresh, factor,
                     &pixfract, &colorfract);
    if (pixfract * colorfract < 0.0001) {
        L_INFO("\n  Pixel fraction neither white nor black = %6.3f"
                      "\n  Color fraction of those pixels = %6.3f"
                      "\n  Quantizing in gray\n",
                      __func__, pixfract, colorfract);
        pixg = pixConvertTo8(pixs, 0);
        pixd = pixThresholdOn8bpp(pixg, ngray, 1);
        pixDestroy(&pixg);
        return pixd;
    }

        /* OK, there is color in the image.
         * Preprocess to handle the gray pixels.  Set the color pixels in pixc
         * to black, and store their (eventual) colormap indices in pixg.*/
    pixc = pixCopy(NULL, pixs);
    pixg = pixCreate(w, h, 8);  /* color pixels will remain 0 here */
    datac = pixGetData(pixc);
    datag = pixGetData(pixg);
    wplc = pixGetWpl(pixc);
    wplg = pixGetWpl(pixg);
    lut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    for (i = 0; i < 256; i++)
        lut[i] = ncolor + 1 + (i * (ngray - 1) + 128) / 255;
    for (i = 0; i < h; i++) {
        linec = datac + i * wplc;
        lineg = datag + i * wplg;
        for (j = 0; j < w; j++) {
            iscolor = FALSE;
            extractRGBValues(linec[j], &rval, &gval, &bval);
            minval = L_MIN(rval, gval);
            minval = L_MIN(minval, bval);
            maxval = L_MAX(rval, gval);
            maxval = L_MAX(maxval, bval);
            if (maxval >= darkthresh &&
                minval <= lightthresh &&
                maxval - minval >= diffthresh) {
                iscolor = TRUE;
            }
            if (!iscolor) {
                linec[j] = 0x0;  /* set to black */
                grayval = (maxval + minval) / 2;
                SET_DATA_BYTE(lineg, j, lut[grayval]);
            }
        }
    }

        /* Median cut on color pixels plus black */
    pixd = pixMedianCutQuantGeneral(pixc, FALSE, 8, ncolor + 1,
                                    DefaultSigBits, 1, 0);

        /* Augment the colormap with gray values.  The new cmap
         * indices should agree with the values previously stored in pixg. */
    cmap = pixGetColormap(pixd);
    nc = pixcmapGetCount(cmap);
    unused = ncolor  + 1 - nc;
    if (unused < 0)
        L_ERROR("Too many colors: extra = %d\n", __func__, -unused);
    if (unused > 0) {  /* fill in with black; these won't be used */
        L_INFO("%d unused colors\n", __func__, unused);
        for (i = 0; i < unused; i++)
            pixcmapAddColor(cmap, 0, 0, 0);
    }
    for (i = 0; i < ngray; i++) {
        grayval = (255 * i) / (ngray - 1);
        pixcmapAddColor(cmap, grayval, grayval, grayval);
    }

        /* Substitute cmap indices for the gray pixels into pixd */
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        lineg = datag + i * wplg;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(lineg, j);  /* if 0, it's a color pixel */
            if (val)
                SET_DATA_BYTE(lined, j, val);
        }
    }

    pixDestroy(&pixc);
    pixDestroy(&pixg);
    LEPT_FREE(lut);
    return pixd;
}


/*!
 * \brief   pixFewColorsMedianCutQuantMixed()
 *
 * \param[in]    pixs          32 bpp rgb
 * \param[in]    ncolor        number of colors to be assigned to pixels
 *                             with significant color
 * \param[in]    ngray         number of gray colors to be used; must be >= 2
 * \param[in]    maxncolors    maximum number of colors to be returned from
 *                             pixColorsForQuantization(); use 0 for default
 * \param[in]    darkthresh    threshold near black; if the lightest component
 *                             is below this, the pixel is not considered to
 *                             be gray or color; use 0 for default
 * \param[in]    lightthresh   threshold near white; if the darkest component
 *                             is above this, the pixel is not considered to
 *                             be gray or color; use 0 for default
 * \param[in]    diffthresh    thresh for the max difference between component
 *                             values; for differences below this, the pixel
 *                             is considered to be gray; use 0 for default
 * \return  pixd   8 bpp, median cut quantized for pixels that are
 *                 not gray; gray pixels are quantized separately over
 *                 the full gray range; null if too many colors or on error
 *
 * <pre>
 * Notes:
 *      (1) This is the "few colors" version of pixMedianCutQuantMixed().
 *          It fails (returns NULL) if it finds more than maxncolors, but
 *          otherwise it gives the same result.
 *      (2) Recommended input parameters are:
 *              %maxncolors:  20
 *              %darkthresh:  20
 *              %lightthresh: 244
 *              %diffthresh:  15  (any higher can miss colors differing
 *                                 slightly from gray)
 *      (3) Both ncolor and ngray should be at least equal to maxncolors.
 *          If they're not, they are automatically increased, and a
 *          warning is given.
 *      (4) If very little color content is found, the input is
 *          converted to gray and quantized in equal intervals.
 *      (5) This can be useful for quantizing orthographically generated
 *          images such as color maps, where there may be more than 256 colors
 *          because of aliasing or jpeg artifacts on text or lines, but
 *          there are a relatively small number of solid colors.
 *      (6) Example of usage:
 *             // Try to quantize, using default values for mixed med cut
 *             Pix *pixq = pixFewColorsMedianCutQuantMixed(pixs, 100, 20,
 *                             0, 0, 0, 0);
 *             if (!pixq)  // too many colors; don't quantize
 *                 pixq = pixClone(pixs);
 * </pre>
 */
PIX *
pixFewColorsMedianCutQuantMixed(PIX       *pixs,
                                l_int32    ncolor,
                                l_int32    ngray,
                                l_int32    maxncolors,
                                l_int32    darkthresh,
                                l_int32    lightthresh,
                                l_int32    diffthresh)
{
l_int32  ncolors, iscolor;
PIX     *pixg, *pixd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (maxncolors <= 0) maxncolors = 20;
    if (darkthresh <= 0) darkthresh = 20;
    if (lightthresh <= 0) lightthresh = 244;
    if (diffthresh <= 0) diffthresh = 15;
    if (ncolor < maxncolors) {
        L_WARNING("ncolor too small; setting to %d\n", __func__, maxncolors);
        ncolor = maxncolors;
    }
    if (ngray < maxncolors) {
        L_WARNING("ngray too small; setting to %d\n", __func__, maxncolors);
        ngray = maxncolors;
    }

        /* Estimate the color content and the number of colors required */
    pixColorsForQuantization(pixs, 15, &ncolors, &iscolor, 0);

        /* Note that maxncolors applies to all colors required to quantize,
         * both gray and colorful */
    if (ncolors > maxncolors)
        return (PIX *)ERROR_PTR("too many colors", __func__, NULL);

        /* If no color, return quantized gray pix */
    if (!iscolor) {
        pixg = pixConvertTo8(pixs, 0);
        pixd = pixThresholdOn8bpp(pixg, ngray, 1);
        pixDestroy(&pixg);
        return pixd;
    }

        /* Use the mixed gray/color quantizer */
    return pixMedianCutQuantMixed(pixs, ncolor, ngray, darkthresh,
                                  lightthresh, diffthresh);
}



/*------------------------------------------------------------------------*
 *                        Median cut indexed histogram                    *
 *------------------------------------------------------------------------*/
/*!
 * \brief   pixMedianCutHisto()
 *
 * \param[in]    pixs        32 bpp; rgb color
 * \param[in]    sigbits     valid: 5 or 6
 * \param[in]    subsample   integer > 0
 * \return  histo    1-d array, giving the number of pixels in each
 *                   quantized region of color space, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Array is indexed by (3 * sigbits) bits.  The array size
 *          is 2^(3 * sigbits).
 *      (2) Indexing into the array from rgb uses red sigbits as
 *          most significant and blue as least.
 * </pre>
 */
l_int32 *
pixMedianCutHisto(PIX     *pixs,
                  l_int32  sigbits,
                  l_int32  subsample)
{
l_int32    i, j, w, h, wpl, rshift, index, histosize;
l_int32   *histo;
l_uint32   mask, pixel;
l_uint32  *data, *line;

    if (!pixs)
        return (l_int32 *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (l_int32 *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (sigbits < 5 || sigbits > 6)
        return (l_int32 *)ERROR_PTR("sigbits not 5 or 6", __func__, NULL);
    if (subsample <= 0)
        return (l_int32 *)ERROR_PTR("subsample not > 0", __func__, NULL);

    histosize = 1 << (3 * sigbits);
    if ((histo = (l_int32 *)LEPT_CALLOC(histosize, sizeof(l_int32))) == NULL)
        return (l_int32 *)ERROR_PTR("histo not made", __func__, NULL);

    rshift = 8 - sigbits;
    mask = 0xff >> rshift;
    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i += subsample) {
        line = data + i * wpl;
        for (j = 0; j < w; j += subsample) {
            pixel = line[j];
            getColorIndexMedianCut(pixel, rshift, mask, sigbits, &index);
            histo[index]++;
        }
    }

    return histo;
}


/*------------------------------------------------------------------------*
 *                               Static helpers                           *
 *------------------------------------------------------------------------*/
/*!
 * \brief   pixcmapGenerateFromHisto()
 *
 * \param[in]    pixs      32 bpp; rgb color
 * \param[in]    depth     of colormap
 * \param[in]    histo
 * \param[in]    histosize
 * \param[in]    sigbits
 * \return  colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is used when the number of colors in the histo
 *          is not greater than maxcolors.
 *      (2) As a side-effect, the histo becomes an inverse colormap,
 *          labeling the cmap indices for each existing color.
 * </pre>
 */
static PIXCMAP *
pixcmapGenerateFromHisto(PIX      *pixs,
                         l_int32   depth,
                         l_int32  *histo,
                         l_int32   histosize,
                         l_int32   sigbits)
{
l_int32   i, index, shift, rval, gval, bval;
l_uint32  mask;
PIXCMAP  *cmap;

    if (!pixs)
        return (PIXCMAP *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIXCMAP *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (!histo)
        return (PIXCMAP *)ERROR_PTR("histo not defined", __func__, NULL);

        /* Capture the rgb values of each occupied cube in the histo,
         * and re-label the histo value with the colormap index. */
    cmap = pixcmapCreate(depth);
    shift = 8 - sigbits;
    mask = 0xff >> shift;
    for (i = 0, index = 0; i < histosize; i++) {
        if (histo[i]) {
            rval = (i >> (2 * sigbits)) << shift;
            gval = ((i >> sigbits) & mask) << shift;
            bval = (i & mask) << shift;
            pixcmapAddColor(cmap, rval, gval, bval);
            histo[i] = index++;
        }
    }

    return cmap;
}


/*!
 * \brief   pixQuantizeWithColormap()
 *
 * \param[in]    pixs        32 bpp; rgb color
 * \param[in]    ditherflag  1 for dither; 0 for no dither
 * \param[in]    outdepth    depth of the returned pixd
 * \param[in]    cmap        colormap
 * \param[in]    indexmap    lookup table
 * \param[in]    mapsize     size of the lookup table
 * \param[in]    sigbits     significant bits in output
 * \return  pixd   quantized to colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The indexmap is a LUT that takes the rgb indices of the
 *          pixel and returns the index into the colormap.
 *      (2) If ditherflag is 1, %outdepth is ignored and the output
 *          depth is set to 8.
 * </pre>
 */
static PIX *
pixQuantizeWithColormap(PIX      *pixs,
                        l_int32   ditherflag,
                        l_int32   outdepth,
                        PIXCMAP  *cmap,
                        l_int32  *indexmap,
                        l_int32   mapsize,
                        l_int32   sigbits)
{
l_uint8   *bufu8r, *bufu8g, *bufu8b;
l_int32    i, j, w, h, wpls, wpld, rshift, index, cmapindex, success;
l_int32    rval, gval, bval, rc, gc, bc;
l_int32    dif, val1, val2, val3;
l_int32   *buf1r, *buf1g, *buf1b, *buf2r, *buf2g, *buf2b;
l_uint32  *datas, *datad, *lines, *lined;
l_uint32   mask, pixel;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (!cmap)
        return (PIX *)ERROR_PTR("cmap not defined", __func__, NULL);
    if (!indexmap)
        return (PIX *)ERROR_PTR("indexmap not defined", __func__, NULL);
    if (ditherflag)
        outdepth = 8;

    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, outdepth);
    pixSetColormap(pixd, cmap);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);

    rshift = 8 - sigbits;
    mask = 0xff >> rshift;
    if (ditherflag == 0) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            if (outdepth == 1) {
                for (j = 0; j < w; j++) {
                    pixel = lines[j];
                    getColorIndexMedianCut(pixel, rshift, mask,
                                           sigbits, &index);
                    if (indexmap[index])
                        SET_DATA_BIT(lined, j);
                }
            } else if (outdepth == 2) {
                for (j = 0; j < w; j++) {
                    pixel = lines[j];
                    getColorIndexMedianCut(pixel, rshift, mask,
                                           sigbits, &index);
                    SET_DATA_DIBIT(lined, j, indexmap[index]);
                }
            } else if (outdepth == 4) {
                for (j = 0; j < w; j++) {
                    pixel = lines[j];
                    getColorIndexMedianCut(pixel, rshift, mask,
                                           sigbits, &index);
                    SET_DATA_QBIT(lined, j, indexmap[index]);
                }
            } else {  /* outdepth == 8 */
                for (j = 0; j < w; j++) {
                    pixel = lines[j];
                    getColorIndexMedianCut(pixel, rshift, mask,
                                           sigbits, &index);
                    SET_DATA_BYTE(lined, j, indexmap[index]);
                }
            }
        }
    } else {  /* ditherflag == 1 */
        success = TRUE;
        bufu8r = bufu8g = bufu8b = NULL;
        buf1r = buf1g = buf1b = buf2r = buf2g = buf2b = NULL;
        bufu8r = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
        bufu8g = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
        bufu8b = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
        buf1r = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf1g = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf1b = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf2r = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf2g = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf2b = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        if (!bufu8r || !bufu8g || !bufu8b || !buf1r || !buf1g ||
            !buf1b || !buf2r || !buf2g || !buf2b) {
            L_ERROR("buffer not made\n", __func__);
            success = FALSE;
            goto buffer_cleanup;
        }

            /* Start by priming buf2; line 1 is above line 2 */
        pixGetRGBLine(pixs, 0, bufu8r, bufu8g, bufu8b);
        for (j = 0; j < w; j++) {
            buf2r[j] = 64 * bufu8r[j];
            buf2g[j] = 64 * bufu8g[j];
            buf2b[j] = 64 * bufu8b[j];
        }

        for (i = 0; i < h - 1; i++) {
                /* Swap data 2 --> 1, and read in new line 2 */
            memcpy(buf1r, buf2r, 4 * w);
            memcpy(buf1g, buf2g, 4 * w);
            memcpy(buf1b, buf2b, 4 * w);
            pixGetRGBLine(pixs, i + 1, bufu8r, bufu8g, bufu8b);
            for (j = 0; j < w; j++) {
                buf2r[j] = 64 * bufu8r[j];
                buf2g[j] = 64 * bufu8g[j];
                buf2b[j] = 64 * bufu8b[j];
            }

                /* Dither */
            lined = datad + i * wpld;
            for (j = 0; j < w - 1; j++) {
                rval = buf1r[j] / 64;
                gval = buf1g[j] / 64;
                bval = buf1b[j] / 64;
                index = ((rval >> rshift) << (2 * sigbits)) +
                        ((gval >> rshift) << sigbits) + (bval >> rshift);
                cmapindex = indexmap[index];
                SET_DATA_BYTE(lined, j, cmapindex);
                pixcmapGetColor(cmap, cmapindex, &rc, &gc, &bc);

                dif = buf1r[j] / 8 - 8 * rc;
                if (dif > DifCap) dif = DifCap;
                if (dif < -DifCap) dif = -DifCap;
                if (dif != 0) {
                    val1 = buf1r[j + 1] + 3 * dif;
                    val2 = buf2r[j] + 3 * dif;
                    val3 = buf2r[j + 1] + 2 * dif;
                    if (dif > 0) {
                        buf1r[j + 1] = L_MIN(16383, val1);
                        buf2r[j] = L_MIN(16383, val2);
                        buf2r[j + 1] = L_MIN(16383, val3);
                    } else {
                        buf1r[j + 1] = L_MAX(0, val1);
                        buf2r[j] = L_MAX(0, val2);
                        buf2r[j + 1] = L_MAX(0, val3);
                    }
                }

                dif = buf1g[j] / 8 - 8 * gc;
                if (dif > DifCap) dif = DifCap;
                if (dif < -DifCap) dif = -DifCap;
                if (dif != 0) {
                    val1 = buf1g[j + 1] + 3 * dif;
                    val2 = buf2g[j] + 3 * dif;
                    val3 = buf2g[j + 1] + 2 * dif;
                    if (dif > 0) {
                        buf1g[j + 1] = L_MIN(16383, val1);
                        buf2g[j] = L_MIN(16383, val2);
                        buf2g[j + 1] = L_MIN(16383, val3);
                    } else {
                        buf1g[j + 1] = L_MAX(0, val1);
                        buf2g[j] = L_MAX(0, val2);
                        buf2g[j + 1] = L_MAX(0, val3);
                    }
                }

                dif = buf1b[j] / 8 - 8 * bc;
                if (dif > DifCap) dif = DifCap;
                if (dif < -DifCap) dif = -DifCap;
                if (dif != 0) {
                    val1 = buf1b[j + 1] + 3 * dif;
                    val2 = buf2b[j] + 3 * dif;
                    val3 = buf2b[j + 1] + 2 * dif;
                    if (dif > 0) {
                        buf1b[j + 1] = L_MIN(16383, val1);
                        buf2b[j] = L_MIN(16383, val2);
                        buf2b[j + 1] = L_MIN(16383, val3);
                    } else {
                        buf1b[j + 1] = L_MAX(0, val1);
                        buf2b[j] = L_MAX(0, val2);
                        buf2b[j + 1] = L_MAX(0, val3);
                    }
                }
            }

                /* Get last pixel in row; no downward propagation */
            rval = buf1r[w - 1] / 64;
            gval = buf1g[w - 1] / 64;
            bval = buf1b[w - 1] / 64;
            index = ((rval >> rshift) << (2 * sigbits)) +
                    ((gval >> rshift) << sigbits) + (bval >> rshift);
            SET_DATA_BYTE(lined, w - 1, indexmap[index]);
        }

            /* Get last row of pixels; no leftward propagation */
        lined = datad + (h - 1) * wpld;
        for (j = 0; j < w; j++) {
            rval = buf2r[j] / 64;
            gval = buf2g[j] / 64;
            bval = buf2b[j] / 64;
            index = ((rval >> rshift) << (2 * sigbits)) +
                    ((gval >> rshift) << sigbits) + (bval >> rshift);
            SET_DATA_BYTE(lined, j, indexmap[index]);
        }

buffer_cleanup:
        LEPT_FREE(bufu8r);
        LEPT_FREE(bufu8g);
        LEPT_FREE(bufu8b);
        LEPT_FREE(buf1r);
        LEPT_FREE(buf1g);
        LEPT_FREE(buf1b);
        LEPT_FREE(buf2r);
        LEPT_FREE(buf2g);
        LEPT_FREE(buf2b);
        if (!success) pixDestroy(&pixd);
    }

    return pixd;
}


/*!
 * \brief   getColorIndexMedianCut()
 *
 * \param[in]    pixel     32 bit rgb
 * \param[in]    rshift    of component: 8 - sigbits
 * \param[in]    mask      over sigbits
 * \param[in]    sigbits
 * \param[out]   pindex    rgb index value
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This is used on each pixel in the source image.  No checking
 *          is done on input values.
 * </pre>
 */
static void
getColorIndexMedianCut(l_uint32  pixel,
                       l_int32   rshift,
                       l_uint32  mask,
                       l_int32   sigbits,
                       l_int32  *pindex)
{
l_int32  rval, gval, bval;

    rval = pixel >> (24 + rshift);
    gval = (pixel >> (16 + rshift)) & mask;
    bval = (pixel >> (8 + rshift)) & mask;
    *pindex = (rval << (2 * sigbits)) + (gval << sigbits) + bval;
    return;
}


/*!
 * \brief   pixGetColorRegion()
 *
 * \param[in]    pixs        32 bpp; rgb color
 * \param[in]    sigbits     valid: 5, 6
 * \param[in]    subsample   integer > 0
 * \return  vbox    minimum 3D box in color space enclosing all pixels,
 *                  or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Computes the minimum 3D box in color space enclosing all
 *          pixels in the image.
 * </pre>
 */
static L_BOX3D *
pixGetColorRegion(PIX     *pixs,
                  l_int32  sigbits,
                  l_int32  subsample)
{
l_int32    rmin, rmax, gmin, gmax, bmin, bmax, rval, gval, bval;
l_int32    w, h, wpl, i, j, rshift;
l_uint32   mask, pixel;
l_uint32  *data, *line;

    if (!pixs)
        return (L_BOX3D *)ERROR_PTR("pixs not defined", __func__, NULL);

    rmin = gmin = bmin = 1000000;
    rmax = gmax = bmax = 0;
    rshift = 8 - sigbits;
    mask = 0xff >> rshift;
    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i += subsample) {
        line = data + i * wpl;
        for (j = 0; j < w; j += subsample) {
            pixel = line[j];
            rval = pixel >> (24 + rshift);
            gval = (pixel >> (16 + rshift)) & mask;
            bval = (pixel >> (8 + rshift)) & mask;
            if (rval < rmin)
                rmin = rval;
            else if (rval > rmax)
                rmax = rval;
            if (gval < gmin)
                gmin = gval;
            else if (gval > gmax)
                gmax = gval;
            if (bval < bmin)
                bmin = bval;
            else if (bval > bmax)
                bmax = bval;
        }
    }

    return box3dCreate(rmin, rmax, gmin, gmax, bmin, bmax);
}


/*!
 * \brief   medianCutApply()
 *
 * \param[in]    histo            array; in rgb colorspace
 * \param[in]    sigbits
 * \param[in]    vbox             input 3D box
 * \param[out]   pvbox1, pvbox2   vbox split in two parts
 * \return  0 if OK, 1 on error
 */
static l_int32
medianCutApply(l_int32   *histo,
               l_int32    sigbits,
               L_BOX3D   *vbox,
               L_BOX3D  **pvbox1,
               L_BOX3D  **pvbox2)
{
l_int32   i, j, k, sum, rw, gw, bw, maxw, index;
l_int32   total, left, right;
l_int32   partialsum[128];
L_BOX3D  *vbox1, *vbox2;

    if (pvbox1) *pvbox1 = NULL;
    if (pvbox2) *pvbox2 = NULL;
    if (!histo)
        return ERROR_INT("histo not defined", __func__, 1);
    if (!vbox)
        return ERROR_INT("vbox not defined", __func__, 1);
    if (!pvbox1 || !pvbox2)
        return ERROR_INT("&vbox1 and &vbox2 not both defined", __func__, 1);

    if (vboxGetCount(vbox, histo, sigbits) == 0)
        return ERROR_INT("no pixels in vbox", __func__, 1);

        /* If the vbox occupies just one element in color space, it can't
         * be split.  Leave the 'sortparam' field at 0, so that it goes to
         * the tail of the priority queue and stays there, thereby avoiding
         * an infinite loop (take off, put back on the head) if it
         * happens to be the most populous box! */
    rw = vbox->r2 - vbox->r1 + 1;
    gw = vbox->g2 - vbox->g1 + 1;
    bw = vbox->b2 - vbox->b1 + 1;
    if (rw == 1 && gw == 1 && bw == 1) {
        *pvbox1 = box3dCopy(vbox);
        return 0;
    }

        /* Select the longest axis for splitting */
    maxw = L_MAX(rw, gw);
    maxw = L_MAX(maxw, bw);
#if  DEBUG_SPLIT_AXES
    if (rw == maxw)
        lept_stderr("red split\n");
    else if (gw == maxw)
        lept_stderr("green split\n");
    else
        lept_stderr("blue split\n");
#endif  /* DEBUG_SPLIT_AXES */

        /* Find the partial sum arrays along the selected axis. */
    total = 0;
    if (maxw == rw) {
        for (i = vbox->r1; i <= vbox->r2; i++) {
            sum = 0;
            for (j = vbox->g1; j <= vbox->g2; j++) {
                for (k = vbox->b1; k <= vbox->b2; k++) {
                    index = (i << (2 * sigbits)) + (j << sigbits) + k;
                    sum += histo[index];
                }
            }
            total += sum;
            partialsum[i] = total;
        }
    } else if (maxw == gw) {
        for (i = vbox->g1; i <= vbox->g2; i++) {
            sum = 0;
            for (j = vbox->r1; j <= vbox->r2; j++) {
                for (k = vbox->b1; k <= vbox->b2; k++) {
                    index = (i << sigbits) + (j << (2 * sigbits)) + k;
                    sum += histo[index];
                }
            }
            total += sum;
            partialsum[i] = total;
        }
    } else {  /* maxw == bw */
        for (i = vbox->b1; i <= vbox->b2; i++) {
            sum = 0;
            for (j = vbox->r1; j <= vbox->r2; j++) {
                for (k = vbox->g1; k <= vbox->g2; k++) {
                    index = i + (j << (2 * sigbits)) + (k << sigbits);
                    sum += histo[index];
                }
            }
            total += sum;
            partialsum[i] = total;
        }
    }

        /* Determine the cut planes, making sure that two vboxes
         * are always produced.  Generate the two vboxes and compute
         * the sum in each of them.  Choose the cut plane within
         * the greater of the (left, right) sides of the bin in which
         * the median pixel resides.  Here's the surprise: go halfway
         * into that side.  By doing that, you technically move away
         * from "median cut," but in the process a significant number
         * of low-count vboxes are produced, allowing much better
         * reproduction of low-count spot colors. */
    vbox1 = vbox2 = NULL;
    if (maxw == rw) {
        for (i = vbox->r1; i <= vbox->r2; i++) {
            if (partialsum[i] > total / 2) {
                vbox1 = box3dCopy(vbox);
                vbox2 = box3dCopy(vbox);
                left = i - vbox->r1;
                right = vbox->r2 - i;
                if (left <= right)
                    vbox1->r2 = L_MIN(vbox->r2 - 1, i + right / 2);
                else  /* left > right */
                    vbox1->r2 = L_MAX(vbox->r1, i - 1 - left / 2);
                vbox2->r1 = vbox1->r2 + 1;
                break;
            }
        }
    } else if (maxw == gw) {
        for (i = vbox->g1; i <= vbox->g2; i++) {
            if (partialsum[i] > total / 2) {
                vbox1 = box3dCopy(vbox);
                vbox2 = box3dCopy(vbox);
                left = i - vbox->g1;
                right = vbox->g2 - i;
                if (left <= right)
                    vbox1->g2 = L_MIN(vbox->g2 - 1, i + right / 2);
                else  /* left > right */
                    vbox1->g2 = L_MAX(vbox->g1, i - 1 - left / 2);
                vbox2->g1 = vbox1->g2 + 1;
                break;
            }
        }
    } else {  /* maxw == bw */
        for (i = vbox->b1; i <= vbox->b2; i++) {
            if (partialsum[i] > total / 2) {
                vbox1 = box3dCopy(vbox);
                vbox2 = box3dCopy(vbox);
                left = i - vbox->b1;
                right = vbox->b2 - i;
                if (left <= right)
                    vbox1->b2 = L_MIN(vbox->b2 - 1, i + right / 2);
                else  /* left > right */
                    vbox1->b2 = L_MAX(vbox->b1, i - 1 - left / 2);
                vbox2->b1 = vbox1->b2 + 1;
                break;
            }
        }
    }
    *pvbox1 = vbox1;
    *pvbox2 = vbox2;
    if (!vbox1)
        return ERROR_INT("vbox1 not made; shouldn't happen", __func__, 1);
    if (!vbox2)
        return ERROR_INT("vbox2 not made; shouldn't happen", __func__, 1);
    vbox1->npix = vboxGetCount(vbox1, histo, sigbits);
    vbox2->npix = vboxGetCount(vbox2, histo, sigbits);
    vbox1->vol = vboxGetVolume(vbox1);
    vbox2->vol = vboxGetVolume(vbox2);

    return 0;
}


/*!
 * \brief   pixcmapGenerateFromMedianCuts()
 *
 * \param[in]    lh        priority queue of pointers to vboxes
 * \param[in]    histo
 * \param[in]    sigbits   valid: 5 or 6
 * \return  cmap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Each vbox in the heap represents a color in the colormap.
 *      (2) As a side-effect, the histo becomes an inverse colormap,
 *          where the part of the array correpsonding to each vbox
 *          is labeled with the cmap index for that vbox.  Then
 *          for each rgb pixel, the colormap index is found directly
 *          by mapping the rgb value to the histo array index.
 * </pre>
 */
static PIXCMAP *
pixcmapGenerateFromMedianCuts(L_HEAP   *lh,
                              l_int32  *histo,
                              l_int32   sigbits)
{
l_int32   index, rval, gval, bval;
L_BOX3D  *vbox;
PIXCMAP  *cmap;

    if (!lh)
        return (PIXCMAP *)ERROR_PTR("lh not defined", __func__, NULL);
    if (!histo)
        return (PIXCMAP *)ERROR_PTR("histo not defined", __func__, NULL);

    rval = gval = bval = 0;  /* make compiler happy */
    cmap = pixcmapCreate(8);
    index = 0;
    while (lheapGetCount(lh) > 0) {
        vbox = (L_BOX3D *)lheapRemove(lh);
        vboxGetAverageColor(vbox, histo, sigbits, index, &rval, &gval, &bval);
        pixcmapAddColor(cmap, rval, gval, bval);
        LEPT_FREE(vbox);
        index++;
    }

    return cmap;
}


/*!
 * \brief   vboxGetAverageColor()
 *
 * \param[in]    vbox       3d region of color space for one quantized color
 * \param[in]    histo
 * \param[in]    sigbits    valid: 5 or 6
 * \param[in]    index      if >= 0, assign to all colors in histo in this vbox
 * \param[out]   prval, pgval, pbval    average color
 * \return  cmap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The vbox represents one color in the colormap.
 *      (2) If index >= 0, as a side-effect, all array elements in
 *          the histo corresponding to the vbox are labeled with this
 *          cmap index for that vbox.  Otherwise, the histo array
 *          is not changed.
 *      (3) The vbox is quantized in sigbits.  So the actual 8-bit color
 *          components are found by multiplying the quantized value
 *          by either 4 or 8.  We must add 0.5 to the quantized index
 *          before multiplying to get the approximate 8-bit color in
 *          the center of the vbox; otherwise we get values on
 *          the lower corner.
 * </pre>
 */
static l_int32
vboxGetAverageColor(L_BOX3D  *vbox,
                    l_int32  *histo,
                    l_int32   sigbits,
                    l_int32   index,
                    l_int32  *prval,
                    l_int32  *pgval,
                    l_int32  *pbval)
{
l_int32  i, j, k, ntot, mult, histoindex, rsum, gsum, bsum;

    if (!vbox)
        return ERROR_INT("vbox not defined", __func__, 1);
    if (!histo)
        return ERROR_INT("histo not defined", __func__, 1);
    if (!prval || !pgval || !pbval)
        return ERROR_INT("&p*val not all defined", __func__, 1);

    *prval = *pgval = *pbval = 0;
    ntot = 0;
    mult = 1 << (8 - sigbits);
    rsum = gsum = bsum = 0;
    for (i = vbox->r1; i <= vbox->r2; i++) {
        for (j = vbox->g1; j <= vbox->g2; j++) {
            for (k = vbox->b1; k <= vbox->b2; k++) {
                 histoindex = (i << (2 * sigbits)) + (j << sigbits) + k;
                 ntot += histo[histoindex];
                 rsum += (l_int32)(histo[histoindex] * (i + 0.5) * mult);
                 gsum += (l_int32)(histo[histoindex] * (j + 0.5) * mult);
                 bsum += (l_int32)(histo[histoindex] * (k + 0.5) * mult);
                 if (index >= 0)
                     histo[histoindex] = index;
            }
        }
    }

    if (ntot == 0) {
        *prval = mult * (vbox->r1 + vbox->r2 + 1) / 2;
        *pgval = mult * (vbox->g1 + vbox->g2 + 1) / 2;
        *pbval = mult * (vbox->b1 + vbox->b2 + 1) / 2;
    } else {
        *prval = rsum / ntot;
        *pgval = gsum / ntot;
        *pbval = bsum / ntot;
    }

#if  DEBUG_MC_COLORS
    lept_stderr("ntot[%d] = %d: [%d, %d, %d], (%d, %d, %d)\n",
                index, ntot, vbox->r2 - vbox->r1 + 1,
                vbox->g2 - vbox->g1 + 1, vbox->b2 - vbox->b1 + 1,
                *prval, *pgval, *pbval);
#endif  /* DEBUG_MC_COLORS */

    return 0;
}


/*!
 * \brief   vboxGetCount()
 *
 * \param[in]    vbox      3d region of color space for one quantized color
 * \param[in]    histo
 * \param[in]    sigbits   valid: 5 or 6
 * \return  number of image pixels in this region, or 0 on error
 */
static l_int32
vboxGetCount(L_BOX3D  *vbox,
             l_int32  *histo,
             l_int32   sigbits)
{
l_int32  i, j, k, npix, index;

    if (!vbox)
        return ERROR_INT("vbox not defined", __func__, 0);
    if (!histo)
        return ERROR_INT("histo not defined", __func__, 0);

    npix = 0;
    for (i = vbox->r1; i <= vbox->r2; i++) {
        for (j = vbox->g1; j <= vbox->g2; j++) {
            for (k = vbox->b1; k <= vbox->b2; k++) {
                 index = (i << (2 * sigbits)) + (j << sigbits) + k;
                 npix += histo[index];
            }
        }
    }

    return npix;
}


/*!
 * \brief   vboxGetVolume()
 *
 * \param[in]    vbox    3d region of color space for one quantized color
 * \return  quantized volume of vbox, or 0 on error
 */
static l_int32
vboxGetVolume(L_BOX3D  *vbox)
{
    if (!vbox)
        return ERROR_INT("vbox not defined", __func__, 0);

    return ((vbox->r2 - vbox->r1 + 1) * (vbox->g2 - vbox->g1 + 1) *
            (vbox->b2 - vbox->b1 + 1));
}

/*!
 * \brief    box3dCreate()
 *
 * \param[in]    r1, r2, g1, g2, b1, b2    initial values
 * \return  vbox
 */
static L_BOX3D *
box3dCreate(l_int32  r1,
            l_int32  r2,
            l_int32  g1,
            l_int32  g2,
            l_int32  b1,
            l_int32  b2)
{
L_BOX3D  *vbox;

    vbox = (L_BOX3D *)LEPT_CALLOC(1, sizeof(L_BOX3D));
    vbox->r1 = r1;
    vbox->r2 = r2;
    vbox->g1 = g1;
    vbox->g2 = g2;
    vbox->b1 = b1;
    vbox->b2 = b2;
    return vbox;
}


/*!
 * \brief     box3dCopy()
 *
 * \param[in]    vbox
 * \return  vboxc   copy of vbox
 *
 * <pre>
 * Notes:
 *      Don't copy the sortparam.
 * </pre>
 */
static L_BOX3D *
box3dCopy(L_BOX3D  *vbox)
{
L_BOX3D  *vboxc;

    if (!vbox)
        return (L_BOX3D *)ERROR_PTR("vbox not defined", __func__, NULL);

    vboxc = box3dCreate(vbox->r1, vbox->r2, vbox->g1, vbox->g2,
                        vbox->b1, vbox->b2);
    vboxc->npix = vbox->npix;
    vboxc->vol = vbox->vol;
    return vboxc;
}
