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
 * \file bilateral.c
 * <pre>
 *
 *     Top level approximate separable grayscale or color bilateral filtering
 *          PIX                 *pixBilateral()
 *          PIX                 *pixBilateralGray()
 *
 *     Implementation of approximate separable bilateral filter
 *          static L_BILATERAL  *bilateralCreate()
 *          static void         *bilateralDestroy()
 *          static PIX          *bilateralApply()
 *
 *     Slow, exact implementation of grayscale or color bilateral filtering
 *          PIX                 *pixBilateralExact()
 *          PIX                 *pixBilateralGrayExact()
 *          PIX                 *pixBlockBilateralExact()
 *
 *     Kernel helper function
 *          L_KERNEL            *makeRangeKernel()
 *
 *  This includes both a slow, exact implementation of the bilateral
 *  filter algorithm (given by Sylvain Paris and Frédo Durand),
 *  and a fast, approximate and separable implementation (following
 *  Yang, Tan and Ahuja).  See bilateral.h for algorithmic details.
 *
 *  The bilateral filter has the nice property of applying a gaussian
 *  filter to smooth parts of the image that don't vary too quickly,
 *  while at the same time preserving edges.  The filter is nonlinear
 *  and cannot be decomposed into two separable filters; however,
 *  there exists an approximate method that is separable.  To further
 *  speed up the separable implementation, you can generate the
 *  intermediate data at reduced resolution.
 *
 *  The full kernel is composed of two parts: a spatial gaussian filter
 *  and a nonlinear "range" filter that depends on the intensity difference
 *  between the reference pixel at the spatial kernel origin and any other
 *  pixel within the kernel support.
 *
 *  In our implementations, the range filter is a parameterized,
 *  one-sided, 256-element, monotonically decreasing gaussian function
 *  of the absolute value of the difference between pixel values; namely,
 *  abs(I2 - I1).  In general, any decreasing function can be used,
 *  and more generally,  any two-dimensional kernel can be used if
 *  you wish to relax the 'abs' condition.  (In that case, the range
 *  filter can be 256 x 256).
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"
#include "bilateral.h"

static L_BILATERAL *bilateralCreate(PIX *pixs, l_float32 spatial_stdev,
                                    l_float32 range_stdev, l_int32 ncomps,
                                    l_int32 reduction);
static PIX *bilateralApply(L_BILATERAL *bil);
static void bilateralDestroy(L_BILATERAL **pbil);


#ifndef  NO_CONSOLE_IO
#define  DEBUG_BILATERAL    0
#endif  /* ~NO_CONSOLE_IO */

/*--------------------------------------------------------------------------*
 *  Top level approximate separable grayscale or color bilateral filtering  *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   pixBilateral()
 *
 * \param[in]    pixs            8 bpp gray or 32 bpp rgb, no colormap
 * \param[in]    spatial_stdev   of gaussian kernel; in pixels, > 0.5
 * \param[in]    range_stdev     of gaussian range kernel; > 5.0; typ. 50.0
 * \param[in]    ncomps          number of intermediate sums J(k,x);
 *                               in [4 ... 30]
 * \param[in]    reduction       1, 2 or 4
 * \return  pixd   bilateral filtered image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This performs a relatively fast, separable bilateral
 *          filtering operation.  The time is proportional to ncomps
 *          and varies inversely approximately as the cube of the
 *          reduction factor.  See bilateral.h for algorithm details.
 *      (2) We impose minimum values for range_stdev and ncomps to
 *          avoid nasty artifacts when either are too small.  We also
 *          impose a constraint on their product:
 *               ncomps * range_stdev >= 100.
 *          So for values of range_stdev >= 25, ncomps can be as small as 4.
 *          Here is a qualitative, intuitive explanation for this constraint.
 *          Call the difference in k values between the J(k) == 'delta', where
 *              'delta' ~ 200 / ncomps
 *          Then this constraint is roughly equivalent to the condition:
 *              'delta' < 2 * range_stdev
 *          Note that at an intensity difference of (2 * range_stdev), the
 *          range part of the kernel reduces the effect by the factor 0.14.
 *          This constraint requires that we have a sufficient number of
 *          PCBs (i.e, a small enough 'delta'), so that for any value of
 *          image intensity I, there exists a k (and a PCB, J(k), such that
 *              |I - k| < range_stdev
 *          Any fewer PCBs and we don't have enough to support this condition.
 *      (3) The upper limit of 30 on ncomps is imposed because the
 *          gain in accuracy is not worth the extra computation.
 *      (4) The size of the gaussian kernel is twice the spatial_stdev
 *          on each side of the origin.  The minimum value of
 *          spatial_stdev, 0.5, is required to have a finite sized
 *          spatial kernel.  In practice, a much larger value is used.
 *      (5) Computation of the intermediate images goes inversely
 *          as the cube of the reduction factor.  If you can use a
 *          reduction of 2 or 4, it is well-advised.
 *      (6) The range kernel is defined over the absolute value of pixel
 *          grayscale differences, and hence must have size 256 x 1.
 *          Values in the array represent the multiplying weight
 *          depending on the absolute gray value difference between
 *          the source pixel and the neighboring pixel, and should
 *          be monotonically decreasing.
 *      (7) Interesting observation.  Run this on prog/fish24.jpg, with
 *          range_stdev = 60, ncomps = 6, and spatial_dev = {10, 30, 50}.
 *          As spatial_dev gets larger, we get the counter-intuitive
 *          result that the body of the red fish becomes less blurry.
 *      (8) The image must be sufficiently big to get reasonable results.
 *          This requires the dimensions to be at least twice the filter size.
 *          Otherwise, return a copy of the input with warning.
 * </pre>
 */
PIX *
pixBilateral(PIX       *pixs,
             l_float32  spatial_stdev,
             l_float32  range_stdev,
             l_int32    ncomps,
             l_int32    reduction)
{
l_int32       w, h, d, filtersize;
l_float32     sstdev;  /* scaled spatial stdev */
PIX          *pixt, *pixr, *pixg, *pixb, *pixd;

    if (!pixs || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs not defined or cmapped", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 32)
        return (PIX *)ERROR_PTR("pixs not 8 or 32 bpp", __func__, NULL);
    if (reduction != 1 && reduction != 2 && reduction != 4)
        return (PIX *)ERROR_PTR("reduction invalid", __func__, NULL);
    filtersize = (l_int32)(2.0 * spatial_stdev + 1.0 + 0.5);
    if (w < 2 * filtersize || h < 2 * filtersize) {
        L_WARNING("w = %d, h = %d; w or h < 2 * filtersize = %d; "
                  "returning copy\n", __func__, w, h, 2 * filtersize);
        return pixCopy(NULL, pixs);
    }
    sstdev = spatial_stdev / (l_float32)reduction;  /* reduced spat. stdev */
    if (sstdev < 0.5)
        return (PIX *)ERROR_PTR("sstdev < 0.5", __func__, NULL);
    if (range_stdev <= 5.0)
        return (PIX *)ERROR_PTR("range_stdev <= 5.0", __func__, NULL);
    if (ncomps < 4 || ncomps > 30)
        return (PIX *)ERROR_PTR("ncomps not in [4 ... 30]", __func__, NULL);
    if (ncomps * range_stdev < 100.0)
        return (PIX *)ERROR_PTR("ncomps * range_stdev < 100.0", __func__, NULL);

    if (d == 8)
        return pixBilateralGray(pixs, spatial_stdev, range_stdev,
                                ncomps, reduction);

    pixt = pixGetRGBComponent(pixs, COLOR_RED);
    pixr = pixBilateralGray(pixt, spatial_stdev, range_stdev, ncomps,
                            reduction);
    pixDestroy(&pixt);
    pixt = pixGetRGBComponent(pixs, COLOR_GREEN);
    pixg = pixBilateralGray(pixt, spatial_stdev, range_stdev, ncomps,
                            reduction);
    pixDestroy(&pixt);
    pixt = pixGetRGBComponent(pixs, COLOR_BLUE);
    pixb = pixBilateralGray(pixt, spatial_stdev, range_stdev, ncomps,
                            reduction);
    pixDestroy(&pixt);
    pixd = pixCreateRGBImage(pixr, pixg, pixb);
    pixDestroy(&pixr);
    pixDestroy(&pixg);
    pixDestroy(&pixb);
    return pixd;
}


/*!
 * \brief   pixBilateralGray()
 *
 * \param[in]    pixs             8 bpp gray
 * \param[in]    spatial_stdev    of gaussian kernel; in pixels, > 0.5
 * \param[in]    range_stdev      of gaussian range kernel; > 5.0; typ. 50.0
 * \param[in]    ncomps           number of intermediate sums J(k,x);
 *                                in [4 ... 30]
 * \param[in]    reduction        1, 2 or 4
 * \return  pixd   8 bpp bilateral filtered image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixBilateral() for constraints on the input parameters.
 *      (2) See pixBilateral() for algorithm details.
 * </pre>
 */
PIX *
pixBilateralGray(PIX       *pixs,
                 l_float32  spatial_stdev,
                 l_float32  range_stdev,
                 l_int32    ncomps,
                 l_int32    reduction)
{
l_float32     sstdev;  /* scaled spatial stdev */
PIX          *pixd;
L_BILATERAL  *bil;

    if (!pixs || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs not defined or cmapped", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp gray", __func__, NULL);
    if (reduction != 1 && reduction != 2 && reduction != 4)
        return (PIX *)ERROR_PTR("reduction invalid", __func__, NULL);
    sstdev = spatial_stdev / (l_float32)reduction;  /* reduced spat. stdev */
    if (sstdev < 0.5)
        return (PIX *)ERROR_PTR("sstdev < 0.5", __func__, NULL);
    if (range_stdev <= 5.0)
        return (PIX *)ERROR_PTR("range_stdev <= 5.0", __func__, NULL);
    if (ncomps < 4 || ncomps > 30)
        return (PIX *)ERROR_PTR("ncomps not in [4 ... 30]", __func__, NULL);
    if (ncomps * range_stdev < 100.0)
        return (PIX *)ERROR_PTR("ncomps * range_stdev < 100.0", __func__, NULL);

    bil = bilateralCreate(pixs, spatial_stdev, range_stdev, ncomps, reduction);
    if (!bil) return (PIX *)ERROR_PTR("bil not made", __func__, NULL);
    pixd = bilateralApply(bil);
    bilateralDestroy(&bil);
    return pixd;
}


/*----------------------------------------------------------------------*
 *       Implementation of approximate separable bilateral filter       *
 *----------------------------------------------------------------------*/
/*!
 * \brief   bilateralCreate()
 *
 * \param[in]    pixs            8 bpp gray, no colormap
 * \param[in]    spatial_stdev   of gaussian kernel; in pixels, > 0.5
 * \param[in]    range_stdev     of gaussian range kernel; > 5.0; typ. 50.0
 * \param[in]    ncomps          number of intermediate sums J(k,x);
 *                               in [4 ... 30]
 * \param[in]    reduction       1, 2 or 4
 * \return  bil, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This initializes a bilateral filtering operation, generating all
 *          the data required.  It takes most of the time in the bilateral
 *          filtering operation.
 *      (2) See bilateral.h for details of the algorithm.
 *      (3) See pixBilateral() for constraints on input parameters, which
 *          are not checked here.
 * </pre>
 */
static L_BILATERAL *
bilateralCreate(PIX       *pixs,
                l_float32  spatial_stdev,
                l_float32  range_stdev,
                l_int32    ncomps,
                l_int32    reduction)
{
l_int32       w, ws, wd, h, hs, hd, i, j, k, index;
l_int32       border, minval, maxval, spatial_size;
l_int32       halfwidth, wpls, wplt, wpld, kval, nval, dval;
l_float32     sstdev, fval1, fval2, denom, sum, norm, kern;
l_int32      *nc, *kindex;
l_float32    *kfract, *range, *spatial;
l_uint32     *datas, *datat, *datad, *lines, *linet, *lined;
L_BILATERAL  *bil;
PIX          *pix1, *pix2, *pixt, *pixsc, *pixd;
PIXA         *pixac;

    if (reduction == 1) {
        pix1 = pixClone(pixs);
    } else if (reduction == 2) {
        pix1 = pixScaleAreaMap2(pixs);
    } else {  /* reduction == 4) */
        pix2 = pixScaleAreaMap2(pixs);
        pix1 = pixScaleAreaMap2(pix2);
        pixDestroy(&pix2);
    }
    if (!pix1)
        return (L_BILATERAL *)ERROR_PTR("pix1 not made", __func__, NULL);

    sstdev = spatial_stdev / (l_float32)reduction;  /* reduced spat. stdev */
    border = (l_int32)(2 * sstdev + 1);
    pixsc = pixAddMirroredBorder(pix1, border, border, border, border);
    pixGetExtremeValue(pix1, 1, L_SELECT_MIN, NULL, NULL, NULL, &minval);
    pixGetExtremeValue(pix1, 1, L_SELECT_MAX, NULL, NULL, NULL, &maxval);
    pixDestroy(&pix1);
    if (!pixsc)
        return (L_BILATERAL *)ERROR_PTR("pixsc not made", __func__, NULL);

    bil = (L_BILATERAL *)LEPT_CALLOC(1, sizeof(L_BILATERAL));
    bil->spatial_stdev = sstdev;
    bil->range_stdev = range_stdev;
    bil->reduction = reduction;
    bil->ncomps = ncomps;
    bil->minval = minval;
    bil->maxval = maxval;
    bil->pixsc = pixsc;
    bil->pixs = pixClone(pixs);

    /* -------------------------------------------------------------------- *
     * Generate arrays for interpolation of J(k,x):
     *  (1.0 - kfract[.]) * J(kindex[.], x) + kfract[.] * J(kindex[.] + 1, x),
     * where I(x) is the index into kfract[] and kindex[],
     * and x is an index into the 2D image array.
     * -------------------------------------------------------------------- */
        /* nc is the set of k values to be used in J(k,x) */
    nc = (l_int32 *)LEPT_CALLOC(ncomps, sizeof(l_int32));
    for (i = 0; i < ncomps; i++)
        nc[i] = minval + i * (maxval - minval) / (ncomps - 1);
    bil->nc = nc;

        /* kindex maps from intensity I(x) to the lower k index for J(k,x) */
    kindex = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    for (i = minval, k = 0; i <= maxval && k < ncomps - 1; k++) {
        fval2 = nc[k + 1];
        while (i < fval2) {
            kindex[i] = k;
            i++;
        }
    }
    kindex[maxval] = ncomps - 2;
    bil->kindex = kindex;

        /* kfract maps from intensity I(x) to the fraction of J(k+1,x) used */
    kfract = (l_float32 *)LEPT_CALLOC(256, sizeof(l_float32));  /* from lower */
    for (i = minval, k = 0; i <= maxval && k < ncomps - 1; k++) {
        fval1 = nc[k];
        fval2 = nc[k + 1];
        while (i < fval2) {
            kfract[i] = (l_float32)(i - fval1) / (l_float32)(fval2 - fval1);
            i++;
        }
    }
    kfract[maxval] = 1.0;
    bil->kfract = kfract;

#if  DEBUG_BILATERAL
    for (i = minval; i <= maxval; i++)
      lept_stderr("kindex[%d] = %d; kfract[%d] = %5.3f\n",
                  i, kindex[i], i, kfract[i]);
    for (i = 0; i < ncomps; i++)
      lept_stderr("nc[%d] = %d\n", i, nc[i]);
#endif  /* DEBUG_BILATERAL */

    /* -------------------------------------------------------------------- *
     *             Generate 1-D kernel arrays (spatial and range)           *
     * -------------------------------------------------------------------- */
    spatial_size = 2 * sstdev + 1;  /* same as the added border */
    spatial = (l_float32 *)LEPT_CALLOC(spatial_size, sizeof(l_float32));
    denom = 2. * sstdev * sstdev;
    for (i = 0; i < spatial_size; i++)
        spatial[i] = expf(-(l_float32)(i * i) / denom);
    bil->spatial = spatial;

    range = (l_float32 *)LEPT_CALLOC(256, sizeof(l_float32));
    denom = 2. * range_stdev * range_stdev;
    for (i = 0; i < 256; i++)
        range[i] = expf(-(l_float32)(i * i) / denom);
    bil->range = range;

    /* -------------------------------------------------------------------- *
     *            Generate principal bilateral component images             *
     * -------------------------------------------------------------------- */
    pixac = pixaCreate(ncomps);
    pixGetDimensions(pixsc, &ws, &hs, NULL);
    datas = pixGetData(pixsc);
    wpls = pixGetWpl(pixsc);
    pixGetDimensions(pixs, &w, &h, NULL);
    wd = (w + reduction - 1) / reduction;
    hd = (h + reduction - 1) / reduction;
    halfwidth = (l_int32)(2.0 * sstdev);
    for (index = 0; index < ncomps; index++) {
        pixt = pixCopy(NULL, pixsc);
        datat = pixGetData(pixt);
        wplt = pixGetWpl(pixt);
        kval = nc[index];
            /* Separable convolutions: horizontal first */
        for (i = 0; i < hd; i++) {
            lines = datas + (border + i) * wpls;
            linet = datat + (border + i) * wplt;
            for (j = 0; j < wd; j++) {
                sum = 0.0;
                norm = 0.0;
                for (k = -halfwidth; k <= halfwidth; k++) {
                    nval = GET_DATA_BYTE(lines, border + j + k);
                    kern = spatial[L_ABS(k)] * range[L_ABS(kval - nval)];
                    sum += kern * nval;
                    norm += kern;
                }
                if (norm > 0.0) {
                    dval = (l_int32)((sum / norm) + 0.5);
                    SET_DATA_BYTE(linet, border + j, dval);
                }
            }
        }
            /* Vertical convolution */
        pixd = pixCreate(wd, hd, 8);
        datad = pixGetData(pixd);
        wpld = pixGetWpl(pixd);
        for (i = 0; i < hd; i++) {
            linet = datat + (border + i) * wplt;
            lined = datad + i * wpld;
            for (j = 0; j < wd; j++) {
                sum = 0.0;
                norm = 0.0;
                for (k = -halfwidth; k <= halfwidth; k++) {
                    nval = GET_DATA_BYTE(linet + k * wplt, border + j);
                    kern = spatial[L_ABS(k)] * range[L_ABS(kval - nval)];
                    sum += kern * nval;
                    norm += kern;
                }
                if (norm > 0.0)
                    dval = (l_int32)((sum / norm) + 0.5);
                else
                    dval = GET_DATA_BYTE(linet, border + j);
                SET_DATA_BYTE(lined, j, dval);
            }
        }
        pixDestroy(&pixt);
        pixaAddPix(pixac, pixd, L_INSERT);
    }
    bil->pixac = pixac;
    bil->lineset = (l_uint32 ***)pixaGetLinePtrs(pixac, NULL);
    return bil;
}


/*!
 * \brief   bilateralApply()
 *
 * \param[in]    bil
 * \return  pixd
 */
static PIX *
bilateralApply(L_BILATERAL  *bil)
{
l_int32      i, j, k, ired, jred, w, h, wpls, wpld, ncomps, reduction;
l_int32      vals, vald, lowval, hival;
l_int32     *kindex;
l_float32    fract;
l_float32   *kfract;
l_uint32    *lines, *lined, *datas, *datad;
l_uint32  ***lineset = NULL;  /* for set of PBC */
PIX         *pixs, *pixd;
PIXA        *pixac;

    if (!bil)
        return (PIX *)ERROR_PTR("bil not defined", __func__, NULL);
    pixs = bil->pixs;
    ncomps = bil->ncomps;
    kindex = bil->kindex;
    kfract = bil->kfract;
    reduction = bil->reduction;
    pixac = bil->pixac;
    lineset = bil->lineset;
    if (pixaGetCount(pixac) != ncomps)
        return (PIX *)ERROR_PTR("PBC images do not exist", __func__, NULL);

    if ((pixd = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    pixGetDimensions(pixs, &w, &h, NULL);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        ired = i / reduction;
        for (j = 0; j < w; j++) {
            jred = j / reduction;
            vals = GET_DATA_BYTE(lines, j);
            k = kindex[vals];
            lowval = GET_DATA_BYTE(lineset[k][ired], jred);
            hival = GET_DATA_BYTE(lineset[k + 1][ired], jred);
            fract = kfract[vals];
            vald = (l_int32)((1.0 - fract) * lowval + fract * hival + 0.5);
            SET_DATA_BYTE(lined, j, vald);
        }
    }

    return pixd;
}


/*!
 * \brief   bilateralDestroy()
 *
 * \param[in,out]   pbil    will be set to null before returning
 */
static void
bilateralDestroy(L_BILATERAL  **pbil)
{
l_int32       i;
L_BILATERAL  *bil;

    if (pbil == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((bil = *pbil) == NULL)
        return;

    pixDestroy(&bil->pixs);
    pixDestroy(&bil->pixsc);
    pixaDestroy(&bil->pixac);
    LEPT_FREE(bil->spatial);
    LEPT_FREE(bil->range);
    LEPT_FREE(bil->nc);
    LEPT_FREE(bil->kindex);
    LEPT_FREE(bil->kfract);
    for (i = 0; i < bil->ncomps; i++)
        LEPT_FREE(bil->lineset[i]);
    LEPT_FREE(bil->lineset);
    LEPT_FREE(bil);
    *pbil = NULL;
}


/*----------------------------------------------------------------------*
 *    Exact implementation of grayscale or color bilateral filtering    *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixBilateralExact()
 *
 * \param[in]    pixs          8 bpp gray or 32 bpp rgb
 * \param[in]    spatial_kel   gaussian kernel
 * \param[in]    range_kel     [optional] 256 x 1, monotonically decreasing
 * \return  pixd   8 bpp bilateral filtered image
 *
 * <pre>
 * Notes:
 *      (1) The spatial_kel is a conventional smoothing kernel, typically a
 *          2-d Gaussian kernel or other block kernel.  It can be either
 *          normalized or not, but must be everywhere positive.
 *      (2) The range_kel is defined over the absolute value of pixel
 *          grayscale differences, and hence must have size 256 x 1.
 *          Values in the array represent the multiplying weight for each
 *          gray value difference between the target pixel and center of the
 *          kernel, and should be monotonically decreasing.
 *      (3) If range_kel == NULL, a constant weight is applied regardless
 *          of the range value difference.  This degenerates to a regular
 *          pixConvolve() with a normalized kernel.
 * </pre>
 */
PIX *
pixBilateralExact(PIX       *pixs,
                  L_KERNEL  *spatial_kel,
                  L_KERNEL  *range_kel)
{
l_int32  d;
PIX     *pixt, *pixr, *pixg, *pixb, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs) != NULL)
        return (PIX *)ERROR_PTR("pixs is cmapped", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
        return (PIX *)ERROR_PTR("pixs not 8 or 32 bpp", __func__, NULL);
    if (!spatial_kel)
        return (PIX *)ERROR_PTR("spatial_ke not defined", __func__, NULL);

    if (d == 8) {
        return pixBilateralGrayExact(pixs, spatial_kel, range_kel);
    } else {  /* d == 32 */
        pixt = pixGetRGBComponent(pixs, COLOR_RED);
        pixr = pixBilateralGrayExact(pixt, spatial_kel, range_kel);
        pixDestroy(&pixt);
        pixt = pixGetRGBComponent(pixs, COLOR_GREEN);
        pixg = pixBilateralGrayExact(pixt, spatial_kel, range_kel);
        pixDestroy(&pixt);
        pixt = pixGetRGBComponent(pixs, COLOR_BLUE);
        pixb = pixBilateralGrayExact(pixt, spatial_kel, range_kel);
        pixDestroy(&pixt);
        pixd = pixCreateRGBImage(pixr, pixg, pixb);

        pixDestroy(&pixr);
        pixDestroy(&pixg);
        pixDestroy(&pixb);
        return pixd;
    }
}


/*!
 * \brief   pixBilateralGrayExact()
 *
 * \param[in]    pixs          8 bpp gray
 * \param[in]    spatial_kel   gaussian kernel
 * \param[in]    range_kel     [optional] 256 x 1, monotonically decreasing
 * \return  pixd   8 bpp bilateral filtered image
 *
 * <pre>
 * Notes:
 *      (1) See pixBilateralExact().
 * </pre>
 */
PIX *
pixBilateralGrayExact(PIX       *pixs,
                      L_KERNEL  *spatial_kel,
                      L_KERNEL  *range_kel)
{
l_int32    i, j, id, jd, k, m, w, h, d, sx, sy, cx, cy, wplt, wpld;
l_int32    val, center_val;
l_uint32  *datat, *datad, *linet, *lined;
l_float32  sum, weight_sum, weight;
L_KERNEL  *keli;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs must be gray", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (!spatial_kel)
        return (PIX *)ERROR_PTR("spatial kel not defined", __func__, NULL);
    kernelGetParameters(spatial_kel, &sy, &sx, NULL, NULL);
    if (w < 2 * sx + 1 || h < 2 * sy + 1) {
        L_WARNING("w = %d < 2 * sx + 1 = %d, or h = %d < 2 * sy + 1 = %d; "
                  "returning copy\n", __func__, w, 2 * sx + 1, h, 2 * sy + 1);
        return pixCopy(NULL, pixs);
    }
    if (!range_kel)
        return pixConvolve(pixs, spatial_kel, 8, 1);
    if (range_kel->sx != 256 || range_kel->sy != 1)
        return (PIX *)ERROR_PTR("range kel not {256 x 1", __func__, NULL);

    keli = kernelInvert(spatial_kel);
    kernelGetParameters(keli, &sy, &sx, &cy, &cx);
    if ((pixt = pixAddMirroredBorder(pixs, cx, sx - cx, cy, sy - cy)) == NULL) {
        kernelDestroy(&keli);
        return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
    }

    pixd = pixCreate(w, h, 8);
    datat = pixGetData(pixt);
    datad = pixGetData(pixd);
    wplt = pixGetWpl(pixt);
    wpld = pixGetWpl(pixd);
    for (i = 0, id = 0; id < h; i++, id++) {
        lined = datad + id * wpld;
        for (j = 0, jd = 0; jd < w; j++, jd++) {
            center_val = GET_DATA_BYTE(datat + (i + cy) * wplt, j + cx);
            weight_sum = 0.0;
            sum = 0.0;
            for (k = 0; k < sy; k++) {
                linet = datat + (i + k) * wplt;
                for (m = 0; m < sx; m++) {
                    val = GET_DATA_BYTE(linet, j + m);
                    weight = keli->data[k][m] *
                        range_kel->data[0][L_ABS(center_val - val)];
                    weight_sum += weight;
                    sum += val * weight;
                }
            }
            SET_DATA_BYTE(lined, jd, (l_int32)(sum / weight_sum + 0.5));
        }
    }

    kernelDestroy(&keli);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixBlockBilateralExact()
 *
 * \param[in]    pixs             8 bpp gray or 32 bpp rgb
 * \param[in]    spatial_stdev    must be > 0.0
 * \param[in]    range_stdev      must be > 0.0
 * \return  pixd   8 bpp or 32 bpp bilateral filtered image
 *
 * <pre>
 * Notes:
 *      (1) See pixBilateralExact().  This provides an interface using
 *          the standard deviations of the spatial and range filters.
 *      (2) The convolution window halfwidth is 2 * spatial_stdev,
 *          and the square filter size is 4 * spatial_stdev + 1.
 *          The kernel captures 95% of total energy.  This is compensated
 *          by normalization.
 *      (3) The range_stdev is analogous to spatial_halfwidth in the
 *          grayscale domain [0...255], and determines how much damping of the
 *          smoothing operation is applied across edges.  The larger this
 *          value is, the smaller the damping.  The smaller the value, the
 *          more edge details are preserved.  These approximations are useful
 *          for deciding the appropriate cutoff.
 *              kernel[1 * stdev] ~= 0.6  * kernel[0]
 *              kernel[2 * stdev] ~= 0.14 * kernel[0]
 *              kernel[3 * stdev] ~= 0.01 * kernel[0]
 *          If range_stdev is infinite there is no damping, and this
 *          becomes a conventional gaussian smoothing.
 *          This value does not affect the run time.
 *      (4) If range_stdev is negative or zero, the range kernel is
 *          ignored and this degenerates to a straight gaussian convolution.
 *      (5) This is very slow for large spatial filters.  The time
 *          on a 3GHz pentium is roughly
 *             T = 1.2 * 10^-8 * (A * sh^2)  sec
 *          where A = # of pixels, sh = spatial halfwidth of filter.
 * </pre>
 */
PIX*
pixBlockBilateralExact(PIX       *pixs,
                       l_float32  spatial_stdev,
                       l_float32  range_stdev)
{
l_int32    d, halfwidth;
L_KERNEL  *spatial_kel, *range_kel;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
        return (PIX *)ERROR_PTR("pixs not 8 or 32 bpp", __func__, NULL);
    if (pixGetColormap(pixs) != NULL)
        return (PIX *)ERROR_PTR("pixs is cmapped", __func__, NULL);
    if (spatial_stdev <= 0.0)
        return (PIX *)ERROR_PTR("invalid spatial stdev", __func__, NULL);
    if (range_stdev <= 0.0)
        return (PIX *)ERROR_PTR("invalid range stdev", __func__, NULL);

    halfwidth = 2 * spatial_stdev;
    spatial_kel = makeGaussianKernel(halfwidth, halfwidth, spatial_stdev, 1.0);
    range_kel = makeRangeKernel(range_stdev);
    pixd = pixBilateralExact(pixs, spatial_kel, range_kel);
    kernelDestroy(&spatial_kel);
    kernelDestroy(&range_kel);
    return pixd;
}


/*----------------------------------------------------------------------*
 *                         Kernel helper function                       *
 *----------------------------------------------------------------------*/
/*!
 * \brief   makeRangeKernel()
 *
 * \param[in]    range_stdev   must be > 0.0
 * \return  kel, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Creates a one-sided Gaussian kernel with the given
 *          standard deviation.  At grayscale difference of one stdev,
 *          the kernel falls to 0.6, and to 0.01 at three stdev.
 *      (2) A typical input number might be 20.  Then pixels whose
 *          value differs by 60 from the center pixel have their
 *          weight in the convolution reduced by a factor of about 0.01.
 * </pre>
 */
L_KERNEL *
makeRangeKernel(l_float32  range_stdev)
{
l_int32    x;
l_float32  val, denom;
L_KERNEL  *kel;

    if (range_stdev <= 0.0)
        return (L_KERNEL *)ERROR_PTR("invalid stdev <= 0", __func__, NULL);

    denom = 2. * range_stdev * range_stdev;
    if ((kel = kernelCreate(1, 256)) == NULL)
        return (L_KERNEL *)ERROR_PTR("kel not made", __func__, NULL);
    kernelSetOrigin(kel, 0, 0);
    for (x = 0; x < 256; x++) {
        val = expf(-(l_float32)(x * x) / denom);
        kernelSetElement(kel, 0, x, val);
    }
    return kel;
}
